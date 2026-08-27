#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <cmath>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include <sys/param.h>

#include "esp_tls_crypto.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"

#include <esp_http_server.h>

#include "include/nvs_utils.h"
#include "include/http_app.h"
#include "include/device_utils.h"
#include "include/app_httpd.h"
#include "include/rtc_ds1302.h"
#include "include/ampoule_test.h"
#include "advanced_config.h"
#include "ampoule_history.h"

#include "include/version_config.h"
#include "include/temperature.h"
#include "include/http_auth.h"
#include "include/security.h"

#include "branding.h"

static const char *TAG = "app_httpd";

static char* http_auth_basic(const char *username, const char *password) {
	int out;
	char *user_info = NULL;
	char *digest = NULL;
	size_t n = 0;
	asprintf(&user_info, "%s:%s", username, password);
	if (!user_info) {
		ESP_LOGE(TAG, "No enough memory for user information");
		return NULL;
	}
	esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char*) user_info,
			strlen(user_info));

	/* 6: The length of the "Basic " string
	 * n: Number of bytes for a base64 encode format
	 * 1: Number of bytes for a reserved which be used to fill zero
	 */
	digest = (char*) calloc(1, 6 + n + 1);
	if (digest) {
		strcpy(digest, "Basic ");
		esp_crypto_base64_encode((unsigned char*) digest + 6, n, (size_t*) &out,
				(const unsigned char*) user_info, strlen(user_info));
	}
	free(user_info);
	return digest;
}

static char* http_check_password(const char *password) {
	int out;
	char *user_info = NULL;
	char *digest = NULL;
	size_t n = 0;
	asprintf(&user_info, "%s", password);
	if (!user_info) {
		ESP_LOGE(TAG, "No enough memory for user information");
		return NULL;
	}
	esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char*) user_info,
			strlen(user_info));

	/* n: Number of bytes for a base64 encode format
	 * 1: Number of bytes for a reserved which be used to fill zero
	 */
	digest = (char*) calloc(1, n + 1);
	if (digest) {
		esp_crypto_base64_encode((unsigned char*) digest, n, (size_t*) &out,
				(const unsigned char*) user_info, strlen(user_info));
	}
	free(user_info);
	return digest;
}

static esp_err_t check_password(httpd_req_t *req) {
	char *buf = NULL;
	size_t buf_len = 0;

	//char *p = NULL;

	buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;

	if (buf_len <= 1) {
		ESP_LOGE(TAG, "No auth header received");

		const char *msg = "{ \"message\": \"Informe a senha corretamente.\"}";
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");

		httpd_resp_sendstr(req, msg);

		free(buf);

		return ESP_FAIL;
	} else {
		buf = (char*) calloc(1, buf_len);
		if (!buf) {
			ESP_LOGE(TAG, "No enough memory for basic authorization");
			return ESP_ERR_NO_MEM;
		}

		if (httpd_req_get_hdr_value_str(req, "Authorization", buf,
				buf_len) == ESP_OK) {
			ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
		} else {
			ESP_LOGE(TAG, "No auth value received");
		}

		string password = getPassword();

		char *auth_credentials = http_check_password(password.c_str());

		printf("\nSenha enviado pelo usuario (base64): %s\n\n", buf);
		printf("Senha sistema (base64): %s - decode: %s\n\n", auth_credentials,
				password.c_str());

		if (!auth_credentials) {
			ESP_LOGE(TAG,
					"No enough memory for basic authorization credentials");
			free(buf);
			return ESP_ERR_NO_MEM;
		}

		if (strcmp(buf, auth_credentials) != 0) {
			// as senhas s�o diferentes retornar mensagem para o usu�rio

			const char *msg =
					"{ \"message\": \"A senha digitada esta incorreta, verifique.\"}";

			printf("%s\n", msg);

			httpd_resp_set_status(req, HTTPD_401);
			httpd_resp_set_type(req, "application/json");
			httpd_resp_set_hdr(req, "Connection", "keep-alive");

			httpd_resp_sendstr(req, msg);

			free(auth_credentials);
			free(buf);

			return ESP_FAIL;
		} else {

			printf("Senha validada com sucesso!\n");

			free(auth_credentials);
			free(buf);

			return ESP_OK;
		}
	}
}

static esp_err_t check_basic_auth(httpd_req_t *req) {
	char *buf = NULL;
	size_t buf_len = 0;
	basic_auth_info_t *basic_auth_info = (basic_auth_info_t*) req->user_ctx;


        (void)basic_auth_info;buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;

	if (buf_len <= 1) {
		ESP_LOGE(TAG, "No auth header received");
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Lummina4\"");
		httpd_resp_send(req, NULL, 0);

		free(buf);

		return ESP_FAIL;
	} else {
		buf = (char*) calloc(1, buf_len);
		if (!buf) {
			ESP_LOGE(TAG, "No enough memory for basic authorization");
			return ESP_ERR_NO_MEM;
		}

		if (httpd_req_get_hdr_value_str(req, "Authorization", buf,
				buf_len) == ESP_OK) {
			ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
		} else {
			ESP_LOGE(TAG, "No auth value received");
		}

		//printf("basic_auth_info->username: %s\n", basic_auth_info->username);
		//printf("basic_auth_info->password: %s\n", basic_auth_info->password);

		string user_name = getUserMaster();
		string password = getPasswordMaster();

		char *auth_credentials = http_auth_basic(user_name.c_str(),
				password.c_str());

//		char *auth_credentials = http_auth_basic(basic_auth_info->username,
//						basic_auth_info->password);

		if (!auth_credentials) {
			ESP_LOGE(TAG,
					"No enough memory for basic authorization credentials");
			free(buf);
			return ESP_ERR_NO_MEM;
		}

		if (strncmp(auth_credentials, buf, buf_len)) {
			ESP_LOGE(TAG, "Not authenticated");
			httpd_resp_set_status(req, HTTPD_401);
			httpd_resp_set_type(req, "application/json");
			httpd_resp_set_hdr(req, "Connection", "keep-alive");
			httpd_resp_set_hdr(req, "WWW-Authenticate",
					"Basic realm=\"Lummina4\"");
			httpd_resp_send(req, NULL, 0);

			free(auth_credentials);
			free(buf);

			return ESP_FAIL;
		} else {

			free(auth_credentials);
			free(buf);

			return ESP_OK;
		}
	}
}

