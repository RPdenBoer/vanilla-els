#include "ota_proxy.h"
#include "shared/wifi_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <lvgl.h>

bool OtaProxy::active = false;
bool OtaProxy::ota_ready = false;
bool OtaProxy::wifi_started = false;
uint32_t OtaProxy::wifi_start_ms = 0;

static lv_obj_t *ota_screen = nullptr;
static lv_obj_t *ota_status = nullptr;
static lv_obj_t *ota_wifi_ui = nullptr;
static lv_obj_t *ota_wifi_motion = nullptr;
static lv_obj_t *ota_reboot_btn = nullptr;
static char ota_status_buf[64] = "";

enum class OtaUiState : uint8_t {
	IDLE = 0,
	WAIT_WIFI,
	WAIT_UPLOAD,
	UPLOADING,
	DONE,
	ERROR,
};

static volatile uint8_t ota_state = static_cast<uint8_t>(OtaUiState::IDLE);
static volatile uint8_t ota_error_code = 0;
static bool motion_wifi_connected = false;
static bool motion_ota_active = false;
static bool reboot_pending = false;
static uint32_t reboot_at_ms = 0;

static void setOtaStatus(const char *text)
{
	if (!ota_status || !text)
		return;
	if (strncmp(ota_status_buf, text, sizeof(ota_status_buf)) == 0)
		return;
	snprintf(ota_status_buf, sizeof(ota_status_buf), "%s", text);
	lv_label_set_text(ota_status, ota_status_buf);
}

static void ensureOtaScreen()
{
	if (!ota_screen)
	{
		ota_screen = lv_obj_create(nullptr);
		lv_obj_set_style_bg_color(ota_screen, lv_color_hex(0x0D0D0D), 0);
		lv_obj_set_style_bg_opa(ota_screen, LV_OPA_COVER, 0);
		lv_obj_set_style_pad_all(ota_screen, 16, 0);
		lv_obj_set_style_pad_row(ota_screen, 12, 0);
		lv_obj_set_layout(ota_screen, LV_LAYOUT_FLEX);
		lv_obj_set_flex_flow(ota_screen, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(ota_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

		lv_obj_t *title = lv_label_create(ota_screen);
		lv_label_set_text(title, "OTA MODE");
		lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
		lv_obj_set_style_text_color(title, lv_color_white(), 0);

		ota_status = lv_label_create(ota_screen);
		lv_label_set_text(ota_status, "Waiting for WiFi...");
		lv_obj_set_style_text_font(ota_status, &lv_font_montserrat_20, 0);
		lv_obj_set_style_text_color(ota_status, lv_palette_main(LV_PALETTE_GREY), 0);

		ota_wifi_ui = lv_label_create(ota_screen);
		lv_label_set_text(ota_wifi_ui, "UI WiFi: ...");
		lv_obj_set_style_text_font(ota_wifi_ui, &lv_font_montserrat_14, 0);
		lv_obj_set_style_text_color(ota_wifi_ui, lv_palette_main(LV_PALETTE_GREY), 0);

		ota_wifi_motion = lv_label_create(ota_screen);
		lv_label_set_text(ota_wifi_motion, "Motion WiFi: ...");
		lv_obj_set_style_text_font(ota_wifi_motion, &lv_font_montserrat_14, 0);
		lv_obj_set_style_text_color(ota_wifi_motion, lv_palette_main(LV_PALETTE_GREY), 0);

		ota_reboot_btn = lv_btn_create(ota_screen);
		lv_obj_set_width(ota_reboot_btn, LV_PCT(60));
		lv_obj_set_height(ota_reboot_btn, 44);
		lv_obj_clear_flag(ota_reboot_btn, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_style_bg_color(ota_reboot_btn, lv_palette_darken(LV_PALETTE_RED, 1), LV_PART_MAIN);
		lv_obj_set_style_border_color(ota_reboot_btn, lv_palette_darken(LV_PALETTE_RED, 1), LV_PART_MAIN);
		lv_obj_set_style_text_color(ota_reboot_btn, lv_color_white(), LV_PART_MAIN);
		lv_obj_add_event_cb(ota_reboot_btn, [](lv_event_t *e) {
			if (lv_event_get_code(e) != LV_EVENT_CLICKED)
				return;
			OtaProxy::requestReboot();
		}, LV_EVENT_CLICKED, nullptr);
		lv_obj_t *btn_label = lv_label_create(ota_reboot_btn);
		lv_label_set_text(btn_label, "Reboot Both");
		lv_obj_center(btn_label);
	}

	lv_screen_load(ota_screen);
}

static void updateOtaUi()
{
	if (!ota_screen)
		return;
	const OtaUiState state = static_cast<OtaUiState>(ota_state);

	static OtaUiState last_state = OtaUiState::IDLE;
	static bool last_ui_wifi = false;
	static bool last_motion_wifi = false;

	if (state != last_state)
	{
		switch (state)
		{
		case OtaUiState::WAIT_WIFI:
			setOtaStatus("Connecting WiFi...");
			break;
		case OtaUiState::WAIT_UPLOAD:
			setOtaStatus("Waiting for upload");
			break;
		case OtaUiState::UPLOADING:
			setOtaStatus("Uploading...");
			break;
		case OtaUiState::DONE:
			setOtaStatus("Upload complete");
			break;
		case OtaUiState::ERROR: {
			char buf[48];
			snprintf(buf, sizeof(buf), "OTA error %u", (unsigned)ota_error_code);
			setOtaStatus(buf);
			break;
		}
		default:
			setOtaStatus("OTA idle");
			break;
		}
		last_state = state;
	}

	const bool ui_wifi = (WiFi.status() == WL_CONNECTED);
	if (ota_wifi_ui && ui_wifi != last_ui_wifi)
	{
		lv_label_set_text(ota_wifi_ui, ui_wifi ? "UI WiFi: connected" : "UI WiFi: disconnected");
		lv_obj_set_style_text_color(ota_wifi_ui,
			ui_wifi ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_GREY), 0);
		last_ui_wifi = ui_wifi;
	}

	if (ota_wifi_motion && motion_wifi_connected != last_motion_wifi)
	{
		lv_label_set_text(ota_wifi_motion,
			motion_wifi_connected ? "Motion WiFi: connected" : "Motion WiFi: disconnected");
		lv_obj_set_style_text_color(ota_wifi_motion,
			motion_wifi_connected ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_GREY), 0);
		last_motion_wifi = motion_wifi_connected;
	}
}

void OtaProxy::beginWifi()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	wifi_started = true;
	wifi_start_ms = millis();
	Serial.println("[OTA] UI WiFi start");
}

