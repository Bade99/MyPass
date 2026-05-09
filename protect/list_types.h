#pragma once

namespace listbox {
constexpr auto& wndclass = wndclass_name("listbox");

struct renderflags {
	bool onSelected, onClicked, onMouseover;
};
typedef void(*listbox_func_renderelement)(HDC dc, rect_i32 r, renderflags flags, void* element, void* user_extra);
typedef void(*listbox_func_on_click)(void* element, void* user_extra);

struct State {
	HWND wnd;
	HWND parent;
	HWND foster_parent;
	struct brushes {//TODO(fran): we want the user to _always_ be in charge of drawing, so idk how useful this is to be sent to them
		HBRUSH bk, border;
		HBRUSH bk_disabled, border_disabled;
	}brushes;

	std::vector<void*> elements;//each row will contain data sent by the user

	listbox_func_renderelement render_element;
	listbox_func_on_click on_click;

	SIZE element_dim;//NOTE: all elements are of the same size
	i32 border_thickness;

	size_t selected_element;//idx of the currently selected element
	size_t mousehover_element;//idx of the element currently under the mouse
	size_t clicked_element;//idx of the element that was clicked/pressed enter

	size_t stored_clicked_element;//stores the last user chosen element, either via left click or enter key press

	HDC offscreendc;

	HBITMAP backbuffer;
	SIZE backbuffer_dim;

	f32 scroll_y;//[0,1]

	void* user_extra;
};
}