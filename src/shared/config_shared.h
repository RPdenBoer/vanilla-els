#pragma once

// ============================================================================
// Shared configuration constants used by both UI and Motion boards
// ============================================================================

// Debug logging (set to 1 to enable verbose SPI/state change logging)
#define DEBUG_SPI_LOGGING 1

// Tool system
static constexpr int TOOL_COUNT = 8;

// Work offset system (G54/G55 style)
static constexpr int OFFSET_COUNT = 3;

// ============================================================================
// RPM Control Segments (non-linear MPG mapping)
// ============================================================================
// Each segment defines an RPM range and step size.
// Fine control at low RPM, coarse at high RPM to mask timing quantization.
// All values in RPM (not RPM×10).

// Segment boundaries (RPM)
static constexpr int32_t RPM_SEG1_END = 500;  // 0 to SEG1_END: finest control
static constexpr int32_t RPM_SEG2_END = 1000; // SEG1_END to SEG2_END
static constexpr int32_t RPM_SEG3_END = 2000; // SEG2_END to SEG3_END
static constexpr int32_t RPM_SEG4_END = 3000; // SEG3_END to SEG4_END (max)

// Step sizes per segment (RPM per encoder count)
// Note: SEG1 uses 0.1 RPM steps (stored as RPM×10 internally)
static constexpr int32_t RPM_SEG1_STEP_X10 = 1;	  // 0.1 RPM per count
static constexpr int32_t RPM_SEG2_STEP_X10 = 10;  // 1 RPM per count
static constexpr int32_t RPM_SEG3_STEP_X10 = 50;  // 10 RPM per count
static constexpr int32_t RPM_SEG4_STEP_X10 = 250; // 50 RPM per count

// Helper function to quantize RPM×10 to nearest valid display step
inline int32_t quantizeRpmX10ForDisplay(int32_t rpm_x10)
{
	if (rpm_x10 < 0)
		rpm_x10 = 0;

	// Determine which segment and round to nearest step
	if (rpm_x10 <= RPM_SEG1_END * 10)
	{
		// Segment 1: 0.1 RPM steps (already at finest resolution)
		return rpm_x10;
	}
	else if (rpm_x10 <= RPM_SEG2_END * 10)
	{
		// Segment 2: 1 RPM steps (round to nearest 10 in rpm_x10)
		return ((rpm_x10 + RPM_SEG2_STEP_X10 / 2) / RPM_SEG2_STEP_X10) * RPM_SEG2_STEP_X10;
	}
	else if (rpm_x10 <= RPM_SEG3_END * 10)
	{
		// Segment 3: 10 RPM steps (round to nearest 100 in rpm_x10)
		return ((rpm_x10 + RPM_SEG3_STEP_X10 / 2) / RPM_SEG3_STEP_X10) * RPM_SEG3_STEP_X10;
	}
	else
	{
		// Segment 4: 50 RPM steps (round to nearest 500 in rpm_x10)
		return ((rpm_x10 + RPM_SEG4_STEP_X10 / 2) / RPM_SEG4_STEP_X10) * RPM_SEG4_STEP_X10;
	}
}

// ============================================================================
// Spindle Configuration
// ============================================================================
// Encoder mode: external motor with quadrature encoder feedback
// Stepper mode: ESP32-driven stepper motor (step count = position)

// Spindle counts per revolution (applies to both modes)
// For encoder: quadrature counts per rev of the physical encoder
// For stepper: steps per rev of the stepper motor
static constexpr int32_t C_COUNTS_PER_REV = 1600;

// ELS gearing configuration
// Stepper: 1600 steps/rev (per your motor/driver setup)
static constexpr int32_t ELS_STEPS_PER_REV = 1600;
// Leadscrew: 2mm pitch => 2000um per rev
static constexpr int32_t ELS_LEADSCREW_PITCH_UM = 2000;

// Linear scales: one decoded quadrature count corresponds to N microns
static constexpr int32_t X_UM_PER_COUNT = 5;
static constexpr int32_t Z_UM_PER_COUNT = 5;

// SPI communication settings
// 64 bytes @ 1 MHz = ~512 µs per transaction, plenty fast for 100 Hz polling
static constexpr uint32_t SPI_CLOCK_HZ = 1000000;   // 1 MHz SPI clock (reliable over wires)
static constexpr uint32_t SPI_POLL_INTERVAL_MS = 10; // Poll motion board every 10ms (100 Hz)
