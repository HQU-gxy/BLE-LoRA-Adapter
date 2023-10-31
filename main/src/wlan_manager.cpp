//
// Created by Kurosu Chan on 2023/10/27.
//

#include "utils.h"
#include "wlan_manager.h"

namespace wlan {
esp_err_t WlanManager::_register_wifi_handlers() {
  auto TAG   = "WlanManager::register_wifi_handlers";
  auto &self = *this;
  auto err   = esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        auto &self         = *static_cast<WlanManager *>(arg);
        auto TAG           = "WlanManager::connect::wifi_event";
        self._is_connected = true;
        ESP_LOGI(TAG, "Connected to AP");
        // connection has been established but somehow the connection is lost
        if (self._has_ip) {
          if (self.mqtt_handle != nullptr) {
            ESP_LOGI(TAG, "Reconnecting to mqtt broker");
            auto err = esp_mqtt_client_reconnect(self.mqtt_handle);
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "Failed to reconnect to mqtt broker; Reason %s", esp_err_to_name(err));
              return;
            }
          }
        }
      },
      &self);
  ESP_RETURN_ON_ERROR(err, TAG, "Failed to register wifi event handler");

  err = esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        auto &self         = *static_cast<WlanManager *>(arg);
        self._is_connected = false;
        auto TAG           = "WlanManager::connect::wifi_event";
        ESP_LOGI(TAG, "Disconnected from AP");
      },
      &self);
  ESP_RETURN_ON_ERROR(err, TAG, "Failed to register wifi event handler");

  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#ip-event-sta-got-ip
  // Upon receiving this event,
  // the application needs to close all sockets and recreate the application when the IPV4 changes to a valid one.
  err = esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        auto &self   = *static_cast<WlanManager *>(arg);
        self._has_ip = true;
        auto TAG     = "WlanManager::connect::ip_event";
        // stop the connect task if it is running
        if (self._connect_task_handle != nullptr) {
          auto old                  = self._connect_task_handle;
          self._connect_task_handle = nullptr;
          vTaskDelete(old);
        }
        // data, aside from event data, that is passed to the handler when it is called
        // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_event.html#_CPPv426esp_event_handler_register16esp_event_base_t7int32_t19esp_event_handler_tPv
        // Event structure for IP_EVENT_STA_GOT_IP, IP_EVENT_ETH_GOT_IP events
        // https://docs.espressif.com/projects/esp-idf/en/v4.0.3/api-reference/network/tcpip_adapter.html
        auto *event   = (ip_event_got_ip_t *)event_data;
        auto &ip_info = event->ip_info;
        ESP_LOGI(TAG, "Got ip: %d.%d.%d.%d", IP2STR(&ip_info.ip));
        if (self.mqtt_handle != nullptr) {
          ESP_LOGI(TAG, "Connecting to mqtt broker");
          auto err = esp_mqtt_client_start(self.mqtt_handle);
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to mqtt broker; Reason %s", esp_err_to_name(err));
            return;
          }
        } else {
          ESP_LOGW(TAG, "mqtt client is not initialized");
        }
      },
      &self);
  ESP_RETURN_ON_ERROR(err, TAG, "Failed to register ip event handler");

  err = esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_LOST_IP, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        auto &self   = *static_cast<WlanManager *>(arg);
        self._has_ip = false;
        auto TAG     = "WlanManager::connect::ip_event";
        ESP_LOGI(TAG, "Lost ip");
        auto err = self.start_connect_task();
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Failed to start connect task");
        }
        if (self.mqtt_handle != nullptr) {
          ESP_LOGI(TAG, "Disconnecting from mqtt broker");
          esp_mqtt_client_stop(self.mqtt_handle);
        } else {
          ESP_LOGW(TAG, "mqtt client is not initialized");
        }
      },
      &self);
  ESP_RETURN_ON_ERROR(err, TAG, "Failed to register ip event handler");

  return ESP_OK;
}

