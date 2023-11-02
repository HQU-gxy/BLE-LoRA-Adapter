//
// Created by Kurosu Chan on 2023/11/2.
//

#ifndef BLE_LORA_ADAPTER_HR_LORA_COMMON_H
#define BLE_LORA_ADAPTER_HR_LORA_COMMON_H

/**
 * @brief some common *constant* definitions for HRLoRA
 */

#include <string>
#include <etl/optional.h>

namespace HrLoRa {
constexpr auto BLE_ADDR_SIZE  = 6;
constexpr auto broadcast_addr = std::array<uint8_t, BLE_ADDR_SIZE>{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
using name_map_key_t = uint8_t;
using addr_t = std::array<uint8_t, BLE_ADDR_SIZE>;
}

#endif // BLE_LORA_ADAPTER_HR_LORA_COMMON_H
