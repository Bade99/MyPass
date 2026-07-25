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
		//TODO(fran): should I redraw?
	}
}

//void set_function_free_elements(HWND wnd, func_free_elements func) {
//	auto& state = *get_state(wnd);
//	if (&state) {
//		state.free_elements = func;
//		//TODO(fran): should I redraw?
//	}
//}

void set_function_on_selection_accepted(HWND wnd, func_on_selection_accepted func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.on_selection_accepted = func;
		//TODO(fran): should I redraw?
	}
}

void set_function_desired_size_combobox(HWND wnd, func_desired_size_combobox func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.desired_size_combobox = func;
		//TODO(fran): should I redraw?
	}
}

void set_function_render_combobox(HWND wnd, func_render_combobox func) {
	auto& state = *get_state(wnd);
	if (&state) {
		state.render_combobox = func;
		//TODO(fran): should I redraw?
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
void set_listbox_dimensions(HWND wnd, listbox::dimensions dims) {
	auto& state = *get_state(wnd);
	if (&state) {
		listbox::set_dimensions(state.controls.listbox, dims);
	}
}

bool is_listbox_open(HWND wnd) {
	bool res = false;

	auto& state = *get_state(wnd);
	if (&state) {
		res = IsWindowVisible(state.controls.listbox);//TODO(fran): better check
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
		listbox::set_sel_noNotify(state.controls.listbox, idx);//TODO(fran): the code for this in the listbox needs a pass, we're basically calling the listbox so it calls us telling about the change in a different future msg
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

LRESULT CALLBACK hookmouseclick(int code, WPARAM wparam, LPARAM lparam) {
	//printf("MOUSECLICKHOOK:0x%08x\n", wparam);
	if (code == HC_ACTION) {//TODO(fran): WH_MOUSE also includes a HC_NOREMOVE code
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
#if 0
				POINT mouse = ((MSLLHOOKSTRUCT*)lparam)->pt;//TODO(fran): _per-monitor-aware_ screen coordinates (idk if that changes something)
#else
				POINT mouse = ((MOUSEHOOKSTRUCT*)lparam)->pt;//screen coordinates
#endif
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

void show_listbox(State& state, bool show) {
	if (show) {
		RECT rw;  GetWindowRect(state.wnd, &rw);
		i32 w = RECTW(rw), h = RECTH(rw) * (i32)listbox::get_element_cnt(state.controls.listbox);
#if 0
		//REMEMBER: SetWindowPos activates the window no matter how many times you tell it not to if you're doing many things like in this case
		SetWindowPos(state.controls.listbox, HWND_NOTOPMOST, rw.left, rw.bottom, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
#else
		SetWindowPos(state.controls.listbox, HWND_NOTOPMOST/*TODO(fran): we may want HWND_TOP or HWND_TOPMOST*/, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE);
		MoveWindow(state.controls.listbox, rw.left, rw.bottom, w, h, TRUE);
		if (state.on_listbox_opening) state.on_listbox_opening(state.wnd, state.controls.listbox, state.user_extra);
		ShowWindow(state.controls.listbox, SW_SHOWNA);//TODO(fran): make sure it's on top of the z order, at least on top of us
#endif
		//We need to know when the user clicks outside of our editbox/listbox to be able to hide the listbox
		if (!state.impl.hookmouseclick) {
			//TODO(fran): maybe this should be standard implementation in the listbox
			__hookmouseclick_store_hwnd(state.wnd);
			state.impl.hookmouseclick = SetWindowsHookEx(WH_MOUSE, hookmouseclick, 0, GetCurrentThreadId()); //attach the hook
			//state.impl.hookmouseclick = SetWindowsHookEx(WH_MOUSE_LL, hookmouseclick, 0, GetCurrentThreadId()); //TODO(fran): I wanted to use this one but apparently low level hooks cannot be run on debug threads, the docs recommend using raw input instead //https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse
			Assert(state.impl.hookmouseclick);
		}

	}
	else {
		//printf("SEARCHBOX:HIDE LISTBOX\n");
		ShowWindow(state.controls.listbox, SW_HIDE);

		UnhookWindowsHookEx(state.impl.hookmouseclick); // remove the hook
		state.impl.hookmouseclick = 0;
	}
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

			render_flags flags;
			flags.onClicked = button_state.onMouseOver && button_state.onLMouseClick;
			//NOTE: on button focus here we mark the button flags as onMouseover, I think that this is a good solution to avoid having to include an additional render step to render something like what we do with buttons, showing a thick focus border
			flags.onMouseover = button_state.onMouseOver || button_state.OnMouseTracking || (GetFocus() == button_state.wnd);
			flags.isEnabled = IsWindowEnabled(button_state.wnd);
			flags.isListboxOpen = is_listbox_open(parent);

			combo_state.render_combobox(dc, rect_i32::create_from(rc), flags, listbox::get_sel_element(combo_state.controls.listbox), combo_state.user_extra);
			return 0;
		}
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
		CREATESTRUCT* createnfo = (CREATESTRUCT*)lparam;
		LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);

		//TODO(fran): settext is redirected to the editbox
		if (createnfo->lpszName) PostMessage(state.wnd, WM_SETTEXT, 0, (LPARAM)createnfo->lpszName);

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
		listbox::set_dimensions(state.controls.listbox, listbox::dimensions().set_border_thickness(1).set_element_h(h));

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
			//if (child == state.controls.editbox) {
				//switch (notif) {
				//case EN_CHANGE:
				//{
				//	//The user has modified the editbox -> retrieve search options based on the content of the editbox
				//	utf16_str txt; _get_edit_str(state.controls.editbox, txt); defer{ if (txt.sz) free_any_str(txt.str); };
				//	//TODO(fran): check what happens if the editbox is empty -> malloc(0) is implementation dependant

				//	//Clear previous search options
				//	if (state.free_elements)
				//		state.free_elements(listbox::get_all_elements(state.controls.listbox)/*HACK*/, state.user_extra);
				//	listbox::remove_all_elements(state.controls.listbox);//NOTE: this is inefficient since in the case of set_elements() we'll basically perform two repaints on the listbox

				//	bool show_lb = false;

				//	if (txt.sz_char() > 1 /*not null nor just the null terminator*/) {
				//		//Get search options
				//		ptr<void*> search_options{ 0 }; defer{ search_options.free(); };
				//		if (state.retrieve_search_options)
				//			search_options = state.retrieve_search_options(txt, state.user_extra);

				//		if (search_options.cnt) {
				//			//Set new elements (auto removes existing ones), show the listbox
				//			listbox::set_elements(state.controls.listbox, search_options.mem, search_options.cnt);
				//			show_lb = true;
				//		}
				//		else {
				//			//No search options, hide the listbox
				//			show_lb = false;
				//		}
				//	}
				//	else {
				//		//The editbox is empty, hide the listbox
				//		show_lb = false;
				//	}

				//	show_listbox(state, show_lb);

				//} break;
				//case EN_ENTER:
				//{
				//	//TODO(fran): it's not that easy to know what to tell the user, we have to differentiate between "the user chose an element from the listbox" and "the user wrote something", also the user could first move to some element of the listbox but afterwards write something more (well actually the problem is mine, it's easy to differentiate, a click is obviously for listbox, and enter is listbox if it's visible (since the value is linked with the editbox's) and editbox if not visible aka listbox has no elements). My problem comes when there's the same writing for different words and then I need to filter with more info, specifically the info that the listbox's elements have (since I plan to add it there). Well actually this is bullshit, it's very simple, we need two cases, if the listbox is not visible then what's in the editbox counts and it's just that info, _but_ if the listbox is visible then what count is that element's data (since the editbox is, in this case, simply showing part of the info the listbox provided).
				//	//A different idea would be the google/internet-search-engine searchbar approach, when the user selects an existing listbox item we go directly to it, otherwise we show possible results on a non floating listbox below the searchbar

				//	//SOLUTION: the listbox's selection is cleared each time the user writes smth, that means we have a way of differentiating between enter for listbox and enter for editbox, if a valid selection is present in the listbox then it's a listbox enter, otherwise it's an editbox one

				//	size_t lb_sel = listbox::get_cur_sel(state.controls.listbox);
				//	size_t lb_cnt = listbox::get_element_cnt(state.controls.listbox);
				//	if (lb_sel < lb_cnt) {
				//		//Valid listbox selection, use the listbox item
				//		if (state.perform_search)
				//			state.perform_search(listbox::get_element_at(state.controls.listbox, lb_sel), true, state.user_extra);
				//	}
				//	else {
				//		//Invalid listbox selection, use the editbox's text
				//		utf16_str txt; _get_edit_str(state.controls.editbox, txt); defer{ if (txt.sz) free_any_str(txt.str); };
				//		if (state.perform_search)
				//			state.perform_search(&txt, false, state.user_extra);
				//	}

				//	//Clear search options
				//	if (state.free_elements)
				//		state.free_elements(listbox::get_all_elements(state.controls.listbox)/*HACK*/, state.user_extra);
				//	listbox::remove_all_elements(state.controls.listbox);
				//	show_listbox(state, false);


				//} break;
				//case EN_ESCAPE:
				//{
				//	show_listbox(state, false);
				//	if (state.free_elements)
				//		state.free_elements(listbox::get_all_elements(state.controls.listbox)/*HACK*/, state.user_extra);
				//	listbox::remove_all_elements(state.controls.listbox);
				//	//TODO(fran): restore the content of the editbox (WM_SETTEXT_NONOTIF)
				//} break;
				//case EN_UP:
				//{
				//	listbox::sel_up(state.controls.listbox);
				//} break;
				//case EN_DOWN:
				//{
				//	listbox::sel_down(state.controls.listbox);
				//} break;
				//case EN_KILLFOCUS:
				//{
				//	//NOTE: since killfocus arrives _before_ LBN_CLK, we need to check who is the one with focus now, if it's the listbox cause the user clicked it then we are okay, otherwise we gotta clear it
				//	if (state.controls.listbox != GetFocus()) {
				//		//Clear search options
				//		if (state.free_elements)
				//			state.free_elements(listbox::get_all_elements(state.controls.listbox)/*HACK*/, state.user_extra);
				//		listbox::remove_all_elements(state.controls.listbox);
				//		show_listbox(state, false);
				//	}
				//} break;
				/*default: Assert(0);
				}
			}*/
			/*else*/
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
					ask_window_for_repaint(state.wnd);//TODO(fran): I added this to make sure the combobox (button) is re-rendered after a selection, but I feel like this should already be called somewhere else, like when we set_element to the button

				} break;
				case LBN_SELCHANGE://The user is moving up and down the elements of the listbox
				{
					/*
					size_t lb_sel = listbox::get_cur_sel(state.controls.listbox);
					size_t lb_cnt = listbox::get_element_cnt(state.controls.listbox);
					if (lb_sel < lb_cnt) {
						//Valid listbox selection
						if (state.show_element_on_editbox)
							state.show_element_on_editbox(state.controls.editbox, listbox::get_element_at(state.controls.listbox, lb_sel), state.user_extra);
						//TODO(fran): save what the user currently wrote to be able to restore it later
					}
					else {
						//Invalid listbox selection
					}
					*/
				} break;
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
		//Clear search options
		//if (state.free_elements)
		//	state.free_elements(listbox::get_all_elements(state.controls.listbox)/*HACK*/, state.user_extra);
		//listbox::remove_all_elements(state.controls.listbox);
		state.reject_button = (bool)wparam;
		show_listbox(state, false);
		ask_window_for_repaint(state.wnd);//TODO(fran): I feel this should already happen somewhere
	} break;
	/*
	case CB_SETCUEBANNER:
	{
		return SendMessage(state.controls.editbox, WM_SETDEFAULTTEXT, wparam, lparam);
	} break;
	*/
	//case CB_GETCUEBANNER://TODO(fran): I dont have a WM_GETDEFAULTTEXT
	//{
	//	//NOTE: there is more than one stupid thing about this msg, first the paramaters are REVERSED compared to WM_GETTEXT, and second there's no CB_GETCUEBANNERLENGTH!
	//	return SendMessage(state.controls.editbox, WM_GETDEFAULTTEXT, lparam, wparam);
	//} break;
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
	case WM_MOUSEWHEEL: //No reason for processing mousewheel input, some comboboxes change their index when being scrolled, I hate that behaviour since the user always ends up doing that by accident when trying to scroll the page
	case WM_DESTROY:
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