//
// Created by Kurosu Chan on 2023/11/2.
//
#include "app_nvs.h"

namespace nvs {
esp_err_t get_addr(nvs::addr_t *addr_ptr) {
  auto TAG      = "get addr";
  esp_err_t err = 0;
  auto handle   = nvs::open_nvs_handle(common::PREF_PARTITION_LABEL, NVS_READONLY, &err);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "failed to open nvs handle, reason %s (%d)", esp_err_to_name(err), err);
    return err;
  }
  err = handle->get_blob(common::PREF_ADDR_BLOB_KEY, addr_ptr, ADDR_SIZE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "failed to get blob from nvs, reason %s (%d)", esp_err_to_name(err), err);
    return err;
  }
  return ESP_OK;
}

esp_err_t set_addr(const addr_t &addr) {
  const auto TAG = "set addr";
  esp_err_t err  = 0;
  auto handle    = nvs::open_nvs_handle(common::PREF_PARTITION_LABEL, NVS_READWRITE, &err);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "failed to open nvs handle, reason %s (%d)", esp_err_to_name(err), err);
    return err;
  }
  err = handle->set_blob(common::PREF_ADDR_BLOB_KEY, addr.data(), ADDR_SIZE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "failed to set blob from nvs, reason %s (%d)", esp_err_to_name(err), err);
    return err;
  }
  return ESP_OK;
}
esp_err_t nvs_init() {
  auto TAG = "WlanManager::init";
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Failed to erase NVS");
    ret = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(ret, TAG, "Failed to init NVS");
  return ESP_OK;
}
}
