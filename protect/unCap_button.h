#pragma once
#include <Windows.h>
#include <windowsx.h>
#include "unCap_Global.h"
#include "unCap_Helpers.h"
#include "unCap_Renderer.h"
#include "windows_undoc.h"

//NOTE: this buttons can have text or an img, but not both at the same time
//NOTE: it's important that the parent uses WS_CLIPCHILDREN to avoid horrible flickering
//NOTE: this button follows the standard button tradition of getting the msg to send to the parent from the hMenu param of CreateWindow/Ex
//NOTE: when clicked notifies the parent through WM_COMMAND with LOWORD(wParam)= msg number specified in hMenu param of CreateWindow/Ex ; HIWORD(wParam)=0 ; lParam= HWND of the button. Just like the standard button

constexpr TCHAR unCap_wndclass_button[] = TEXT("unCap_wndclass_button");

static LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

struct ButtonProcState { //NOTE: must be initialized to zero
	HWND wnd;
	HWND parent;

	bool onMouseOver;
	bool onLMouseClick;//The left click is being pressed on the background area
	bool OnMouseTracking; //Left click was pressed in our bar and now is still being held, the user will probably be moving the mouse around, so we want to track it to move the scrollbar

	UINT msg_to_send;

	HFONT font;

	HBRUSH br_border, br_bk, br_fore, br_bkpush, br_bkmouseover;

	HICON icon;
	HBITMAP bmp;
};

void init_wndclass_unCap_button(HINSTANCE instance) {
	WNDCLASSEXW button;

	button.cbSize = sizeof(button);
	button.style = CS_HREDRAW | CS_VREDRAW;
	button.lpfnWndProc = ButtonProc;
	button.cbClsExtra = 0;
	button.cbWndExtra = sizeof(ButtonProcState*);
	button.hInstance = instance;
	button.hIcon = NULL;
	button.hCursor = LoadCursor(nullptr, IDC_ARROW);
	button.hbrBackground = NULL;
	button.lpszMenuName = NULL;
	button.lpszClassName = unCap_wndclass_button;
	button.hIconSm = NULL;

	ATOM class_atom = RegisterClassExW(&button);
	Assert(class_atom);
}