esp_err_t WlanManager::nvs_init() {
  auto TAG = "WlanManager::init";
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Failed to erase NVS");
    ret = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(ret, TAG, "Failed to init NVS");
  _has_nvs_init = true;
  return ESP_OK;
}

esp_err_t WlanManager::wifi_init() { // NOLINT(*-make-member-function-const)
  auto TAG = "WlanManager::init";
  // Initialize NVS
  if (!_has_nvs_init) {
    ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "Failed to init NVS");
  }
  // https://github.com/espressif/esp-idf/blob/master/examples/wifi/scan/main/scan.c
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Failed to init netif");
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Failed to create event loop");
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "Failed to init wifi");
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Failed to set wifi mode");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start wifi");
  ESP_RETURN_ON_ERROR(_register_wifi_handlers(), TAG, "Failed to register wifi handlers");
  return ESP_OK;
}

esp_err_t WlanManager::_connect(const AP &access_point) {
  const auto TAG = "WlanManager::connect";
  wifi_config_t wifi_config{};

  // length of ssid must be less than 32
  if (access_point.ssid.size() > 32) {
    return ESP_ERR_INVALID_ARG;
  }
  std::fill(wifi_config.sta.ssid, wifi_config.sta.ssid + 32, 0x00);
  std::copy(access_point.ssid.begin(), access_point.ssid.end(), wifi_config.sta.ssid);

  // length of password must be less than 64
  if (access_point.password.size() > 64) {
    return ESP_ERR_INVALID_ARG;
  }
  std::fill(wifi_config.sta.password, wifi_config.sta.password + 64, 0x00);
  std::copy(access_point.password.begin(), access_point.password.end(), wifi_config.sta.password);
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Failed to set wifi config");
  ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Failed to connect to wifi");

  return ESP_OK;
}

esp_err_t WlanManager::set_ap(AP new_ap) {
  auto TAG = "WlanManager::set_ap";
  if (new_ap.ssid.empty()) {
    return ESP_ERR_INVALID_ARG;
  }
  this->_ap = std::move(new_ap);
  if (_is_connected) {
    ESP_RETURN_ON_ERROR(_disconnect(), TAG, "Failed to disconnect from wifi");
  }
  return ESP_OK;
}

esp_err_t WlanManager::_disconnect() {
  const auto TAG = "WlanManager::disconnect";
  ESP_RETURN_ON_ERROR(esp_wifi_disconnect(), TAG, "Failed to disconnect from wifi");
  return ESP_OK;
}

esp_err_t WlanManager::mqtt_init() {
  esp_mqtt_client_config_t mqtt_cfg{};
  // uri have precedence over other fields
  mqtt_cfg.broker.address.uri     = BROKER_URL;
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  this->mqtt_handle               = client;
  ESP_RETURN_ON_ERROR(_register_mqtt_handlers(), "WlanManager::mqtt_init", "Failed to register mqtt handlers");
  return ESP_OK;
}

esp_err_t WlanManager::connect() {
  if (_ap == etl::nullopt) {
    return ESP_ERR_INVALID_STATE;
  }
  return _connect(_ap.value());
}

