#pragma once
#include "win_sdk.h"
#include "global.h"
#include "helpers.h"

/*INFO:
	- The Scrollbar decides its position and size on its own based on its parent and whether it's horizontal or vertical
	- The Scrollbar is only usable with edit controls (for now)
	- IMPORTANT: the parent must use WS_CLIPCHILDREN to avoid horrible flickering
*/

/*TODOs
TODO: reduce flickering when tracking -> use double buffering, example: http://www.catch22.net/tuts/win32/flicker-free-drawing from this I understand flicker will always happen if you overdraw. Currently, since all we draw is squares we can calculate the exact area that needs bk clearing, but if we want to do rounded rectangles it's gonna be impossible to get right, we need double buffering. another example: https://docs.microsoft.com/en-us/previous-versions/ms969905(v=msdn.10)?redirectedfrom=MSDN

*/

namespace scrollbar {

void set_stats(HWND wnd, u32 rangemax, u32 pagesz, u32 pos) {
	SendMessage(wnd, U_SB_SET_RANGEMAX, rangemax, 0);
	SendMessage(wnd, U_SB_SET_PAGESZ, pagesz, 0);
	SendMessage(wnd, U_SB_SET_POS, pos, 0);
}

static void resize_wnd(State& state, i32 scrollbar_thickness) {
	RECT r; GetClientRect(state.parent, &r);
	i32 spacing = 2;// A few pixels of spacing so the control doesnt feel so stuck to the corners

	i32 scroll_x, scroll_y, scroll_w, scroll_h;

	switch (state.placement) {
	//vertical
	case Placement::left:
	{
		scroll_x = spacing;
		scroll_y = spacing;
		scroll_w = scrollbar_thickness;
		scroll_h = RECTH(r) - spacing;
	}break;
	case Placement::right:
	{
		scroll_x = RECTW(r) - scrollbar_thickness - spacing;
		scroll_y = spacing;
		scroll_w = scrollbar_thickness;
		scroll_h = RECTH(r) - spacing;
	}break;
	//horizontal
	case Placement::top:
	{
		scroll_x = spacing;
		scroll_y = spacing;
		scroll_w = RECTW(r) - spacing;
		scroll_h = scrollbar_thickness;
	}break;
	case Placement::bottom:
	{
		scroll_x = spacing;
		scroll_y = RECTH(r) - scrollbar_thickness - spacing;
		scroll_w = RECTW(r) - spacing;
		scroll_h = scrollbar_thickness;
	}break;
	}
	RECT non_clipped_rc = rectWH(scroll_x, scroll_y, scroll_w, scroll_h);
	RECT clipped_rc = clip_fit_childs(state.parent, state.wnd, non_clipped_rc);

	MoveWindow(state.wnd, clipped_rc.left, clipped_rc.top, RECTW(clipped_rc), RECTH(clipped_rc), TRUE);
}

bool is_vertical(State& state) {
	return state.placement == Placement::left || state.placement == Placement::right;
}

RECT calc_scrollbar(State& state) {
	RECT client_rc;
	GetClientRect(state.wnd, &client_rc);
	f32 sb_pos = safe_ratio0((f32)state.p, (f32)distance(state.range_max, state.range_min));
	f32 sb_sz = safe_ratio0((f32)state.page_sz, (f32)distance(state.range_max, state.range_min));
	bool vertical = is_vertical(state);
	f32 client_extent = vertical ? RECTH(client_rc) : RECTW(client_rc);
	f32 sb_lenght = (sb_sz * client_extent);

	LONG cursor_height = GetSystemMetrics(SM_CYCURSOR);
	if (sb_sz != 0) sb_lenght = maximum(sb_lenght, cursor_height);

	RECT sb_rc;
	if (vertical) {
		sb_rc.left = client_rc.left;
		sb_rc.right = client_rc.right;
		sb_rc.top = (i32)(client_rc.top + sb_pos * (client_extent - sb_lenght));
		sb_rc.bottom = (i32)(sb_rc.top + sb_lenght);
	}
	else {
		sb_rc.left = (i32)(client_rc.left + sb_pos * (client_extent - sb_lenght));
		sb_rc.right = (i32)(sb_rc.left + sb_lenght);
		sb_rc.top = client_rc.top;
		sb_rc.bottom = client_rc.bottom;
	}
	return sb_rc;
}

auto get_state(HWND wnd) { _control_create_function__get_state }

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	auto& state = *get_state(hwnd); _control_validate_state;
	switch (msg) {
	case WM_MOUSEWHEEL:
	{
		return SendMessage(state.parent, msg, wparam, lparam);
	} break;
	case WM_CANCELMODE:
	{
		if (state.OnMouseTrackingSb) {
			ReleaseCapture();
			state.OnMouseTrackingSb = false;
		}
		state.onLMouseClickBk = false;
		state.onMouseOverSb = false;
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
		if (state.OnMouseTrackingSb) {
			ReleaseCapture();
			state.OnMouseTrackingSb = false;
		}
		state.onLMouseClickBk = false;
		return 0;
	} break;
	case WM_TIMER:
	{
		if (timer_id_bk_click_held == (UINT_PTR)wparam) {
			KillTimer(state.wnd, timer_id_bk_click_held);
			if (state.onLMouseClickBk) {
				//Check the mouse is still in the bk area, it could be that it moved away or that the bar reached the timer's position
				POINT mouse;
				GetCursorPos(&mouse);
				ScreenToClient(state.wnd, &mouse);
				RECT client_rc; GetClientRect(state.wnd, &client_rc);
				RECT sb_rc = calc_scrollbar(state);
				if (test_pt_rc(mouse, client_rc) && !test_pt_rc(mouse, sb_rc)) {
					//Mouse is in bk area
					if (is_vertical(state)) {
						if (mouse.y < sb_rc.top) //mouse hit above the bar
							SendMessage(state.parent, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), (LPARAM)state.wnd);
						else //mouse hit below the bar
							SendMessage(state.parent, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), (LPARAM)state.wnd);
					}
					else {
						if (mouse.x < sb_rc.left) //mouse hit left of the bar
							SendMessage(state.parent, WM_HSCROLL, MAKELONG(SB_PAGELEFT, 0), (LPARAM)state.wnd);
						else //mouse hit right of the bar
							SendMessage(state.parent, WM_HSCROLL, MAKELONG(SB_PAGERIGHT, 0), (LPARAM)state.wnd);
					}
					SetTimer(state.wnd, timer_id_bk_click_held, USER_TIMER_MINIMUM, NULL);
				}
			}
		}
		return 0;
	} break;
	case WM_LBUTTONDOWN:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		
		if (state.onMouseOverSb) {
			//Click happened inside the bar, so we want to capture the mouse movement in case the user starts moving the mouse trying to scroll

			if (!state.OnMouseTrackingSb) { //Check that we are not already tracking to prevent shadow clicks from affecting us
				SetCapture(state.wnd);//Keep capturing the mouse while the user is still pressing the button, even if the mouse leaves our client area
				state.OnMouseTrackingSb = true;

				RECT sb = calc_scrollbar(state);

				state.mouseStartDeltaP = is_vertical(state) ? mouse.y - sb.top : mouse.x - sb.left;
			}
		}
		else {
			//Mouse is inside the background
			//TODO(fran): make sure it's inside the bk, cause when we are tracking we may still get this msg from outside the client area
			state.onLMouseClickBk = true;

			//Notify parent
			RECT sb_rc = calc_scrollbar(state);
			if (is_vertical(state)) {
				if (mouse.y < sb_rc.top) //mouse hit above the bar
					SendMessage(state.parent, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), (LPARAM)state.wnd);
				else //mouse hit below the bar
					SendMessage(state.parent, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), (LPARAM)state.wnd);
			}
			else {
				if (mouse.x < sb_rc.top) //mouse hit left of the bar
					SendMessage(state.parent, WM_HSCROLL, MAKELONG(SB_PAGELEFT, 0), (LPARAM)state.wnd);
				else //mouse hit right of the bar
					SendMessage(state.parent, WM_HSCROLL, MAKELONG(SB_PAGERIGHT, 0), (LPARAM)state.wnd);
			}
			//Start timer to check if the user wants to continue scrolling
			SetTimer(state.wnd, timer_id_bk_click_held, 500, NULL);
		}
		ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_MOUSELEAVE:
	{
		POINT mouse; GetCursorPos(&mouse); ScreenToClient(state.wnd, &mouse);
		bool prev_onMouseOverSb = state.onMouseOverSb;
		state.onMouseOverSb = test_pt_rc(mouse, calc_scrollbar(state));
		if (prev_onMouseOverSb != state.onMouseOverSb) ask_window_for_repaint(state.wnd);
		return 0;
	} break;
	case WM_MOUSEMOVE:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		bool prev_onMouseOverSb = state.onMouseOverSb;
		bool prev_OnMouseTrackingSb = state.OnMouseTrackingSb;

		RECT sb_rc = calc_scrollbar(state);
		state.onMouseOverSb = test_pt_rc(mouse, sb_rc);

		if (state.OnMouseTrackingSb) {
			//We are tracking the mouse to move the scrollbar
			RECT client_rc;
			GetClientRect(state.wnd, &client_rc);

			i32 mouse_location;
			f32 sb_extent;
			if (is_vertical(state)) {
				mouse_location = mouse.y;
				sb_extent = RECTH(client_rc) - RECTH(sb_rc);
			}
			else {
				mouse_location = mouse.x;
				sb_extent = RECTW(client_rc) - RECTW(sb_rc);
			}

			f32 mouse_p = mouse_location - state.mouseStartDeltaP; //0 to height
			float p_ratio = clamp(0.0f, safe_ratio0(mouse_p, sb_extent), 1.0f);//0.0 to 1.0
			state.p = (int)(distance(state.range_max, state.range_min) * p_ratio); //min to max range

			//TODO-INFO(fran): The scrollbar has additional travel distance after the text editor has already reached the end. This is because the text editor scrolls by the first visible line instead of the last one. I dont really think that this should be fixed because I like being able to over-scroll further down, but unfortunately windows' default text editor cannot do that.

			//Notify parent
			SendMessage(state.parent, WM_VSCROLL, MAKELONG(SB_THUMBTRACK, state.p), (LPARAM)state.wnd);
			//TODO(fran): I suppose we're gonna have a problem with this sometimes, since from the edit control's side I update the scrollbar too when it repaints/scrolls and all the cases I set

			ask_window_for_repaint(state.wnd);
		}

		bool state_change = prev_onMouseOverSb != state.onMouseOverSb || prev_OnMouseTrackingSb != state.OnMouseTrackingSb;
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
		return MA_ACTIVATE;
	} break;
	case WM_NCHITTEST:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		RECT rcWindow; GetWindowRect(state.wnd, &rcWindow);
		LRESULT hittest = test_pt_rc(mouse, rcWindow) ? HTCLIENT : HTNOWHERE; //HTVSCROLL
		return hittest;
	} break;
	case WM_ERASEBKGND:
	{
		return 1; //1: say that we erased the bk | 0: bk should be painted on WM_PAINT (fErase is true)
	} break;
	case WM_NCPAINT:
	case WM_CREATE:
	case WM_NCCALCSIZE: 
	{
		return 0;
	} break;
	case WM_NCCREATE: 
	{
		State* state = (State*)calloc(1, sizeof(State));
		Assert(state);
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)state);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		state->parent = creation_nfo->hwndParent;
		state->wnd = hwnd;
		return 1;
	} break;
	case U_SB_SET_RANGEMAX:
	{
		state.range_max = (int)wparam;
		ask_window_for_repaint(state.wnd);

	} break;
	case U_SB_SET_PAGESZ:
	{
		state.page_sz = (int)wparam;
		ask_window_for_repaint(state.wnd);

	} break;
	case U_SB_SET_POS:
	{
		state.p = (int)wparam;
		ask_window_for_repaint(state.wnd);
	} break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };

		RECT sb_rc = calc_scrollbar(state);
		HBRUSH sb_br;
		if (state.onMouseOverSb || state.OnMouseTrackingSb) sb_br = colors.ScrollbarMouseOver;
		else sb_br = colors.Scrollbar;

		//Paint the bar
