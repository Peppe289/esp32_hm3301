// Enable `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2`
// Version 1 is deprecated (from docs).
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "i2c_hm3301.h"

static i2c_master_dev_handle_t dev_handle;
static i2c_master_bus_handle_t bus_handle;

static const i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = HM3301_DEV_ADDR,
    .scl_speed_hz = 100000,
    .flags.disable_ack_check = false,
};

static const i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .scl_io_num = HM3301_SCL_IO_NUM,
    .sda_io_num = HM3301_SDA_IO_NUM,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

uint8_t i2c_hm3301_read(uint8_t *raw_data, struct hm3301_pm *hm3301) {
  uint8_t data_rd[HM3301_BIT_LEN] = {0};
  uint8_t cmd = HM3301_READ_CMD;
  esp_err_t err;
  uint16_t sum = 0;

  // Send command code to read (0x88) then read (29 byte)
  err = i2c_master_transmit_receive(dev_handle, &cmd, 1, data_rd,
                                    HM3301_BIT_LEN, pdMS_TO_TICKS(100));
  if (err != ESP_OK) {
    printf("I2C Read-Receive fallito con codice: %d", err);
    return -1;
  }

  for (int i = 0; i < (HM3301_BIT_LEN - 1); i++) {
    sum += data_rd[i];
  }

  if (((uint8_t)(sum & 0xFF)) != data_rd[HM3301_BIT_LEN - 1]) {
    printf("Checksum Failed");
    return -1;
  }

  if (raw_data != NULL) {
    memcpy(raw_data, data_rd, sizeof(data_rd));
  }

  if (!hm3301) {
    return -1;
  }

  hm3301->pm1_0 = HM3301_PM1_0(data_rd);
  hm3301->pm2_5 = HM3301_PM2_5(data_rd);
  hm3301->pm10 = HM3301_PM10(data_rd);

  return 0;
}

void init_i2c_hm3301() {
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
}