#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "scan_manager.h"
#include "server_callback.h"
#include "whitelist_char_callback.h"
#include "esp_hal.h"
#include "common.h"
#include "hr_lora.h"
#include "app_nvs.h"
#include <endian.h>
#include <freertos/event_groups.h>
#include <etl/random.h>
#include <cstring>
#include <utility>
#include <esp_random.h>

// #define DISABLE_LORA

extern "C" void app_main();

const auto RecvEvt = BIT0;

static const auto send_lk_timeout_tick = 100;

// https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32c3/api-reference/system/power_management.html
// https://github.com/espressif/esp-idf/tree/b4268c874a4/examples/wifi/power_save
class SendScheduler {
  static constexpr auto TAG = "SendScheduler";
  std::vector<uint8_t> data{};
  TimerHandle_t timer = nullptr;
  struct send_timer_param_t {
    std::function<void()> fn;
  };
  send_timer_param_t timer_param{[]() {}};

public:
  std::function<void(uint8_t *data, size_t size)>
      send = nullptr;

  void clear_timer() {
    if (this->timer != nullptr) {
      xTimerStop(this->timer, portMAX_DELAY);
      xTimerDelete(this->timer, portMAX_DELAY);
      this->timer = nullptr;
    }
  }

  /**
   * @brief schedule the data to be sent
   * @param pdata the data to be sent
   * @param size the size of the data
   * @param interval the interval between transmission
   * @return if the previous timer is replaced
   */
  bool schedule(uint8_t *pdata, uint8_t size, std::chrono::milliseconds interval) {
    this->data       = std::vector<uint8_t>(pdata, pdata + size);
    bool is_replaced = false;
    if (this->timer != nullptr) {
      clear_timer();
      is_replaced = true;
    }

    auto send_task = [this]() {
      if (this->send != nullptr) {
        send(data.data(), data.size());
      } else {
        ESP_LOGW(TAG, "send callback is empty");
      }
      clear_timer();
    };

    this->timer_param.fn = send_task;
    auto run_send_task   = [](TimerHandle_t handle) {
      auto *param = static_cast<send_timer_param_t *>(pvTimerGetTimerID(handle));
      param->fn();
    };
    this->timer = xTimerCreate("send_timer",
                               interval.count(),
                               pdFALSE,
                               &this->timer_param,
                               run_send_task);
    xTimerStart(this->timer, portMAX_DELAY);
    return is_replaced;
  }
};

/**
 * @brief try to transmit the data
 * @note would block until the transmission is done and will start receiving after that
 */
void try_transmit(uint8_t *data, size_t size,
                  SemaphoreHandle_t lk, TickType_t timeout_tick,
                  LLCC68 &rf) {
  const auto TAG = "try_transmit";
  if (xSemaphoreTake(lk, timeout_tick) != pdTRUE) {
    ESP_LOGE(TAG, "failed to take rf_lock; no transmission happens;");
    return;
  }
  auto err = rf.transmit(data, size);
  if (err == RADIOLIB_ERR_NONE) {
    // ok
  } else if (err == RADIOLIB_ERR_TX_TIMEOUT) {
    ESP_LOGW(TAG, "tx timeout; please check the busy pin;");
  } else {
    ESP_LOGE(TAG, "failed to transmit, code %d", err);
  }
  rf.standby();
  rf.startReceive();
  xSemaphoreGive(lk);
}

size_t try_receive(uint8_t *buf, size_t max_size,
                   SemaphoreHandle_t lk, TickType_t timeout_tick,
                   LLCC68 &rf) {
  const auto TAG = "try_receive";
  // https://www.freertos.org/a00122.html
  if (xSemaphoreTake(lk, timeout_tick) != pdTRUE) {
    ESP_LOGE(TAG, "failed to take rf_lock");
    return 0;
  }
  auto length = rf.getPacketLength(true);
  if (length > max_size) {
    ESP_LOGE(TAG, "packet length %d > %d max buffer size", length, max_size);
    xSemaphoreGive(lk);
    return 0;
  }
  auto err = rf.readData(buf, length);
  std::string irq_status_str;
  auto status = rf.getIrqStatus();
  if (status & RADIOLIB_SX126X_IRQ_TIMEOUT) {
    irq_status_str += "t";
  }
  if (status & RADIOLIB_SX126X_IRQ_RX_DONE) {
    irq_status_str += "r";
  }
  if (status & RADIOLIB_SX126X_IRQ_CRC_ERR) {
    irq_status_str += "c";
  }
  if (status & RADIOLIB_SX126X_IRQ_HEADER_ERR) {
    irq_status_str += "h";
  }
  if (status & RADIOLIB_SX126X_IRQ_TX_DONE) {
    irq_status_str += "x";
  }
  if (!irq_status_str.empty()) {
    ESP_LOGI(TAG, "flag=%s", irq_status_str.c_str());
  }
  if (err != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "failed to read data, code %d", err);
    xSemaphoreGive(lk);
    return 0;
  }
  xSemaphoreGive(lk);
  return length;
}

