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
#include "heart_monitor.h"
#include "whitelist.h"
#include "utils.h"
#include "common.h"

namespace blue {
const int MAX_DEVICE_NUM  = 12;
const int MAX_SERVICE_NUM = 4;
const int MAX_CHAR_NUM    = 4;

/**
 * @brief print all services and characteristics of a device
 * @param NimBLEClient a client reference
 * @note You should have connected to the device before calling this function
 */
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
      const auto TAG = "SCAN RESULT";
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

class ScanManager : public NimBLEScanCallbacks {
public:
  using addr_t       = HeartMonitor::addr_t;
  using device_ptr_t = std::unique_ptr<HeartMonitor>;
  using addr_ptr_t   = std::unique_ptr<white_list::Addr>;

private:
  addr_ptr_t target_addr = nullptr;
  device_ptr_t device    = nullptr;
  // null if the scanning task is not running
  TaskHandle_t scan_task_handle = nullptr;

public:
  std::function<void(HeartMonitor &device, uint8_t *, size_t)> on_data = nullptr;
  void set_target_addr(addr_t addr) {
    white_list::Addr addr_ = {std::move(addr)};
    // disconnect current device
    if (device != nullptr) {
      device->client->disconnect();
      NimBLEDevice::deleteClient(device->client);
      device = nullptr;
    }
    target_addr = std::make_unique<white_list::Addr>(std::move(addr_));
    // start scanning task
  }

  /**
   * @brief start the scanning task
   * @note should be kicked off in the main thread.
   *  When the device is connected successfully, the scanning task will be deleted.
   *  When the device is disconnected, the scanning task will be restarted.
   */
  bool start_scanning_task() {
    if (scan_task_handle != nullptr) {
      return false;
    }
    auto scanning_task = [](void *pvParameters) {
      const auto TAG = "scanning";
      auto &self     = *static_cast<ScanManager *>(pvParameters);
      auto &scan     = *NimBLEDevice::getScan();
      scan.setScanCallbacks(&self, false);
      scan.setInterval(1349);
      scan.setWindow(449);
      scan.setActiveScan(true);
      ESP_LOGI(TAG, "Initiated");
      for (;;) {
        bool ok = scan.start(common::SCAN_TIME.count(), false);
        if (!ok) {
          ESP_LOGE(TAG, "Failed to start scan");
        }
        vTaskDelay(common::SCAN_TOTAL_TIME.count() / portTICK_PERIOD_MS);
      }
    };
    xTaskCreate(scanning_task, "scan", 4096,
                this, 5, &scan_task_handle);
    return true;
  }

  void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
    const auto TAG = "ScanCallback::onResult";
    auto name      = advertisedDevice->getName();

    if (!name.empty()) {
      ESP_LOGI(TAG, "name=%s; addr=%s; rssi=%d", name.c_str(),
               advertisedDevice->getAddress().toString().c_str(),
               advertisedDevice->getRSSI());
    }
    struct ConnectTaskParam {
      TaskHandle_t task_handle;
      std::function<void()> task;
    };
    if (target_addr == nullptr) {
      return;
    }
    auto native_addr = advertisedDevice->getAddress().getNative();
    bool eq          = std::equal(target_addr->addr.begin(), target_addr->addr.end(), native_addr);
    if (!eq) {
      return;
    }
    auto &self          = *this;
    auto nimble_address = advertisedDevice->getAddress();
    auto addr_native    = nimble_address.getNative();
    auto addr           = addr_t{};
    std::copy(addr_native, addr_native + HeartMonitor::ADDR_SIZE, addr.begin());
    ESP_LOGI(TAG, "try to connect to %s (%s)", name.c_str(), nimble_address.toString().c_str());
    // for some reason the connection would block the scan callback for a long time
    // I have to create a new thread to do the connection
    auto connect_task = [name, addr, nimble_address, &self]() {
      const auto TAG        = "connect";
      NimBLEClient *pClient = nullptr;
      if (self.device != nullptr) {
        pClient = self.device->client;
        assert(pClient != nullptr);
      } else {
        pClient  = NimBLEDevice::getClientByPeerAddress(nimble_address);
        auto dev = HeartMonitor{
            .name   = name,
            .addr   = addr,
            .client = pClient,
        };
        self.device = std::make_unique<HeartMonitor>(std::move(dev));
      }
      auto &client = *pClient;
      auto ok      = client.connect();
      if (!ok) {
        ESP_LOGE(TAG, "Failed to connect to %s", name.c_str());
        return;
      }
      ESP_LOGI(TAG, "connected to %s", name.c_str());
      self.stop_scanning_task();
      print_services_chars(client);
    };
    auto param = new ConnectTaskParam{
        .task_handle = nullptr,
        .task        = connect_task,
    };

    /**
     * A wrapper task for FreeRTOS to run the closure
     */
    auto run_connect_task = [](void *param) {
      auto &p = *static_cast<ConnectTaskParam *>(param);
      p.task();
      auto handle = p.task_handle;
      delete &p;
      ESP_LOGI("connecting", "end");
      vTaskDelete(handle);
    };
    xTaskCreate(run_connect_task, "connect", 4096,
                param, 5, &param->task_handle);
  };

private:
  /**
   * @brief stop the scanning task
   * @note should be called when the device is connected successfully.
   */
  bool stop_scanning_task() {
    if (scan_task_handle == nullptr) {
      return false;
    }
    vTaskDelete(scan_task_handle);
    scan_task_handle = nullptr;
    return true;
  }
};
}

#endif // WIT_HUB_SCAN_CALLBACK_H
