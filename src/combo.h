#pragma once
#include "win_sdk.h"
#include "platform.h"
#include "helpers.h"
#include "button.h"
#include "list.h"

//TODO(fran): I could provide default functionality for some things via always having all the functions set to my defaults, when the user changes them we use theirs, if they removed it we use ours, this also removes the branching inefficiency of having to check for valid function pointers before every call
//TODO(fran): provide element index in rendering functions
//TODO(fran): provide a way to store the default text, maybe a user definable function that receives the str provided by wm_setdefaulttext and decides how to store and free it. we could use index ((size_t)-1) to let the user know when it's the default text

namespace combobox {
constexpr auto combobox_base_msg_addr = WM_USER + 3300;
constexpr auto CBM_CLKOUTSIDE = combobox_base_msg_addr + 20; //do not use, internal msg

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_user_extra(HWND wnd, void* user_extra) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.user_extra = user_extra;
		listbox::set_user_extra(state.controls.listbox, user_extra);
		ask_window_for_repaint(state.controls.button);
	}
}

void set_function_on_selection_accepted(HWND wnd, func_on_selection_accepted func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.on_selection_accepted = func;
	}
}

void set_function_desired_size_combobox(HWND wnd, func_desired_size_combobox func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.desired_size_combobox = func;
		if (state.parent) ask_window_for_resize(state.parent);
	}
}

void set_function_render_combobox(HWND wnd, func_render_combobox func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.render_combobox = func;
		ask_window_for_repaint(state.controls.button);
	}
}

void set_function_on_listbox_opening(HWND wnd, func_on_listbox_opening func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.on_listbox_opening = func;
	}
}

void set_function_render_listbox_element(HWND wnd, listbox::listbox_func_renderelement func) {
	auto& state = *get_state(wnd);
	if (&state) {
		listbox::set_function_render(state.controls.listbox, func);
	}
}

//TODO(fran): instead of working as a pasamanos it'd be best to provide an attach_listbox() function with a full configured listbox that abides by our listbox specification
void set_listbox_dimensions(HWND wnd, listbox::Dimensions dims) {
	auto& state = *get_state(wnd);
	if (&state) {
		listbox::set_dimensions(state.controls.listbox, dims);
	}
}

bool is_listbox_open(HWND wnd) {
	bool res = false;

	auto& state = *get_state(wnd);
	if (&state) {
		res = IsWindowVisible(state.controls.listbox);
	}
	return res;
}

auto get_controls(HWND wnd) {
	auto& state = *get_state(wnd);
	if (&state) {
		return state.controls;
	}
	return decltype(state.controls){0};
}

size_t get_count(HWND wnd) {
	size_t res = 0;
	auto& state = *get_state(wnd);
	if (&state) {
		res = SendMessage(state.controls.listbox, LB_GETCOUNT, 0, 0);
	}
	return res;
}

void set_cur_sel(HWND wnd, size_t idx) {
	auto& state = *get_state(wnd);
	if (&state) {
		listbox::set_sel_noNotify(state.controls.listbox, idx);
		ask_window_for_repaint(state.controls.button);
	}
}

//IMPORTANT (multiwnd/multithreading dubious): uses a static object to store the hwnd, since we use it to hide listboxes, and only one is active at any single time this shouldnt be a problem, but you never know
HWND __hookmouseclick_store_hwnd(HWND _wnd = (HWND)INT32_MIN) {
	static HWND wnd{ 0 };
	if (_wnd != (HWND)INT32_MIN) {
		wnd = _wnd;
	}
	return wnd;
}

