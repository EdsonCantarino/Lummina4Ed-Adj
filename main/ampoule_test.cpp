#include "include/ampoule_test.h"
#include "include/light_sensor.h"
#include "include/heater.h"
#include "include/led_uv.h"
#include "include/led_panel.h"
#include "include/ampoules.h"
#include "include/rtc_ds1302.h"
#include "include/buzzer.h"
#include "include/keyboard.h"
#include "include/task_manager.h"
#include "include/printer.h"
#include "include/nvs_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_timer.h"
#include <cmath>

#include "include/ampoule_test_history.h"
#include "include/helper_utils.h"
#include "include/TimerManager.h"

#define TASK_TIME 28000
#define TASK_TIME_DELAY 2000

#define DEFAULT_TIME_TEST (20 * 60) // 20 * 60
//#define DEFAULT_TIME_TEST (2 * 60) // 20 * 60

TimerManager timeManager;
int alarmTimeout = 30 * 60;
//int alarmTimeout = 5 * 60;

TaskHandle_t ampoules_test_task_handle = NULL;

TaskHandle_t ampoules_test_error_task_handle = NULL;

LinkedList<AmpouleSensor> ampoules = LinkedList<AmpouleSensor>();
LinkedList<string> ampoules_history = LinkedList<string>();

string ampoule_test_history_to_string(ampoule_test_history_t hist);
esp_err_t set_history_string(string h, bool auto_save, bool auto_load);

void ampoule_clear_test_done(int index);
long ampoule_get_time_test(int id);
esp_err_t ampoules_test_timer_start_task();
esp_err_t ampoules_test_timer_stop_task();

void ampoule_set_test_counter(int index);

bool is_ampoule_in_test = false;
bool is_cavities_in_test = false;

bool is_printing = false;

volatile bool temp_out_of_range_cancel = false;

void trigger_temp_out_of_range_cancel() {
	temp_out_of_range_cancel = true;
}

void load_histories() {

	load_ampoules_test_history(ampoules_history);
}

vector<string> string_split(string s, string delimiter) {
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;
	vector<string> res;

	while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(s.substr(pos_start));
	return res;
}

ampoule_test_history_t to_ampoule_test_history_t(string history,
		bool is_history = true) {
	ampoule_test_history_t test_hist = ampoule_test_history_t();

	if (history == "") {
		test_hist.id_test = -1;
		return test_hist;
	}

	vector<string> history_splited = string_split(history, ";");

	test_hist.id = stoi(history_splited[1]);
	test_hist.id_test = stoi(history_splited[0]);
	test_hist.ampola = test_hist.id;
	test_hist.ciclo = history_splited[2];
	test_hist.dt_inicio = history_splited[3];
	test_hist.hr_inicio = history_splited[4];
	test_hist.dt_fim = history_splited[5];
	test_hist.hr_fim = history_splited[6];
	test_hist.resultado = history_splited[7];
	test_hist.temperature = stoi(history_splited[8]);

	if (is_history) {
		char date_time[30];
		rtc_ds1302_get_date_time(date_time);

		test_hist.resultado = "C";

		string date = date_time;

		vector<string> v_date_time = string_split(date, " ");

		test_hist.dt_fim = v_date_time[0];
		test_hist.hr_fim = v_date_time[1];
	}

	return test_hist;
}

ampoule_test_history_t convert_temp_histories(int index) {

	string history_str = load_ampoules_history_temp(index);

	return to_ampoule_test_history_t(history_str);
}

void load_temp_histories() {
}

