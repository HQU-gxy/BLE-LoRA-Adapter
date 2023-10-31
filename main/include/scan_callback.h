//
// Created by Kurosu Chan on 2023/10/26.
//

#ifndef WIT_HUB_SCAN_CALLBACK_H
#define WIT_HUB_SCAN_CALLBACK_H

#include <etl/vector.h>
#include <etl/map.h>
#include <etl/algorithm.h>
#include <etl/flat_map.h>
#include <NimBLEDevice.h>
#include "wifi_entity.h"
#include "wit_device.h"
#include "utils.h"

namespace blue {
const auto TARGET         = "WT901BLE67";
const int MAX_DEVICE_NUM  = 12;
const int MAX_SERVICE_NUM = 4;
const int MAX_CHAR_NUM    = 4;

// who define these UUIDs? so stupid
const char *SERVICE_UUID = "0000ffe5-0000-1000-8000-00805f9a34fb";
const char *READ_CHAR    = "0000ffe4-0000-1000-8000-00805f9a34fb";
const char *WRITE_CHAR   = "0000ffe9-0000-1000-8000-00805f9a34fb";

void print_services_chars(NimBLEClient &client) {
  const auto TAG = "client";
  auto pServices = client.getServices(true);
  if (!pServices) {
    ESP_LOGE(TAG, "Failed to get services");
    return;
  }
  auto &services = *pServices;
  if (services.empty()) {
    ESP_LOGW(TAG, "No services found");
  }
  for (auto *pService : services) {
    auto pChars = pService->getCharacteristics(true);
    if (!pChars) {
      ESP_LOGE(TAG, "Failed to get characteristics");
      return;
    }
    auto &chars = *pChars;
    if (chars.empty()) {
      ESP_LOGW(TAG, "No characteristics found for service %s", pService->getUUID().toString().c_str());
    }
    for (auto *pChar : chars) {
      const auto TAG = "SCAN_RESULT";
      ESP_LOGI(TAG, "service=%s, char=%s [%s%s%s%s%s]",
               pService->getUUID().toString().c_str(),
               pChar->getUUID().toString().c_str(),
               pChar->canNotify() ? "n" : "",
               pChar->canRead() ? "r" : "",
               pChar->canWrite() ? "w" : "",
               pChar->canWriteNoResponse() ? "W" : "",
               pChar->canIndicate() ? "i" : "");
    }
  }
}

class ScanCallback : public NimBLEScanCallbacks {
  using addr_t       = WitDevice::addr_t;
  using device_map_t = etl::vector<WitDevice, MAX_DEVICE_NUM>;
  device_map_t devices{};

public:
  std::function<void(WitDevice &device, uint8_t *, size_t)> on_data = nullptr;
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
    const auto TAG = "ScanCallback::onResult";
    auto name      = advertisedDevice->getName();

    if (!name.empty()) {
      ESP_LOGI(TAG, "name=%s; addr=%s; rssi=%d", name.c_str(), advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getRSSI());
    }
    struct ConnectTaskParam {
      TaskHandle_t task_handle;
      std::function<void()> task;
    };

    if (name == TARGET) {
      auto &self          = *this;
      auto nimble_address = advertisedDevice->getAddress();
      auto addr_native    = nimble_address.getNative();
      auto addr           = addr_t{};
      std::copy(addr_native, addr_native + WitDevice::ADDR_SIZE, addr.begin());
      ESP_LOGI(TAG, "try to connect to %s (%s)", name.c_str(), nimble_address.toString().c_str());
      // for some reason the connection would block the scan callback for a long time
      // I have to create a new thread to do the connection
      auto connect_task = [name, addr, nimble_address, &self]() {
        const auto TAG = "connect";
        auto &devices  = self.devices;
        if (name == TARGET) {
          NimBLEClient *pClient = nullptr;
          auto found            = etl::find_if(devices.begin(), devices.end(), [&addr](const auto &device) {
            return device.addr == addr;
          });
          if (found != devices.end()) {
            ESP_LOGW(TAG, "device already exists");
            pClient = found->client;
          } else {
            ESP_LOGI(TAG, "device not found, creating a new one");
            pClient     = NimBLEDevice::createClient();
            auto device = WitDevice{.addr = addr, .client = pClient};
            devices.emplace_back(std::move(device));
            // might be race condition here
            found = &devices.back();
          }
          auto &device = *found;
          auto &client = *pClient;
          if (client.isConnected()) {
            ESP_LOGW(TAG, "client is already connected");
            return;
          }
          auto ok = client.connect(nimble_address);
          if (!ok) {
            ESP_LOGE(TAG, "Failed to connect to the device");
            return;
          }
          print_services_chars(client);
          auto pService = client.getService(NimBLEUUID{SERVICE_UUID});
          if (!pService) {
            ESP_LOGE(TAG, "failed to get service");
            return;
          }
          device.service = pService;
          auto pReadChar = pService->getCharacteristic(NimBLEUUID{READ_CHAR});
          if (!pReadChar) {
            ESP_LOGE(TAG, "failed to get read char");
            return;
          }
          device.read_char = pReadChar;
          auto pWriteChar =
              pService->getCharacteristic(NimBLEUUID{WRITE_CHAR});
          if (!pWriteChar) {
            ESP_LOGE(TAG, "failed to get write char");
            return;
          }
          device.write_char = pWriteChar;

          auto on_notify = [&self, &device](NimBLERemoteCharacteristic *pChar, uint8_t *pData, size_t length, bool isNotify) {
            const auto TAG = "on_notify";
            ESP_LOGD(TAG, "%s", utils::toHex(pData, length).c_str());
            if (self.on_data != nullptr) {
              self.on_data(device, pData, length);
            }
          };
          ok = device.read_char->subscribe(true, on_notify);
          if (!ok) {
            ESP_LOGE(TAG, "failed to subscribe");
          }
        }
      };
      auto param = new ConnectTaskParam{
          .task_handle = nullptr,
          .task        = connect_task,
      };
      xTaskCreate([](void *param) {
        auto &p = *static_cast<ConnectTaskParam *>(param);
        p.task();
        auto handle = p.task_handle;
        delete &p;
        ESP_LOGI("connecting", "end");
        vTaskDelete(handle);
      },
                  "connect", 4096, param, 5, &param->task_handle);
    }
  };
  bool toDevice(addr_t &addr, uint8_t *data, size_t length) {
    const auto TAG = "ScanCallback::toDevice";
    auto found     = etl::find_if(devices.begin(), devices.end(), [&addr](const auto &device) {
      return device.addr == addr;
    });
    if (found == devices.end()) {
      ESP_LOGE(TAG, "device not found");
      return false;
    }
    auto &device = *found;
    if (!device.write_char) {
      ESP_LOGE(TAG, "write char not found");
      return false;
    }
    auto ok = device.write_char->writeValue(data, length, true);
    if (!ok) {
      ESP_LOGE(TAG, "failed to write");
      return false;
    }
    return true;
  }
};
}

#endif // WIT_HUB_SCAN_CALLBACK_H
