#pragma once
#include "animation.h"
#include <new>

struct SmoothScroll {
	using ApplyScrollPosition = void (*)(HWND window, f32 previous_offset, f32 current_offset);
	static constexpr f64 smoothing_time_ms = 50.0;

	HWND wnd = nil;
	ApplyScrollPosition apply_position = nil;
	f32 offset = 0;
	f32 wheel_remainder = 0;
	f32 remaining_distance = 0;
	i64 last_tick = 0;
	u32 frame_interval_ms = 0;
	bool active = false;

	void init(HWND window, ApplyScrollPosition apply) {
		wnd = window;
		apply_position = apply;
	}

	void cancel() {
		if (wnd) KillTimer(wnd, timer_id());
		remaining_distance = 0;
		active = false;
		wheel_remainder = 0;
	}

	~SmoothScroll() { cancel(); }

	void start(i32 increment) {
		if (!increment) return;
		if (remaining_distance && signum(remaining_distance) != signum(increment)) remaining_distance = 0;
		remaining_distance += increment;
		if (!active) {
			frame_interval_ms = animation::timer_interval_ms(wnd);
			last_tick = StartCounter();
			active = true;
			if (!SetTimer(wnd, timer_id(), frame_interval_ms, timer_proc)) snap_to_target();
		}
	}

	void tick() {
		if (!active) return;
		i64 counter = StartCounter();
		f64 elapsed_ms = minimum(f64(counter - last_tick) / GetPCFrequency(), 2.0 * frame_interval_ms);
		last_tick = counter;
#ifdef _DEBUG
		elapsed_ms *= animation::Timeline::debug_speed_multiplier;
#endif
		f32 distance = remaining_distance * (1.f - (f32)std::exp(-elapsed_ms / smoothing_time_ms));
		if (std::abs(remaining_distance - distance) <= 0.5f) distance = remaining_distance;
		f32 previous_offset = offset;
		offset += distance;
		remaining_distance -= distance;
		if (offset != previous_offset) apply_position(wnd, previous_offset, offset);
		if (!active) return;
		if (!remaining_distance) {
			active = false;
			KillTimer(wnd, timer_id());
		}
		else if (!SetTimer(wnd, timer_id(), frame_interval_ms, timer_proc)) snap_to_target();
	}

private:
	UINT_PTR timer_id() const { return reinterpret_cast<UINT_PTR>(this); }

	void snap_to_target() {
		KillTimer(wnd, timer_id());
		active = false;
		f32 previous_offset = offset;
		offset += remaining_distance;
		remaining_distance = 0;
		if (offset != previous_offset) apply_position(wnd, previous_offset, offset);
	}

	static void CALLBACK timer_proc(HWND hwnd, UINT, UINT_PTR timer_id, DWORD) {
		if (!IsWindow(hwnd)) return;
		if (auto* owner = reinterpret_cast<SmoothScroll*>(timer_id); owner && owner->wnd == hwnd) owner->tick();
	}
};
