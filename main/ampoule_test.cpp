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
#include <ctime>

#include "include/ampoule_test_history.h"
#include "include/helper_utils.h"
#include "include/TimerManager.h"
#include "advanced_config.h"
#include "ampoule_history.h"

// Deve ficar igual ao valor do nivel 0 (default/sem nenhum aperto do botao
// de tempo) em ampoule_set_time_test() - hoje 5 min, ver comentario la.
#define DEFAULT_TIME_TEST (5 * 60)

TimerManager timeManager;
int alarmTimeout = 30 * 60;
//int alarmTimeout = 5 * 60;

TaskHandle_t ampoules_test_task_handle = NULL;

TaskHandle_t ampoules_test_error_task_handle = NULL;

LinkedList<AmpouleSensor> ampoules = LinkedList<AmpouleSensor>();

string ampoule_test_history_to_string(ampoule_test_history_t hist);

void ampoule_clear_test_done(int index);
long ampoule_get_time_test(int id);
esp_err_t ampoules_test_timer_start_task();
esp_err_t ampoules_test_timer_stop_task();

void ampoule_set_test_counter(int index);

bool is_ampoule_in_test = false;
bool is_cavities_in_test = false;

bool is_printing = false;

// True enquanto um teste esta sendo finalizado/cancelado (imprimindo +
// gravando o registro no historico). O watchdog de impressora
// (attempt_safe_printer_recovery, printer.cpp) precisa respeitar essa flag
// alem de ampoule_any() - motivo: no caso "ampola removida durante o
// teste", a propria remocao que dispara o cancelamento tambem faz
// ampoule_any() virar false na hora, e a tentativa de imprimir o ticket
// desse cancelamento pode falhar (impressora desconectada) e setar
// is_printer_error() ANTES do set_history() rodar - sem essa flag o
// watchdog reiniciava o equipamento no meio do caminho e o registro
// cancelado nunca chegava a ser persistido (bug reportado pelo cliente).
volatile bool ampoule_finalize_in_progress = false;

bool is_ampoule_finalize_in_progress() {
	return ampoule_finalize_in_progress;
}

volatile bool temp_out_of_range_cancel = false;

