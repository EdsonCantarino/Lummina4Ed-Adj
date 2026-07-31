#include <stdio.h>  /* printf, scanf, NULL */
#include <stdlib.h> /* malloc, free, rand */
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>

using namespace std;

//#include <version_config.h>

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "include/nvs_utils.h"
#include "include/device_utils.h"
#include "include/rtc_ds1302.h"
#include "include/ampoule_test_history.h"
#include "ampoule_history.h"
#include "include/serial_number.h"
#include "include/ampoule_test.h"
#include "nvs_helpers.h"

static const char *TAG = "NVS_UTILS";

char *no_data = (char*) '\0';

int32_t restart_counter = 0;
int32_t ampoule_test_counter = 0;

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

//#define STORAGE_NAMESPACE "settings"
//#define ADMIN_NAMESPACE "admin"
//#define DEVICE_NAMESPACE "device"
//#define TEMP_DEVICE_NAMESPACE "temp_device"
//#define AMPOULE_NAMESPACE "ampoules"
//#define AMPOULE_HISTORY_NAMESPACE "data"
//#define POSITIVEPERCENTAGE_NAMESPACE "positive_perc"
//#define CALIBRATION_NAMESPACE "calibration"
//#define SERIAL_NUMBER_NAMESPACE "serial_number"

float positive_percentage = -1.0f;
string serial_number = "";
float calibration_factor = -1.0f;