LRESULT CALLBACK mouse_click_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
	//printf("MOUSECLICKHOOK:0x%08x\n", wparam);
	if (code == HC_ACTION) {
		switch (wparam) {
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_NCLBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
		{
			HWND wnd = __hookmouseclick_store_hwnd();
			auto& state = *get_state(wnd);
			if (&state) {
				//check if the mouse is inside our controls
				POINT mouse = ((MOUSEHOOKSTRUCT*)lparam)->pt;//screen coordinates
				RECT lbrc; GetWindowRect(state.controls.listbox, &lbrc);
				RECT cbrc; GetWindowRect(state.wnd, &cbrc);
				if (!test_pt_rc(mouse, lbrc)) {
					//Mouse clicked outside our control -> hide the listbox
					PostMessage(state.wnd, CBM_CLKOUTSIDE, test_pt_rc(mouse, cbrc), 0);
				}

			}
		}break;
		}
	}
	return CallNextHookEx(0, code, wparam, lparam);
}

LRESULT CALLBACK root_subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR) {
	if ((msg == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE) || msg == WM_CANCELMODE || msg == WM_NCDESTROY) {
		HWND combo = reinterpret_cast<HWND>(subclass_id);
		if (IsWindow(combo)) SendMessage(combo, CBM_CLKOUTSIDE, 0, 0);
	}
	if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, root_subclass_proc, subclass_id);
	return DefSubclassProc(hwnd, msg, wparam, lparam);
}

