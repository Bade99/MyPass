#pragma once
#include "win_sdk.h"
#include "Platform.h"
#include "helpers.h"
#include "renderer.h"

//------------------"API"------------------:
//listbox::wndclass identifies the window class to be used when calling CreateWindow()
//listbox::set_brushes() to set the HBRUSHes used
//listbox::set_render_function() to provide a rendering function for all elements of the list
//listbox::set_user_extra() extra user-defined data to be sent on each function call

//--------------"Notifications"---------------: (through WM_COMMAND)
//IMPORTANT: this listbox always sends notifications
//LBN_CLK: notifies whenever the user has clicked an item on the listbox, IMPORTANT: the current click remains only while on the WM_COMMAND response, meaning you must call listbox::get_clicked_element() there, it wont be correct later
//LBN_SELCHANGE: the selection has changed (eg by calling listbox::sel_up())

//------------------"Styles"------------------:
#define LSB_ROUNDRECT_END 0x0001 //Control has rounded ending corners (eg if it's displayed below another wnd then the top corners remain squared while the bottom ones get rounded
//TODO(fran): try to implement this, we'll need ws_ex_layered or ws_ex_transparent

//TODO(fran): rendering: there's something clearly screwed up, when you hover with the mouse along many elements you can see a vertical cut as it changes from one to another, wtf is that? also it doesnt always happen
#define LBN_CLK (LBN_KILLFOCUS+10) //NOTE: LBN_KILLFOCUS is the last default listbox notif

namespace listbox {

	State* get_state(HWND wnd) { _control_create_function__get_state }
	void render_backbuffer(State& state);

	void set_user_extra(HWND wnd, void* user_extra) {
		State& state = *get_state(wnd);
		if (&state) {
			state.user_extra = user_extra;
			render_backbuffer(state);
			ask_window_for_repaint(state.wnd);
		}
	}

	//NOTE: "sz" should be the already precalculated size to cover all the render elements, and maxing out in its w component by the size of the window rectangle (since we allow vertical but not horizontal scrolling)
	void resize_backbuffer(State& state, SIZE sz) {
		RECT rc; GetClientRect(state.wnd, &rc);
		state.backbuffer_dim = { maximum(sz.cx, RECTW(rc)), maximum(sz.cy, RECTH(rc)) };
		HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
		if (state.backbuffer) DeleteBitmap(state.backbuffer);
		state.backbuffer = CreateCompatibleBitmap(dc, state.backbuffer_dim.cx, state.backbuffer_dim.cy);
	}

	void render_element_at(State& state, HDC dc, size_t element_index) {
		//TODO(fran): we could apply a transform so the user can render as if from {0,0} and we simply send a SIZE obj and they dont have to bother with offsetting by left and top
		rect_i32 elem_rc{
			0,
			(i32)element_index * state.element_dim.cy,
			state.element_dim.cx,
			state.element_dim.cy
		};
		//TODO(fran): offset rectangle by state.border_thickness

		RenderFlags flags;
		flags.on_clicked = state.clicked_element == element_index;
		flags.on_selected = state.selected_element == element_index;
		flags.on_mouseover = state.mouse_hover_element == element_index;

		state.render_element(dc, elem_rc, flags, state.elements[element_index], state.user_extra);
	}

	void render_backbuffer(State& state, bool window_enabled) {
		render_diagnostics::Scope diagnostic{ render_diagnostics::Event::list_backbuffer };
		if (state.backbuffer) {
			HDC& dc = state.offscreen_dc;
			auto oldbmp = SelectBitmap(dc, state.backbuffer); defer{ SelectBitmap(dc,oldbmp); };

			RECT backbuffer_rc{ 0,0,state.backbuffer_dim.cx,state.backbuffer_dim.cy };

			//Paint the background
			HBRUSH bk_br = !window_enabled && state.brushes.bk_disabled ? state.brushes.bk_disabled : state.brushes.bk;
			FillRect(dc, &backbuffer_rc, bk_br);

			//NOTE: do _not_ paint the border here, since it should fit the real size of the wnd

			//Render the elements
			//TODO(fran): we should have a list of changed elems so we know which ones need redrawing, otherwise this wont scale at all
			if (state.render_element) {
				for (size_t i = 0; i < state.elements.size(); i++) render_element_at(state, dc, i);
			}
		}
	}
	void render_backbuffer(State& state) { render_backbuffer(state, IsWindowEnabled(state.wnd)); }