static esp_err_t restart_device(httpd_req_t *req) {
	const char *msg =
			"{ \"message\": \"O dispositivo ser� reiniciado em 10 segundos\"}";
	httpd_resp_set_status(req, "200 OK");
	httpd_resp_sendstr(req, msg);

	// Restart module
	for (int i = 10; i >= 0; i--) {
		ESP_LOGI(TAG, "Restarting in %d seconds...\n", i);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	ESP_LOGI(TAG, "RESTART_ID=2 - Restarting now (HTTP restart_device).\n");

	fflush(stdout);
	vTaskDelay(pdMS_TO_TICKS(100));
	esp_restart();
}

static esp_err_t bootstrap_css_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	/* our custom page sits at /helloworld in this example */
	if (uri.find("/bootstrap/css/bootstrap.min.css") != string::npos) {
		extern const unsigned char _start_bootstrap_min_css[] asm("_binary_bootstrap_min_css_gz_start");
		extern const unsigned char _end_bootstrap_min_css[] asm("_binary_bootstrap_min_css_gz_end");

		printf("Index do pagina: %s - %s", _start_bootstrap_min_css, _end_bootstrap_min_css);

		const size_t _size = (_end_bootstrap_min_css - _start_bootstrap_min_css);
		httpd_resp_set_type(req, "text/css");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_bootstrap_min_css, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_toaster_css_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	/* our custom page sits at /helloworld in this example */
	if (uri.find("/bootstrap/css/bootstrap-toaster.min.css") != string::npos) {
		extern const unsigned char _start_bootstrap_toaster_min[] asm("_binary_bootstrap_toaster_min_css_gz_start");
		extern const unsigned char _end_bootstrap_toaster_min[] asm("_binary_bootstrap_toaster_min_css_gz_end");
		const size_t _size = (_end_bootstrap_toaster_min - _start_bootstrap_toaster_min);
		httpd_resp_set_type(req, "text/css");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_bootstrap_toaster_min, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}




// end resources

static esp_err_t bootstrap_icons_css_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/bootstrap/css/bootstrap-icons.css") != string::npos) {
		extern const unsigned char _start_bootstrap_icons_css[] asm("_binary_bootstrap_icons_css_gz_start");
		extern const unsigned char _end_bootstrap_icons_css[] asm("_binary_bootstrap_icons_css_gz_end");
		const size_t _size = (_end_bootstrap_icons_css - _start_bootstrap_icons_css);
		httpd_resp_set_type(req, "text/css");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_bootstrap_icons_css, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_icons_fonts_woff_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find(
			"/bootstrap/css/fonts/bootstrap-icons.woff?a97b3594ad416896e15824f6787370e0")
			!= string::npos) {
		extern const unsigned char _start[] asm("_binary_bootstrap_icons_woff_gz_start");
		extern const unsigned char _end[] asm("_binary_bootstrap_icons_woff_gz_end");
		const size_t _size = (_end - _start);
		httpd_resp_set_type(req, "application/font-woff");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_icons_fonts_woff2_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find(
			"/bootstrap/css/fonts/bootstrap-icons.woff2?a97b3594ad416896e15824f6787370e0")
			!= string::npos) {
		extern const unsigned char _start[] asm("_binary_bootstrap_icons_woff2_gz_start");
		extern const unsigned char _end[] asm("_binary_bootstrap_icons_woff2_gz_end");
		const size_t _size = (_end - _start);
		httpd_resp_set_type(req, "application/font-woff2");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_js_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/bootstrap/js/bootstrap.min.js") != string::npos) {
		extern const unsigned char _start_bootstrap_min_js[] asm("_binary_bootstrap_min_js_gz_start");
		extern const unsigned char _end_bootstrap_min_js[] asm("_binary_bootstrap_min_js_gz_end");
		const size_t _size = (_end_bootstrap_min_js - _start_bootstrap_min_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_bootstrap_min_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_toaster_js_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/bootstrap/js/bootstrap-toaster.min.js") != string::npos) {
		extern const unsigned char _start_bootstrap_toaster_min_js[] asm("_binary_bootstrap_toaster_min_js_gz_start");
		extern const unsigned char _end_bootstrap_toaster_min_js[] asm("_binary_bootstrap_toaster_min_js_gz_end");
		const size_t _size = (_end_bootstrap_toaster_min_js - _start_bootstrap_toaster_min_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_bootstrap_toaster_min_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_datetimepicker_css_get_handler(httpd_req_t *req) {
	if (strcmp(req->uri,
			"/bootstrap_datetimepicker/bootstrap.datetimepicker.min.css")
			== 0) {
		extern const unsigned char _start_datetimepicker_min_css[] asm("_binary_bootstrap_datetimepicker_min_css_gz_start");
		extern const unsigned char _end_datetimepicker_min_css[] asm("_binary_bootstrap_datetimepicker_min_css_gz_end");
		const size_t _size = (_end_datetimepicker_min_css - _start_datetimepicker_min_css);
		httpd_resp_set_type(req, "text/css");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_datetimepicker_min_css, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t bootstrap_datetimepicker_js_get_handler(httpd_req_t *req) {
	if (strcmp(req->uri,
			"/bootstrap_datetimepicker/bootstrap.datetimepicker.min.js") == 0) {
		extern const unsigned char _start_bootstrap_datetimepicker_min_js[] asm("_binary_bootstrap_datetimepicker_min_js_gz_start");
		extern const unsigned char _end_bootstrap_datetimepicker_min_js[] asm("_binary_bootstrap_datetimepicker_min_js_gz_end");
		const size_t _size = (_end_bootstrap_datetimepicker_min_js - _start_bootstrap_datetimepicker_min_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_bootstrap_datetimepicker_min_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t jquery_js_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/jquery/jquery.min.js") != string::npos) {
		extern const unsigned char _start_jquery_min_js[] asm("_binary_jquery_min_js_gz_start");
		extern const unsigned char _end_jquery_min_js[] asm("_binary_jquery_min_js_gz_end");
		const size_t _size = (_end_jquery_min_js - _start_jquery_min_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_jquery_min_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t moment_locale_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/moment/moment-with-locales.js") != string::npos) {
		extern const unsigned char _start_moment_with_locales_js[] asm("_binary_moment_with_locales_js_gz_start");
		extern const unsigned char _end_moment_with_locales_js[] asm("_binary_moment_with_locales_js_gz_end");
		const size_t _size = (_end_moment_with_locales_js - _start_moment_with_locales_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_moment_with_locales_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t moment_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/moment/moment.js") != string::npos) {
		extern const unsigned char _start_moment_js[] asm("_binary_moment_js_gz_start");
		extern const unsigned char _end_moment_js[] asm("_binary_moment_js_gz_end");
		const size_t _size = (_end_moment_js - _start_moment_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_moment_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t chart_js_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/chartjs/chart.min.js") != string::npos) {
		extern const unsigned char _start_chart_min_js[] asm("_binary_chart_min_js_gz_start");
		extern const unsigned char _end_chart_min_js[] asm("_binary_chart_min_js_gz_end");
		const size_t _size = (_end_chart_min_js - _start_chart_min_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_chart_min_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t custom_js_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/custom/custom.js") != string::npos) {
		extern const unsigned char _start_custom_js[] asm("_binary_custom_js_gz_start");
		extern const unsigned char _end_custom_js[] asm("_binary_custom_js_gz_end");
		const size_t _size = (_end_custom_js - _start_custom_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_custom_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t jquery_multiLanguage_js_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/jquerymultilanguage/jquery.multilanguage.min.js")
			!= string::npos) {
		extern const unsigned char _start_jquery_multilanguage_min_js[] asm("_binary_jquery_multilanguage_min_js_gz_start");
		extern const unsigned char _end_jquery_multilanguage_min_js[] asm("_binary_jquery_multilanguage_min_js_gz_end");
		const size_t _size = (_end_jquery_multilanguage_min_js - _start_jquery_multilanguage_min_js);
		httpd_resp_set_type(req, "text/javascript");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_jquery_multilanguage_min_js, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t translate_json_post_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/translate") != string::npos) {
		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc(SCRATCH_BUFSIZE);

		int received = 0;
		if (total_len >= SCRATCH_BUFSIZE) {
			/* Respond with 500 Internal Server Error */
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				free(buf);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		ESP_LOGI(TAG, "%s\n", buf);

		cJSON *root = cJSON_Parse(buf);
		free(buf);

		if (cJSON_GetObjectItem(root, "language")) {
			string l = string(
					cJSON_GetObjectItem(root, "language")->valuestring);

			if (l == "en-us") {
				//if (uri.find("/translate/en-us.json") != string::npos) {
				extern const unsigned char _start_en_us_json[] asm("_binary_en_us_json_gz_start");
				extern const unsigned char _end_en_us_json[] asm("_binary_en_us_json_gz_end");
				const size_t _size = (_end_en_us_json - _start_en_us_json);
				httpd_resp_set_type(req, "application/json");
				httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
				httpd_resp_send(req, (const char*) _start_en_us_json, _size);
			} else if (l == "es-es") {
				extern const unsigned char _start_es_es_json[] asm("_binary_es_es_json_gz_start");
				extern const unsigned char _end_es_es_json[] asm("_binary_es_es_json_gz_end");
				const size_t _size = (_end_es_es_json - _start_es_es_json);
				httpd_resp_set_type(req, "application/json");
				httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
				httpd_resp_send(req, (const char*) _start_es_es_json, _size);
			} else if (l == "ko-kr") {
				extern const unsigned char _start_ko_kr_json[] asm("_binary_ko_kr_json_gz_start");
				extern const unsigned char _end_ko_kr_json[] asm("_binary_ko_kr_json_gz_end");
				const size_t _size = (_end_ko_kr_json - _start_ko_kr_json);
				httpd_resp_set_type(req, "application/json");
				httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
				httpd_resp_send(req, (const char*) _start_ko_kr_json, _size);
			} else {
				extern const unsigned char _start_pt_br_json[] asm("_binary_pt_br_json_gz_start");
				extern const unsigned char _end_pt_br_json[] asm("_binary_pt_br_json_gz_end");
				const size_t _size = (_end_pt_br_json - _start_pt_br_json);
				httpd_resp_set_type(req, "application/json");
				httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
				httpd_resp_send(req, (const char*) _start_pt_br_json, _size);
			}

			cJSON_Delete(root);
		} else {
			/* send a 404 otherwise */
			cJSON_Delete(root);
			httpd_resp_send_404(req);
		}

		return ESP_OK;

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t settings_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/") != string::npos) {
		ESP_LOGI(TAG, "Serving page /settings");

		extern const unsigned char _start_settings_html[] asm("_binary_settings_html_gz_start");
		extern const unsigned char _end_settings_html[] asm("_binary_settings_html_gz_end");

		size_t _size = _end_settings_html - _start_settings_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_settings_html, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t reset_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/reset") != string::npos) {
		ESP_LOGI(TAG, "Serving page /reset");

		extern const unsigned char _start_reset_html[] asm("_binary_reset_html_gz_start");
		extern const unsigned char _end_reset_html[] asm("_binary_reset_html_gz_end");

		size_t _size = _end_reset_html - _start_reset_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_reset_html, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t settings_serialnumber_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/admin/serialnumber") != string::npos) {
		ESP_LOGI(TAG, "Serving page /admin/serialnumber");

		extern const unsigned char _start_serialnumber_html[] asm("_binary_serialnumber_html_gz_start");
		extern const unsigned char _end_serialnumber_html[] asm("_binary_serialnumber_html_gz_end");

		size_t _size = _end_serialnumber_html - _start_serialnumber_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_serialnumber_html, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_settings_serialnumber_get_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/settings/serialnumber") != string::npos) {
		string serial_number = get_serial_number();

		httpd_resp_set_type(req, "application/json");
		cJSON *root = cJSON_CreateObject();

		cJSON_AddStringToObject(root, "serialNumber", serial_number.c_str());

		const char *sys_info = cJSON_Print(root);
		httpd_resp_sendstr(req, sys_info);

		//printf("\nPP: %f\n\n", restrict_settings.positivePercentage);
		//printf("\nJSON: %s\n\n", sys_info);

		free((void*) sys_info);
		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_settings_get_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/settings") != string::npos) {
		device_settings_t settings = read_device_settings();

		string serialNumber = get_serial_number();
		float positive_perc = get_positive_percentage();

		char rtc_date_time[72];
		rtc_ds1302_get_date_time(rtc_date_time);

		httpd_resp_set_type(req, "application/json");
		cJSON *root = cJSON_CreateObject();

		cJSON_AddStringToObject(root, "institution",
				settings.institution.c_str());
		cJSON_AddStringToObject(root, "serialNumber", serialNumber.c_str());
		cJSON_AddNumberToObject(root, "positivePercentage", positive_perc);
		cJSON_AddStringToObject(root, "version", getFirmwareVersion());
		cJSON_AddStringToObject(root, "deviceDateTime", rtc_date_time);

		const char *sys_info = cJSON_Print(root);
		httpd_resp_sendstr(req, sys_info);

		//printf("\nPP: %f\n\n", restrict_settings.positivePercentage);
		//printf("\nJSON: %s\n\n", sys_info);

		free((void*) sys_info);
		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t restrict_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/admin/restrict") != string::npos) {
		ESP_LOGI(TAG, "Serving page /admin/restrict");

		httpd_resp_set_status(req, HTTPD_200);

		extern const unsigned char _start_restrict_html[] asm("_binary_restrict_html_gz_start");
		extern const unsigned char _end_restrict_html[] asm("_binary_restrict_html_gz_end");

		size_t _size = _end_restrict_html - _start_restrict_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

		//httpd_resp_set_hdr(req, "Connection", "keep-alive");

		httpd_resp_send(req, (const char*) _start_restrict_html, _size);

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t advanced_config_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/admin/advanced_config") != string::npos) {
		ESP_LOGI(TAG, "Serving page /admin/advanced_config");

		httpd_resp_set_status(req, HTTPD_200);

		extern const unsigned char _start_advanced_config_html[] asm("_binary_advanced_config_html_gz_start");
		extern const unsigned char _end_advanced_config_html[] asm("_binary_advanced_config_html_gz_end");

		size_t _size = _end_advanced_config_html - _start_advanced_config_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

		httpd_resp_send(req, (const char*) _start_advanced_config_html, _size);

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t calibration_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/calibration") != string::npos) {
		ESP_LOGI(TAG, "Serving page /calibration");

		httpd_resp_set_status(req, HTTPD_200);

		extern const unsigned char _start_calibration_html[] asm("_binary_calibration_html_gz_start");
		extern const unsigned char _end_calibration_html[] asm("_binary_calibration_html_gz_end");

		size_t _size = _end_calibration_html - _start_calibration_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

		//httpd_resp_set_hdr(req, "Connection", "keep-alive");

		httpd_resp_send(req, (const char*) _start_calibration_html, _size);

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t ampoules_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/admin") != string::npos) {
		ESP_LOGI(TAG, "Serving page /ampoules");

		extern const unsigned char _start_ampoules_html[] asm("_binary_ampoules_html_gz_start");
		extern const unsigned char _end_ampoules_html[] asm("_binary_ampoules_html_gz_end");

		size_t _size = _end_ampoules_html - _start_ampoules_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		httpd_resp_send(req, (const char*) _start_ampoules_html, _size);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_ampoules_get_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/api/v1/ampoules") != string::npos) {

		string json = convert_ampoules_test_to_json();

		httpd_resp_set_type(req, "application/json");
		httpd_resp_sendstr(req, json.c_str());

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_ampoules_status_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/ampoules/status") != string::npos) {

		string json = convert_ampoules_test_status_to_json();

		httpd_resp_set_type(req, "application/json");
		httpd_resp_sendstr(req, json.c_str());

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_temperatures_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/temperatures") != string::npos) {

		string json = read_temperatures_calibration_json();

		httpd_resp_set_type(req, "application/json");
		httpd_resp_sendstr(req, json.c_str());

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t restart_device_get_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/api/v1/device/restart") != string::npos) {
		restart_device(req);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static void send_test_in_progress_error(httpd_req_t *req) {
	ESP_LOGW(TAG, "Alteracao recusada: analise em andamento");

	cJSON *root = cJSON_CreateObject();
	cJSON_AddBoolToObject(root, "success", false);
	cJSON_AddStringToObject(root, "message",
			"Não é possível alterar essa configuração enquanto houver análises em andamento.");

	char *json = cJSON_Print(root);

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);
}

static esp_err_t device_settings_post_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/device/settings") != string::npos) {
		if (ampoule_any()) {
			send_test_in_progress_error(req);
			return ESP_OK;
		}

		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc(SCRATCH_BUFSIZE);

		int received = 0;
		if (total_len >= SCRATCH_BUFSIZE) {
			/* Respond with 500 Internal Server Error */
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				free(buf);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		ESP_LOGI(TAG, "%s\n", buf);

		device_settings_t settings;

		int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

		cJSON *root = cJSON_Parse(buf);
		free(buf);

		if (cJSON_GetObjectItem(root, "year")) {
			int data = (int) cJSON_GetObjectItem(root, "year")->valueint;
			year = data;
		}

		if (cJSON_GetObjectItem(root, "month")) {
			int data = (int) cJSON_GetObjectItem(root, "month")->valueint;
			month = data;
		}

		if (cJSON_GetObjectItem(root, "day")) {
			int data = (int) cJSON_GetObjectItem(root, "day")->valueint;
			day = data;
		}

		if (cJSON_GetObjectItem(root, "hour")) {
			int data = (int) cJSON_GetObjectItem(root, "hour")->valueint;
			hour = data;
		}

		if (cJSON_GetObjectItem(root, "minute")) {
			int data = (int) cJSON_GetObjectItem(root, "minute")->valueint;
			minute = data;
		}

		if (cJSON_GetObjectItem(root, "second")) {
			int data = (int) cJSON_GetObjectItem(root, "second")->valueint;
			second = data;
		}

		if (cJSON_GetObjectItem(root, "institution")) {
			char *inst = cJSON_GetObjectItem(root, "institution")->valuestring;

			settings.institution = inst;
		}

		ESP_LOGI(TAG, "Device Settings:");

		struct tm time = { .tm_sec = second, .tm_min = minute, .tm_hour = hour,
				.tm_mday = day, .tm_mon = (month -1), .tm_year = (year - 1900), .tm_wday = 0, .tm_yday = 0, .tm_isdst = -1 };

		rtc_ds1302_print_date_time(time);

		printf("\tInstitution: %s!\n", settings.institution.c_str());

		// Ajusta a data e hora para a hora recebido da app web.
		rtc_ds1302_adjust_timestamp(time);

		// Alterar aqui para salvar direto
		if (save_device_settings(settings) != ESP_OK) {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": false}");
		} else {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": true}");
		}

		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_settings_serialnumber_post_handler(httpd_req_t *req) {
	if (strcmp(req->uri, "/api/v1/restrict/serialnumber") == 0) {
		if (ampoule_any()) {
			send_test_in_progress_error(req);
			return ESP_OK;
		}

		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc(SCRATCH_BUFSIZE);

		int received = 0;
		if (total_len >= SCRATCH_BUFSIZE) {
			/* Respond with 500 Internal Server Error */
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				free(buf);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		ESP_LOGI(TAG, "%s\n", buf);

		string serialNumber = "";

		cJSON *root = cJSON_Parse(buf);
		free(buf);

		if (cJSON_GetObjectItem(root, "serialNumber")) {
			char *serial =
					cJSON_GetObjectItem(root, "serialNumber")->valuestring;

			serialNumber = serial;
		}

		ESP_LOGI(TAG, "Serial Number: %s\n", serialNumber.c_str());

		if (save_serial_number(serialNumber) != ESP_OK) {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": false}");
		} else {
			restart_device(req);
		}

		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t restrict_device_settings_post_handler(httpd_req_t *req) {
	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/api/v1/restrict/settings") != string::npos) {
		if (ampoule_any()) {
			send_test_in_progress_error(req);
			return ESP_OK;
		}

		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc(SCRATCH_BUFSIZE);

		int received = 0;
		if (total_len >= SCRATCH_BUFSIZE) {
			/* Respond with 500 Internal Server Error */
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				free(buf);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		ESP_LOGI(TAG, "%s\n", buf);

		float pp = 0.0f;

		cJSON *root = cJSON_Parse(buf);
		free(buf);

		if (cJSON_GetObjectItem(root, "positivePercentage")) {
			char *pdata =
					cJSON_GetObjectItem(root, "positivePercentage")->valuestring;

			pp = atof(pdata);
		}

		ESP_LOGI(TAG, "Positive Percentage: %.1f\n", pp);

		if (save_positive_percentage(pp) != ESP_OK) {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": false}");
		} else {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": true}");
		}

		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_advanced_config_get_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/api/v1/advanced_config") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Consulta GET /api/v1/advanced_config (crcError=%d)",
			g_advanced_config_crc_error);

	cJSON *root = cJSON_CreateObject();

	cJSON_AddNumberToObject(root, "ledCaptureTime",
			g_advanced_config.led_capture_time_s);
	cJSON_AddNumberToObject(root, "loopCycleTime",
			g_advanced_config.loop_cycle_time_s);
	cJSON_AddNumberToObject(root, "samplesInitial",
			g_advanced_config.samples_initial);
	cJSON_AddNumberToObject(root, "samplesFinal",
			g_advanced_config.samples_final);
	cJSON_AddNumberToObject(root, "earlyCheckTime",
			g_advanced_config.early_check_time_s);

	cJSON_AddNumberToObject(root, "heaterSetpoint",
			g_advanced_config.heater_setpoint_c);
	cJSON_AddNumberToObject(root, "heaterMinTemp",
			g_advanced_config.heater_min_temp_c);
	cJSON_AddNumberToObject(root, "heaterMaxTemp",
			g_advanced_config.heater_max_temp_c);
	cJSON_AddNumberToObject(root, "heaterReleaseTemp",
			g_advanced_config.heater_release_temp_c);

	const char *operation_mode_str =
			g_advanced_config.operation_mode == OPERATION_MODE_CRC1 ? "crc1" :
			g_advanced_config.operation_mode == OPERATION_MODE_ETO ? "eto" :
					"normal";
	cJSON_AddStringToObject(root, "operationMode", operation_mode_str);

	cJSON *cavities = cJSON_CreateArray();
	for (int i = 0; i < 4; i++) {
		cJSON_AddItemToArray(cavities,
				cJSON_CreateBool(g_advanced_config.cavity_enabled[i]));
	}
	cJSON_AddItemToObject(root, "cavityEnabled", cavities);

	cJSON_AddBoolToObject(root, "crcError", g_advanced_config_crc_error);

	int cavities_enabled_count = advanced_config_cavities_enabled_count();

	cJSON_AddNumberToObject(root, "minLoopCycleTime",
			advanced_config_min_loop_cycle_time(
					g_advanced_config.led_capture_time_s,
					cavities_enabled_count));

	cJSON_AddNumberToObject(root, "minEarlyCheckTime",
			advanced_config_min_early_check_time(
					g_advanced_config.samples_initial,
					g_advanced_config.samples_final,
					g_advanced_config.loop_cycle_time_s));

	char *json = cJSON_Print(root);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);

	return ESP_OK;
}

static void send_advanced_config_error(httpd_req_t *req, const char *message) {
	ESP_LOGW(TAG, "Configuracao avancada recusada: %s", message);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddBoolToObject(root, "success", false);
	cJSON_AddStringToObject(root, "message", message);

	char *json = cJSON_Print(root);

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);
}

static esp_err_t api_advanced_config_post_handler(httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/api/v1/advanced_config") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	if (ampoule_any()) {
		send_advanced_config_error(req,
				"Não é possível salvar enquanto houver análises em andamento.");
		return ESP_OK;
	}

	int total_len = req->content_len;
	int cur_len = 0;

	char *buf = (char*) malloc(SCRATCH_BUFSIZE);

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
				"content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
		if (received <= 0) {
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}

	buf[total_len] = '\0';

	ESP_LOGI(TAG, "%s\n", buf);

	cJSON *root = cJSON_Parse(buf);
	free(buf);

	if (!root) {
		send_advanced_config_error(req, "JSON inválido.");
		return ESP_OK;
	}

	advanced_config_t cfg = g_advanced_config;

	cJSON *item;

	if ((item = cJSON_GetObjectItem(root, "ledCaptureTime")))
		cfg.led_capture_time_s = (float) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "loopCycleTime")))
		cfg.loop_cycle_time_s = (uint32_t) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "samplesInitial")))
		cfg.samples_initial = (uint8_t) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "samplesFinal")))
		cfg.samples_final = (uint8_t) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "earlyCheckTime")))
		cfg.early_check_time_s = (uint32_t) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "heaterSetpoint")))
		cfg.heater_setpoint_c = (float) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "heaterMinTemp")))
		cfg.heater_min_temp_c = (float) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "heaterMaxTemp")))
		cfg.heater_max_temp_c = (float) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "heaterReleaseTemp")))
		cfg.heater_release_temp_c = (float) item->valuedouble;

	if ((item = cJSON_GetObjectItem(root, "operationMode")) && item->valuestring) {
		if (strcmp(item->valuestring, "crc1") == 0)
			cfg.operation_mode = OPERATION_MODE_CRC1;
		else if (strcmp(item->valuestring, "eto") == 0)
			cfg.operation_mode = OPERATION_MODE_ETO;
		else
			cfg.operation_mode = OPERATION_MODE_NORMAL;
	}

	cJSON *cavities = cJSON_GetObjectItem(root, "cavityEnabled");
	int cavities_enabled_count = 0;

	if (cavities && cJSON_IsArray(cavities) && cJSON_GetArraySize(cavities) == 4) {
		for (int i = 0; i < 4; i++) {
			cJSON *c = cJSON_GetArrayItem(cavities, i);
			cfg.cavity_enabled[i] = cJSON_IsTrue(c);
			if (cfg.cavity_enabled[i])
				cavities_enabled_count++;
		}
	} else {
		for (int i = 0; i < 4; i++) {
			if (cfg.cavity_enabled[i])
				cavities_enabled_count++;
		}
	}

	cJSON_Delete(root);

	// CRC1: so a cavidade 1 pode ficar habilitada - forcado no backend
	// independente do que a tela web enviou (defesa contra requisicao
	// manual/fora da tela).
	if (cfg.operation_mode == OPERATION_MODE_CRC1) {
		cfg.cavity_enabled[0] = true;
		cfg.cavity_enabled[1] = false;
		cfg.cavity_enabled[2] = false;
		cfg.cavity_enabled[3] = false;
		cavities_enabled_count = 1;
	}

	// Validacao defensiva no firmware - independente da validacao ja
	// feita na tela web, o backend nunca aceita uma combinacao fora dos
	// limites absolutos ou que viole as regras cruzadas entre itens.
	if (cfg.led_capture_time_s < 0.5f || cfg.led_capture_time_s > 7.0f) {
		send_advanced_config_error(req,
				"Tempo de captura fora da faixa permitida (0,5 a 7 segundos).");
		return ESP_OK;
	}

	if (cfg.loop_cycle_time_s < 2 || cfg.loop_cycle_time_s > 50) {
		send_advanced_config_error(req,
				"Tempo de looping fora da faixa permitida (2 a 50 segundos).");
		return ESP_OK;
	}

	if (cfg.samples_initial < 3 || cfg.samples_initial > 10
			|| cfg.samples_final < 3 || cfg.samples_final > 10) {
		send_advanced_config_error(req,
				"Quantidade de amostras fora da faixa permitida (3 a 10).");
		return ESP_OK;
	}

	// CRC1 permite reduzir o minimo absoluto de checagem antecipada de 3
	// para 1 minuto (pedido do cliente); Normal e ETO mantem o minimo de 3.
	uint32_t early_check_abs_min_s =
			cfg.operation_mode == OPERATION_MODE_CRC1 ? 60 : 180;

	if (cfg.early_check_time_s < early_check_abs_min_s
			|| cfg.early_check_time_s > 900) {
		send_advanced_config_error(req,
				cfg.operation_mode == OPERATION_MODE_CRC1 ?
						"Tempo de checagem antecipada fora da faixa permitida (1 a 15 minutos)." :
						"Tempo de checagem antecipada fora da faixa permitida (3 a 15 minutos).");
		return ESP_OK;
	}

	if (cavities_enabled_count < 1) {
		send_advanced_config_error(req,
				"É necessário manter ao menos uma cavidade ativa.");
		return ESP_OK;
	}

	uint32_t min_loop = advanced_config_min_loop_cycle_time(
			cfg.led_capture_time_s, cavities_enabled_count);

	if (cfg.loop_cycle_time_s < min_loop) {
		send_advanced_config_error(req,
				"Tempo de looping menor do que o mínimo permitido para o tempo de captura e a quantidade de cavidades ativas configurados.");
		return ESP_OK;
	}

	uint32_t min_early_check = advanced_config_min_early_check_time(
			cfg.samples_initial, cfg.samples_final, cfg.loop_cycle_time_s);

	if (cfg.early_check_time_s < min_early_check) {
		send_advanced_config_error(req,
				"Tempo de checagem antecipada menor do que o mínimo permitido para a quantidade de amostras e o tempo de looping configurados.");
		return ESP_OK;
	}

	// Item 6 (temperatura) - faixa absoluta de engenharia (20 a 70 graus),
	// provisoria ate confirmacao de quem valida o metodo biologico/clinico -
	// serve so para barrar valores absurdos (negativos, centenas de graus),
	// nao e uma faixa clinicamente validada.
	const float HEATER_TEMP_ABS_MIN = 20.0f;
	const float HEATER_TEMP_ABS_MAX = 70.0f;

	if (cfg.heater_setpoint_c < HEATER_TEMP_ABS_MIN
			|| cfg.heater_setpoint_c > HEATER_TEMP_ABS_MAX
			|| cfg.heater_min_temp_c < HEATER_TEMP_ABS_MIN
			|| cfg.heater_min_temp_c > HEATER_TEMP_ABS_MAX
			|| cfg.heater_max_temp_c < HEATER_TEMP_ABS_MIN
			|| cfg.heater_max_temp_c > HEATER_TEMP_ABS_MAX
			|| cfg.heater_release_temp_c < HEATER_TEMP_ABS_MIN
			|| cfg.heater_release_temp_c > HEATER_TEMP_ABS_MAX) {
		send_advanced_config_error(req,
				"Temperatura fora da faixa absoluta permitida (20 a 60 graus).");
		return ESP_OK;
	}

	// Regra cruzada: minimo e maximo precisam manter uma margem de pelo
	// menos 4 graus para cada lado do setpoint - evita uma janela apertada
	// demais que gere alarme/cancelamento de teste pela oscilacao normal
	// do aquecedor.
	if (cfg.heater_setpoint_c - cfg.heater_min_temp_c < 4.0f) {
		send_advanced_config_error(req,
				"Temperatura mínima muito próxima do setpoint (mínimo de 4 graus de margem).");
		return ESP_OK;
	}

	if (cfg.heater_max_temp_c - cfg.heater_setpoint_c < 4.0f) {
		send_advanced_config_error(req,
				"Temperatura máxima muito próxima do setpoint (mínimo de 4 graus de margem).");
		return ESP_OK;
	}

	// Regra cruzada: liberacao (piso da faixa "estabilizada") = minimo + 2.
	if (fabsf(
			cfg.heater_release_temp_c - (cfg.heater_min_temp_c + 2.0f))
			> 0.01f) {
		send_advanced_config_error(req,
				"Temperatura de liberação precisa ser exatamente 2 graus acima da temperatura mínima.");
		return ESP_OK;
	}

	if (advanced_config_save(cfg) != ESP_OK) {
		send_advanced_config_error(req,
				"Erro ao gravar a configuração (falha de integridade na gravação).");
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Configuracao avancada salva com sucesso via POST /api/v1/advanced_config");

	ampoule_apply_cavity_enabled_config();

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_sendstr(req, "{\"success\": true}");

	return ESP_OK;
}

