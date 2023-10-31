#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "scan_callback.h"
#include "wlan_manager.h"

extern "C" void app_main();

void app_main() {
  vTaskDelete(nullptr);
}
