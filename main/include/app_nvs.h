//
// Created by Kurosu Chan on 2023/11/2.
//

#ifndef BLE_LORA_ADAPTER_APP_NVS_H
#define BLE_LORA_ADAPTER_APP_NVS_H

#include <etl/array.h>
#include <nvs_handle.hpp>
#include <nvs_flash.h>
#include <esp_check.h>
#include "heart_monitor.h"
#include "common.h"

/**
 * @sa https://github.com/espressif/esp-idf/blob/8fc8f3f47997aadba21facabc66004c1d22de181/examples/storage/nvs_rw_value_cxx/main/nvs_value_example_main.cpp
 */
namespace app_nvs {
using addr_t                    = blue::HeartMonitor::addr_t;
using name_map_key_t            = uint8_t;
static constexpr auto ADDR_SIZE = blue::HeartMonitor::ADDR_SIZE;

/**
 * @brief get the Bluetooth LE address of last connected heart rate monitor from nvs
 * @param [out] addr_ptr the pointer to the access point
 * @return error code
 */
esp_err_t get_addr(addr_t *addr_ptr);

esp_err_t set_addr(const addr_t &addr);

/**
 * @brief initialize nvs flash
 * @return ESP_OK on success
 */
esp_err_t nvs_init();

esp_err_t get_name_map_key(name_map_key_t *key_ptr);;

esp_err_t set_name_map_key(name_map_key_t key);;
}

#endif // BLE_LORA_ADAPTER_APP_NVS_H
