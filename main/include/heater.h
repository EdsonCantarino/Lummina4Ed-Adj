#ifndef MAIN_INCLUDE_HEATER_H_
#define MAIN_INCLUDE_HEATER_H_

#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "config.h"

esp_err_t heater_setup();
esp_err_t heater_start();
esp_err_t heater_stop();

float get_heater_temperature();

// Roda a decisao liga/desliga do aquecedor + logs de status + logica de
// estabilizacao. Chamada direto por check_temperature_task (temperature.cpp)
// logo apos cada leitura do DS18B20 - antes era uma task separada
// consumindo de um message buffer, mas so tinha um unico consumidor e a
// task tinha um vTaskDelay(3000) fixo no fim do loop, dessincronizado da
// task produtora depois que a leitura do sensor ficou mais rapida (achado
// em 27/08). Fundir elimina a defasagem e o handoff extra entre tasks.
void process_heater_temperature(float temperature);

bool check_if_heater_temperature_stabilized();

void set_heater_controlling(bool status);

float get_target_temperature();
float get_min_temperature(bool is_in_test);
float get_max_temperature(bool is_in_test);
bool check_temperature_status(bool is_in_test);
bool check_if_heater_temperature_stabilized_ampoules();
bool heater_has_reached_target();
void reset_heater_reached_target();

bool is_temperature_in_range();


#endif
