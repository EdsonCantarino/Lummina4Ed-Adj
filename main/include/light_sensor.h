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

// Troca o canal ativo do ADC sem disparar a leitura ainda - permite dar mais
// tempo de assentamento analogico entre a troca e a captura real (ver
// ampoule_test.cpp::prepare_test, item CHANNEL_SWITCH_DELAY_MS). Quem nao
// precisa desse controle fino continua usando so read_channel_value(), que
// troca e le em sequencia como sempre fez.
void light_sensor_select_channel(uint8_t channel);

// Le o canal que ja foi selecionado por light_sensor_select_channel() -
// nao troca o canal de novo.
long light_sensor_read_selected_channel(uint8_t channel);

// Numero de leituras seguidas em que o canal precisou "assumir" a ultima
// leitura valida por timeout do DRDY. Reseta para 0 assim que uma leitura
// completa com sucesso.
uint8_t get_channel_consecutive_timeouts(uint8_t channel);
void reset_channel_consecutive_timeouts(uint8_t channel);

#endif
