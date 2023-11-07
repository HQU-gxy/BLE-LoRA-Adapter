//
// Created by Kurosu Chan on 2023/11/1.
//

#ifndef BLE_LORA_ADAPTER_COMMON_H
#define BLE_LORA_ADAPTER_COMMON_H

#include <chrono>
#include <driver/gpio.h>

namespace common {
namespace pin {
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html
  constexpr auto SCK  = GPIO_NUM_8;
  constexpr auto MOSI = GPIO_NUM_9;
  constexpr auto MISO = GPIO_NUM_10;
  // i.e. NSS chip select
  constexpr auto CS   = GPIO_NUM_3;
  constexpr auto BUSY = GPIO_NUM_19;
  constexpr auto RST  = GPIO_NUM_18;
  constexpr auto DIO1 = GPIO_NUM_1;
  constexpr auto DIO2 = GPIO_NUM_2;
}

static const char *BLE_CHAR_WHITE_LIST_UUID = "12a481f0-9384-413d-b002-f8660566d3b0";
static const char *BLE_CHAR_DEVICE_UUID     = "a2f05114-fdb6-4549-ae2a-845b4be1ac48";
static const char *BLE_STANDARD_HR_SERVICE_UUID = "180d";
static const char *BLE_STANDARD_HR_CHAR_UUID    = "2a37";
static const char *BLE_CHAR_HR_SERVICE_UUID     = BLE_STANDARD_HR_SERVICE_UUID;
static const char *BLE_CHAR_HR_CHAR_UUID        = BLE_STANDARD_HR_CHAR_UUID;
constexpr auto BLE_NAME                         = "LoRA-Adapter";
constexpr auto SCAN_TIME                        = std::chrono::milliseconds(2500);
// scan time + sleep time
constexpr auto SCAN_TOTAL_TIME = std::chrono::milliseconds(5000);
static auto MAX_RF_MSG_SCHEDULE_DELAY_MS = 3000;
static uint32_t INTERVAL_SEND_NAMED_HR_COUNT = 15;
static_assert(SCAN_TOTAL_TIME > SCAN_TIME);

static constexpr auto PREF_PARTITION_LABEL = "st";
static constexpr auto PREF_NAME_MAP_KEY_WORD8_KEY = "nmk";
static constexpr auto PREF_ADDR_BLOB_KEY   = "addr";
}

#endif // BLE_LORA_ADAPTER_COMMON_H