//void load_temp_histories() {
//	LinkedList<ampoule_test_history_t> history_list = load_ampoules_test();
//
//	LinkedList<ampoule_test_history_t> histories_temp =
//			convert_temp_histories();
//
//	if (histories_temp.size() > 0) {
//		printf("\nChegou aqui <<<<<<<>>>>>>> [%d]\n\n", history_list.size());
//
//		for (int j = 0; j < history_list.size(); j++) {
//			printf("\n Entrou aqui no for <<<<<<<>>>>>>>\n\n");
//			ampoule_test_history_t j_history = history_list[j];
//
//			printf("\n Pegou o index [%d] da lista <<<<<<<>>>>>>>\n\n", j);
//			int index = -1;
//
//			for (int i = 0; i < histories_temp.size(); i++) {
//
//				ampoule_test_history_t i_history = histories_temp[i];
//
//				if (i_history.id_test == j_history.id_test) {
//					index = i;
//					continue;
//				}
//			}
//
//			if (index != -1) {
//				history_list.remove(index);
//			}
//
//		}
//
//		if (histories_temp.size() > 0) {
//
//			char date_time[30];
//			rtc_ds1302_get_date_time(date_time);
//
//			for (int i = 0; i < histories_temp.size(); i++) {
//
//				if (histories_temp[i].id_test == -1)
//					continue;
//
//				ampoule_test_history_t h = histories_temp[i];
//
//				h.resultado = "C";
//
//				string date = date_time;
//
//				vector<string> date_time = string_split(date, " ");
//
//				h.dt_fim = date_time[0];
//				h.hr_fim = date_time[1];
//
//				string shist = ampoule_test_history_to_string(h);
//				set_history_string(shist, true, false);
//
//			}
//		}
//
//		load_histories();
//	}
//}

void clear_histories() {
	ampoules_history = LinkedList<string>();
}

string convert_ampoules_test_to_json() {
	cJSON *root;
	root = cJSON_CreateArray();

//cJSON *array;
	cJSON *element;

	for (int i = 0; i < 4; i++) {
		//array = cJSON_CreateArray();

		element = cJSON_CreateObject();

		cJSON_AddNumberToObject(element, "ampoule", ampoules[i].id);
		cJSON_AddNumberToObject(element, "incubation", ampoules[i].id_test);
		cJSON_AddNumberToObject(element, "type", ampoules[i].time_test);
		cJSON_AddStringToObject(element, "start_date",
				ampoules[i].date_start.c_str());
		cJSON_AddStringToObject(element, "start_hour",
				ampoules[i].hour_start.c_str());
		cJSON_AddStringToObject(element, "end_date",
				ampoules[i].date_end.c_str());
		cJSON_AddStringToObject(element, "end_hour",
				ampoules[i].hour_end.c_str());
		cJSON_AddNumberToObject(element, "positive_percentage",
				ampoules[i].get_positive_percentage());

		cJSON *ampoules_result;
		ampoules_result = cJSON_CreateArray();

		cJSON_AddItemToObject(element, "samples", ampoules_result);

		int size = ampoules[i].samples.size();

		for (int j = 0; j < size; j++) {
			cJSON *sample = NULL;
			sample = cJSON_CreateNumber(ampoules[i].samples[j]);
			cJSON_AddItemToArray(ampoules_result, sample);
		}

		//cJSON_AddItemToArray(array, element);
		//cJSON_AddItemToArray(root, array);
		cJSON_AddItemToArray(root, element);
	}

	char *ampoules_samples_json = cJSON_Print(root);
	ESP_LOGI("JSON", "\n%s\n\n", ampoules_samples_json);
	cJSON_Delete(root);

	return ampoules_samples_json;
}

string convert_ampoules_test_status_to_json() {
	cJSON *root;
	root = cJSON_CreateObject();

	cJSON_AddBoolToObject(root, "ampoules_status", ampoule_any());

	char *_json = cJSON_Print(root);
	ESP_LOGI("JSON", "\n%s\n\n", _json);
	cJSON_Delete(root);

	return _json;
}

string convert_history() {
	int size = ampoules_history.size();

	string shist = "";

	for (int i = 0; i < size; i++) {

		if (shist != "") {
			shist.append("|");
		}

		shist.append(ampoules_history[i]);
	}

//printf("\nTeste dados memoria: %s\n\n", shist.c_str());

	return shist;
}

esp_err_t set_history_string(string h, bool auto_save, bool auto_load) {

	ampoules_history.add(h);

	if (ampoules_history.size() > 12) {
		ampoules_history.shift();
	}

	if (auto_save) {

		string s = convert_history();

		esp_err_t err = save_ampoules_test(s);

		return err;
	}

	if (auto_load) {
		load_histories();
	}

	return ESP_OK;
}

