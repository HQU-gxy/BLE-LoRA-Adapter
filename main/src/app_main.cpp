#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

extern "C" void app_main();

void app_main() {
  vTaskDelete(nullptr);
}
