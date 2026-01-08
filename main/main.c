#include "connection/conn_thread.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_hm3301.h"
#include "nmea.h"
#include "spi_flash_mmap.h"
#include "uart_gps.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <unistd.h>

#include "esp_log.h"

static const char *TAG = "MAIN";

#include "cJSON.h"
#include "connection/mqtt/conn_mqtt_client.h"

static char *getString(nmea_uart_data_s *gps_data, struct hm3301_pm *hm3301) {
  cJSON *root;
  cJSON *pm, *position, *longitude, *latitude, *time;
  char *string;

  if (gps_data || hm3301)
    root = cJSON_CreateObject();
  else
    return NULL;

  if (gps_data) {
    cJSON_AddNumberToObject(root, "satellites", gps_data->n_satellites);

    position = cJSON_AddObjectToObject(root, "position");
    longitude = cJSON_AddObjectToObject(position, "longitude");
    latitude = cJSON_AddObjectToObject(position, "latitude");
    cJSON_AddNumberToObject(longitude, "degrees",
                            gps_data->position.longitude.degrees);
    cJSON_AddNumberToObject(longitude, "minutes",
                            gps_data->position.longitude.minutes);
    cJSON_AddStringToObject(
        longitude, "cardinal",
        (char[]){gps_data->position.longitude.cardinal, '\0'});
    cJSON_AddNumberToObject(latitude, "degrees",
                            gps_data->position.latitude.degrees);
    cJSON_AddNumberToObject(latitude, "minutes",
                            gps_data->position.latitude.minutes);
    cJSON_AddStringToObject(
        latitude, "cardinal",
        (char[]){gps_data->position.latitude.cardinal, '\0'});
    time = cJSON_AddObjectToObject(root, "time");
    cJSON_AddNumberToObject(time, "hours", gps_data->time.tm_hour);
    cJSON_AddNumberToObject(time, "minutes", gps_data->time.tm_min);
    cJSON_AddNumberToObject(time, "seconds", gps_data->time.tm_sec);
  }

  if (hm3301) {
    pm = cJSON_AddObjectToObject(root, "hm3301");
    cJSON_AddNumberToObject(pm, "PM1.0", hm3301->pm1_0);
    cJSON_AddNumberToObject(pm, "PM2.5", hm3301->pm2_5);
    cJSON_AddNumberToObject(pm, "PM10", hm3301->pm10);
  }

  string = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return string;
}

void app_main(void) {
  uint8_t data_rd[HM3301_BIT_LEN];
  struct hm3301_pm *hm3301 = NULL;
  nmea_uart_data_s *nmea_gps = NULL;
  char *json_string;

  init_i2c_hm3301();
  init_gps_uart();
  connection_listener_start();

  vTaskDelay(pdMS_TO_TICKS(1000));

  for (;;) {
    hm3301 = malloc(sizeof(struct hm3301_pm));

    if (i2c_hm3301_read(data_rd, hm3301)) {
      ESP_LOGE(TAG, "HM3301: Error reading\n");
      continue;
    }

    if (!HM3301_HEADER_INTEGRITY(data_rd)) {
      ESP_LOGD(TAG, "HM3301: Frame out of sync\n");
      continue;
    }

    nmea_gps = gps_read_task();

    json_string = getString(nmea_gps, hm3301);

    if (hm3301) {
      free(hm3301);
      hm3301 = NULL;
    }

    if (nmea_gps) {
      free(nmea_gps);
      nmea_gps = NULL;
    }

    if (json_string) {
      ESP_LOGI(TAG, "JSON\n: %s\n", json_string);
      publish((const char *)json_string);
      free(json_string);
      json_string = NULL;
    }

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
