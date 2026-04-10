#ifndef AmpouleSensor_h
#define AmpouleSensor_h

#include "esp_system.h"
#include "esp_log.h"
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include "queue.h"
#include <iomanip>

using namespace std;

#include <LinkedList.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "DateTime.h"

#include "nvs_helpers.h"

class Utils {

private:
	NVSHelper nvs_storage;

public:
	// for string delimiter
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

	float get_positive_percentage() {

		nvs_storage.begin();

		float pp = nvs_storage.getFloat("positive_perc", 0.0f);

		if (pp == 0.0f) {
			return 40.0f;
		}

		//setprecision(1);

		return pp;
	}
};

class AmpouleTestResult: public Utils {
private:
	int seconds, hours, minutes;
	int id = 0;
	string id_test = "";
	string date_start = "";
	string hour_start = "";
	string date_end = "";
	string hour_end = "";
	bool is_positived = false;
	bool is_canceled = false;
	int cicle = 0;
	int temperature = 0;

public:

	void set_result(bool positived, bool canceled = false) {
		is_positived = positived;
		is_canceled = canceled;
	}

	void set_temperature(float temp) {
		temperature = (int) temp;
	}

	void set_id(int i) {
		id = i;
	}

	string get_id() {
		//return string("0" + to_string(id));
		return string(to_string(id));
	}

	void set_id_test(string id) {
		id_test = id;
	}

	string get_id_test() {

		return id_test;
	}

	int get_temperature() {
		return temperature == 0 ? 60 : temperature;
	}

	string get_date(bool start = true) {
		if (start)
			return date_start;
		else
			return date_end;
	}

	string get_hour(bool start = true) {
		if (start)
			return hour_start;
		else
			return hour_end;
	}

	string get_time_in_test() {
		string date1 = (date_start + " " + hour_start);
		string date2 = (date_end + " " + hour_end);

		string time_diff = diffDateTime(date2, date1);

		//printf("date_time_start: %s\r\n", date1.c_str());
		//printf("date_time_end: %s\r\n", date2.c_str());
		//printf("time_diff: %s\r\n", time_diff.c_str());

		return time_diff;
	}

	void set_cicle(long total_seconds) {
		hours = total_seconds / 3600;
		minutes = (total_seconds % 3600) / 60;
		seconds = (total_seconds % 3600) % 60;
	}

	string get_cicle() {
		string time;

		if (hours <= 0) {
			time = to_string(minutes) + " MIN";
		} else {
			time = to_string(hours) + " HOR";
		}

		//printf("%s\n", time.c_str());

		return time;
	}

	void set_date_time(string date, bool is_start = true) {

		vector<string> date_time = split(date, " ");

		if (date_time.size() < 2)
			return;
		else {
			if (is_start) {
				date_start = date_time[0];
				hour_start = date_time[1];
			} else {
				date_end = date_time[0];
				hour_end = date_time[1];
			}
		}

//		printf("%s\n", date_start.c_str());
//		printf("%s\n", hour_start.c_str());
//		printf("%s\n", date_end.c_str());
//		printf("%s\n", hour_end.c_str());
	}

	string get_formated_date_time(bool is_start = true,
			string separator = " ") {

		if (is_start) {
			return date_start + separator + hour_start;
		} else {
			return date_end + separator + hour_end;
		}
	}

	bool get_result_positived() {
		return is_positived;
	}

	bool get_result_canceld() {
		return is_canceled;
	}
};

class AmpouleSensor: public Utils {
private:
	bool print_result_console = false;
	bool print_result_console_count = 0;
	LinkedList<long> ordered_samples;
	bool is_ordered = false;

	bool ampoule_current_status = false;
	bool ampoule_previous_status = false;

	bool ampoule_alarm_inserted = false;
	bool ampoule_alarm_removed = false;
	bool ampoule_is_alarm_on = true;

	float positive_percentage = 40.0f;
	bool is_disabled = false;

	bool is_locked = false;

public:
	int id;
	long id_test = 0;
	bool is_present = false;
	long time_test;

