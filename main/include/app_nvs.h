//
// Created by Kurosu Chan on 2023/11/2.
//

#ifndef BLE_LORA_ADAPTER_APP_NVS_H
#define BLE_LORA_ADAPTER_APP_NVS_H

#include <etl/array.h>
#include <nvs_handle.hpp>
#include "heart_monitor.h"
#include "common.h"

/**
 * @sa https://github.com/espressif/esp-idf/blob/8fc8f3f47997aadba21facabc66004c1d22de181/examples/storage/nvs_rw_value_cxx/main/nvs_value_example_main.cpp
 */
namespace nvs {
using addr_t                    = blue::HeartMonitor::addr_t;
static constexpr auto ADDR_SIZE = blue::HeartMonitor::ADDR_SIZE;

/**
 * @brief get the Bluetooth LE address of last connected heart rate monitor from nvs
 * @param [out] addr_ptr the pointer to the access point
 * @return error code
 */
esp_err_t get_addr(addr_t *addr_ptr);

esp_err_t set_addr(const addr_t &addr);
}

#endif // BLE_LORA_ADAPTER_APP_NVS_H