esp_err_t set_history_temp_string(int index, string history) {

	return save_ampoules_history_temp(history, index);
}

static int get_ticket_temperature() {
	return (int) get_target_temperature();
}

ampoule_test_history_t to_ampoule_test_history(int index, bool is_cancelled =
		false, bool is_history_temp = false) {

	ampoule_test_history_t hist = ampoule_test_history_t();
	hist.ampola = to_string(ampoules[index].id);
	hist.ciclo = to_string(ampoules[index].time_test);
	hist.dt_fim = ampoules[index].date_end;
	hist.dt_inicio = ampoules[index].date_start;
	hist.hr_fim = ampoules[index].hour_end;
	hist.hr_inicio = ampoules[index].hour_start;
	hist.id = ampoules[index].id;
	hist.id_test = ampoules[index].id_test;

	if (!is_history_temp) {
		hist.positive_percentage = ampoules[index].get_positive_percentage();

		if (is_cancelled) {
			hist.resultado = "C";
		} else {
			hist.resultado = ampoules[index].is_positived ? "P" : "N";
		}
	} else {
		hist.resultado = "C";
	}

	hist.temperature = get_ticket_temperature();

	return hist;
}

string ampoule_test_history_to_string(ampoule_test_history_t hist) {
	string shist = "";
	shist.append(to_string(hist.id_test));
	shist.append(";");
	shist.append(to_string(hist.id));
	shist.append(";");
	shist.append(hist.ciclo);
	shist.append(";");
	shist.append(hist.dt_inicio);
	shist.append(";");
	shist.append(hist.hr_inicio);
	shist.append(";");
	shist.append(hist.dt_fim);
	shist.append(";");
	shist.append(hist.hr_fim);
	shist.append(";");
	shist.append(hist.resultado);
	shist.append(";");
	shist.append(to_string(hist.temperature));

	return shist;
}

esp_err_t set_history(int index, bool is_cancelled = false) {

	if (ampoules_history.size() <= 0)
		load_histories();

	ampoule_test_history_t hist = to_ampoule_test_history(index, is_cancelled);

	string shist = ampoule_test_history_to_string(hist);

	return set_history_string(shist, true, true);
}

esp_err_t set_temp_history(int index) {

	ampoule_test_history_t hist = to_ampoule_test_history(index, true, true);

	string shist = ampoule_test_history_to_string(hist);

	return set_history_temp_string(index, shist);
}

AmpouleTestResult get_test_result(int index) {
	AmpouleTestResult result = AmpouleTestResult();

	ampoules[index].get_test_result();

	result.set_id(ampoules[index].id);

	int32_t test_id = ampoules[index].id_test;
	std::string id_test_format = std::to_string(test_id);
	str_pad_to(id_test_format, 10, '0');

	result.set_id_test(id_test_format);

	result.set_date_time(ampoules[index].get_formated_date_time(true), true);
	result.set_date_time(ampoules[index].get_formated_date_time(false), false);

	result.set_cicle(ampoules[index].time_test);
	result.set_result(ampoules[index].is_positived, false);

	result.set_temperature(get_ticket_temperature());

	return result;
}

void set_is_priting(bool ispriting) {
	is_printing = ispriting;
}

void prepare_test(int ampoule) {
// Liga o led UV
	led_uv_on(ampoule);

	vTaskDelay(pdMS_TO_TICKS(TASK_TIME_DELAY));

// Desliga o aquecedor
	set_heater_controlling(false);
}

void finalize_test(int ampoule) {
// Desliga o led UV
	led_uv_off(ampoule);

// liga o aquecedor
	set_heater_controlling(true);
}

void set_date_time(int id, bool is_init) {
	char date_time[30];
	rtc_ds1302_get_date_time(date_time);

//	ESP_LOGI("", "Date & Time: %s", date_time);

	string dt = date_time;

	if (is_init) {
		ampoules[id].set_date_time(dt, true);
		ampoules[id].set_date_time(dt, false);
	} else {
		ampoules[id].set_date_time(dt, false);
	}

//	ESP_LOGI("", "Date & Time: %s",
//			ampoules[id].get_formated_date_time(true).c_str());
//	ESP_LOGI("", "Date & Time: %s",
//			ampoules[id].get_formated_date_time(false).c_str());
}

