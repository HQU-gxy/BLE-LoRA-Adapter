//
// Created by Kurosu Chan on 2023/11/2.
//

#ifndef BLE_LORA_ADAPTER_QUERY_DEVICE_BY_MAC_H
#define BLE_LORA_ADAPTER_QUERY_DEVICE_BY_MAC_H

#include <string>
#include <etl/optional.h>
#include "hr_lora_common.tpp"

namespace HrLoRa {
namespace query_device_by_mac {
  struct t {
    addr_t addr{};
  };
  consteval size_t size_needed() {
    return BLE_ADDR_SIZE + 1;
  }
  uint8_t magic = 0x37;
  size_t marshal(t &data, uint8_t *buffer, size_t size) {
    if (size < size_needed()) {
      return 0;
    }
    buffer[0] = magic;
    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      buffer[i + 1] = data.addr[i];
    }
    return size_needed();
  }
  etl::optional<t> unmarshal(const uint8_t *buffer, size_t size) {
    if (size < size_needed()) {
      return etl::nullopt;
    }

    t data;
    if (buffer[0] != magic) {
      return etl::nullopt;
    }

    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      data.addr[i] = buffer[i + 1];
    }

    return data;
  }
}

namespace hr_device {
  struct t {
    addr_t addr{};
    // zero terminated string
    std::string name{};
  };
  size_t size_needed(t &data) {
    return BLE_ADDR_SIZE + data.name.size() + 1;
  }
  size_t marshal(t &data, uint8_t *buffer, size_t size) {
    if (size < size_needed(data)) {
      return 0;
    }
    size_t offset = 0;
    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      buffer[offset++] = data.addr[i];
    }
    for (char i : data.name) {
      buffer[offset++] = i;
    }
    buffer[offset++] = 0;
    return offset;
  }
  etl::optional<t> unmarshal(const uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < BLE_ADDR_SIZE) {
      return etl::nullopt;
    }

    t data;
    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      data.addr[i] = buffer[i];
    }
    size_t offset = BLE_ADDR_SIZE;
    while (offset < buffer_size && buffer[offset] != 0) {
      data.name.push_back(buffer[offset++]);
    }
    return data;
  }
}

namespace query_device_by_mac_response {
  uint8_t magic = 0x47;
  struct t {
    addr_t repeater_addr{};
    name_map_key_t key                 = 0;
    etl::optional<hr_device::t> device = etl::nullopt;
  };
  size_t size_needed(t &data) {
    return sizeof(magic) +
           BLE_ADDR_SIZE +
           sizeof(t::key) +
           (data.device ? hr_device::size_needed(*data.device) : 0);
  }
  size_t marshal(t &data, uint8_t *buffer, size_t size) {
    if (size < size_needed(data)) {
      return 0;
    }
    size_t offset    = 0;
    buffer[offset++] = magic;
    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      buffer[offset++] = data.repeater_addr[i];
    }
    buffer[offset++] = data.key;
    // last bit indicates whether there is a device
    uint8_t flag = 0x00;
    if (data.device) {
      flag |= 0x01;
    }
    buffer[offset++] = flag;
    if (data.device) {
      offset += hr_device::marshal(*data.device, buffer + offset, size - offset);
    }
    return offset;
  }
  etl::optional<t> unmarshal(const uint8_t *buffer, size_t size) {
    if (size < BLE_ADDR_SIZE + sizeof(name_map_key_t)) {
      return etl::nullopt;
    }

    t data;
    if (buffer[0] != magic) {
      return etl::nullopt;
    }
    size_t offset = 1;
    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      data.repeater_addr[i] = buffer[offset++];
    }
    data.key     = buffer[offset++];
    uint8_t flag = buffer[offset++];
    if (flag & 0x01) {
      data.device = hr_device::unmarshal(buffer + offset, size - offset);
    } else {
      data.device = etl::nullopt;
    }
    return data;
  }
}
}

#endif // BLE_LORA_ADAPTER_QUERY_DEVICE_BY_MAC_H