struct handle_message_callbacks_t {
  std::function<void(uint8_t *data, size_t size, std::chrono::milliseconds)> schedule = nullptr;
  std::function<void(uint8_t *data, size_t size)> send                                = nullptr;
  std::function<etl::optional<blue::HeartMonitor>()> get_device                       = nullptr;
  std::function<void(HrLoRa::name_map_key_t)> set_name_map_key                        = nullptr;
  std::function<HrLoRa::name_map_key_t()> get_name_map_key                            = nullptr;
};

/**
 * @brief handle the message received from LoRa
 * @param data the data received
 * @param size the size of the data
 * @param callbacks the callbacks to handle the message. This function would do nothing if any of the callback is empty.
 */
void handle_message(uint8_t *data, size_t size, const handle_message_callbacks_t &callbacks) {
  const auto TAG   = "recv";
  bool is_cb_empty = callbacks.send == nullptr ||
                     callbacks.schedule == nullptr ||
                     callbacks.get_device == nullptr ||
                     callbacks.set_name_map_key == nullptr ||
                     callbacks.get_name_map_key == nullptr;
  if (is_cb_empty) {
    ESP_LOGE(TAG, "at least one callback is empty");
    return;
  }
  auto is_my_address = [](const HrLoRa::addr_t &req_addr) {
    auto my_addr        = NimBLEDevice::getAddress();
    auto my_addr_native = my_addr.getNative();
    bool is_broadcast   = std::equal(req_addr.begin(), req_addr.end(), HrLoRa::broadcast_addr.data());
    bool eq             = is_broadcast || std::equal(req_addr.begin(), req_addr.end(), my_addr_native);
    return eq;
  };
  auto get_device_status = [&callbacks]() {
    auto my_addr        = NimBLEDevice::getAddress();
    auto my_addr_native = my_addr.getNative();
    auto status         = HrLoRa::repeater_status::t{
                .repeater_addr = HrLoRa::addr_t{},
                .key           = callbacks.get_name_map_key(),
    };
    std::copy(my_addr_native, my_addr_native + HrLoRa::BLE_ADDR_SIZE, status.repeater_addr.data());
    auto device = callbacks.get_device();
    if (device) {
      auto dev = HrLoRa::hr_device::t{};
      std::copy(device->addr.begin(), device->addr.end(), dev.addr.data());
      dev.name = device->name;
    } else {
      status.device = etl::nullopt;
    }
    return status;
  };

  static auto rng = etl::random_xorshift(esp_random());
  auto magic      = data[0];
  switch (magic) {
    case HrLoRa::query_device_by_mac::magic: {
      auto r = HrLoRa::query_device_by_mac::unmarshal(data, size);
      if (!r) {
        ESP_LOGE(TAG, "failed to unmarshal query_device_by_mac");
        break;
      }
      auto &req = r.value();
      if (!is_my_address(req.addr)) {
        ESP_LOGI(TAG, "%s is not for me", utils::toHex(req.addr.data(), req.addr.size()).c_str());
        break;
      }
      auto status     = get_device_status();
      uint8_t buf[64] = {0};
      auto sz         = HrLoRa::repeater_status::marshal(status, buf, sizeof(buf));
      if (sz == 0) {
        ESP_LOGE(TAG, "failed to marshal query_device_by_mac_response");
        break;
      }
      auto interval = std::chrono::milliseconds{rng.range(0, common::MAX_RF_MSG_SCHEDULE_DELAY_MS)};
      callbacks.schedule(buf, sz, interval);
      break;
    }
    case HrLoRa::set_name_map_key::magic: {
      auto r = HrLoRa::set_name_map_key::unmarshal(data, size);
      if (!r) {
        ESP_LOGE(TAG, "failed to unmarshal set_name_map_key");
        break;
      }
      auto &req = r.value();
      callbacks.set_name_map_key(req.key);
      app_nvs::set_name_map_key(req.key);
      ESP_LOGI(TAG, "set name map key to %d", req.key);
      // send the new status back after setting the name map key
      uint8_t buf[64] = {0};
      auto status     = get_device_status();
      auto sz         = HrLoRa::repeater_status::size_needed(status);
      if (sz == 0) {
        ESP_LOGE(TAG, "failed to marshal repeater_status");
        break;
      }
      callbacks.send(buf, sz);
      break;
    }
    case HrLoRa::hr_data::magic:
    case HrLoRa::repeater_status::magic: {
      // from other repeater. do nothing.
      return;
    }
    default: {
      ESP_LOGW("recv", "unknown magic: 0x%02x", magic);
    }
  }
}

