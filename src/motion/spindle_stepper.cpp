#include "spindle_stepper.h"
#include "config_motion.h"
#include "mpg_encoder.h"
#include <Arduino.h>
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

// ============================================================================
// Static member initialization
// ============================================================================
volatile int32_t SpindleStepper::position = 0;
int16_t SpindleStepper::rpm_signed = 0;
int16_t SpindleStepper::rpm_abs = 0;
int16_t SpindleStepper::target_rpm = 0;
int16_t SpindleStepper::current_rpm = 0;
int8_t SpindleStepper::direction = 0;
bool SpindleStepper::running = false;

uint32_t SpindleStepper::last_update_us = 0;
uint32_t SpindleStepper::step_interval_us = 0;
uint32_t SpindleStepper::last_step_us = 0;

void* SpindleStepper::timer_handle = nullptr;

// Hardware timer for step generation
static gptimer_handle_t step_timer = nullptr;
static volatile bool step_pin_state = false;

// ============================================================================
// Timer ISR - generates step pulses at precise intervals
// ============================================================================
static bool IRAM_ATTR timer_isr_callback(gptimer_handle_t timer,
                                          const gptimer_alarm_event_data_t *edata,
                                          void *user_ctx) {
    (void)timer;
    (void)edata;
    (void)user_ctx;
    
    // Toggle step pin (rising edge = step)
    if (!step_pin_state) {
        // Rising edge - this is the actual step
        GPIO.out_w1ts = (1UL << SPINDLE_STEP_PIN);
        step_pin_state = true;
        
        // Count position based on direction
        if (SpindleStepper::getDirection() > 0) {
            SpindleStepper::position++;
        } else if (SpindleStepper::getDirection() < 0) {
            SpindleStepper::position--;
        }
    } else {
        // Falling edge
        GPIO.out_w1tc = (1UL << SPINDLE_STEP_PIN);
        step_pin_state = false;
    }
    
    return false;  // Don't yield to higher priority task
}

// ============================================================================
// Initialization
// ============================================================================
bool SpindleStepper::init() {
    // Configure GPIO pins
    pinMode(SPINDLE_STEP_PIN, OUTPUT);
    pinMode(SPINDLE_DIR_PIN, OUTPUT);
    digitalWrite(SPINDLE_STEP_PIN, LOW);
    digitalWrite(SPINDLE_DIR_PIN, LOW);
    
    if (SPINDLE_EN_PIN >= 0) {
        pinMode(SPINDLE_EN_PIN, OUTPUT);
        digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled initially (active low assumed)
    }
    
    // Configure direction switch inputs
    pinMode(SPINDLE_FWD_PIN, INPUT_PULLUP);
    pinMode(SPINDLE_REV_PIN, INPUT_PULLUP);
    
    // Note: MPG encoder is initialized separately in main.cpp
    
    // Initialize hardware timer for step generation
    // We'll use 1MHz resolution (1us ticks)
    gptimer_config_t timer_config = {};
    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_config.direction = GPTIMER_COUNT_UP;
    timer_config.resolution_hz = 1000000;  // 1MHz = 1us resolution
    
    esp_err_t err = gptimer_new_timer(&timer_config, &step_timer);
    if (err != ESP_OK) {
        Serial.printf("[SpindleStepper] Timer create failed: %d\n", err);
        return false;
    }
    
    // Register ISR callback
    gptimer_event_callbacks_t cbs = {};
    cbs.on_alarm = timer_isr_callback;
    err = gptimer_register_event_callbacks(step_timer, &cbs, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[SpindleStepper] Callback register failed: %d\n", err);
        return false;
    }
    
    // Enable timer
    err = gptimer_enable(step_timer);
    if (err != ESP_OK) {
        Serial.printf("[SpindleStepper] Timer enable failed: %d\n", err);
        return false;
    }
    
    last_update_us = micros();
    
    Serial.printf("[SpindleStepper] Initialized: %ld steps/rev, max %ld RPM\n",
        SPINDLE_STEPS_PER_REV, SPINDLE_MAX_RPM);
    
    return true;
}

