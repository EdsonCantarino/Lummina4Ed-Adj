#ifndef MAIN_INCLUDE_TEMPERATURE_H_
#define MAIN_INCLUDE_TEMPERATURE_H_

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ds18x20.h>
#include <esp_log.h>
#include <esp_err.h>
#include "config.h"
#include "LinkedList.h"

using namespace std;

void temperature_setup();
string read_temperatures_calibration_json();
void refresh_calibration_factor();

#endif
