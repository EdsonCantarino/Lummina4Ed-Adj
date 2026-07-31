#ifndef NVS_UTILS_H_
#define NVS_UTILS_H_

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "device_utils.h"
#include "ampoule_test_history.h"

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_init();
esp_err_t save_restart_counter(void);

esp_err_t save_device_settings(device_settings_t settings);
device_settings_t read_device_settings();

esp_err_t sync_device_settings();

extern int32_t restart_counter;

esp_err_t get_ampoule_test_counter(int32_t &ampoute_test_cont);

LinkedList<ampoule_test_history_t> load_ampoules_test();
esp_err_t save_ampoules_test(string history);

esp_err_t load_ampoules_test_history(LinkedList<string> &history);
LinkedList<ampoule_test_history_t> split_ampoules_tes_history(string histories);

esp_err_t save_calibration(float calibration_factor);
esp_err_t get_calibration_factor(float &factor);

esp_err_t save_positive_percentage(float positive_percentage);
float get_positive_percentage();

// Quantidade de resultados mais recentes a imprimir quando o botao fisico
// de imprimir e pressionado (1 a AMPOULE_HISTORY_MAX_RECORDS). Independente
// da struct advanced_config_t de proposito - assim ajustar isso nao reseta
// as demais configuracoes avancadas de unidades ja configuradas em campo.
esp_err_t save_print_count(uint8_t count);
uint8_t get_print_count();

esp_err_t save_serial_number(string serial_number);
string get_serial_number();

esp_err_t reset_user_data();

string get_institution();

string get_language();
esp_err_t save_language(string language);

esp_err_t get_new_ampoule_test_count(int32_t &ampoule_test_count);

esp_err_t save_ampoules_history_temp(string history, int index);
string load_ampoules_history_temp(int index);
esp_err_t reset_ampoules_history_temp(int index);
LinkedList<ampoule_test_history_t> load_ampoules_histories_temp();

esp_err_t save_reset_reason(string reasons);
LinkedList<string> load_reset_reason();

#ifdef __cplusplus
}
#endif

#endif /* NVS_UTILS_H_ */
