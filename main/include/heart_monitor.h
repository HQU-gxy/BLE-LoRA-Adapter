//
// Created by Kurosu Chan on 2023/10/27.
//

#ifndef WIT_HUB_WIT_DEVICE_H
#define WIT_HUB_WIT_DEVICE_H

#include <NimBLEDevice.h>

namespace blue {
struct HeartMonitor {
  static const int ADDR_SIZE = 6;
  using addr_t               = etl::array<uint8_t, ADDR_SIZE>;

  std::string name;
  addr_t addr{0};
  NimBLEClient *client             = nullptr;
};
}

#endif // WIT_HUB_WIT_DEVICE_H
