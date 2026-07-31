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

// Numero de leituras seguidas em que o canal precisou "assumir" a ultima
// leitura valida por timeout do DRDY. Reseta para 0 assim que uma leitura
// completa com sucesso.
uint8_t get_channel_consecutive_timeouts(uint8_t channel);
void reset_channel_consecutive_timeouts(uint8_t channel);

#endif
