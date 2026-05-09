#pragma once
#include "win_sdk.h"
#include "global.h"
#include "helpers.h"
#include "renderer.h"

//TODO(fran): new class btn_text_or_img: if the text fits then draw it, otherwise render the img, great for cool resizing that allows for the same control to take different shapes but maintain all functionality

//NOTE: this buttons can have text or an img, but not both at the same time
//NOTE: it's important that the parent uses WS_CLIPCHILDREN to avoid horrible flickering
//NOTE: this button follows the standard button tradition of getting the msg to send to the parent from the hMenu param of CreateWindow/Ex
//NOTE: when clicked notifies the parent through WM_COMMAND with LOWORD(wParam)= msg number specified in hMenu param of CreateWindow/Ex ; HIWORD(wParam)=0 ; lParam= HWND of the button. Just like the standard button

namespace button {

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_theme(HWND wnd, const Theme& src) { _control_create_function__set_theme }

void set_user_data(HWND wnd, void* user_data) { _control_create_function__set_user_data }

void set_functions(HWND wnd, const Functions& functions) { _control_create_function__set_functions }

void notify_on_click(State& state) {
	if (state.functions.on_click) state.functions.on_click(state.user_data);
	else PostMessage(state.parent, WM_COMMAND, (WPARAM)MAKELONG(state.msg_to_send, 0), (LPARAM)state.wnd);
}

void set_selected(HWND btn, bool selected) {
	State& state = *get_state(btn);
	if (state.selected != selected) {
		state.selected = selected;
		ask_window_for_repaint(btn);
	}
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case BCM_GETIDEALSIZE:
	{
		SIZE* sz = (SIZE*)lparam;//NOTE: all sizes are relative to the entire button, not just the img or text
		DWORD style = (DWORD)GetWindowLongPtr(state.wnd, GWL_STYLE);
		if (sz->cx) { //calculate cy based on cx
			if (style & BS_ICON || style & BS_BITMAP) {
				sz->cy = sz->cx; //we always assume that imgs are square
			}
			else { //we got text
				HFONT font = state.theme.font;
				HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
				if (font) (HFONT)SelectObject(dc, (HGDIOBJ)font);
				TEXTMETRIC tm; GetTextMetrics(dc, &tm);
				sz->cy = (int)((float)tm.tmHeight * 1.2f);
			}
		}
		else { //calculate cx and cy
			if (style & BS_ICON) {
				auto iconnfo = MyGetIconInfo(state.theme.icon);
				sz->cx = iconnfo.w;
				sz->cy = iconnfo.h;
			}
			else if (style & BS_BITMAP) {
				BITMAP bitmap; GetObject(state.theme.bmp, sizeof(bitmap), &bitmap);
				sz->cx = bitmap.bmWidth;
				sz->cy = bitmap.bmHeight;
			}
			else { //we got text
				HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
				if (state.theme.font) (HFONT)SelectObject(dc, (HGDIOBJ)state.theme.font);
				TEXTMETRIC tm; GetTextMetrics(dc, &tm);

				TCHAR Text[max_expected_text_length];
				int len = (int)SendMessage(state.wnd, WM_GETTEXT, ARRAYSIZE(Text), (LPARAM)Text);
				
				GetTextExtentPoint32(dc, Text, len, sz);
				sz->cx = (int)((float)sz->cx * 1.2f);
				sz->cy = (int)((float)sz->cy * 1.2f);
			}
		}
		return TRUE;
	} break;
	case BM_GETIMAGE:
	{
		if (wparam == IMAGE_BITMAP) return (LRESULT)state.theme.bmp;
		else if (wparam == IMAGE_ICON) return (LRESULT)state.theme.icon;
		return 0;
	}
	case BM_SETIMAGE://TODO: in this call you decide whether to show both img and txt, only txt or only img, see doc
	{
		if (wparam == IMAGE_BITMAP) {
			HBITMAP old = state.theme.bmp;
			state.theme.bmp = (HBITMAP)lparam;
			return (LRESULT)old;
		}
		else if (wparam == IMAGE_ICON) {
			HICON old = state.theme.icon;
			state.theme.icon = (HICON)lparam;
			return (LRESULT)old;
		}
		return 0;
	} break;
	case WM_CANCELMODE:
	{
		//We got canceled/deactivated, but doc says we should cancel everything mouse capture related, so stop tracking
		if (state.OnMouseTracking) {
			ReleaseCapture();//stop capturing the mouse
			state.OnMouseTracking = false;
		}
		state.onLMouseClick = false;
		state.onMouseOver = false;
		InvalidateRect(state.wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_NCDESTROY:
	{
		//we are getting killed
		//doc says: This message frees any memory internally allocated for the window.
		return DefWindowProc(hwnd, msg, wparam, lparam);//Probably does nothing
	} break;
	case WM_GETTEXT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
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
			ReleaseCapture();//stop capturing the mouse
			state.OnMouseTracking = false;
		}
		state.onLMouseClick = false;

		return 0;
	} break;
	case WM_LBUTTONDOWN:
	{
		//Left click is down
		if (state.onMouseOver) {
			//Click happened inside the button, so we want to capture the mouse movement in case the user starts moving the mouse trying to scroll

			//TODO(fran): maybe first check that we arent already tracking (!state.OnMouseTrackingSb)
			SetCapture(state.wnd);//We want to keep capturing the mouse while the user is still pressing some button, even if the mouse leaves our client area
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
		//After WM_NCHITTEST and WM_SETCURSOR we finally get that the mouse has moved
		//Sent to the window where the cursor is, unless someone else is explicitly capturing it, in which case this gets sent to them
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//client coords, relative to upper-left corner

		//Store previous state
		bool prev_onMouseOver = state.onMouseOver;

		RECT rc; GetClientRect(state.wnd, &rc);
		if (test_pt_rc(mouse, rc)) {//Mouse is inside the button
			state.onMouseOver = true;
		}
		else {//Mouse is outside the button
			state.onMouseOver = false;
		}

		bool state_change = prev_onMouseOver != state.onMouseOver;
		if (state_change) {
			InvalidateRect(state.wnd, NULL, TRUE);
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
		//Sent to us when we're still an inactive window and we get a mouse press
		//TODO(fran): we could also ask our parent (wparam) what it wants to do with us
		return MA_ACTIVATE; //Activate our window and post the mouse msg
	} break;
	case WM_SETCURSOR:
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
		return handle_wm_setcursor(hwnd, msg, wparam, lparam, state.theme.cursor);
	} break;
	case WM_NCHITTEST:
	{
		return handle_wm_nchittest(state.wnd, lparam);
	}
	case WM_ERASEBKGND:
	{
		//You receive this msg if you didnt specify hbrBackground  when you registered the class, now it's up to you to draw the background
		HDC dc = (HDC)wparam;
		//TODO(fran): look at https://docs.microsoft.com/en-us/windows/win32/gdi/drawing-a-custom-window-background and SetMapModek, allows for transforms

		return 0; //If you return 0 then on WM_PAINT fErase will be true, aka paint the background there
	} break;
	case WM_NCPAINT:
	{
		//Paint non client area, we shouldnt have any
		HDC hdc = GetDCEx(hwnd, (HRGN)wparam, DCX_WINDOW | DCX_USESTYLE);
		ReleaseDC(hwnd, hdc);
		return 0; //we process this message, for now
	} break;
	case WM_WINDOWPOSCHANGED:
	{
		WINDOWPOS* p = (WINDOWPOS*)lparam; //new window pos, size, etc
		return DefWindowProc(hwnd, msg, wparam, lparam); //TODO(fran): if we handle this msg ourselves instead of calling DefWindowProc we wont need to handle WM_SIZE and WM_MOVE since they wont be sent, also it says it's more efficient https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged
	} break;
	case WM_WINDOWPOSCHANGING:
	{
		//Someone calls SetWindowPos with the new values, here you can apply modifications over those
		WINDOWPOS* p = (WINDOWPOS*)lparam;
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SHOWWINDOW: //On startup I received this cause of WS_VISIBLE flag
	{
		//Sent when window is about to be hidden or shown, doesnt let it clear if we are in charge of that or it's going to happen no matter what we do
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOVE: //Sent on startup after WM_SIZE, although possibly sent by DefWindowProc after I let it process WM_SIZE, not sure
	{
		//This msg is received _after_ the window was moved
		//Here you can obtain x and y of your window's client area
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_SIZE: {
		//NOTE: neat, here you resize your render target, if I had one or cared to resize windows' https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-size
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCCREATE: { //1st msg received
		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		st->parent = creation_nfo->hwndParent;
		st->wnd = hwnd;
		st->msg_to_send = (UINT)(UINT_PTR)creation_nfo->hMenu;
		if(creation_nfo->lpszName)SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)creation_nfo->lpszName);
		return TRUE; //continue creation, this msg seems kind of more of a user setup place, strange considering there's also WM_CREATE
	} break;
	case WM_NCCALCSIZE: { //2nd msg received https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize
		if (wparam) {
			//Indicate part of current client area that is valid
			NCCALCSIZE_PARAMS* calcsz = (NCCALCSIZE_PARAMS*)lparam;
			return 0; //causes the client area to resize to the size of the window, including the window frame
		}
		else {
			RECT* client_rc = (RECT*)lparam;
			//TODO(fran): make client_rc cover the full window area
			return 0;
		}
	} break;
	case WM_SETTEXT:
	{
		//This function is insane, it actually does painting on it's own without telling nobody, so we need a way to kill that
		//I think there are a couple approaches that work, I took this one which works since windows 95 (from what the article says)
		//Many thanks for yet another hack http://www.catch22.net/tuts/win32/custom-titlebar
		LONG_PTR  dwStyle = GetWindowLongPtr(state.wnd, GWL_STYLE);
		// turn off WS_VISIBLE
		SetWindowLongPtr(state.wnd, GWL_STYLE, dwStyle & ~WS_VISIBLE);

		// perform the default action, minus painting
		LRESULT ret = DefWindowProc(state.wnd, msg, wparam, lparam);

		// turn on WS_VISIBLE
		SetWindowLongPtr(state.wnd, GWL_STYLE, dwStyle);

		// perform custom painting, aka dont and do what should be done, repaint, it's really not that expensive for our case, we barely call WM_SETTEXT, and it can be optimized out later
		RedrawWindow(state.wnd, NULL, NULL, RDW_INVALIDATE);

		return ret;
	} break;
	case WM_PAINT:
	{
		//IMPORTANT TODO(fran): fix rendering bugs, sometimes part of the borders dont get drawn, I think I found the bug, border br doesnt adapt like bk, we have push, mouseover, etc, the border is just one so it's probably creating inconsistency problems

		PAINTSTRUCT ps; //TODO(fran): we arent using the rectangle from the ps, I think we should for performance
		//TODO(fran): Check that we are going to paint something new
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };

		RECT rc; GetClientRect(state.wnd, &rc);
		auto w = RECTW(rc), h = RECTH(rc);
		auto min_dim = minimum(w, h);

		DWORD style = (DWORD)GetWindowLongPtr(state.wnd, GWL_STYLE);
		const auto& theme = state.theme;
		HBRUSH bkbr, forebr, borderbr = theme.brushes.border.normal;
		if ((state.onMouseOver && state.onLMouseClick) || state.selected) {
			bkbr = theme.brushes.bk.clicked;
			forebr = theme.brushes.foreground.clicked;
		}
		else if (state.onMouseOver || state.OnMouseTracking || (GetFocus()==state.wnd)) {
			bkbr = theme.brushes.bk.mouseover;
			forebr = theme.brushes.foreground.mouseover;
		}
		else {
			bkbr = theme.brushes.bk.normal;
			forebr = theme.brushes.foreground.normal;
		}
		SetBkColor(dc, ColorFromBrush(bkbr));
		HBRUSH oldbr = SelectBrush(dc, bkbr); defer{ SelectBrush(dc, oldbr); };

		//TODO: clip text rendering to this
		urender::draw_background(dc, rc, bkbr, borderbr, theme.dimensions);

		if (style & BS_ICON) {//Here will go buttons that only have an icon

			HICON icon = state.theme.icon;
			//NOTE: we assume all icons to be squares 1:1
			auto iconnfo = MyGetIconInfo(icon);
			int max_sz = (int)((float)min_dim * .8f);
			int icon_height = max_sz;
			int icon_width = icon_height;
			int icon_align_height = (h - icon_height) / 2;
			int icon_align_width = (w - icon_width) / 2;
			urender::draw_icon(dc, icon_align_height, icon_align_width, icon_width, icon_height, icon, 0, 0, iconnfo.w, iconnfo.h);
		}
		else if (style & BS_BITMAP) {
			constexpr auto min_sz = 12; //anything below 12px is commonly just a hodgepodge of random pixels
			BITMAP bitmap; GetObject(state.theme.bmp, sizeof(bitmap), &bitmap);
			int max_sz = (int)((float)min_dim * .8f);
			if (bitmap.bmBitsPixel == 1) {
				//TODO(fran):unify rect calculation with icon
				max_sz = roundNdown(bitmap.bmWidth, max_sz); //HACK: instead use png + gdi+ + color matrices
				if (!max_sz) {
					if ((bitmap.bmWidth % 2) == 0) max_sz = bitmap.bmWidth / 2;
					else max_sz = bitmap.bmWidth; //More HACKs
				}
				if (max_sz > bitmap.bmWidth) max_sz = bitmap.bmWidth;//TODO(fran): HACK nº 1000, for this specific program (MyPass) some if we scale some icons bigger than their original size they look terrible (specially the close button), therefore we dont allow it. Solution: stop using 1 bit images for icons and use 8 bit grayscale
			} elif(bitmap.bmBitsPixel == 8) {
				if (max_sz < bitmap.bmWidth) {
					auto test_w = bitmap.bmWidth;
					while (test_w && test_w > max_sz) test_w /= 2;
					max_sz = test_w;
				}
			}
			max_sz = maximum(max_sz, min_sz);
			int bmp_height = max_sz;
			int bmp_width = bmp_height;
			int bmp_align_height = (h - bmp_height) / 2;
			int bmp_align_width = (w - bmp_width) / 2;

			if (bitmap.bmBitsPixel == 1)
				urender::draw_mask(dc, bmp_align_width, bmp_align_height, bmp_width, bmp_height, state.theme.bmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, forebr);
			elif(bitmap.bmBitsPixel == 8)
				urender::draw_menu_mask8(dc, bmp_align_width, bmp_align_height, bmp_width, bmp_height, state.theme.bmp, forebr);
		}
		else { //Here will go buttons that only have text
			HFONT font = state.theme.font;
			if (font) {//if font == NULL then it is using system font(default I assume)
				(HFONT)SelectObject(dc, (HGDIOBJ)font);
			}
			SetTextColor(dc, ColorFromBrush(forebr));
			auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };
			TCHAR Text[max_expected_text_length];
			int len = (int)SendMessage(state.wnd, WM_GETTEXT, ARRAYSIZE(Text), (LPARAM)Text);

			// Calculate vertical position for the item string so that it will be vertically centered
			SIZE txt_sz; GetTextExtentPoint32(dc, Text, len, &txt_sz);
			int yPos = (rc.bottom + rc.top - txt_sz.cy) / 2;

			SetTextAlign(dc, TA_CENTER);
			int xPos = (rc.right - rc.left) / 2;
			TextOut(dc, xPos, yPos, Text, len);
		}
		return 0;
	} break;
	case WM_DESTROY:
	{
		free(&state);
	}break;
	case WM_STYLECHANGING:
	{
		// SetWindowLong... related, we can check the proposed new styles and change them
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_STYLECHANGED:
	{
		// Notifies that the style was changed, you cant do nothing here
		return DefWindowProc(hwnd, msg, wparam, lparam);
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
	case WM_GETICON:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CREATE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_IME_SETCONTEXT: //When we get keyboard focus for the first time this gets sent
	{
		return 0; //We dont want IME for a button
	} break;
	case WM_SETFOCUS: //Button has WS_TABSTOP style and we got keyboard focus thanks to that
	{
		//TODO(fran): repaint and show as if the user is hovering over the button
		InvalidateRect(state.wnd, NULL, TRUE);
		return 0;
	}
	case WM_KEYUP:
	{
		return 0;
	} break;
	case WM_KEYDOWN:
	{
		//Nothing for us here
		return 0;
	} break;
	case WM_CHAR:
	{
		//Here we check for enter and tab
		TCHAR c = (TCHAR)wparam;
		switch (c) {
		case VK_TAB://Tab
		{
			SetFocus(GetNextDlgTabItem(GetParent(state.wnd), state.wnd, FALSE));
		}break;
		case VK_RETURN://Received when the user presses the "enter" key //Carriage Return aka \r
		{
			//SendMessage(state.wnd, WM_LBUTTONDOWN, ); //TODO(fran): send this to cause repaint and thus make it look as if the user clicked the button
			notify_on_click(state);
		}break;
		}
		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
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