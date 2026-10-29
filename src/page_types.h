#pragma once
#include "smooth_scroll.h"

namespace page {
constexpr auto& wndclass = wndclass_name("page");

struct Theme {
	union {
		struct {
			BrushGroup bk, border;
		};
		BrushGroup all[2];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} brushes;
	struct {
		u32 border_thickness = U32MAX;
	}dimensions;

	bool copy_from(const Theme& src) {
		bool repaint = false;
		_theme_copy_all_brushes(src.brushes, this->brushes);

		_theme_copy_u32(src.dimensions.border_thickness, this->dimensions.border_thickness);

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

	SmoothScroll scroller;

	void init() {
		new (&scroller) SmoothScroll{};
	}

	void uninit() {
		scroller.~SmoothScroll();
	}
};
}