esp_err_t WlanManager::do_subscribe() {
  if (mqtt_handle == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!_has_ip) {
    return ESP_ERR_INVALID_STATE;
  }
  for (auto &topic : subscribed_topics) {
    ESP_LOGI("WlanManager::do_subscribe", "subscribing to %s", topic.c_str());
    auto msg_id = esp_mqtt_client_subscribe(mqtt_handle, topic.c_str(), 0);
    if (msg_id < 0) {
      return ESP_FAIL;
    }
  }
  return ESP_OK;
}
esp_err_t WlanManager::subscribe(const std::string &topic) {
  auto existed = std::find_if(subscribed_topics.begin(), subscribed_topics.end(), [&topic](const std::string &t) {
    return t == topic;
  });
  // already subscribed
  if (existed != subscribed_topics.end()) {
    return ESP_OK;
  }
  subscribed_topics.push_back(topic);
  if (mqtt_handle == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!_has_ip) {
    return ESP_ERR_INVALID_STATE;
  }
  ESP_LOGI("WlanManager::subscribe", "subscribing to %s", topic.c_str());
  auto msg_id = esp_mqtt_client_subscribe(mqtt_handle, topic.c_str(), 0);
  if (msg_id < 0) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t WlanManager::unsubscribe(const std::string &topic) {
  subscribed_topics.erase(std::remove(subscribed_topics.begin(), subscribed_topics.end(), topic), subscribed_topics.end());
  return esp_mqtt_client_unsubscribe(mqtt_handle, topic.c_str());
}

esp_err_t WlanManager::_register_mqtt_handlers() {
  auto TAG = "mqtt";
  auto err = esp_mqtt_client_register_event(
      mqtt_handle, MQTT_EVENT_CONNECTED,
      [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        auto &manager = *static_cast<WlanManager *>(arg);
        manager.do_subscribe();
      },
      this);
  ESP_RETURN_ON_ERROR(err, TAG, "register MQTT_EVENT_CONNECTED handler");
  err = esp_mqtt_client_register_event(
      mqtt_handle, MQTT_EVENT_DATA,
      [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        const auto TAG = "mqtt::MQTT_EVENT_DATA";
        auto &manager  = *static_cast<WlanManager *>(arg);
        auto &event    = *static_cast<esp_mqtt_event_handle_t>(event_data);
        auto sub_data  = std::vector<uint8_t>(event.data, event.data + event.data_len);
        auto sub_topic = std::string(event.topic, event.topic_len);
        ESP_LOGI(TAG, "topic=%s (%d), data=%s (%d)", sub_topic.c_str(), sub_topic.size(),
                 utils::toHex(sub_data.data(), sub_data.size()).c_str(), sub_data.size());
        auto sub_msg = MqttSubMsg{.topic = std::move(sub_topic), .data = std::move(sub_data)};
        manager._sub_msg_chan << std::move(sub_msg);
      },
      this);
  ESP_RETURN_ON_ERROR(err, TAG, "register MQTT_EVENT_DATA handler");
  return ESP_OK;
}

void WlanManager::connect_task(void *pvParameters) {
  auto &self     = *static_cast<WlanManager *>(pvParameters);
  const auto TAG = "connect_task";
  for (;;) {
    if (self._ap.has_value()) {
      auto err = self.connect();
      if (err == ESP_OK) {
        auto old                  = self._connect_task_handle;
        self._connect_task_handle = nullptr;
        if (old != nullptr) {
          vTaskDelete(old);
        }
        return;
      } else {
        ESP_LOGE(TAG, "failed to connect to ap, reason %s (%d)", esp_err_to_name(err), err);
      }
    } else {
       ESP_LOGW(TAG, "no ap to connect");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

esp_err_t WlanManager::start_connect_task() {
  if (_connect_task_handle != nullptr) {
    auto old             = _connect_task_handle;
    _connect_task_handle = nullptr;
    vTaskDelete(old);
  }
  auto ok = xTaskCreate(connect_task, "connect_task", 4096, this, 1, &_connect_task_handle);
  if (ok != pdPASS) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t WlanManager::publish(const MqttPubMsg &msg) {
  if (mqtt_handle == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!_has_ip) {
    return ESP_ERR_INVALID_STATE;
  }
  auto id = esp_mqtt_client_publish(mqtt_handle,
                                    msg.topic.c_str(),
                                    reinterpret_cast<const char *>(msg.data.data()),
                                    msg.data.size(),
                                    msg.qos,
                                    msg.retain);
  if (id < 0) {
    return ESP_FAIL;
  } else {
    return ESP_OK;
  }
}
}
