#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "DateTime.h"

using namespace std;

tm add_seconds(tm t1, int seconds) {
	time_t epoch = mktime(&t1);
	epoch += seconds;
	return *localtime(&epoch);
}

void add_seconds(char *buffer, string date1, int seconds) {
	tm time = { };

	istringstream ss1(date1);
	ss1 >> get_time(&time, "%d/%m/%Y %H:%M:%S");

	tm new_time = add_seconds(time, seconds);

	sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", new_time.tm_mday,
			(new_time.tm_mon + 1), (new_time.tm_year + 1900), new_time.tm_hour,
			new_time.tm_min, new_time.tm_sec);

	printf("Date & Time Init: %s\n", date1.c_str());
	printf("Date & Time End: %s\n", buffer);
}

time_t get_time(string date) {
	tm tm = { };

	int day = stoi(date.substr(0, 2));
	int month = stoi(date.substr(3, 2));
	int year = stoi(date.substr(6, 4));

	int hour = stoi(date.substr(11, 2));
	int minute = stoi(date.substr(14, 2));
	int second = stoi(date.substr(17, 2));

	printf("date: %s\n", date.c_str());
	printf("day: %d\n", day);
	printf("month: %d\n", month);
	printf("year: %d\n", year);
	printf("hour: %d\n", hour);
	printf("minute: %d\n", minute);
	printf("second: %d\n", second);

	tm.tm_mday = day;
	tm.tm_mon = month;
	tm.tm_year = year;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;

	return mktime(&tm);
}

long diff_time(string date1, string date2) {
	tm tm1 = { };
	tm tm2 = { };

//	istringstream ss1(date1);
//	ss1 >> get_time(&tm1, "%d/%m/%Y %H:%M:%S");
//
//	std::istringstream ss2(date2);
//	ss2 >> get_time(&tm2, "%d/%m/%Y %H:%M:%S");
//
//	time_t s1 = mktime(&tm1);
//	time_t s2 = mktime(&tm2);

	time_t s1 = get_time(date1);
	time_t s2 = get_time(date2);

	printf("s1: %lld\n", s1);
	printf("s2: %lld\n", s2);

	auto difference = difftime(s1 < s2 ? s2 : s1, s1 < s2 ? s1 : s2);

	printf("diferenca: %f\n", difference);

	return (long) difference;
}

string diffDateTime(string date1, string date2, long reference) {

	tm tm1 = { };
	tm tm2 = { };

//	istringstream ss1(date1);
//	ss1 >> get_time(&tm1, "%d/%m/%Y %H:%M:%S");
//
//	std::istringstream ss2(date2);
//	ss2 >> get_time(&tm2, "%d/%m/%Y %H:%M:%S");
//
//	time_t s1 = mktime(&tm1);
//	time_t s2 = mktime(&tm2);

	time_t s1 = get_time(date1);
	time_t s2 = get_time(date2);

	printf("s1: %lld\n", s1);
	printf("s2: %lld\n", s2);

	auto difference = difftime(s1 < s2 ? s2 : s1, s1 < s2 ? s1 : s2);

	printf("diferenca: %f\n", difference);

	if (difference < reference) {
		difference = reference;
	} else if ((difference - reference) > 30) {
		difference = reference;
	}

	int hours = difference / 3600;
	int mins = (difference - (hours * 3600)) / 60;
	int secs = difference - (hours * 3600) - (mins * 60);

	// Formata a sa�da para o formato "hh:mm:ss".
	string output;

	if (hours < 10)
		output += "0";     // Adiciona um 0 � esquerda se necess�rio.
	output += to_string(hours) + ":";

	if (mins < 10)
		output += "0";      // Adiciona um 0 � esquerda se necess�rio.
	output += to_string(mins) + ":";

	if (secs < 10)
		output += "0";      // Adiciona um 0 � esquerda se necess�rio.
	output += to_string(secs);

//	printf("output: %s\r\n", output.c_str());

	return output;     // Retorna o valor calculado em formato de string.
}

string diffDateTime(string date1, string date2) {

	tm tm1 = { };
	tm tm2 = { };

//	istringstream ss1(date1);
//	ss1 >> get_time(&tm1, "%d/%m/%Y %H:%M:%S");
//
//	std::istringstream ss2(date2);
//	ss2 >> get_time(&tm2, "%d/%m/%Y %H:%M:%S");
//
//	time_t s1 = mktime(&tm1);
//	time_t s2 = mktime(&tm2);

	time_t s1 = get_time(date1);
	time_t s2 = get_time(date2);

	printf("s1: %lld\n", s1);
	printf("s2: %lld\n", s2);

	auto difference = difftime(s1 < s2 ? s2 : s1, s1 < s2 ? s1 : s2);

	printf("diferenca: %f\n", difference);

	int hours = difference / 3600;
	int mins = (difference - (hours * 3600)) / 60;
	int secs = difference - (hours * 3600) - (mins * 60);

	// Formata a sa�da para o formato "hh:mm:ss".
	string output;

	if (hours < 10)
		output += "0";     // Adiciona um 0 � esquerda se necess�rio.
	output += to_string(hours) + ":";

	if (mins < 10)
		output += "0";      // Adiciona um 0 � esquerda se necess�rio.
	output += to_string(mins) + ":";

	if (secs < 10)
		output += "0";      // Adiciona um 0 � esquerda se necess�rio.
	output += to_string(secs);

//	printf("output: %s\r\n", output.c_str());

	return output;     // Retorna o valor calculado em formato de string.
}

int convertStringToSeconds(string time) {
	int hh, mm, ss;

	// Extrair os valores de horas, minutos e segundos da string.
	hh = stoi(time.substr(0, 2));
	mm = stoi(time.substr(3, 2));
	ss = stoi(time.substr(6, 2));

	// Calcular o total de segundos.
	int totalSeconds = (hh * 3600) + (mm * 60) + ss;

	return totalSeconds;
}
