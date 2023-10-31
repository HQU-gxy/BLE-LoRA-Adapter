//
// Created by Kurosu Chan on 2023/10/26.
//

#ifndef WIT_HUB_WLAN_MANAGER_H
#define WIT_HUB_WLAN_MANAGER_H

#include <esp_check.h>
#include <mqtt_client.h>
#include <esp_wifi.h>
#include <etl/optional.h>
#include <nvs_flash.h>
#include "wifi_entity.h"
#include <msd/channel.hpp>

namespace wlan {
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFi.h
class WlanManager {
  /**
   * @brief whether the device is connected to an access point
   * @sa WIFI_EVENT_STA_CONNECTED
   * @sa WIFI_EVENT_STA_DISCONNECTED
   * @note this flag will be set to false when the connection is lost
   */
  bool _is_connected = false;
  /**
   * @brief whether the device has a valid ip address
   * @sa IP_EVENT_STA_GOT_IP
   * @sa IP_EVENT_STA_LOST_IP
   * @note this flag has more strict condition than `_is_connected`
   * and it will keep true even if the connection is lost for a while
   */
  bool _has_ip       = false;
  bool _has_nvs_init = false;
  /**
   * @sa https://github.com/espressif/esp-idf/blob/8fc8f3f47997aadba21facabc66004c1d22de181/examples/protocols/mqtt/tcp/main/app_main.c
   * @sa https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html
   */
  esp_mqtt_client_handle_t mqtt_handle = nullptr;
  etl::optional<AP> _ap                = etl::nullopt;
  std::vector<std::string> subscribed_topics{"/wit/+/control/#"};
  sub_msg_chan_t _sub_msg_chan{8};
  TaskHandle_t _connect_task_handle = nullptr;

private:
  esp_err_t _register_wifi_handlers();

  esp_err_t _register_mqtt_handlers();

  static esp_err_t _connect(const AP &access_point);

  static esp_err_t _disconnect();

  esp_err_t do_subscribe();

  /**
   * @param pvParameters a pointer to a WlanManager instance, passed by the caller of xTaskCreate
   * @effect delete the task itself and set [WlanManager::_connect_task_handle] to nullptr
   */
  static void connect_task(void *pvParameters);

public:
  WlanManager() = default;
  [[nodiscard]] bool is_connected() const {
    return _is_connected;
  }
  [[nodiscard]] bool has_ip() const {
    return _has_ip;
  }

  /**
   * @note don't deallocate the returned pointer or messing with it.
   * You DON'T own it.
   */
  [[nodiscard]] sub_msg_chan_t *sub_msg_chan() {
    return &_sub_msg_chan;
  }

  /**
   * @brief initialize nvs flash
   * @sideeffect set `_has_nvs_init` to true
   * @return ESP_OK on success
   */
  esp_err_t nvs_init();

  /**
   * @brief initialize wlan (and nvs flash if not initialized)
   * @sa https://github.com/espressif/esp-idf/blob/8fc8f3f47997aadba21facabc66004c1d22de181/examples/bluetooth/esp_ble_mesh/coex_test/components/case/wifi_connect.c#L61
   * @return ESP_OK on success
   */
  esp_err_t wifi_init();

  esp_err_t mqtt_init();

  esp_err_t start_connect_task();

  esp_err_t set_ap(AP new_ap);

  etl::optional<AP> ap() {
    return _ap;
  };

  esp_err_t subscribe(const std::string &topic);

  esp_err_t unsubscribe(const std::string &topic);

  esp_err_t connect();

  esp_err_t publish(const MqttPubMsg &msg);
  ;
};

struct WifiScanTaskParam {
  wlan::WlanManager *pManager = nullptr;
  TaskHandle_t task_handle    = nullptr;
};
}

#endif // WIT_HUB_WLAN_MANAGER_H
