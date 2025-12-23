#ifndef __UART_GPS_H__
#define __UART_GPS_H__


#include "driver/uart.h"

#include "gpgga.h"
#include "gpgll.h"
#include "gpgsa.h"
#include "gpgsv.h"
#include "gprmc.h"
#include "gptxt.h"
#include "gpvtg.h"
#include "nmea.h"

void init_gps_uart(void);
void gps_read_task();

#endif