	string date_start = "";
	string hour_start = "";
	string date_end = "";
	string hour_end = "";

	long average;
	bool test_done = false;
	bool is_positived;
	bool is_testing = false;
	long current_time = 0;

	bool print_result = true;
	bool printed = false;
	bool cancelled_by_temp = false;

	//long current_time = 0;

	int count_alarm_is_ausent = 1;

	LinkedList<long> samples = LinkedList<long>();

	AmpouleSensor() { // @suppress("Class members should be properly initialized")

	}

	AmpouleSensor(int id, bool p) { // @suppress("Class members should be properly initialized")
		this->id = id;
		this->is_present = p;
	}

	~AmpouleSensor() {

	}

	bool get_locked_status() {
		return is_locked;
	}

	void set_locked_status(bool status) {
		is_locked = status;
	}

	bool get_disabled_status() {
		return is_disabled;
	}

	void set_disabled_status(bool disabled) {
		is_disabled = disabled;
	}

	long get_current_time() {
		string date1 = (date_start + " " + hour_start);
		string date2 = (date_end + " " + hour_end);

		return diff_time(date2, date1);
	}

	void set_positive_percentage(float positive_perc) {
		positive_percentage = positive_perc;
	}

	void set_date_time(string date, bool is_start = true) {

		vector<string> date_time = split(date, " ");

		if (date_time.size() < 2)
			return;
		else {
			if (is_start) {
				date_start = date_time[0];
				hour_start = date_time[1];
			} else {
				date_end = date_time[0];
				hour_end = date_time[1];
			}
		}
	}

	string get_formated_date_time(bool is_start = true) {

		if (is_start) {
			return date_start + " " + hour_start;
		} else {
			return date_end + " " + hour_end;
		}
	}

	char* get_ampoule_id() {
		char *num;
		char buffer[2];

		asprintf(&num, "%d", id);

		strcat(strcpy(buffer, "0"), num);

		char *buf = buffer;

		return buf;
	}

	void set_apoule_alarm(bool status) {
		bool is_changed_status = false;

		// estado inicial da ampola
		if (status != ampoule_current_status) {
			ampoule_current_status = status;

			is_changed_status = true;
		}

		if (is_changed_status && status) {
			ampoule_alarm_removed = false;
			ampoule_alarm_inserted = true;
		}

		if (is_changed_status && !status) {
			ampoule_alarm_removed = true;
			ampoule_alarm_inserted = false;
		}

//		printf(
//				"******* ampoule_alarm_removed: %d - ampoule_alarm_inserted: %d - is_changed_status: %d - ampoule_current_status: %d \n",
//				ampoule_alarm_removed, ampoule_alarm_inserted,
//				is_changed_status, ampoule_current_status);
	}

	void clear_ampoule_alarm() {
		ampoule_alarm_removed = false;
		ampoule_alarm_inserted = false;

	}

	void set_ampoule_is_alarm_on(bool status) {
		ampoule_is_alarm_on = status;
	}

	int get_ampoule_alarm() {
		bool error = is_error() && count_alarm_is_ausent >= 0;

		if (ampoule_alarm_inserted)
			return 1;
		else if (ampoule_alarm_removed && !error)
			return 2;
		else if (error && count_alarm_is_ausent == 1) {
			return 3;
		} else
			return 0;
	}

	string get_time_in_test() {
		string date1 = (date_start + " " + hour_start);
		string date2 = (date_end + " " + hour_end);

		string time_diff = diffDateTime(date2, date1);

//		printf("date_time_start: %s\r\n", date1.c_str());
//		printf("date_time_end: %s\r\n", date2.c_str());
//		printf("time_diff: %s\r\n", time_diff.c_str());

		return time_diff;
	}

	bool is_error() {
		return this->is_testing && !this->is_present && !this->test_done;
	}

