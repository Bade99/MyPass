#pragma once
#include <Windows.h>
#include "unCap_Global.h"
#include "unCap_Helpers.h"
#include "windows_undoc.h"

//NOTE: this scrollbar is only for edit controls (for now), and only vertical
//NOTE: it's important that the parent uses WS_CLIPCHILDREN to avoid horrible flickering

/*TODOs
TODO: scrollbar teleports when we press and move the mouse, cause we told it to do that, instead it should have the initial distance between scrollbar origin and mouse into account

TODO: reduce flickering when tracking, maybe use double buffering, example: http://www.catch22.net/tuts/win32/flicker-free-drawing from this I understand flicker will always happen if you overdraw, we could fix it now, since all we draw is squares we can calculate the exact area that needs bk clearing, but if we want to do rounded rectangles it's gonna be impossible to get right. another example: https://docs.microsoft.com/en-us/previous-versions/ms969905(v=msdn.10)?redirectedfrom=MSDN

TODO: scrollbar goes a little too far on the bottom and starts getting cut

TODO: establish clip region so we dont have to worry about errors drawing outside our client area (seems like BeginPaint takes cares of that automatically)
*/

constexpr TCHAR unCap_wndclass_scrollbar[] = TEXT("unCap_wndclass_scrollbar");

enum class ScrollBarPlacement { left, right, top, bottom }; //NOTE: left and right should only be used for vertical scrollbars, top and bottom for horizontal
struct ScrollProcState { //NOTE: must be initialized to zero
	ScrollBarPlacement place;
	HWND wnd;
	HWND parent;
	int range_min;
	int range_max;
	int page_sz;
	int p;

	bool onMouseOverSb;//The mouse is over our bar
	bool onLMouseClickBk;//The left click is being pressed on the background area
	bool OnMouseTrackingSb; //Left click was pressed in our bar and now is still being held, the user will probably be moving the mouse around, so we want to track it to move the scrollbar
	//NOTE: we'll probably also need a onMouseOverBk or just general mouseOver

	//bool fullBkRepaint;
	//int oldSb_idx;
	//RECT oldSb[3]; //WM_PAINT will annotate where it painted the scrollbar so WM_ERASEBACKGROUND can clean only those parts //TODO(fran): Im not sure we really need to store more than one, if we always ask for bk erasing on InvalidateRect
};

#define U_SB_AUTORESIZE (WM_USER + 300) /*Auto resize and reposition*/
#define U_SB_SET_PLACEMENT (WM_USER + 301) /*Sets ScrollBarPlacement and also makes the necessary repositioning and resizing ; wParam=ScrollBarPlacement*/
#define U_SB_SET_RANGEMAX (WM_USER + 302) /*Sets max scroll range (min is always 0) ; wParam=int*/
#define U_SB_SET_PAGESZ (WM_USER + 303) /*Sets max lines visible at the same time on the parent control ; wParam=int*/
#define U_SB_SET_POS (WM_USER + 304) /*Sets current position in the range ; wParam=int*/

void U_SB_set_stats(HWND hwnd, UINT rangemax, UINT pagesz, UINT pos) {
	SendMessage(hwnd, U_SB_SET_RANGEMAX, rangemax, 0);
	SendMessage(hwnd, U_SB_SET_PAGESZ, pagesz, 0);
	SendMessage(hwnd, U_SB_SET_POS, pos, 0);
}

