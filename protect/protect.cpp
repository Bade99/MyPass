
//TODO(fran): store show_passwords wnd position
//TODO(fran): get a non stolen app icon

#include "resource.h"
#include "targetver.h"
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <windowsx.h>
#include <Shlwapi.h>
#include <vector>
#include <algorithm> //any_of
#include "unCap_Renderer.h"
#include "LANGUAGE_MANAGER.h"
#include "unCap_Global.h"
#include "unCap_Serialization.h"
#include "protect_Core.h"

#define WM_NEXT (WM_USER+5000) //proceed to the next window in the state machine
#define WM_RESET (WM_USER+5001) //clear critical information
#define WM_START (WM_USER+5002) //you are all set up, start whatever it is you do, this is always a SendMessage, you must use all the init data right there, it's not guaranteed to be there after this msg finishes

//+ Fixing windows' bad design: //TODO(fran): move to richedit .h file
LPCWSTR get_richedit_classW(int setclass = 0 /*for internal use, not end user*/) {
    static int v = 1;
    if (setclass) v = setclass;
    switch (v) {
    case 1: return L"RICHEDIT";     //v1.0
    case 2:
    case 3: return L"RichEdit20W"; //v3.0 or 2.0
    case 4: return L"RICHEDIT50W";  //v4.1
    default: return L"";
    }

}

BOOL load_richedit() {//NOTE: use RICHEDIT_CLASS for the window's class name
    int success = false;
    if (!success) { success = (int)(UINT_PTR)LoadLibrary(_t("Msftedit.dll")); if (success)success = 4; } //v4.1
    if (!success) { success = (int)(UINT_PTR)LoadLibrary(_t("Riched20.dll")); if (success)success = 2; } //v3.0 or 2.0
    if (!success) { success = (int)(UINT_PTR)LoadLibrary(_t("Riched32.dll")); if (success)success = 1; } //v1.0
    get_richedit_classW(success);
    return success;
}
//+

//----------------------GLOBALS----------------------:
i32 n_tabs = 0;//Needed for serialization

UNCAP_COLORS unCap_colors{ 0 };
UNCAP_FONTS unCap_fonts{ 0 };

constexpr cstr app_name[] = _t("MyPass");

#ifdef _DEBUG
//TODO(fran): change to logging
#define _SHOWCONSOLE
#else
//#define _SHOWCONSOLE
#endif

#include "unCap_button.h"
#include "unCap_edit_oneline.h"
#include "unCap_uncapnc.h"
#include "protect_login.h"
#include "unCap_scrollbar.h"
#include "protect_show_passwords.h"

//----------------------LINKER----------------------:
#pragma comment(lib, "comctl32.lib" ) //common controls lib
#pragma comment(lib,"shlwapi.lib") //strcpynw
#pragma comment(lib,"UxTheme.lib") // setwindowtheme
#pragma comment(lib,"Imm32.lib") // IME related stuff

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"") //for multiline edit control

str GetFontFaceName() {
    //Font guidelines: https://docs.microsoft.com/en-us/windows/win32/uxguide/vis-fonts
    //Stock fonts: https://docs.microsoft.com/en-us/windows/win32/gdi/using-a-stock-font-to-draw-text

    //TODO(fran): can we take the best codepage from each font and create our own? (look at font linking & font fallback)

    //We looked at 2195 fonts, this is what's left
    //Options:
    //Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
    //-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
    //-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
    //-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

    i64 cnt = StartCounter(); defer{ printf("ELAPSED: %f ms\n",EndCounter(cnt)); };

    HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); }; //You can use any hdc, but not NULL
    std::vector<str> fontnames;
    EnumFontFamiliesEx(dc, NULL
        , [](const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)->int {((std::vector<str>*)lParam)->push_back(lpelfe->lfFaceName); return TRUE; }
    , (LPARAM)&fontnames, NULL);

    const TCHAR* requested_fontname[] = { TEXT("Segoe UI"), TEXT("Arial Unicode MS"), TEXT("Microsoft YaHei"), TEXT("Microsoft YaHei UI")
                                        , TEXT("Microsoft JhengHei"), TEXT("Microsoft JhengHei UI") };

    for (int i = 0; i < ARRAYSIZE(requested_fontname); i++)
        if (std::any_of(fontnames.begin(), fontnames.end(), [f = requested_fontname[i]](str s) {return !s.compare(f); })) return requested_fontname[i];

    return TEXT("");
}

