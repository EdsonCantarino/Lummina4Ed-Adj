#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_tls_crypto.h"

#include "include/http_app.h"
#include "include/app_httpd.h"

#include "sdkconfig.h"
//#include "version_config.h"

#ifndef DEFAULT_AP_IP

#define DEFAULT_AP_IP CONFIG_DEFAULT_AP_IP

#endif

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

/* @brief the HTTP server handle */
static httpd_handle_t httpd_handle = NULL;

/* strings holding the URLs of the wifi manager */
static char *http_root_url = NULL;
static char *http_redirect_url = NULL;
static char *http_js_url = NULL;
static char *http_css_url = NULL;

//const static char http_404_hdr[] = "404 Not Found";

void http_app_stop() {

	if (httpd_handle != NULL) {

		/* dealloc URLs */
		if (http_root_url) {
			free(http_root_url);
			http_root_url = NULL;
		}
		if (http_redirect_url) {
			free(http_redirect_url);
			http_redirect_url = NULL;
		}
		if (http_js_url) {
			free(http_js_url);
			http_js_url = NULL;
		}
		if (http_css_url) {
			free(http_css_url);
			http_css_url = NULL;
		}

		/* stop server */
		httpd_stop(httpd_handle);
		httpd_handle = NULL;
	}
}

void http_app_start(bool lru_purge_enable) {
	esp_err_t err;

	if (httpd_handle == NULL) {

		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.max_uri_handlers = 38;

		/* this is an important option that isn't set up by default.
		 * We could register all URLs one by one, but this would not work while the fake DNS is active */
		config.uri_match_fn = httpd_uri_match_wildcard;
		config.lru_purge_enable = lru_purge_enable;
		config.send_wait_timeout = 5;
		config.recv_wait_timeout = 30;
		//config.max_open_sockets = 8;

		/* generate the URLs */
		if (http_root_url == NULL) {
			int root_len = strlen(WEBAPP_LOCATION);
			/* root url, eg "/"   */
			const size_t http_root_url_sz = sizeof(char) * (root_len + 1);
			http_root_url = malloc(http_root_url_sz);
			memset(http_root_url, 0x00, http_root_url_sz);
			strcpy(http_root_url, WEBAPP_LOCATION);

			/* redirect url */
			size_t redirect_sz = 22 + root_len + 1; /* strlen(http://255.255.255.255) + strlen("/") + 1 for \0 */
			http_redirect_url = malloc(sizeof(char) * redirect_sz);
			*http_redirect_url = '\0';

			if (root_len == 1) {
				snprintf(http_redirect_url, redirect_sz, "http://%s",
				DEFAULT_AP_IP);
			} else {
				snprintf(http_redirect_url, redirect_sz, "http://%s%s",
				DEFAULT_AP_IP, WEBAPP_LOCATION);
			}

		}

		err = httpd_start(&httpd_handle, &config);

		if (err == ESP_OK) {
			ESP_LOGI(TAG, "Registering URI handlers");

			app_httpd_register_uri(httpd_handle);
		}
	}
}
