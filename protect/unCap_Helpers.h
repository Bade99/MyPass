#pragma once
#include <Windows.h>
#include "unCap_Platform.h"

#ifdef _DEBUG
#define UNCAP_ASSERTIONS
#endif

#ifdef UNCAP_ASSERTIONS
#define Assert(assertion) if(!(assertion))*(int*)NULL=0
#else 
#define Assert(assertion) 
#endif

//"runtime" here means we guarantee this code will be executed in any build configuration with assertions enabled or not
#define runtime_assert(assertion,msg) if (!(assertion))MessageBoxA(0,msg,"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND) || (*(int*)NULL = 0)==0

#ifdef UNICODE
#define to_str(v) std::to_wstring(v)
#else
#define to_str(v) std::to_string(v)
#endif

//NOTE: do NOT include the null terminator
#ifdef UNICODE
static size_t cstr_len(const cstr* s) {
	return wcslen(s);
}
#define cstr_printf(...)  wprintf(__VA_ARGS__)
#else
static size_t cstr_len(const cstr* s) {
	return strlen(s);
}

#define cstr_printf(...)  printf(__VA_ARGS__)
#endif

//Thanks to https://handmade.network/forums/t/1273-post_your_c_c++_macro_tricks/3
template <typename F> struct Defer { Defer(F f) : f(f) {} ~Defer() { f(); } F f; };
template <typename F> Defer<F> makeDefer(F f) { return Defer<F>(f); };
#define __defer( line ) defer_ ## line
#define _defer( line ) __defer( line )
struct defer_dummy { };
template<typename F> Defer<F> operator+(defer_dummy, F&& f) { return makeDefer<F>(std::forward<F>(f)); }
//usage: defer{block of code;}; //the last defer in a scope gets executed first (LIFO)
#define defer auto _defer( __LINE__ ) = defer_dummy( ) + [ & ]( )

static u32 safe_u64_to_u32(u64 n) {
	Assert(n <= 0xFFFFFFFF); //TODO(fran): u32_max and _min
	return (u32)n;
}

static void free_file_memory(void* memory) {
	if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}
struct read_entire_file_res { void* mem; u32 sz;/*bytes*/ };
//NOTE: free the memory with free_file_memory()
static read_entire_file_res read_entire_file(const cstr* filename) {
	read_entire_file_res res{ 0 };
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		defer{ CloseHandle(hFile); };
		if (LARGE_INTEGER sz; GetFileSizeEx(hFile, &sz)) {
			u32 sz32 = safe_u64_to_u32(sz.QuadPart);
			void* mem = VirtualAlloc(0, sz32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);//TOOD(fran): READONLY?
			if (mem) {
				if (DWORD bytes_read; ReadFile(hFile, mem, sz32, &bytes_read, 0) && sz32 == bytes_read) {
					//SUCCESS
					res.mem = mem;
					res.sz = sz32;
				}
				else {
					free_file_memory(mem);
				}
			}
		}
	}
	return res;
}
static bool write_entire_file(const cstr* filename, void* memory, u32 mem_sz) {
	bool res = false;
	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		defer{ CloseHandle(hFile); };
		if (DWORD bytes_written; WriteFile(hFile, memory, mem_sz, &bytes_written, 0)) {
			//SUCCESS
			res = mem_sz == bytes_written;
		}
		else {
			//TODO(fran): log
		}
	}
	return res;
}
static bool append_to_file(const cstr* filename, void* memory, u32 mem_sz) {
	bool res = false;
	HANDLE hFile = CreateFile(filename, FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		defer{ CloseHandle(hFile); };
		if (DWORD bytes_written; WriteFile(hFile, memory, mem_sz, &bytes_written, 0)) {
			//SUCCESS
			res = mem_sz == bytes_written;
		}
		else {
			//TODO(fran): log
		}
	}
	return res;
}

//Timing info for testing
static f64 GetPCFrequency() {
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return f64(li.QuadPart) / 1000.0; //milliseconds
}
static i64 StartCounter() {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}
static f64 EndCounter(i64 CounterStart, f64 PCFreq = GetPCFrequency())
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}

static bool str_found(size_t p) {
	return p != str::npos;
}

//NOTE: offset should be 1 after the last character of "begin"
//NOTE: returns str::npos if not found
static size_t find_closing_str(str text, size_t offset, str begin, str close) {
	//Finds closing str, eg: open= { ; close= } ; you're here-> {......{..}....} <-finds this one
	size_t closePos = offset - 1;
	i32 counter = 1;
	while (counter > 0 && text.size() > ++closePos) {
		if (!text.compare(closePos, begin.size(), begin)) {
			counter++;
		}
		else if (!text.compare(closePos, close.size(), close)) {
			counter--;
		}
	}
	return !counter ? closePos : str::npos;
}