int APIENTRY wWinMain(HINSTANCE hInstance,HINSTANCE,LPWSTR,int)
{
    //if (!cmd && !PathFileExists(cmd)) {
    //    MessageBox(0, _t("You must specify a valid file to decrypt through the command line parameter"), _t("Error - Invalid File"), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    //    return 0;
    //}
    //Assert(sizeof(*cmd) > 1);

    urender::init(); defer{ urender::uninit(); };

#ifdef _SHOWCONSOLE
    AllocConsole();
    FILE* ___s; defer{ fclose(___s); };
    freopen_s(&___s, "CONIN$", "r", stdin);
    freopen_s(&___s, "CONOUT$", "w", stdout);
    freopen_s(&___s, "CONOUT$", "w", stderr);
#endif

    LOGFONT lf{ 0 };
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfHeight = -15;//TODO(fran): parametric
    //INFO: by default if I dont set faceName it uses "Modern", looks good but it lacks some charsets
    StrCpyN(lf.lfFaceName, GetFontFaceName().c_str(), ARRAYSIZE(lf.lfFaceName));

    unCap_fonts.General = CreateFontIndirect(&lf);
    Assert(unCap_fonts.General);

    lf.lfHeight = (LONG)((float)GetSystemMetrics(SM_CYMENU) * .85f);

    unCap_fonts.Menu = CreateFontIndirectW(&lf);
    Assert(unCap_fonts.Menu);

    constexpr wchar_t serialization_folder[] = L"\\MyPass";
    const str to_deserialize = load_file_serialized(serialization_folder);

    LANGUAGE_MANAGER& lang_mgr = LANGUAGE_MANAGER::Instance(); lang_mgr.SetHInstance(hInstance);
    LoginSettings login_cl;
    ShowPasswordsSettings show_cl;

    _BeginDeserialize();
    _deserialize_struct(lang_mgr, to_deserialize);
    _deserialize_struct(login_cl, to_deserialize);
    _deserialize_struct(show_cl, to_deserialize);
    _deserialize_struct(unCap_colors, to_deserialize);
    default_colors_if_not_set(&unCap_colors);
    defer{ for (HBRUSH& b : unCap_colors.brushes) if (b) { DeleteBrush(b); b = NULL; } };

    
    bool richedit = load_richedit(); runtime_assert(richedit, "Couldn't find any Rich Edit library");
    init_wndclass_protect_search(hInstance);
    init_wndclass_unCap_uncapnc(hInstance);
    init_wndclass_unCap_button(hInstance);
    init_wndclass_unCap_edit_oneline(hInstance);
    init_wndclass_protect_login(hInstance);
    init_wndclass_unCap_scrollbar(hInstance);
    init_wndclass_protect_show_passwords(hInstance);

    RECT login_nc_rc = UNCAPNC_calc_nonclient_rc_from_client(login_cl.rc, FALSE);
    RECT show_nc_rc = UNCAPNC_calc_nonclient_rc_from_client(show_cl.rc, TRUE);

    LoginResults login_results{0};
    LoginData login_data{ &login_cl, &login_results };
    unCapNcLpParam login_nclpparam;
    login_nclpparam.client_class_name = protect_wndclass_login;
    login_nclpparam.client_lp_param = &login_data;

    ShowPasswordsStart show_start;
    ShowPasswordsData show_data; show_data.settings = &show_cl; show_data.start = &show_start;

    unCapNcLpParam show_nclpparam;
    show_nclpparam.client_class_name = protect_wndclass_show_passwords;
    show_nclpparam.client_lp_param = &show_data;

    HWND login_nc = CreateWindowEx(WS_EX_CONTROLPARENT, unCap_wndclass_uncap_nc, app_name, WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        login_nc_rc.left, login_nc_rc.top, RECTWIDTH(login_nc_rc), RECTHEIGHT(login_nc_rc), nullptr, nullptr, hInstance, &login_nclpparam);
    Assert(login_nc);

    HWND show_nc = CreateWindowEx(WS_EX_CONTROLPARENT, unCap_wndclass_uncap_nc, app_name, WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        show_nc_rc.left, show_nc_rc.top, RECTWIDTH(show_nc_rc), RECTHEIGHT(show_nc_rc), nullptr, nullptr, hInstance, &show_nclpparam);
    Assert(show_nc);

    UpdateWindow(login_nc);
    UpdateWindow(show_nc);

    PostMessage(0, WM_NEXT, 0, 0); //start the state machine

    MSG msg;
    BOOL bRet;

    enum wnd_op {
        login,show_passwords
    };

    struct wnd_state {
        HWND wnd;
        wnd_op op;
    };

    wnd_state state_machine[] = { {login_nc,login} , {show_nc, show_passwords} };
    int state_idx = -1;
    int state_cnt = ARRAYSIZE(state_machine);
    //auto is_valid_state = [=](int state) -> bool { return state >= 0 && state < state_cnt; };
    auto prev_state = [=](int current_state)->int {current_state--; current_state %= state_cnt; if (current_state < 0) current_state = state_cnt + current_state; return current_state; };
    auto next_state = [=](int current_state)->int {current_state++; current_state %= state_cnt; if (current_state < 0) current_state = state_cnt + current_state; return current_state; };

    //TODO(fran): once we get the password we should encrypt it in some way, and send that to the next wnd. SHA for example?

    // Main message loop:
    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        Assert(bRet != -1);//there was an error

        if (msg.message != WM_NEXT) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            state_idx = next_state(state_idx);
            wnd_state new_state = state_machine[state_idx];
            wnd_state old_state = state_machine[prev_state(state_idx)];
            switch (new_state.op) {
            case login:
            {
                HWND old_wnd = old_state.wnd;
                HWND old_wnd_cl = UNCAPNC_get_state(old_wnd)->client;
                HWND new_wnd = new_state.wnd;
                PostMessage(old_wnd_cl, WM_RESET, 0, 0);
                ShowWindow(old_wnd, SW_HIDE);
                ShowWindow(new_wnd, SW_SHOW);
            } break;
            case show_passwords:
            {
                HWND old_wnd = old_state.wnd;
                HWND old_wnd_cl = UNCAPNC_get_state(old_wnd)->client;
                HWND new_wnd = new_state.wnd;
                HWND new_wnd_cl = UNCAPNC_get_state(new_wnd)->client;
                //int chars_written = SendMessage(old_wnd, WM_GETCREDENTIALS_PASSWORD, (WPARAM)password, ARRAYSIZE(password)); //TODO(fran): maybe this is a better idea
                //TODO(fran): we shouldnt be using this guys directly, probably better is to ask for their *state and get this stuff from there so it's not specific to this code
                sha256(login_results.password.str, login_results.password.sz_chars * sizeof(*login_results.password.str), show_start.key);
                show_data.start->username = login_results.username;

                SendMessage(new_wnd_cl, WM_START, 0, 0); //starting the pipeline

                PostMessage(old_wnd_cl, WM_RESET, 0, 0);
                ShowWindow(old_wnd, SW_HIDE);
                ShowWindow(new_wnd, SW_SHOW);
            } break;
            }
        }
    }

    str serialized;
    _BeginSerialize();
    serialized += _serialize_struct(lang_mgr);
    serialized += _serialize_struct(login_cl);
    serialized += _serialize_struct(unCap_colors);

    save_to_file_serialized(serialized, serialization_folder);

    return (int) msg.wParam;
}
