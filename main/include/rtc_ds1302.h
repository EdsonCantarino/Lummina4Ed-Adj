#ifndef INCLUDE_RTC_DS1302_H_
#define INCLUDE_RTC_DS1302_H_

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ds1302.h>
#include <esp_log.h>
#include <esp_err.h>
#include "config.h"

void rtc_ds1302_adjust_timestamp(tm time);
void rtc_ds1302_get_date_time(char * buffer);
void rtc_ds1302_setup();
void rtc_ds1302_print_date_time(const struct tm time);

esp_err_t rtc_write_sram_memory(int value);
int rtc_read_sram_memory();

#endif
