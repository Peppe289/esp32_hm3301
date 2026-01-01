idf_component_register(SRCS "i2c_hm3301.c" "main.c" "uart_gps.c" "connection/wifi_conn.c" "connection/ble_gatts.c" "connection/conn_thread.c" "connection/mqtt/mqtt_client.c"
                    INCLUDE_DIRS "."
                    REQUIRES libnmea spi_flash driver esp_driver_i2c bt nvs_flash esp_wifi mqtt esp-tls)