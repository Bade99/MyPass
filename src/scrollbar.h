#pragma once
#include "win_sdk.h"
#include "global.h"
#include "helpers.h"

/*INFO:
	- The Scrollbar decides its position and size on its own based on its parent and whether it's horizontal or vertical
	- The Scrollbar is only usable with edit controls (for now)
	- IMPORTANT: the parent must use WS_CLIPCHILDREN to avoid horrible flickering
*/

/*TODOs
TODO: boolean option to enable overscrolling, some controls can allow it, like well made text editors that allow you to overscroll at the bottom, while other controls like more standard lists would not want to overscroll and instead want to stop specifically on the last element and not allow to go any further
TODO: should Placement be part of the Theme?
*/

namespace scrollbar {

void set_stats(HWND wnd, u32 rangemax, u32 pagesz, u32 pos) {
	SendMessage(wnd, custom_message::SET_RANGEMAX, rangemax, 0);
	SendMessage(wnd, custom_message::SET_PAGESZ, pagesz, 0);
	SendMessage(wnd, custom_message::SET_POS, pos, 0);
}

void resize_wnd(State& state, i32 scrollbar_thickness) {
	RECT r; GetClientRect(state.parent, &r);
	i32 spacing = 2;// A few pixels of spacing so the control doesnt feel so stuck to the corners

	rect_i32 scroll;

	switch (state.placement) {
	//vertical
	case Placement::left:
	{
		scroll.x = spacing;
		scroll.y = spacing;
		scroll.w = scrollbar_thickness;
		scroll.h = RECTH(r) - spacing * 2;
	}break;
	case Placement::right:
	{
		scroll.x = RECTW(r) - scrollbar_thickness - spacing;
		scroll.y = spacing;
		scroll.w = scrollbar_thickness;
		scroll.h = RECTH(r) - spacing * 2;
	}break;
	//horizontal
	case Placement::top:
	{
		scroll.x = spacing;
		scroll.y = spacing;
		scroll.w = RECTW(r) - spacing * 2;
		scroll.h = scrollbar_thickness;
	}break;
	case Placement::bottom:
	{
		scroll.x = spacing;
		scroll.y = RECTH(r) - scrollbar_thickness - spacing;
		scroll.w = RECTW(r) - spacing * 2;
		scroll.h = scrollbar_thickness;
	}break;
	}
	//RECT clipped_rc = clip_fit_childs(state.parent, state.wnd, scroll.to_RECT());

	MoveWindow(state.wnd, scroll);
}

bool is_vertical(State& state) {
	return state.placement == Placement::left || state.placement == Placement::right;
}

void resize_controls(State& state) {
	RECT rc; GetClientRect(state.wnd, &rc);
	rect_i32 r = rect_i32::create_from(rc);
	auto dim = minimum(r.w, r.h);

	rect_i32 btn_up, btn_down;
	if (is_vertical(state)) {
		btn_up = r.cut_top(dim);
		btn_down = r.cut_bottom(dim);
	}
	else {
		btn_up = r.cut_left(dim);
		btn_down = r.cut_right(dim);
	}

	MoveWindow(state.controls.btn_up, btn_up);
	MoveWindow(state.controls.btn_down, btn_down);
}

RECT get_scrollbar_work_area(State& state) {
	RECT res{}; GetClientRect(state.wnd, &res);
	RECT btn{}; GetClientRect(state.controls.btn_up, &btn);
	if (is_vertical(state)) {
		auto dim = RECTH(btn);
		res.top += dim;
		res.bottom -= dim;
	}
	else {
		auto dim = RECTW(btn);
		res.left += dim;
		res.right -= dim;
	}
	return res;
}

i32 get_max_sb_pos(const State& state) {
	i32 res = state.range_max - state.page_sz;
	return res;
}

i32 clamp_pos(const State& state, i32 pos) {
	i32 res = clamp(0, pos, get_max_sb_pos(state));
	return res;
}

/**
  * Result is in client coordinates
  */
RECT calc_scrollbar(State& state) {
	RECT work_rc = get_scrollbar_work_area(state);

	f32 sb_pos = safe_ratio0<f32>(state.pos, state.range_max);
	f32 sb_sz = clamp01(safe_ratio0<f32>(state.page_sz, state.range_max));
	bool vertical = is_vertical(state);
	f32 work_extent = vertical ? RECTH(work_rc) : RECTW(work_rc);
	f32 sb_lenght = sb_sz * work_extent;

	f32 cursor_height = DPI(GetSystemMetrics(SM_CYCURSOR));
	if (sb_sz != 0) sb_lenght = maximum(sb_lenght, cursor_height);

	RECT sb_rc;
	if (vertical) {
		sb_rc.left = work_rc.left;
		sb_rc.right = work_rc.right;
		sb_rc.top = (i32)(work_rc.top + sb_pos * work_extent);
		if (sb_rc.top + sb_lenght > work_rc.bottom) sb_rc.top = work_rc.bottom - sb_lenght;
		sb_rc.bottom = (i32)(sb_rc.top + sb_lenght);
	}
	else {
		sb_rc.left = (i32)(work_rc.left + sb_pos * work_extent);
		if (sb_rc.left + sb_lenght > work_rc.right) sb_rc.left = work_rc.right - sb_lenght;
		sb_rc.right = (i32)(sb_rc.left + sb_lenght);
		sb_rc.top = work_rc.top;
		sb_rc.bottom = work_rc.bottom;
	}

	return sb_rc;
}

bool is_bar_visible(State& state) {
	bool res = state.range_max > state.page_sz;
	return res;
}

void _update_window_visibility(State& state) {
	bool old_visibility = IsWindowVisible(state.wnd);
	bool new_visibility = is_bar_visible(state);
	if (old_visibility != new_visibility) 
		ShowWindow(state.wnd, new_visibility ? SW_SHOW : SW_HIDE);
}
void update_window_visibility(State& state) { if (state.autohide) _update_window_visibility(state); }

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_theme(HWND wnd, const Theme& src) { 
	_control_create_function__set_theme;

	if (repaint) {
		State& state = *get_state(wnd);

		auto hollow_brush = GetStockBrush(HOLLOW_BRUSH);

		button::Theme btn{};
		btn.dimensions.border_thickness = 0;
		for (auto& b : btn.brushes.bk.all) b = hollow_brush;
		for (auto& b : btn.brushes.border.all) b = hollow_brush;
		btn.brushes.foreground = state.theme.brushes.bar_bk.normal != hollow_brush ? state.theme.brushes.bar_bk : state.theme.brushes.bar_border;

		if (is_vertical(state)) {
			btn.bmp = bmps.solid_arrow_up;
			button::set_theme(state.controls.btn_up, btn);
			btn.bmp = bmps.solid_arrow_down;
			button::set_theme(state.controls.btn_down, btn);
		}
		else {
			btn.bmp = bmps.solid_arrow_left;
			button::set_theme(state.controls.btn_up, btn);
			btn.bmp = bmps.solid_arrow_right;
			button::set_theme(state.controls.btn_down, btn);
		}
	}
}

HWND get_parent_to_notify(State& state) {
	return state.manager_parent ? state.manager_parent : state.parent;
}

void add_controls(State& state) {
	auto& controls = state.controls;

	{
		auto& control = state.controls.btn_up;
		control = create_window(state.wnd, button::wndclass, nil, WS_CHILD | BS_BITMAP);
		button::set_user_data(control, &state);
		button::set_functions(control, {
			.on_click = [](void* data, HWND wnd) {
				auto& state = *(State*)data;
				auto parent = get_parent_to_notify(state);
				if (is_vertical(state))
					SendMessage(parent, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), (LPARAM)state.wnd);
				else 
					SendMessage(parent, WM_HSCROLL, MAKELONG(SB_LINELEFT, 0), (LPARAM)state.wnd);
			}
		});
	}

