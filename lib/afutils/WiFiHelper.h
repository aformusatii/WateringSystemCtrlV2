
#ifndef _WiFiHelper_H_
#define _WiFiHelper_H_

#include <WiFiUDP.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiScan.h>

#define WIFI_CHECK_INTERVAL_MS    100

class WiFiHelper {

	private:
		std::function<void(void)> setup_callback;
		unsigned long wifi_check_timer;
		wl_status_t wifi_last_status;

	public:
		WiFiHelper(void);

		void begin(const char *ssid,const char *key, std::function<void(void)> scallback);
		void begin(const char *ssid,const char *key, const uint8_t*, std::function<void(void)> scallback);
		bool configureStaticIp(uint8_t, uint8_t, uint8_t, uint8_t);
		void loop(unsigned long);

		bool wiFiOk;

};

#endif /* _WiFiHelper_H_ */
