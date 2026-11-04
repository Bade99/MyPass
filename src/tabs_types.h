#pragma once
#include "animation.h"

namespace tabs {
	constexpr auto& wndclass = wndclass_name("tabs");
	constexpr size_t no_selection = (size_t)-1;

	using OnSelectionChanged = void(*)(void* user_data, HWND wnd, size_t index);

	struct Theme {
		union {
			struct {
				BrushGroup bk, foreground, accent;
			};
			BrushGroup all[3];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		HFONT font = nil;
		HCURSOR cursor = nil;

		bool copy_from(const Theme& src) {
			bool repaint = false;
			_theme_copy_all_brushes(src.brushes, this->brushes);
			_theme_copy_pointer(src.font, this->font);
			_theme_copy_pointer(src.cursor, this->cursor);
			return repaint;
		}
	};

	struct Item { str label; i32 width = 0; };
	struct UnderlineBounds { i32 left = 0, right = 0; };
	struct UnderlineAnimation {
		animation::Timeline timeline;
		UnderlineBounds from, to, current;
	};

	struct State : WindowState {
		Theme theme;
		std::vector<Item> items;
		OnSelectionChanged on_selection_changed = nil;
		void* user_data = nil;
		size_t selected = no_selection;
		size_t hovered = no_selection;
		i32 scroll_x = 0;
		i32 content_width = 0;
		bool tracking_mouse = false;
		UnderlineAnimation underline;
	};
}