	{
		auto& control = state.controls.btn_down;
		control = create_window(state.wnd, button::wndclass, nil, WS_CHILD | BS_BITMAP);
		button::set_user_data(control, &state);
		button::set_functions(control, {
			.on_click = [](void* data, HWND wnd) {
				auto& state = *(State*)data;
				auto parent = get_parent_to_notify(state);
				if (is_vertical(state))
					SendMessage(parent, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), (LPARAM)state.wnd);
				else
					SendMessage(parent, WM_HSCROLL, MAKELONG(SB_LINERIGHT, 0), (LPARAM)state.wnd);
			}
		});
	}
}

void update_btn_visibility(State& state) {
	auto set_visibility = [](HWND wnd, bool new_visibility) {
		if (IsWindowVisible(wnd) != new_visibility) ShowWindow(wnd, new_visibility ? SW_SHOW : SW_HIDE);
	};
	bool new_visibility = state.onLMouseClickBk || state.onMouseOverSb || state.onMouseTrackingSb || state.onMouseOverControl;

	for (auto& btn : state.controls.all) set_visibility(btn, new_visibility);
}

void send_bk_click_message(State& state, POINT mouse, RECT sb_rc) {
	auto parent = get_parent_to_notify(state);
	if (is_vertical(state)) {
		if (mouse.y < sb_rc.top) //mouse hit above the bar
			SendMessage(parent, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), (LPARAM)state.wnd);
		else //mouse hit below the bar
			SendMessage(parent, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), (LPARAM)state.wnd);
	}
	else {
		if (mouse.x < sb_rc.left) //mouse hit left of the bar
			SendMessage(parent, WM_HSCROLL, MAKELONG(SB_PAGELEFT, 0), (LPARAM)state.wnd);
		else //mouse hit right of the bar
			SendMessage(parent, WM_HSCROLL, MAKELONG(SB_PAGERIGHT, 0), (LPARAM)state.wnd);
	}
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	auto& state = *get_state(hwnd); _control_validate_state;
	switch (msg) {
	case WM_MOUSEWHEEL:
	{
		return SendMessage(get_parent_to_notify(state), msg, wparam, lparam);
	} break;
	case WM_CANCELMODE:
	{
		if (state.onMouseTrackingSb) {
			ReleaseCapture();
			state.onMouseTrackingSb = false;
		}
		state.onLMouseClickBk = false;
		state.onMouseOverSb = false;
		state.onMouseOverControl = false;
		update_btn_visibility(state);
		ask_window_for_repaint(state.wnd);
	} break;
	case WM_CAPTURECHANGED:
	{
		//We lost mouse capture
		ask_window_for_repaint(state.wnd);
	} break;
	case WM_LBUTTONUP:
	{
		if (state.onMouseTrackingSb) {
			ReleaseCapture();
			state.onMouseTrackingSb = false;
		}
		state.onLMouseClickBk = false;
		update_btn_visibility(state);
	} break;
	case WM_LBUTTONDOWN:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		
		if (state.onMouseOverSb) {
			//Click happened inside the bar, so we want to capture the mouse movement in case the user starts moving the mouse trying to scroll

			if (!state.onMouseTrackingSb) { //Check that we are not already tracking to prevent shadow clicks from affecting us
				SetCapture(state.wnd);//Keep capturing the mouse while the user is still pressing the button, even if the mouse leaves our client area
				state.onMouseTrackingSb = true;

				state.mouse_start_p = is_vertical(state) ? mouse.y : mouse.x;
				state.stored_pos = state.pos;
			}
		}
		else {
			//Mouse is inside the background
			//TODO(fran): make sure it's inside the bk, cause when we are tracking we may still get this msg from outside the client area
			state.onLMouseClickBk = true;

			//Notify parent
			RECT sb_rc = calc_scrollbar(state);
			send_bk_click_message(state, mouse, sb_rc);
			//Start timer to check if the user wants to continue scrolling
			static void (*bk_scroll_proc)(HWND, UINT, UINT_PTR, DWORD) = [](HWND wnd, UINT, UINT_PTR anim_id, DWORD) {
				KillTimer(wnd, anim_id);
				auto& state = *get_state(wnd);
				if (&state && state.onLMouseClickBk) {
					//Check the mouse is still in the bk area, it could be that it moved away or that the bar reached the timer's position
					POINT mouse; GetCursorPos(&mouse); ScreenToClient(state.wnd, &mouse);
					RECT client_rc; GetClientRect(state.wnd, &client_rc);
					RECT sb_rc = calc_scrollbar(state);
					if (test_pt_rc(mouse, client_rc) && !test_pt_rc(mouse, sb_rc)) {
						//Mouse is in bk area
						send_bk_click_message(state, mouse, sb_rc);
						SetTimer(state.wnd, anim_id, USER_TIMER_MINIMUM, bk_scroll_proc);
					}
				}
			};
			SetTimer(state.wnd, timer_id_bk_click_held, 500, bk_scroll_proc);
		}
		update_btn_visibility(state);
		ask_window_for_repaint(state.wnd);
	} break;
	case WM_MOUSELEAVE:
	{
		POINT mouse; GetCursorPos(&mouse); ScreenToClient(state.wnd, &mouse);
		bool prev_onMouseOverSb = state.onMouseOverSb;
		bool prev_onMouseOverControl = state.onMouseOverControl;
		state.onMouseOverSb = test_pt_rc(mouse, calc_scrollbar(state));
		state.onMouseOverControl = test_pt_rc(mouse, get_client_rect(state.wnd));
		bool state_change = prev_onMouseOverSb != state.onMouseOverSb || prev_onMouseOverControl != state.onMouseOverControl;
		if (state_change) {
			ask_window_for_repaint(state.wnd);
			update_btn_visibility(state);
		}
	} break;
	case WM_MOUSEMOVE:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		bool prev_onMouseOverSb = state.onMouseOverSb;
		bool prev_OnMouseTrackingSb = state.onMouseTrackingSb;
		bool prev_onMouseOverControl = state.onMouseOverControl;

		state.onMouseOverSb = test_pt_rc(mouse, calc_scrollbar(state));
		state.onMouseOverControl = test_pt_rc(mouse, get_client_rect(state.wnd));

		if (state.onMouseTrackingSb) {
			//We are tracking the mouse to move the scrollbar
			RECT sb_work_area = get_scrollbar_work_area(state);

			i32 mouse_location;
			f32 sb_extent;
			bool vertical = is_vertical(state);
			if (vertical) {
				mouse_location = mouse.y;
				sb_extent = RECTH(sb_work_area);
			}
			else {
				mouse_location = mouse.x;
				sb_extent = RECTW(sb_work_area);
			}

			f32 displacement = mouse_location - state.mouse_start_p; //pixels
			f32 p_ratio = safe_ratio0(displacement, sb_extent); //percentage of total pixel size of work area
			state.pos = clamp_pos(state, state.stored_pos + (i32)(state.range_max * p_ratio)); //min to max range

			//Notify parent
			SendMessage(get_parent_to_notify(state), vertical ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_THUMBTRACK, state.pos), (LPARAM)state.wnd);
			//TODO(fran): the scroll position for SB_THUMBTRACK is limited to 16bits, we should either extend the message somehow to be able to pass in more data, use a custom message, or use the technique indicated in https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getscrollinfo by standardising that receivers of this notification must call the scrollbar back with GetScrollInfo to get the real 32bit position

			ask_window_for_repaint(state.wnd);
		}

		bool state_change = prev_onMouseOverSb != state.onMouseOverSb || prev_OnMouseTrackingSb != state.onMouseTrackingSb || prev_onMouseOverControl != state.onMouseOverControl;
		if (state_change) {
			ask_window_for_repaint(state.wnd);
			update_btn_visibility(state);
			TRACKMOUSEEVENT track;
			track.cbSize = sizeof(track);
			track.hwndTrack = state.wnd;
			track.dwFlags = TME_LEAVE;
			TrackMouseEvent(&track);
		}
	} break;
	case WM_MOUSEACTIVATE:
	{
		return MA_ACTIVATE;
	} break;
	case WM_NCHITTEST:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		RECT rcWindow; GetWindowRect(state.wnd, &rcWindow);
		LRESULT hittest = test_pt_rc(mouse, rcWindow) ? HTCLIENT : HTNOWHERE; //HTVSCROLL
		return hittest;
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		resize_controls(state);
		return res;
	} break;
	case WM_NCPAINT:
	case WM_NCCALCSIZE: 
	{
	} break;
	case WM_NCCREATE: 
	{
		State* state = (State*)calloc(1, sizeof(State));
		Assert(state);
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)state);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		state->parent = creation_nfo->hwndParent;
		state->wnd = hwnd;
		state->manager_parent = (HWND)creation_nfo->lpCreateParams;
		return 1;
	} break;
	case WM_CREATE:
	{
		add_controls(state);
	} break;
	case custom_message::SET_RANGEMAX:
	{
		int new_range_max = wparam;
		if (state.range_max != new_range_max) {
			state.range_max = new_range_max;
			update_window_visibility(state);
			ask_window_for_repaint(state.wnd);
		}
	} break;
	case custom_message::SET_PAGESZ:
	{
		int new_page_sz = wparam;
		if (state.page_sz != new_page_sz) {
			state.page_sz = new_page_sz;
			update_window_visibility(state);
			ask_window_for_repaint(state.wnd);
		}
	} break;
	case custom_message::SET_POS:
	{
		int new_p = wparam;
		if (state.pos != new_p) {
			state.pos = new_p;
			ask_window_for_repaint(state.wnd);
		}
	} break;
	case WM_ERASEBKGND:
	{
		auto dc = (HDC)wparam;
		RECT r; GetClientRect(state.wnd, &r);
		auto& brushes = state.theme.brushes;
		urender::draw_background(dc, r, brushes.bk.normal, brushes.border.normal, state.theme.dimensions);
		return 1;
	} break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };

		if (is_bar_visible(state)) {
			auto& brushes = state.theme.brushes;
			RECT rc = calc_scrollbar(state);
			HBRUSH bk, border;
			if (state.onMouseOverSb || state.onMouseTrackingSb) {
				border = brushes.bar_border.mouseover;
				bk = brushes.bar_bk.mouseover;
			}
			else {
				border = brushes.bar_border.normal;
				bk = brushes.bar_bk.normal;
			}

			//Paint the bar
			urender::draw_background(dc, rc, bk, border, state.theme.dimensions);
		}
	} break;
	case custom_message::SET_PLACEMENT:
	{
		state.placement = (Placement)wparam;
		SendMessage(state.wnd, custom_message::AUTORESIZE, 0, 0);
	} break;
	case custom_message::AUTORESIZE:
	{
		int scrollbar_thickness = DPI(maximum(GetSystemMetrics(SM_CXVSCROLL) * .8f, 5));
		resize_wnd(state, scrollbar_thickness);
		update_window_visibility(state);
	} break;
	case custom_message::SET_AUTOHIDE:
	{
		bool new_autohide = wparam;
		state.autohide = new_autohide;
		update_window_visibility(state);
	} break;
	case WM_NCDESTROY:
	{
		set_window_state(state.wnd, nil);
		free(&state);
	}break;
	default:
#ifdef _DEBUG_HWND_MESSAGES
		Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	}
	return 0;
}

_init_wndclass_at_startup;
}