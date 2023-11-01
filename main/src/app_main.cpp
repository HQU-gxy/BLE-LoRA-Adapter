#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "scan_callback.h"
#include "server_callback.h"
#include "wlan_manager.h"
#include "esp_hal.h"
#include "common.h"

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
    esp_restart();
  }
  ESP_LOGI(TAG, "RF init success!");
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  NimBLEDevice::init(BLE_NAME);
  auto &server    = *NimBLEDevice::createServer();
  auto &server_cb = *new ServerCallbacks();
  server.setCallbacks(&server_cb);

  auto &hr_service = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  // repeat the data from the connected device
  auto &hr_char      = *hr_service.createCharacteristic(BLE_CHAR_HEARTBEAT_UUID,
                                                        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  auto &scan_manager = *new ScanManager();
  server.start();
  scan_manager.start_scanning_task();
  NimBLEDevice::startAdvertising();

  vTaskDelete(nullptr);
}
