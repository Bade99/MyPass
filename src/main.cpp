
//TODO(fran): add a note tip about password breaches, eg check https://haveibeenpwned.com/ or similar to make sure non of your accounts have been breached


//----------------------Warnings-----------------------:
#pragma warning (disable: 4244) // C4244: 'initializing': conversion from 'float' to 'int', possible loss of data
#pragma warning (disable: 4312) // C4312 : 'type cast' : conversion from 'int' to 'HMENU' of greater size


#define WM_STATE_NEXT (WM_USER+5000) //proceed to the next window in the state machine
#define WM_STATE_RESET (WM_USER+5001) //clear critical information
#define WM_STATE_START (WM_USER+5002) //you are all set up, start whatever it is you do, this is always a SendMessage, you must use all the init data right there, it's not guaranteed to be there after this msg finishes (return 1 if sucessfully started, 0 otherwise to go back to the prior wnd). wparam = const pointer to Start object of the corresponding type based on what's being started
#define WM_STATE_START_ATTEMPT (WM_USER+5003) //login was attempted but failed, this is always a PostMessage. wparam = login::AttemptResult failure mode

#include "win_sdk.h"
#include "resource.h"
#include "platform.h"
#include "macros.h"
#include "global.h"
#include "language_manager.h"
#include "advanced_helpers.h"
#include "renderer.h"
#include "serialization.h"
#include "core.h"
#include "win_msg_common_handler.h"

#include "page_types.h"
#include "button_types.h"
#include "toast_types.h"
#include "nonclient_types.h"
#include "scrollbar_types.h"
#include "text_types.h"
#include "list_types.h"
#include "combo_types.h"
#include "table_types.h"
#include "search_types.h"
#include "password_editor_types.h"

#include "login_types.h"
#include "editor_types.h"

#include "control_helpers.h"

#include "style.h"

#include "button.h"
#include "scrollbar.h"
#include "page.h"
#include "text.h"
#include "search.h"
#include "nonclient.h"
#include "toast.h"
#include "table.h"
#include "password_editor.h"
#include "list.h"
#include "combo.h"

#include "login.h"
#include "editor.h"

#include "debug.h"

str GetFontFaceName() {
	//Font guidelines: https://docs.microsoft.com/en-us/windows/win32/uxguide/vis-fonts
	//Stock fonts: https://docs.microsoft.com/en-us/windows/win32/gdi/using-a-stock-font-to-draw-text

	//TODO(fran): can we take the best codepage from each font and create our own? (look at font linking & font fallback)
	//TODO(fran): speed up: use a custom allocator that starts by performing a single wchar_t[2500][22] allocation instead of using individually allocated strings

	//We looked at 2195 fonts, this is what's left
	//Options:
	//Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
	//-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
	//-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
	//-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

	i64 cnt = StartCounter(); defer{ printf("ELAPSED FIND FONT: %f ms\n",EndCounter(cnt)); };

	HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); };
	struct FontNameLess {
		bool operator()(const str& left, const str& right) const {
			return _wcsicmp(left.c_str(), right.c_str()) < 0;
		}
	};
	using FontNames = std::set<str, FontNameLess>;
	FontNames fontnames;
	EnumFontFamiliesEx(dc, NULL
		, [](const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam) -> int {
			((FontNames*)lParam)->insert(lpelfe->lfFaceName);
			return TRUE;
		}
	, (LPARAM)&fontnames, NULL);

	const TCHAR* requested_fontname[] = { TEXT("Segoe UI"), TEXT("Arial Unicode MS"), TEXT("Microsoft YaHei"), TEXT("Microsoft YaHei UI"), TEXT("Microsoft JhengHei"), TEXT("Microsoft JhengHei UI") };

	for (const auto requested_font : requested_fontname)
		if (auto font = fontnames.find(requested_font); font != fontnames.end())
			return *font;

	return TEXT("");
}

void setup_fonts() {
	LOGFONT lf{ 0 };
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfHeight = DPI(-15);
	//INFO: by default if I dont set faceName it uses "Modern", looks good but it lacks some charsets
	string_copy(lf.lfFaceName, str_view(GetFontFaceName()));
	fonts.General = CreateFontIndirect(&lf);
	Assert(fonts.General);

	auto default_weight = lf.lfWeight;
	lf.lfWeight = FW_BOLD;
	fonts.GeneralBold = CreateFontIndirect(&lf);
	Assert(fonts.GeneralBold);
	lf.lfWeight = default_weight;


	lf.lfHeight = (LONG)((float)GetSystemMetrics(SM_CYMENU) * .85f);
	fonts.Menu = CreateFontIndirect(&lf);
	Assert(fonts.Menu);

	lf.lfWeight = FW_BOLD;
	fonts.SmallBold = CreateFontIndirect(&lf);
	Assert(fonts.SmallBold);

	atexit([]() { for (auto& f : fonts.all) if (f) { DeleteObject(f); f = nil; } });
}