static void SCROLL_resize_wnd(ScrollProcState* state, int scrollbar_thickness) {
	RECT r; GetWindowRect(state->parent, &r); //TODO(fran): im using GetWindowRect, but later it may be better to use GetClientRect
	int spacing = 2;// A few pixels of spacing so the control doesnt feel so suck to the corners

	int scroll_x, scroll_y, scroll_w, scroll_h;

	switch (state->place) {
		//vertical
	case ScrollBarPlacement::left:
	{
		scroll_x = spacing;
		scroll_y = spacing;
		scroll_w = scrollbar_thickness;
		scroll_h = RECTHEIGHT(r) - spacing;
	}break;
	case ScrollBarPlacement::right:
	{
		scroll_x = RECTWIDTH(r) - scrollbar_thickness - spacing;
		scroll_y = spacing;
		scroll_w = scrollbar_thickness;
		scroll_h = RECTHEIGHT(r) - spacing;
	}break;
	//horizontal
	case ScrollBarPlacement::top:
	{
		scroll_x = spacing;
		scroll_y = spacing;
		scroll_w = RECTWIDTH(r) - spacing;
		scroll_h = scrollbar_thickness;
	}break;
	case ScrollBarPlacement::bottom:
	{
		scroll_x = spacing;
		scroll_y = RECTHEIGHT(r) - scrollbar_thickness - spacing;
		scroll_w = RECTWIDTH(r) - spacing;
		scroll_h = scrollbar_thickness;
	}break;
	}
	RECT non_clipped_rc = rectWH(scroll_x, scroll_y, scroll_w, scroll_h);
	RECT clipped_rc = clip_fit_childs(state->parent, state->wnd, non_clipped_rc);

	MoveWindow(state->wnd, clipped_rc.left, clipped_rc.top, RECTWIDTH(clipped_rc), RECTHEIGHT(clipped_rc), TRUE);
}

static bool is_vertical(ScrollBarPlacement place) {
	return place == ScrollBarPlacement::left || place == ScrollBarPlacement::right;
}

static RECT SCROLL_calc_scrollbar(ScrollProcState* state) {
	RECT client_rc;
	GetClientRect(state->wnd, &client_rc);
	float sb_pos = safe_ratio0((float)state->p, (float)distance(state->range_max, state->range_min));
	float sb_sz = safe_ratio0((float)state->page_sz, (float)distance(state->range_max, state->range_min));
	LONG sb_lenght = (LONG)(sb_sz * (float)RECTHEIGHT(client_rc));

	LONG cursor_height = GetSystemMetrics(SM_CYCURSOR);
	if (sb_sz != 0) sb_lenght = max(sb_lenght, cursor_height);

	RECT sb_rc;
	sb_rc.left = client_rc.left;
	sb_rc.right = client_rc.right;
	sb_rc.top = (LONG)(sb_pos * (float)RECTHEIGHT(client_rc));
	sb_rc.bottom = sb_rc.top + sb_lenght;
	return sb_rc;
}

//TODO(fran): the scrollbar doesnt go with the style of the application, we should probably make it be just borders or something like that

