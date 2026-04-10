#ifndef INCLUDE_LIGHT_SENSOR_H_
#define INCLUDE_LIGHT_SENSOR_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cs553x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Error library
#include "esp_err.h"


void light_sensor_main();
void light_sensor_setup();

long read_channel_value(uint8_t channel);

#endif
