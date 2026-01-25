#include "WiFiHelper.h"

WiFiHelper::WiFiHelper() {
	wifi_check_timer = 0;
	wiFiOk = false;
	wifi_last_status = WL_NO_SHIELD;
}

void WiFiHelper::begin(const char *ssid,const char *key, std::function<void(void)> scallback) {
	setup_callback = scallback;

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, key);
}

void WiFiHelper::loop(unsigned long current_time) {

	if ((current_time - wifi_check_timer) < WIFI_CHECK_INTERVAL_MS) {
		// not the time yet
		return;
	}

	wifi_check_timer = current_time;

	wl_status_t status = WiFi.status();

	// inform only if wifi status changes
	if (status != wifi_last_status) {
		Serial.println("WIFI Status: " + String(status));
	}

	// Store last wifi status
	wifi_last_status = status;

	if (wiFiOk) {
		// setup complete exit
		return;
	}

	// kick off the setup if this is not complete and WIFI just connected
	if (status == WL_CONNECTED) {
		setup_callback();
		wiFiOk = true;
	}

}

bool WiFiHelper::configureStaticIp(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet) {

	IPAddress localIP(first_octet, second_octet, third_octet, fourth_octet);
	IPAddress gateway(192, 168, 100, 1);
	IPAddress subnet(255, 255, 255, 0);
	IPAddress primaryDNS(1, 1, 1, 1);
	IPAddress secondaryDNS(8, 8, 8, 8);

	return WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS);
}