	void render_backbuffer_element(State& state, size_t elem_idx) {
		if (state.backbuffer && elem_idx < state.elements.size()) {
			HDC& dc = state.offscreen_dc;
			auto oldbmp = SelectBitmap(dc, state.backbuffer); defer{ SelectBitmap(dc,oldbmp); };

			//NOTE: do _not_ paint the border here, since it should fit the real size of the wnd

			if (state.render_element) {
				RECT elem_rc = rectWH(0, (i32)elem_idx * state.element_dim.cy, state.element_dim.cx, state.element_dim.cy);
				int saved_dc = SaveDC(dc);
				if (saved_dc) IntersectClipRect(dc, elem_rc.left, elem_rc.top, elem_rc.right, elem_rc.bottom);
				HBRUSH bk_br = !IsWindowEnabled(state.wnd) && state.brushes.bk_disabled ? state.brushes.bk_disabled : state.brushes.bk;
				FillRect(dc, &elem_rc, bk_br);
				render_element_at(state, dc, elem_idx);
				if (saved_dc) RestoreDC(dc, saved_dc);
			}
		}
	}

	size_t mouse_element_at(State& state, POINT mouse) {
		RECT client_rc; GetClientRect(state.wnd, &client_rc);
		if (!PtInRect(&client_rc, mouse) || state.element_dim.cy <= 0) return no_element;

		i64 backbuffer_y = (i64)state.scroll_y + mouse.y;
		if (backbuffer_y < 0) return no_element;
		size_t element = (size_t)backbuffer_y / state.element_dim.cy;
		return element < state.elements.size() ? element : no_element;
	}

	void clear_clicked_element(State& state) {
		size_t old_element = state.clicked_element;
		state.clicked_element = no_element;
		if (old_element != no_element && old_element < state.elements.size()) {
			render_backbuffer_element(state, old_element);
			ask_window_for_repaint(state.wnd);
		}
	}

	void full_backbuffer_redo(State& state) {
		size_t elem_cnt = state.elements.size();
		SIZE sz{ state.element_dim.cx, (i32)elem_cnt * state.element_dim.cy };
		resize_backbuffer(state, sz);
		RECT client_rc; GetClientRect(state.wnd, &client_rc);
		state.scroll_y = clamp(0, state.scroll_y, maximum(0, state.backbuffer_dim.cy - RECTH(client_rc)));
		render_backbuffer(state);
	}

	//NOTE: the caller takes care of deleting the brushes, we dont do it
	//Any null HBRUSH param is ignored and the current one remains
	void set_brushes(HWND wnd, BOOL repaint, HBRUSH bk, HBRUSH border, HBRUSH bk_disabled, HBRUSH border_disabled) {
		auto& state = *get_state(wnd);
		if (&state) {

			if (bk)state.brushes.bk = bk;
			if (bk_disabled)state.brushes.bk_disabled = bk_disabled;

			if (border)state.brushes.border = border;
			if (border_disabled)state.brushes.border_disabled = border_disabled;

			if (repaint) {
				render_backbuffer(state); //re-render with new colors
				ask_window_for_repaint(state.wnd);
			}
		}
	}

