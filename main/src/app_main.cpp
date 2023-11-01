#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "scan_callback.h"
#include "wlan_manager.h"
#include "esp_hal.h"
#include <driver/gpio.h>

extern "C" void app_main();

namespace pin {
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html
constexpr auto SCK = GPIO_NUM_8;
constexpr auto MOSI = GPIO_NUM_9;
constexpr auto MISO = GPIO_NUM_10;
// i.e. NSS chip select
constexpr auto CS = GPIO_NUM_3;
constexpr auto BUSY = GPIO_NUM_19;
constexpr auto RST = GPIO_NUM_18;
constexpr auto DIO1 = GPIO_NUM_1;
constexpr auto DIO2 = GPIO_NUM_2;
}

void app_main() {
  const auto TAG = "main";
  ESP_LOGI(TAG, "boot");
  auto &hal = *new ESPHal(pin::SCK, pin::MISO, pin::MOSI);
  hal.init();
  ESP_LOGI(TAG, "hal init success!");
  auto &module = *new Module(&hal, pin::CS, pin::DIO1, pin::RST, pin::BUSY);
  auto &rf = *new LLCC68(&module);
  auto st = rf.begin(434, 500.0, 7, 7,
                     RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22, 8, 1.6);
  if (st != RADIOLIB_ERR_NONE){
    ESP_LOGE(TAG, "failed, code %d", st);
    esp_restart();
  }
  ESP_LOGI(TAG, "RF init success!");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_restart();
}
