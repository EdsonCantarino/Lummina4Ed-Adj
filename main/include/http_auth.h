#ifndef HTTP_AUTH_H_
#define HTTP_AUTH_H_

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_tls_crypto.h"
#include <esp_http_server.h>


typedef struct {
	char *username;
	char *password;
} basic_auth_info_t;


#endif
