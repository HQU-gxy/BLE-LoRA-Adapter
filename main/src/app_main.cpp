#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "scan_manager.h"
#include "server_callback.h"
#include "whitelist_char_callback.h"
#include "esp_hal.h"
#include "common.h"
#include "hr_lora.h"
#include "app_nvs.h"

extern "C" void app_main();

void app_main() {
  using namespace common;
  using namespace blue;
  const auto TAG = "main";
  ESP_LOGI(TAG, "boot");
  auto &hal = *new ESPHal(pin::SCK, pin::MISO, pin::MOSI);
  hal.init();
  ESP_LOGI(TAG, "hal init success!");
  auto &module = *new Module(&hal, pin::CS, pin::DIO1, pin::RST, pin::BUSY);
  auto &rf     = *new LLCC68(&module);
  auto st      = rf.begin(434, 500.0, 7, 7,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                          22, 8, 1.6);
  if (st != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "failed, code %d", st);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
  }
  ESP_LOGI(TAG, "RF init success!");

  NimBLEDevice::init(BLE_NAME);
  auto &server    = *NimBLEDevice::createServer();
  auto &server_cb = *new ServerCallbacks();
  server.setCallbacks(&server_cb);

  auto &scan_manager = *new ScanManager();
  auto &hr_service   = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  // repeat the data from the connected device
  auto &hr_char    = *hr_service.createCharacteristic(BLE_CHAR_HR_CHAR_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  auto &white_char = *hr_service.createCharacteristic(BLE_CHAR_WHITE_LIST_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto &white_cb   = *new WhiteListCallback();
  white_char.setCallbacks(&white_cb);
  white_cb.on_request_address = [&scan_manager]() {
    return scan_manager.get_target_addr();
  };
  white_cb.on_disconnect = [&scan_manager]() {
    scan_manager.set_target_addr(etl::nullopt);
  };
  white_cb.on_address = [&scan_manager](WhiteListCallback::addr_opt_t addr) {
    if (addr.has_value()) {
      scan_manager.set_target_addr(etl::make_optional(addr.value().addr));
    } else {
      scan_manager.set_target_addr(etl::nullopt);
    }
  };

  scan_manager.on_data = [&rf, &hr_char](HeartMonitor &device, uint8_t *data, size_t size) {
    ESP_LOGI("scan_manager", "data: %s", utils::toHex(data, size).c_str());
    // I should encode it a bit
    rf.transmit(data, size);
    hr_char.setValue(data, size);
    hr_char.notify();
  };

  server.start();
  scan_manager.start_scanning_task();
  NimBLEDevice::startAdvertising();

  vTaskDelete(nullptr);
}
