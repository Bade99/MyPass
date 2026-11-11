#pragma once
#include "smooth_scroll.h"
#include "math.h"

namespace page {
constexpr auto& wndclass = wndclass_name("page");

using OnViewportChanged = void(*)(void*, rect_i32);

struct Theme {
	union {
		struct {
			BrushGroup bk, border;
		};
		BrushGroup all[2];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} brushes;
	union Dimensions {
		struct { UINumber border_thickness; };
		UINumber all[1];
		Dimensions() : all{} { for (auto& dimension : all) dimension.value = F32INFINITY; }
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} dimensions;

	bool copy_from(const Theme& src) {
		bool repaint = false;
		_theme_copy_all_brushes(src.brushes, this->brushes);

		repaint |= _theme_copy_all_dimensions(src.dimensions, this->dimensions);

		return repaint;
	}
};

struct State {
	HWND wnd;
	HWND parent;
	Theme theme;

	union Controls {
		struct {
			HWND v_scroll;
			HWND h_scroll;
			HWND content_area;
		};
		HWND all[3];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} controls;

	bool does_scrolling;
	StatefulFunction<OnViewportChanged> on_viewport_changed;

	SmoothScroll scroller;
	struct ScrollIntoView {
		i32 from = 0, to = 0;
		bool active = false;
	} scroll_into_view;

	void init() {
		new (&scroller) SmoothScroll{};
	}

	void uninit() {
		scroller.~SmoothScroll();
	}
};
}
