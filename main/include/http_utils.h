
#ifndef _HTTP_UTILS_H_
#define _HTTP_UTILS_H_

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "esp_http_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t _http_event_handler(esp_http_client_event_t *evt);

#ifdef __cplusplus
}
#endif

#endif
