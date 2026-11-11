#pragma once

namespace button {

constexpr auto& wndclass = wndclass_name("button");

constexpr auto max_expected_text_length = 256;

typedef void(*func_onclick)(void* data, HWND wnd);

union Functions {
	struct {
		func_onclick on_click;
	};
	void* all[1]{ 0 };
private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
};

struct Theme {
	union {
		struct {
			BrushGroup foreground, bk, border;
			//TODO: try adding a 'selected' brush to the button theme for when it's selected boolean equals true
		};
		BrushGroup all[3];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} brushes;
	union Dimensions {
		struct {
			UINumber border_thickness;
			UINumber border_radius;
			UINumber image_padding;
			UINumber image_ideal_size;
		};
		UINumber all[4];
		Dimensions() : all{} { for (auto& dimension : all) dimension.value = F32INFINITY; }
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} dimensions;
	HFONT font     = nil;
	HICON icon     = nil;
	HBITMAP bmp    = nil;
	HCURSOR cursor = nil;

	bool copy_from(const Theme& src) {
		bool repaint = false;
		_theme_copy_all_brushes(src.brushes, this->brushes);

		repaint |= _theme_copy_all_dimensions(src.dimensions, this->dimensions);

		_theme_copy_pointer(src.font, this->font);
		_theme_copy_pointer(src.icon, this->icon);
		_theme_copy_pointer(src.bmp, this->bmp);
		_theme_copy_pointer(src.cursor, this->cursor);

		return repaint;
	}
};

struct State : WindowState { //NOTE: must be initialized to zero
	bool onMouseOver;
	bool onLMouseClick;//The left click is being pressed on the background area
	bool onKeyboardClick;
	bool OnMouseTracking; //Left click was pressed in our bar and now is still being held, the user will probably be moving the mouse around, so we want to track it to move the scrollbar

	UINT msg_to_send;

	Theme theme;
	Functions functions;

	bool selected; // User can set to true so the button is sticky, stays rendered as 'clicked' even while not being clicked
	
	void* user_data;
};

}
