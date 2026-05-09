#pragma once

namespace page {
constexpr auto& wndclass = wndclass_name("page");

struct Theme {
	union {
		struct {
			brush_group bk, border;
		};
		brush_group all[2];
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

	bool does_scrolling;

	struct {
		bool active; //currently performing animation
		int current_frame; //starts at
		f32 dt;
		int total_frames;
		i64 _time_between; //internal
		f32 time_between_last_two_scroll_events; //ms
	} scroll_anim;
	std::deque<int> scroll_tasks;

	f32 scroll;//vertical scrolling of page //@int NOTE(fran): given the limitations of MoveWindow we must interpret this value as an integer (cast), the float precision is only used for animations

	void init() {
		scroll_tasks = decltype(scroll_tasks)();
	}

	void uninit() {
		scroll_tasks.~deque();
	}
};
}