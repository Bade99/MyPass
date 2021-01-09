#pragma once
#include <Windows.h>
#include "unCap_Platform.h"
#include "unCap_Helpers.h"
#include "unCap_Global.h"
#include "unCap_edit_oneline.h"
#include "unCap_button.h"
#include "LANGUAGE_MANAGER.h"

constexpr TCHAR protect_wndclass_login[] = TEXT("protect_wndclass_login");

//IDEA: why not, instead of having two separate hwnds, nc and cl, just have one, and the nc be a separate function call, immediate problem to solve are msgs that both would need, eg create and such

//USAGE: once the user presses on "Login" we will send a msg to ourselves that will not arrive, it's for the guy on top controlling the msg loop, it will have a state machine type of thing where it knows to current state, the next, and what's needed to get to the next (eg getting data from the previous), and finally to request the current wnd to stop (DestroyWindow)

//TODO(fran): failure after multiple wrong passwords? eg 10 min block

#define LOGIN_EDIT_USERNAME 10
#define LOGIN_EDIT_PASSWORD 11
#define LOGIN_BTN_LOGIN 12

struct LoginSettings {

#define foreach_LoginSettings_member(op) \
		op(RECT, rc,200,200,600,450 ) \

	//TODO(fran): last_dir wont work for ansi cause the function that uses it is only unicode, problem is I need serialization and that is encoding dependent, +1 for binary serializaiton

	foreach_LoginSettings_member(_generate_member);

	_generate_default_struct_serialize(foreach_LoginSettings_member);

	_generate_default_struct_deserialize(foreach_LoginSettings_member);

};

_add_struct_to_serialization_namespace(LoginSettings)

struct LoginResults {
	text username; //TODO(fran): the control where this is entered shouldnt accept chars that cant be used to create a folder

	text password;
};

struct LoginData { //a pointer to this struct is to be sent on CreateWindow, struct MUST be zeroed out
	LoginSettings* settings;
	LoginResults* results;
};

struct LoginProcState {
	HWND wnd;
	HWND nc_parent;

	union LoginControls {
		struct {
			HWND edit_username;
			HWND edit_password;
			HWND button_login;
		};
		HWND all[3];//REMEMBER TO UPDATE
	}controls;
	LoginSettings* settings;
	LoginResults* results;
};

