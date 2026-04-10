#ifndef MAIN_INCLUDE_I2C_DRIVER_H_
#define MAIN_INCLUDE_I2C_DRIVER_H_

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// I2C driver
#include "driver/i2c.h"

// Error library
#include "esp_err.h"

#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_FREQ_HZ 100000

void setup_i2c_driver();
void scan_i2c();

#endif