void finalize_ampoule_test(int index, int ampoule, bool early_result) {
	printf("***** [AMPOLA %d] FINALIZANDO TESTES *****\n", ampoule);

	turn_on_buzzer_button();

	ampoules[index].test_done = true;
	ampoules[index].is_testing = false;

	set_buzzer_on_off(true);

	if (!early_result) {
		char date_time[30];

		add_seconds(date_time, ampoules[index].get_formated_date_time(true),
				ampoules[index].time_test);

		string dt = date_time;

		ampoules[index].set_date_time(dt, false);

		printf("Date & Time End (Final): %s\n", dt.c_str());

	} else {
		set_date_time(index, false);
	}

	printf("***** [AMPOLA %d] TESTE FINALIZADO COM SUCESSO *****\n", ampoule);

	printf("\n***** [AMPOLA %d] IMPRIMINDO RESULTADO *****\n\n", ampoule);

	print_ampoule_test(index);

// Chamar depois de print_ampoule_test para pegar o resultado do test, antes o resultado ser� sempre negativo.
	set_history(index);
	reset_ampoules_history_temp(index);

// Liga o led verde ou vermelho se positivado ou nï¿½o
	ampoules_leds_positived(ampoules[index].id, ampoules[index].is_positived);

	ampoules[index].print_test_result();

	timeManager.stop_timer();
	timeManager.start_timer();
}

bool get_is_cavities_in_test() {
	return is_cavities_in_test;
}

void ampoule_test_check_cavity() {

	is_cavities_in_test = true;

//start_stop_led_effect_test(true);

	for (int index = 0; index < 4; index++) {

		int ampoule = ampoules[index].id;

		// Desliga os LEDS UV
		finalize_test(ampoule);

		long min = 0;
		long max = 0;

		for (int i = 0; i < 5; i++) {
			long l = read_channel_value(index);

			if (min == 0) {
				min = l;

				vTaskDelay(10);
				continue;
			}

			if (min < l) {
				min = l;

				vTaskDelay(10);
				continue;
			}

		}

		// Liga os LEDS UV e desliga o aquecimento
		prepare_test(ampoule);

		for (int i = 0; i < 5; i++) {
			long l = read_channel_value(index);

			if (max == 0) {
				max = l;

				vTaskDelay(10);
				continue;
			}

			if (l > max) {
				max = l;

				vTaskDelay(10);
				continue;
			}

		}

		finalize_test(ampoule);

		printf("\n[Cavidade %d] - UV desligado : %ld | UV ligado: %ld\n",
				(index + 1), min, max);

		if (min >= max) {
			printf("Cavidade %d desativada\n", (index + 1));
			ampoules[index].set_disabled_status(true);
			//ampoules_leds_disabled(index, true);
		} else {
			ampoules[index].set_disabled_status(false);
		}
	}

//start_stop_led_effect_test(false);
	set_led_function_active();

	is_cavities_in_test = false;
}

void ampoule_test_check_cavity_finalize() {
	set_led_function_on_off(ampoules[0].get_disabled_status(),
			ampoules[1].get_disabled_status(),
			ampoules[2].get_disabled_status(),
			ampoules[3].get_disabled_status());
}

