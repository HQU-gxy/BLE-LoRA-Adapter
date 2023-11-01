//
// Created by Kurosu Chan on 2023/11/1.
//

#ifndef BLE_LORA_ADAPTER_WHITELIST_CHAR_CALLBACK_H
#define BLE_LORA_ADAPTER_WHITELIST_CHAR_CALLBACK_H

#include <pb_decode.h>
#include "whitelist.h"
#include "pb_encode.h"
#include <NimBLEDevice.h>

class WhiteListCallback : public NimBLECharacteristicCallbacks {
public:
  using addr_opt_t                                 = etl::optional<white_list::Addr>;
  std::function<void(white_list::Addr)> on_address = nullptr;
  std::function<void()> on_disconnect              = nullptr;
  /**
   * @return the address that on the current target list, if exists
   */
  std::function<addr_opt_t()> on_request_address = nullptr;
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
    constexpr auto TAG         = "WhiteListCallback";
    auto value                 = pCharacteristic->getValue();
    auto istream               = pb_istream_from_buffer(value.data(), value.size());
    ::WhiteListRequest request = WhiteListRequest_init_zero;
    auto request_opt           = white_list::unmarshal_while_list_request(&istream, request);
    if (!request_opt.has_value()) {
      ESP_LOGE(TAG, "bad unmarshal");
      return;
    }
    auto &req = request_opt.value();
    if (std::holds_alternative<white_list::list_t>(req)) {
      auto &list = std::get<white_list::list_t>(req);
      if (list.empty()) {
        ESP_LOGW(TAG, "empty list");
        return;
      }
      if (list.size() > 1) {
        ESP_LOGW(TAG, "only the first element would be used (list size: %d)", list.size());
        return;
      }
      auto &item = list[0];
      if (std::holds_alternative<white_list::Name>(item)) {
        ESP_LOGE(TAG, "name is not supported yet");
        return;
      } else {
        auto &addr = std::get<white_list::Addr>(item);
        ESP_LOGI(TAG, "addr: %s", utils::toHex(addr.addr.data(), addr.addr.size()).c_str());
        if (on_address != nullptr) {
          on_address(std::move(addr));
        } else {
          ESP_LOGW(TAG, "on_address is not set");
        }
      }
    } else {
      auto command = std::get<white_list::command_t>(req);
      ESP_LOGI(TAG, "command: %d", command);
      switch (command) {
        case WhiteListCommand_REQUEST: {
          addr_opt_t addr_opt = etl::nullopt;
          if (on_request_address != nullptr) {
            addr_opt = on_request_address();
          } else {
            ESP_LOGW(TAG, "on_request_address is not set");
          }
          ::WhiteListResponse response = WhiteListResponse_init_zero;
          white_list::response_t resp;
          if (addr_opt.has_value()) {
            resp = white_list::response_t{white_list::list_t{addr_opt.value()}};
          } else {
            resp = white_list::response_t{WhiteListErrorCode_NULL};
          }
          auto buf     = etl::array<uint8_t, 64>{};
          auto ostream = pb_ostream_from_buffer(buf.data(), buf.size());
          auto ok      = white_list::marshal_white_list_response(&ostream, response, resp);
          if (!ok) {
            ESP_LOGE(TAG, "bad marshal");
            return;
          }
          auto len = ostream.bytes_written;
          pCharacteristic->setValue(buf.data(), len);
          break;
        }
        case WhiteListCommand_DISCONNECT: {
          if (on_disconnect != nullptr) {
            on_disconnect();
          } else {
            ESP_LOGW(TAG, "on_disconnect is not set");
          }
          break;
        }
        default: {
          ESP_LOGE(TAG, "unknown command: %d", command);
          break;
        }
      }
    }
  };
};

#endif // BLE_LORA_ADAPTER_WHITELIST_CHAR_CALLBACK_H