vector<string> split(string s, string delimiter) {
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

NVSHelper nvs_storage;
NVSHelper nvs_ampoules;
NVSHelper nvs_ampoules_history;
NVSHelper nvs_ampoules_history_temp;

esp_err_t nvs_init() {

	ESP_LOGI(TAG, "Initializing NVS!");

	bool ok = nvs_storage.begin("storage");

	if (ok)
		ESP_LOGI(TAG, "NVS initialized: settings!\n");

	ok = nvs_ampoules_history.begin("data");

	if (ok)
		ESP_LOGI(TAG, "NVS initialized: data!\n");

	ok = nvs_ampoules_history_temp.begin("temp");

	if (ok)
		ESP_LOGI(TAG, "NVS initialized: data!\n");

	ok = nvs_ampoules.begin("ampoules");

	if (ok)
		ESP_LOGI(TAG, "NVS initialized: ampoules!\n");

	return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t save_restart_counter(void) {
	ESP_LOGI(TAG, "Save restart counter\n");

	restart_counter = nvs_storage.getInt("restart_conter");

	restart_counter++;

	ESP_LOGI(TAG, "Contador de reinicializacao: %ld", restart_counter);

	bool ok = nvs_storage.setInt("restart_conter", restart_counter, true);

	return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t get_calibration_factor(float &factor) {
	//ESP_LOGI(TAG, "Save Ampoule Test Counter\n");

	if (calibration_factor == -1.0f) {

		factor = nvs_storage.getFloat("calib_factor", 0.0f);

		//ESP_LOGI(TAG, "Temperature Factor: (%.1f)!\n", factor);
	} else {
		factor = calibration_factor;
	}

	return ESP_OK;

}

esp_err_t save_calibration(float factor) {
	ESP_LOGI(TAG, "Prepare to save Calibration Factor in NVS\n");

	bool ok = nvs_storage.setFloat("calib_factor", factor, true);

	if (!ok) {
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Calibration Factor: %f saved in NVS!\n", factor);

	calibration_factor = factor;

	return ESP_OK;
}

esp_err_t save_serial_number(string sn) {
	ESP_LOGI(TAG, "Prepare to save Serial Number in NVS\n");

	bool ok = nvs_storage.setString("serial_number", sn.c_str(), true);

	if (!ok) {
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Serial Number: %s saved in NVS!\n", sn.c_str());

	serial_number = sn;

	return ESP_OK;
}

esp_err_t save_print_count(uint8_t count) {
	ESP_LOGI(TAG, "Prepare to save Print Count in NVS\n");

	bool ok = nvs_storage.setInt("print_count", count, true);

	if (!ok) {
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Print Count: %d saved in NVS!\n", count);

	return ESP_OK;
}

uint8_t get_print_count() {
	int64_t count = nvs_storage.getInt("print_count", -1);

	if (count < 1 || count > AMPOULE_HISTORY_MAX_RECORDS) {
		return 12; // default - mesmo comportamento historico do limite antigo
	}

	return (uint8_t) count;
}

esp_err_t save_positive_percentage(float pp) {
	ESP_LOGI(TAG, "Prepare to save Restrict Device Setting in NVS\n");

	bool ok = nvs_storage.setFloat("positive_perc", pp, true);

	if (!ok) {
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Positive Percentage: %f saved in NVS!\n", pp);

	positive_percentage = pp;

	return ESP_OK;
}

bool save_institution(string inst) {
	bool ok = nvs_storage.setString("institution", inst.c_str(), true);

	if (!ok) {

		ESP_LOGE(TAG, "Error write device settings -> institution in NVS!");
	} else {
		ESP_LOGI(TAG, "Device Settings -> Institution: %s saved in NVS!\n",
				inst.c_str());
	}

	return ok;
}

string get_institution() {
	return nvs_storage.getString("institution");
}

esp_err_t save_device_settings(device_settings_t settings) {
	ESP_LOGI(TAG, "Prepare to save Device Setting in NVS\n");

	bool ok = save_institution(settings.institution);

	return ok ? ESP_OK : ESP_FAIL;
}

LinkedList<ampoule_test_history_t> split_ampoules_test(string histories) {
	LinkedList<ampoule_test_history_t> history = LinkedList<
			ampoule_test_history_t>();

	vector<string> xhistories = split(histories, "|");

	for (const auto &hist : xhistories) {
		vector<string> xhist = split(hist, ";");

		ampoule_test_history_t test_hist = ampoule_test_history_t();

		test_hist.id_test = stoi(xhist[0]);
		test_hist.id = stoi(xhist[1]);
		test_hist.ampola = test_hist.id;
		test_hist.ciclo = xhist[2];
		test_hist.dt_inicio = xhist[3];
		test_hist.hr_inicio = xhist[4];
		test_hist.dt_fim = xhist[5];
		test_hist.hr_fim = xhist[6];
		test_hist.resultado = xhist[7];
		test_hist.temperature = stoi(xhist[8]);

		history.add(test_hist);
	}

	return history;
}

void split_ampoules_test_history(string histories,
		LinkedList<string> &history) {
	vector<string> xhistories = split(histories, "|");

	for (int i = 0; i < xhistories.size(); i++) {
		history.add(xhistories[i]);
	}
}

LinkedList<ampoule_test_history_t> load_ampoules_test() {
	ESP_LOGI(TAG, "Prepare to load Ampoules Test History on NVS\n");

	string histories = nvs_ampoules_history.getString("history");

	LinkedList<ampoule_test_history_t> history = LinkedList<
			ampoule_test_history_t>();

	if (histories.length() > 0) {
		history = split_ampoules_test(histories);

		ESP_LOGI(TAG, "AQUI: Ampoules test history size: %d!\n",
				history.size());
	}

	return history;
}

esp_err_t load_ampoules_test_history(LinkedList<string> &history) {
	ESP_LOGI(TAG, "Prepare to load Ampoules Test History on NVS\n");

	string histories = nvs_ampoules_history.getString("history");

	if (histories.length() > 0) {
		split_ampoules_test_history(histories, history);
		ESP_LOGI(TAG, "Ampoules test history size: %d!\n", history.size());
	}

	return ESP_OK;
}

string load_ampoules_history_temp(int index) {
	ESP_LOGI(TAG, "Prepare to load Ampoules Temp History on NVS\n");

	string history = nvs_ampoules_history_temp.getString(
			"temp_history_" + to_string(index));

	return history;
}

LinkedList<ampoule_test_history_t> load_ampoules_histories_temp() {
	LinkedList<ampoule_test_history_t> history_list = LinkedList<
			ampoule_test_history_t>();

	for (int i = 0; i < 4; i++) {
		string history_str = load_ampoules_history_temp(i);
		ampoule_test_history_t test_hist = ampoule_test_history_t();

		if (history_str == "") {
//			test_hist.id_test = -1;
//			history.add(test_hist);
			continue;
		}

		vector<string> history_splited = split(history_str, ";");

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

		history_list.add(test_hist);
	}

	return history_list;
}

esp_err_t reset_ampoules_history_temp(int index) {
	bool ok = nvs_ampoules_history_temp.erase(
			"temp_history_" + to_string(index), true);

	return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t save_ampoules_test(string history) {
	ESP_LOGI(TAG, "Prepare to save Ampoules Test History in NVS\n");

	bool ok = false;

	if (history.length() > 0) {

		ok = nvs_ampoules_history.setString("history", history, true);
	}

	if (ok) {
		ESP_LOGI(TAG, "Ampoules Test History saved in NVS!\n");
	}

	return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t save_ampoules_history_temp(string history, int index) {
	ESP_LOGI(TAG, "Prepare to save Ampoules Temp History in NVS\n");

	bool ok = false;

	if (history.length() > 0) {

		ok = nvs_ampoules_history_temp.setString(
				"temp_history_" + to_string(index), history, true);
	}

	if (ok) {
		ESP_LOGI(TAG, "Ampoules Temp History saved in NVS!\n");
	}

	return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t save_language(string language) {
	ESP_LOGI(TAG, "Prepare to save Language in NVS\n");

	bool ok = false;

	if (language.length() > 0) {

		ok = nvs_storage.setString("language", language, true);
	}

	if (ok) {
		ESP_LOGI(TAG, "Language saved in NVS!\n");
	}

	return ok ? ESP_OK : ESP_FAIL;
}

string get_language() {

	string sn = nvs_storage.getString("language");

	if (sn.length() <= 0) {
		sn = "pt-br";
	}

	printf("\n\nLanguage: %s\n\n", sn.c_str());

	return sn;
}

string get_serial_number() {

	if (serial_number == "") {

		string sn = nvs_storage.getString("serial_number");

		if (sn.length() <= 0) {
			sn = string(DEFAULT_SERIAL_NUMBER);
		}

		printf("\n\nSerial Number %s\n\n", sn.c_str());

		return sn;
	}

	return serial_number;
}

float get_positive_percentage() {

	if (positive_percentage == -1.0f) {
		float pp = nvs_storage.getFloat("positive_perc", 0.0f);

		if (pp == 0.0f) {
			return 20.0f;
		}

		return pp;
	}

	return positive_percentage;

}

device_settings_t read_device_settings() {
	ESP_LOGI(TAG, "Prepare to read Device Setting on NVS\n");

	string institution = get_institution();

	if (institution.length() <= 0) {
		institution = "**********";
	}

	ESP_LOGI(TAG, "Device Settings: Institution: %s!\n", institution.c_str());

	device_settings_t settings;
	settings.institution = institution;

	return settings;
}

esp_err_t reset_user_data() {
	ESP_LOGI(TAG, "Excluindo dados da institui��o");

	bool ok = nvs_storage.erase("institution", true);

	if (ok) {
		ESP_LOGI(TAG, "Institui��o excluida com sucesso");
	} else {
		ESP_LOGE(TAG, "Nenhuma Institui��o excluida");
	}

//	ESP_LOGI(TAG, "Excluindo Fator de Calibra��o");
//
//	ok = nvs_storage.erase("calib_factor", true);
//
//	if (ok) {
//		ESP_LOGI(TAG, "Fator de Calibra��o excluido com sucesso");
//	} else {
//		ESP_LOGE(TAG, "Nenhum Fator de Calibra��o excluido");
//	}

	ESP_LOGI(TAG, "Excluindo Resultado dos ultimos 12 testes");

	ok = nvs_ampoules_history.erase("history", true);

	if (ok) {
		ESP_LOGI(TAG, "Resultado dos ultimos 12 testes excluidos com sucesso");
	} else {
		ESP_LOGE(TAG, "Nao ha resultado de testes para serem excluido");
	}

	// Temp history

	ESP_LOGI(TAG, "Excluindo dados temporarios");

	ok = nvs_ampoules_history_temp.erase("temp_history_0", true);

	if (ok) {
		ESP_LOGI(TAG, "Dado temporario excluido com sucesso");
	} else {
		ESP_LOGE(TAG, "Nao ha dados temporarios para serem excluido");
	}

	ok = nvs_ampoules_history_temp.erase("temp_history_1", true);

	if (ok) {
		ESP_LOGI(TAG, "Dado temporario excluido com sucesso");
	} else {
		ESP_LOGE(TAG, "Nao ha dados temporarios para serem excluido");
	}

	ok = nvs_ampoules_history_temp.erase("temp_history_2", true);

	if (ok) {
		ESP_LOGI(TAG, "Dado temporario excluido com sucesso");
	} else {
		ESP_LOGE(TAG, "Nao ha dados temporarios para serem excluido");
	}

	ok = nvs_ampoules_history_temp.erase("temp_history_3", true);

	if (ok) {
		ESP_LOGI(TAG, "Dado temporario excluido com sucesso");
	} else {
		ESP_LOGE(TAG, "Nao ha dados temporarios para serem excluido");
	}

	// end Temp History

	ok = nvs_ampoules.setInt("test_conter", (int32_t)0, true);

	if (ok) {
		ESP_LOGI(TAG, "Sequencia numerica dos testes excluida com sucesso");
	} else {
		ESP_LOGE(TAG, "Nenhuma sequencia numerica dos testes foi excluida");
	}

	esp_err_t err = rtc_write_sram_memory(0);

	int32_t counter = rtc_read_sram_memory();

	if (err == ESP_OK && counter == 0) {
		ESP_LOGI(TAG, "Sequencia numerica dos testes reiniciada com sucesso");
	} else {
		ESP_LOGE(TAG, "Nenhuma sequencia numerica dos testes foi reiniciada");
	}

	return ESP_OK;
}

esp_err_t get_ampoule_test_counter(int32_t &ampoute_test_cont) {
	ESP_LOGI(TAG, "Save Ampoule Test Counter\n");

	int32_t ampoule_test_counter = nvs_ampoules.getInt("test_conter", 0);

	ampoule_test_counter++;
	ampoute_test_cont = ampoule_test_counter;

	bool ok = nvs_ampoules.setInt("test_conter", ampoule_test_counter, true);

	ESP_LOGI(TAG, "Ampoule Test Counter: (%ld)!\n", ampoule_test_counter);

	return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t get_new_ampoule_test_count(int32_t &ampoule_test_count) {
	ESP_LOGI(TAG, "Save Ampoule Test Counter in SRAM - DS1302\n");

	int32_t ampoule_test_counter = rtc_read_sram_memory();

	int32_t ampoule_test_counter_rom = nvs_ampoules.getInt("test_conter", 0);

	ESP_LOGI(TAG, "Ampoule Test Counter in ROM: (%ld)!\n",
			ampoule_test_counter_rom);

	if (ampoule_test_counter_rom > ampoule_test_counter) {
		ampoule_test_counter = ampoule_test_counter_rom;
	}

	if (ampoule_test_counter == 0 || ampoule_test_counter == -1) {
		ampoule_test_counter = 1;
	} else {
		ampoule_test_counter++;
	}

	ampoule_test_count = ampoule_test_counter;

	ESP_LOGI(TAG, "Ampoule Test Counter: (%ld)!\n", ampoule_test_count);

	esp_err_t ok = rtc_write_sram_memory((int)ampoule_test_counter);

	ESP_LOGI(TAG, "Ampoule Test Counter: (%ld)!\n", ampoule_test_counter);

	ok = (esp_err_t)nvs_ampoules.setInt("test_conter", ampoule_test_counter, true);

	ESP_LOGI(TAG, "Ampoule Test Counter: (%ld)!\n", ampoule_test_counter);

	return ok;
}

esp_err_t save_reset_reason(string reasons) {
	ESP_LOGI(TAG, "Prepare to save Reset Reason in NVS\n");

	bool ok = false;

	if (reasons.length() > 0) {

		ok = nvs_storage.setString("reset_reason", reasons, true);
	}

	if (ok) {
		ESP_LOGI(TAG, "Reset Reason saved in NVS!\n");
	}

	return ok ? ESP_OK : ESP_FAIL;
}

LinkedList<string> load_reset_reason() {
	ESP_LOGI(TAG, "Prepare to load Reset Reason on NVS\n");

	string reasons = nvs_storage.getString("reset_reason");

	ESP_LOGI(TAG, "Reset Reason: %s!\n", reasons.c_str());

	LinkedList<string> list = LinkedList<string>();

	if (reasons.length() > 0) {

		vector<string> v_reasons = split(reasons, ";");

		for (const auto &hist : v_reasons) {
			list.add(hist.c_str());
		}

		ESP_LOGI(TAG, "Reset Reason size: %d!\n", list.size());
	}

	ESP_LOGI(TAG, "Aqui retornando a lista\n");
	return list;
}
