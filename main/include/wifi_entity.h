//
// Created by Kurosu Chan on 2023/10/27.
//

#ifndef WIT_HUB_WIFI_ENTITY_H
#define WIT_HUB_WIFI_ENTITY_H
#include<string>
#include<vector>
#include<msd/channel.hpp>

namespace wlan {
const auto BROKER_URL = "mqtt://weihua-iot.cn:1883";
struct MqttPubMsg {
  std::string topic;
  std::vector<uint8_t> data;
  int qos = 0;
  // retain flag
  int retain = 0;
};

struct MqttSubMsg {
  std::string topic;
  std::vector<uint8_t> data;
};

struct AP {
  std::string ssid;
  std::string password;
};

using sub_msg_chan_t = msd::channel<MqttSubMsg>;
}


#endif // WIT_HUB_WIFI_ENTITY_H