//Returns first non matching char, could be 1 past the size of the string
static size_t find_till_no_match(str s, size_t offset, str match) {
	size_t p = offset - 1;
	while (s.size() > ++p) {
		size_t r = (str(1, s[p])).find_first_of(match.c_str());//NOTE: find_first_of is really badly designed
		if (!str_found(r)) {
			break;
		}
	}
	return p;
}

//RECT related

#define RECTWIDTH(r) (r.right >= r.left ? r.right - r.left : r.left - r.right )

#define RECTHEIGHT(r) (r.bottom >= r.top ? r.bottom - r.top : r.top - r.bottom )

static RECT rectWH(LONG left, LONG top, LONG width, LONG height) {
	RECT r;
	r.left = left;
	r.top = top;
	r.right = r.left + width;
	r.bottom = r.top + height;
	return r;
}

static RECT rectNpxL(RECT r, int n) {
	RECT res{ r.left,r.top,r.left + n,r.bottom };
	return res;
}
static RECT rectNpxT(RECT r, int n) {
	RECT res{ r.left,r.top,r.right,r.top + n };
	return res;
}
static RECT rectNpxR(RECT r, int n) {
	RECT res{ r.right - n,r.top,r.right,r.bottom };
	return res;
}
static RECT rectNpxB(RECT r, int n) {
	RECT res{ r.left ,r.bottom - n,r.right,r.bottom };
	return res;
}

static RECT rect1pxL(RECT r) {
	RECT res = rectNpxL(r, 1);
	return res;
}
static RECT rect1pxT(RECT r) {
	RECT res = rectNpxT(r, 1);
	return res;
}
static RECT rect1pxR(RECT r) {
	RECT res = rectNpxR(r, 1);
	return res;
}
static RECT rect1pxB(RECT r) {
	RECT res = rectNpxB(r, 1);
	return res;
}

