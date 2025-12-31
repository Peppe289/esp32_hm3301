#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sys/_pthreadtypes.h>

#include "ble_gatts.h"
#include "portmacro.h"
#include "wifi_conn.h"

#undef ESP_LOGI
#define ESP_LOGI(...)

void *manageConnection(void *args) {
  for (;;) {
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // If the wifi is connected or not yet all is fine.
    if (isWiFiConnecting()) {
      ESP_LOGI("MAIN", "WIFI IS CONNECTED.");
      continue;
    }

    // If the ssid and password have data, try WIFI connection.
    // Then clear up for the next connection.
    if (ssid[0] != '\0' && password[0] != '\0') {
      disable_bt();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      ESP_LOGI("MAIN", "TRY TO START WIFI CONNECTION");
      wifi_init_sta(ssid, password);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      memset(ssid, 0, sizeof(ssid));
      memset(password, 0, sizeof(password));
      continue;
    }

    // If the wifi isn't connected enable or keep alive the BT.
    if (!isBTEnabled()) {
      ESP_LOGI("MAIN", "BT DISABLED. START IT.");
      disableWIFI();
      start_bt();
    } else {
      ESP_LOGI("MAIN", "BT IS ENABLED");
    }
  }

  return NULL;
}

void connection_listener_start(void) {
  esp_err_t ret;
  pthread_t tconfig;

  pthread_create(&tconfig, NULL, manageConnection, NULL);

  // Initialize NVS.
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
     ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
   ESP_ERROR_CHECK(nvs_flash_erase());
   ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  // Don't wait for ending process
  // pthread_join(tconfig, NULL);
}