//
// Created by Kurosu Chan on 2023/10/27.
//

#ifndef WIT_HUB_WIT_DEVICE_H
#define WIT_HUB_WIT_DEVICE_H

namespace blue {
struct WitDevice {
  static const int ADDR_SIZE = 6;
  using addr_t               = etl::array<uint8_t, ADDR_SIZE>;

  addr_t addr{0};
  NimBLEClient *client             = nullptr;
  NimBLERemoteService *service           = nullptr;
  NimBLERemoteCharacteristic *read_char  = nullptr;
  NimBLERemoteCharacteristic *write_char = nullptr;
};
}

#endif // WIT_HUB_WIT_DEVICE_H