static esp_err_t api_advanced_config_restore_defaults_post_handler(
		httpd_req_t *req) {

	if (check_basic_auth(req) != ESP_OK) {
		return ESP_FAIL;
	}

	string uri = req->uri;

	if (uri.find("/api/v1/advanced_config/restore_defaults")
			== string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	if (ampoule_any()) {
		send_advanced_config_error(req,
				"Não é possível restaurar o padrão de fábrica enquanto houver análises em andamento.");
		return ESP_OK;
	}

	ESP_LOGW(TAG,
			"Restauracao de padrao de fabrica solicitada via web (POST /api/v1/advanced_config/restore_defaults)");

	if (advanced_config_restore_defaults() != ESP_OK) {
		send_advanced_config_error(req,
				"Erro ao restaurar o padrão de fábrica. Tente novamente.");
		return ESP_OK;
	}

	ampoule_apply_cavity_enabled_config();

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_sendstr(req, "{\"success\": true}");

	return ESP_OK;
}

static esp_err_t history_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/history") != string::npos) {
		ESP_LOGI(TAG, "Serving page /history");

		httpd_resp_set_status(req, HTTPD_200);

		extern const unsigned char _start_history_html[] asm("_binary_history_html_gz_start");
		extern const unsigned char _end_history_html[] asm("_binary_history_html_gz_end");

		size_t _size = _end_history_html - _start_history_html;
		httpd_resp_set_type(req, "text/html");
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

		httpd_resp_send(req, (const char*) _start_history_html, _size);

	} else {
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static void send_history_error(httpd_req_t *req, const char *message) {
	ESP_LOGW(TAG, "Historico/impressao recusado: %s", message);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddBoolToObject(root, "success", false);
	cJSON_AddStringToObject(root, "message", message);

	char *json = cJSON_Print(root);

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);
}

