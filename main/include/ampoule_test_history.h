#ifndef AMPOULE_TEST_HISTORY_H_
#define AMPOULE_TEST_HISTORY_H_

#include "esp_system.h"
#include "esp_err.h"
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include "queue.h"

using namespace std;

#include <LinkedList.h>

struct ampoule_test_history_t {
	int id;
	int32_t id_test;
	string ampola;
	string dt_inicio;
	string hr_inicio;
	string dt_fim;
	string hr_fim;
	string resultado;
	string ciclo;
	int temperature;
	float positive_percentage;
};

#endif
