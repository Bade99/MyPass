#pragma once
#include "win_sdk.h"
#include "global.h"
#include "helpers.h"
#include "renderer.h"

//TODO(fran): new class btn_text_or_img: if the text fits then draw it, otherwise render the img, great for cool resizing that allows for the same control to take different shapes but maintain all functionality

//NOTE: this buttons can have text or an image, or both at the same time
//NOTE: it's important that the parent uses WS_CLIPCHILDREN to avoid horrible flickering
//NOTE: this button follows the standard button tradition of getting the msg to send to the parent from the hMenu param of CreateWindow/Ex
//NOTE: when clicked notifies the parent through WM_COMMAND with LOWORD(wParam)= msg number specified in hMenu param of CreateWindow/Ex ; HIWORD(wParam)=0 ; lParam= HWND of the button. Just like the standard button

namespace button {

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_theme(HWND wnd, const Theme& src) { _control_create_function__set_theme }

void set_user_data(HWND wnd, void* user_data) { _control_create_function__set_user_data }

void set_functions(HWND wnd, const Functions& functions) { _control_create_function__set_functions }

void notify_on_click(State& state) {
	if (state.functions.on_click) state.functions.on_click(state.user_data, state.wnd);
	else PostMessage(state.parent, WM_COMMAND, (WPARAM)MAKELONG(state.msg_to_send, 0), (LPARAM)state.wnd);
}

void set_selected(HWND btn, bool selected) {
	State& state = *get_state(btn);
	if (state.selected != selected) {
		state.selected = selected;
		ask_window_for_repaint(btn);
	}
}

bool image_only(State& state, DWORD style) {
	// Per the docs https://learn.microsoft.com/en-us/windows/win32/controls/bm-setimage
	// BS_ICON/BS_BITMAP indicate exclusive usage of the image if it is present
	return (state.theme.bmp || state.theme.icon) && (style & BS_ICON || style & BS_BITMAP); 
}

bool image_and_text(State& state, DWORD style) {
	// Per the docs https://learn.microsoft.com/en-us/windows/win32/controls/bm-setimage
	// BS_ICON/BS_BITMAP indicate exclusive usage of the image if it is present
	return (state.theme.bmp || state.theme.icon) && !(style & BS_ICON || style & BS_BITMAP);
}

enum class image_placement{ left, right, full};
void draw_image(State& state, HDC dc, HBRUSH br, DWORD style, rect_i32& bounds, image_placement placement) {
	auto min_dim = minimum(bounds.w, bounds.h);
	if (min_dim <= 0) return;

	auto calc_image_rect = [](rect_i32& bounds, i32 max_image_sz, image_placement placement) {
		auto padding = DPI(8);
		switch (placement) {
		case image_placement::left: return bounds.cut_left(max_image_sz + padding).cut_center(max_image_sz);
		case image_placement::right: return bounds.cut_right(max_image_sz + padding).cut_center(max_image_sz);
		case image_placement::full: return bounds.cut_center(max_image_sz);
		default: Assert(0); return bounds;
		}
	};

	if (style & BS_ICON) {
		HICON icon = state.theme.icon;
		//NOTE: we assume all icons to be squares 1:1
		auto iconnfo = MyGetIconInfo(icon);
		int max_sz = (int)((f32)min_dim * .8f);
		auto img = calc_image_rect(bounds, max_sz, placement);
		urender::draw_icon(dc, img.x, img.y, img.w, img.h, icon, 0, 0, iconnfo.w, iconnfo.h);
	}
	elif(style & BS_BITMAP) {
		int max_sz = urender::bitmap_draw_size((int)((f32)min_dim * .8f), bounds.w, bounds.h);
		auto img = calc_image_rect(bounds, max_sz, placement);
		urender::draw_mask(dc, img.x, img.y, img.w, img.h, state.theme.bmp, br);
	}
}

void draw_text(State& state, HDC dc, HBRUSH br, const rect_i32& bounds) {
	TCHAR title[max_expected_text_length]{};
	GetWindowText(state.wnd, title, ARRAYSIZE(title));
	RECT text_bounds{ bounds.left, bounds.top, bounds.left + bounds.w, bounds.top + bounds.h };
	urender::draw_text(dc, text_bounds, to_utf_str(title), state.theme.font, br,
		urender::TextAlignment::center, 0);
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case BCM_GETIDEALSIZE:
	{
		SIZE* sz = (SIZE*)lparam;//NOTE: all sizes are relative to the entire button, not just the img or text
		DWORD style = (DWORD)GetWindowLongPtr(state.wnd, GWL_STYLE);
		auto text_sz = [](State& state) {
			HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd, dc); };
			HFONT old_font = state.theme.font ? SelectFont(dc, state.theme.font) : nil;
			defer{ if (old_font) SelectFont(dc, old_font); };

			cstr title[max_expected_text_length]{};
			auto title_len = (size_t)GetWindowText(state.wnd, title, ARRAYSIZE(title));
			SIZE res = urender::measure_text(dc, state.theme.font, { title, title_len });
			res.cx += urender::measure_text(dc, state.theme.font, L"    ").cx;

			TEXTMETRIC metrics{};
			GetTextMetrics(dc, &metrics);
			res.cy = maximum(res.cy, metrics.tmHeight) + 2 * (i32)ceilf(DPI(4));
			return res;
		};
		if (sz->cx) { //calculate cy based on cx
			auto image_sz_cy = [](const SIZE* sz) { return sz->cx; }; //we assume that imgs are always square
			sz->cy = image_only(state, style) ? image_sz_cy(sz)
				: image_and_text(state, style) ? maximum(image_sz_cy(sz), text_sz(state).cy)
				: text_sz(state).cy;
		}
		else { //calculate cx and cy
			auto image_sz = [](State& state, DWORD style) {
				SIZE res{};
				if (style & BS_ICON) {
					auto iconnfo = MyGetIconInfo(state.theme.icon);
					res.cx = iconnfo.w;
					res.cy = iconnfo.h;
				}
				elif (style & BS_BITMAP) {
					BITMAP bitmap; GetObject(state.theme.bmp, sizeof(bitmap), &bitmap);
					res.cx = bitmap.bmWidth;
					res.cy = bitmap.bmHeight;
				}
				return res;
			};
			*sz = image_only(state, style) ? image_sz(state, style)
				: image_and_text(state, style) ? image_sz(state, style) + text_sz(state)
				: text_sz(state);
		}
		return TRUE;
	} break;
	case BM_GETIMAGE:
	{
		if (wparam == IMAGE_BITMAP) return (LRESULT)state.theme.bmp;
		elif (wparam == IMAGE_ICON) return (LRESULT)state.theme.icon;
		return 0;
	}
	case BM_SETIMAGE:
	{
		if (wparam == IMAGE_BITMAP) {
			HBITMAP old = state.theme.bmp;
			state.theme.bmp = (HBITMAP)lparam;
			return (LRESULT)old;
		}
		elif (wparam == IMAGE_ICON) {
			HICON old = state.theme.icon;
			state.theme.icon = (HICON)lparam;
			return (LRESULT)old;
		}
		return 0;
	} break;
	case WM_CANCELMODE:
	{
		if (state.OnMouseTracking) {
			ReleaseCapture();
			state.OnMouseTracking = false;
		}
		state.onLMouseClick = false;
		state.onKeyboardClick = false;
		state.onMouseOver = false;
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_CAPTURECHANGED:
	{
		state.OnMouseTracking = false;
		state.onLMouseClick = false;
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_LBUTTONUP:
	{
		bool clicked = state.OnMouseTracking && state.onMouseOver;
		if (state.OnMouseTracking) {
			ReleaseCapture();
			state.OnMouseTracking = false;
		}
		state.onLMouseClick = false;
		if (clicked) notify_on_click(state);

		return 0;
	} break;
	case WM_LBUTTONDOWN:
	{
		if (state.onMouseOver) {
			//Click happened inside the button, so we want to capture the mouse movement in case the user starts moving the mouse trying to move the mouse around

			SetCapture(state.wnd);//We want to keep capturing the mouse while the user is still pressing the button, even if the mouse leaves our client area
			state.OnMouseTracking = true;
			state.onLMouseClick = true;
		}
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_MOUSELEAVE:
	{
		//we asked TrackMouseEvent for this so we can update when the mouse leaves our client area, which we dont get notified about otherwise and is needed, for example when the user hovers on top the button and then hovers outside the client area
		state.onMouseOver = false;
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_MOUSEMOVE:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//client coords, relative to upper-left corner

		//Store previous state
		bool prev_onMouseOver = state.onMouseOver;

		RECT rc; GetClientRect(state.wnd, &rc);
		state.onMouseOver = test_pt_rc(mouse, rc);

		bool state_change = prev_onMouseOver != state.onMouseOver;
		if (state_change) {
			ask_window_for_repaint(state.wnd);
			TRACKMOUSEEVENT track;
			track.cbSize = sizeof(track);
			track.hwndTrack = state.wnd;
			track.dwFlags = TME_LEAVE;
			TrackMouseEvent(&track);
		}

		return 0;
	} break;
	case WM_MOUSEACTIVATE:
	{
		return handle_wm_mouseactivate();
	} break;
	case WM_SETCURSOR:
	{
		return handle_wm_setcursor(hwnd, msg, wparam, lparam, state.theme.cursor);
	} break;
	case WM_NCHITTEST:
	{
		return handle_wm_nchittest(state.wnd, lparam);
	}
	case WM_NCCREATE: {
		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		st->parent = creation_nfo->hwndParent;
		st->wnd = hwnd;
		st->msg_to_send = (UINT)(UINT_PTR)creation_nfo->hMenu;
		if(creation_nfo->lpszName) SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)creation_nfo->lpszName);
		return TRUE;
	} break;
	case WM_PAINT:
	{
		render_diagnostics::Scope diagnostic{ render_diagnostics::Event::button_paint };
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };

		RECT rc; GetClientRect(state.wnd, &rc);
		rect_i32 bounds{ .x = rc.left, .y = rc.top, .w = RECTW(rc), .h = RECTH(rc) };

		DWORD style = (DWORD)GetWindowLongPtr(state.wnd, GWL_STYLE);
		const auto& theme = state.theme;
		HBRUSH bkbr, forebr, borderbr = theme.brushes.border.normal;
		auto dimensions = theme.dimensions;
		if (!IsWindowEnabled(state.wnd)) {
			bkbr = theme.brushes.bk.disabled;
			forebr = theme.brushes.foreground.disabled;
			if (theme.brushes.border.disabled) borderbr = theme.brushes.border.disabled;
		} elif ((state.onMouseOver && state.onLMouseClick) || state.onKeyboardClick || state.selected) {
			bkbr = theme.brushes.bk.clicked;
			forebr = theme.brushes.foreground.clicked;
			if (theme.brushes.border.clicked) borderbr = theme.brushes.border.clicked;
		} elif (state.onMouseOver || state.OnMouseTracking || (GetFocus()==state.wnd)) {
			bkbr = theme.brushes.bk.mouseover;
			forebr = theme.brushes.foreground.mouseover;
			if (theme.brushes.border.mouseover) borderbr = theme.brushes.border.mouseover;
		} else {
			bkbr = theme.brushes.bk.normal;
			forebr = theme.brushes.foreground.normal;
		}
		if (GetFocus() == state.wnd) { 
			borderbr = GetStockBrush(WHITE_BRUSH);
			dimensions.border_thickness = DPI(3);
		}
		SetBkColor(dc, ColorFromBrush(bkbr));
		HBRUSH oldbr = SelectBrush(dc, bkbr); defer{ SelectBrush(dc, oldbr); };

		//TODO: clip text rendering to the fill area of this (exclude border)
		urender::draw_background(dc, rc, bkbr, borderbr, dimensions);

		if (image_only(state, style)) 
			draw_image(state, dc, forebr, style, bounds, image_placement::full);
		elif(image_and_text(state, style)) {
			draw_image(state, dc, forebr, style, bounds, image_placement::left);
			draw_text(state, dc, forebr, bounds);
		}
		else 
			draw_text(state, dc, forebr, bounds);
		return 0;
	} break;
	case WM_NCDESTROY:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		set_window_state(hwnd, nil);
		free(&state);
		return res;
	} break;
	case WM_SETFONT:
	{
		state.theme.font = (HFONT)wparam;
		if ((BOOL)LOWORD(lparam) == TRUE) ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_GETFONT:
	{
		return (LRESULT)state.theme.font;
	} break;
	case WM_SETFOCUS: //Button has WS_TABSTOP style and we got keyboard focus thanks to that
	{
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		state.onKeyboardClick = false;
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_KEYDOWN:
	{
		if (wparam == VK_RETURN && !state.onKeyboardClick) {
			state.onKeyboardClick = true;
			RedrawWindow(state.wnd, nil, nil, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		return 0;
	} break;
	case WM_KEYUP:
	{
		if (wparam == VK_RETURN && state.onKeyboardClick) {
			state.onKeyboardClick = false;
			ask_window_for_repaint(state.wnd);
		}
		return 0;
	} break;
	case WM_CHAR:
	{
		//Here we check for enter and tab
		TCHAR c = (TCHAR)wparam;
		switch (c) {
		case VK_TAB://Tab
		{
			//handle_tabstop_transition(state.wnd);
		}break;
		case VK_RETURN://Received when the user presses the "enter" key //Carriage Return aka \r
		{
			notify_on_click(state);
		}break;
		}
		return 0;
	} break;
	case WM_IME_SETCONTEXT: //We dont want IME for a button
	case WM_NCCALCSIZE:
	{
		return 0;
	} break;
	#ifdef _DEBUG_HWND_MESSAGES
	case WM_CREATE:
	case WM_DESTROY:
	case WM_MOVE:
	case WM_SIZE: 
	case WM_WINDOWPOSCHANGED:
	case WM_WINDOWPOSCHANGING:
	case WM_SHOWWINDOW:
	case WM_SETTEXT:
	case WM_GETTEXT:
	case WM_GETICON:
	case WM_STYLECHANGING:
	case WM_STYLECHANGED:
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