static esp_err_t api_history_config_get_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/history_config") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	cJSON *root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "printCount", get_print_count());
	cJSON_AddNumberToObject(root, "maxRecords", AMPOULE_HISTORY_MAX_RECORDS);
	cJSON_AddNumberToObject(root, "totalRecords", ampoule_history_get_count());

	char *json = cJSON_Print(root);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);

	return ESP_OK;
}

static esp_err_t api_history_config_post_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/history_config") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	if (ampoule_any()) {
		send_history_error(req,
				"Não é possível alterar essa configuração enquanto houver análises em andamento.");
		return ESP_OK;
	}

	int total_len = req->content_len;
	int cur_len = 0;

	char *buf = (char*) malloc(SCRATCH_BUFSIZE);

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
				"content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
		if (received <= 0) {
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}

	buf[total_len] = '\0';

	cJSON *root = cJSON_Parse(buf);
	free(buf);

	if (!root) {
		send_history_error(req, "JSON inválido.");
		return ESP_OK;
	}

	cJSON *item = cJSON_GetObjectItem(root, "printCount");

	if (!item) {
		cJSON_Delete(root);
		send_history_error(req, "Campo printCount ausente.");
		return ESP_OK;
	}

	int print_count = item->valueint;

	cJSON_Delete(root);

	if (print_count < 1 || print_count > AMPOULE_HISTORY_MAX_RECORDS) {
		send_history_error(req,
				"Quantidade a imprimir fora da faixa permitida (1 a 64).");
		return ESP_OK;
	}

	if (save_print_count((uint8_t) print_count) != ESP_OK) {
		send_history_error(req, "Erro ao gravar a configuração. Tente novamente.");
		return ESP_OK;
	}

	ESP_LOGI(TAG, "printCount salvo: %d", print_count);

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_sendstr(req, "{\"success\": true}");

	return ESP_OK;
}