void ampoule_test(int index) {
	int ampoule = ampoules[index].id;

//printf("**** NUMERO DA AMPOLA %d \n", ampoule);

	if (ampoules[index].is_present) {
		long time = ampoules[index].time_test;

		int test = ampoules[index].samples.size() + 1;

		if (test == 1) {
			//buzzer_continuous(pdMS_TO_TICKS(250));

			//turn_on_buzzer_button();

			set_date_time(index, true);

			ampoule_set_test_counter(index);

			ampoules[index].is_testing = true;
			ampoules[index].is_positived = false;
			ampoules[index].test_done = false;
			ampoules[index].current_time = 0;

			ampoules[index].count_alarm_is_ausent = 1;

			set_temp_history(index);
		}

		int32_t test_id = ampoules[index].id_test;
		std::string id_test_format = std::to_string(test_id);
		str_pad_to(id_test_format, 10, '0');

		printf("\n\n[AMPOLA %d] Numero Sequencial do teste : [ %s ]\n", ampoule,
				id_test_format.c_str());

		printf("[AMPOLA %d] Executando teste: %d\n", ampoule, test);

		printf("[AMPOLA %d] Tempo total de teste        : %ld (segundos)\n",
				ampoule, time);
		printf("[AMPOLA %d] Tempo em teste              : %ld (segundos)\n",
				ampoule, ampoules[index].current_time);
		printf("[AMPOLA %d] Tempo para concluir o teste : %ld (segundos)\n",
				ampoule, time - ampoules[index].current_time);

		if (ampoules[index].current_time >= 420 && !ampoules[index].test_done
				&& ampoules[index].is_testing) {
			bool is_positived = ampoules[index].calcule_test_result();

			if (is_positived) {
				set_date_time(index, false);
				finalize_ampoule_test(index, ampoule, true);
			}
		}

		if (ampoules[index].current_time >= time) {
			set_date_time(index, false);
			finalize_ampoule_test(index, ampoule, false);
		} else {

			bool is_long_test = ampoules[index].current_time > 1200;

			if (!is_long_test) {
				ampoules[index].current_time += ((TASK_TIME + TASK_TIME_DELAY)
						/ 1000);
			} else {
				ampoules[index].current_time += (((TASK_TIME + TASK_TIME_DELAY)
						* 2) / 1000);
			}

//			ampoules[index].current_time += ((TASK_TIME + TASK_TIME_DELAY)
//					/ 1000);

			prepare_test(ampoule);

			// Faz a leitura do sensor
			long sensor = read_channel_value(index);

			sensor = sensor / 100;

			printf("[AMPOLA %d] Valor recebido do sensor: %ld\n", ampoule,
					sensor);

			ampoules[index].add_sensor_value(sensor);

			finalize_test(ampoule);

			printf("[AMPOLA %d] Aguardando proxima leitura de teste\n",
					ampoule);

			set_date_time(index, false);
		}
	}
}

