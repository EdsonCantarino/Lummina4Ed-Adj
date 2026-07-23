#ifndef MAIN_INCLUDE_AMPOULE_TEST_H_
#define MAIN_INCLUDE_AMPOULE_TEST_H_

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/message_buffer.h>
#include <esp_system.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>
#include <string.h>
#include "config.h"


#include <LinkedList.h>
#include <AmpouleSensor.h>

void ampoule_test_setup();
void ampoule_set_time_test(int id, int level);

bool ampoule_any();
bool ampoule_get_status(int id);
void ampoule_set_status(int id, bool present);
void ampoule_set_status(bool ampoule1, bool ampoule2, bool ampoule3,
		bool ampoule4);

void ampoule_apply_cavity_enabled_config();

void clear_histories();
void ampoule_test_start();

bool is_test_done(int id);
bool is_testing(int id);
bool is_any_testing();
bool is_any_in_test_done();
bool is_any_present_cancelled_by_temp();

void ampoule_initial_beep(int id);

void set_is_priting(bool ispriting);
AmpouleTestResult get_test_result(int index);

string convert_ampoules_test_to_json();

bool get_is_cavities_in_test();
void ampoule_test_check_cavity();
void ampoule_test_check_cavity_finalize();

bool ampoule_is_disabled(int id);

bool ampoule_is_locked(int id);

void load_temp_histories();

void trigger_temp_out_of_range_cancel();

string convert_ampoules_test_status_to_json();

#endif
