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

#include "cJSON.h"

static char *getString(nmea_uart_data_s *gps_data, struct hm3301_pm *hm3301) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "satellites", gps_data->n_satellites);

  if (gps_data) {
    cJSON *position = cJSON_AddObjectToObject(root, "position");
    cJSON *longitude = cJSON_AddObjectToObject(position, "longitude");
    cJSON *latitude = cJSON_AddObjectToObject(position, "latitude");
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
    cJSON *time = cJSON_AddObjectToObject(root, "time");
    cJSON_AddNumberToObject(time, "hours", gps_data->time.tm_hour);
    cJSON_AddNumberToObject(time, "minutes", gps_data->time.tm_min);
    cJSON_AddNumberToObject(time, "seconds", gps_data->time.tm_sec);
  }

  if (hm3301) {
    cJSON *pm = cJSON_AddObjectToObject(root, "hm3301");
    cJSON_AddNumberToObject(pm, "PM1.0", hm3301->pm1_0);
    cJSON_AddNumberToObject(pm, "PM2.5", hm3301->pm2_5);
    cJSON_AddNumberToObject(pm, "PM10", hm3301->pm10);
  }

  char *string = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return string;
}

void app_main(void) {
  uint8_t data_rd[HM3301_BIT_LEN];
  struct hm3301_pm *hm3301 = NULL;
  nmea_uart_data_s *nmea_gps = NULL;

  init_i2c_hm3301();
  init_gps_uart();
  connection_listener_start();

  vTaskDelay(pdMS_TO_TICKS(1000));

  for (;;) {
    hm3301 = malloc(sizeof(struct hm3301_pm));
    if (i2c_hm3301_read(data_rd, hm3301)) {
      printf("Errore nella lettura\n");
    }

    if (!HM3301_HEADER_INTEGRITY(data_rd)) {
      printf("Frame desincronizzato\n");
    }

    for (int i = 0; i < HM3301_BIT_LEN; i++) {
      printf("%02X ", data_rd[i]);
    }
    printf("\n");
    if (hm3301) {
      printf("PM1.0=%u PM2.5=%u PM10=%u\n", hm3301->pm1_0, hm3301->pm2_5,
             hm3301->pm10);
    }
    printf("===============================\n");
    nmea_gps = gps_read_task();

    if (nmea_gps) {
      printf("NMEA Satelliti: %d\n", nmea_gps->n_satellites);
      printf("Longitude:\n");
      printf("  Degrees: %d\n", nmea_gps->position.longitude.degrees);
      printf("  Minutes: %f\n", nmea_gps->position.longitude.minutes);
      printf("  Cardinal: %c\n", (char)nmea_gps->position.longitude.cardinal);
      printf("Latitude:\n");
      printf("  Degrees: %d\n", nmea_gps->position.latitude.degrees);
      printf("  Minutes: %f\n", nmea_gps->position.latitude.minutes);
      printf("  Cardinal: %c\n", (char)nmea_gps->position.latitude.cardinal);

      char buf[100];
      if (strftime(buf, sizeof(buf), "%H:%M:%S",
                   (const struct tm *)&(nmea_gps->time))) {
        printf("Time: %s\n", buf);
      }
      char *json_string = getString(nmea_gps, hm3301);
      printf("JSON: %s\n", json_string);
      free(json_string);
      free(nmea_gps);
      nmea_gps = NULL;
      free(hm3301);
      hm3301 = NULL;
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
}
