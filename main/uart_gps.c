#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_compiler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#define _UART_DEBUG

#ifdef _UART_DEBUG
#define LOGE(...) fprintf(stderr, _LOG __VA_ARGS__);
#define LOGI(...) fprintf(stdout, _LOG __VA_ARGS__);
#define LOGD(...) fprintf(stdout, _LOG __VA_ARGS__);
#else
#define LOGE(...)
#define LOGI(...)
#define LOGD(...)
#endif

#define GPS_TXD (GPIO_NUM_21)
#define GPS_RXD (GPIO_NUM_20)
#define GPS_UART_PORT UART_NUM_1
#define GPS_BAUD_RATE 9600

// this is worth to make it more generic later
// and use macro like this: [nmea_##data_type]
typedef enum {
  nmea_unknown = NMEA_UNKNOWN,
  nmea_gpgga = NMEA_GPGGA,
  nmea_gpgll = NMEA_GPGLL,
  nmea_gpgsa = NMEA_GPGSA,
  nmea_gpgsv = NMEA_GPGSV,
  nmea_gprmc = NMEA_GPRMC,
  nmea_gptxt = NMEA_GPTXT,
  nmea_gpvtg = NMEA_GPVTG
} nmea_low_t;

//#define NMEA_CAST_DATA(data_type, data) ((nmea_##data_type##_s *)data)

#define NMEA_UART_DATA_SIZE (NMEA_GPVTG + 1)

static void *nmea_uart_data[NMEA_UART_DATA_SIZE] = {
    [nmea_unknown] = NULL, [nmea_gpgga] = NULL, [nmea_gpgll] = NULL,
    [nmea_gpgsa] = NULL,   [nmea_gpgsv] = NULL, [nmea_gprmc] = NULL,
    [nmea_gptxt] = NULL,   [nmea_gpvtg] = NULL,
};

const uart_config_t uart_config = {
    .baud_rate = GPS_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

void init_gps_uart(void) {
  uart_driver_install(GPS_UART_PORT, 256 * 2, 0, 0, NULL, 0);
  uart_param_config(GPS_UART_PORT, &uart_config);
  uart_set_pin(GPS_UART_PORT, GPS_TXD, GPS_RXD, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
}

static void gps_data(nmea_s *data) {
  // char buf[255];

  if (NULL != data) {
    if (0 < data->errors) {
      //LOGI("WARN: The sentence struct contains parse errors!\n");
      return;
    }

    if (nmea_uart_data[data->type])
      nmea_free(nmea_uart_data[data->type]);

    nmea_uart_data[data->type] = data;
  }
}

#define endline(x) unlikely((x) == '\0' || (x) == '\n')

static void gps_parse(const char *str) {
  char *start = (char *)str;
  static char cpy[100] = {0};
  static uint8_t index = 0;

  // Sync
  while (*start != '$' && *start != '\0')
    start++;

  if (endline(*start))
    return; // Empty message

  while (true) {
    index = 0;
    memset(cpy, 0, sizeof(cpy));

    while (true) {
      if (endline(*start)) {
        start++;
        cpy[index] = '\n';

        if (endline(*start))
          goto last_row;
        break;
      }
      cpy[index++] = *start;
      start++;
    }

    // If data lenght is too long or not start with '$', skip to next line
    if (unlikely(index >= sizeof(cpy) - 1 || cpy[0] != '$'))
      continue;

    gps_data(nmea_parse(cpy, strlen(cpy), 0));
  }

last_row:
  gps_data(nmea_parse(cpy, strlen(cpy), 0));
}

#define LENGHT_BUFFER 350

void gps_read_task() {
  static uint8_t *data = NULL;

  if (data == NULL) {
    data = (uint8_t *)malloc(LENGHT_BUFFER);
  }

  int len = uart_read_bytes(GPS_UART_PORT, data, LENGHT_BUFFER - 1,
                            20 / portTICK_PERIOD_MS);
  if (len > 0) {
    LOGI("\n\n\n%s\n\n\n\n", (char *)data);
    data[len] = '\0';
    gps_parse((char *)data);
  }
}