	void add_sensor_value(long val) {

		printf("Tamanho da lista de resultados: (%d)\n", samples.size());

		if (samples.size() < 10) {
			samples.add(val);
		}else{

			samples.remove(5);

			printf("Tamanho da lista de resultados (após exclusão): (%d)\n", samples.size());

			samples.add(val);

			printf("Tamanho da lista de resultados (final): (%d)\n", samples.size());
		}
	}

	void clear_sensor_values() {
		samples.clear();
	}

	long sum_values() {
		long value = 0;

		int size = samples.size();

		for (int i = 0; i < size; i++) {
			value = value + samples.get(i);
		}

		return value;
	}

	long get_average() {
		long value = 0;

		int size = samples.size();

		for (int i = 0; i < size; i++) {
			long v = samples.get(i);
			long xvalue = (long) v;

			value = value + xvalue;
		}

		if (value == 0 || size == 0)
			return 0;

		average = (value / size);

		return average;
	}

	void print_test_result() {
		int size = samples.size();

		if (size > 0) {
			printf(
					"\n\n**************************************************************\n");
			printf("Ampola sob teste (%d)\n", id);
			printf("Data e hora de inicio: %s\n",
					get_formated_date_time(true).c_str());
			printf("Data e hora de fim: %s\n",
					get_formated_date_time(false).c_str());
			printf("Resultado:\n");

			long value = 0;

			for (int i = 0; i < size; i++) {
				long v = samples.get(i);
				long xvalue = (long) v;

				printf("     Valor da amostra [%d]:   %ld\r\n", i + 1, xvalue);

				value = value + xvalue;
			}

			average = (value / size);

			printf("     -----------------------------\n");
			printf("     Soma dos valores:       %ld\r\n", value);
			printf("     Quantidade de amostras: %d\r\n", size);
			printf("     -----------------------------\n");
			printf("     Media dos valores:      %ld\r\n", average);

			bool res = get_test_result(average);

			if (res) {
				printf("\n******* AMOSTRA POSITIVADA *******\n");
			} else {
				printf("\n******* AMOSTRA NEGATIVADA *******\n");
			}

			printf(
					"\n**************************************************************\n\n");
		}
	}

	static int compare(long &a, long &b) {
		if (a > b)
			return 1;
		else if (a < b)
			return -1;
		else
			return 0;
	}

	bool calcule_test_result() {
		average = get_average();

		return get_test_result(average, false);
	}

	bool get_test_result() {
		return calcule_test_result();
	}

	bool get_test_result(float average, bool print_perc_msg = true) {

		int size = samples.size();

		if (size <= 0)
			return false;

		float diff = 0.0f;
		float medH = 0;
		float medL = 0;
		medH = medH + (float) samples.get(size - 1);
		medH = medH + (float) samples.get(size - 2);
		medH = medH + (float) samples.get(size - 3);
		medH = medH + (float) samples.get(size - 4);
		medH = medH + (float) samples.get(size - 5);
		medL = medL + (float) samples.get(0);
		medL = medL + (float) samples.get(1);
		medL = medL + (float) samples.get(2);
		medL = medL + (float) samples.get(3);
		medL = medL + (float) samples.get(4);

		for (int i = 0; i < size; i++) {
			float s = (float) samples.get(i) - average;

			if (s < 0.0f)
				s = s * -1.0f;

			diff += s;
		}

		if (diff > 0)
			diff = (float) diff / size;
		diff = (diff / average) * 100;

		diff = ((medH - medL) / medL) * 100;

//		if (diff < 0.0f)
//			diff = diff * -1.0f;

		if (print_perc_msg) {
			printf("     Variacao Percentual:    %.2f\r\n", diff);
			printf("     -----------------------------\n");
		}

		float pp = get_positive_percentage();
		printf("\nPositive Percentage: %f\n", pp);

		//is_positived = diff > 50.0f;
		//is_positived = diff > 10.0f;

		is_positived = diff > pp;

		printf("\nIs Positive: %d\n?", is_positived);

		// apenas para testar
//		if(id == 3 || id == 1)
//			is_positived = true;

		return is_positived;

	}
};

#endif
