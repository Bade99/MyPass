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
//TODO(fran): clicking on the listbox should not deactivate the wnd that was previously active

#define LBN_CLK (LBN_KILLFOCUS+10) //NOTE: LBN_KILLFOCUS is the last default listbox notif

//TODO(fran): add extra functionality for non floating listboxes, eg VK_UP and VK_DOWN to change the selection
//TODO(fran): BUG: reproduction: hold click on an element, move outside the window and only then release click, the element will remain in the clicked state, solution: CaptureMouse() probably

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

	State* get_state(HWND wnd) { _control_create_function__get_state }

	void set_user_extra(HWND wnd, void* user_extra) {
		State& state = *get_state(wnd);
		if (&state) {
			state.user_extra = user_extra;
			//TODO(fran): should I redraw?
		}
	}

	//IMPORTANT: the return ptr must not be freed, it points to the actual elements of the listbox
	//ptr<void*> get_all_elements(HWND wnd) {//TODO(fran): this shouldnt exist but I need it in order to be lazy in the searchbox
	//	ptr<void*> res{ 0 };
	//	auto& state = *get_state(wnd);
	//	if (&state) {
	//		if (state.elements.size()) {
	//			res.mem = &state.elements[0];//TODO(fran): this isnt very nice
	//			res.cnt = state.elements.size();
	//		}
	//	}
	//	return res;
	//}

	//NOTE: "sz" should be the already precalculated size to cover all the render elements, and maxing out in its w component by the size of the window rectangle (since we allow vertical but not horizontal scrolling)
	void resize_backbuffer(State& state, SIZE sz) {
		RECT rc; GetClientRect(state.wnd, &rc);
		state.backbuffer_dim = { maximum(sz.cx, RECTW(rc)), maximum(sz.cy, RECTH(rc)) };
		HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
		if (state.backbuffer) DeleteBitmap(state.backbuffer);
		state.backbuffer = CreateCompatibleBitmap(dc, state.backbuffer_dim.cx, state.backbuffer_dim.cy);
	}

	void render_backbuffer(State& state) {
		if (state.backbuffer) {
			HDC& dc = state.offscreendc;
			auto oldbmp = SelectBitmap(dc, state.backbuffer); defer{ SelectBitmap(dc,oldbmp); };

			RECT backbuffer_rc{ 0,0,state.backbuffer_dim.cx,state.backbuffer_dim.cy };
			bool window_enabled = IsWindowEnabled(state.wnd); //TODO(fran): we need to repaint the backbuf if the window is disabled and the dis_... are !=0 and != than the non dis_... brushes

			//Paint the background
#if 0
			HBRUSH bk_br = window_enabled ? state.brushes.bk : state.brushes.bk_dis;
#else
			HBRUSH bk_br = state.brushes.bk;
#endif
			FillRect(dc, &backbuffer_rc, bk_br);

			//NOTE: do _not_ paint the border here, since it should fit the real size of the wnd

			//Render the elements
			//TODO(fran): we should have a list of changed elems so we know which ones need redrawing, otherwise this wont scale at all
			if (state.render_element) {
				for (size_t i = 0; i < state.elements.size(); i++) {
					//TODO(fran): we could apply a transform so the user can render as if from {0,0} and we simply send a SIZE obj and they dont have to bother with offsetting by left and top
					rect_i32 elem_rc{
							backbuffer_rc.left
						, backbuffer_rc.top + (i32)i * state.element_dim.cy
						, state.element_dim.cx
						, state.element_dim.cy
					};
					//TODO(fran): offset rectangle by state.border_thickness

					renderflags flags;
					flags.onClicked = state.clicked_element == i;
					flags.onSelected = state.selected_element == i;
					flags.onMouseover = state.mousehover_element == i;

					state.render_element(dc, elem_rc, flags, state.elements[i], state.user_extra);
				}
			}
		}
	}

	void render_backbuffer_element(State& state, size_t elem_idx) {
		//TODO(fran): this is a quick HACK reusing render_backbuffer()'s code
		if (state.backbuffer) {
			HDC& dc = state.offscreendc;
			auto oldbmp = SelectBitmap(dc, state.backbuffer); defer{ SelectBitmap(dc,oldbmp); };

			RECT backbuffer_rc{ 0,0,state.backbuffer_dim.cx,state.backbuffer_dim.cy };
			bool window_enabled = IsWindowEnabled(state.wnd); //TODO(fran): we need to repaint the backbuf if the window is disabled and the dis_... are !=0 and != than the non dis_... brushes

			//Paint the background
#if 0
			HBRUSH bk_br = window_enabled ? state.brushes.bk : state.brushes.bk_dis;
#else
			HBRUSH bk_br = state.brushes.bk;
#endif
			//FillRect(dc, &backbuffer_rc, bk_br);

			//NOTE: do _not_ paint the border here, since it should fit the real size of the wnd

			//Render the elements
			//TODO(fran): we should have a list of changed elems so we know which ones need redrawing, otherwise this wont scale at all
			if (state.render_element) {
				Assert(elem_idx < state.elements.size()); //BUG(fran): there's some case that causes this to trigger at app startup with elem_idx = 1 & elements.size() = 1. INFO: apparently happens when the mouse is on top of the wnd just as it is being created, causing bad stuff to happen like getting negative mouse positions
				size_t i = elem_idx;
				rect_i32 elem_rc{
						backbuffer_rc.left
					, backbuffer_rc.top + (i32)i * state.element_dim.cy
					, state.element_dim.cx
					, state.element_dim.cy
				};

				renderflags flags;
				flags.onClicked = state.clicked_element == i;
				flags.onSelected = state.selected_element == i;
				flags.onMouseover = state.mousehover_element == i;

				state.render_element(dc, elem_rc, flags, state.elements[i], state.user_extra);
			}
		}
	}

	void full_backbuffer_redo(State& state) {
		//SIZE sz = calc_element_placements(state);
		size_t elem_cnt = state.elements.size();
		SIZE sz{ state.element_dim.cx, (i32)elem_cnt * state.element_dim.cy };
		resize_backbuffer(state, sz);
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

	struct dimensions {
		//Border for the outside of the listbox
		int border_thickness = INT32_MAX;
		//Height of one element (all elements have the same width and height)
		int element_h = INT32_MAX;
		dimensions& set_border_thickness(int x) { border_thickness = x; return *this; }
		dimensions& set_element_h(int x) { element_h = x; return *this; }
	};

	void set_dimensions(HWND wnd, dimensions dims) {
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
		state.selected_element = state.mousehover_element = state.clicked_element = state.stored_clicked_element = UINT32_MAX;
		//TODO(fran): not sure whether I want to clear everything or just the selected_element
		notify_parent(state, LBN_SELCHANGE);
	}

	void clear_selected_noNotify(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			state.selected_element = UINT32_MAX;
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
			for (int i = 0; i < count; i++) state.elements.push_back(values + i);
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

	void sel_up(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			if (state.elements.size()) {
				if (state.selected_element != UINT32_MAX)
					state.selected_element = state.selected_element == 0 ?
					state.elements.size() - 1 : (state.selected_element - 1) % state.elements.size();
				else state.selected_element = state.elements.size() - 1;
				//TODO(fran): only re-render if the selected_element changed 
				notify_parent(state, LBN_SELCHANGE);
				render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}
		}
	}

	void sel_down(HWND wnd) {
		auto& state = *get_state(wnd);
		if (&state) {
			if (state.elements.size()) {
				if (state.selected_element != UINT32_MAX)
					state.selected_element = (state.selected_element + 1) % state.elements.size();
				else state.selected_element = 0;
				//TODO(fran): only re-render if the selected_element changed 
				notify_parent(state, LBN_SELCHANGE);
				render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}
		}
	}

	void set_sel(HWND wnd, size_t idx) {//TODO(fran): idk if it makes sense that this one is different and sends lbn_clk
		auto& state = *get_state(wnd);
		if (&state) {
			if (idx == (size_t)-1) state.selected_element = UINT32_MAX;
			else if (idx < state.elements.size()) {
				state.selected_element = idx;
				//TODO(fran): only re-render if the selected_element changed 
				state.clicked_element = idx;
				state.stored_clicked_element = state.clicked_element;
				notify_parent(state, LBN_CLK);
				state.clicked_element = UINT32_MAX;
				render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}
		}
	}

	void set_sel_noNotify(HWND wnd, size_t idx) {
		auto& state = *get_state(wnd);
		if (&state) {
			auto old_selected_element = state.selected_element;
			if (idx == (size_t)-1) state.selected_element = UINT32_MAX;
			else if (idx < state.elements.size()) {
				state.selected_element = idx;
				//TODO(fran): only re-render if the selected_element changed 
				state.stored_clicked_element = idx;
				state.clicked_element = UINT32_MAX;
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

			st->selected_element = st->mousehover_element = st->clicked_element = st->stored_clicked_element = UINT32_MAX;//TODO(fran): use std::limits<size_t>::max()

			HDC __front_dc = GetDC(st->wnd); defer{ ReleaseDC(st->wnd,__front_dc); };
			st->offscreendc = CreateCompatibleDC(__front_dc);

			//TODO(fran): check styles

			return TRUE; //continue creation
		} break;
		case WM_SIZE:
		{
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
				DeleteDC(state.offscreendc);
				set_window_state(state.wnd, nil);
				free(&state);
			}
			return 0;
		}break;
		case WM_NCHITTEST:
		{
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			RECT rw; GetWindowRect(state.wnd, &rw);
			LRESULT hittest = HTNOWHERE;
			if (test_pt_rc(mouse, rw)) hittest = HTCLIENT;
			return hittest;
		} break;
		case WM_SETCURSOR:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_MOUSEMOVE:
		{
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

			if (state.elements.size()) {
				size_t mouse_on_backbuffer_y = (size_t)(state.scroll_y * (f32)state.backbuffer_dim.cy) + mouse.y;
				size_t mousehover_element = mouse_on_backbuffer_y / state.element_dim.cy;

				if (state.mousehover_element != mousehover_element) {
					size_t old_element = state.mousehover_element;

					state.mousehover_element = mousehover_element;

					if (old_element != UINT32_MAX)render_backbuffer_element(state, old_element);
					render_backbuffer_element(state, state.mousehover_element);
					//render_backbuffer(state);
					ask_window_for_repaint(state.wnd);

					//We now need to know when the mouse exits our window to clear the mousehover_element
					TRACKMOUSEEVENT track;
					track.cbSize = sizeof(track);
					track.hwndTrack = state.wnd;
					track.dwFlags = TME_LEAVE;
					TrackMouseEvent(&track);
				}
			}

			return DefWindowProc(hwnd, msg, wparam, lparam);
		} break;
		case WM_MOUSELEAVE:
		{
			//we asked TrackMouseEvent for this so we can update when the mouse leaves our client area, which we dont get notified about otherwise and is needed, for example when the user hovers on top an element and then hovers outside the wnd
			//Clear the mousehover_element
			if (state.mousehover_element != UINT32_MAX) {
				size_t old_element = state.mousehover_element;
				state.mousehover_element = UINT32_MAX;
				render_backbuffer_element(state, old_element);
				//render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}
			return 0;
		} break;
		case WM_MOUSEACTIVATE://When the user clicks on us this is 1st msg received
		{
			HWND parent = (HWND)wparam;
			WORD hittest = LOWORD(lparam);
			WORD mouse_msg = HIWORD(lparam);
			return MA_NOACTIVATE; //Activate our window and post the mouse msg
		} break;
		case WM_LBUTTONDOWN:
		{
			//wparam = test for virtual keys pressed
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

			size_t mouse_on_backbuffer_y = (size_t)(state.scroll_y * (f32)state.backbuffer_dim.cy) + mouse.y;
			size_t clicked_element = mouse_on_backbuffer_y / state.element_dim.cy;

			if (state.clicked_element != clicked_element) {
				state.clicked_element = clicked_element;

				if (state.clicked_element != UINT32_MAX)render_backbuffer_element(state, state.clicked_element);
				//render_backbuffer(state);
				ask_window_for_repaint(state.wnd);
			}

			return 0;
		} break;
		case WM_LBUTTONUP:
		{
			//NOTE: usually this kind of control registers a click on btn release

			//wparam = test for virtual keys pressed
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

			size_t mouse_on_backbuffer_y = (size_t)(state.scroll_y * (f32)state.backbuffer_dim.cy) + mouse.y;
			size_t unclicked_element = mouse_on_backbuffer_y / state.element_dim.cy;

			//Check the user released the click on the same element, otherwise they're canceling it
			if (state.clicked_element == unclicked_element && state.clicked_element < state.elements.size()) {
				//IMPORTANT: this must be sent via SendMessage since the selection/click will be cleared afterwards
				state.stored_clicked_element = state.clicked_element;
				notify_parent(state, LBN_CLK);
			}
			//Clear the clicked element since the mouse btn is now up
			size_t old_clicked_element = state.clicked_element;
			state.clicked_element = UINT32_MAX;
			if (old_clicked_element != UINT32_MAX)render_backbuffer_element(state, old_clicked_element);
			//render_backbuffer(state);
			ask_window_for_repaint(state.wnd);

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
			HDC& offscreendc = state.offscreendc;
			bool window_enabled = IsWindowEnabled(state.wnd);
			//TODO(fran): we need to re-render backbuf if the window is disabled and the dis_... are !=0 and != than the non dis_... brushes
			LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
			int backbuf_x = 0;
			int backbuf_y = (int)(state.scroll_y * (f32)state.backbuffer_dim.cy);
			Assert(state.backbuffer_dim.cx == w);
			int backbuf_w = w;
			int backbuf_h = h;
			if (state.backbuffer) {
				auto oldbmp = SelectBitmap(offscreendc, state.backbuffer); defer{ SelectBitmap(offscreendc,oldbmp); };

				BitBlt(dc, 0, 0, w, h, offscreendc, backbuf_x, backbuf_y, SRCCOPY);
			}
			// Border

			HBRUSH border_br = window_enabled ? state.brushes.border : state.brushes.border_disabled;
			FillRectBorder(dc, rc, border_thickness, border_br, BORDERALL);

			EndPaint(hwnd, &ps);
			return 0;
		} break;
#if 0
		case WM_MOUSEWHEEL:
		{
			//TODO(fran): offset state.scroll_y;
			Assert(0);
			return 0;
		} break;
#else
		case WM_MOUSEWHEEL:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);//propagates the msg to the parent
		} break;
#endif
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
		//case WM_CANCELMODE: //TODO(fran): stop capturing the mouse
		//{
		//	//We got canceled by the system, not really sure what it means but doc says we should cancel everything mouse capture related, so stop tracking
		//	if (state.OnMouseTracking) {
		//		ReleaseCapture();//stop capturing the mouse
		//		state.OnMouseTracking = false;
		//	}
		//	state.onLMouseClick = false;
		//	state.onMouseOver = false;
		//	InvalidateRect(state.wnd, NULL, TRUE);
		//	return 0;
		//} break;
		//---------Responses to the user---------:
		case LB_GETCURSEL:
		{
			return state.selected_element;//TODO(fran): idk if we want to check if state.clicked_element is valid too
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
		//case WM_CANCELMODE:
		//{
		//	TRACKMOUSEEVENT track;
		//	track.cbSize = sizeof(track);
		//	track.hwndTrack = state.wnd;
		//	track.dwFlags = TME_LEAVE | TME_CANCEL;//cancel previous TME_LEAVE request if active //TODO(fran): parametric/automatic way to know what to cancel
		//	TrackMouseEvent(&track);
		//	return 0;
		//} break;
		//case WM_ENABLE:
		//{
		//	BOOL enabled = (BOOL)wparam;//TODO(fran): here we could check whether the enabled state is actually changing and only then ask for repaint
		//	ask_window_for_repaint(state.wnd);
		//	return 0;
		//} break;

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