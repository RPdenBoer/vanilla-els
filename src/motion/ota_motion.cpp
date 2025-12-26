#include "ota_motion.h"
#include "shared/wifi_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

bool OtaMotion::active = false;
bool OtaMotion::ota_ready = false;
bool OtaMotion::wifi_started = false;
uint32_t OtaMotion::wifi_start_ms = 0;
bool OtaMotion::wifi_connected = false;

void OtaMotion::beginWifi()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	wifi_started = true;
	wifi_start_ms = millis();
	Serial.println("[OTA] Motion WiFi start");
}

void OtaMotion::beginOta()
{
	ArduinoOTA.setHostname("vanilla-els-motion");
	ArduinoOTA.setRebootOnSuccess(false);
	ArduinoOTA.begin();
	ota_ready = true;
	Serial.println("[OTA] Motion ready");
}

void OtaMotion::start()
{
	if (active)
		return;
	active = true;
	ota_ready = false;
	wifi_started = false;
	beginWifi();
}

void OtaMotion::handle()
{
	if (!active)
		return;

	wifi_connected = (WiFi.status() == WL_CONNECTED);

	if (WiFi.status() == WL_CONNECTED)
	{
		if (!ota_ready)
			beginOta();
		ArduinoOTA.handle();
		return;
	}

	if (!wifi_started)
		beginWifi();
}

bool OtaMotion::isActive()
{
	return active;
}

bool OtaMotion::isWifiConnected()
{
	return wifi_connected;
}
