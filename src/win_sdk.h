#pragma once

#define DCX_USESTYLE 0x00010000 // Crucial undocumented feature for WM_NCPAINT: HDC dc = GetDCEx(hwnd, 0, DCX_WINDOW | DCX_USESTYLE);

#define WM_UAHDESTROYWINDOW 0x90
#define WM_UAHDRAWMENU 0x91
#define WM_UAHDRAWMENUITEM 0x92
#define WM_UAHINITMENU 0x93
#define WM_UAHMEASUREMENUITEM 0x94
#define WM_UAHNCPAINTMENUPOPUP 0x95
#define WM_UAHUPDATE 0x96

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX // Exclude macro definitions for min and max

//Messages that, like window's default messages, can and should if possible be implemented by any type of HWND
#define extramsgs_base_msg_addr (WM_USER + 5000)

// wparam = unused
// lparam = pointer to null terminated cstr
// returns TRUE if the text was set, and FALSE otherwise
#define WM_SETDEFAULTTEXT (extramsgs_base_msg_addr+1) 

// wparam = unused
// lparam = pointer to null terminated cstr
// returns TRUE if the text was set, and FALSE otherwise
#define WM_SETTOOLTIPTEXT (extramsgs_base_msg_addr+2)

#define WM_DESIRED_SIZE (extramsgs_base_msg_addr+3) 
// wparam = pointer to SIZE with maximum bounds, should be modified to represent the minimum SIZE the control can take
// lparam = pointer to SIZE with maximum bounds, should be modified to represent the maximum SIZE the control can take
// returns:
	// 0 : dont care
	// 1 : flexible : the two SIZE params represent the lower and upper bound but also allow for values in between
	// 2 : fixed    : the two SIZE params represent the small and big size and dont allow for values in between

#define WM_ENABLE_REQUEST (extramsgs_base_msg_addr+4)
// wparam = bool, should enable or disable itself and child controls at discretion of the control
// lparam = unused

#define wndclass_name(name) _APP_NAME _t("_wndclass_") _t(name)


// // Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>

#include <windows.h>
#include <windowsx.h>
//#include <uxtheme.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include "win_msg_mapper.h"

/**
  * Linker Configuration
  */
#pragma comment(lib, "comctl32.lib" ) //common controls lib
#pragma comment(lib,"shlwapi.lib") //strcpynw //TODO(fran): use strcpy options from some other lib we dont need to manually link to
//#pragma comment(lib,"UxTheme.lib") // setwindowtheme
#pragma comment(lib,"Imm32.lib") // IME related stuff
#pragma comment(lib,"Msimg32.lib") // AlphaBlend

//#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"") //for multiline edit control

enum desired_size : unsigned int { dontcare = 0, flexible, fixed };
static desired_size GetWindowDesiredSize(HWND wnd, SIZE* min, SIZE* max) {
	return (desired_size)SendMessageW(wnd, WM_DESIRED_SIZE, (WPARAM)min, (LPARAM)max);
}

struct _desired_size { SIZE min, max; desired_size flexibility; };
static _desired_size GetWindowDesiredSize(HWND wnd, SIZE min, SIZE max) {
	_desired_size res;
	res.flexibility = (desired_size)SendMessageW(wnd, WM_DESIRED_SIZE, (WPARAM)&min, (LPARAM)&max);
	res.min = min;
	res.max = max;
	return res;
}


static BOOL SetCaretPos(POINT p) {
	BOOL res = SetCaretPos(p.x, p.y);
	return res;
}


static bool operator==(POINT p1, POINT p2) {
	bool res = p1.x == p2.x && p1.y == p2.y;
	return res;
}

static bool operator!=(POINT p1, POINT p2) {
	bool res = !(p1 == p2);
	return res;
}


static bool operator==(SIZE s1, SIZE s2) {
	bool res = s1.cx == s2.cx && s1.cy == s2.cy;
	return res;
}

static bool operator!=(SIZE s1, SIZE s2) {
	bool res = !(s1 == s2);
	return res;
}


static bool operator==(RECT r1, RECT r2) {
	bool res = r1.bottom == r2.bottom && r1.left == r2.left && r1.right == r2.right && r1.top == r2.top;
	return res;
}

static bool operator!=(RECT r1, RECT r2) {
	bool res = !(r1 == r2);
	return res;
}