#include "encoder_motion.h"
#include "config_motion.h"
#include <Arduino.h>
#include "driver/gpio.h"

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
#include "driver/pulse_cnt.h"
#endif

// Static member initialization
EncoderMotion::QuadAxis EncoderMotion::x_axis = {0, 0, 0, 0, 1};
EncoderMotion::QuadAxis EncoderMotion::z_axis = {0, 0, 0, 0, 1};

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
pcnt_unit_handle_t pcnt_unit = nullptr;
pcnt_channel_handle_t pcnt_chan_a = nullptr;
pcnt_channel_handle_t pcnt_chan_b = nullptr;
volatile int32_t EncoderMotion::c_pcnt_accum = 0;

int16_t EncoderMotion::rpm_signed = 0;
int16_t EncoderMotion::rpm_abs = 0;
#endif

// ============================================================================
// Quadrature decoder for X/Z linear encoders (GPIO ISR based)
// ============================================================================

static inline uint8_t read_ab(uint8_t pin_a, uint8_t pin_b) {
    const int a = gpio_get_level((gpio_num_t)pin_a);
    const int b = gpio_get_level((gpio_num_t)pin_b);
    return (uint8_t)(((a ? 1 : 0) << 1) | (b ? 1 : 0));
}

void IRAM_ATTR EncoderMotion::quadIsr(void *arg) {
    QuadAxis *axis = (QuadAxis *)arg;

    // Quadrature state machine transition table
    static const int8_t delta_tbl[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0,
    };

    const uint8_t old_state = axis->state & 0x3;
    const uint8_t new_state = read_ab(axis->pin_a, axis->pin_b);
    axis->state = new_state;

    const uint8_t idx = (uint8_t)((old_state << 2) | new_state);
    const int8_t d = delta_tbl[idx];
    if (d != 0) {
        axis->count += (int32_t)d * (int32_t)axis->dir;
    }
}

void EncoderMotion::initLinearAxis(QuadAxis &axis) {
    pinMode(axis.pin_a, INPUT_PULLUP);
    pinMode(axis.pin_b, INPUT_PULLUP);

    axis.state = read_ab(axis.pin_a, axis.pin_b);

    attachInterruptArg((int)axis.pin_a, quadIsr, (void *)&axis, CHANGE);
    attachInterruptArg((int)axis.pin_b, quadIsr, (void *)&axis, CHANGE);
}

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
// ============================================================================
// PCNT overflow callback (extends 16-bit counter to 32-bit)
// ============================================================================

static bool IRAM_ATTR pcnt_on_reach(pcnt_unit_handle_t unit,
                                    const pcnt_watch_event_data_t *edata,
                                    void *user_ctx) {
    (void)user_ctx;
    EncoderMotion::c_pcnt_accum += edata->watch_point_value;
    pcnt_unit_clear_count(unit);
    return true;
}
#endif

// ============================================================================
// Initialization
// ============================================================================

bool EncoderMotion::init() {
    // Initialize X axis encoder
    x_axis.pin_a = X_PINA;
    x_axis.pin_b = X_PINB;
    x_axis.count = 0;
    x_axis.dir = X_INVERT_DIR ? -1 : 1;
    initLinearAxis(x_axis);

    // Initialize Z axis encoder
    z_axis.pin_a = Z_PINA;
    z_axis.pin_b = Z_PINB;
    z_axis.count = 0;
    z_axis.dir = Z_INVERT_DIR ? -1 : 1;
    initLinearAxis(z_axis);

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
	// Initialize spindle encoder using PCNT hardware
    pinMode(C_PINA, INPUT_PULLUP);
    pinMode(C_PINB, INPUT_PULLUP);

    pcnt_unit_config_t unit_config = {};
    unit_config.high_limit = PCNT_H_LIM;
    unit_config.low_limit  = PCNT_L_LIM;

    esp_err_t err = pcnt_new_unit(&unit_config, &pcnt_unit);
    if (err != ESP_OK) return false;

    // Channel A: edges on A, direction from B level
    pcnt_chan_config_t chan_a_config = {};
    chan_a_config.edge_gpio_num = C_PINA;
    chan_a_config.level_gpio_num = C_PINB;

    err = pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a);
    if (err != ESP_OK) return false;

    err = pcnt_channel_set_edge_action(
        pcnt_chan_a,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE
    );
    if (err != ESP_OK) return false;

    err = pcnt_channel_set_level_action(
        pcnt_chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE
    );
    if (err != ESP_OK) return false;

    // Channel B: edges on B, direction from A level (inverted mapping)
    pcnt_chan_config_t chan_b_config = {};
    chan_b_config.edge_gpio_num = C_PINB;
    chan_b_config.level_gpio_num = C_PINA;

    err = pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b);
    if (err != ESP_OK) return false;

    err = pcnt_channel_set_edge_action(
        pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE
    );
    if (err != ESP_OK) return false;

    err = pcnt_channel_set_level_action(
        pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP
    );
    if (err != ESP_OK) return false;

    // Glitch filter
    pcnt_glitch_filter_config_t filter_config = {};
    filter_config.max_glitch_ns = 2000;
    (void)pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config);

    // Watch points for overflow handling
    (void)pcnt_unit_add_watch_point(pcnt_unit, PCNT_H_LIM);
    (void)pcnt_unit_add_watch_point(pcnt_unit, PCNT_L_LIM);

    // Overflow callback
    pcnt_event_callbacks_t cbs = {};
    cbs.on_reach = pcnt_on_reach;
    err = pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, nullptr);
    if (err != ESP_OK) return false;

    // Enable and start
    err = pcnt_unit_enable(pcnt_unit);
    if (err != ESP_OK) return false;

    err = pcnt_unit_clear_count(pcnt_unit);
    if (err != ESP_OK) return false;

    c_pcnt_accum = 0;
    err = pcnt_unit_start(pcnt_unit);
    if (err != ESP_OK) return false;
#endif // SPINDLE_MODE_ENCODER

	return true;
}

// ============================================================================
// Update (called periodically from motion task)
// ============================================================================

void EncoderMotion::update() {
#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
	// RPM calculation (run at lower rate) - only for encoder mode
	static uint32_t last_ms = 0;
    static int32_t last_total = 0;

    const uint32_t now = millis();
    const uint32_t dt_ms = now - last_ms;
    if (dt_ms < 100) return;  // 10 Hz RPM update

    int32_t total = getTotalSpindleCount();
    int32_t delta = total - last_total;

    last_total = total;
    last_ms = now;

    // Calculate RPM
    float dt_s = (float)dt_ms / 1000.0f;
    float revs = (float)delta / (float)C_COUNTS_PER_REV;
    float rps = revs / dt_s;
    float rpmf = rps * 60.0f;

    rpm_signed = (int16_t)lroundf(rpmf);
    rpm_abs = (int16_t)lroundf(fabsf(rpmf));
#endif
	// In stepper mode, RPM comes from SpindleStepper class
}

// ============================================================================
// Getters
// ============================================================================

int32_t EncoderMotion::getXCount() {
    int32_t v;
    noInterrupts();
    v = x_axis.count;
    interrupts();
    return v;
}

int32_t EncoderMotion::getZCount() {
    int32_t v;
    noInterrupts();
    v = z_axis.count;
    interrupts();
    return v;
}

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
int32_t EncoderMotion::getTotalSpindleCount() {
    int count = 0;
    pcnt_unit_get_count(pcnt_unit, &count);
    return (int32_t)c_pcnt_accum + (int32_t)count;
}

int32_t EncoderMotion::getSpindleCount() {
    return getTotalSpindleCount();
}
#endif
