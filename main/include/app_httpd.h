#ifndef _APP_HTTPD_H_
#define _APP_HTTPD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "app_httpd.h"

#define MDNS_HOST_NAME "lummina4-app"
#define MDNS_INSTANCE "lummina-4 app web server"

#define SCRATCH_BUFSIZE (10240)

#define HTTPD_401      "401 UNAUTHORIZED"           /*!< HTTP Response 401 */


void app_httpd_register_uri(httpd_handle_t *httpd_handle);

#ifdef __cplusplus
}
#endif

#endif /* _APP_HTTPD_H_ */
