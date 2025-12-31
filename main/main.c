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

char *getString(nmea_uart_data_s *data) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "satellites", data->n_satellites);

  cJSON *position = cJSON_AddObjectToObject(root, "position");
  cJSON *longitude = cJSON_AddObjectToObject(position, "longitude");
  cJSON *latitude = cJSON_AddObjectToObject(position, "latitude");
  cJSON_AddNumberToObject(longitude, "degrees",
                          data->position.longitude.degrees);
  cJSON_AddNumberToObject(longitude, "minutes",
                          data->position.longitude.minutes);
  cJSON_AddStringToObject(longitude, "cardinal",
                          (char[]){data->position.longitude.cardinal, '\0'});
  cJSON_AddNumberToObject(latitude, "degrees", data->position.latitude.degrees);
  cJSON_AddNumberToObject(latitude, "minutes", data->position.latitude.minutes);
  cJSON_AddStringToObject(latitude, "cardinal",
                          (char[]){data->position.latitude.cardinal, '\0'});
  cJSON *time = cJSON_AddObjectToObject(root, "time");
  cJSON_AddNumberToObject(time, "hours", data->time.tm_hour);
  cJSON_AddNumberToObject(time, "minutes", data->time.tm_min);
  cJSON_AddNumberToObject(time, "seconds", data->time.tm_sec);
  char *string = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return string;
}

void app_main(void) {
  uint8_t data_rd[HM3301_BIT_LEN];
  struct hm3301_pm ret;

  init_i2c_hm3301();
  init_gps_uart();
  connection_listener_start();

  vTaskDelay(pdMS_TO_TICKS(5000));

  while (true) {
    ret = i2c_hm3301_read(data_rd);

    if (!HM3301_HEADER_INTEGRITY(data_rd)) {
      printf("Frame desincronizzato\n");
    }

    for (int i = 0; i < HM3301_BIT_LEN; i++) {
      printf("%02X ", data_rd[i]);
    }
    printf("\n");
    printf("PM1.0=%u PM2.5=%u PM10=%u\n", ret.pm1_0, ret.pm2_5, ret.pm10);

    printf("===============================\n");
    nmea_uart_data_s *nmea_uart_data = gps_read_task();

    if (nmea_uart_data) {
      printf("NMEA Satelliti: %d\n", nmea_uart_data->n_satellites);
      printf("Longitude:\n");
      printf("  Degrees: %d\n", nmea_uart_data->position.longitude.degrees);
      printf("  Minutes: %f\n", nmea_uart_data->position.longitude.minutes);
      printf("  Cardinal: %c\n",
             (char)nmea_uart_data->position.longitude.cardinal);
      printf("Latitude:\n");
      printf("  Degrees: %d\n", nmea_uart_data->position.latitude.degrees);
      printf("  Minutes: %f\n", nmea_uart_data->position.latitude.minutes);
      printf("  Cardinal: %c\n",
             (char)nmea_uart_data->position.latitude.cardinal);

      char buf[100];
      if (strftime(buf, sizeof(buf), "%H:%M:%S",
                   (const struct tm *)&(nmea_uart_data->time))) {
        printf("Time: %s\n", buf);
      }
      char *json_string = getString(nmea_uart_data);
      printf("JSON: %s\n", json_string);
      free(json_string);
      free(nmea_uart_data);
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
}