void ampoule_test_error_task(void *pvParameter) {
	while (true) {
		for (int i = 0; i < 4; i++) {
			int id = ampoules[i].id;
			int alarm_status = ampoules[i].get_ampoule_alarm();

			//printf("\nalarm_status: %d\n", alarm_status);

			if (alarm_status == 0) {
				continue;
			} else if (alarm_status == 1) {
				// 1 - soar o beep de ampoula inserida (apenas se temperatura no range >= 33)
				if (is_temperature_in_range()) {
					buzzer_continuous(pdMS_TO_TICKS(250));
				}

				ampoules[i].clear_ampoule_alarm();
			} else if (alarm_status == 2) {
				// 2 -ampola removida

				//buzzer_continuous(pdMS_TO_TICKS(250));

				for (int x = 0; x < 4; x++) {
					buzzer_on();
					vTaskDelay(pdMS_TO_TICKS(50));
					buzzer_off();
					vTaskDelay(pdMS_TO_TICKS(50));
				}

				ampoules[i].clear_ampoule_alarm();
			} else {
				// 3 - beep de ampoula removida durante um teste

				set_date_time(i, false);

				vTaskDelay(pdMS_TO_TICKS(1000));

				// Aqui imprimir o resutado da ampola... apenas se o test tiver sido iniciado.
				print_ampoule_test(i, true);

				vTaskDelay(pdMS_TO_TICKS(1000));

				ampoules[i].count_alarm_is_ausent = -1;

				// Aciona os leds de alarme
				ampoules_leds_removed_error(id);

				// Salva em mem�ria o resultado.
				set_history(i, true);
				reset_ampoules_history_temp(i);

				if (ampoules[i].samples.size() > 0) {

					// Limpa os valores do sensor
					ampoules[i].samples.clear();
				}

				//ampoule_clear_test_done(i);

				printf("[AMPOLA %d] Removida da cavidade de teste\n", id);
				// Liga o Alarme
				//set_alarm(true);

				for (int m = 0; m < 4; m++) {
					buzzer_on();
					vTaskDelay(pdMS_TO_TICKS(50));
					buzzer_off();
					vTaskDelay(pdMS_TO_TICKS(50));
				}

				buzzer_on();
				vTaskDelay(pdMS_TO_TICKS(500));
				buzzer_off();
				vTaskDelay(pdMS_TO_TICKS(500));

				ampoules[i].clear_ampoule_alarm();
			}
		}

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void ampoules_test_timer_task(void *pvParameter) {
	while (true) {

		int64_t t1 = esp_timer_get_time();
		//ESP_LOGI("TEST", "Entering light sleep for 0.5s, time since boot: %lld us",
		//		t1);

		if (!check_if_heater_temperature_stabilized()) {

			vTaskDelay(pdMS_TO_TICKS(TASK_TIME));
			continue;
		}

		is_ampoule_in_test = true;

		for (int i = 0; i < 4; i++) {
			if (!ampoules[i].test_done) {
				ampoule_test(i);
			}
		}

		is_ampoule_in_test = false;

		int64_t t2 = esp_timer_get_time();
		//ESP_LOGI("TEST", "Woke up from light sleep, time since boot: %lld us", t2);

		int64_t tt = llabs((t2 - t1) / 1000);

		if (tt < 0) {
			tt = llabs((t1 - t2) / 1000);
		}

		ESP_LOGI("TEST", "Tempo total de execucao: %lld us", tt);

		bool is_long_test = false;

		for (int i = 0; i < 4; i++) {
			if (ampoules[i].current_time > 1200) {
				is_long_test = true;
			}
		}

		if (!is_long_test) {
			int64_t delay = (TASK_TIME + TASK_TIME_DELAY) - round(tt);

			ESP_LOGI("TEST", "Delay: %lld ms", delay);

			vTaskDelay(pdMS_TO_TICKS(delay));
		} else {
			int64_t delay = (TASK_TIME + TASK_TIME_DELAY);

			delay = (delay * 2) - round(tt);

			ESP_LOGI("TEST", "Long Time Delay: %lld ms", delay);

			vTaskDelay(pdMS_TO_TICKS(delay));
		}
	}

	vTaskDelete( NULL);
}

void ampoules_test_check_done_task(void *pvParameter) {
	while (true) {
		if (!ampoule_any()) {
			set_alarm(false);
			set_buzzer_alert_on_off(false);
			vTaskDelay(pdMS_TO_TICKS(1000));
			continue;
		}

		if (!is_ampoule_in_test) {

			// Cancela todos os testes quando temperatura sair do range 33-43 durante um teste
			if (temp_out_of_range_cancel) {
				temp_out_of_range_cancel = false;
				for (int i = 0; i < 4; i++) {
					if (ampoules[i].is_testing) {
						printf("\n***** [AMPOLA %d] CANCELANDO TESTE POR TEMPERATURA FORA DO RANGE *****\n\n", ampoules[i].id);
						set_date_time(i, false);
						vTaskDelay(pdMS_TO_TICKS(500));
						print_ampoule_test(i, true);
						vTaskDelay(pdMS_TO_TICKS(1000));
						set_history(i, true);
						reset_ampoules_history_temp(i);
						ampoules[i].is_testing = false;
						ampoules[i].test_done = true;
						ampoules[i].cancelled_by_temp = true;
						ampoules[i].samples.clear();
					}
				}
				// Não reseta heater_reached_target: novos testes podem iniciar assim que
				// temperatura voltar ao range (>= 33°C), sem exigir re-aquecimento até 37°C.
			}

			bool alarm_is_on = false;

			for (int i = 0; i < 4; i++) {
				if (ampoules[i].test_done && !is_printing) {
					//vTaskDelay(pdMS_TO_TICKS(500));

					//set_ampoule_led_test_done(true);

//					// Liga o led verde ou vermelho se positivado ou nï¿½o
//					ampoules_leds_positived(ampoules[i].id,
//							ampoules[i].is_positived);
//
//					ampoules[i].print_test_result();

					if (!alarm_is_on) {
						set_buzzer_alert_on_off(true);
//						timeManager.stop_timer();
//						timeManager.start_timer();
					}

//					vTaskDelay(pdMS_TO_TICKS(100));
//
//					if (!ampoules[i].printed) {
//						printf(
//								"\n***** [AMPOLA %d] IMPRIMINDO RESULTADO *****\n\n",
//								ampoules[i].id);
//
//						print_ampoule_test(i);
//
//						ampoules[i].printed = true;
//					}
				} else {
					set_ampoule_led_test_done(ampoules[i].test_done);
				}
			}
		}

		vTaskDelay(pdMS_TO_TICKS(2000));
	}

	vTaskDelete( NULL);
}

esp_err_t ampoules_test_timer_start_task() {
	if (ampoules_test_task_handle == NULL) {

		xTaskCreate(ampoules_test_timer_task, "TASK_TST_AMP_1", 1024 * 5,
		NULL,
		configMAX_PRIORITIES - 1, &ampoules_test_task_handle);
	}

	if (ampoules_test_error_task_handle == NULL) {
		xTaskCreate(ampoule_test_error_task, "TASK_TST_ERROR", 1024 * 5,
		NULL, configMAX_PRIORITIES - 6, &ampoules_test_error_task_handle);
	}

	return ESP_OK;
}

esp_err_t ampoules_test_timer_stop_task() {
	if (ampoules_test_task_handle != NULL) {
		vTaskResume(ampoules_test_task_handle);
	}

	return ESP_OK;
}

void init_ampoules() {
	for (int i = 1; i < 5; i++) {
		AmpouleSensor s = AmpouleSensor(i, false);

		// TODO: Ajustar para 20 * 60 quando finalizar os testes.
		s.time_test = DEFAULT_TIME_TEST;
		s.is_present = false;
		s.average = 0;
		s.is_positived = false;
		s.test_done = false;

		ampoules.add(s);
	}
}

void ampoule_clear_test_done(int index) {
// reinicia os testes se forem finalizados.
	ampoules[index].test_done = false;
	ampoules[index].cancelled_by_temp = false;
	ampoules[index].is_testing = false;
	ampoules[index].samples.clear();
	ampoules[index].id_test = 0;
//	ampoules[index].is_init_alarm = true;

	reset_ampoules_history_temp(index);
	set_buzzer_alert_on_off(false);
}

void ampoule_set_test_counter(int index) {

	int32_t ampoute_test_cont = 0;
	esp_err_t err = get_new_ampoule_test_count(ampoute_test_cont);
//get_ampoule_test_counter(ampoute_test_cont);

	if (err != ESP_OK) {

	}

	ampoules[index].id_test = ampoute_test_cont;
}

void ampoule_set_status(bool ampoule1, bool ampoule2, bool ampoule3,
		bool ampoule4) {

	bool amp1_is_disabbled = ampoules[0].get_disabled_status();
	bool amp2_is_disabbled = ampoules[1].get_disabled_status();
	bool amp3_is_disabbled = ampoules[2].get_disabled_status();
	bool amp4_is_disabbled = ampoules[3].get_disabled_status();

	ampoules[0].set_locked_status(ampoule1);
	ampoules[1].set_locked_status(ampoule2);
	ampoules[2].set_locked_status(ampoule3);
	ampoules[3].set_locked_status(ampoule4);

	if (!amp1_is_disabbled) {
		ampoules[0].is_present = ampoule1;
		ampoules[0].set_apoule_alarm(ampoule1);
	}

	if (!amp2_is_disabbled) {
		ampoules[1].is_present = ampoule2;
		ampoules[1].set_apoule_alarm(ampoule2);
	}

	if (!amp3_is_disabbled) {
		ampoules[2].is_present = ampoule3;
		ampoules[2].set_apoule_alarm(ampoule3);
	}

	if (!amp4_is_disabbled) {
		ampoules[3].is_present = ampoule4;
		ampoules[3].set_apoule_alarm(ampoule4);
	}

	if (!ampoule1 && ampoules[0].test_done && !amp1_is_disabbled) {
		ampoule_clear_test_done(0);
	}

	if (!ampoule2 && ampoules[1].test_done && !amp2_is_disabbled) {
		ampoule_clear_test_done(1);
	}

	if (!ampoule3 && ampoules[2].test_done && !amp3_is_disabbled) {
		ampoule_clear_test_done(2);
	}

	if (!ampoule4 && ampoules[3].test_done && !amp4_is_disabbled) {
		ampoule_clear_test_done(3);
	}
}

void ampoule_set_status(int id, bool present) {
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			ampoules[i].is_present = present;

			if (!present && ampoules[i].test_done) {
				// reinicia os testes se forem finalizados.
				ampoules[i].test_done = false;
				ampoules[i].cancelled_by_temp = false;
				ampoules[i].is_testing = false;
				ampoules[i].samples.clear();
				ampoules[i].printed = false;
			}

			break;
		}
	}
}

