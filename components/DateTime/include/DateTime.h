
#ifndef AMPOULE_SENSOR_DATETIME_H_
#define AMPOULE_SENSOR_DATETIME_H_

#include <iostream>
#include <string>
#include <ctime>

using namespace std;

void add_seconds(char *buffer, string date1, int seconds);

string diffDateTime(string date1, string date2);
long diff_time(string date1, string date2);


#endif
