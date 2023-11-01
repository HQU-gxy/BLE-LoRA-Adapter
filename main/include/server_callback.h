//
// Created by Kurosu Chan on 2023/11/1.
//

#ifndef BLE_LORA_ADAPTER_SERVER_CALLBACK_H
#define BLE_LORA_ADAPTER_SERVER_CALLBACK_H

#include <NimBLEDevice.h>

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;

  void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override;
};

#endif // BLE_LORA_ADAPTER_SERVER_CALLBACK_H