void show_listbox(State& state, bool show) {
	if (show) {
		RECT rw;  GetWindowRect(state.wnd, &rw);
		i32 w = RECTW(rw), h = RECTH(rw) * (i32)listbox::get_element_cnt(state.controls.listbox);
		i32 popup_y = rw.bottom;
		MONITORINFO monitor_info{ .cbSize = sizeof(MONITORINFO) };
		if (GetMonitorInfo(MonitorFromRect(&rw, MONITOR_DEFAULTTONEAREST), &monitor_info)) {
			i32 screen_margin = DPI(8);
			i32 space_below = maximum(0, monitor_info.rcWork.bottom - screen_margin - rw.bottom);
			i32 space_above = maximum(0, rw.top - monitor_info.rcWork.top - screen_margin);
			if (h > space_below && space_above > space_below) {
				h = minimum(h, space_above);
				popup_y = rw.top - h;
			}
			else h = minimum(h, space_below);
		}
#if 0
		//REMEMBER: SetWindowPos activates the window no matter how many times you tell it not to if you're doing many things like in this case
		SetWindowPos(state.controls.listbox, HWND_NOTOPMOST, rw.left, rw.bottom, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
#else
		SetWindowPos(state.controls.listbox, HWND_NOTOPMOST/*TODO(fran): we may want HWND_TOP or HWND_TOPMOST*/, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE);
		MoveWindow(state.controls.listbox, rw.left, popup_y, w, h, TRUE);
		if (state.on_listbox_opening) state.on_listbox_opening(state.wnd, state.controls.listbox, state.user_extra);
		ShowWindow(state.controls.listbox, SW_SHOWNA);//TODO(fran): make sure it's on top of the z order, at least on top of us
#endif
		SetFocus(state.controls.button);
		//We need to know when the user clicks outside of our editbox/listbox to be able to hide the listbox
		if (!state.impl.mouse_click_hook) {
			//TODO(fran): maybe this should be standard implementation in the listbox
			__hookmouseclick_store_hwnd(state.wnd);
			state.impl.mouse_click_hook = SetWindowsHookEx(WH_MOUSE, mouse_click_hook_proc, 0, GetCurrentThreadId()); //attach the hook
			Assert(state.impl.mouse_click_hook);
		}
		if (!state.impl.root_wnd) {
			HWND root = GetAncestor(state.wnd, GA_ROOT);
			if (root && SetWindowSubclass(root, root_subclass_proc, reinterpret_cast<UINT_PTR>(state.wnd), 0)) {
				state.impl.root_wnd = root;
			}
			else Assert(0);
		}

	}
	else {
		//printf("SEARCHBOX:HIDE LISTBOX\n");
		if (state.impl.root_wnd) {
			RemoveWindowSubclass(state.impl.root_wnd, root_subclass_proc, reinterpret_cast<UINT_PTR>(state.wnd));
			state.impl.root_wnd = 0;
		}
		if (state.impl.mouse_click_hook) {
			UnhookWindowsHookEx(state.impl.mouse_click_hook);
			state.impl.mouse_click_hook = 0;
		}
		ShowWindow(state.controls.listbox, SW_HIDE);
	}
	ask_window_for_repaint(state.controls.button);
}

LRESULT CALLBACK ButtonPaintProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/) {
	HWND parent = GetParent(hwnd);
	auto& button_state = *button::get_state(hwnd);
	auto& combo_state = *combobox::get_state(parent);

	switch (msg) {
	case WM_PAINT:
	{
		if (combo_state.render_combobox) {
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(button_state.wnd, &ps); defer{ EndPaint(button_state.wnd, &ps); };
			RECT rc; GetClientRect(button_state.wnd, &rc);

			RenderFlags flags;
			flags.on_clicked = button_state.onMouseOver && button_state.onLMouseClick;
			//NOTE: on button focus here we mark the button flags as on_mouseover, I think that this is a good solution to avoid having to include an additional render step to render something like what we do with buttons, showing a thick focus border
			flags.on_mouseover = button_state.onMouseOver || button_state.OnMouseTracking || (GetFocus() == button_state.wnd);
			flags.is_enabled = IsWindowEnabled(button_state.wnd);
			flags.is_listbox_open = is_listbox_open(parent);

			combo_state.render_combobox(dc, rect_i32::create_from(rc), flags, listbox::get_sel_element(combo_state.controls.listbox), combo_state.user_extra);
			return 0;
		}
	} break;
	case WM_KEYDOWN:
	{
		if (IsWindowVisible(combo_state.controls.listbox)) {
			if (wparam == VK_UP) { listbox::sel_up(combo_state.controls.listbox); return 0; }
			if (wparam == VK_DOWN) { listbox::sel_down(combo_state.controls.listbox); return 0; }
			if (wparam == VK_ESCAPE) { show_listbox(combo_state, false); return 0; }
		}
	} break;
	case WM_CHAR:
	{
		if (wparam == VK_RETURN && IsWindowVisible(combo_state.controls.listbox)) {
			size_t selected = listbox::get_cur_sel(combo_state.controls.listbox);
			if (selected != listbox::no_element) listbox::set_sel(combo_state.controls.listbox, selected);
			else show_listbox(combo_state, false);
			return 0;
		}
	} break;
	case WM_MOUSEWHEEL:
	{
		if (IsWindowVisible(combo_state.controls.listbox))
			return SendMessage(combo_state.controls.listbox, msg, wparam, lParam);
	} break;
	}

	return DefSubclassProc(hwnd, msg, wparam, lParam);
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	auto& state = *get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{ //1st msg received
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		st->wnd = hwnd;
		st->parent = creation_nfo->hwndParent;
		return TRUE; //continue creation
	} break;
	case WM_CREATE:
	{
		state.controls.button = CreateWindow(button::wndclass, NULL, WS_CHILD | WS_VISIBLE
			, 0, 0, 0, 0, state.wnd, 0, NULL, NULL);
		auto hand_cursor = LoadCursor(nil, IDC_HAND);
		button::Theme btn{}; btn.cursor = hand_cursor;
		button::set_theme(state.controls.button, btn);

		SetWindowSubclass(state.controls.button, ButtonPaintProc, 0, 0);

		state.controls.listbox = CreateWindowEx(WS_EX_TOOLWINDOW /*| WS_EX_TOPMOST*/, listbox::wndclass, 0, WS_POPUP
			, 0, 0, 0, 0, NULL, 0, NULL, NULL);
		listbox::set_parent(state.controls.listbox, state.wnd);

		return DefWindowProc(hwnd, msg, wparam, lparam);//TODO(fran): remove once we know all the things this does
	} break;
	case WM_SIZE: //4th, sent _after_ the wnd has been resized
	{
		//TODO(fran): do I resize the listbox here, or only when I want to show it?
		RECT r; GetClientRect(state.wnd, &r);
		i32 w = RECTW(r), h = RECTH(r);
		MoveWindow(state.controls.button, 0, 0, w, h, FALSE);

		//TODO(fran): this should be set by the user, we need to add the measure listbox so we know the window height they want
		listbox::set_dimensions(state.controls.listbox, listbox::Dimensions().set_border_thickness(1).set_element_h(h));

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DESIRED_SIZE:
	{
		//state.measure_combobox(,state.user_extra);
		SIZE* min = (decltype(min))wparam;
		SIZE* max = (decltype(max))lparam;

		if (state.desired_size_combobox) {
			HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
			return state.desired_size_combobox(min, max, dc, listbox::get_sel_element(state.controls.listbox), state.user_extra);
		}
		else return 0;
	} break;
	case WM_SETFONT:
	{
		HFONT font = (HFONT)wparam;
		BOOL redraw = LOWORD(lparam);
		//SendMessage(state.controls.editbox, msg, wparam, lparam);
		//SendMessage(state.controls.listbox, msg, wparam, lparam);
		if (redraw) ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_NCDESTROY://Last msg. Sent _after_ WM_DESTROY
	{
		if (&state) {
			DestroyWindow(state.controls.listbox);//NOTE: since this isnt a child I dont think it gets automatically destroyed
			set_window_state(state.wnd, nil);
			free(&state);
		}
		return 0;
	}break;
	case WM_DESTROY:
	{
		if (state.impl.root_wnd || state.impl.mouse_click_hook || IsWindowVisible(state.controls.listbox)) {
			show_listbox(state, false);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCHITTEST:
	{
		return handle_wm_nchittest(state.wnd, lparam);
	} break;
	case WM_MOUSEACTIVATE:
	{
		return handle_wm_mouseactivate();
	} break;
	case WM_PARENTNOTIFY:
	{
		WORD event = LOWORD(wparam);
		Assert(event == WM_CREATE || event == WM_DESTROY || event == WM_LBUTTONDOWN || event == WM_MBUTTONDOWN || event == WM_RBUTTONDOWN || event == WM_XBUTTONDOWN);
		return 0;
	} break;
	case WM_COMMAND:
	{
		HWND child = (HWND)lparam;
		if (child) {//Notifs from our childs
			WORD notif = HIWORD(wparam);
			if (child == state.controls.button) {
				if (!state.reject_button) show_listbox(state, true);
				else state.reject_button = false;
			}
			else if (child == state.controls.listbox) {
				switch (notif) {
				case LBN_CLK:
				{
					if (state.on_selection_accepted)
						state.on_selection_accepted(listbox::get_clicked_element(state.controls.listbox), state.user_extra);

					show_listbox(state, false);

				} break;
				case LBN_SELCHANGE: break;
				default: Assert(0);
				}
			}
			else {
				Assert(0);
			}
		}
		else {
			//Menu notifications
			Assert(0);
		}
		return 0;
	} break;
	case CBM_CLKOUTSIDE://The user clicked outside our listbox -> hide the listbox
	{
		state.reject_button = (bool)wparam;
		show_listbox(state, false);
	} break;
	case WM_MOUSEWHEEL:
	{
		if (IsWindowVisible(state.controls.listbox))
			return SendMessage(state.controls.listbox, msg, wparam, lparam);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SETFOCUS:
	{
		SetFocus(state.controls.button);
		return 0;
	} break;
	case WM_SETDEFAULTTEXT:
	case WM_SETTEXT:
	case WM_GETTEXT:
	case WM_GETTEXTLENGTH:
	case WM_KILLFOCUS:
	case WM_NCCALCSIZE:
	case WM_NCPAINT: //we process this message, for now
	case WM_ERASEBKGND: //If you return 0 then on WM_PAINT fErase will be true, aka paint the background there
	case WM_GETFONT: // or SendMessage(state.controls.editbox, msg, wparam, lparam);
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_IME_SETCONTEXT: //We dont want IME
	{
		return 0;
	} break;
	#ifdef _DEBUG_HWND_MESSAGES
	case WM_PAINT:
	case WM_SETCURSOR:
	case WM_MOUSEMOVE:
	case WM_MOVE:
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
	case WM_SHOWWINDOW:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	#endif
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
