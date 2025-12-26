#pragma once

#include <stdbool.h>
#include <stdint.h>

class OtaProxy {
public:
	static void start();
	static void requestReboot();
	static void handle();
	static bool isActive();
	static bool shouldRequestReboot();
	static void setMotionWifi(bool connected);
	static void setMotionOtaActive(bool active);

private:
	static bool active;
	static bool ota_ready;
	static bool wifi_started;
	static uint32_t wifi_start_ms;
	static void beginWifi();
	static void beginOta();
};
