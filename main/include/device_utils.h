#ifndef DEVICE_UTILS_H_
#define DEVICE_UTILS_H_

#include <stdio.h>
#include <string>
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif
using namespace std;


typedef struct {
	string institution;
	string version;
	esp_err_t err = ESP_OK;
} device_settings_t;


#ifdef __cplusplus
}
#endif

#endif /* DEVICE_UTILS_H_ */
