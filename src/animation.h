#pragma once
#include "helpers.h"
#include <cmath>

namespace animation {

struct Timeline {
#ifdef _DEBUG
	static inline f32 debug_speed_multiplier = 1.f;
	i64 last_counter = 0;
	f64 elapsed_ms = 0;
#endif

	i64 start_counter = 0; f32 duration_ms = 0; bool active = false;

	void start(f32 duration /*ms*/) {
		start_counter = StartCounter();
		duration_ms = duration;
#ifdef _DEBUG
		last_counter = start_counter;
		elapsed_ms = 0;
#endif
		active = true;
	}

	f32 progress() {
		if (!active) return 1.f;
#ifdef _DEBUG
		i64 counter = StartCounter();
		elapsed_ms += f64(counter - last_counter) / GetPCFrequency() * debug_speed_multiplier;
		last_counter = counter;
		return clamp01((f32)(elapsed_ms / duration_ms));
#else
		return clamp01((f32)(EndCounter(start_counter) / duration_ms));
#endif
	}

	void stop() { active = false; }
};

static u32 timer_interval_ms(HWND wnd) {
	auto refresh_rate = refresh_rate_hz(wnd);
	if (refresh_rate <= 1) refresh_rate = 60;
	return maximum(1u, (u32)roundf(1000.f / refresh_rate));
}

static f32 ease_in_out_parametric_blend(f32 t /*[0.0,1.0]*/) { return squared(t) / (2.0f * (squared(t) - t) + 1.0f); }

static f32 ease_in_out_smoothstep(f32 t) { return squared(t) * (3.0f - 2.0f * t); }

static f32 ease_out_cubic(f32 progress) { return 1.f - cubed(1.f - progress); }

static f32 exponential_smoothing_factor(f64 elapsed_ms, f64 time_constant_ms) {
	if (elapsed_ms <= 0) return 0.f;
	if (time_constant_ms <= 0) return 1.f;
	return 1.f - (f32)std::exp(-elapsed_ms / time_constant_ms);
}

static bool enabled_by_system() { BOOL enabled = true; return !SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0) || enabled; }

}
