#include "config.h"
#include "include/wifi_ap.h"
#include "include/led_panel.h"
#include "esp_system.h"
#include "esp_mac.h"

#define WIFI_AP_SSID      		   CONFIG_DEFAULT_AP_SSID
//#define WIFI_AP_PASS     		   CONFIG_DEFAULT_AP_PASSWORD
#define WIFI_AP_CHANNEL  		   CONFIG_DEFAULT_AP_CHANNEL

#define DEFAULT_AP_IP			   CONFIG_DEFAULT_AP_IP
#define DEFAULT_AP_GATEWAY		   CONFIG_DEFAULT_AP_GATEWAY
#define DEFAULT_AP_NETMASK		   CONFIG_DEFAULT_AP_NETMASK
#define DEFAULT_AP_MAX_CONNECTIONS CONFIG_DEFAULT_AP_MAX_CONNECTIONS
#define DEFAULT_AP_BEACON_INTERVAL CONFIG_DEFAULT_AP_BEACON_INTERVAL

const char *wifi_password;

static const char *TAG = "WIFI SOFTAP";

/* @brief netif object for the ACCESS POINT */
static esp_netif_t *esp_netif_ap = NULL;

char* get_mac_address(bool formated) {
	uint8_t baseMac[6];
	esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
	char baseMacChr[18] = { 0 };

	if (formated) {
		sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0],
				baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
	} else {
		sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1],
				baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
	}

	char *s_ptr = baseMacChr;

	ESP_LOGI(TAG, "%s", s_ptr);

	return s_ptr;
}

char* get_base_mac_address() {
	uint8_t baseMac[6];
	esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
	char baseMacChr[18] = { 0 };

	sprintf(baseMacChr, "%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);

	char *s_ptr = baseMacChr;

	ESP_LOGI(TAG, "%s", s_ptr);

	return s_ptr;
}

char* get_accesspoint_name(const char *ap) {
	char *base_mac = get_base_mac_address();

	char ap_name[100] = { };

	strcat(ap_name, ap);
	strcat(ap_name, "-");
	strcat(ap_name, base_mac);

	char *s_ptr = ap_name;

	ESP_LOGI(TAG, "Access Point : %s", s_ptr);

	return s_ptr;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data) {
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t *event =
				(wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac),
				event->aid);

		// Ligar o Led...
		wifi_led(false);

	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t *event =
				(wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac),
				event->aid);

		// Deligar o Led...
		wifi_led(true);
	}
}

void wifi_init_softap(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_ap = esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

	const char *wifi_ssid = get_accesspoint_name(WIFI_AP_SSID);

	wifi_config_t wifi_config = { .ap = { .ssid = WIFI_AP_SSID, .channel =
			WIFI_AP_CHANNEL, .max_connection = DEFAULT_AP_MAX_CONNECTIONS,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK }, };

	memcpy(wifi_config.ap.ssid, wifi_ssid, 32);


	if (strlen(wifi_password) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}else{
		memcpy(wifi_config.ap.password, wifi_password, 64);
	}

	esp_netif_dhcps_stop(esp_netif_ap); /* DHCP client/server must be stopped before setting new IP information. */
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	inet_pton(AF_INET, DEFAULT_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, DEFAULT_AP_NETMASK, &ap_ip_info.netmask);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

//	ESP_ERROR_CHECK(
//			esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_config.ap_bandwidth));
//	ESP_ERROR_CHECK(esp_wifi_set_ps(wifi_config.sta_power_save));

	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
			WIFI_AP_SSID, wifi_password, WIFI_AP_CHANNEL);
}

void wifi_ap_main(const char *password) {
	ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

	wifi_password = password;

	wifi_init_softap();
}