long ampoule_get_time_test(int id) {
	long time = -1;

	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			time = ampoules[i].time_test;
			break;
		}
	}

	return time;
}
/*
void ampoule_set_time_test(int id, int level) {
	long time = 0;

	printf("LEVEL %d\n", level);

	if (level == 1) {
		time = 60 * 60;
	} else if (level == 2) {
		time = 2 * 60 * 60;
	} else if (level == 3) {
		time = 3 * 60 * 60;
	} else {
		time = 20 * 60;
	}

	printf("TIME  %ld\n", time);

	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			ampoules[i].time_test = time;
			break;
		}
	}
}
*/
void ampoule_set_time_test(int id, int level) {
	long time = 20 * 60;

	printf("LEVEL IGNORADO %d\n", level);
	printf("TIME FIXO %ld\n", time);

	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			ampoules[i].time_test = time;
			break;
		}
	}
}

bool ampoule_any() {
	bool any = false;

	for (int i = 0; i < 4; i++) {
		if (ampoules[i].is_present) {
			any = true;
			break;
		}
	}

	return any;
}

void ampoule_initial_beep(int id) {
	for (int i = 0; i < 4; i++) {

		//printf("******* %d\n", ampoules[i].is_init_alarm);

		if (ampoules[i].id == id) {
//			ampoules[i].is_init_alarm = false;

			buzzer_continuous(pdMS_TO_TICKS(250));
			vTaskDelay(pdMS_TO_TICKS(250));
			break;
		}
	}
}

