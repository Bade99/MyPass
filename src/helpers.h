#pragma once
#include "win_sdk.h"
#include "platform.h"
#include "macros.h"
#include "reflection.h"
#include "math.h"
#include "vector.h"

/**
  * File Handling
  */

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


/**
  * Timing Information
  */

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
static f64 EndCounter(i64 CounterStart, f64 PCFreq = GetPCFrequency()) {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}


/**
  * String Manipulation
  */

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

static utf16* strip(utf16* s) {
	// Trim leading space
	while (iswspace(*s)) s++;

	if (*s == 0) return s; // All spaces

	// Trim trailing space
	auto end = s + wcslen(s) - 1;
	while (end > s && iswspace(*end)) end--;

	// Write new null terminator
	end[1] = 0;

	return s;
}

//True if "c" is in "chars", stops on the null terminator(if c == '\0' it will always return false)
static bool contains_char(const cstr* chars, size_t char_cnt, cstr c) {
	bool res = false;
	if (chars) for (size_t i = 0; i < char_cnt; i++) if (chars[i] == c) { res = true; break; }
	return res;
}


/**
  * RECT
  */

#define RECTW(r) (distance(r.right, r.left))
#define RECTH(r) (distance(r.bottom, r.top))

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
#define BORDERALL		(BORDERLEFT|BORDERTOP|BORDERRIGHT|BORDERBOTTOM)
//NOTE: borders dont get centered, if you choose 5 it'll go 5 into the rc. TODO(fran): centered borders ?
static void FillRectBorder(HDC dc, RECT r, int thickness, HBRUSH br, int borders) {
	RECT borderrc;
	if (borders & BORDERLEFT) { borderrc = rectNpxL(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERTOP) { borderrc = rectNpxT(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERRIGHT) { borderrc = rectNpxR(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERBOTTOM) { borderrc = rectNpxB(r, thickness); FillRect(dc, &borderrc, br); }
}

static bool test_pt_rc(POINT p, const RECT& r) {
	bool res = (p.y >= r.top &&
				p.y < r.bottom &&
				p.x >= r.left &&
				p.x < r.right);
	return res;
}

static bool rcs_overlap(RECT r1, RECT r2) {
	bool res = (r1.left < r2.right&& r1.right > r2.left && r1.top < r2.bottom&& r1.bottom > r2.top);
	return res;
}

static bool test_pt_rc(POINT p, const rect_i32& r) {
	bool res = false;
	if (p.y >= r.top &&//top
		p.y < r.bottom() &&//bottom
		p.x >= r.left &&//left
		p.x < r.right())//right
	{
		res = true;
	}
	return res;
}

static RECT get_window_rect_at(HWND wnd, HWND reference) {
	RECT r{}; GetWindowRect(wnd, &r);
	MapWindowPoints(0, reference, (POINT*)&r, 2);
	return r;
}

/**
  * Finds the rectangle with the biggest area in an array of RECTs, 
  * if two have the same area it chooses the one first on the array
  */
static RECT max_area(RECT* rcs, int len) { 
	int idx = 0;
	int max_area = 0;
	if (!rcs || !len) return {};
	for (int i = 0; i < len; i++) {
		RECT* test = rcs + i;
		int area = RECTW((*test)) * RECTH((*test));
		if (area > max_area) { max_area = area; idx = i; }
	}
	return *(rcs + idx); //TODO(fran): might be better to simply return the pointer
}

/**
  * Calculates the bounding box the encompasses all the rectangles in the array of RECTs
  */
static RECT bounding_box(const std::initializer_list<RECT>& rcs) {
	RECT bounds{};
	for (auto& test : rcs) {
		if (bounds.left > test.left) bounds.left = test.left;
		if (bounds.right < test.right) bounds.right = test.right;
		if (bounds.top > test.top) bounds.top = test.top;
		if (bounds.bottom < test.bottom) bounds.bottom = test.bottom;
	}
	return bounds;
}

/**
  * Calculates the bounding box the encompasses all the rectangles in the array of RECTs
  * based on their position with respect to the reference window
  */
static RECT bounding_box(HWND reference, HWND* wnds, int cnt) {
	Assert(wnds);
	RECT bounds{.left = _I32_MAX, .top = _I32_MAX, .right = _I32_MIN, .bottom = _I32_MIN };
	for (int i = 0; i < cnt; i++) {
		auto test = get_window_rect_at(wnds[i], reference);
		if (bounds.left > test.left) bounds.left = test.left;
		if (bounds.right < test.right) bounds.right = test.right;
		if (bounds.top > test.top) bounds.top = test.top;
		if (bounds.bottom < test.bottom) bounds.bottom = test.bottom;
	}
	return bounds;
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

static void validate_rect_in_screen(RECT& target, RECT default_rect) {
	auto default_w = RECTW(default_rect), default_h = RECTH(default_rect);
	if (RECTW(target) < 100) target.right = target.left + default_w;
	if (RECTH(target) < 100) target.bottom = target.top + default_h;

	HMONITOR hMonitor = MonitorFromRect(&target, MONITOR_DEFAULTTONULL);

	if (!hMonitor) {
		target = default_rect;
		return;
	}

	if (MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(hMonitor, &mi)) {
		auto& bounds = mi.rcWork;
		auto fudge = 15;
		InflateRect(&bounds, fudge, fudge);
		if (target.left < bounds.left || target.right > bounds.right) {
			target.left = (bounds.left + bounds.right - default_w) / 2;
			target.right = target.left + default_w;
		}
		if (target.top < bounds.top || target.bottom > bounds.bottom) {
			target.top = (bounds.top + bounds.bottom - default_h) / 2;
			target.bottom = target.top + default_h;
		}
	}
}

// w : width | h : height | n : element count | p : padding
auto pack_squares(i32 w, i32 h, i32 n, i32 p) {
	struct PackingResult { i32 rows, cols, dim; } best{};

	// Large enough to prioritize size over aspect mismatch
	constexpr i64 K = 1'000'000;

	i64 best_score = I64MIN;

	for (int c = 1; c <= n; c++) {
		int r = (n + c - 1) / c;

		int usable_w = w - (c + 1) * p;
		int usable_h = h - (r + 1) * p;

		if (usable_w <= 0 || usable_h <= 0) continue;

		int dim_w = usable_w / c;
		int dim_h = usable_h / r;
		int dim = minimum(dim_w, dim_h);

		if (dim <= 0) continue;

		// Integer aspect mismatch
		auto aspect_error = abs(((i64)c) * h - ((i64)r) * w);

		auto score = ((i64)dim) * K - aspect_error;

		if (score > best_score) {
			best_score = score;
			best = { .rows=r, .cols=c, .dim=dim };
		}
	}

	return best;
}


/**
  * HMONITOR
  */

static u32 refresh_rate_hz(HWND wnd) {
	//TODO(fran): this may be simpler with GetDeviceCaps
	u32 res = 60;
	HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
	if (mon) {
		MONITORINFOEX nfo;
		nfo.cbSize = sizeof(nfo);
		if (GetMonitorInfo(mon, &nfo)) {
			DEVMODE devmode;
			devmode.dmSize = sizeof(devmode);
			devmode.dmDriverExtra = 0;
			if (EnumDisplaySettings(nfo.szDevice, ENUM_CURRENT_SETTINGS, &devmode)) {
				res = devmode.dmDisplayFrequency;
			}
		}
	}
	return res;
}


/**
  * HBRUSH
  */

static COLORREF ColorFromBrush(HBRUSH br) {
	LOGBRUSH lb;
	GetObject(br, sizeof(lb), &lb);
	return lb.lbColor;
}

union brush_group {
	struct {
		HBRUSH normal, disabled, mouseover, clicked;
	};
	HBRUSH all[4]{ 0 };

	/**
	 * Copy all non null elements, ignore the rest.
	 * Returns true if any modification was made.
	 */
	bool copy_from(const brush_group& src)
	{
		bool copied = false;
		for (auto i = 0; i < ARRAYSIZE(this->all); i++) 
			if (src.all[i] && src.all[i] != this->all[i]) {
				this->all[i] = src.all[i]; 
				copied = true; 
			}
		return copied;
	}

private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
};


/**
  * HICON
  */



static auto MyGetIconInfo(HICON hIcon)
{
	struct MYICON_INFO { i32 w, h, bpp; } myinfo; ZeroMemory(&myinfo, sizeof(myinfo));

	ICONINFO info; ZeroMemory(&info, sizeof(info));
	defer{
		if (info.hbmColor) DeleteObject(info.hbmColor);
		if (info.hbmMask) DeleteObject(info.hbmMask);
	};

	if (!GetIconInfo(hIcon, &info)) return myinfo;

	BITMAP bmp; ZeroMemory(&bmp, sizeof(bmp));

	if (info.hbmColor) {
		if (GetObject(info.hbmColor, sizeof(bmp), &bmp) > 0) {
			myinfo.w = bmp.bmWidth;
			myinfo.h = bmp.bmHeight;
			myinfo.bpp = bmp.bmBitsPixel;
		}
	}
	elif (info.hbmMask) { // Icon has no color plane, image data stored in mask
		if (GetObject(info.hbmMask, sizeof(bmp), &bmp) > 0) {
			myinfo.w = bmp.bmWidth;
			myinfo.h = bmp.bmHeight / 2;
			myinfo.bpp = 1;
		}
	}

	return myinfo;
}

/**
  * HMENU
  */

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

/**
  * DPI
  */

static f32 dpiCorrection(f32 val) {
	f32 res = val * (f32)GetDpiForSystem() / 96/*default dpi*/;
	return res;
}
static f32 DPI(f32 val) { return dpiCorrection(val); }


struct UINumber {
	enum class type {
		px,     // Specific pixel value
		dpi,    // DPI aware (scaled) pixel value
		dyn,    // Fills whatever space is left
		percent	// Percentage of a reference dimension
	} type = type::dpi;
	f32 value;

	f32 to_px(u32 reference = 0) const {
		switch (type) {
		case type::px:      return value;
		case type::dpi:     return DPI(value);
		case type::dyn:     return reference; //Not meant to be used like this, it's mostly a marker to start further calculation to assign a size to multiple dyn elements in contention for the same space
		case type::percent: return reference * (value / 100.f);
		default: Assert(0); return 0;
		}
	}
};

static SIZE avg_str_dim(HFONT font, size_t char_cnt) {
	SIZE res;

	HDC dc = GetDC(nil); defer{ ReleaseDC(nil,dc); };
	HFONT old_font = (HFONT)SelectObject(dc, font); defer{ SelectObject(dc,old_font); };
	TEXTMETRIC tm{}; GetTextMetrics(dc, &tm);
	res.cy = tm.tmHeight;
	res.cx = (int)(tm.tmAveCharWidth * char_cnt);

	return res;
}

/**
  * HWND
  */

struct WindowState {
	HWND wnd, parent;
	HWND manager_parent; //window in charge of resizing on size request changes from this control
};

template <typename Func = void(*)(void*)>
struct StatefulFunction {
	void* state;
	Func function;

	template <typename... Args>
	decltype(auto) operator()(Args&&... args) const {
		return function(state, std::forward<Args>(args)...);
	}

	explicit operator bool() const { return function; }
};

static void init_wndclass(LPCWSTR class_name, WNDPROC proc, UINT extra_styles = 0, LPWSTR default_cursor = IDC_ARROW, HINSTANCE inst = GetModuleHandleW(nil)) {
	//INFO: Now that we use pre_post_main we cant depend on anything that isnt calculated at compile time for class creation
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(wcex);
	wcex.style = CS_HREDRAW | CS_VREDRAW | extra_styles;
	wcex.lpfnWndProc = proc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(void*);
	wcex.hInstance = inst;
	wcex.hIcon = nil;
	wcex.hCursor = LoadCursor(nullptr, default_cursor);
	wcex.hbrBackground = nil;
	wcex.lpszMenuName = nil;
	wcex.lpszClassName = class_name;
	wcex.hIconSm = nil;
	ATOM class_atom = RegisterClassExW(&wcex);
	runtime_assert(class_atom, (str(L"Failed to initialize class ") + class_name).c_str());
}

/**
  * Automatically registers the wndclass of the corresponding control at startup.
  * Classes are de-registered automatically by the os, so no need to do it ourselves in the destructor
  */
#define _init_wndclass_at_startup \
	struct pre_post_main {	\
		pre_post_main() { init_wndclass(wndclass, proc); } \
		~pre_post_main() {} \
	} static const PREMAIN_POSTMAIN; 

static HWND create_root_window(HINSTANCE instance, RECT r, void* data = nil, const utf16* title = app_name, u32 extra_styles = 0, u32 extra_extended_styles = 0) {
	HWND res = CreateWindowEx(
		WS_EX_CONTROLPARENT | extra_extended_styles,
		//TODO(fran): had to manually hack in the wndclass name for nonclient since at this point it's not yet defined
		wndclass_name("nonclient"), title,
		WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | extra_styles,
		r.left, r.top, RECTW(r), RECTH(r), nil, nil, instance, data
	);
	Assert(res);
	UpdateWindow(res);
	return res;
}

static HWND create_window(HWND parent, const utf16* wndclass, const utf16* title = nil, u32 styles = WS_VISIBLE | WS_CHILD, u32 extended_styles = 0, u64 wnd_id = 0, HWND manager_parent = nil) {
	auto res = CreateWindowExW(
		WS_EX_COMPOSITED | WS_EX_TRANSPARENT | extended_styles,
		wndclass, title, styles
		, 0, 0, 0, 0, parent, (HMENU)wnd_id, nil, manager_parent);
	Assert(res);
	return res;
}

static void move_window(HWND wnd, RECT r, bool repaint = true) {
	MoveWindow(wnd, r.left, r.top, RECTW(r), RECTH(r), repaint);
}

static auto MoveWindow(HWND wnd, rect_i32 r, bool repaint = true) {
	return MoveWindow(wnd, r.x, r.y, r.w, r.h, repaint);
}

template<typename T> 
static T* get_window_state(HWND wnd) {
	T* state = (T*)GetWindowLongPtr(wnd, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

static void set_window_state(HWND wnd, void* state) {
	SetWindowLongPtr(wnd, 0, (LONG_PTR)state);
}

static void ask_window_for_repaint(HWND wnd) { InvalidateRect(wnd, nullptr, true); }

static void ask_window_for_resize(HWND wnd) { PostMessageW(wnd, WM_SIZE, 0, 0); }

static void set_window_manager_parent(HWND wnd, HWND manager_parent) {
	WindowState& state = *get_window_state<WindowState>(wnd); Assert(&state);
	state.manager_parent = manager_parent;
}

static void SetText_txt_app(HWND wnd, const TCHAR* new_txt, const TCHAR* new_appname, bool txt_first = true) {
	if (!new_txt || !*new_txt) {
		SetWindowText(wnd, new_appname);
	}
	elif (!new_appname || !*new_appname) {
		SetWindowText(wnd, new_txt);
	}
	else {
		if (!txt_first) {
			auto tmp = new_txt;
			new_txt = new_appname;
			new_appname = tmp;
		}
		str title_window = new_txt;
		title_window += L" - ";
		title_window += new_appname;
		SetWindowTextW(wnd, title_window.c_str());
	}
}

struct tooltip_props {
	bool multiline = false;
	u32 delay_ms = U32MAX; // delay between start of mouse over and showing the tooltip
	u32 duration_ms = U32MAX; // time that the tooltip remains visible
};
static auto add_mouseover_tooltip(HWND target, u64 msg_resource_id, tooltip_props props = {}) {
	auto tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		target, nil, nil, nil);
	Assert(tooltip);

	TOOLINFO toolInfo{ sizeof(toolInfo) };
	toolInfo.hwnd = target;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = (decltype(toolInfo.uId))target;
	toolInfo.hinst = GetModuleHandle(nil);
	toolInfo.lpszText = (decltype(toolInfo.lpszText))msg_resource_id;
	auto addtool_res = SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	Assert(addtool_res);

	if (props.multiline)
		SendMessage(tooltip, TTM_SETMAXTIPWIDTH, 0, DPI(1000)); //Enables multiline
	if (props.delay_ms != U32MAX)
		SendMessage(tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, (LPARAM)props.delay_ms);
	if (props.duration_ms != U32MAX)
		SendMessage(tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)props.duration_ms);

	return tooltip;
}

/**
  * Clipboard
  */

/**
  * Copies text to the clipboard
  * char_cnt: number of characters to copy excluding the null terminator
  * Returns: number of characters copied, 0 if there was nothing to copy, -1 on failure
  **/
static size_t copy_text_to_clipboard(const cstr* text, size_t char_cnt) {
	#ifdef UNICODE
	UINT format = CF_UNICODETEXT;
	#else
	UINT format = CF_TEXT;
	#endif
	constexpr auto char_sz = sizeof(*text);
	constexpr size_t failure = -1;
	const auto total_chars_copied = char_cnt + 1;

	if (!text || !*text) return 0;

	if (OpenClipboard(nil)) {
		defer{ CloseClipboard(); };
		HANDLE set_clipboard_res = nil;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, total_chars_copied * char_sz);
		if (!mem) return failure;
		defer{ if (!set_clipboard_res) GlobalFree(mem); };

		{
			auto txt = (cstr*)GlobalLock(mem);
			if (!txt) return failure;
			defer{ GlobalUnlock(mem); };

			memcpy(txt, text, char_cnt * char_sz); //copy selection
			txt[char_cnt] = 0; //null terminate
		}

		set_clipboard_res = SetClipboardData(format, mem);
		if (!set_clipboard_res) return failure;

		return total_chars_copied;
	}
	else return failure;
}