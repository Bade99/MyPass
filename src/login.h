#pragma once
#include "win_sdk.h"
#include "platform.h"
#include "helpers.h"
#include "global.h"
#include "text.h"
#include "button.h"
#include "language_manager.h"

namespace login {

auto get_state(HWND wnd) { _control_create_function__get_state }

void resize_controls(State& state) {
	auto& controls = state.controls;
	RECT r; GetClientRect(state.wnd, &r);
	int w = RECTW(r);
	int h = RECTH(r);
	int w_pad = DPI(8);
	int h_pad = DPI(8);

	auto normal_window_h = DPI(30);

	int h_center = h / 2;

	if (!state.signup_mode) {
		int edit_username_w = DPI(140);
		int edit_username_h = normal_window_h;

		int edit_password_w = edit_username_w;
		int edit_password_h = edit_username_h;
	
		int btn_login_w = DPI(70);
		int btn_login_h = edit_password_h;

		int edit_password_y = h_center - edit_password_h/2;
		int edit_password_x = (w - edit_password_w) / 2;

		int edit_username_y = edit_password_y - h_pad - edit_username_h;
		int edit_username_x = (w - edit_username_w) / 2;

		int btn_login_y = edit_password_y + edit_password_h + h_pad;
		int btn_login_x = (w - btn_login_w) / 2;

		if (IsWindowVisible(controls.static_invalid_password)) {
			i32 small_pad = DPI(2);
			rect_i32 static_invalid_password{
				.x = edit_password_x, .y = (i32)(edit_password_y + edit_password_h + small_pad),
				.w = edit_password_w, .h = (i32)(edit_password_h * .75f)
			};
			MoveWindow(controls.static_invalid_password, static_invalid_password);

			btn_login_y = static_invalid_password.bottom() + small_pad;
		}

		//TODO(fran): resizer that takes into account multiple hwnds
		MoveWindow(controls.edit_username, edit_username_x, edit_username_y, edit_username_w, edit_username_h, false);
		MoveWindow(controls.edit_password, edit_password_x, edit_password_y, edit_password_w, edit_password_h, false);
		MoveWindow(controls.button_login, btn_login_x, btn_login_y, btn_login_w, btn_login_h, false);
	}
	else {
		int static_msg_w = DPI(200);
		int static_msg_h = normal_window_h * 2 + h_pad;
		int static_msg_y = h_center - normal_window_h / 2 - h_pad - normal_window_h;
		int static_msg_x = (w - static_msg_w) / 2;

		int btn_signup_w = (static_msg_w - h_pad) / 2;
		int btn_signup_h = normal_window_h;
		int btn_signup_y = static_msg_y + static_msg_h + h_pad;
		int btn_signup_x = static_msg_x;

		int btn_cancel_w = btn_signup_w;
		int btn_cancel_h = btn_signup_h;
		int btn_cancel_y = btn_signup_y;
		int btn_cancel_x = btn_signup_x + btn_signup_w + h_pad;

		MoveWindow(controls.static_signup_user, static_msg_x, static_msg_y, static_msg_w, static_msg_h, true);
		MoveWindow(controls.button_signup, btn_signup_x, btn_signup_y, btn_signup_w, btn_signup_h, true);
		MoveWindow(controls.button_cancel, btn_cancel_x, btn_cancel_y, btn_cancel_w, btn_cancel_h, true);
	}
}

void set_signup_mode(State& state, bool signup_mode) {
	state.signup_mode = signup_mode;

	auto& controls = state.controls;
	HWND signup_mode_wnds[]{controls.static_signup_user, controls.button_signup, controls.button_cancel};
	HWND non_singup_mode_wnds[]{ controls.edit_username, controls.edit_password, controls.button_login };
	i32 signup_mode_wnds_show = SW_SHOW, non_singup_mode_wnds_show = SW_HIDE;
	if (!state.signup_mode) std::swap(signup_mode_wnds_show, non_singup_mode_wnds_show);
	for (auto w : signup_mode_wnds) ShowWindow(w, signup_mode_wnds_show);
	for (auto w : non_singup_mode_wnds) ShowWindow(w, non_singup_mode_wnds_show);
	ShowWindow(controls.static_invalid_password, SW_HIDE);
	ask_window_for_resize(state.wnd);
	ask_window_for_repaint(state.wnd);
}

void add_controls(State& state) {
	auto& controls = state.controls;
	controls.edit_username = create_window(state.wnd, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP, 0, EDIT_USERNAME);
	edit_oneline::set_theme(controls.edit_username, themes.login_editoneline);
	AWDT(controls.edit_username, LANG_LOGIN_USERNAME);
	edit_oneline::set_function_has_invalid_chars(controls.edit_username, [](const utf16* str, size_t char_cnt, void*) -> auto {
		edit_oneline::_has_invalid_chars res{};
		constexpr auto& invalid_username_chars = _t("<>:\"/\\|?*"); //INFO: there's more to it than this, there are some special restrictions that we implemented outside the control when the user presses login https://gist.github.com/doctaphred/d01d05291546186941e1b7ddc02034d3
		for (u64 i = 0; i < ArraySizeWithoutTerminator(invalid_username_chars); i++) {
			if (contains_char(str, char_cnt, invalid_username_chars[i])) {
				res.res = true;
				res.explanation = (RS(LANG_EDIT_USERNAME_INVALIDCHARS) + L" " + invalid_username_chars);
				break;
			}
		}
		return res;
	});

	controls.edit_password = create_window(state.wnd, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_PASSWORD, 0, EDIT_PASSWORD);
	edit_oneline::set_theme(controls.edit_password, themes.login_editoneline);
	AWDT(controls.edit_password, LANG_LOGIN_PASSWORD);

	controls.static_invalid_password = create_window(state.wnd, edit_oneline::wndclass, nil, WS_CHILD | WS_DISABLED);
	edit_oneline::set_theme(controls.static_invalid_password, themes.login_text_static_error);

	controls.button_login = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP, 0, BTN_LOGIN);
	AWT(controls.button_login, LANG_CONTROL_LOGIN);
	button::set_theme(controls.button_login, themes.login_btn);