void app_main() {
  using namespace common;
  using namespace blue;
  const auto TAG = "main";
  ESP_LOGI(TAG, "boot");

  // https://github.com/h2zero/esp-nimble-cpp/blob/4e65ce5d32a458b285c536f680edc550c60aeb92/examples/Bluetooth_5/NimBLE_extended_server/main/main.cpp
  auto err = app_nvs::nvs_init();
  ESP_ERROR_CHECK(err);
  app_nvs::addr_t addr{0};
  bool has_addr = false;
  err           = app_nvs::get_addr(&addr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "no device addr, fallback back to nullptr; reason %s (%d);", esp_err_to_name(err), err);
  } else {
    ESP_LOGI(TAG, "addr=%s", utils::toHex(addr.data(), addr.size()).c_str());
    has_addr = true;
  }

  /**
   * @brief a key that is used to map the name of the device to a number
   */
  static auto name_map_key = HrLoRa::name_map_key_t{0};
  auto name_map_key_ptr    = &name_map_key;
  err                      = app_nvs::get_name_map_key(name_map_key_ptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "no name map key, fallback back to 0; reason %s (%d);", esp_err_to_name(err), err);
  } else {
    ESP_LOGI(TAG, "name map key=%d", *name_map_key_ptr);
  }

#ifndef DISABLE_LORA
  static auto hal = ESPHal(pin::SCK, pin::MISO, pin::MOSI);
  hal.init();
  ESP_LOGI(TAG, "hal init success!");
  static auto module = Module(&hal, pin::CS, pin::DIO1, pin::RST, pin::BUSY);
  static auto rf     = LLCC68(&module);
  auto st            = rf.begin(433.2, 500.0, 10, 7,
                                RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                                22, 8, 1.6);
  if (st != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "RF begin failed, code %d", st);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
  }
  ESP_LOGI(TAG, "RF began!");

  /** Radio receive interruption */
  struct rf_recv_interrupt_data_t {
    EventGroupHandle_t evt_grp;
  };
  auto evt_grp = xEventGroupCreate();

  /**
   * used in `rf.setPacketReceivedAction`
   * a workaround to pass an argument to ISR.
   * Actually ESP IDF has a API to pass an argument to ISR (`gpio_isr_handler_add`),
   * but RadioLib doesn't use it and I don't want to break the API (though I can).
   */
  static auto rf_recv_interrupt_data = rf_recv_interrupt_data_t{evt_grp};
  rf.setPacketReceivedAction([]() {
    // https://www.freertos.org/xEventGroupSetBitsFromISR.html
    BaseType_t task_woken = pdFALSE;
    auto xResult          = xEventGroupSetBitsFromISR(rf_recv_interrupt_data.evt_grp, RecvEvt, &task_woken);
    if (xResult != pdFAIL) {
      portYIELD_FROM_ISR(task_woken);
    }
  });

  rf.standby();
  rf.startReceive();
