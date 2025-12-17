#include "encoder.h"
#include "config.h"
#include "coordinates.h"
#include <Arduino.h>
#include "driver/gpio.h"

pcnt_unit_handle_t EncoderManager::c_pcnt_unit = nullptr;
pcnt_channel_handle_t EncoderManager::c_pcnt_chan = nullptr;
volatile int32_t EncoderManager::c_pcnt_accum = 0;

int32_t EncoderManager::c_raw_ticks = 0;
int32_t EncoderManager::rpm_raw = 0;
bool EncoderManager::c_show_rpm = false;

EncoderManager::QuadAxis EncoderManager::x_axis = {0, 0, 0, 0, 1};
EncoderManager::QuadAxis EncoderManager::z_axis = {0, 0, 0, 0, 1};

static inline uint8_t read_ab(uint8_t pin_a, uint8_t pin_b)
{
    // Use gpio_get_level for speed/ISR-safety
    const int a = gpio_get_level((gpio_num_t)pin_a);
    const int b = gpio_get_level((gpio_num_t)pin_b);
    return (uint8_t)(((a ? 1 : 0) << 1) | (b ? 1 : 0));
}

void IRAM_ATTR EncoderManager::quadIsr(void *arg)
{
    QuadAxis *axis = (QuadAxis *)arg;

    // Transition table for quadrature state machine.
    // index = (old_state<<2) | new_state
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

void EncoderManager::initLinearAxis(QuadAxis &axis)
{
    pinMode(axis.pin_a, INPUT_PULLUP);
    pinMode(axis.pin_b, INPUT_PULLUP);

    axis.state = read_ab(axis.pin_a, axis.pin_b);

    // Attach interrupts on both channels so we decode all transitions.
    attachInterruptArg((int)axis.pin_a, quadIsr, (void *)&axis, CHANGE);
    attachInterruptArg((int)axis.pin_b, quadIsr, (void *)&axis, CHANGE);
}

void EncoderManager::updateLinearAxes()
{
    int32_t x_count;
    int32_t z_count;

    noInterrupts();
    x_count = x_axis.count;
    z_count = z_axis.count;
    interrupts();

    CoordinateSystem::x_raw_um = x_count * X_UM_PER_COUNT;
    CoordinateSystem::z_raw_um = z_count * Z_UM_PER_COUNT;
}

int32_t EncoderManager::getXCount()
{
    int32_t v;
    noInterrupts();
    v = x_axis.count;
    interrupts();
    return v;
}

int32_t EncoderManager::getZCount()
{
    int32_t v;
    noInterrupts();
    v = z_axis.count;
    interrupts();
    return v;
}

bool EncoderManager::init() {
    // X/Z quadrature via GPIO interrupts
    x_axis.pin_a = X_PINA;
    x_axis.pin_b = X_PINB;
    x_axis.count = 0;
    x_axis.dir = X_INVERT_DIR ? -1 : 1;

    z_axis.pin_a = Z_PINA;
    z_axis.pin_b = Z_PINB;
    z_axis.count = 0;
    z_axis.dir = Z_INVERT_DIR ? -1 : 1;

    initLinearAxis(x_axis);
    initLinearAxis(z_axis);

    // Configure inputs
    pinMode(C_PINA, INPUT_PULLUP);
    pinMode(C_PINB, INPUT_PULLUP);

    pcnt_unit_config_t unit_config = {};
    unit_config.high_limit = PCNT_H_LIM;
    unit_config.low_limit  = PCNT_L_LIM;

    esp_err_t err = pcnt_new_unit(&unit_config, &c_pcnt_unit);
    if (err != ESP_OK) return false;

    pcnt_chan_config_t chan_config = {};
    chan_config.edge_gpio_num = C_PINA;   // pulses on A
    chan_config.level_gpio_num = C_PINB;  // direction from B level

    err = pcnt_new_channel(c_pcnt_unit, &chan_config, &c_pcnt_chan);
    if (err != ESP_OK) return false;

    // x2 decoding: count both rising and falling edges on A
    // Direction determined by B (level action inverts)
    err = pcnt_channel_set_edge_action(
        c_pcnt_chan,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, // rising edge
        PCNT_CHANNEL_EDGE_ACTION_DECREASE  // falling edge
    );
    if (err != ESP_OK) return false;

    err = pcnt_channel_set_level_action(
        c_pcnt_chan,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,    // when B low
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE  // when B high -> invert direction
    );
    if (err != ESP_OK) return false;

    // Optional glitch filter (units are in APB clock cycles; keep modest)
    pcnt_glitch_filter_config_t filter_config = {};
    filter_config.max_glitch_ns = 2000; // 2 us
    (void)pcnt_unit_set_glitch_filter(c_pcnt_unit, &filter_config);

    // Add watch points for accumulator extension
    (void)pcnt_unit_add_watch_point(c_pcnt_unit, PCNT_H_LIM);
    (void)pcnt_unit_add_watch_point(c_pcnt_unit, PCNT_L_LIM);

    // Register callbacks
    pcnt_event_callbacks_t cbs = {};
    cbs.on_reach = onReachCallback;
    err = pcnt_unit_register_event_callbacks(c_pcnt_unit, &cbs, nullptr);
    if (err != ESP_OK) return false;

    // Enable and start
    err = pcnt_unit_enable(c_pcnt_unit);
    if (err != ESP_OK) return false;

    err = pcnt_unit_clear_count(c_pcnt_unit);
    if (err != ESP_OK) return false;

    c_pcnt_accum = 0;
    err = pcnt_unit_start(c_pcnt_unit);
    if (err != ESP_OK) return false;

    return true;
}

void EncoderManager::update() {
    // Always keep linear axes fresh
    updateLinearAxes();

    // Sample every N ms
    static uint32_t last_ms = 0;
    static int32_t last_total = 0;

    const uint32_t now = millis();
    const uint32_t dt_ms = now - last_ms;
    if (dt_ms < 100) return; // 10 Hz update

    int32_t total = getTotalCount();
    int32_t delta = total - last_total;

    last_total = total;
    last_ms = now;

    // Phase: wrap to 0..1599
    int32_t phase = CoordinateSystem::wrap01599(total);
    c_raw_ticks = phase;

    // RPM: delta counts / counts_per_rev -> revs in dt
    // rpm = revs/sec * 60
    float dt_s = (float)dt_ms / 1000.0f;
    float revs = (float)delta / (float)C_COUNTS_PER_REV;
    float rps  = revs / dt_s;
    float rpmf = fabsf(rps * 60.0f);

    rpm_raw = (int32_t)lroundf(rpmf);

    // Mode switching with hysteresis
    if (!c_show_rpm && rpm_raw >= RPM_SHOW_RPM_ON) {
        c_show_rpm = true;
    } else if (c_show_rpm && rpm_raw <= RPM_SHOW_RPM_OFF) {
        c_show_rpm = false;
    }
}

int32_t EncoderManager::getSpindleTotalCount() {
    return getTotalCount();
}

int32_t EncoderManager::getTotalCount() {
    int count = 0;
    pcnt_unit_get_count(c_pcnt_unit, &count);
    // extended total = accum + current hw count
    return (int32_t)c_pcnt_accum + (int32_t)count;
}

bool IRAM_ATTR EncoderManager::onReachCallback(pcnt_unit_handle_t unit,
                                              const pcnt_watch_event_data_t *edata,
                                              void *user_ctx) {
    (void)user_ctx;
    // edata->watch_point_value is the threshold reached (e.g. +12000 or -12000)
    c_pcnt_accum += edata->watch_point_value;

    // clear hardware count so it can continue accumulating
    pcnt_unit_clear_count(unit);
    return true; // yield if needed
}