#define BORDERLEFT 0x01
#define BORDERTOP 0x02
#define BORDERRIGHT 0x04
#define BORDERBOTTOM 0x08
//NOTE: borders dont get centered, if you choose 5 it'll go 5 into the rc. TODO(fran): centered borders
static void FillRectBorder(HDC dc, RECT r, int thickness, HBRUSH br, int borders) {
	RECT borderrc;
	if (borders & BORDERLEFT) { borderrc = rectNpxL(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERTOP) { borderrc = rectNpxT(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERRIGHT) { borderrc = rectNpxR(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERBOTTOM) { borderrc = rectNpxB(r, thickness); FillRect(dc, &borderrc, br); }
}

static bool test_pt_rc(POINT p, RECT r) {
	bool res = false;
	if (p.y >= r.top &&//top
		p.y < r.bottom &&//bottom
		p.x >= r.left &&//left
		p.x < r.right)//right
	{
		res = true;
	}
	return res;
}

static bool rcs_overlap(RECT r1, RECT r2) {
	bool res = (r1.left < r2.right&& r1.right > r2.left && r1.top < r2.bottom&& r1.bottom > r2.top);
	return res;
}

static bool sameRc(RECT r1, RECT r2) {
	bool res = r1.bottom == r2.bottom && r1.left == r2.left && r1.right == r2.right && r1.top == r2.top;
	return res;
}

static RECT max_area(RECT* rcs, int len) { //Finds the rectangle with the biggest area in an array of RECTs, if two rcs have the same area it chooses the one first on the array //NOTE: if len is 0 it will crash
	int idx = 0;
	int max_area = 0;
	if (rcs) {
		for (int i = 0; i < len; i++) {
			RECT* test = rcs + i;
			int area = RECTWIDTH((*test)) * RECTHEIGHT((*test));
			if (area > max_area) { max_area = area; idx = i; }
		}
	}
	return *(rcs + idx); //TODO(fran): might be better to simply return the pointer
}

//Clips your rect "r" so it doesnt overlap the already present child windows of that parent
//"child" should be your wnd so we can filter it out
//NOTE: since we are talking about childs, the coordinates of "r" should be relative to the client area of the parent
static RECT clip_fit_childs(HWND parent, HWND child, RECT r) {
	RECT res;
	struct clip_fit_childs_test { HWND parent; HWND child; HRGN rgn; };
	clip_fit_childs_test t; t.parent = parent;  t.child = child; t.rgn = CreateRectRgn(r.left, r.top, r.right, r.bottom); defer{ DeleteObject(t.rgn); };

	EnumChildWindows(
		parent,
		[](HWND other_child, LPARAM lparam) {
			clip_fit_childs_test* test = (clip_fit_childs_test*)lparam;
			if (other_child != test->child && GetParent(other_child)==test->parent /*avoid enumerating childs of childs*/) {//TODO(fran): we should also filter non visible wnds I think, if everybody starts using this it could work well, maybe not
				RECT subtract_rc;
				GetClientRect(other_child, &subtract_rc);
				MapWindowPoints(other_child, test->parent, (LPPOINT)&subtract_rc, 2);

				HRGN subtract_rgn = CreateRectRgn(subtract_rc.left, subtract_rc.top, subtract_rc.right, subtract_rc.bottom); defer{ DeleteObject(subtract_rgn); };
				CombineRgn(test->rgn, test->rgn, subtract_rgn, RGN_DIFF);
			}
			return TRUE;
		},
		(LPARAM)&t);

	int rgn_len = GetRegionData(t.rgn, 0, 0);
	//INFO:RGNDATA always stores it's region polygon in RECTs
	RGNDATA* rgn_data = (RGNDATA*)malloc(rgn_len); defer{ free(rgn_data); };
	rgn_data->rdh.dwSize = sizeof(rgn_data->rdh); //TODO(fran): not sure I need to set anything in the header
	GetRegionData(t.rgn, rgn_len, rgn_data);
	//TODO(fran): check what happens when the region is empty, is there only one RECT with all zeroes or is it a null pointer and we crash
	if (rgn_data->rdh.nCount) res = max_area((RECT*)rgn_data->Buffer, rgn_data->rdh.nCount);
	else res = { 0,0,0,0 };

	return res;
}


//HBRUSH related

static COLORREF ColorFromBrush(HBRUSH br) {
	LOGBRUSH lb;
	GetObject(br, sizeof(lb), &lb);
	return lb.lbColor;
}

//ICON related

struct MYICON_INFO //This is so stupid I find it hard to believe
{
	int     nWidth;
	int     nHeight;
	int     nBitsPerPixel;
};

static MYICON_INFO MyGetIconInfo(HICON hIcon)
{
	MYICON_INFO myinfo; ZeroMemory(&myinfo, sizeof(myinfo));

	ICONINFO info; ZeroMemory(&info, sizeof(info));

	BOOL bRes = FALSE; bRes = GetIconInfo(hIcon, &info);
	if (!bRes) return myinfo;

	BITMAP bmp; ZeroMemory(&bmp, sizeof(bmp));

	if (info.hbmColor)
	{
		const int nWrittenBytes = GetObject(info.hbmColor, sizeof(bmp), &bmp);
		if (nWrittenBytes > 0)
		{
			myinfo.nWidth = bmp.bmWidth;
			myinfo.nHeight = bmp.bmHeight;
			myinfo.nBitsPerPixel = bmp.bmBitsPixel;
		}
	}
	else if (info.hbmMask)
	{
		// Icon has no color plane, image data stored in mask
		const int nWrittenBytes = GetObject(info.hbmMask, sizeof(bmp), &bmp);
		if (nWrittenBytes > 0)
		{
			myinfo.nWidth = bmp.bmWidth;
			myinfo.nHeight = bmp.bmHeight / 2;
			myinfo.nBitsPerPixel = 1;
		}
	}

	if (info.hbmColor) DeleteObject(info.hbmColor);
	if (info.hbmMask) DeleteObject(info.hbmMask);

	return myinfo;
}

//HMENU related

static BOOL SetMenuItemData(HMENU hmenu, UINT item, BOOL fByPositon, ULONG_PTR data) {
	MENUITEMINFO i;
	i.cbSize = sizeof(i);
	i.fMask = MIIM_DATA;
	i.dwItemData = data;
	return SetMenuItemInfo(hmenu, item, fByPositon, &i);
}

static BOOL SetMenuItemString(HMENU hmenu, UINT item, BOOL fByPositon, const TCHAR* str) {
	MENUITEMINFOW menu_setter;
	menu_setter.cbSize = sizeof(menu_setter);
	menu_setter.fMask = MIIM_STRING;
	menu_setter.dwTypeData = const_cast<TCHAR*>(str);
	BOOL res = SetMenuItemInfoW(hmenu, item, fByPositon, &menu_setter);
	return res;
}

//HWND related

static void SetText_txt_app(HWND wnd, const TCHAR* new_txt, const TCHAR* new_appname, bool txt_first = true) {
	if (new_txt == NULL || *new_txt == NULL) {
		SetWindowText(wnd, new_appname);
	}
	if (new_appname == NULL || *new_appname == NULL) {
		SetWindowText(wnd, new_txt);
	}
	else {
		if (!txt_first) {
			const TCHAR* tmp = new_txt;
			new_txt = new_appname;
			new_appname = tmp;
		}
		str title_window = new_txt;
		title_window += L" - ";
		title_window += new_appname;
		SetWindowTextW(wnd, title_window.c_str());
	}
}