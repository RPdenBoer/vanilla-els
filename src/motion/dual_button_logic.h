#pragma once

#include <stdint.h>

// Shared state machine for two momentary buttons (left/right) with:
// - short-press on release
// - long-press (hold) start callback, plus stop callback on release
// - optional short-press lockout and overlap (both-pressed) debounce
//
// Convention: left => dir +1, right => dir -1

struct DualButtonState
{
	bool left_prev_pressed = false;
	bool right_prev_pressed = false;

	bool left_long_handled = false;
	bool right_long_handled = false;

	bool left_blocked = false;
	bool right_blocked = false;
	uint32_t left_overlap_start_ms = 0;
	uint32_t right_overlap_start_ms = 0;

	uint32_t left_down_ms = 0;
	uint32_t right_down_ms = 0;

	uint32_t last_short_ms = 0;
};

struct DualButtonConfig
{
	uint32_t long_press_ms = 350;
	uint32_t short_lockout_ms = 50;
	uint32_t overlap_debounce_ms = 20;
	bool require_other_released_for_short = true;
};

template <typename LockedOutFn, typename OnShortFn, typename CanStartLongFn, typename OnLongStartFn, typename OnLongStopFn>
inline void dualButtonUpdate(
	DualButtonState &st,
	const DualButtonConfig &cfg,
	bool left_pressed,
	bool right_pressed,
	uint32_t now_ms,
	LockedOutFn locked_out,
	OnShortFn on_short,
	CanStartLongFn can_start_long,
	OnLongStartFn on_long_start,
	OnLongStopFn on_long_stop)
{
	const bool left_edge = left_pressed && !st.left_prev_pressed;
	const bool right_edge = right_pressed && !st.right_prev_pressed;
	const bool left_released = !left_pressed && st.left_prev_pressed;
	const bool right_released = !right_pressed && st.right_prev_pressed;

	// Track press start
	if (left_edge)
	{
		st.left_down_ms = now_ms;
		st.left_long_handled = false;
		st.left_blocked = false;
		st.left_overlap_start_ms = 0;
	}
	if (right_edge)
	{
		st.right_down_ms = now_ms;
		st.right_long_handled = false;
		st.right_blocked = false;
		st.right_overlap_start_ms = 0;
	}

	// Overlap tracking: if both are pressed for longer than overlap_debounce_ms,
	// block interpreting either press as a valid direction press.
	if (cfg.overlap_debounce_ms > 0)
	{
		if (left_pressed && right_pressed)
		{
			if (st.left_overlap_start_ms == 0)
				st.left_overlap_start_ms = now_ms;
			if (st.right_overlap_start_ms == 0)
				st.right_overlap_start_ms = now_ms;

			if ((now_ms - st.left_overlap_start_ms) >= cfg.overlap_debounce_ms)
				st.left_blocked = true;
			if ((now_ms - st.right_overlap_start_ms) >= cfg.overlap_debounce_ms)
				st.right_blocked = true;
		}
		else
		{
			st.left_overlap_start_ms = 0;
			st.right_overlap_start_ms = 0;
		}
	}

	// Long-press detection (exclusive press only)
	if (left_pressed && !right_pressed && !st.left_long_handled && !st.left_blocked)
	{
		if ((now_ms - st.left_down_ms) >= cfg.long_press_ms)
		{
			if (!locked_out() && can_start_long(+1))
				on_long_start(+1);
			st.left_long_handled = true;
		}
	}
	if (right_pressed && !left_pressed && !st.right_long_handled && !st.right_blocked)
	{
		if ((now_ms - st.right_down_ms) >= cfg.long_press_ms)
		{
			if (!locked_out() && can_start_long(-1))
				on_long_start(-1);
			st.right_long_handled = true;
		}
	}

	// Release handling: stop long press, or trigger short press (on release)
	if (left_released)
	{
		if (st.left_long_handled)
		{
			on_long_stop(+1);
		}
		else
		{
			const bool other_ok = !cfg.require_other_released_for_short || !right_pressed;
			const bool lockout_ok = (now_ms - st.last_short_ms) >= cfg.short_lockout_ms;
			if (!st.left_blocked && other_ok && lockout_ok && !locked_out())
			{
				on_short(+1);
				st.last_short_ms = now_ms;
			}
		}
		st.left_overlap_start_ms = 0;
	}
	if (right_released)
	{
		if (st.right_long_handled)
		{
			on_long_stop(-1);
		}
		else
		{
			const bool other_ok = !cfg.require_other_released_for_short || !left_pressed;
			const bool lockout_ok = (now_ms - st.last_short_ms) >= cfg.short_lockout_ms;
			if (!st.right_blocked && other_ok && lockout_ok && !locked_out())
			{
				on_short(-1);
				st.last_short_ms = now_ms;
			}
		}
		st.right_overlap_start_ms = 0;
	}

	st.left_prev_pressed = left_pressed;
	st.right_prev_pressed = right_pressed;
}
