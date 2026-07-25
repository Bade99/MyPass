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

	auto calc_image_rect = [](rect_i32& bounds, i32 max_image_sz, image_placement placement) {
		auto padding = DPI(8);
		switch (placement) {
		case image_placement::left: return bounds.cut_left(max_image_sz + padding).cut_center(max_image_sz);
		case image_placement::right: return bounds.cut_right(max_image_sz + padding).cut_center(max_image_sz);
		case image_placement::full: return bounds.cut_center(max_image_sz);
		}
	};

	if (style & BS_ICON) {
		HICON icon = state.theme.icon;
		//NOTE: we assume all icons to be squares 1:1
		auto iconnfo = MyGetIconInfo(icon);
		int max_sz = (int)((float)min_dim * .8f);
		auto img = calc_image_rect(bounds, max_sz, placement);
		urender::draw_icon(dc, img.x, img.y, img.w, img.h, icon, 0, 0, iconnfo.w, iconnfo.h);
	}
	elif(style & BS_BITMAP) {
		constexpr auto min_sz = 12; //anything below 12px is commonly just a hodgepodge of random pixels
		BITMAP bitmap; GetObject(state.theme.bmp, sizeof(bitmap), &bitmap);
		int max_sz = (int)((float)min_dim * .8f);
		if (bitmap.bmBitsPixel == 1) {
			max_sz = roundNdown(bitmap.bmWidth, max_sz); //HACK: instead use png + gdi+ + color matrices
			if (!max_sz) {
				if ((bitmap.bmWidth % 2) == 0) max_sz = bitmap.bmWidth / 2;
				else max_sz = bitmap.bmWidth; //More HACKs
			}
			if (max_sz > bitmap.bmWidth) max_sz = bitmap.bmWidth;//TODO(fran): HACK nº 1000, for this specific program (MyPass) some if we scale some icons bigger than their original size they look terrible (specially the close button), therefore we dont allow it. Solution: stop using 1 bit images and use 8 bit grayscale
		} elif(bitmap.bmBitsPixel == 8) {
			if (max_sz < bitmap.bmWidth) {
				auto test_w = bitmap.bmWidth;
				while (test_w && test_w > max_sz) test_w /= 2;
				max_sz = test_w;
			}
		}
		max_sz = maximum(max_sz, min_sz);
		auto img = calc_image_rect(bounds, max_sz, placement);

		if (bitmap.bmBitsPixel == 1)
			urender::draw_mask(dc, img.x, img.y, img.w, img.h, state.theme.bmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, br);
		elif(bitmap.bmBitsPixel == 8)
			urender::draw_menu_mask8(dc, img.x, img.y, img.w, img.h, state.theme.bmp, br);
	}
}

void draw_text(State& state, HDC dc, HBRUSH br, const rect_i32& bounds) {
	HFONT font = state.theme.font;
	if (font) SelectFont(dc, font);
	SetTextColor(dc, ColorFromBrush(br));
	auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };
	TCHAR title[max_expected_text_length];
	int len = (int)SendMessage(state.wnd, WM_GETTEXT, ARRAYSIZE(title), (LPARAM)title);

	// Calculate vertical position for the item string so that it will be vertically centered
	SIZE txt_sz; GetTextExtentPoint32(dc, title, len, &txt_sz);
	int yPos = (bounds.bottom() + bounds.top - txt_sz.cy) / 2;

	SetTextAlign(dc, TA_CENTER); //TODO(fran): probably we want left aligned when an image is also being rendered?
	int xPos = (bounds.right() - bounds.left) / 2;
	TextOut(dc, xPos, yPos, title, len);
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case BCM_GETIDEALSIZE:
	{
		SIZE* sz = (SIZE*)lparam;//NOTE: all sizes are relative to the entire button, not just the img or text
		DWORD style = (DWORD)GetWindowLongPtr(state.wnd, GWL_STYLE);
		constexpr auto text_scale_factor = 1.2f;
		if (sz->cx) { //calculate cy based on cx
			auto image_sz_cy = [](const SIZE* sz) { return sz->cx; }; //we assume that imgs are always square
			auto text_sz_cy = [](State& state) {
				HFONT font = state.theme.font;
				HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
				if (font) SelectFont(dc, font);
				TEXTMETRIC tm; GetTextMetrics(dc, &tm);
				return (i32)((f32)tm.tmHeight * text_scale_factor);
			};
			sz->cy = image_only(state, style) ? image_sz_cy(sz)
				: image_and_text(state, style) ? maximum(image_sz_cy(sz), text_sz_cy(state))
				: text_sz_cy(state);
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
			auto text_sz = [](State& state) {
				SIZE res{};
				HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
				if (state.theme.font) SelectFont(dc, state.theme.font);
				TEXTMETRIC tm; GetTextMetrics(dc, &tm);

				cstr t[max_expected_text_length];
				int len = (int)SendMessage(state.wnd, WM_GETTEXT, ARRAYSIZE(t), (LPARAM)t);

				GetTextExtentPoint32(dc, t, len, &res);
				res.cx = (i32)((f32)res.cx * text_scale_factor);
				res.cy = (i32)((f32)res.cy * text_scale_factor);
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
		state.onMouseOver = false;
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_CAPTURECHANGED:
	{
		//We lost mouse capture
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_LBUTTONUP:
	{
		if (state.OnMouseTracking) {
			if (state.onMouseOver) notify_on_click(state);
			ReleaseCapture();
			state.OnMouseTracking = false;
		}
		state.onLMouseClick = false;

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
		//IMPORTANT TODO(fran): fix rendering bugs, sometimes part of the borders dont get drawn, I think I found the bug, border br doesnt adapt like bk, we have push, mouseover, etc, the border is just one so it's probably creating inconsistency problems

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
			borderbr = theme.brushes.border.disabled;
		} elif ((state.onMouseOver && state.onLMouseClick) || state.selected) {
			bkbr = theme.brushes.bk.clicked;
			forebr = theme.brushes.foreground.clicked;
		} elif (state.onMouseOver || state.OnMouseTracking || (GetFocus()==state.wnd)) {
			bkbr = theme.brushes.bk.mouseover;
			forebr = theme.brushes.foreground.mouseover;
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
	case WM_DESTROY:
	{
		free(&state);
	}break;
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
		ask_window_for_repaint(state.wnd);
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
			//SendMessage(state.wnd, WM_LBUTTONDOWN, ); //TODO(fran): send this to cause repaint and thus make it look as if the user clicked the button
			notify_on_click(state);
		}break;
		}
		return 0;
	} break;
	case WM_IME_SETCONTEXT: //We dont want IME for a button
	case WM_KEYUP:
	case WM_KEYDOWN: //Nothing for us here
	case WM_NCCALCSIZE:
	{
		return 0;
	} break;
	#ifdef _DEBUG_HWND_MESSAGES
	case WM_CREATE:
	case WM_NCDESTROY:
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