void trigger_temp_out_of_range_cancel() {
	temp_out_of_range_cancel = true;
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
	ampoule_history_clear();
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

// Converte "dd/mm/yyyy" + "hh:mm:ss" (formato usado em todo o projeto,
// vindo do RTC) para timestamp Unix. Retorna 0 se o parse falhar.
static uint32_t parse_date_time_to_ts(const string &date, const string &hour) {
	struct tm tm_val = { };
	int day, month, year, h, min, sec;

	if (sscanf(date.c_str(), "%d/%d/%d", &day, &month, &year) != 3)
		return 0;

	if (sscanf(hour.c_str(), "%d:%d:%d", &h, &min, &sec) != 3)
		return 0;

	tm_val.tm_mday = day;
	tm_val.tm_mon = month - 1;
	tm_val.tm_year = year - 1900;
	tm_val.tm_hour = h;
	tm_val.tm_min = min;
	tm_val.tm_sec = sec;
	tm_val.tm_isdst = -1;

	time_t t = mktime(&tm_val);

	if (t < 0)
		return 0;

	return (uint32_t) t;
}

esp_err_t set_history(int index, bool is_cancelled = false,
		bool printed_ok = false) {

	ampoule_test_history_t hist = to_ampoule_test_history(index, is_cancelled);

	ampoule_history_record_t record = { };
	record.id_test = (uint32_t) hist.id_test;
	record.cavidade = (uint8_t) hist.id;
	// hist.ciclo vem em segundos (ampoules[index].time_test); ciclo_minutos
	// e consumido como MINUTOS (tela de Historico, reimpressao via
	// printer.cpp) - sem o /60 aqui, um teste de 20min (1200s) era gravado
	// como "1200 min".
	record.ciclo_minutos = (uint16_t) (atoi(hist.ciclo.c_str()) / 60);
	record.ts_inicio = parse_date_time_to_ts(hist.dt_inicio, hist.hr_inicio);
	record.ts_fim = parse_date_time_to_ts(hist.dt_fim, hist.hr_fim);
	record.temperatura = (uint8_t) hist.temperature;

	if (hist.resultado == "P")
		record.resultado = AMPOULE_RESULT_POSITIVE;
	else if (hist.resultado == "C")
		record.resultado = AMPOULE_RESULT_CANCELLED;
	else
		record.resultado = AMPOULE_RESULT_NEGATIVE;

	record.printed = printed_ok;

	return ampoule_history_add(record);
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

	// Sem zeros a esquerda no numero da incubacao (pedido do cliente 21/08).
	int32_t test_id = ampoules[index].id_test;
	std::string id_test_format = std::to_string(test_id);

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

// Tempo antes da leitura do ADC em que o aquecedor fica desligado, pra
// evitar ruido eletrico na conversao - antes ficava desligado durante
// TODA a espera de captura do LED (led_capture_time_s inteiro), sobrando
// pouquissimo tempo de aquecedor-ligado por cavidade e derrubando a
// temperatura em testes com as 4 cavidades ativas (RESTART_ID=5 falso em
// campo, 18/08). So o fim da janela de captura precisa do aquecedor
// desligado; o resto da espera ele pode continuar sob controle normal.
// 100ms -> 300ms em 27/08: depois do fix que faz o aquecedor religar na
// hora (heater.cpp, set_heater_controlling), o chaveamento ficou muito
// mais frequente que antes (ele nao fica mais minutos desligado) - o
// ruido relativo medido no ADC subiu de ~0,02-0,16% (log de 14/08, antes
// do fix) pra ~0,3-0,9% (mesma metodologia, amostra do mesmo tamanho).
// 300ms da mais margem de assentamento antes da conversao (ADS1248 a
// 20SPS ja leva ~50ms por conversao sozinho).
#define HEATER_OFF_BEFORE_READ_MS 300

void prepare_test(int ampoule) {
// Liga o led UV
	led_uv_on(ampoule);

	uint32_t capture_ms = (uint32_t) (g_advanced_config.led_capture_time_s
			* 1000);
	uint32_t heater_off_ms =
			(capture_ms > HEATER_OFF_BEFORE_READ_MS) ?
					HEATER_OFF_BEFORE_READ_MS : capture_ms;

	vTaskDelay(pdMS_TO_TICKS(capture_ms - heater_off_ms));

// Desliga o aquecedor so na reta final da captura, perto da leitura
	set_heater_controlling(false);

	vTaskDelay(pdMS_TO_TICKS(heater_off_ms));
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

	ampoule_finalize_in_progress = true;

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

	bool printed_ok = print_ampoule_test(index);

// Chamar depois de print_ampoule_test para pegar o resultado do test, antes o resultado ser� sempre negativo.
	set_history(index, false, printed_ok);
	reset_ampoules_history_temp(index);

	ampoule_finalize_in_progress = false;

// Liga o led verde ou vermelho se positivado ou nï¿½o
	ampoules_leds_positived(ampoules[index].id, ampoules[index].is_positived);

	ampoules[index].print_test_result();

	timeManager.stop_timer();
	timeManager.start_timer();
}

// Numero de leituras seguidas "assumidas" (timeout do DRDY, valor congelado
// no ultimo dado bom) que uma cavidade tolera antes de abortar o teste por
// falha de sensor. Isolado por cavidade (ver get_channel_consecutive_timeouts
// em light_sensor.cpp) - uma falha isolada nao aborta nada, so falha
// persistente.
#define MAX_CONSECUTIVE_SENSOR_TIMEOUTS 3

void abort_ampoule_test_sensor_fault(int index, int ampoule) {
	printf(
			"***** [AMPOLA %d] TESTE ABORTADO - falha persistente de leitura do sensor *****\n",
			ampoule);

	ampoule_finalize_in_progress = true;

	// Desliga o LED UV e volta a controlar o aquecedor - prepare_test() ja
	// tinha ligado o LED/desligado o aquecedor antes da leitura que falhou,
	// e esse abort pula o finalize_test() normal do fluxo.
	finalize_test(ampoule);

	bool printed_ok = print_ampoule_test(index, true);

	set_history(index, true, printed_ok);
	reset_ampoules_history_temp(index);

	ampoules[index].test_done = true;
	ampoules[index].is_testing = false;

	if (ampoules[index].samples.size() > 0) {
		ampoules[index].samples.clear();
	}

	reset_channel_consecutive_timeouts(index);

	for (int m = 0; m < 4; m++) {
		buzzer_on();
		vTaskDelay(pdMS_TO_TICKS(50));
		buzzer_off();
		vTaskDelay(pdMS_TO_TICKS(50));
	}

	ampoule_finalize_in_progress = false;
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

	if (!ampoules[index].is_present && ampoules[index].is_testing
			&& !ampoules[index].test_done) {
		ESP_LOGW("", "[AMPOLA %d] is_present=false durante teste em "
				"andamento - iteracao pulada (leitura instavel do sensor?)",
				ampoule);
	}

	if (ampoules[index].is_present) {
		long time = ampoules[index].time_test;

		int test = ampoules[index].samples.size() + 1;

		if (test == 1) {
			//buzzer_continuous(pdMS_TO_TICKS(250));

			//turn_on_buzzer_button();

			// ETO e fixo em 20 min (botao de tempo fica desabilitado nesse
			// modo, ver keyboard.cpp) - forcado aqui, no inicio do teste,
			// porque time_test pode ter ficado com um valor herdado de
			// Normal/CRC1 (5 min por padrao) se o modo foi trocado pela
			// tela web sem reboot.
			if (g_advanced_config.operation_mode == OPERATION_MODE_ETO) {
				ampoules[index].time_test = 20 * 60;
				time = 20 * 60;
			}

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

		if (ampoules[index].current_time
				>= (long) g_advanced_config.early_check_time_s
				&& !ampoules[index].test_done && ampoules[index].is_testing) {
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
				ampoules[index].current_time +=
						g_advanced_config.loop_cycle_time_s;
			} else {
				ampoules[index].current_time +=
						(g_advanced_config.loop_cycle_time_s * 2);
			}

			prepare_test(ampoule);

			// Faz a leitura do sensor
			long sensor = read_channel_value(index);

			// Religa o aquecedor assim que a leitura termina, antes de
			// desligar o LED UV (finalize_test la embaixo religa de novo -
			// idempotente, so garante o estado em qualquer caminho de
			// saida, como o abort de timeout abaixo). Minimiza o tempo
			// real de aquecedor desligado por cavidade.
			set_heater_controlling(true);

			if (get_channel_consecutive_timeouts(index)
					>= MAX_CONSECUTIVE_SENSOR_TIMEOUTS) {
				abort_ampoule_test_sensor_fault(index, ampoule);
				return;
			}

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

				// Seta ANTES do print: e a propria remocao que zera
				// ampoule_any(), entao o watchdog de impressora
				// (printer.cpp) precisa saber que ha um cancelamento em
				// andamento mesmo que o print abaixo falhe e dispare
				// is_printer_error() (ver ampoule_finalize_in_progress).
				ampoule_finalize_in_progress = true;

				set_date_time(i, false);

				vTaskDelay(pdMS_TO_TICKS(1000));

				// Aqui imprimir o resutado da ampola... apenas se o test tiver sido iniciado.
				bool printed_ok = print_ampoule_test(i, true);

				vTaskDelay(pdMS_TO_TICKS(1000));

				ampoules[i].count_alarm_is_ausent = -1;

				// Aciona os leds de alarme
				ampoules_leds_removed_error(id);

				// Salva em mem�ria o resultado.
				set_history(i, true, printed_ok);
				reset_ampoules_history_temp(i);

				if (ampoules[i].samples.size() > 0) {

					// Limpa os valores do sensor
					ampoules[i].samples.clear();
				}

				//ampoule_clear_test_done(i);

				printf("[AMPOLA %d] Removida da cavidade de teste\n", id);
				// Liga o Alarme
				//set_alarm(true);

				// So libera o watchdog da impressora depois do beep de
				// alarme terminar de tocar - liberar antes (como a versao
				// anterior deste fix fazia) deixava o reinicio cortar o
				// beep no meio quando a impressora estava em erro.
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

				ampoule_finalize_in_progress = false;

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

			vTaskDelay(
					pdMS_TO_TICKS(g_advanced_config.loop_cycle_time_s * 1000));
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

		int64_t cycle_ms = (int64_t) g_advanced_config.loop_cycle_time_s * 1000;

		// Piso de folga entre uma rodada de leitura das 4 ampolas e a
		// proxima - e o unico intervalo em que o aquecedor fica realmente
		// livre pra ligar (prepare_test/finalize_test o desligam durante
		// cada leitura individual). Antes o piso era 100ms, so o bastante
		// pra nao corromper o vTaskDelay - na pratica, com 4 cavidades a
		// leitura ja leva mais tempo que o loop_cycle_time_s configurado
		// (4,5s medidos em campo contra um ciclo de 4s), entao o delay
		// calculado ficava negativo e caia direto nesse piso: o aquecedor
		// ficava ligado so por poucos milissegundos entre cavidades,
		// derrubando a temperatura em testes longos e causando reinicio
		// falso por temperatura baixa (RESTART_ID=5, log de campo 18/08).
		// 2s garante folga real pro aquecedor recuperar, **independente**
		// do loop_cycle_time_s configurado - corrige tambem unidades que
		// ja tem um valor antigo/insuficiente gravado na memoria.
		const int64_t HEATER_RECOVERY_MIN_MS = 2000;

		if (!is_long_test) {
			int64_t delay = cycle_ms - round(tt);

			if (delay < HEATER_RECOVERY_MIN_MS)
				delay = HEATER_RECOVERY_MIN_MS;

			ESP_LOGI("TEST", "Delay: %lld ms", delay);

			vTaskDelay(pdMS_TO_TICKS(delay));
		} else {
			int64_t delay = (cycle_ms * 2) - round(tt);

			if (delay < HEATER_RECOVERY_MIN_MS)
				delay = HEATER_RECOVERY_MIN_MS;

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
				ampoule_finalize_in_progress = true;
				for (int i = 0; i < 4; i++) {
					if (ampoules[i].is_testing) {
						printf("\n***** [AMPOLA %d] CANCELANDO TESTE POR TEMPERATURA FORA DO RANGE *****\n\n", ampoules[i].id);
						set_date_time(i, false);
						vTaskDelay(pdMS_TO_TICKS(500));
						bool printed_ok = print_ampoule_test(i, true);
						vTaskDelay(pdMS_TO_TICKS(1000));
						set_history(i, true, printed_ok);
						reset_ampoules_history_temp(i);
						ampoules[i].is_testing = false;
						ampoules[i].test_done = true;
						ampoules[i].cancelled_by_temp = true;
						ampoules[i].samples.clear();
					}
				}
				ampoule_finalize_in_progress = false;
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

// Aplica o estado de cavidades habilitadas/desabilitadas configurado via
// web (item 4). Cavidades desabilitadas ficam com is_present sempre
// congelado (ver ampoule_set_status(bool,bool,bool,bool) abaixo), entao o
// loop de testes ja as ignora naturalmente.
void ampoule_apply_cavity_enabled_config() {
	for (int i = 0; i < 4; i++) {
		ampoules[i].set_disabled_status(!g_advanced_config.cavity_enabled[i]);
	}
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
void ampoule_set_time_test(int id, int level) {
	long time = 0;

	printf("LEVEL %d\n", level);

	// Sequencia de tempos nos botoes fisicos: 5min -> 20min -> 1h -> 3h
	// (level vem de keyboard.cpp como d->level-1, entao 0 e o default/nivel
	// inicial). Antes era 20min -> 1h -> 2h -> 3h.
	if (level == 1) {
		time = 20 * 60;
	} else if (level == 2) {
		time = 60 * 60;
	} else if (level == 3) {
		time = 3 * 60 * 60;
	} else {
		time = 5 * 60;
	}

	printf("TIME  %ld\n", time);

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

