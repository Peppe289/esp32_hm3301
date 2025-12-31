#include "driver/uart.h"
#include "esp_compiler.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gpgga.h"
#include "gpgll.h"
#include "gpgsa.h"
#include "gpgsv.h"
#include "gprmc.h"
#include "gptxt.h"
#include "gpvtg.h"
#include "nmea.h"

#include "uart_gps.h"

#define _LOG "UART: "

// #define _UART_DEBUG

#ifdef _UART_DEBUG
#define LOGE(...)                                                              \
  {                                                                            \
    fprintf(stderr, _LOG "%s: ", __func__);                                    \
    fprintf(stderr, _LOG __VA_ARGS__);                                         \
  }
#define LOGI(...)                                                              \
  {                                                                            \
    fprintf(stdout, _LOG "%s: ", __func__);                                    \
    fprintf(stdout, _LOG __VA_ARGS__);                                         \
  }
#define LOGD(...)                                                              \
  {                                                                            \
    fprintf(stdout, _LOG "%s: ", __func__);                                    \
    fprintf(stdout, _LOG __VA_ARGS__);                                         \
  }
#else
#define LOGE(...)
#define LOGI(...)
#define LOGD(...)
#endif

#define GPS_TXD 21
#define GPS_RXD 20
#define GPS_UART_PORT UART_NUM_1
#define GPS_BAUD_RATE 9600
#define LENGHT_BUFFER 200

const uart_config_t uart_config = {
    .baud_rate = GPS_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

void init_gps_uart(void) {
  uart_driver_install(GPS_UART_PORT, LENGHT_BUFFER * 2, 0, 0, NULL, 0);
  uart_param_config(GPS_UART_PORT, &uart_config);
  uart_set_pin(GPS_UART_PORT, GPS_TXD, GPS_RXD, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
}

static void gps_data(nmea_s *data, nmea_uart_data_s *nmea_uart_data) {
  if (NULL != data) {
    if (0 < data->errors) {
      LOGD("Invalid String\n");
      return;
    }

    LOGD("DATA Type: %d\n", data->type);

    if (NMEA_GPGGA == data->type) {
      nmea_uart_data->n_satellites = ((nmea_gpgga_s *)data)->n_satellites;
      LOGD("Satellites: %d\n", ((nmea_gpgga_s *)data)->n_satellites);

      // Estrai la latitudine
      nmea_uart_data->position.latitude.degrees =
          ((nmea_gpgga_s *)data)->latitude.degrees;
      nmea_uart_data->position.latitude.minutes =
          ((nmea_gpgga_s *)data)->latitude.minutes;
      nmea_uart_data->position.latitude.cardinal =
          ((nmea_gpgga_s *)data)->latitude.cardinal;
      LOGD("Latitude: %d° %f' %c\n", nmea_uart_data->position.latitude.degrees,
           nmea_uart_data->position.latitude.minutes,
           (char)nmea_uart_data->position.latitude.cardinal);

      // Estrai la longitudine
      nmea_uart_data->position.longitude.degrees =
          ((nmea_gpgga_s *)data)->longitude.degrees;
      nmea_uart_data->position.longitude.minutes =
          ((nmea_gpgga_s *)data)->longitude.minutes;
      nmea_uart_data->position.longitude.cardinal =
          ((nmea_gpgga_s *)data)->longitude.cardinal;
      LOGD("Longitude: %d° %f' %c\n",
           nmea_uart_data->position.longitude.degrees,
           nmea_uart_data->position.longitude.minutes,
           (char)nmea_uart_data->position.longitude.cardinal);

      nmea_uart_data->time = ((nmea_gpgga_s *)data)->time;
      char buf[100];
      if (strftime(buf, sizeof(buf), "%H:%M:%S",
                   (const struct tm *)&(((nmea_gpgga_s *)data)->time))) {
        LOGD("Time: %s\n", buf);
      }
    }

    nmea_free(data);
  }
}

#define endline(x) unlikely((x) == '\0' || (x) == '\n')

static nmea_uart_data_s *gps_parse(const char *str,
                                   nmea_uart_data_s *nmea_uart_data, int len) {
  char *start = (char *)str;
  char cpy[100] = {0};
  size_t index = 0;

  // Sync
  while (*start != '$' && *start != '\0' && (len-- > 0))
    start++;

  if (endline(*start))
    return NULL; // Empty message

  while (len > 0) {
    index = 0;
    memset(cpy, 0, sizeof(cpy));
    for (;;) {
      if (endline(*start)) {
        start++;
        len--;
        cpy[index] = '\n';

        if (start == NULL || endline(*start))
          goto last_row;
        break;
      }
      cpy[index++] = *start;
      start++;
      len--;
    }

    // If data lenght is too long or not start with '$', skip to next line
    if (unlikely(index >= sizeof(cpy) - 1 || cpy[0] != '$'))
      continue;

    LOGD("Get String: %s\n", cpy);

    gps_data(nmea_parse(cpy, strlen(cpy), 0), nmea_uart_data);
  }

last_row:
  if (start != NULL) {
    LOGD("Get String: %s\n", cpy);
    gps_data(nmea_parse(cpy, strlen(cpy), 0), nmea_uart_data);
  }
  return nmea_uart_data;
}

nmea_uart_data_s *gps_read_task() {
  uint8_t data[LENGHT_BUFFER];
  nmea_uart_data_s *nmea_uart_data;
  time_t start = time(NULL);

  nmea_uart_data = malloc(sizeof(nmea_uart_data_s));
  if (nmea_uart_data == NULL) {
    LOGE("Memory allocation failed\n");
    return NULL;
  }
  memset(nmea_uart_data, 0, sizeof(nmea_uart_data_s));
  for (;;) {
    int len = uart_read_bytes(GPS_UART_PORT, data, LENGHT_BUFFER - 1,
                              20 / portTICK_PERIOD_MS);
    if (len > 0) {
      LOGI("\n\n\n%s\n\n\n\n", (char *)data);
      data[len] = '\0';
      gps_parse((char *)data, nmea_uart_data, len);
      if (nmea_uart_data->n_satellites > 0 &&
          (nmea_uart_data->position.latitude.degrees != 0 ||
           nmea_uart_data->position.longitude.degrees != 0 ||
           nmea_uart_data->position.latitude.minutes != 0 ||
           nmea_uart_data->position.longitude.minutes != 0 ||
           nmea_uart_data->position.latitude.cardinal != 0 ||
           nmea_uart_data->position.longitude.cardinal != 0)) {
        return nmea_uart_data;
      } else {
        LOGD("Data Not Valid... Try Again\n");
        if (difftime(time(NULL), start) > 5) {
          printf("GPS Time out!\n");
          free(nmea_uart_data);
          return NULL;
        }
      }
    }
  }
  return NULL;
}