static LRESULT CALLBACK ScrollProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	/*INFO:
		-the scrollbars decide position and size on their own based on their parent and whether they are horizontal or vertical
	*/
	static int scrollbar_thickness = max(GetSystemMetrics(SM_CXVSCROLL) / 2, 5);

	constexpr UINT_PTR timer_id_bk_click_held = 1;

	if (msg == WM_CREATE) {
		ScrollProcState* state = (ScrollProcState*)calloc(1, sizeof(ScrollProcState));
		Assert(state);
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)state);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		state->parent = creation_nfo->hwndParent;
		state->wnd = hwnd;
		//state->fullBkRepaint = true;
		return 0;
	}

	ScrollProcState* state = (ScrollProcState*)GetWindowLongPtr(hwnd, 0); //INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	//Assert(state); //NOTE: cannot check thanks to the grandeur of windows' hidden msgs before WM_CREATE
	switch (msg) {
	case WM_MOUSEWHEEL:
	{
		//Let the parent decide how to handle it
		return SendMessage(state->parent, msg, wparam, lparam);//TODO(fran): maybe PostMessage is better?

	} break;
	case WM_CANCELMODE:
	{
		printf("\n\n\nWM_CANCELMODE\n\n\n");
		//We got canceled by the system, not really sure what it means but doc says we should cancel everything scrollbar related, so stop tracking
		if (state->OnMouseTrackingSb) {
			ReleaseCapture();//stop capturing the mouse
			state->OnMouseTrackingSb = false;
		}
		state->onLMouseClickBk = false;
		state->onMouseOverSb = false;
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
		//Spy++ asks for it, so we'll give it to it, should just write '\0'
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
		if (state->OnMouseTrackingSb) {
			ReleaseCapture();//stop capturing the mouse
			state->OnMouseTrackingSb = false;
		}
		state->onLMouseClickBk = false;

		return 0;
	} break;
	case WM_TIMER:
	{
		if (timer_id_bk_click_held == (UINT_PTR)wparam) {
			KillTimer(state->wnd, timer_id_bk_click_held);
			if (state->onLMouseClickBk) {
				//Check the mouse is still in the bk area, it could be that it moved away or that the bar reached the timer's position
				POINT mouse;
				GetCursorPos(&mouse);
				ScreenToClient(state->wnd, &mouse);
				RECT client_rc; GetClientRect(state->wnd, &client_rc);
				RECT sb_rc = SCROLL_calc_scrollbar(state);
				if (test_pt_rc(mouse, client_rc) && !test_pt_rc(mouse, sb_rc)) {
					//Mouse is in bk area
					if (mouse.y < sb_rc.top) //mouse hit above the bar
						SendMessage(state->parent, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), (LPARAM)state->wnd);
					else //mouse hit below the bar
						SendMessage(state->parent, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), (LPARAM)state->wnd);

					SetTimer(state->wnd, timer_id_bk_click_held, USER_TIMER_MINIMUM, NULL);
				}
			}
			//TODO(fran): I have the feeling that you need to kill the timer, otherwise it'll be constantly spamming your msg queue once it reached the time
			return 0;
		}
	} break;
	case WM_LBUTTONDOWN:
	{
		//Left click is down
		if (state->onMouseOverSb) {
			//Click happened inside the bar, so we want to capture the mouse movement in case the user starts moving the mouse trying to scroll

			//TODO(fran): maybe first check that we arent already tracking (!state->OnMouseTrackingSb)
			SetCapture(state->wnd);//We want to keep capturing the mouse while the user is still pressing some button, even if the mouse leaves our client area
			state->OnMouseTrackingSb = true;
		}
		else {
			//Mouse is inside the background
			//TODO(fran): make sure it's inside the bk, cause when we are tracking we may still get this msg from outside the client area
			state->onLMouseClickBk = true;

			//Notify parent
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			RECT sb_rc = SCROLL_calc_scrollbar(state);
			if (mouse.y < sb_rc.top) //mouse hit above the bar
				SendMessage(state->parent, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), (LPARAM)state->wnd);
			else //mouse hit below the bar
				SendMessage(state->parent, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), (LPARAM)state->wnd);
			//Start timer to check if the user wants to continue scrolling
			SetTimer(state->wnd, timer_id_bk_click_held, 500, NULL);
		}
		InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_MOUSELEAVE:
	{
		//we asked TrackMouseEvent for this so we can update when the mouse leaves our client area, which we dont get notified about otherwise and is needed, for example when the user hovers on top the bar and then hovers outside the client area
		bool prev_onMouseOverSb = state->onMouseOverSb;
		POINT mouse;
		GetCursorPos(&mouse);
		ScreenToClient(state->wnd, &mouse);
		RECT sb_rc = SCROLL_calc_scrollbar(state);
		if (test_pt_rc(mouse, sb_rc)) {//Mouse is inside the bar
			state->onMouseOverSb = true;
		}
		else {//Mouse is outside the bar
			state->onMouseOverSb = false;
		}
		bool state_change = prev_onMouseOverSb != state->onMouseOverSb;
		if (state_change) {
			InvalidateRect(state->wnd, NULL, TRUE);
		}
		return 0;
	} break;
	case WM_MOUSEMOVE: //TODO(fran): scroll when mouse clicks the background
	{
		//printf("MOUSEMOVE\n");
		//After WM_NCHITTEST and WM_SETCURSOR we finally get that the mouse has moved
		//Sent to the window where the cursor is, unless someone else is explicitly capturing it, in which case this gets sent to them
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//client coords, relative to upper-left corner

		//Store previous state
		bool prev_onMouseOverSb = state->onMouseOverSb;
		bool prev_OnMouseTrackingSb = state->OnMouseTrackingSb;

		RECT sb_rc = SCROLL_calc_scrollbar(state);
		if (test_pt_rc(mouse, sb_rc)) {//Mouse is inside the bar
			state->onMouseOverSb = true;
		}
		else {//Mouse is outside the bar
			state->onMouseOverSb = false;
		}

		//With wparam you can test for different button presses, (wparam & MK_LBUTTON) --> left button down
		if (state->OnMouseTrackingSb) {
			//We are tracking the mouse to move the scrollbar
			RECT client_rc;
			GetClientRect(state->wnd, &client_rc);

			int mouse_p = clamp(client_rc.top, mouse.y, client_rc.bottom); //0 to height
			float p_ratio = safe_ratio0((float)mouse_p, (float)RECTHEIGHT(client_rc));//0.0 to 1.0
			state->p = (int)((float)distance(state->range_max, state->range_min) * p_ratio); //min to max range

			//Notify parent
			SendMessage(state->parent, WM_VSCROLL, MAKELONG(SB_THUMBTRACK, state->p), (LPARAM)state->wnd);
			//TODO(fran): I suppose we're gonna have a problem with this sometimes, since from the edit control's side I update the scrollbar too when it repaints/scrolls and all the cases I set

			InvalidateRect(state->wnd, NULL, TRUE);
		}

		bool state_change = prev_onMouseOverSb != state->onMouseOverSb || prev_OnMouseTrackingSb != state->OnMouseTrackingSb;
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
		RECT rcWindow; GetWindowRect(state->wnd, &rcWindow);

		LRESULT hittest = HTNOWHERE;

		// Determine if the point is inside the window
		if (mouse.y >= rcWindow.top &&//top
			mouse.y < rcWindow.bottom &&//bottom
			mouse.x >= rcWindow.left &&//left
			mouse.x < rcWindow.right)//right
		{
			hittest = HTCLIENT;//HTVSCROLL
		}
		return hittest;
	}
	case WM_ERASEBKGND:
	{
		//You receive this msg if you didnt specify hbrBackground  when you registered the class, now it's up to you to draw the background
		//TODO(fran): some guy said this is old stuff to try to reduce calls to WM_PAINT and not needed anymore, so maybe I should just do the background in WM_PAINT
		HDC dc = (HDC)wparam;
		//TODO(fran): look at https://docs.microsoft.com/en-us/windows/win32/gdi/drawing-a-custom-window-background and SetMapModek, allows for transforms

		//TODO(fran): see what we do on resize
		//if (state->fullBkRepaint) {
		//	state->fullBkRepaint = false;
		//	RECT r;
		//	GetClientRect(hwnd, &r);
		//	FillRect(dc, &r, unCap_colors.ScrollbarBk);
		//}
		//else {
		//	for(int i=0; i < state->oldSb_idx; i++)
		//		FillRect(dc, &state->oldSb[i], unCap_colors.ScrollbarBk);
		//}
		//state->oldSb_idx = 0;

		return 1; //If you return 0 then on WM_PAINT fErase will be true, aka paint it there
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
	case U_SB_SET_RANGEMAX:
	{
		state->range_max = (int)wparam;
		InvalidateRect(state->wnd, NULL, TRUE);

	} break;
	case U_SB_SET_PAGESZ:
	{
		state->page_sz = (int)wparam;
		InvalidateRect(state->wnd, NULL, TRUE);

	} break;
	case U_SB_SET_POS:
	{
		state->p = (int)wparam;
		InvalidateRect(state->wnd, NULL, TRUE);
	} break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state->wnd, &ps);

		{//TEST
#if 0
			float sb_pos = (float)state->p;

			printf("SB_POS=%f\n", sb_pos);
			sb_pos = safe_ratio0((float)sb_pos, (float)distance(state->range_max, state->range_min));
			printf("SB_RENDER_POS=%f%%\n", sb_pos * 100.f);
#endif
		}
		//Calculate new bar rc
		RECT sb_rc = SCROLL_calc_scrollbar(state);
		//Check that we are going to paint a new rc
		//if (!state->oldSb_idx || !sameRc(sb_rc, state->oldSb[state->oldSb_idx])) { //TODO(fran): this should check for the color that was used, since it does happen that the bar stays in the same place but changes color, that would allow for not always asking InvalidateRect to send wm_erasebk
			//Decide bar color
		HBRUSH sb_br;
		if (state->onMouseOverSb || state->OnMouseTrackingSb) sb_br = unCap_colors.ScrollbarMouseOver;
		else sb_br = unCap_colors.Scrollbar;

		//Annotate bar RECT for WM_ERASEBK to remove next time
		//state->oldSb[state->oldSb_idx++] = sb_rc;
		//printf("SB IDX: %d\n", state->oldSb_idx);
		//Assert(state->oldSb_idx < ARRAYSIZE(state->oldSb));
		//TODO(fran): check that we are painting at a new position, if not do not paint

		//Paint the bar
