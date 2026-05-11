
//TODO(fran): add a note tip about password breaches, eg check https://haveibeenpwned.com/ or similar to make sure non of your accounts have been breached


//----------------------Warnings-----------------------:
#pragma warning (disable: 4244) // C4244: 'initializing': conversion from 'float' to 'int', possible loss of data
#pragma warning (disable: 4312) // C4312 : 'type cast' : conversion from 'int' to 'HMENU' of greater size


#define WM_STATE_NEXT (WM_USER+5000) //proceed to the next window in the state machine
#define WM_STATE_RESET (WM_USER+5001) //clear critical information
#define WM_STATE_START (WM_USER+5002) //you are all set up, start whatever it is you do, this is always a SendMessage, you must use all the init data right there, it's not guaranteed to be there after this msg finishes (return 1 if sucessfully started, 0 otherwise to go back to the prior wnd)
#define WM_STATE_START_ATTEMPT (WM_USER+5003) //login was attempted but failed, this is always a PostMessage. wparam = login::AttemptResult failure mode

#include "win_sdk.h"
#include "resource.h"
#include "platform.h"
#include "macros.h"
#include "renderer.h"
#include "language_manager.h"
#include "global.h"
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

#include "style.h"

#include "page.h"
#include "button.h"
#include "text.h"
#include "search.h"
#include "nonclient.h"
#include "toast.h"
#include "scrollbar.h"
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
    //TODO(fran): case insensitive comparison
    //TODO(fran): speed up: use a custom allocator that starts by performing a single wchar_t[2500][22] allocation instead of using individually allocated strings

    //We looked at 2195 fonts, this is what's left
    //Options:
    //Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
    //-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
    //-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
    //-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

    i64 cnt = StartCounter(); defer{ printf("ELAPSED FIND FONT: %f ms\n",EndCounter(cnt)); };

    HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); };
    std::set<str> fontnames;
    EnumFontFamiliesEx(dc, NULL
        , [](const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam) -> int {
            ((std::set<str>*)lParam)->insert(lpelfe->lfFaceName);
            return TRUE;
        }
    , (LPARAM)&fontnames, NULL);

    const TCHAR* requested_fontname[] = { TEXT("Segoe UI"), TEXT("Arial Unicode MS"), TEXT("Microsoft YaHei"), TEXT("Microsoft YaHei UI"), TEXT("Microsoft JhengHei"), TEXT("Microsoft JhengHei UI") };

    for (const auto requested_font : requested_fontname)
        if (fontnames.find(requested_font) != fontnames.end())
            return requested_font;

    return TEXT("");
}

void setup_fonts() {
    LOGFONT lf{ 0 };
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfHeight = DPI(-15);
    //INFO: by default if I dont set faceName it uses "Modern", looks good but it lacks some charsets
    StrCpyN(lf.lfFaceName, GetFontFaceName().c_str(), ARRAYSIZE(lf.lfFaceName));

    fonts.General = CreateFontIndirect(&lf);
    Assert(fonts.General);

    auto default_weight = lf.lfWeight;
    lf.lfWeight = FW_BOLD;
    fonts.GeneralBold = CreateFontIndirect(&lf);
    Assert(fonts.GeneralBold);
    lf.lfWeight = default_weight;


    lf.lfHeight = (LONG)((float)GetSystemMetrics(SM_CYMENU) * .85f);
    fonts.Menu = CreateFontIndirectW(&lf);
    Assert(fonts.Menu);

    lf.lfWeight = FW_BOLD;
    fonts.SmallBold = CreateFontIndirectW(&lf);
    Assert(fonts.SmallBold);

    atexit([]() { for (auto& f : fonts.all) if (f) { DeleteObject(f); f = nil; } });
}

void setup_dpi_awareness() {
    bool dpi_aware = SetProcessDPIAware(); Assert(dpi_aware); //TODO(fran): only for Windows Vista and above //TODO(fran): this is only sort of dpi aware, we tell windows that we check for dpi the first time but that never check it again, therefore if dpi changes after we already loaded we will be scaled by windows, but at least we look correct as long as the user doesnt change their current dpi (also this means that when we call GetDpiForSystem we will always get the same value, the dpi at the moment the application started)
    //https://docs.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
    //https://github.com/tringi/win32-dpi/blob/master/win32-dpi.cpp
}

void ensure_close_windows(HWND login, HWND editor) {
    /**
     * When a window is closed we destroy it and send the quit message to the app.
     * That does not mean that other open windows are automatically destroyed,
     * so we do it manually to allow them to exit gracefully.
     */
    constexpr auto& nonclient = nonclient::wndclass;
    constexpr auto sz = ARRAYSIZE(nonclient);
    WCHAR test_class[sz];
    for (auto w : {login, editor})
        if (IsWindow(w) && GetClassNameW(w, &test_class[0], sz) && !wcsncmp(test_class, nonclient, sz))
            DestroyWindow(w);
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
        nonclient::calc_nonclient_rc_from_client({ .left = 0, .top = 0, .right = 200, .bottom = 300 }, false),
        &debug_nc_param
    );
    #endif

    login::Results login_results{0};
    login::Data login_data{ &login_cl, &login_results };
    nonclient::LpParam login_nclpparam{
        .client_class_name = login::wndclass,
        .client_lp_param = &login_data,
        .can_maximize = false,
    };

    editor::Start editor_start{0};
    editor::Data editor_data{
        .settings = &editor_cl,
        .start = &editor_start,
    };

    nonclient::LpParam show_nclpparam{
        .client_class_name = editor::wndclass,
        .client_lp_param = &editor_data,
    };

    HWND login_wnd = create_root_window(
        hInstance, 
        nonclient::calc_nonclient_rc_from_client(login_cl.rc, false), 
        &login_nclpparam
    );

    HWND editor_wnd = create_root_window(
        hInstance, 
        nonclient::calc_nonclient_rc_from_client(editor_cl.rc, true), 
        &show_nclpparam
    );
    
    PostMessage(0, WM_STATE_NEXT, 0, 0); //start the state machine

    enum class wnd_task { login, show_passwords };
    struct wnd_state { HWND wnd; wnd_task task; };

    wnd_state state_machine[] = { 
        {.wnd = login_wnd, .task = wnd_task::login},
        {.wnd = editor_wnd, .task = wnd_task::show_passwords} 
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
            wnd_state new_state = state_machine[state_idx];
            wnd_state old_state = state_machine[prev_state(state_idx)];
            switch (new_state.task) {
            case wnd_task::login:
            {
                PostMessage(nonclient::get_state(old_state.wnd)->client, WM_STATE_RESET, 0, 0);
                ShowWindow(old_state.wnd, SW_HIDE);
                ShowWindow(new_state.wnd, SW_SHOW);
                #ifdef _DEBUG
                debug_track_wnd = login_wnd;
                #endif
            } break;
            case wnd_task::show_passwords:
            {
                SetCursor(loading_cursor); defer{ SetCursor(arrow_cursor); };
                sha256(login_results.password.str, login_results.password.sz_chars * sizeof(*login_results.password.str), editor_start.key);
                editor_data.start->username = login_results.username;
                editor_data.start->signup = login_results.signup;

                // Attempt to start the next state
                auto start_attempt = (login::AttemptResult)SendMessage(nonclient::get_state(new_state.wnd)->client, WM_STATE_START, 0, 0);

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
                TranslateMessage(&msg);
                DispatchMessage(&msg);
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

    save_to_file_serialized(serialized, serialization_folder);

    return (int) msg.wParam;
}