	for (auto ctl : {controls.edit_username, controls.edit_password}) SetWindowFont(ctl, fonts.General, true);
	SetWindowFont(controls.button_login, fonts.GeneralBold, true);
	SetWindowFont(controls.static_invalid_password, fonts.SmallBold, true);


	// Signup mode Controls

	controls.static_signup_user = create_window(state.wnd, edit_oneline::wndclass, nil, WS_CHILD | WS_DISABLED);
	edit_oneline::set_theme(controls.static_signup_user, themes.login_text_static_signup);

	controls.button_signup = create_window(state.wnd, button::wndclass, nil, WS_CHILD | WS_TABSTOP, 0, BTN_LOGIN);
	AWT(controls.button_signup, LANG_CONTROL_SIGNUP);
	button::set_theme(controls.button_signup, themes.login_btn);

	controls.button_cancel = create_window(state.wnd, button::wndclass, nil, WS_CHILD | WS_TABSTOP);
	AWT(controls.button_cancel, LANG_CONTROL_CANCEL);
	button::set_theme(controls.button_cancel, themes.login_btn_cancel);
	button::set_user_data(controls.button_cancel, &state);
	button::set_functions(controls.button_cancel, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			set_signup_mode(state, false);
		}
	});

	SetWindowFont(controls.static_signup_user, fonts.General, true);
	SetWindowFont(controls.button_signup, fonts.GeneralBold, true);
	SetWindowFont(controls.button_cancel, fonts.General, true);
}

void save_settings(State& state) {
	RECT rc; GetWindowRect(state.wnd, &rc);
	state.settings->rc = rc;
}