ButtonProcState* UNCAPBTN_get_state(HWND hwnd) {
	ButtonProcState* state = (ButtonProcState*)GetWindowLongPtr(hwnd, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

//NOTE: any NULL HBRUSH remains unchanged
void UNCAPBTN_set_brushes(HWND uncap_btn, BOOL repaint, HBRUSH border_br, HBRUSH bk_br, HBRUSH fore_br, HBRUSH bkpush_br, HBRUSH bkmouseover_br) {
	ButtonProcState* state = UNCAPBTN_get_state(uncap_btn);
	if (border_br)state->br_border = border_br;
	if (bk_br)state->br_bk = bk_br;
	if (fore_br)state->br_fore = fore_br;
	if (bkpush_br)state->br_bkpush = bkpush_br;
	if (bkmouseover_br)state->br_bkmouseover = bkmouseover_br;
	if (repaint)InvalidateRect(state->wnd, NULL, TRUE);
}

static LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	ButtonProcState* state = UNCAPBTN_get_state(hwnd);
	//Assert(state); //NOTE: cannot check thanks to the grandeur of windows' msgs before WM_NCCREATE
	switch (msg) {
	case BM_GETIMAGE:
	{
		if (wparam == IMAGE_BITMAP) return (LRESULT)state->bmp;
		else if (wparam == IMAGE_ICON) return (LRESULT)state->icon;
		return 0;
	}
	case BM_SETIMAGE://TODO: in this call you decide whether to show both img and txt, only txt or only img, see doc
	{
		if (wparam == IMAGE_BITMAP) {
			HBITMAP old = state->bmp;
			state->bmp = (HBITMAP)lparam;
			return (LRESULT)old;
		}
		else if (wparam == IMAGE_ICON) {
			HICON old = state->icon;
			state->icon = (HICON)lparam;
			return (LRESULT)old;
		}
		return 0;
	} break;
	case WM_CANCELMODE:
	{
		printf("\n\n\nWM_CANCELMODE\n\n\n");
		//We got canceled by the system, not really sure what it means but doc says we should cancel everything mouse capture related, so stop tracking
		if (state->OnMouseTracking) {
			ReleaseCapture();//stop capturing the mouse
			state->OnMouseTracking = false;
		}
		state->onLMouseClick = false;
		state->onMouseOver = false;
		InvalidateRect(state->wnd, NULL, TRUE);
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
		InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_LBUTTONUP:
	{
		if (state->OnMouseTracking) {
			if (state->onMouseOver) PostMessage(state->parent, WM_COMMAND, (WPARAM)MAKELONG(state->msg_to_send, 0), (LPARAM)state->wnd);//notify parent
			ReleaseCapture();//stop capturing the mouse
			state->OnMouseTracking = false;
		}
		state->onLMouseClick = false;

		return 0;
	} break;
	case WM_LBUTTONDOWN:
	{
		//Left click is down
		if (state->onMouseOver) {
			//Click happened inside the button, so we want to capture the mouse movement in case the user starts moving the mouse trying to scroll

			//TODO(fran): maybe first check that we arent already tracking (!state->OnMouseTrackingSb)
			SetCapture(state->wnd);//We want to keep capturing the mouse while the user is still pressing some button, even if the mouse leaves our client area
			state->OnMouseTracking = true;
			state->onLMouseClick = true;
		}
		InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_MOUSELEAVE:
	{
		//we asked TrackMouseEvent for this so we can update when the mouse leaves our client area, which we dont get notified about otherwise and is needed, for example when the user hovers on top the button and then hovers outside the client area
		state->onMouseOver = false;
		InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_MOUSEMOVE:
	{
		//After WM_NCHITTEST and WM_SETCURSOR we finally get that the mouse has moved
		//Sent to the window where the cursor is, unless someone else is explicitly capturing it, in which case this gets sent to them
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//client coords, relative to upper-left corner

		//Store previous state
		bool prev_onMouseOver = state->onMouseOver;

		RECT rc; GetClientRect(state->wnd, &rc);
		if (test_pt_rc(mouse, rc)) {//Mouse is inside the button
			state->onMouseOver = true;
		}
		else {//Mouse is outside the button
			state->onMouseOver = false;
		}

		bool state_change = prev_onMouseOver != state->onMouseOver;
		if (state_change) {
			InvalidateRect(state->wnd, NULL, TRUE);
			TRACKMOUSEEVENT track;
			track.cbSize = sizeof(track);
			track.hwndTrack = state->wnd;
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
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCHITTEST:
	{
		//Received when the mouse goes over the window, on mouse press or release, and on WindowFromPoint

		// Get the point coordinates for the hit test.
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Screen coords, relative to upper-left corner

		// Get the window rectangle.
		RECT rw; GetWindowRect(state->wnd, &rw);

		LRESULT hittest = HTNOWHERE;

		// Determine if the point is inside the window
		if (test_pt_rc(mouse, rw))hittest = HTCLIENT;

		return hittest;
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
		ButtonProcState* st = (ButtonProcState*)calloc(1, sizeof(ButtonProcState));
		Assert(st);
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)st);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		st->parent = creation_nfo->hwndParent;
		st->wnd = hwnd;
		st->msg_to_send = (UINT)(UINT_PTR)creation_nfo->hMenu;
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
		LONG_PTR  dwStyle = GetWindowLongPtr(state->wnd, GWL_STYLE);
		// turn off WS_VISIBLE
		SetWindowLongPtr(state->wnd, GWL_STYLE, dwStyle & ~WS_VISIBLE);

		// perform the default action, minus painting
		LRESULT ret = DefWindowProc(state->wnd, msg, wparam, lparam);

		// turn on WS_VISIBLE
		SetWindowLongPtr(state->wnd, GWL_STYLE, dwStyle);

		// perform custom painting, aka dont and do what should be done, repaint, it's really not that expensive for our case, we barely call WM_SETTEXT, and it can be optimized out later
		RedrawWindow(state->wnd, NULL, NULL, RDW_INVALIDATE);

		return ret;
	} break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps; //TODO(fran): we arent using the rectangle from the ps, I think we should for performance
		HDC dc = BeginPaint(state->wnd, &ps);

		RECT rc; GetClientRect(state->wnd, &rc);

		//TODO(fran): Check that we are going to paint something new
		HBRUSH oldbr, bkbr;
		if (state->onMouseOver && state->onLMouseClick) {
			bkbr = state->br_bkpush;
		}
		else if (state->onMouseOver || state->OnMouseTracking || (GetFocus()==state->wnd)) {
			bkbr = state->br_bkmouseover;
		}
		else {
			bkbr = state->br_bk;
		}
		SetBkColor(dc, ColorFromBrush(bkbr));
		oldbr = SelectBrush(dc, bkbr);

		//TODO(clipping)
		int borderSize = 1;
		HBRUSH borderbr = state->br_border;
		HPEN pen = CreatePen(PS_SOLID, borderSize, ColorFromBrush(borderbr)); //border

		HPEN oldpen = (HPEN)SelectObject(dc, pen);

		//Border
		Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom); //uses pen for border and brush for bk

		SelectObject(dc, oldpen);
		DeleteObject(pen);

		DWORD style = (DWORD)GetWindowLongPtr(state->wnd, GWL_STYLE);
		if (style & BS_ICON) {//Here will go buttons that only have an icon

			HICON icon = state->icon;
			//NOTE: we assume all icons to be squares 1:1
			MYICON_INFO iconnfo = MyGetIconInfo(icon);
			int max_sz = (int)((float)min(RECTWIDTH(rc), RECTHEIGHT(rc)) * .8f);
			int icon_height = max_sz;
			int icon_width = icon_height;
			int icon_align_height = (RECTHEIGHT(rc) - icon_height) / 2;
			int icon_align_width = (RECTWIDTH(rc) - icon_width) / 2;
			urender::draw_icon(dc, icon_align_height, icon_align_width, icon_width, icon_height, icon, 0, 0, iconnfo.nWidth, iconnfo.nHeight);
		}
		else if (style & BS_BITMAP) {
			BITMAP bitmap; GetObject(state->bmp, sizeof(bitmap), &bitmap);
			if (bitmap.bmBitsPixel == 1) {
				//TODO(fran):unify rect calculation with icon
				int max_sz = roundNdown(bitmap.bmWidth, (int)((float)min(RECTWIDTH(rc), RECTHEIGHT(rc)) * .8f)); //HACK: instead use png + gdi+ + color matrices
				if (!max_sz)max_sz = bitmap.bmWidth; //More HACKs

				int bmp_height = max_sz;
				int bmp_width = bmp_height;
				int bmp_align_height = (RECTHEIGHT(rc) - bmp_height) / 2;
				int bmp_align_width = (RECTWIDTH(rc) - bmp_width) / 2;
				urender::draw_mask(dc, bmp_align_width, bmp_align_height, bmp_width, bmp_height, state->bmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, state->br_fore);
			}
		}
		else { //Here will go buttons that only have text
			HFONT font = state->font;
			if (font) {//if font == NULL then it is using system font(default I assume)
				(HFONT)SelectObject(dc, (HGDIOBJ)font);
			}
			SetTextColor(dc, ColorFromBrush(state->br_fore));
			WCHAR Text[40];
			int len = (int)SendMessage(state->wnd, WM_GETTEXT, ARRAYSIZE(Text), (LPARAM)Text);

			TEXTMETRIC tm;
			GetTextMetrics(dc, &tm);
			// Calculate vertical position for the item string so that it will be vertically centered
			int yPos = (rc.bottom + rc.top - tm.tmHeight) / 2;

			SetTextAlign(dc, TA_CENTER);
			int xPos = (rc.right - rc.left) / 2;
			TextOut(dc, xPos, yPos, Text, len);
		}

		SelectBrush(dc, oldbr);
		EndPaint(state->wnd, &ps);

		return 0;
	} break;
	case WM_DESTROY:
	{
		free(state);
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
		state->font = (HFONT)wparam;
		if ((BOOL)LOWORD(lparam) == TRUE) InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_GETFONT:
	{
		return (LRESULT)state->font;
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
		InvalidateRect(state->wnd, NULL, TRUE);
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
			SetFocus(GetNextDlgTabItem(GetParent(state->wnd), state->wnd, FALSE));
		}break;
		case VK_RETURN://Received when the user presses the "enter" key //Carriage Return aka \r
		{
			//SendMessage(state->wnd, WM_LBUTTONDOWN, ); //TODO(fran): repaint and show as if the user clicked the button
			PostMessage(state->parent, WM_COMMAND, (WPARAM)MAKELONG(state->msg_to_send, 0), (LPARAM)state->wnd);//notify parent
			
		}break;
		}
		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	default:
#ifdef _DEBUG
		Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	}
	return 0;
}