// ============================================================================
// Read control inputs (MPG encoder and direction switch)
// ============================================================================
void SpindleStepper::readControls() {
    // Read direction switch (active LOW)
    bool fwd = (digitalRead(SPINDLE_FWD_PIN) == LOW);
    bool rev = (digitalRead(SPINDLE_REV_PIN) == LOW);
    
    // Determine direction (both on = invalid, treat as stop)
    if (fwd && !rev) {
        direction = 1;
    } else if (rev && !fwd) {
        direction = -1;
    } else {
        direction = 0;
    }
    
    // Get RPM from MPG encoder (only when in RPM control mode)
    if (MpgEncoder::getMode() == MpgMode::RPM_CONTROL) {
        target_rpm = MpgEncoder::getRpmSetting();
    }
    // When in jog mode, target_rpm stays at last value (spindle keeps running)
    
    // If direction is off, target is 0 (for acceleration ramping)
    if (direction == 0) {
        // Don't reset target_rpm - it represents the "set" RPM
        // Just let the spindle stop via acceleration control
    }
}

// ============================================================================
// Update speed with acceleration limiting
// ============================================================================
void SpindleStepper::updateSpeed() {
    uint32_t now_us = micros();
    uint32_t dt_us = now_us - last_update_us;
    last_update_us = now_us;
    
    // Calculate max RPM change for this update interval
    int32_t max_delta = (SPINDLE_ACCEL_RPM_PER_SEC * (int32_t)dt_us) / 1000000;
    if (max_delta < 1) max_delta = 1;
    
    // Apply acceleration limiting
    int16_t rpm_target = (direction == 0) ? 0 : target_rpm;
    
    if (current_rpm < rpm_target) {
        current_rpm += max_delta;
        if (current_rpm > rpm_target) current_rpm = rpm_target;
    } else if (current_rpm > rpm_target) {
        current_rpm -= max_delta;
        if (current_rpm < rpm_target) current_rpm = rpm_target;
    }
    
    // Update public RPM values
    rpm_abs = current_rpm;
    rpm_signed = current_rpm * direction;
    
    // Calculate step interval from RPM
    // steps_per_sec = (RPM / 60) * STEPS_PER_REV
    // interval_us = 1000000 / steps_per_sec / 2  (divide by 2 for toggle timing)
    
    if (current_rpm < SPINDLE_MIN_RPM) {
        // Stopped or too slow
        step_interval_us = 0;
        running = false;
    } else {
        int32_t steps_per_sec = ((int32_t)current_rpm * SPINDLE_STEPS_PER_REV) / 60;
        // Divide by 2 because timer toggles (need 2 interrupts per step)
        step_interval_us = 500000 / steps_per_sec;  // 1000000 / (steps_per_sec * 2)
        
        // Clamp to reasonable range
        if (step_interval_us < 6) step_interval_us = 6;  // ~83kHz max
        if (step_interval_us > 50000) step_interval_us = 50000;  // 10 Hz min
        
        running = true;
    }
}

// ============================================================================
// Main update - call from loop
// ============================================================================
void SpindleStepper::update() {
    // Read control inputs
    readControls();
    
    // Update speed with acceleration
    updateSpeed();
    
    // Set direction pin
    bool dir_level = (direction >= 0);
    if (SPINDLE_INVERT_DIR) dir_level = !dir_level;
    digitalWrite(SPINDLE_DIR_PIN, dir_level ? HIGH : LOW);
    
    // Update timer alarm interval
    static uint32_t prev_interval = 0;
    static bool timer_running = false;
    
    if (step_interval_us > 0 && running) {
        if (step_interval_us != prev_interval || !timer_running) {
            // Stop timer, reconfigure, restart
            if (timer_running) {
                gptimer_stop(step_timer);
            }
            
            gptimer_alarm_config_t alarm_config = {};
            alarm_config.alarm_count = step_interval_us;
            alarm_config.reload_count = 0;
            alarm_config.flags.auto_reload_on_alarm = true;
            gptimer_set_alarm_action(step_timer, &alarm_config);
            
            gptimer_set_raw_count(step_timer, 0);
            gptimer_start(step_timer);
            
            timer_running = true;
            prev_interval = step_interval_us;
        }
        
        // Enable driver if we have an enable pin
        if (SPINDLE_EN_PIN >= 0) {
            digitalWrite(SPINDLE_EN_PIN, LOW);  // Active low enable
        }
    } else {
        // Stop stepping
        if (timer_running) {
            gptimer_stop(step_timer);
            timer_running = false;
        }
        
        // Disable driver if we have an enable pin
        if (SPINDLE_EN_PIN >= 0) {
            digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled
        }
    }
}

// ============================================================================
// Emergency stop
// ============================================================================
void SpindleStepper::stop() {
    gptimer_stop(step_timer);
    running = false;
    current_rpm = 0;
    rpm_signed = 0;
    rpm_abs = 0;
    target_rpm = 0;
    direction = 0;
    
    if (SPINDLE_EN_PIN >= 0) {
        digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled
    }
    
    Serial.println("[SpindleStepper] Emergency stop");
}
