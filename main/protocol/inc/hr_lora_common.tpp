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
using name_map_key_t          = uint8_t;
using addr_t                  = std::array<uint8_t, BLE_ADDR_SIZE>;

/**
 * @brief a concept to check if a type (use as a module/namespace) is marshallable
 * @tparam T a module type that has a actual member type `t` and a static function `marshal`
 */
template <typename T>
concept marshallable = requires(const T::t &t, uint8_t *buffer, size_t size) {
  { T::marshal(t, buffer, size) } -> std::convertible_to<size_t>;
};

/**
 * @brief a concept to check if a type (use as a module/namespace) is unmarshallable
 * @tparam T a module type that has a actual member type `t` and a static function `unmarshal`
 */
template <typename T>
concept unmarshallable = requires(const uint8_t *buffer, size_t size) {
  { T::unmarshal(buffer, size) } -> std::convertible_to<etl::optional<typename T::t>>;
};
}

#endif // BLE_LORA_ADAPTER_HR_LORA_COMMON_H