bool ampoule_get_status(int id) {
	bool is_present = false;
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			is_present = ampoules[i].is_present;
			break;
		}
	}

	return is_present;
}

bool ampoule_is_disabled(int id) {
	bool is_disabled = false;
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			is_disabled = ampoules[i].get_disabled_status();
			break;
		}
	}

	return is_disabled;
}

bool ampoule_is_locked(int id) {
	bool is_locked = false;
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			is_locked = ampoules[i].get_locked_status();
			break;
		}
	}

	return is_locked;
}

bool is_test_done(int id) {
	bool is_done = false;
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			is_done = ampoules[i].test_done;
			break;
		}
	}

	return is_done;
}

bool is_any_in_test_done() {
	bool is_done = false;
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].test_done) {
			is_done = true;
			break;
		}
	}

	return is_done;
}

bool is_any_present_cancelled_by_temp() {
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].is_present && ampoules[i].cancelled_by_temp) {
			return true;
		}
	}
	return false;
}

bool is_testing(int id) {
	bool is_done = false;
	for (int i = 0; i < 4; i++) {
		if (ampoules[i].id == id) {
			is_done = ampoules[i].is_testing;
			break;
		}
	}

	return is_done;
}

bool is_any_testing() {
	bool is_done = false;

	for (int i = 0; i < 4; i++) {
		if (ampoules[i].is_testing) {
			is_done = true;
			break;
		}
	}

	return is_done;
}

void ampoule_test_setup() {
	init_ampoules();

//load_temp_histories();

	xTaskCreate(ampoules_test_check_done_task, "TASK_CHECK_TST_DONE", 1024 * 8,
	NULL, configMAX_PRIORITIES - 5, NULL);

//	xTaskCreate(ampoules_test_temp_nvs_task, "TASK_TEMP_NVS", 1024 * 5,
//		&ampoules_test_task_handle, configMAX_PRIORITIES - 8, NULL);

	//load_temp_histories();

}

void ampoule_test_start() {
	if (ampoule_any()) {
		ampoules_test_timer_start_task();
	} else {
		ampoules_test_timer_stop_task();
	}
}

