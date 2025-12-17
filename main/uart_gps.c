#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>

#include "nmea.h"
#include "gpgll.h"
#include "gpgga.h"
#include "gprmc.h"
#include "gpgsa.h"
#include "gpvtg.h"
#include "gptxt.h"
#include "gpgsv.h"

#define GPS_TXD (GPIO_NUM_21)
#define GPS_RXD (GPIO_NUM_20)
#define GPS_UART_PORT UART_NUM_1
#define GPS_BAUD_RATE 9600

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

void gps_parse(const char *str)
{
	char *start = str;
	uint8_t run = true;
	nmea_s *data;

	// Sync
	while (*start != '$' && *start != '\0')
	{
		start++;
	}
	if (*start == '\0')
		return; // Message not found

	while (run)
	{
		char buf[255];
		char cpy[255] = {0};
		int index = 0;

		while (true)
		{
			if (*start == '\n' || *start == '\0')
			{
				start++;
				if (*start == '\0')
					run = false;
				break;
			}
			if (*start != '\r')
			{
				cpy[index++] = *start;
			}
			start++;
		}

		cpy[index++] = '\r';
		cpy[index] = '\n';

		// Invalid string
		if (cpy[0] != '$')
			continue;

		data = nmea_parse(cpy, strlen(cpy), 0);
		if (NULL != data)
		{
			if (0 < data->errors)
			{
				printf("WARN: The sentence struct contains parse errors!\n");
			}

			if (NMEA_GPGGA == data->type)
			{
				printf("GPGGA sentence\n");
				nmea_gpgga_s *gpgga = (nmea_gpgga_s *)data;
				printf("Number of satellites: %d\n", gpgga->n_satellites);
				printf("Altitude: %f %c\n", gpgga->altitude, gpgga->altitude_unit);
			}

			if (NMEA_GPGLL == data->type)
			{
				printf("GPGLL sentence\n");
				nmea_gpgll_s *pos = (nmea_gpgll_s *)data;
				printf("Longitude:\n");
				printf("  Degrees: %d\n", pos->longitude.degrees);
				printf("  Minutes: %f\n", pos->longitude.minutes);
				printf("  Cardinal: %c\n", (char)pos->longitude.cardinal);
				printf("Latitude:\n");
				printf("  Degrees: %d\n", pos->latitude.degrees);
				printf("  Minutes: %f\n", pos->latitude.minutes);
				printf("  Cardinal: %c\n", (char)pos->latitude.cardinal);
				if (strftime(buf, sizeof(buf), "%H:%M:%S", &pos->time))
					printf("Time: %s\n", buf);
			}

			if (NMEA_GPRMC == data->type)
			{
				printf("GPRMC sentence\n");
				nmea_gprmc_s *pos = (nmea_gprmc_s *)data;
				printf("Longitude:\n");
				printf("  Degrees: %d\n", pos->longitude.degrees);
				printf("  Minutes: %f\n", pos->longitude.minutes);
				printf("  Cardinal: %c\n", (char)pos->longitude.cardinal);
				printf("Latitude:\n");
				printf("  Degrees: %d\n", pos->latitude.degrees);
				printf("  Minutes: %f\n", pos->latitude.minutes);
				printf("  Cardinal: %c\n", (char)pos->latitude.cardinal);
				if (strftime(buf, sizeof(buf), "%d %b %T %Y", &pos->date_time))
					printf("Date & Time: %s\n", buf);
				printf("Speed, in Knots: %f:\n", pos->gndspd_knots);
				printf("Track, in degrees: %f\n", pos->track_deg);
				printf("Magnetic Variation:\n");
				printf("  Degrees: %f\n", pos->magvar_deg);
				printf("  Cardinal: %c\n", (char)pos->magvar_cardinal);
				double adjusted_course = pos->track_deg;
				if (NMEA_CARDINAL_DIR_EAST == pos->magvar_cardinal)
				{
					adjusted_course -= pos->magvar_deg;
				}
				else if (NMEA_CARDINAL_DIR_WEST == pos->magvar_cardinal)
				{
					adjusted_course += pos->magvar_deg;
				}
				else
				{
					printf("Invalid Magnetic Variation Direction!!\n");
				}

				printf("Adjusted Track (heading): %f\n", adjusted_course);
			}

			if (NMEA_GPGSA == data->type)
			{
				nmea_gpgsa_s *gpgsa = (nmea_gpgsa_s *)data;

				printf("GPGSA Sentence:\n");
				printf("  Mode: %c\n", gpgsa->mode);
				printf("  Fix:  %d\n", gpgsa->fixtype);
				printf("  PDOP: %.2lf\n", gpgsa->pdop);
				printf("  HDOP: %.2lf\n", gpgsa->hdop);
				printf("  VDOP: %.2lf\n", gpgsa->vdop);
			}

			if (NMEA_GPGSV == data->type)
			{
				nmea_gpgsv_s *gpgsv = (nmea_gpgsv_s *)data;

				printf("GPGSV Sentence:\n");
				printf("  Num: %d\n", gpgsv->sentences);
				printf("  ID:  %d\n", gpgsv->sentence_number);
				printf("  SV:  %d\n", gpgsv->satellites);
				printf("  #1:  %d %d %d %d\n", gpgsv->sat[0].prn, gpgsv->sat[0].elevation, gpgsv->sat[0].azimuth, gpgsv->sat[0].snr);
				printf("  #2:  %d %d %d %d\n", gpgsv->sat[1].prn, gpgsv->sat[1].elevation, gpgsv->sat[1].azimuth, gpgsv->sat[1].snr);
				printf("  #3:  %d %d %d %d\n", gpgsv->sat[2].prn, gpgsv->sat[2].elevation, gpgsv->sat[2].azimuth, gpgsv->sat[2].snr);
				printf("  #4:  %d %d %d %d\n", gpgsv->sat[3].prn, gpgsv->sat[3].elevation, gpgsv->sat[3].azimuth, gpgsv->sat[3].snr);
			}

			if (NMEA_GPTXT == data->type)
			{
				nmea_gptxt_s *gptxt = (nmea_gptxt_s *)data;

				printf("GPTXT Sentence:\n");
				printf("  ID: %d %d %d\n", gptxt->id_00, gptxt->id_01, gptxt->id_02);
				printf("  %s\n", gptxt->text);
			}

			if (NMEA_GPVTG == data->type)
			{
				nmea_gpvtg_s *gpvtg = (nmea_gpvtg_s *)data;

				printf("GPVTG Sentence:\n");
				printf("  Track [deg]:   %.2lf\n", gpvtg->track_deg);
				printf("  Speed [kmph]:  %.2lf\n", gpvtg->gndspd_kmph);
				printf("  Speed [knots]: %.2lf\n", gpvtg->gndspd_knots);
			}

			nmea_free(data);
		}
	}
}

void gps_read_task() {
  uint8_t *data = (uint8_t *)malloc(256);
  int len = uart_read_bytes(GPS_UART_PORT, data, 255, 20 / portTICK_PERIOD_MS);
  if (len > 0) {
    data[len] = '\0';
    gps_parse((char *)data);
  }
  free(data);
}