#define _TRANSPARENTSB 1 /*TODO(fran): this can be made into a user definable style*/
#if _TRANSPARENTSB
		int sb_border_thickness = 1;
		FillRectBorder(dc, sb_rc, sb_border_thickness, sb_br, BORDERLEFT | BORDERTOP | BORDERRIGHT | BORDERBOTTOM);
#else
		FillRect(dc, &sb_rc, sb_br);//TODO(fran): bilinear blend, aka subpixel precision rendering so we dont get bar hickups 
#endif

		//Clip the drawing region
		{
			HRGN restoreRegion = CreateRectRgn(0, 0, 0, 0); if (GetClipRgn(dc, restoreRegion) != 1) { DeleteObject(restoreRegion); restoreRegion = NULL; }defer{ SelectClipRgn(dc, restoreRegion); if (restoreRegion != NULL) DeleteObject(restoreRegion); };
#if _TRANSPARENTSB
			RECT left = rectNpxL(sb_rc, sb_border_thickness);
			RECT top = rectNpxT(sb_rc, sb_border_thickness);
			RECT right = rectNpxR(sb_rc, sb_border_thickness);
			RECT bottom = rectNpxB(sb_rc, sb_border_thickness);
			ExcludeClipRect(dc, left.left, left.top, left.right, left.bottom);
			ExcludeClipRect(dc, top.left, top.top, top.right, top.bottom);
			ExcludeClipRect(dc, right.left, right.top, right.right, right.bottom);
			ExcludeClipRect(dc, bottom.left, bottom.top, bottom.right, bottom.bottom);
#else
			ExcludeClipRect(dc, sb_rc.left, sb_rc.top, sb_rc.right, sb_rc.bottom);
#endif
			//Draw bk
			RECT rc;
			GetClientRect(state->wnd, &rc);
			FillRect(dc, &rc, unCap_colors.ScrollbarBk);
			//Restore old region
		}

		EndPaint(state->wnd, &ps);
		return 0;
	} break;
	case U_SB_SET_PLACEMENT:
	{
		state->place = (ScrollBarPlacement)wparam;
		Assert(is_vertical(state->place));
		SendMessage(state->wnd, U_SB_AUTORESIZE, 0, 0);
	} break;
	case U_SB_AUTORESIZE:
	{
		SCROLL_resize_wnd(state, scrollbar_thickness);
	} break;
	case WM_DESTROY:
	{
		free(state);
	}break;
	case WM_INPUTLANGCHANGE://For some reason the scrollbar also gets this
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}break;
	default:
#ifdef _DEBUG
		Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	}
	return 0;
}


void init_wndclass_unCap_scrollbar(HINSTANCE instance) {
	WNDCLASSEXW scrollbar;

	scrollbar.cbSize = sizeof(scrollbar);
	scrollbar.style = CS_HREDRAW | CS_VREDRAW;
	scrollbar.lpfnWndProc = ScrollProc;
	scrollbar.cbClsExtra = 0;
	scrollbar.cbWndExtra = sizeof(ScrollProcState*);
	scrollbar.hInstance = instance;
	scrollbar.hIcon = NULL;
	scrollbar.hCursor = LoadCursor(nullptr, IDC_ARROW);
	scrollbar.hbrBackground = NULL;
	scrollbar.lpszMenuName = NULL;
	scrollbar.lpszClassName = unCap_wndclass_scrollbar;
	scrollbar.hIconSm = NULL;

	ATOM class_atom = RegisterClassExW(&scrollbar);
	Assert(class_atom);
}