	void set_parent(HWND wnd, HWND foster_parent) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.foster_parent = foster_parent;
		}
	}

	void set_function_render(HWND wnd, listbox_func_renderelement func) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.render_element = func;
			render_backbuffer(state); //re-render with new element rendering function
			ask_window_for_repaint(state.wnd);
		}
	}

	void set_function_on_click(HWND wnd, listbox_func_on_click func) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.on_click = func;
		}
	}

	struct Dimensions {
		//Border for the outside of the listbox
		int border_thickness = INT32_MAX;
		//Height of one element (all elements have the same width and height)
		int element_h = INT32_MAX;
		Dimensions& set_border_thickness(int x) { border_thickness = x; return *this; }
		Dimensions& set_element_h(int x) { element_h = x; return *this; }
	};

	void set_dimensions(HWND wnd, Dimensions dims) {
		auto& state = *get_state(wnd);
		if (&state) {
			bool redo_backbuffer = false;
			if (dims.border_thickness != INT32_MAX && dims.border_thickness != state.border_thickness) {
				state.border_thickness = dims.border_thickness;
				redo_backbuffer = true;
			}
			if (dims.element_h != INT32_MAX && dims.element_h != state.element_dim.cy) {
				state.element_dim.cy = dims.element_h;
				redo_backbuffer = true;
			}
			if (redo_backbuffer) {
				full_backbuffer_redo(state);
			}
		}
	}

	void notify_parent(State& state, WORD notif) {
		if (notif == LBN_CLK && state.on_click) state.on_click(state.elements[state.clicked_element], state.user_extra);
		else SendMessage(state.parent ? state.parent : state.foster_parent, WM_COMMAND, MAKELONG(0, notif), (LPARAM)state.wnd);
	}

	void clear_selection(State& state) {
		state.selected_element = state.mouse_hover_element = state.clicked_element = state.stored_clicked_element = no_element;
		//TODO(fran): not sure whether I want to clear everything or just the selected_element
		notify_parent(state, LBN_SELCHANGE);
	}

	void clear_selected_noNotify(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.selected_element = no_element;
		}
	}

	//Removes any elements already present and sets the new ones
	void set_elements(HWND wnd, void** values, size_t count) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.elements.clear();//delete old elements
			clear_selection(state);
			for (int i = 0; i < count; i++) state.elements.push_back(values[i]);
			full_backbuffer_redo(state); //render new elements
			ask_window_for_repaint(state.wnd);
		}
	}

	//Adds new elements to the list without removing already present ones
	void add_elements(HWND wnd, void** values, size_t count) {
		auto& state = *get_state(wnd);
		if (&state) {
			for (size_t i = 0; i < count; i++) state.elements.push_back(values + i);
			full_backbuffer_redo(state); //render new elements
			ask_window_for_repaint(state.wnd);
		}
	}

	//Adds new elements to the list without removing already present ones
	//add_elements copies the contiguous pointers to the values passed in, meaning those memory addresses need to stay valid afterwards, and are used to access the element's data. Meanwhile, this function lets you copy a list of values, eg: 
	// auto values = [1,2,3] -> add_elements_by_value(values, 3) -> [1,2,3]
	// static auto values = [1,2,3] -> add_elements(values, 3) -> [&values[0],&values[1],&values[2]]
	void add_elements_by_value(HWND wnd, void** values, size_t count) {
		auto& state = *get_state(wnd);
		if (&state) {
			for (int i = 0; i < count; i++) state.elements.push_back((void*)*(u64*)(values + i));
			full_backbuffer_redo(state); //render new elements
			ask_window_for_repaint(state.wnd);
		}
	}

	void remove_all_elements(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.elements.clear();
			clear_selection(state);
			full_backbuffer_redo(state);
			ask_window_for_repaint(state.wnd);
		}
	}

	//IMPORTANT TODO(fran): this functions should map to a SendMessage, otherwise we're bypassing the msg queue and an external user cant see what we're doing nor modify it

	size_t get_element_cnt(HWND wnd) {
		size_t res = SendMessage(wnd, LB_GETCOUNT, 0, 0);
		return res;
	}

	void* get_clicked_element(HWND wnd) {
		void* res = 0;
		auto& state = *get_state(wnd);
		if (&state) {
			if (state.clicked_element < state.elements.size())
				res = state.elements[state.clicked_element];
		}
		return res;
	}

	//Gets the user chosen element, either via click or enter key press
	void* get_sel_element(HWND wnd) {
		void* res = 0;
		auto& state = *get_state(wnd);
		if (&state) {
			if (state.stored_clicked_element < state.elements.size())
				res = state.elements[state.stored_clicked_element];
		}
		return res;
	}

	//returns the currently selected item index, if no item is selected this value will be higher than the current element count
	size_t get_cur_sel(HWND wnd) {
		size_t res = SendMessage(wnd, LB_GETCURSEL, 0, 0);
		return res;
	}

	void* get_element_at(HWND wnd, size_t idx) {
		void* res = (decltype(res))SendMessage(wnd, LB_GETITEMDATA, idx, 0);
		return res;
	}

	void scroll_selection_into_view(State& state) {
		RECT client_rc; GetClientRect(state.wnd, &client_rc);
		int view_height = RECTH(client_rc);
		int content_height = state.backbuffer_dim.cy;
		if (view_height <= 0 || content_height <= 0 || state.element_dim.cy <= 0) return;

		int max_offset = maximum(0, content_height - view_height);
		int offset = clamp(0, state.scroll_y, max_offset);
		i64 element_top = (i64)state.selected_element * state.element_dim.cy;
		if (element_top < offset) offset = (int)element_top;
		else if (element_top + state.element_dim.cy > offset + view_height)
			offset = (int)(element_top + state.element_dim.cy - view_height);
		state.scroll_y = clamp(0, offset, max_offset);
	}

	void sel_up(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			if (state.elements.size()) {
				auto old_selected_element = state.selected_element;
				if (state.selected_element != no_element)
					state.selected_element = state.selected_element == 0 ?
					state.elements.size() - 1 : (state.selected_element - 1) % state.elements.size();
				else state.selected_element = state.elements.size() - 1;
				if (old_selected_element != state.selected_element) {
					notify_parent(state, LBN_SELCHANGE);
					render_backbuffer(state);
					scroll_selection_into_view(state);
					ask_window_for_repaint(state.wnd);
				}
			}
		}
	}

	void sel_down(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			if (state.elements.size()) {
				auto old_selected_element = state.selected_element;
				if (state.selected_element != no_element)
					state.selected_element = (state.selected_element + 1) % state.elements.size();
				else state.selected_element = 0;
				if (old_selected_element != state.selected_element) {
					notify_parent(state, LBN_SELCHANGE);
					render_backbuffer(state);
					scroll_selection_into_view(state);
					ask_window_for_repaint(state.wnd);
				}
			}
		}
	}

	void set_sel(HWND wnd, size_t idx) {//TODO(fran): idk if it makes sense that this one is different and sends lbn_clk
		auto& state = *get_state(wnd);
		if (&state) {
			auto old_selected_element = state.selected_element;
			if (idx == no_element) state.selected_element = no_element;
			else if (idx < state.elements.size()) {
				state.selected_element = idx;
				state.clicked_element = idx;
				state.stored_clicked_element = state.clicked_element;
				notify_parent(state, LBN_CLK);
				state.clicked_element = no_element;
			}
			if (old_selected_element != state.selected_element) {
				render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}
		}
	}

	void set_sel_noNotify(HWND wnd, size_t idx) {
		auto& state = *get_state(wnd);
		if (&state) {
			auto old_selected_element = state.selected_element;
			if (idx == no_element) state.selected_element = no_element;
			else if (idx < state.elements.size()) {
				state.selected_element = idx;
				state.stored_clicked_element = idx;
				state.clicked_element = no_element;
			}
			if (old_selected_element != state.selected_element) {
				render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}
		}
	}

	LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		//printf("LISTBOX:%s\n",msgToString(msg));
		State& state = *get_state(hwnd);
		switch (msg) {
		case WM_NCCREATE:
		{
			CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

			State* st = (State*)calloc(1, sizeof(State));
			Assert(st);
			set_window_state(hwnd, st);
			st->wnd = hwnd;
			st->parent = creation_nfo->hwndParent;//NOTE: we could also be a floating wnd and parent be null
			//Assert(st->parent == 0);//NOTE: we only handle floating listboxes for now
			st->elements = decltype(st->elements)();

			st->selected_element = st->mouse_hover_element = st->clicked_element = st->stored_clicked_element = no_element;

			HDC __front_dc = GetDC(st->wnd); defer{ ReleaseDC(st->wnd,__front_dc); };
			st->offscreen_dc = CreateCompatibleDC(__front_dc);

			//TODO(fran): check styles

			return TRUE; //continue creation
		} break;
		case WM_SIZE:
		{
			render_diagnostics::Scope diagnostic{ render_diagnostics::Event::list_resize };
			RECT rc; GetClientRect(state.wnd, &rc);
			state.element_dim.cx = RECTW(rc);
			full_backbuffer_redo(state);
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_CREATE:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_MOVE:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_WINDOWPOSCHANGING:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_WINDOWPOSCHANGED:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_SHOWWINDOW:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_DESTROY:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_NCCALCSIZE: 
		case WM_NCPAINT:
		case WM_ERASEBKGND:
		{
			return 0;
		} break;
		case WM_SETFONT:
		{
			HFONT font = (HFONT)wparam;
			BOOL redraw = LOWORD(lparam);
			//state.font = font;
			render_backbuffer(state);
			if (redraw) ask_window_for_repaint(state.wnd);
			return 0;
		} break;
		case WM_GETFONT:
		{
			//return (LRESULT)state.font;
			return 0;
		} break;
		case WM_NCDESTROY:
		{
			if (&state) {
				if (state.backbuffer) { DeleteBitmap(state.backbuffer); state.backbuffer = 0; }
				state.elements.~vector();
				DeleteDC(state.offscreen_dc);
				set_window_state(state.wnd, nil);
				free(&state);
			}
			return 0;
		}break;
		case WM_NCHITTEST:
		{
			return handle_wm_nchittest(state.wnd, lparam);
		} break;
		case WM_SETCURSOR:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_MOUSEMOVE:
		{
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

			size_t mouse_hover_element = mouse_element_at(state, mouse);

			if (state.mouse_hover_element != mouse_hover_element) {
				size_t old_element = state.mouse_hover_element;

				state.mouse_hover_element = mouse_hover_element;

				if (old_element != no_element && old_element < state.elements.size()) render_backbuffer_element(state, old_element);
				if (state.mouse_hover_element != no_element && state.mouse_hover_element < state.elements.size()) render_backbuffer_element(state, state.mouse_hover_element);
				ask_window_for_repaint(state.wnd);

			//We now need to know when the mouse exits our window to clear the mouse_hover_element
				TRACKMOUSEEVENT track;
				track.cbSize = sizeof(track);
				track.hwndTrack = state.wnd;
				track.dwFlags = TME_LEAVE;
				TrackMouseEvent(&track);
			}

			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_MOUSELEAVE:
		{
			//we asked TrackMouseEvent for this so we can update when the mouse leaves our client area, which we dont get notified about otherwise and is needed, for example when the user hovers on top an element and then hovers outside the wnd
			//Clear the mouse_hover_element
			if (state.mouse_hover_element != no_element) {
				size_t old_element = state.mouse_hover_element;
				state.mouse_hover_element = no_element;
				if (old_element != no_element && old_element < state.elements.size()) render_backbuffer_element(state, old_element);
				ask_window_for_repaint(state.wnd);
			}
			return 0;
		} break;
		case WM_MOUSEACTIVATE://When the user clicks on us this is 1st msg received
		{
			if (GetWindowLongPtr(state.wnd, GWL_STYLE) & WS_CHILD)
				return DefWindowProc(hwnd, msg, wparam, lparam);
			HWND parent = (HWND)wparam;
			WORD hittest = LOWORD(lparam);
			WORD mouse_msg = HIWORD(lparam);
			return MA_NOACTIVATE; //Activate our window and post the mouse msg
		} break;
		case WM_LBUTTONDOWN:
		{
			//wparam = test for virtual keys pressed
			if (GetWindowLongPtr(state.wnd, GWL_STYLE) & WS_CHILD) SetFocus(state.wnd);
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

			size_t clicked_element = mouse_element_at(state, mouse);

			if (state.clicked_element != clicked_element) {
				clear_clicked_element(state);
				state.clicked_element = clicked_element;

				if (state.clicked_element != no_element && state.clicked_element < state.elements.size()) render_backbuffer_element(state, state.clicked_element);
				ask_window_for_repaint(state.wnd);
			}
			if (clicked_element != no_element) SetCapture(state.wnd);

			return 0;
		} break;
		case WM_LBUTTONUP:
		{
			//NOTE: usually this kind of control registers a click on btn release

			//wparam = test for virtual keys pressed
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

			size_t unclicked_element = mouse_element_at(state, mouse);

			//Check the user released the click on the same element, otherwise they're canceling it
			if (state.clicked_element != no_element && state.clicked_element == unclicked_element && state.clicked_element < state.elements.size()) {
				//IMPORTANT: this must be sent via SendMessage since the selection/click will be cleared afterwards
				size_t old_selected_element = state.selected_element;
				state.selected_element = state.stored_clicked_element = state.clicked_element;
				if (old_selected_element != no_element && old_selected_element != state.selected_element)
					render_backbuffer_element(state, old_selected_element);
				notify_parent(state, LBN_CLK);
			}
			//Clear the clicked element since the mouse btn is now up
			clear_clicked_element(state);
			if (GetCapture() == state.wnd) ReleaseCapture();

			return 0;
		} break;
		case WM_IME_SETCONTEXT://Sent after SetFocus to us
		{
			BOOL is_active = (BOOL)wparam;
			u32 disp_flags = (u32)lparam;
			return 0; //We dont want IME
		}break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			RECT rc; GetClientRect(state.wnd, &rc);
			int w = RECTW(rc), h = RECTH(rc);
			int border_thickness = state.border_thickness;
			HDC dc = BeginPaint(state.wnd, &ps);
			HDC& offscreen_dc = state.offscreen_dc;
			bool window_enabled = IsWindowEnabled(state.wnd);
			LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
			int backbuf_x = 0;
			int backbuf_y = state.scroll_y;
			Assert(state.backbuffer_dim.cx == w);
			int backbuf_w = w;
			int backbuf_h = h;
			if (state.backbuffer) {
				auto oldbmp = SelectBitmap(offscreen_dc, state.backbuffer); defer{ SelectBitmap(offscreen_dc,oldbmp); };

				BitBlt(dc, 0, 0, w, h, offscreen_dc, backbuf_x, backbuf_y, SRCCOPY);
			}
			// Border

			HBRUSH border_br = !window_enabled && state.brushes.border_disabled ? state.brushes.border_disabled : state.brushes.border;
			FillRectBorder(dc, rc, border_thickness, border_br, BORDERALL);

			EndPaint(hwnd, &ps);
			return 0;
		} break;
		case WM_MOUSEWHEEL:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);//propagates the msg to the parent
		} break;
		case WM_GETTEXT:
		{
			return 0;
		} break;
		case WM_GETTEXTLENGTH:
		{
			return 0;
		} break;
		case WM_SETTEXT:
		{
			return TRUE;//we "set" the text
		} break;
		case WM_ACTIVATEAPP://Sent when you get activated or deactivated (for floating listbox)
		{
			BOOL activated = (BOOL)wparam;
			//lparam == the other programs' thread id
			return 0;
		} break;
		case WM_UAHDESTROYWINDOW:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);//NOTE: received because of DestroyWindow()
		} break;
		case WM_NCACTIVATE:
		{
			//So basically this guy is another WM_NCPAINT, just do exactly the same, but also here we have the option to paint slightly different if we are deactivated, so the user can see the change
			BOOL active = (BOOL)wparam; //Indicates active or inactive state for a title bar or icon that needs to be changed
			//state.active = active;
			HRGN opt_upd_rgn = (HRGN)lparam; // handle to optional update region for the nonclient area. If set to -1, do nothing
			RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
			return TRUE; //NOTE: it says we can return FALSE to "prevent the change"
		} break;
		case WM_ACTIVATE:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);//TODO(fran): we may not want this default behaviour: If the window is being activated and is not minimized, the DefWindowProc function sets the keyboard focus to the window. If the window is activated by a mouse click, it also receives a WM_MOUSEACTIVATE message.
		} break;
		case WM_SETFOCUS:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_KILLFOCUS:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_KEYUP:
		{
			//NOTE: erroneous input received when using the listbox for the searchbox, while the user writes on the editbox we get keyboard focus and therefore a keyup that should have gone to the editbox
			//NOTE: for key events in this control we should handle on WM_KEYDOWN _not_ up
			return 0;
		}
		case WM_GETDLGCODE:
		{
			return GetWindowLongPtr(state.wnd, GWL_STYLE) & WS_CHILD ? DLGC_WANTARROWS : 0;
		}
		case WM_KEYDOWN:
		{
			if (GetWindowLongPtr(state.wnd, GWL_STYLE) & WS_CHILD) {
				if (wparam == VK_UP) { sel_up(state.wnd); return 0; }
				if (wparam == VK_DOWN) { sel_down(state.wnd); return 0; }
			}
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		case WM_CANCELMODE:
		{
			clear_clicked_element(state);
			if (GetCapture() == state.wnd) ReleaseCapture();
			return 0;
		} break;
		case WM_CAPTURECHANGED:
		{
			clear_clicked_element(state);
			return 0;
		} break;
		//---------Responses to the user---------:
		case LB_GETCURSEL:
		{
			return state.selected_element;
		} break;
		case LB_GETCOUNT:
		{
			return state.elements.size();
		} break;
		case LB_GETITEMDATA:
		{
			void* res = 0;
			size_t idx = wparam;
			if (idx < state.elements.size()) res = state.elements[idx];
			return (LRESULT)res;
		} break;
		case WM_ENABLE:
		{
			render_backbuffer(state, wparam != 0);
			ask_window_for_repaint(state.wnd);
			return 0;
		} break;

		default:
#ifdef _DEBUG_HWND_MESSAGES
			printf("LISTBOX UNHANDLED:%s\n", msgToString(msg));
			if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
				TCHAR arr[256];
				int res = GetClipboardFormatName(msg, arr, 256);
				cstr_printf(arr); printf("\n");
				//After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive "MSIMEQueryPosition"
				return DefWindowProc(hwnd, msg, wparam, lparam);
			}
			//Assert(0);
#else 
			return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
		}
		return 0;
	}

	_init_wndclass_at_startup;
}