void setup_dpi_awareness() {
	//DPI awareness is set via the app.manifest file
	 
	//TODO(fran): currently we only support "system dpi aware", we get the current dpi value, but it never changes (GetDpiForSystem always returns the same value), therefore if dpi changes afterwards then our windows will be stretched by Windows, but at least we look correct as long as the user doesnt change their current dpi. Next goal is PerMonitorV2 aware, which allows us to adapt realtime to dpi changes, but of course does need us to regenerate at least our fonts, and provide a stateful DPI object that you can generate when resizing so that you can get the dpi for the current window you are resizing
	//https://docs.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
	//https://github.com/tringi/win32-dpi/blob/master/win32-dpi.cpp
}

void setup_debug_console() {
#ifdef _SHOWCONSOLE
	AllocConsole();
	
	freopen_s(&console_file_handle, "CONIN$", "r", stdin);
	freopen_s(&console_file_handle, "CONOUT$", "w", stdout);
	freopen_s(&console_file_handle, "CONOUT$", "w", stderr);
	atexit([]() { if (console_file_handle) { fclose(console_file_handle); console_file_handle = nil; } });
#endif
}

int APIENTRY wWinMain(HINSTANCE hInstance,HINSTANCE,LPWSTR,int)
{
	setup_debug_console();
	setup_fonts();

	const str to_deserialize = load_file_serialized(serialization_folder);

	LanguageManager& lang_mgr = LanguageManager::Instance(); lang_mgr.SetHInstance(hInstance);
	login::Settings login_cl;
	editor::Settings editor_cl;

	_BeginDeserialize();
	_deserialize_struct(lang_mgr, to_deserialize);
	_deserialize_struct(login_cl, to_deserialize);
	login_cl.validate();
	_deserialize_struct(editor_cl, to_deserialize);
	editor_cl.validate();
	#ifndef _DEBUG // While debugging always use default struct colors to make it easier to try different ones
	_deserialize_struct(colors, to_deserialize);
	#endif
	default_colors_if_not_set(&colors);
	defer{ for (HBRUSH& b : colors.brushes) if (b) { DeleteBrush(b); b = NULL; } };

	load_styles();

	#ifdef _DEBUG
	HWND debug_track_wnd = nil;
	nonclient::LpParam debug_nc_param{
		.client_class_name = debug::wndclass,
		.client_lp_param = &debug_track_wnd,
	};
	HWND debug_wnd = create_root_window(
		hInstance,
		nonclient::calc_nonclient_rc_from_client({ .left = 0, .top = 0, .right = 300, .bottom = 300 }, false),
		&debug_nc_param
	);
	#endif

	login::Results login_results{0};
	login::Data login_data{ &login_cl, &login_results };
	nonclient::LpParam login_nclpparam{
		.client_class_name = login::wndclass,
		.client_lp_param = &login_data,
		.can_maximize = false,
		.min_window_width = (i32)DPI(300),
	};

	editor::Data editor_data{
		.settings = &editor_cl,
	};

	nonclient::LpParam show_nclpparam{
		.client_class_name = editor::wndclass,
		.client_lp_param = &editor_data,
		.min_window_width = (i32)DPI(500),
	};

	auto validate_nonclient_rect_in_screen = [](RECT nc, RECT default_nc, i32 min_width) {
		nc.right = maximum(nc.right, nc.left + min_width);
		default_nc.right = maximum(default_nc.right, default_nc.left + min_width);
		validate_rect_in_screen(nc, default_nc);
		return nc;
	};

	HWND login_wnd = create_root_window(
		hInstance, 
		validate_nonclient_rect_in_screen(nonclient::calc_nonclient_rc_from_client(login_cl.rc, true), login_cl.rc, login_nclpparam.min_window_width),
		&login_nclpparam
	);

	HWND editor_wnd = create_root_window(
		hInstance, 
		validate_nonclient_rect_in_screen(nonclient::calc_nonclient_rc_from_client(editor_cl.rc, true), editor_cl.rc, show_nclpparam.min_window_width),
		&show_nclpparam
	);
	
	PostMessage(0, WM_STATE_NEXT, 0, 0); //start the state machine

	enum class WindowTask { login, show_passwords };
	struct WindowTaskState { HWND wnd; WindowTask task; };

	WindowTaskState state_machine[] = {
		{.wnd = login_wnd, .task = WindowTask::login},
		{.wnd = editor_wnd, .task = WindowTask::show_passwords}
	};
	i32 state_idx = -1;
	constexpr i32 state_cnt = ARRAYSIZE(state_machine);
	
	auto transition_state = [](i32 current_state, i32 direction) -> i32 {
		Assert(abs(direction) == 1);
		current_state += direction;
		current_state %= state_cnt;
		if (current_state < 0) current_state = state_cnt + current_state; 
		return current_state;
	};

	auto prev_state = [&transition_state](i32 current_state) ->i32 { return transition_state(current_state, -1); };
	auto next_state = [&transition_state](i32 current_state) ->i32 { return transition_state(current_state, +1); };

	auto loading_cursor = LoadCursor(hInstance, IDC_APPSTARTING);
	auto arrow_cursor = LoadCursor(hInstance, IDC_ARROW);

	// Main message loop:
	MSG msg;
	BOOL bRet;
	while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
	{
		Assert(bRet != -1);// There was an error
		switch (msg.message) {
		case WM_STATE_NEXT:
		{
			state_idx = next_state(state_idx);
			WindowTaskState new_state = state_machine[state_idx];
			WindowTaskState old_state = state_machine[prev_state(state_idx)];
			switch (new_state.task) {
			case WindowTask::login:
			{
				PostMessage(nonclient::get_state(old_state.wnd)->client, WM_STATE_RESET, 0, 0);
				ShowWindow(old_state.wnd, SW_HIDE);
				ShowWindow(new_state.wnd, SW_SHOW);
				#ifdef _DEBUG
				debug_track_wnd = login_wnd;
				#endif
			} break;
			case WindowTask::show_passwords:
			{
				SetCursor(loading_cursor); defer{ SetCursor(arrow_cursor); };
				const editor::Start editor_start{ .username = login_results.username, .password = login_results.password, .signup = login_results.signup };

				// Attempt to start the next state
				auto start_attempt = (login::AttemptResult)SendMessage(nonclient::get_state(new_state.wnd)->client, WM_STATE_START, (WPARAM)&editor_start, 0);
				login::secure_zero_free(login_results.password);
				login::secure_zero_free(login_results.username);

				if (start_attempt == login::AttemptResult::success) {
					PostMessage(nonclient::get_state(old_state.wnd)->client, WM_STATE_RESET, 0, 0);
					ShowWindow(old_state.wnd, SW_HIDE);
					ShowWindow(new_state.wnd, SW_SHOW);
					#ifdef _DEBUG
					debug_track_wnd = editor_wnd;
					#endif
				}
				else {
					// Rollback to previous state
					state_idx = prev_state(state_idx);
					auto wnd = nonclient::get_state(state_machine[state_idx].wnd)->client;
					PostMessage(wnd, WM_STATE_START_ATTEMPT, (WPARAM)start_attempt, 0);
				}
			} break;
			}
		} break;
#if 0
		case WM_DPICHANGED:
		{
			int new_dpi = HIWORD(msg.wParam);
			auto a = GetDpiForSystem();
			//UpdateDpiDependentFontsAndResources();

			RECT* const prcNewWindow = (RECT*)msg.lParam;
			SetWindowPos(msg.hwnd, NULL, prcNewWindow->left, prcNewWindow->top, prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top, SWP_NOZORDER | SWP_NOACTIVATE);
			break;
		} break;
#endif
		default:
		{
			if (!(app_shortcuts.table && TranslateAccelerator(app_shortcuts.listener_wnd ? app_shortcuts.listener_wnd : msg.hwnd, app_shortcuts.table, &msg))) {
				bool needs_dialog_parsing = msg.message == WM_KEYDOWN && msg.wParam == VK_TAB; //TODO(fran): controls that use tab, eg multiline text editor, need to process WM_GETDLGCODE and return DLGC_WANTTAB
				if (!needs_dialog_parsing || !IsDialogMessage(get_dialog_root(msg.hwnd), &msg)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		} break;
		}
	}

	ensure_close_windows(login_wnd, editor_wnd);

	str serialized;
	_BeginSerialize();
	serialized += _serialize_struct(lang_mgr);
	serialized += _serialize_struct(login_cl);
	serialized += _serialize_struct(editor_cl);
	serialized += _serialize_struct(colors);

	if (!save_to_file_serialized(serialized, serialization_folder))
		MessageBox(0, _t("Unable to save application settings"), RCS(LANG_ERROR), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);

	return (int) msg.wParam;
}