static esp_err_t api_buzzer_config_get_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/buzzer_config") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	cJSON *root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "buzzerAlertTimeoutMin",
			get_buzzer_alert_timeout_min());

	char *json = cJSON_Print(root);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);

	return ESP_OK;
}

static esp_err_t api_buzzer_config_post_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/buzzer_config") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	if (ampoule_any()) {
		send_history_error(req,
				"Não é possível alterar essa configuração enquanto houver análises em andamento.");
		return ESP_OK;
	}

	int total_len = req->content_len;
	int cur_len = 0;

	char *buf = (char*) malloc(SCRATCH_BUFSIZE);

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
				"content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
		if (received <= 0) {
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}

	buf[total_len] = '\0';

	cJSON *root = cJSON_Parse(buf);
	free(buf);

	if (!root) {
		send_history_error(req, "JSON inválido.");
		return ESP_OK;
	}

	cJSON *item = cJSON_GetObjectItem(root, "buzzerAlertTimeoutMin");

	if (!item) {
		cJSON_Delete(root);
		send_history_error(req, "Campo buzzerAlertTimeoutMin ausente.");
		return ESP_OK;
	}

	int minutes = item->valueint;

	cJSON_Delete(root);

	if (minutes < 1 || minutes > 30) {
		send_history_error(req,
				"Tempo fora da faixa permitida (1 a 30 minutos).");
		return ESP_OK;
	}

	if (save_buzzer_alert_timeout_min((uint8_t) minutes) != ESP_OK) {
		send_history_error(req,
				"Erro ao gravar a configuração. Tente novamente.");
		return ESP_OK;
	}

	ESP_LOGI(TAG, "buzzerAlertTimeoutMin salvo: %d", minutes);

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_sendstr(req, "{\"success\": true}");

	return ESP_OK;
}