void OtaProxy::beginOta()
{
	ArduinoOTA.setHostname("vanilla-els-ui");
	ArduinoOTA.setRebootOnSuccess(false);
	ArduinoOTA.onStart([]() {
		ota_state = static_cast<uint8_t>(OtaUiState::UPLOADING);
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		if (total == 0)
			return;
		(void)progress;
	});
	ArduinoOTA.onEnd([]() {
		ota_state = static_cast<uint8_t>(OtaUiState::DONE);
	});
	ArduinoOTA.onError([](ota_error_t error) {
		ota_error_code = static_cast<uint8_t>(error);
		ota_state = static_cast<uint8_t>(OtaUiState::ERROR);
	});
	ArduinoOTA.begin();
	ota_ready = true;
	ota_state = static_cast<uint8_t>(OtaUiState::WAIT_UPLOAD);
	Serial.println("[OTA] UI ready");
}

void OtaProxy::start()
{
	ensureOtaScreen();
	ota_state = static_cast<uint8_t>(OtaUiState::WAIT_WIFI);
	if (active)
		return;
	active = true;
	ota_ready = false;
	wifi_started = false;
	beginWifi();
}

void OtaProxy::handle()
{
	if (!active)
		return;

	if (reboot_pending && (int32_t)(millis() - reboot_at_ms) >= 0)
	{
		ESP.restart();
	}

	if (WiFi.status() == WL_CONNECTED)
	{
		if (!ota_ready)
			beginOta();
		ArduinoOTA.handle();
		updateOtaUi();
		return;
	}

	if (!wifi_started)
		beginWifi();
	ota_state = static_cast<uint8_t>(OtaUiState::WAIT_WIFI);
	updateOtaUi();
}

bool OtaProxy::isActive()
{
	return active;
}

void OtaProxy::requestReboot()
{
	if (reboot_pending)
		return;
	reboot_pending = true;
	reboot_at_ms = millis() + 500;
	ota_state = static_cast<uint8_t>(OtaUiState::DONE);
	setOtaStatus("Rebooting...");
}

bool OtaProxy::shouldRequestReboot()
{
	return reboot_pending;
}

void OtaProxy::setMotionWifi(bool connected)
{
	motion_wifi_connected = connected;
}

void OtaProxy::setMotionOtaActive(bool active_motion)
{
	motion_ota_active = active_motion;
	(void)motion_ota_active;
}