#endif
  auto *rf_lock = xSemaphoreCreateMutex();

  NimBLEDevice::init(BLE_NAME);
  auto &server          = *NimBLEDevice::createServer();
  static auto server_cb = ServerCallbacks();
  server.setCallbacks(&server_cb);

  static auto scan_manager = ScanManager();
  auto &hr_service         = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  // repeat the data from the connected device
  auto &hr_char        = *hr_service.createCharacteristic(BLE_CHAR_HR_CHAR_UUID,
                                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  auto &white_char     = *hr_service.createCharacteristic(BLE_CHAR_WHITE_LIST_UUID,
                                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto &device_char    = *hr_service.createCharacteristic(BLE_CHAR_DEVICE_UUID,
                                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  static auto white_cb = WhiteListCallback();
  white_char.setCallbacks(&white_cb);
  white_cb.on_request_address = []() {
    return scan_manager.get_target_addr();
  };
  white_cb.on_disconnect = []() {
    scan_manager.set_target_addr(etl::nullopt);
  };
  white_cb.on_address = [](WhiteListCallback::addr_opt_t addr) {
    if (addr.has_value()) {
      scan_manager.set_target_addr(etl::make_optional(addr.value().addr));
    } else {
      scan_manager.set_target_addr(etl::nullopt);
    }
  };

  /**
   * @brief should always allocate on heap when using `run_recv_task`
   */
  struct recv_task_param_t {
    std::function<void(LLCC68 &)> task;
    LLCC68 *rf;
    TaskHandle_t handle;
    EventGroupHandle_t evt_grp;
  };

  static auto send_scheduler = SendScheduler();
#ifndef DISABLE_LORA
  send_scheduler.send = [rf_lock](uint8_t *data, size_t size) {
    try_transmit(data, size, rf_lock, send_lk_timeout_tick, rf);
  };
  static auto handle_message_callbacks = handle_message_callbacks_t{
      .schedule         = [](uint8_t *data, size_t size, std::chrono::milliseconds interval) { send_scheduler.schedule(data, size, interval); },
      .send             = [rf_lock](uint8_t *data, size_t size) { try_transmit(data, size, rf_lock, send_lk_timeout_tick, rf); },
      .get_device       = []() { return scan_manager.get_device(); },
      .set_name_map_key = [name_map_key_ptr](HrLoRa::name_map_key_t key) { *name_map_key_ptr = key; },
      .get_name_map_key = [name_map_key_ptr]() { return *name_map_key_ptr; },
  };
#else
  send_scheduler.send                  = [](uint8_t *data, size_t size) {};
  static auto handle_message_callbacks = handle_message_callbacks_t{
      .schedule         = [](uint8_t *data, size_t size, std::chrono::milliseconds interval) {},
      .send             = [](uint8_t *data, size_t size) {},
      .get_device       = []() { return etl::nullopt; },
      .set_name_map_key = [name_map_key_ptr](HrLoRa::name_map_key_t key) { *name_map_key_ptr = key; },
      .get_name_map_key = [name_map_key_ptr]() { return *name_map_key_ptr; },
  };
#endif

#ifndef DISABLE_LORA
  auto recv_task = [evt_grp, rf_lock](LLCC68 &rf) {
    const auto TAG = "recv";
    for (;;) {
      xEventGroupWaitBits(evt_grp, RecvEvt, pdTRUE, pdFALSE, portMAX_DELAY);
      uint8_t data[255];
      auto size = try_receive(data, sizeof(data), rf_lock, send_lk_timeout_tick, rf);
      if (size == 0) {
        continue;
      } else {
        ESP_LOGI(TAG, "data=%s(%d)", utils::toHex(data, size).c_str(), size);
      }
      handle_message(data, size, handle_message_callbacks);
    }
  };

  /**
   * a helper function to run a function on a new FreeRTOS task
   */
  auto run_recv_task = [](void *pvParameter) {
    auto param = reinterpret_cast<recv_task_param_t *>(pvParameter);
    [[unlikely]] if (param->task != nullptr && param->rf != nullptr) {
      param->task(*param->rf);
    } else {
      ESP_LOGW("recv task", "bad precondition");
    }
    auto handle = param->handle;
    delete param;
    vTaskDelete(handle);
  };
  static auto recv_param = recv_task_param_t{recv_task, &rf, nullptr, evt_grp};

  scan_manager.on_result = [&device_char](std::string device_name, const uint8_t *addr) {
    uint8_t buf[32]                 = {0};
    auto TAG                        = "on_result";
    auto ostream                    = pb_ostream_from_buffer(buf, sizeof(buf));
    ::bluetooth_device_pb device_pb = bluetooth_device_pb_init_zero;
    device_pb.mac.funcs.encode      = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
      const auto addr_ptr = reinterpret_cast<const uint8_t *>(*arg);
      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      }
      return pb_encode_string(stream, addr_ptr, white_list::BLE_MAC_ADDR_SIZE);
    };
    device_pb.mac.arg           = const_cast<uint8_t *>(addr);
    const auto target_name_size = sizeof(device_pb.name) - 1;
    if (device_name.size() > target_name_size) {
      device_name.resize(target_name_size);
      ESP_LOGW(TAG, "Truncated device name to %s", device_name.c_str());
    }
    std::memcpy(device_pb.name, device_name.c_str(), device_name.size());
    auto ok = pb_encode(&ostream, bluetooth_device_pb_fields, &device_pb);
    if (!ok) {
      ESP_LOGE(TAG, "Failed to encode the device");
      return;
    }
    auto sz = ostream.bytes_written;
    device_char.setValue(buf, sz);
    device_char.notify();
  };
#endif

  static uint32_t on_data_counter = 0;
  scan_manager.on_data            = [&hr_char, name_map_key_ptr, rf_lock](HeartMonitor &device, uint8_t *data, size_t size) {
    const auto TAG = "scan_manager";
    // ESP_LOGI(TAG, "data: %s", utils::toHex(data, size).c_str());
    // https://community.home-assistant.io/t/ble-heartrate-monitor/300354/43
    // 3.103 Heart Rate Measurement of GATT Specification Supplement
    // first byte is a struct of some flags
    // if bit 0 of the first byte is 0, it's uint8
    if (size < 2) {
      ESP_LOGW(TAG, "bad data size: %d", size);
      return;
    }
    int hr;
    if ((data[0] & 0b1) == 0) {
      hr = data[1];
    } else {
      // if bit 0 of the first byte is 1, it's uint16 and it's little endian
      hr = ::le16dec(data + 1);
    }
    if (hr > 255) {
      ESP_LOGW(TAG, "hr overflow; cap to 255;");
      hr = 255;
    }

    uint8_t buf[48] = {0};

    size_t sz;
    if (hr <= 0) {
      ESP_LOGW(TAG, "hr=%d; skip;", hr);
      return;
    } else {
      ESP_LOGI(TAG, "hr=%d", hr);
    }

    if (on_data_counter % common::INTERVAL_SEND_NAMED_HR_COUNT == 0) {
      auto named_hr_data = HrLoRa::named_hr_data::t{
                     .key = *name_map_key_ptr,
                     .hr  = static_cast<uint8_t>(hr),
      };
      std::copy(device.addr.begin(), device.addr.end(), named_hr_data.addr.begin());
      sz = HrLoRa::named_hr_data::marshal(named_hr_data, buf, sizeof(buf));
      if (sz == 0) {
        ESP_LOGE(TAG, "failed to marshal named_hr_data");
        return;
      }
    } else {
      auto hr_data = HrLoRa::hr_data::t{
                     .key = *name_map_key_ptr,
                     .hr  = static_cast<uint8_t>(hr),
      };
      sz = HrLoRa::hr_data::marshal(hr_data, buf, sizeof(buf));
      if (sz == 0) {
        ESP_LOGE(TAG, "failed to marshal hr_data");
        return;
      }
    }

    on_data_counter += 1;

#ifndef DISABLE_LORA
    rf.standby();
    // for LoRa we encode the data as `HrLoRa::hr_data`
    try_transmit(buf, sz, rf_lock, send_lk_timeout_tick, rf);
#else
    auto a = rf_lock;
#endif
    // for Bluetooth LE character we just repeat the data
    hr_char.setValue(data, size);
    hr_char.notify();
  };

  /**
   * the server should be started before scanning and advertising
   */
  hr_service.start();
  server.start();
  NimBLEDevice::startAdvertising();

  if (has_addr) {
    scan_manager.set_target_addr(etl::make_optional(addr));
  }

  scan_manager.start_scanning_task();
#ifndef DISABLE_LORA
  xTaskCreate(run_recv_task, "recv_task", 4096, &recv_param, 0, &recv_param.handle);
#endif
  vTaskDelete(nullptr);
}