static esp_err_t api_history_get_handler(httpd_req_t *req) {

	string uri = req->uri;

	if (uri.find("/api/v1/history") == string::npos) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	int offset = 0;

	char query[32];
	if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
		char offset_str[16];
		if (httpd_query_key_value(query, "offset", offset_str,
				sizeof(offset_str)) == ESP_OK) {
			offset = atoi(offset_str);
		}
	}

	if (offset < 0)
		offset = 0;

	cJSON *root = cJSON_CreateObject();

	int total = ampoule_history_get_count();
	cJSON_AddNumberToObject(root, "total", total);
	cJSON_AddNumberToObject(root, "offset", offset);

	cJSON *records = cJSON_CreateArray();

	for (int i = offset; i < offset + 8 && i < total; i++) {
		ampoule_history_record_t rec;

		if (!ampoule_history_get_record(i, rec))
			continue;

		cJSON *item = cJSON_CreateObject();

		cJSON_AddNumberToObject(item, "idTest", rec.id_test);
		cJSON_AddNumberToObject(item, "cavidade", rec.cavidade);
		cJSON_AddNumberToObject(item, "cicloMinutos", rec.ciclo_minutos);
		cJSON_AddNumberToObject(item, "tsInicio", rec.ts_inicio);
		cJSON_AddNumberToObject(item, "tsFim", rec.ts_fim);
		cJSON_AddNumberToObject(item, "temperatura", rec.temperatura);

		const char *resultado_str =
				rec.resultado == AMPOULE_RESULT_POSITIVE ? "P" :
				rec.resultado == AMPOULE_RESULT_CANCELLED ? "C" : "N";
		cJSON_AddStringToObject(item, "resultado", resultado_str);

		cJSON_AddItemToArray(records, item);
	}

	cJSON_AddItemToObject(root, "records", records);

	char *json = cJSON_Print(root);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, json);

	free(json);
	cJSON_Delete(root);

	return ESP_OK;
}

static esp_err_t calibration_post_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/device/calibration") != string::npos) {

		if (ampoule_any()) {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req,
					"{\"success\": false, \"ampoules_in_test\": true, \"message\": \"Existem análises em andamento.\"}");

			return ESP_OK;
		}

		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc((SCRATCH_BUFSIZE * 2));

		int received = 0;
		if (total_len >= (SCRATCH_BUFSIZE * 2)) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		ESP_LOGI(TAG, "%s\n", buf);

		float factor = 0.0f;

		cJSON *root = cJSON_Parse(buf);

		if (cJSON_GetObjectItem(root, "factor")) {
			string factor_temp =
					cJSON_GetObjectItem(root, "factor")->valuestring;

			printf("%s\n", factor_temp.c_str());

			float f = atof(factor_temp.c_str());
			factor = f; /// 10.0f;
		}

		ESP_LOGI(TAG, "Calibration Factor: %.2f", factor);
		//ESP_LOGI(TAG, "%ld", temp);

		if (save_calibration(factor) == ESP_OK) {

			refresh_calibration_factor();

			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": true}");
		} else {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": false}");
		}

		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t reset_post_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/reset") != string::npos) {

		if (ampoule_any()) {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req,
					"{\"success\": false, \"ampoules_in_test\": true, \"message\": \"Existem análises em andamento.\"}");

			return ESP_OK;
		}

		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc((SCRATCH_BUFSIZE * 2));

		int received = 0;
		if (total_len >= (SCRATCH_BUFSIZE * 2)) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		if (check_password(req) == ESP_OK) {

			esp_err_t err = reset_user_data();

			clear_histories();

			if (err == ESP_OK) {
				httpd_resp_set_status(req, "200 OK");
				httpd_resp_sendstr(req, "{\"success\": true}");
			} else {
				httpd_resp_set_status(req, "200 OK");
				httpd_resp_sendstr(req, "{\"success\": false}");
			}

			ESP_LOGI(TAG, "RESTART_ID=3 - Restarting now (HTTP reset_user_data).\n");
			fflush(stdout);
			vTaskDelay(pdMS_TO_TICKS(100));
			esp_restart();
		}

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_language_post_handler(httpd_req_t *req) {
	if (strcmp(req->uri, "/api/v1/language") == 0) {
		if (ampoule_any()) {
			send_test_in_progress_error(req);
			return ESP_OK;
		}

		int total_len = req->content_len;
		int cur_len = 0;

		char *buf = (char*) malloc(SCRATCH_BUFSIZE);

		int received = 0;
		if (total_len >= SCRATCH_BUFSIZE) {
			/* Respond with 500 Internal Server Error */
			free(buf);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
					"content too long");
			return ESP_FAIL;
		}
		while (cur_len < total_len) {
			received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
			if (received <= 0) {
				/* Respond with 500 Internal Server Error */
				free(buf);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
						"Failed to post control value");
				return ESP_FAIL;
			}
			cur_len += received;
		}

		buf[total_len] = '\0';

		ESP_LOGI(TAG, "%s\n", buf);

		string language = "";

		cJSON *root = cJSON_Parse(buf);
		free(buf);

		if (cJSON_GetObjectItem(root, "language")) {
			char *l = cJSON_GetObjectItem(root, "language")->valuestring;

			language = l;
		}

		ESP_LOGI(TAG, "Language: %s\n", language.c_str());

		if (save_language(language) != ESP_OK) {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": false}");
		} else {
			httpd_resp_set_status(req, "200 OK");
			httpd_resp_sendstr(req, "{\"success\": true}");
		}

		cJSON_Delete(root);
	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

