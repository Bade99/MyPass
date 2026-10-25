#pragma once
#include "helpers.h"

namespace animation {

struct Timeline {
	i64 start_counter = 0; f32 duration_ms = 0; bool active = false;

	void start(f32 duration /*ms*/) { start_counter = StartCounter(); duration_ms = duration; active = true; }

	f32 progress() const { return active ? clamp01((f32)(EndCounter(start_counter) / duration_ms)) : 1.f; }

	void stop() { active = false; }
};

static f32 ease_in_out_parametric_blend(f32 t /*[0.0,1.0]*/) { return squared(t) / (2.0f * (squared(t) - t) + 1.0f); }

static f32 ease_in_out_smoothstep(f32 t) { return squared(t) * (3.0f - 2.0f * t); }

static f32 ease_out_cubic(f32 progress) { return 1.f - cubed(1.f - progress); }

static bool enabled_by_system() { BOOL enabled = true; return !SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0) || enabled; }

}