bool any_char(cstr* s, bool op(cstr)) {
	bool res = false;
	if (s && *s) { //TODO(fran): more compact
		do {
			if (op(*s)) {
				res = true;
				break;
			}
		} while (*++s);
	}
	return res;
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case WM_CREATE:
	{
		CREATESTRUCT* createnfo = (CREATESTRUCT*)lparam;
		add_controls(state);
		return 0;
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		resize_controls(state);
		return res;
	} break;
	case WM_NCDESTROY:
	{
		save_settings(state);
		if (&state) {
			if (state.results->password.str) {
				ZeroMemory(state.results->password.str, state.results->password.sz_chars * sizeof(*state.results->password.str));
				free(state.results->password.str);
				state.results->password.str = nullptr;
				state.results->password.sz_chars = 0;
			}
			if (state.results->username.str) {
				ZeroMemory(state.results->username.str, state.results->username.sz_chars * sizeof(*state.results->username.str));
				free(state.results->username.str);
				state.results->username.str = nullptr;
				state.results->username.sz_chars = 0;
			}
			SetWindowText(state.controls.edit_password, _t("")); //TODO(fran): we should make sure the current bytes get zeroed out
			SetWindowText(state.controls.edit_username, _t(""));
			set_window_state(state.wnd, nil);
			free(&state);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCCREATE:
	{
		CREATESTRUCT* create_nfo = (CREATESTRUCT*)lparam;
		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		st->nc_parent = GetParent(hwnd);
		st->wnd = hwnd;
		st->settings = ((Data*)create_nfo->lpCreateParams)->settings;
		st->results = ((Data*)create_nfo->lpCreateParams)->results;
		set_window_state(hwnd, st);
		return TRUE;
	} break;
	case WM_COMMAND:
	{
		printf("HIWORD(wparam)=%d\n", HIWORD(wparam));
		// Msgs from my controls
		switch (LOWORD(wparam))
		{
		case EDIT_USERNAME://TODO(fran): this is actually pretty stupid, we know all our hwnds so we dont really need identifiers where we could have duplicates without knowing
		case EDIT_PASSWORD:
		{
			WORD msg_code = HIWORD(wparam);
			switch (msg_code) {
			case EN_ENTER://User pressed enter key, aka wants to login
			{
				PostMessage(state.wnd, WM_COMMAND, MAKELONG(BTN_LOGIN, 0), (LPARAM)state.controls.button_login);//TODO(fran): decide on a general way to simulate the action of a child control, maybe simulate a click? idk
				return 0;
			} break;
			}
		} break;
		case BTN_LOGIN:
		{
			bool success = false;
			size_t username_len = GetWindowTextLength(state.controls.edit_username) + 1;//includes null terminator
			size_t password_len = GetWindowTextLength(state.controls.edit_password) + 1;//includes null terminator
			if (username_len > 1 && password_len > 1) { //Check: there has to be something written
				cstr* username_buf = (cstr*)malloc(username_len * sizeof(*username_buf));
				cstr* password_buf = (cstr*)malloc(password_len * sizeof(*password_buf));
				GetWindowText(state.controls.edit_username, username_buf, (int)username_len);
				GetWindowText(state.controls.edit_password, password_buf, (int)password_len);
				int username_sz = (int)(username_len - 1) * sizeof(*username_buf); //we dont care about the null terminator
				int password_sz = (int)(password_len - 1) * sizeof(*password_buf); //we dont care about the null terminator

				//Perform the last checks on the username https://gist.github.com/doctaphred/d01d05291546186941e1b7ddc02034d3
				bool check_failed = false;
				//1.reserved characters: we manage that with EM_SETINVALIDCHARS in the edit control
				//2.chars with value 31 or lower (control chars):
				if (any_char(username_buf, [](cstr c) { return c <= 31; })) {
					check_failed = true;
					edit_oneline::show_tip(state.controls.edit_username, RCS(LANG_LOGIN_USERNAME_CONTROLCHARS), EDITONELINE_default_tooltip_duration, default_text_tooltip_location);
				}
				//3.reserved names (CON, PRN, AUX, NUL, COM0, COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9, LPT0, LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, LPT9, also all this followed by a . (eg. COM0.fefe.ef is invalid, once you put the dot it will always be invalid): we append a known string before each username

				//4.do not start nor end with space:
				if (username_buf[0] == _t(' ') || username_buf[username_len - 2] == _t(' ')) {
					//NOTE: actually it can start with a space thanks to our fix to problem 3. but still, a username starting with spaces is strange
					check_failed = true;
					edit_oneline::show_tip(state.controls.edit_username, RCS(LANG_LOGIN_USERNAME_STARTENDSPACE), EDITONELINE_default_tooltip_duration, default_text_tooltip_location);
				}
				//5.do not end with . :
				if (username_buf[username_len - 2] == _t('.')) {
					check_failed = true;
					edit_oneline::show_tip(state.controls.edit_username, RCS(LANG_LOGIN_USERNAME_ENDDOT), EDITONELINE_default_tooltip_duration, default_text_tooltip_location);
				}

				if (check_failed) {
					ZeroMemory(username_buf, username_sz);//TODO(fran): this two should be a mini function or macro (zero_free())
					ZeroMemory(password_buf, password_sz);
					free(username_buf);
					free(password_buf);
					return 0;
				}

				if (sizeof(*username_buf) == 1) {//we got ascii data, we always want utf16 otherwise the key would stop working when switching from ascii to unicode controls
					defer{ free(username_buf); free(password_buf); };
					int username_sz_char = MultiByteToWideChar(CP_ACP, 0, (LPCCH)username_buf, username_sz, 0, 0);//TODO(fran): make sure this guy does NOT force a null terminator to be appended at the end
					int password_sz_char = MultiByteToWideChar(CP_ACP, 0, (LPCCH)password_buf, password_sz, 0, 0);//TODO(fran): make sure this guy does NOT force a null terminator to be appended at the end
					if (username_sz_char && password_sz_char) {
						void* username_mem = malloc(username_sz_char * sizeof(WCHAR));
						void* password_mem = malloc(password_sz_char * sizeof(WCHAR));
						if (username_mem && password_mem) {
							MultiByteToWideChar(CP_ACP, 0, (LPCCH)username_buf, username_sz, (LPWSTR)username_mem, username_sz_char);
							MultiByteToWideChar(CP_ACP, 0, (LPCCH)password_buf, password_sz, (LPWSTR)password_mem, password_sz_char);
							if (state.results->password.str) free(state.results->password.str);//this shouldnt ever happen
							if (state.results->username.str) free(state.results->username.str);//this shouldnt ever happen
							state.results->password.str = (WCHAR*)password_mem;
							state.results->password.sz_chars = password_sz_char;
							state.results->username.str = (WCHAR*)username_mem;
							state.results->username.sz_chars = username_sz_char;
							success = true;
							state.results->signup = state.signup_mode;
						}
					}
				}
				else {
					state.results->username.str = username_buf;
					state.results->username.sz_chars = username_len - 1;
					state.results->password.str = password_buf;
					state.results->password.sz_chars = password_len - 1;
					state.results->signup = state.signup_mode;
					success = true;
				}
				if(success) PostMessage(0, WM_STATE_NEXT, 0, 0);//TODO(fran): maybe storing the results in WM_STATE_NEXT is better?
			}
			else {
				if(username_len==1) edit_oneline::show_tip(state.controls.edit_username, RCS(LANG_LOGIN_USERNAME_EMPTY), EDITONELINE_default_tooltip_duration, default_text_tooltip_location);
				else edit_oneline::show_tip(state.controls.edit_password, RCS(LANG_LOGIN_PASSWORD_EMPTY), EDITONELINE_default_tooltip_duration, default_text_tooltip_location);
			}
			return 0;
		}
#define _generate_enum_cases(member,value_expr) case Language::member:
		_foreach_language(_generate_enum_cases)
		{
			LanguageManager::Instance().ChangeLanguage((Language)LOWORD(wparam));
			return 0;
		}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_LBUTTONUP:
	{
		SetFocus(state.wnd);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_STATE_START_ATTEMPT:
	{
		auto start_attempt = (AttemptResult)wparam;
		switch (start_attempt) {
		case AttemptResult::fail_password:
		{
			SetWindowText(state.controls.static_invalid_password, RCS(LANG_ERROR_PASSWORD));
			ShowWindow(state.controls.static_invalid_password, SW_SHOW);
			ask_window_for_resize(state.wnd);
			ask_window_for_repaint(state.wnd);
			auto hide_error_text = [](HWND wnd, UINT, UINT_PTR timer_id, DWORD) {
				KillTimer(wnd, timer_id);
				if (auto& state = *get_state(wnd); &state) {
					ShowWindow(state.controls.static_invalid_password, SW_HIDE);
					ask_window_for_resize(wnd);
					ask_window_for_repaint(wnd);
				}
				};
			SetTimer(state.wnd, (UINT_PTR)&hide_error_text, invalid_password_message_duration_ms, hide_error_text);
		} break;
		case AttemptResult::fail_username:
		{
			auto user = std::wstring_view(state.results->username.str, state.results->username.sz_chars);
			auto signup_msg = std::vformat(RS(LANG_CREATEACCOUNT), std::make_wformat_args(user));
			SetWindowText(state.controls.static_signup_user, signup_msg.c_str());
			set_signup_mode(state, true);
		} break;
		}
	} break;
	case WM_STATE_RESET:
	{
		if (state.results->password.str) {
			ZeroMemory(state.results->password.str, state.results->password.sz_chars * sizeof(*state.results->password.str));
			free(state.results->password.str);
			state.results->password.str = nullptr;
			state.results->password.sz_chars = 0;
		}
		if (state.results->username.str) {
			ZeroMemory(state.results->username.str, state.results->username.sz_chars * sizeof(*state.results->username.str));
			free(state.results->username.str);
			state.results->username.str = nullptr;
			state.results->username.sz_chars = 0;
		}
		SetWindowText(state.controls.edit_password, _t(""));
		SetWindowText(state.controls.edit_username, _t(""));
	} break;
	case WM_CLOSE:
	{
		return 0;
	} break;
	case WM_ERASEBKGND:
	{
		HDC dc = (HDC)wparam;
		auto& controls = state.controls;
		RECT r; GetClientRect(state.wnd, &r);
		FillRect(dc, &r, colors.ControlBk);
		HWND signup_wnds[] { controls.static_signup_user, controls.button_cancel };
		HWND non_signup_wnds[] { controls.edit_username, controls.button_login };
		RECT bounds = state.signup_mode ? 
			bounding_box(state.wnd, signup_wnds, ARRAYSIZE(signup_wnds)) :
			bounding_box(state.wnd, non_signup_wnds, ARRAYSIZE(non_signup_wnds));
		//TODO: login Theme
		const auto pad = DPI(20);
		InflateRect(&bounds, pad, pad);
		urender::draw_round_rectangle(dc, bounds, themes.base_btn.dimensions.border_radius.to_px(), colors.Card_Bk_Soft);
		return 1;
	}
	#ifdef _DEBUG_HWND_MESSAGES
	case WM_NCCALCSIZE:
	case WM_NOTIFYFORMAT:
	case WM_QUERYUISTATE: //Neat, I dont know who asks for this but it's interesting, you can tell whether you need to draw keyboards accels
	case WM_MOVE:
	case WM_SHOWWINDOW:
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
	case WM_NCPAINT:
	case WM_PAINT:
	case WM_NCHITTEST:
	case WM_SETCURSOR:
	case WM_MOUSEMOVE:
	case WM_MOUSEACTIVATE:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_CONTEXTMENU://Interesting
	case WM_DESTROY:
	case WM_DELETEITEM://sent to owner of combo/list box (for menus also) when it is destroyed or items are being removed. when you do SetMenu you also get this msg if you had a menu previously attached
		//TODO(fran): we could try to use this to manage state destruction
	case WM_GETTEXT:
	case WM_PARENTNOTIFY:
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

_add_struct_to_serialization_namespace(login::Settings);
