// Information for efectively working in this codebase and with Windows

#include "win_sdk.h"

/**
  * Custom window state structs must be initialized to zero when created
  */
struct State { /*...*/ };
State& state = *(State*)calloc(1, sizeof(State));


/**
  * Avoid plain enums as they pollute the current namespace, but
  * if you need to be able to combine multiple enum flags then
  * use this instead of having to fight with enum class and casting
  */
struct Placement
{
	static const int left = 1 << 1;
	static const int right = 1 << 2;
	static const int top = 1 << 3;
	static const int bottom = 1 << 4;
};
// Or:
namespace Placement {
	enum Placement {
		left = 1 << 1,
		top = 1 << 2,
		right = 1 << 3,
		bottom = 1 << 4,
	};
}


/**
  * Window Menu
  */
// NOTE: Windows' menu bar is terrible, it doesnt care to update for nothing
// Calling SetMenu after one has already been set doesn't really work, the user should call our set_menu and get_menu instead of Windows' SetMenu and GetMenu


/**
  * Window Proc Messages
  */
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_NCCREATE:
	{ //1st msg received
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		return TRUE; //continue creation
	} break;
	case WM_NCDESTROY://Last msg. Sent _after_ WM_DESTROY
	{
		return 0;
	}break;
	case WM_NCCALCSIZE: { //2nd msg received https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize
		if (wparam) {
			//Indicate part of current client area that is valid
			NCCALCSIZE_PARAMS* calcsz = (NCCALCSIZE_PARAMS*)lparam;
			return 0; //causes the client area to resize to the size of the window, including the window frame
		}
		else {
			RECT* client_rc = (RECT*)lparam;
			return 0;
		}
	} break;
	case WM_SIZE: //4th, sent _after_ the wnd has been resized
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOVE:  //5th, This msg is received _after_ the window was moved //Sent on startup after WM_SIZE, although possibly sent by DefWindowProc after I let it process WM_SIZE, not sure
	{
		//This msg is received _after_ the window was moved
		//Here you can obtain x and y of your window's client area
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_WINDOWPOSCHANGING:
	{
		//Someone calls SetWindowPos with the new values, here you can apply modifications over those
		WINDOWPOS* p = (WINDOWPOS*)lparam;
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_WINDOWPOSCHANGED:
	{
		WINDOWPOS* p = (WINDOWPOS*)lparam; //new window pos, size, etc
		return DefWindowProc(hwnd, msg, wparam, lparam); //TODO(fran): if we handle this msg ourselves instead of calling DefWindowProc we wont need to handle WM_SIZE and WM_MOVE since they wont be sent, also it says it's more efficient https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged
	} break;
	case WM_PARENTNOTIFY: //Notifications about my child controls
	{
		return 0;
	} break;
	case WM_SHOWWINDOW: //6th. On startup you receive this cause of WS_VISIBLE flag //On startup you receive this if you changed the WS_VISIBLE flag on the control creation styles
	{
		//Sent when window is about to be hidden or shown, doesnt let it clear if we are in charge of that or it's going to happen no matter what we do
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCPAINT: //7th
	{
		//Paint non client area, we shouldnt have any
		HDC hdc = GetDCEx(hwnd, (HRGN)wparam, DCX_WINDOW | DCX_USESTYLE);
		ReleaseDC(hwnd, hdc);
		return 0; //we process this message, for now
	} break;
	case WM_ERASEBKGND: //8th, you receive this msg if you didnt specify hbrBackground when you registered the class, now it's up to you to draw the background
	{
		HDC dc = (HDC)wparam;
		return 0; //we dont erase the background here //If you return 0 then on WM_PAINT fErase will be true, aka paint the background there
	} break;
	case WM_NCHITTEST: //When the mouse goes over us this is 1st msg received
	{
		//Received when the mouse goes over the window, on mouse press or release, and on WindowFromPoint

		// Point coordinates for the hit test.
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Screen coords, relative to upper-left corner
	}
	case WM_SETCURSOR: //When the mouse goes over us this is 2nd msg received
	{
		//DefWindowProc passes this to its parent to see if it wants to change the cursor settings, we'll make a decision, setting the mouse cursor, and halting proccessing so it stays like that
		//Sent after getting the result of WM_NCHITTEST, mouse is inside our window and mouse input is not being captured

		/* https://docs.microsoft.com/en-us/windows/win32/learnwin32/setting-the-cursor-image
			if we pass WM_SETCURSOR to DefWindowProc, the function uses the following algorithm to set the cursor image:
			1. If the window has a parent, forward the WM_SETCURSOR message to the parent to handle.
			2. Otherwise, if the window has a class cursor, set the cursor to the class cursor.
			3. If there is no class cursor, set the cursor to the arrow cursor.
		*/
		//NOTE: I think this is good enough for now
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEACTIVATE: //When the user clicks on us this is 1st msg received
	{
		//Sent to us when we're still an inactive window and we get a mouse press
		//TODO(fran): we could also ask our parent (wparam) what it wants to do with us
		HWND parent = (HWND)wparam;
		WORD hittest = LOWORD(lparam);
		WORD mouse_msg = HIWORD(lparam);
		return MA_ACTIVATE; //Activate our window and post the mouse msg
	} break;
	case WM_MOUSEMOVE: /*WM_MOUSEFIRST*/ //When the mouse goes over us this is 3rd msg received 
	{
		//TODO(fran): scroll when mouse clicks the background
		
		//After WM_NCHITTEST and WM_SETCURSOR we finally get that the mouse has moved
		//Sent to the window where the cursor is, unless someone else is explicitly capturing it, in which case this gets sent to them
		//With WPARAM you can test for different button presses, (wparam & MK_LBUTTON) --> left button down
			//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//client coords, relative to upper-left corner
		return 0;
	} break;
	case WM_MOUSELEAVE:
	{
		//Gotta ask TrackMouseEvent for this so you can update when the mouse leaves the client area, which we dont get notified about otherwise
		return 0;
	} break;
	case WM_IME_SETCONTEXT://sent the first time on SetFocus //When we get keyboard focus for the first time this gets sent 
	{
	} break;
	case WM_SETFOCUS: //Triggered, for example, when the user clicks and generates a WM_XBUTTONDOWN //TODO(fran): not sure what to do with this, set focus back to whoever had it, set focus to our parent, do nothing?
	case WM_KILLFOCUS:
	{
		return 0;
	} break;
	case WM_COMMAND://Notifs from our childs
	{
		HWND child = (HWND)lparam;
		if (child) {//a control has sent a notification
			WORD notif = HIWORD(wparam);
		}
	} break;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

/**
  * Window Proc Messages Specific for Non Client Windows
  */
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_NCPAINT: 
	{
		// Check https://chromium.googlesource.com/chromium/chromium/+/5db69ae220c803e9c1675219b5cc5766ea3bb698/chrome/views/window.cc they block drawing so windows doesnt draw on top of them, cause the non client area is also painted in other msgs like settext
		// Also https://social.msdn.microsoft.com/Forums/windows/en-US/a407591a-4b1e-4adc-ab0b-3c8b3aec3153/the-evil-wmncpaint?forum=windowsuidevelopment I took the implementation from there, but there's also two others I can try
	} break;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

/**
  * Richedit (a possible candidate to replace the standard text editor)
  */
#include <Richedit.h> // also includes additional messages: EM_FINDTEXT, EM_EXSETSEL
#include <commdlg.h> //FR_DOWN,... //INFO: FR_DOWN was added in rich edit 2.0, so if you want to do the proper implementation you gotta special case that

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

void setup_window_classes(HINSTANCE hInstance) {
    bool richedit = load_richedit(); runtime_assert(richedit, L"Couldn't find any Rich Edit library");
}
//+

void create_richedit() {
	state.controls.edit_passwords = CreateWindowExW(NULL, get_richedit_classW(), NULL, WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_CLIPCHILDREN | WS_VISIBLE | WS_TABSTOP | ES_NOHIDESEL //| WS_VSCROLL | WS_HSCROLL 
		, 0, 0, 0, 0
		, state.wnd
		, NULL, NULL, NULL);

	#define EDIT_PASSWORDS_MAX_TEXT_LENGTH (32767*2) //32767 is the default
	SendMessageW(state.controls.edit_passwords, EM_EXLIMITTEXT, 0, EDIT_PASSWORDS_MAX_TEXT_LENGTH); //msg completely different from edit control, good job microsoft

	SetWindowSubclass(state.controls.edit_passwords, EditProc, 0, (DWORD_PTR)calloc(1, sizeof(EditProcState)));

	RICHEDIT_set_bk_color(state.controls.edit_passwords, ColorFromBrush(colors.ControlBk));
	//RICHEDIT_set_txt_color(state.controls.edit_passwords, ColorFromBrush(colors.ControlTxt)); //IMPORTANT: this needs to be called each time the text changes
	RICHEDIT_set_txt_bk_color(state.controls.edit_passwords, ColorFromBrush(colors.ControlBk));

	HWND VScrollControl = CreateWindowExW(NULL, wndclass_scrollbar, NULL, WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0, state.controls.edit_passwords, NULL, NULL, NULL);
	SendMessage(VScrollControl, U_SB_SET_PLACEMENT, (WPARAM)Placement::right, 0);
	SendMessageW(state.controls.edit_passwords, EM_SETVSCROLL, (WPARAM)VScrollControl, 0);

	Init searchinit;
	searchinit.parent_type = richedit;
	searchinit.SearchFlag_flags = 0;
	#if 0
	searchinit.SearchPlacement_flags = Placement::right;// Placement::bottom;
	#else
	searchinit.SearchPlacement_flags = Placement::bottom;
	#endif
	HWND SearchControl = CreateWindowExW(NULL, protect_wndclass_search, NULL, WS_CHILD,
		0, 0, 0, 0, state.controls.edit_passwords, NULL, NULL, &searchinit);
	SEARCH_set_brushes(SearchControl, TRUE, colors.Search_Bk, colors.Search_Bk, colors.Search_Txt, colors.Search_BkPush, colors.Search_BkMouseOver, colors.Search_Edit_Bk, colors.Search_Edit_Txt);
	SendMessageW(state.controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)SearchControl, 0);
	//TODO(fran): for some reason IME doesnt get correctly set up in the search wnd
	//TODO(fran): enumchildwindows of your parent to make sure you dont overlap with one another
}

void RICHEDIT_set_bk_color(HWND wnd, COLORREF clr) {
	SendMessage(wnd, EM_SETBKGNDCOLOR, 0, clr);
}

void RICHEDIT_set_txt_color(HWND wnd, COLORREF clr) { //TODO(fran): does not want to work, seems to me like you have to manually set the color each time you add text
	CHARFORMAT cf;
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = clr;
	cf.dwEffects = 0;
	SendMessage(wnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

void RICHEDIT_set_txt_bk_color(HWND wnd, COLORREF clr) {
	CHARFORMAT2 cf;
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_BACKCOLOR;
	cf.crBackColor = clr;
	cf.dwEffects = 0;
	SendMessage(wnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}