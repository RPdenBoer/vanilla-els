#pragma once

#include <stdbool.h>
#include <stdint.h>

class OtaMotion {
public:
	static void start();
	static void handle();
	static bool isActive();
	static bool isWifiConnected();

private:
	static bool active;
	static bool ota_ready;
	static bool wifi_started;
	static uint32_t wifi_start_ms;
	static bool wifi_connected;
	static void beginWifi();
	static void beginOta();
};