#define _TRANSPARENTSB 1 /*TODO(fran): this can be made into a user definable style*/
#if _TRANSPARENTSB
		int sb_border_thickness = 1;
		FillRectBorder(dc, sb_rc, sb_border_thickness, sb_br, BORDERLEFT | BORDERTOP | BORDERRIGHT | BORDERBOTTOM);
#else
		FillRect(dc, &sb_rc, sb_br);//TODO(fran): bilinear blend, aka subpixel precision rendering so we dont get bar hickups 
#endif

		//Clip the drawing region for the background to exclude the scrollbar itself, thus avoiding overdraw and flickering
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
			GetClientRect(state.wnd, &rc);
			FillRect(dc, &rc, colors.ScrollbarBk);
			//Restore old region
		}

		return 0;
	} break;
	case U_SB_SET_PLACEMENT:
	{
		state.placement = (Placement)wparam;
		SendMessage(state.wnd, U_SB_AUTORESIZE, 0, 0);
	} break;
	case U_SB_AUTORESIZE:
	{
		int scrollbar_thickness = maximum(GetSystemMetrics(SM_CXVSCROLL) * .7f, 5);
		resize_wnd(state, scrollbar_thickness);
	} break;
	case WM_NCDESTROY:
	{
		set_window_state(state.wnd, nil);
		free(&state);
	}break;
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