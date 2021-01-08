#pragma once
#include <Windows.h>
#include "unCap_Helpers.h"
#include "unCap_math.h"
#include <CommCtrl.h>
#include "unCap_scrollbar.h"
#include "protect_search.h"

#define edit_base_msg_addr (WM_USER+200)

#define EM_GET_MAX_VISIBLE_LINES (edit_base_msg_addr+1) /*Retrieves a count for the max number of lines that can be displayed at once in the current window. return=int*/
#define EM_SETVSCROLL (edit_base_msg_addr+2) /*Sets the vertical scrollbar that is to be used. wParam=HWND of the scrollbar control*/
//#define EM_SETHSCROLL (WM_USER+202) /*Sets the horizontal scrollbar that is to be used. wParam=HWND of the scrollbar control*/
//#define EM_GET_MAX_VISIBLE_CHARS_PER_LINE (WM_USER+203) /*Retrieves a count for the max number of characters that can be displayed in one line at once in the current window. return=int*/
#define WM_SAVE (edit_base_msg_addr+13) /*A control has asked to save its contents wParam=HWND ; lParam=unused*/
#define EM_SETSEARCHWND (edit_base_msg_addr+14) /*Sets the search wnd that is used, wParam=HWND of the search control ; lParam=unused*/

//TODO(fran): ability to search, use EM_GETHANDLE to get the cstr* without copying and search, keep count of \n for the percentage to scroll


struct EditProcState {
	bool initialized;
	HWND wnd;
	HWND parent;

	HWND vscrollbar;// , hscrollbar;
	HWND search;	//addon search window
};
//NOTE: EditProc controls require the creation of a EditProcState struct with calloc, and for it to be passed as the 4th param in SetWindowSubclass, the object will now be managed by the procedure and does not need the user to handle its memory
//NOTE: no left-right scrolling, it is line wrap handled
int EDIT_get_max_visible_lines(HWND hwnd) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	int page_height = RECTHEIGHT(rc);

	TEXTMETRIC tm;
	HDC dc = GetDC(hwnd);
	GetTextMetrics(dc, &tm);
	ReleaseDC(hwnd, dc);
	int line_height = tm.tmHeight + tm.tmExternalLeading;

	int visible_lines = safe_ratio0(page_height, line_height);

	return visible_lines;
}
void EDIT_update_scrollbar(EditProcState* state) {
	if (state->vscrollbar) {
		int line_count = EDIT_get_max_visible_lines(state->wnd);
		int range_max = (int)DefSubclassProc(state->wnd, EM_GETLINECOUNT, 0, 0);
		int pos = (int)DefSubclassProc(state->wnd, EM_GETFIRSTVISIBLELINE, 0, 0);
		U_SB_set_stats(state->vscrollbar, range_max, line_count, pos);
	}
}
LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData) {
	printf(msgToString(msg)); printf("\n");
	Assert(dwRefData);
	EditProcState* state = (EditProcState*)dwRefData;
	if (!state->initialized) {
		state->initialized = true;
		state->wnd = hwnd;
		state->parent = GetParent(hwnd);
	}

	//static long long msg_cnt = 0;
	//printf("%lld:%s\n",msg_cnt++, msgToString(msg));

	switch (msg) {
	case WM_KEYDOWN:
	{
		char vk = (char)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		switch (vk) {
		case _t('S')://Save
		{
			if (ctrl_is_down) {
				PostMessage(state->parent, WM_SAVE, (WPARAM)state->wnd, 0);
			}
		} break;
		case _t('F')://Find
		{
			//TODO(fran): what if this where pressed while the search wnd has focus? we need to intercept that window's msgs probably, maybe it automatically sends them to the parent, or we could make it do it
			if (ctrl_is_down) {
				if (state->search) {
					if (IsWindowVisible(state->search)) {
						ShowWindow(state->search, SW_HIDE);
					}
					else {
						ShowWindow(state->search, SW_SHOW);
						SetFocus(state->search);
					}

				}
			}
		} break;
		}
		return DefSubclassProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEWHEEL:
	{
		//TODO(fran): look at more reasonable algorithms, also this one should probably get a little exponential
		short zDelta = (short)(((float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA) * 3.f);
		if (zDelta >= 0)
			for (int i = 0; i < zDelta; i++)
				SendMessage(state->wnd, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), 0); //TODO(fran): use ScrollWindowEx ?
		else
			for (int i = 0; i > zDelta; i--)
				SendMessage(state->wnd, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), 0);
		return 0;
	} break;
	case EM_GET_MAX_VISIBLE_LINES: {
		return EDIT_get_max_visible_lines(hwnd);
	} break;
		/*case EM_GET_MAX_VISIBLE_CHARS_PER_LINE: {
			RECT rc;
			GetClientRect(hwnd, &rc);
			int line_width = RECTWIDTH(rc);

			TEXTMETRIC tm;
			HDC dc = GetDC(hwnd);
			GetTextMetrics(dc, &tm);
			ReleaseDC(hwnd, dc);
			int char_width = tm.tmAveCharWidth; //TODO(fran): better approximation

			int char_count = line_width / char_width;

			return char_count;
		} break;*/
	case EM_SETVSCROLL: 
	{
		state->vscrollbar = (HWND)wparam;
	} break;
	case EM_SETSEARCHWND:
	{
		state->search = (HWND)wparam;
		return 0;
	} break;
		//case EM_SETHSCROLL: {
		//	state->hscrollbar = (HWND)wparam;
		//} break;
	//case TCM_RESIZE: //TODO(fran): get rid of this, our parent should tell us our new size, after that this control can be sent to a separate .h file
	//{
	//	SIZE* control_size = (SIZE*)wparam;

	//	MoveWindow(hwnd, TabOffset.leftOffset, TabOffset.topOffset, control_size->cx - TabOffset.rightOffset - TabOffset.leftOffset, control_size->cy - TabOffset.bottomOffset - TabOffset.topOffset, TRUE);
	//	//x & y remain fixed and only width & height change

	//	SendMessage(state->vscrollbar, U_SB_AUTORESIZE, 0, 0);

	//	EDIT_update_scrollbar(state); //NOTE: actually here you just need to update nPage

	//	return TRUE;
	//}
	case WM_SIZE:
	{
		LRESULT res = DefSubclassProc(hwnd, msg, wparam, lparam);
		SIZE* control_size = (SIZE*)wparam;

		//MoveWindow(hwnd, TabOffset.leftOffset, TabOffset.topOffset, control_size->cx - TabOffset.rightOffset - TabOffset.leftOffset, control_size->cy - TabOffset.bottomOffset - TabOffset.topOffset, TRUE);
		//x & y remain fixed and only width & height change

		if (state->vscrollbar)SendMessage(state->vscrollbar, U_SB_AUTORESIZE, 0, 0);
		if (state->search)SendMessage(state->search, SRH_AUTORESIZE, 0, 0);

		EDIT_update_scrollbar(state); //NOTE: actually here you just need to update nPage

		return res;
	}
	case WM_DESTROY:
	{
		free(state);
		//return DefSubclassProc(hwnd, msg, wparam, lparam); //TODO(fran): shouldnt I call this?
	}break;
	//Messages that could trigger the need for updating the scrollbar
	case WM_PAINT:
	case EM_POSFROMCHAR:
		//TODO(fran): add more
	{
		LRESULT res = DefSubclassProc(hwnd, msg, wparam, lparam);
		EDIT_update_scrollbar(state);
		return res;
	} break;
	default:return DefSubclassProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

//Rich Edit
#include <Richedit.h>

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