static esp_err_t api_language_get_handler(httpd_req_t *req) {
	string uri = req->uri;

	if (uri.find("/api/v1/language") != string::npos) {

		string l = get_language();

		httpd_resp_set_type(req, "application/json");
		cJSON *root = cJSON_CreateObject();

		cJSON_AddStringToObject(root, "language", l.c_str());

		const char *sys_info = cJSON_Print(root);
		httpd_resp_sendstr(req, sys_info);

		free((void*) sys_info);
		cJSON_Delete(root);

	} else {
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

// resources


static esp_err_t logo_get_handler(httpd_req_t *req) {
    string uri = req->uri;

    if (uri.find("/resources/logo.png") != string::npos) {
        const branding_entry_t* b = branding_get(LOGO);
        const size_t _size = (size_t)(b->end - b->start);
        httpd_resp_set_type(req, "image/png");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, (const char*) b->start, _size);
    } else {
        httpd_resp_send_404(req);
    }

    return ESP_OK;
}

static const httpd_uri_t logo_get_uri = { .uri = "/resources/logo.png",
		.method = HTTP_GET, .handler = logo_get_handler, .user_ctx = NULL };

// end resources

static const httpd_uri_t bootstrap_css_get_uri = { .uri =
		"/bootstrap/css/bootstrap.min.css", .method = HTTP_GET, .handler =
		bootstrap_css_get_handler, .user_ctx = NULL };

static const httpd_uri_t bootstrap_toaster_css_get_uri = { .uri =
		"/bootstrap/css/bootstrap-toaster.min.css", .method = HTTP_GET,
		.handler = bootstrap_toaster_css_get_handler, .user_ctx = NULL };

static const httpd_uri_t bootstrap_icons_css_get_uri = { .uri =
		"/bootstrap/css/bootstrap-icons.css", .method = HTTP_GET, .handler =
		bootstrap_icons_css_get_handler, .user_ctx = NULL };

static const httpd_uri_t bootstrap_icons_fonts_woff_get_uri =
		{
				.uri =
						"/bootstrap/css/fonts/bootstrap-icons.woff?a97b3594ad416896e15824f6787370e0",
				.method = HTTP_GET, .handler =
						bootstrap_icons_fonts_woff_get_handler, .user_ctx =
				NULL };

static const httpd_uri_t bootstrap_icons_fonts_woff2_get_uri =
		{
				.uri =
						"/bootstrap/css/fonts/bootstrap-icons.woff2?a97b3594ad416896e15824f6787370e0",
				.method = HTTP_GET, .handler =
						bootstrap_icons_fonts_woff2_get_handler, .user_ctx =
				NULL };

static const httpd_uri_t bootstrap_js_get_uri = { .uri =
		"/bootstrap/js/bootstrap.min.js", .method = HTTP_GET, .handler =
		bootstrap_js_get_handler, .user_ctx = NULL };

static const httpd_uri_t bootstrap_toaster_js_get_uri = { .uri =
		"/bootstrap/js/bootstrap-toaster.min.js", .method = HTTP_GET, .handler =
		bootstrap_toaster_js_get_handler, .user_ctx = NULL };

static const httpd_uri_t bootstrap_datetimepicker_css_get_uri = { .uri =
		"/bootstrap_datetimepicker/bootstrap.datetimepicker.min.css", .method =
		HTTP_GET, .handler = bootstrap_datetimepicker_css_get_handler,
		.user_ctx = NULL };

static const httpd_uri_t bootstrap_datetimepicker_js_get_uri = { .uri =
		"/bootstrap_datetimepicker/bootstrap.datetimepicker.min.js", .method =
		HTTP_GET, .handler = bootstrap_datetimepicker_js_get_handler,
		.user_ctx = NULL };

static const httpd_uri_t jquery_js_get_uri = { .uri = "/jquery/jquery.min.js",
		.method = HTTP_GET, .handler = jquery_js_get_handler, .user_ctx = NULL };

static const httpd_uri_t moment_locale_get_uri = { .uri =
		"/moment/moment-with-locales.js", .method = HTTP_GET, .handler =
		moment_locale_get_handler, .user_ctx = NULL };

static const httpd_uri_t moment_get_uri = { .uri = "/moment/moment.js",
		.method = HTTP_GET, .handler = moment_get_handler, .user_ctx = NULL };

static const httpd_uri_t chart_js_get_uri = { .uri = "/chartjs/chart.min.js",
		.method = HTTP_GET, .handler = chart_js_get_handler, .user_ctx = NULL };

static const httpd_uri_t custom_js_get_uri = { .uri = "/custom/custom.js",
		.method = HTTP_GET, .handler = custom_js_get_handler, .user_ctx = NULL };

static const httpd_uri_t restart_device_get_uri = { .uri =
		"/api/v1/device/restart", .method = HTTP_GET, .handler =
		restart_device_get_handler, .user_ctx = NULL };

static const httpd_uri_t index_get_uri = { .uri = "/admin", .method = HTTP_GET,
		.handler = ampoules_handler, .user_ctx = NULL };

static const httpd_uri_t restrict_get_uri = { .uri = "/admin/restrict",
		.method = HTTP_GET, .handler = restrict_handler, .user_ctx = NULL };

static const httpd_uri_t advanced_config_get_uri = { .uri =
		"/admin/advanced_config", .method = HTTP_GET, .handler =
		advanced_config_handler, .user_ctx = NULL };

static const httpd_uri_t api_advanced_config_get_uri = { .uri =
		"/api/v1/advanced_config", .method = HTTP_GET, .handler =
		api_advanced_config_get_handler, .user_ctx = NULL };

static const httpd_uri_t api_advanced_config_post_uri = { .uri =
		"/api/v1/advanced_config", .method = HTTP_POST, .handler =
		api_advanced_config_post_handler, .user_ctx = NULL };

static const httpd_uri_t api_advanced_config_restore_defaults_post_uri = {
		.uri = "/api/v1/advanced_config/restore_defaults", .method =
		HTTP_POST, .handler = api_advanced_config_restore_defaults_post_handler,
		.user_ctx = NULL };

static const httpd_uri_t history_get_uri = { .uri = "/history",
		.method = HTTP_GET, .handler = history_handler, .user_ctx = NULL };

static const httpd_uri_t api_history_config_get_uri = { .uri =
		"/api/v1/history_config", .method = HTTP_GET, .handler =
		api_history_config_get_handler, .user_ctx = NULL };

static const httpd_uri_t api_history_config_post_uri = { .uri =
		"/api/v1/history_config", .method = HTTP_POST, .handler =
		api_history_config_post_handler, .user_ctx = NULL };

static const httpd_uri_t api_buzzer_config_get_uri = { .uri =
		"/api/v1/buzzer_config", .method = HTTP_GET, .handler =
		api_buzzer_config_get_handler, .user_ctx = NULL };

static const httpd_uri_t api_buzzer_config_post_uri = { .uri =
		"/api/v1/buzzer_config", .method = HTTP_POST, .handler =
		api_buzzer_config_post_handler, .user_ctx = NULL };

static const httpd_uri_t api_history_get_uri = { .uri = "/api/v1/history",
		.method = HTTP_GET, .handler = api_history_get_handler, .user_ctx =
		NULL };

static const httpd_uri_t calibration_get_uri = { .uri = "/calibration",
		.method = HTTP_GET, .handler = calibration_handler, .user_ctx = NULL };

static const httpd_uri_t settings_get_uri = { .uri = "/", .method = HTTP_GET,
		.handler = settings_handler, .user_ctx = NULL };

//static const httpd_uri_t ampoules_get_uri = { .uri = "/ampoules", .method =
//		HTTP_GET, .handler = ampoules_handler, .user_ctx = NULL };

static const httpd_uri_t api_settings_get_uri = { .uri = "/api/v1/settings",
		.method = HTTP_GET, .handler = api_settings_get_handler, .user_ctx =
		NULL };

static const httpd_uri_t api_ampoules_get_uri = { .uri = "/api/v1/ampoules",
		.method = HTTP_GET, .handler = api_ampoules_get_handler, .user_ctx =
		NULL };

static const httpd_uri_t api_ampoules_status_get_uri = { .uri =
		"/api/v1/ampoules/status", .method = HTTP_GET, .handler =
		api_ampoules_status_get_handler, .user_ctx =
NULL };

static const httpd_uri_t api_temperatures_get_uri = { .uri =
		"/api/v1/temperatures", .method = HTTP_GET, .handler =
		api_temperatures_get_handler, .user_ctx = NULL };

static const httpd_uri_t calibration_post_uri = { .uri =
		"/api/v1/device/calibration", .method = HTTP_POST, .handler =
		calibration_post_handler, .user_ctx = NULL };

static const httpd_uri_t device_settings_post_uri = { .uri =
		"/api/v1/device/settings", .method = HTTP_POST, .handler =
		device_settings_post_handler, .user_ctx = NULL };

static const httpd_uri_t restrict_device_settings_post_uri = { .uri =
		"/api/v1/restrict/settings", .method = HTTP_POST, .handler =
		restrict_device_settings_post_handler, .user_ctx = NULL };

static const httpd_uri_t serialnumber_get_uri = { .uri = "/admin/serialnumber",
		.method = HTTP_GET, .handler = settings_serialnumber_handler,
		.user_ctx = NULL };

static const httpd_uri_t api_serialnumber_get_uri = { .uri =
		"/api/v1/settings/serialnumber", .method = HTTP_GET, .handler =
		api_settings_serialnumber_get_handler, .user_ctx = NULL };

static const httpd_uri_t api_serialnumber_post_uri = { .uri =
		"/api/v1/restrict/serialnumber", .method = HTTP_POST, .handler =
		api_settings_serialnumber_post_handler, .user_ctx = NULL };

static const httpd_uri_t reset_post_uri = { .uri = "/api/v1/reset", .method =
		HTTP_POST, .handler = reset_post_handler, .user_ctx =
NULL };

static const httpd_uri_t reset_uri = { .uri = "/reset", .method = HTTP_GET,
		.handler = reset_handler, .user_ctx =
		NULL };

static const httpd_uri_t jquery_multiLanguage_js_get_uri = { .uri =
		"/jquerymultilanguage/jquery.multilanguage.min.js", .method = HTTP_GET,
		.handler = jquery_multiLanguage_js_get_handler, .user_ctx = NULL };

static const httpd_uri_t translate_json_post_uri = { .uri = "/api/v1/translate",
		.method = HTTP_POST, .handler = translate_json_post_handler, .user_ctx =
		NULL };

static const httpd_uri_t language_get_uri = { .uri = "/api/v1/language",
		.method = HTTP_GET, .handler = api_language_get_handler, .user_ctx =
		NULL };

static const httpd_uri_t language_post_uri = { .uri = "/api/v1/language",
		.method = HTTP_POST, .handler = api_language_post_handler, .user_ctx =
		NULL };

//static const httpd_uri_t api_settings_get_uri = { .uri = "/api/v1/settings",
//		.method = HTTP_GET, .handler = api_settings_get_handler, .user_ctx =
//		NULL };

//static const httpd_uri_t _uri = {
//    .uri       = ,
//    .method    = HTTP_GET,
//    .handler   = ,
//    .user_ctx  = NULL
//};

static void httpd_register_basic_auth(httpd_handle_t httpd_handle,
		httpd_uri_t uri_handler) {
	basic_auth_info_t *basic_auth_info = (basic_auth_info_t*) calloc(1,
			sizeof(basic_auth_info_t));

	if (basic_auth_info) {

//		string user_name = "maxximed";
//		string password = getPassword();

		string user_name = getUserMaster();
		string password = getPasswordMaster();

		basic_auth_info->username = const_cast<char*>(user_name.c_str());
		basic_auth_info->password = const_cast<char*>(password.c_str());

		uri_handler.user_ctx = basic_auth_info;

		httpd_register_uri_handler(httpd_handle, &uri_handler);
	}
}

void app_httpd_register_uri(httpd_handle_t *httpd_handle) {

	httpd_register_uri_handler(httpd_handle, &logo_get_uri);
	httpd_register_uri_handler(httpd_handle, &bootstrap_css_get_uri);
	httpd_register_uri_handler(httpd_handle, &bootstrap_toaster_css_get_uri);
	httpd_register_uri_handler(httpd_handle, &bootstrap_icons_css_get_uri);

	httpd_register_uri_handler(httpd_handle,
			&bootstrap_icons_fonts_woff_get_uri);
	httpd_register_uri_handler(httpd_handle,
			&bootstrap_icons_fonts_woff2_get_uri);

	httpd_register_uri_handler(httpd_handle, &bootstrap_js_get_uri);
	httpd_register_uri_handler(httpd_handle, &bootstrap_toaster_js_get_uri);
	httpd_register_uri_handler(httpd_handle,
			&bootstrap_datetimepicker_css_get_uri);
	httpd_register_uri_handler(httpd_handle,
			&bootstrap_datetimepicker_js_get_uri);
	httpd_register_uri_handler(httpd_handle, &jquery_js_get_uri);
	httpd_register_uri_handler(httpd_handle, &moment_locale_get_uri);
	httpd_register_uri_handler(httpd_handle, &moment_get_uri);
	httpd_register_uri_handler(httpd_handle, &chart_js_get_uri);
	httpd_register_uri_handler(httpd_handle, &custom_js_get_uri);

	// Rotas sem restri��o
	httpd_register_uri_handler(httpd_handle, &calibration_get_uri);
	httpd_register_uri_handler(httpd_handle, &settings_get_uri);
	httpd_register_uri_handler(httpd_handle, &api_settings_get_uri);
	httpd_register_uri_handler(httpd_handle, &api_temperatures_get_uri);
	httpd_register_uri_handler(httpd_handle, &calibration_post_uri);
	httpd_register_uri_handler(httpd_handle, &device_settings_post_uri);
	//httpd_register_uri_handler(httpd_handle, &api_settings_get_uri);

	httpd_register_uri_handler(httpd_handle, &reset_post_uri);
	httpd_register_uri_handler(httpd_handle, &reset_uri);

	httpd_register_uri_handler(httpd_handle, &jquery_multiLanguage_js_get_uri);
	httpd_register_uri_handler(httpd_handle, &translate_json_post_uri);

	httpd_register_uri_handler(httpd_handle, &language_get_uri);
	httpd_register_uri_handler(httpd_handle, &language_post_uri);

	httpd_register_uri_handler(httpd_handle, &history_get_uri);
	httpd_register_uri_handler(httpd_handle, &api_history_config_get_uri);
	httpd_register_uri_handler(httpd_handle, &api_history_config_post_uri);
	httpd_register_uri_handler(httpd_handle, &api_buzzer_config_get_uri);
	httpd_register_uri_handler(httpd_handle, &api_buzzer_config_post_uri);
	httpd_register_uri_handler(httpd_handle, &api_history_get_uri);

	// Rotas protegidas
	httpd_register_basic_auth(httpd_handle, restart_device_get_uri);
	httpd_register_basic_auth(httpd_handle, index_get_uri);
	httpd_register_basic_auth(httpd_handle, restrict_get_uri);
	httpd_register_basic_auth(httpd_handle, api_ampoules_get_uri);
	httpd_register_basic_auth(httpd_handle, api_ampoules_status_get_uri);
	httpd_register_basic_auth(httpd_handle, restrict_device_settings_post_uri);

	httpd_register_basic_auth(httpd_handle, advanced_config_get_uri);
	httpd_register_basic_auth(httpd_handle, api_advanced_config_get_uri);
	httpd_register_basic_auth(httpd_handle, api_advanced_config_post_uri);
	httpd_register_basic_auth(httpd_handle,
			api_advanced_config_restore_defaults_post_uri);

	httpd_register_basic_auth(httpd_handle, serialnumber_get_uri);
	httpd_register_basic_auth(httpd_handle, api_serialnumber_get_uri);
	httpd_register_basic_auth(httpd_handle, api_serialnumber_post_uri);
}
