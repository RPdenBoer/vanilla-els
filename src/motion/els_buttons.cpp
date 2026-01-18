#include "els_buttons.h"

#include <Arduino.h>

#include "config_motion.h"
#include "dual_button_logic.h"
#include "els_core.h"
#include "mpg_encoder.h"

namespace {
	static bool jog_active = false;
	static int8_t jog_dir = 0;
	static DualButtonState g_btn_state;
	static constexpr DualButtonConfig BTN_CFG = {
		.long_press_ms = SPINDLE_JOG_PRESS_MS,
		.short_lockout_ms = MOMENTARY_SHORT_LOCKOUT_MS,
		.overlap_debounce_ms = MOMENTARY_OVERLAP_DEBOUNCE_MS,
		.require_other_released_for_short = true,
	};

	static inline bool elsLockedOut() {
		// Match UI behavior: when MPG is routing to Z jog, don't allow starting ELS/jog.
		return (MpgEncoder::getMode() == MpgMode::JOG_Z) && !ElsCore::isEnabled();
	}
}

void ElsButtons::init() {
	pinMode(ELS_BTN_LEFT_PIN, INPUT_PULLUP);
	pinMode(ELS_BTN_RIGHT_PIN, INPUT_PULLUP);
	jog_active = false;
	jog_dir = 0;
	g_btn_state = DualButtonState{};
	Serial.printf("[ElsButtons] Init: L=%d, R=%d\n", ELS_BTN_LEFT_PIN, ELS_BTN_RIGHT_PIN);
}

void ElsButtons::handleShortPress(int8_t dir) {
	if (elsLockedOut())
		return;

	// If ELS is currently running, any press is an e-stop.
	if (ElsCore::isEnabled()) {
		ElsCore::setJog(0, false);
		ElsCore::setEnabled(false);
		ElsCore::setDirectionMul(1);
		return;
	}

	// Otherwise, enable ELS in the requested direction.
	ElsCore::setJog(0, false);
	ElsCore::setDirectionMul(dir);
	ElsCore::setEnabled(true);
}

void ElsButtons::startJog(int8_t dir) {
	if (elsLockedOut())
		return;

	// Disable ELS while jogging to avoid mixed outputs.
	ElsCore::setEnabled(false);
	ElsCore::setDirectionMul(1);

	jog_active = true;
	jog_dir = (dir > 0) ? 1 : -1;
	ElsCore::setJog(jog_dir, true);
}

void ElsButtons::stopJog(int8_t dir) {
	if (!jog_active)
		return;
	if (jog_dir != ((dir > 0) ? 1 : -1))
		return;

	jog_active = false;
	jog_dir = 0;
	ElsCore::setJog(0, false);
}

void ElsButtons::update() {
	const uint32_t now_ms = millis();

	const bool left_pressed = (digitalRead(ELS_BTN_LEFT_PIN) == LOW);
	const bool right_pressed = (digitalRead(ELS_BTN_RIGHT_PIN) == LOW);

	dualButtonUpdate(
		g_btn_state,
		BTN_CFG,
		left_pressed,
		right_pressed,
		now_ms,
		[]() { return elsLockedOut(); },
		[](int8_t dir) { ElsButtons::handleShortPress(dir); },
		[](int8_t /*dir*/) { return true; },
		[](int8_t dir) { ElsButtons::startJog(dir); },
		[](int8_t dir) { ElsButtons::stopJog(dir); });
}