LoginProcState* LOGIN_get_state(HWND login) {
	LoginProcState* state = (LoginProcState*)GetWindowLongPtr(login, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

void LOGIN_set_state(HWND login, LoginProcState* state) {
	SetWindowLongPtr(login, 0, (LONG_PTR)state);
}

void LOGIN_resize_controls(LoginProcState* state) {
	RECT r; GetClientRect(state->wnd, &r);
	int w = RECTWIDTH(r);
	int h = RECTHEIGHT(r);
	int w_pad = (int)((float)w * .05f);
	int h_pad = (int)((float)h * .05f);

	int h_center = h / 2;

	int edit_username_w = 140;
	int edit_username_h = 30;

	int edit_password_w = edit_username_w;
	int edit_password_h = edit_username_h;
	
	int edit_username_edit_password_pad_h = h_pad;//maybe constant pad is better
	int edit_password_btn_login_pad_h = h_pad;//maybe constant pad is better

	int btn_login_w = 70;
	int btn_login_h = edit_password_h;

	int edit_password_y = h_center - edit_password_h/2;
	int edit_password_x = (w - edit_password_w) / 2;

	int edit_username_y = edit_password_y - edit_username_edit_password_pad_h - edit_username_h;
	int edit_username_x = (w - edit_username_w) / 2;

	int btn_login_y = edit_password_y + edit_password_h + edit_password_btn_login_pad_h;
	int btn_login_x = (w - btn_login_w) / 2;;

	//TODO(fran): resizer that takes into account multiple hwnds
	MoveWindow(state->controls.edit_username, edit_username_x, edit_username_y, edit_username_w, edit_username_h, FALSE);
	MoveWindow(state->controls.edit_password, edit_password_x, edit_password_y, edit_password_w, edit_password_h, FALSE);
	MoveWindow(state->controls.button_login, btn_login_x, btn_login_y, btn_login_w, btn_login_h, FALSE);
}

void LOGIN_add_controls(LoginProcState* state) {
	state->controls.edit_username = CreateWindow(unCap_wndclass_edit_oneline, _t(""), WS_VISIBLE | WS_CHILD | ES_LEFT | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, (HMENU)LOGIN_EDIT_USERNAME, NULL, NULL);
	EDITONELINE_set_brushes(state->controls.edit_username, TRUE, unCap_colors.ControlTxt, unCap_colors.ControlBk, unCap_colors.Img, unCap_colors.ControlTxt_Disabled, unCap_colors.ControlBk_Disabled, unCap_colors.Img_Disabled);

	SendMessage(state->controls.edit_username, WM_SETDEFAULTTEXT, 0, (LPARAM)RCS(LANG_LOGIN_USERNAME));
	cstr invalid_username_chars[] = _t("<>:\"/\\|?*"); //INFO: there's more to it than this, there are some special restrictions that we implemented outside the control when the user presses login https://gist.github.com/doctaphred/d01d05291546186941e1b7ddc02034d3
	SendMessage(state->controls.edit_username, EM_SETINVALIDCHARS, 0, (LPARAM)invalid_username_chars);


	state->controls.edit_password = CreateWindow(unCap_wndclass_edit_oneline, _t(""), WS_VISIBLE | WS_CHILD | ES_LEFT | WS_TABSTOP | ES_PASSWORD
		, 0, 0, 0, 0, state->wnd, (HMENU)LOGIN_EDIT_PASSWORD, NULL, NULL);
	EDITONELINE_set_brushes(state->controls.edit_password, TRUE, unCap_colors.ControlTxt, unCap_colors.ControlBk, unCap_colors.Img, unCap_colors.ControlTxt_Disabled, unCap_colors.ControlBk_Disabled, unCap_colors.Img_Disabled);

	SendMessage(state->controls.edit_password, WM_SETDEFAULTTEXT, 0, (LPARAM)RCS(LANG_LOGIN_PASSWORD));

	state->controls.button_login = CreateWindow(unCap_wndclass_button, NULL, WS_VISIBLE | WS_CHILD | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, (HMENU)LOGIN_BTN_LOGIN, NULL, NULL);
	AWT(state->controls.button_login, LANG_CONTROL_LOGIN);
	UNCAPBTN_set_brushes(state->controls.button_login, TRUE, unCap_colors.Img, unCap_colors.ControlBk, unCap_colors.ControlTxt, unCap_colors.ControlBkPush, unCap_colors.ControlBkMouseOver);

	for (auto ctl : state->controls.all)
		SendMessage(ctl, WM_SETFONT, (WPARAM)unCap_fonts.General, TRUE);
}

void LOGIN_save_settings(LoginProcState* state) {
	RECT rc; GetWindowRect(state->wnd, &rc);
	state->settings->rc = rc;
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

LRESULT CALLBACK LoginProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LoginProcState* state = LOGIN_get_state(hwnd);
	switch (msg) {
	case WM_CREATE:
	{
		CREATESTRUCT* createnfo = (CREATESTRUCT*)lparam;

		LOGIN_add_controls(state);

		return 0;
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		LOGIN_resize_controls(state);
		return res;
	} break;
	case WM_NCDESTROY:
	{
		LOGIN_save_settings(state);
		if (state) {
			if (state->results->password.str) {
				ZeroMemory(state->results->password.str, state->results->password.sz_chars * sizeof(*state->results->password.str));
				free(state->results->password.str);
				state->results->password.str = nullptr;
				state->results->password.sz_chars = 0;
			}
			if (state->results->username.str) {
				ZeroMemory(state->results->username.str, state->results->username.sz_chars * sizeof(*state->results->username.str));
				free(state->results->username.str);
				state->results->username.str = nullptr;
				state->results->username.sz_chars = 0;
			}
			SetWindowText(state->controls.edit_password, _t("")); //TODO(fran): we should make sure the current bytes get zeroed out
			SetWindowText(state->controls.edit_username, _t(""));
			free(state);
			state = nullptr;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCCREATE:
	{
		CREATESTRUCT* create_nfo = (CREATESTRUCT*)lparam;
		LoginProcState* st = (LoginProcState*)calloc(1, sizeof(LoginProcState));
		Assert(st);
		st->nc_parent = GetParent(hwnd);
		st->wnd = hwnd;
		st->settings = ((LoginData*)create_nfo->lpCreateParams)->settings;
		st->results = ((LoginData*)create_nfo->lpCreateParams)->results;
		LOGIN_set_state(hwnd, st);
		return TRUE;
	} break;
	case WM_COMMAND:
	{
		printf("HIWORD(wparam)=%d\n", HIWORD(wparam));
		// Msgs from my controls
		switch (LOWORD(wparam))
		{
		case LOGIN_EDIT_USERNAME://TODO(fran): this is actually pretty stupid, we know all our hwnds so we dont really need identifiers where we could have duplicates without knowing
		case LOGIN_EDIT_PASSWORD:
		{
			WORD msg_code = HIWORD(wparam);
			switch (msg_code) {
			case EN_ENTER://User pressed enter key, aka wants to login
			{
				PostMessage(state->wnd, WM_COMMAND, MAKELONG(LOGIN_BTN_LOGIN, 0), (LPARAM)state->controls.button_login);//TODO(fran): decide on a general way to simulate the action of a child control, maybe simulate a click? idk
				return 0;
			} break;
			}
		} break;
		case LOGIN_BTN_LOGIN:
		{
			bool success = false;
			size_t username_len = GetWindowTextLength(state->controls.edit_username) + 1;//includes null terminator
			size_t password_len = GetWindowTextLength(state->controls.edit_password) + 1;//includes null terminator
			if (username_len > 1 && password_len > 1) { //Check: there has to be something written
				cstr* username_buf = (cstr*)malloc(username_len * sizeof(*username_buf));
				cstr* password_buf = (cstr*)malloc(password_len * sizeof(*password_buf));
				GetWindowText(state->controls.edit_username, username_buf, username_len);
				GetWindowText(state->controls.edit_password, password_buf, password_len);
				int username_sz = (username_len - 1) * sizeof(*username_buf); //we dont care about the null terminator
				int password_sz = (password_len - 1) * sizeof(*password_buf); //we dont care about the null terminator

				//Perform the last checks on the username https://gist.github.com/doctaphred/d01d05291546186941e1b7ddc02034d3
				bool check_failed = false;
				//1.reserved characters: we manage that with EM_SETINVALIDCHARS in the edit control
				//2.chars with value 31 or lower (control chars):
				if (any_char(username_buf, [](cstr c) { return c <= 31; })) {
					check_failed = true;
					EDITONELINE_show_tip(state->controls.edit_username, RCS(LANG_LOGIN_USERNAME_CONTROLCHARS), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
				}
				//3.reserved names (CON, PRN, AUX, NUL, COM0, COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9, LPT0, LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, LPT9, also all this followed by a . (eg. COM0.fefe.ef is invalid, once you put the dot it will always be invalid): we append a known string before each username

				//4.do not start nor end with space:
				if (username_buf[0] == _t(' ') || username_buf[username_len - 2] == _t(' ')) {
					//NOTE: actually it can start with a space thanks to our fix to problem 3. but still, a username starting with spaces is strange
					check_failed = true;
					EDITONELINE_show_tip(state->controls.edit_username, RCS(LANG_LOGIN_USERNAME_STARTENDSPACE), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
				}
				//5.do not end with . :
				if (username_buf[username_len - 2] == _t('.')) {
					check_failed = true;
					EDITONELINE_show_tip(state->controls.edit_username, RCS(LANG_LOGIN_USERNAME_ENDDOT), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
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
							if (state->results->password.str) free(state->results->password.str);//this shouldnt ever happen
							if (state->results->username.str) free(state->results->username.str);//this shouldnt ever happen
							state->results->password.str = (WCHAR*)password_mem;
							state->results->password.sz_chars = password_sz_char;
							state->results->username.str = (WCHAR*)username_mem;
							state->results->username.sz_chars = username_sz_char;
							success = true;
						}
					}
				}
				else {
					state->results->username.str = username_buf;
					state->results->username.sz_chars = username_len - 1;
					state->results->password.str = password_buf;
					state->results->password.sz_chars = password_len - 1;
					success = true;
				}
				if(success) PostMessage(0, WM_NEXT, 0, 0);//TODO(fran): maybe storing the results in WM_NEXT is better?
			}
			else {
				if(username_len==1)EDITONELINE_show_tip(state->controls.edit_username, RCS(LANG_LOGIN_USERNAME_EMPTY), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
				else EDITONELINE_show_tip(state->controls.edit_password, RCS(LANG_LOGIN_PASSWORD_EMPTY), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
			}
			return 0;
		}
#define _generate_enum_cases(member,value_expr) case LANGUAGE::member:
		_foreach_language(_generate_enum_cases)
		{
			LANGUAGE_MANAGER::Instance().ChangeLanguage((LANGUAGE)LOWORD(wparam));
			return 0;
		}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCCALCSIZE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NOTIFYFORMAT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_QUERYUISTATE: //Neat, I dont know who asks for this but it's interesting, you can tell whether you need to draw keyboards accels
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOVE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SHOWWINDOW:
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
	case WM_NCPAINT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ERASEBKGND:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);//TODO(fran): or return 1, idk
	} break;
	case WM_PAINT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCHITTEST:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SETCURSOR:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEMOVE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEACTIVATE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_LBUTTONDOWN:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_LBUTTONUP:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_RBUTTONDOWN:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_RBUTTONUP:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CONTEXTMENU://Interesting
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DESTROY:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DELETEITEM://sent to owner of combo/list box (for menus also) when it is destroyed or items are being removed. when you do SetMenu you also get this msg if you had a menu previously attached
		//TODO(fran): we could try to use this to manage state destruction
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETTEXT://we should return NULL
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_PARENTNOTIFY:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_RESET:
	{
		if (state->results->password.str) {
			ZeroMemory(state->results->password.str, state->results->password.sz_chars * sizeof(*state->results->password.str));
			free(state->results->password.str);
			state->results->password.str = nullptr;
			state->results->password.sz_chars = 0;
		}
		if (state->results->username.str) {
			ZeroMemory(state->results->username.str, state->results->username.sz_chars * sizeof(*state->results->username.str));
			free(state->results->username.str);
			state->results->username.str = nullptr;
			state->results->username.sz_chars = 0;
		}
		SetWindowText(state->controls.edit_password, _t(""));
		SetWindowText(state->controls.edit_username, _t(""));
	} break;
	default:
#ifdef _DEBUG
		Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	}
	return 0;
}

ATOM init_wndclass_protect_login(HINSTANCE inst) {
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = LoginProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(LoginProcState*);
	wcex.hInstance = inst;
	wcex.hIcon = LoadIcon(inst, MAKEINTRESOURCE(MYPASS_ICO_LOGO)); //TODO(fran): LoadImage to choose the best size
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = unCap_colors.ControlBk;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = protect_wndclass_login;
	wcex.hIconSm = LoadIcon(inst, MAKEINTRESOURCE(MYPASS_ICO_LOGO)); //TODO(fran): LoadImage to choose the best size

	ATOM class_atom = RegisterClassExW(&wcex);
	Assert(class_atom);
	return class_atom;
}