#pragma once
#include <Windows.h>
#include <windowsx.h>
#include "windows_msg_mapper.h"
#include <vector>
#include <Shlwapi.h>
#include "windows_undoc.h"
#include "unCap_Helpers.h"
#include "unCap_Reflection.h"

//TODO(fran): show password button

//NOTE: this took two days to fully implement, certainly a hard control but not as much as it's made to believe, obviously im just doing single line but extrapolating to multiline isnt much harder now a single line works "all right"

constexpr TCHAR unCap_wndclass_edit_oneline[] = TEXT("unCap_wndclass_edit_oneline");

#define editoneline_base_msg_addr (WM_USER + 1500)
#define WM_SETDEFAULTTEXT (editoneline_base_msg_addr+1) /*wparam=unused ; lparam=pointer to null terminated cstring*/ /*TODO(fran): many different windows could use this msg, it should be global*/
#define EM_SETINVALIDCHARS (editoneline_base_msg_addr+2) /*characters that you dont want the edit control to accept either trough WM_CHAR or WM_PASTE*/ /*lparam=unused ; wparam=pointer to null terminated cstring, each char will be added as invalid*/ /*INFO: overrides the previously invalid characters that were set before*/ /*TODO(fran): what to do on pasting? reject the entire string or just ignore those chars*/

//Notification msgs, sent to the parent through WM_COMMAND with LOWORD(wparam)=control identifier ; HIWORD(wparam)=msg code ; lparam=HWND of the control
//IMPORTANT: this are sent _only_ if you have set an identifier for the control (hMenu on CreateWindow)
#define _editoneline_notif_base_msg 0x0104
#define EN_ENTER (_editoneline_notif_base_msg+1) /*User has pressed the enter key*/
#define EN_ESCAPE (_editoneline_notif_base_msg+2) /*User has pressed the escape key*/


#define EDITONELINE_default_tooltip_duration 3000 /*ms*/
#define EDITONELINE_tooltip_timer_id 0xf1

//---------Differences with a default oneline edit control
//·EN_CHANGE is sent when there is a modification to the text, of any type, and it's sent immediately, not after rendering

static LRESULT CALLBACK EditOnelineProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

constexpr cstr password_char = sizeof(password_char) > 1 ? _t('●') : _t('*');

struct char_sel { int x, y; };//xth character of the yth row, goes left to right and top to bottom
struct txt_sel { char_sel min, max; }; //TODO(fran): implement selections
struct EditOnelineProcState {
	HWND wnd;
	HWND parent;
	u32 identifier;
	struct editonelinebrushes {
		HBRUSH txt, bk, border;//NOTE: for now we use the border color for the caret
		HBRUSH txt_dis, bk_dis, border_dis; //disabled
	}brushes;
	u32 char_max_sz;//doesnt include terminating null, also we wont have terminating null
	HFONT font;
	char_sel char_cur_sel;//this is in "character" coordinates, zero-based
	struct caretnfo {
		HBITMAP bmp;
		SIZE dim;
		POINT pos;//client coords, it's also top down so this is the _top_ of the caret
	}caret;

	str char_text;//much simpler to work with and debug
	std::vector<int> char_dims;//NOTE: specifies, for each character, its width
	//TODO(fran): it's probably better to use signed numbers in case the text overflows ths size of the control
	int char_pad_x;//NOTE: specifies x offset from which characters start being placed on the screen, relative to the client area. For a left aligned control this will be offset from the left, for right aligned it'll be offset from the right, and for center alignment it'll be the left most position from where chars will be drawn
	cstr default_text[100]; //NOTE: uses txt_dis brush for rendering
	cstr invalid_chars[100];

	union EditOnelineControls {
		struct {
			HWND tooltip;
		};
		HWND all[1];//REMEMBER TO UPDATE
	}controls;
};

void init_wndclass_unCap_edit_oneline(HINSTANCE instance) {
	WNDCLASSEXW edit_oneline;

	edit_oneline.cbSize = sizeof(edit_oneline);
	edit_oneline.style = CS_HREDRAW | CS_VREDRAW;
	edit_oneline.lpfnWndProc = EditOnelineProc;
	edit_oneline.cbClsExtra = 0;
	edit_oneline.cbWndExtra = sizeof(EditOnelineProcState*);
	edit_oneline.hInstance = instance;
	edit_oneline.hIcon = NULL;
	edit_oneline.hCursor = LoadCursor(nullptr, IDC_IBEAM);
	edit_oneline.hbrBackground = NULL;
	edit_oneline.lpszMenuName = NULL;
	edit_oneline.lpszClassName = unCap_wndclass_edit_oneline;
	edit_oneline.hIconSm = NULL;

	ATOM class_atom = RegisterClassExW(&edit_oneline);
	Assert(class_atom);
}

EditOnelineProcState* EDITONELINE_get_state(HWND wnd) {
	EditOnelineProcState* state = (EditOnelineProcState*)GetWindowLongPtr(wnd, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

void EDITONELINE_set_state(HWND wnd, EditOnelineProcState* state) {//NOTE: only used on creation
	SetWindowLongPtr(wnd, 0, (LONG_PTR)state);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
}

//NOTE: the caller takes care of deleting the brushes, we dont do it
//TODO(fran): should have border as first param after repaint to be equal to set_brushes for BUTTON
void EDITONELINE_set_brushes(HWND editoneline, BOOL repaint, HBRUSH txt, HBRUSH bk, HBRUSH border, HBRUSH txt_disabled, HBRUSH bk_disabled, HBRUSH border_disabled) {
	EditOnelineProcState* state = EDITONELINE_get_state(editoneline);
	if (state) {
		if (txt)state->brushes.txt = txt;
		if (bk)state->brushes.bk = bk;
		if (border)state->brushes.border = border;
		if (txt_disabled)state->brushes.txt_dis = txt_disabled;
		if (bk_disabled)state->brushes.bk_dis = bk_disabled;
		if (border_disabled)state->brushes.border_dis = border_disabled;
		if (repaint)InvalidateRect(state->wnd, NULL, TRUE);
	}
}

SIZE EDITONELINE_calc_caret_dim(EditOnelineProcState* state) {
	SIZE res;
	HDC dc = GetDC(state->wnd); defer{ ReleaseDC(state->wnd,dc); };
	HFONT oldfont = SelectFont(dc, state->font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm;
	GetTextMetrics(dc, &tm);
	int avg_height = tm.tmHeight;
	res.cx = 1;
	res.cy = avg_height;
	return res;
}

int EDITONELINE_calc_line_height_px(EditOnelineProcState* state) {
	int res;
	HDC dc = GetDC(state->wnd); defer{ ReleaseDC(state->wnd,dc); };
	HFONT oldfont = SelectFont(dc, state->font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm;
	GetTextMetrics(dc, &tm);
	res = tm.tmHeight;
	return res;
}

SIZE EDITONELINE_calc_char_dim(EditOnelineProcState* state, cstr c) {
	Assert(c != _t('\t'));

	LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
	if (style & ES_PASSWORD) c = password_char;

	SIZE res;
	HDC dc = GetDC(state->wnd); defer{ ReleaseDC(state->wnd,dc); };
	HFONT oldfont = SelectFont(dc, state->font); defer{ SelectFont(dc, oldfont); };
	//UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };
	//SetTextAlign(dc, TA_LEFT);
	GetTextExtentPoint32(dc, &c, 1, &res);
	return res;
}

SIZE EDITONELINE_calc_text_dim(EditOnelineProcState* state) {
	SIZE res;
	LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
	if (style & ES_PASSWORD) {
		res = EDITONELINE_calc_char_dim(state, 0);
		res.cx *= state->char_text.length();
	}
	else {
		HDC dc = GetDC(state->wnd); defer{ ReleaseDC(state->wnd,dc); };
		HFONT oldfont = SelectFont(dc, state->font); defer{ SelectFont(dc, oldfont); };
		//UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };
		//SetTextAlign(dc, TA_LEFT);
		GetTextExtentPoint32(dc, state->char_text.c_str(), (int)state->char_text.length(), &res);
	}

	return res;
}

POINT EDITONELINE_calc_caret_p(EditOnelineProcState* state) {
	Assert(state->char_cur_sel.x - 1 < (int)state->char_dims.size());
	POINT res = { state->char_pad_x, 0 };
	for (int i = 0; i < state->char_cur_sel.x; i++) {
		res.x += state->char_dims[i];
		//TODO(fran): probably wrong
	}
	//NOTE: we are single line so we always want the text to be vertically centered, same goes for the caret
	RECT rc; GetClientRect(state->wnd, &rc);
	int wnd_h = RECTHEIGHT(rc);
	int caret_h = state->caret.dim.cy;
	res.y = (wnd_h - state->caret.dim.cy) / 2;
	return res;
}

BOOL SetCaretPos(POINT p) {
	BOOL res = SetCaretPos(p.x, p.y);
	return res;
}

//TODOs:
//-caret stops blinking after a few seconds of being shown, this seems to be a windows """feature""", we may need to set a timer to re-blink the caret every so often while we have keyboard focus

bool operator==(POINT p1, POINT p2) {
	bool res = p1.x == p2.x && p1.y == p2.y;
	return res;
}

bool operator!=(POINT p1, POINT p2) {
	bool res = !(p1 == p2);
	return res;
}

//NOTE: pasting from the clipboard establishes a couple of invariants: lines end with \r\n, there's a null terminator, we gotta parse it carefully cause who knows whats inside
bool EDITONELINE_paste_from_clipboard(EditOnelineProcState* state, const cstr* txt) { //returns true if it could paste something
	bool res = false;
	size_t char_sz = cstr_len(txt);//does not include null terminator
	if ((size_t)state->char_max_sz >= state->char_text.length() + char_sz) {

	}
	else {
		char_sz -= ((state->char_text.length() + char_sz) - (size_t)state->char_max_sz);
	}
	if (char_sz > 0) {
		//TODO(fran): remove invalid chars (we dont know what could be inside)
		state->char_text.insert((size_t)state->char_cur_sel.x, txt, char_sz);
		for (size_t i = 0; i < char_sz; i++) state->char_dims.insert(state->char_dims.begin() + state->char_cur_sel.x + i, EDITONELINE_calc_char_dim(state, txt[i]).cx);
		state->char_cur_sel.x += (int)char_sz;

		LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
		if (style & ES_CENTER) {
			//Recalc pad_x
			RECT rc; GetClientRect(state->wnd, &rc);
			state->char_pad_x = (RECTWIDTH(rc) - EDITONELINE_calc_text_dim(state).cx) / 2;

		}
		state->caret.pos = EDITONELINE_calc_caret_p(state);
		SetCaretPos(state->caret.pos);
		res = true;
	}
	return res;
}

void EDITONELINE_set_composition_pos(EditOnelineProcState* state)//TODO(fran): this must set the other stuff too
{
	HIMC imc = ImmGetContext(state->wnd);
	if (imc != NULL) {
		defer{ ImmReleaseContext(state->wnd, imc); };
		COMPOSITIONFORM cf;
		//NOTE immgetcompositionwindow fails
#if 0 //IME window no longer draws the unnecessary border with resizing and button, it is placed at cf.ptCurrentPos and extends down after each character
		cf.dwStyle = CFS_POINT;

		if (GetFocus() == state->wnd) {
			//NOTE: coords are relative to your window area/nc area, _not_ screen coords
			cf.ptCurrentPos.x = state->caret.pos.x;
			cf.ptCurrentPos.y = state->caret.pos.y + state->caret.dim.cy;
		}
		else {
			cf.ptCurrentPos.x = 0;
			cf.ptCurrentPos.y = 0;
		}
#else //IME window no longer draws the unnecessary border with resizing and button, it is placed at cf.ptCurrentPos and has a max size of cf.rcArea in the x axis, the y axis can extend a lot longer, basically it does what it wants with y
		cf.dwStyle = CFS_RECT;

		//TODO(fran): should I place the IME in line with the caret or below so the user can see what's already written in that line?
		if (GetFocus() == state->wnd) {
			cf.ptCurrentPos.x = state->caret.pos.x;
			cf.ptCurrentPos.y = state->caret.pos.y + state->caret.dim.cy;
			//TODO(fran): programatically set a good x axis size
			cf.rcArea = { state->caret.pos.x , state->caret.pos.y + state->caret.dim.cy,state->caret.pos.x + 100,state->caret.pos.y + state->caret.dim.cy + 100 };
		}
		else {
			cf.rcArea = { 0,0,0,0 };
			cf.ptCurrentPos.x = 0;
			cf.ptCurrentPos.y = 0;
		}
#endif
		BOOL res = ImmSetCompositionWindow(imc, &cf); Assert(res);
	}
}

void EDITONELINE_set_composition_font(EditOnelineProcState* state)//TODO(fran): this must set the other stuff too
{
	HIMC imc = ImmGetContext(state->wnd);
	if (imc != NULL) {
		defer{ ImmReleaseContext(state->wnd, imc); };
		LOGFONT lf;
		int getobjres = GetObject(state->font, sizeof(lf), &lf); Assert(getobjres == sizeof(lf));

		BOOL setres = ImmSetCompositionFont(imc, &lf); Assert(setres);
	}
}

//True if "c" is in "chars", stops on the null terminator(if c == '\0' it will always return false)
bool contains_char(cstr c, cstr* chars) {
	bool res = false;
	if (chars && *chars) { //TODO(fran): more compact
		do {
			if (c == *chars) {
				res = true;
				break;
			}
		} while (*++chars);
	}
	return res;
}

enum ETP {//EditOneline_tooltip_placement
	left = (1 << 1),
	top = (1 << 2),
	right = (1 << 3),
	bottom = (1 << 4),
	//current_char = (1 << 5), //instead of placement in relation to the control wnd it will be done relative to a character
};
//TODO(fran): positioning relative to a character offset, eg {3,5} 3rd char of 5th row place on top or left or...
void EDITONELINE_show_tip(HWND editoneline, const cstr* msg, int duration_ms, u32 ETP_flags) {
	EditOnelineProcState* state = EDITONELINE_get_state(editoneline); Assert(state);
	TOOLINFO toolInfo{ sizeof(toolInfo) };
	toolInfo.hwnd = state->wnd;
	toolInfo.uId = (UINT_PTR)state->wnd;
	toolInfo.hinst = LANGUAGE_MANAGER::Instance().GetHInstance();
	toolInfo.lpszText = const_cast<cstr*>(msg);
	SendMessage(state->controls.tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo);
	SendMessage(state->controls.tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&toolInfo);
	POINT tooltip_p{0,0};
	RECT tooltip_r;
	tooltip_r.left = 0;
	tooltip_r.right = 50;
	tooltip_r.top = 0;
	tooltip_r.bottom = EDITONELINE_calc_line_height_px(state);//The tooltip uses the same font, so we know the height of its text area
	SendMessage(state->controls.tooltip, TTM_ADJUSTRECT, TRUE, (LPARAM)&tooltip_r);//convert text area to tooltip full area

	RECT rc; GetClientRect(state->wnd, &rc);

	if (ETP_flags & ETP::left) {
		tooltip_p.x = 0;
	}

	if (ETP_flags & ETP::right) {
		tooltip_p.x = RECTWIDTH(rc);
	}

	if (ETP_flags & ETP::top) {
		tooltip_p.y = -RECTHEIGHT(tooltip_r);
	}

	if (ETP_flags & ETP::bottom) {
		tooltip_p.y = RECTHEIGHT(rc);
	}

	ClientToScreen(state->wnd, &tooltip_p);
	SendMessage(state->controls.tooltip, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(tooltip_p.x, tooltip_p.y));
	SetTimer(state->wnd, EDITONELINE_tooltip_timer_id, duration_ms, NULL);
}

void EDITONELINE_notify_parent(EditOnelineProcState* state, WORD notif_code) {
	PostMessage(state->parent, WM_COMMAND, MAKELONG(state->identifier, notif_code), (LPARAM)state->wnd);
}

//TODO(fran): some day paint/handle my own IME window
LRESULT CALLBACK EditOnelineProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	printf(msgToString(msg)); printf("\n");

	EditOnelineProcState* state = EDITONELINE_get_state(hwnd);
	switch (msg) {
		//TODOs(fran):
		//-on WM_STYLECHANGING check for password changes, that'd need a full recalculation
		//-on a WM_STYLECHANGING we should check if the alignment has changed and recalc/redraw every char, NOTE: I dont think windows' controls bother with this since it's not too common of a use case
		//-region selection after WM_LBUTTONDOWN, do tracking
		//-wm_gettetxlength
		//-copy,cut,... (I already have paste) https://docs.microsoft.com/en-us/windows/win32/dataxchg/using-the-clipboard?redirectedfrom=MSDN#_win32_Copying_Information_to_the_Clipboard
		//- https://docs.microsoft.com/en-us/windows/win32/intl/ime-window-class

	case WM_NCCREATE:
	{ //1st msg received
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		Assert(!(creation_nfo->style & ES_MULTILINE));
		Assert(!(creation_nfo->style & ES_RIGHT));//TODO(fran)

		EditOnelineProcState* st = (EditOnelineProcState*)calloc(1, sizeof(EditOnelineProcState));
		Assert(st);
		EDITONELINE_set_state(hwnd, st);
		st->wnd = hwnd;
		st->parent = creation_nfo->hwndParent;
		st->identifier = (u32)(UINT_PTR)creation_nfo->hMenu;
		st->char_max_sz = 32767;//default established by windows
		*st->default_text = 0;
		//NOTE: ES_LEFT==0, that was their way of defaulting to left
		if (creation_nfo->style & ES_CENTER) {
			//NOTE: ES_CENTER needs the pad to be recalculated all the time

			st->char_pad_x = abs(creation_nfo->cx / 2);//HACK
		}
		else {//we are either left or right
			st->char_pad_x = 3;
		}
		st->char_text = str();//REMEMBER: this c++ guys dont like being calloc-ed, they need their constructor, or, in this case, someone else's, otherwise they are badly initialized
		st->char_dims = std::vector<int>();
		return TRUE; //continue creation
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
	case WM_CREATE://3rd msg received
	{
		//Create our tooltip
		state->controls.tooltip = CreateWindowEx(WS_EX_TOPMOST/*make sure it can be seen in any wnd config*/, TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,//INFO(fran): TTS_BALLOON doesnt work on windows10, you need a registry change https://superuser.com/questions/1349414/app-wont-show-balloon-tip-notifications-in-windows-10 simply amazing
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			state->wnd, NULL, NULL, NULL);
		Assert(state->controls.tooltip);

		TOOLINFO toolInfo { sizeof(toolInfo) };
		toolInfo.hwnd = state->wnd;
		toolInfo.uFlags = TTF_IDISHWND | TTF_ABSOLUTE | TTF_TRACK /*allows to show the tooltip when we please*/; //TODO(fran): TTF_TRANSPARENT might be useful, mouse msgs that go to the tooltip are sent to the parent aka us
		toolInfo.uId = (UINT_PTR)state->wnd;
		toolInfo.rect = { 0 };
		toolInfo.hinst = LANGUAGE_MANAGER::Instance().GetHInstance(); //NOTE: this allows us to send the id of the string instead of having to allocate it ourselves
		toolInfo.lpszText = 0;
		toolInfo.lParam = 0;
		toolInfo.lpReserved = 0;
		BOOL addtool_res = SendMessage(state->controls.tooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
		Assert(addtool_res);

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SIZE: {//4th, strange, I though this was sent only if you didnt handle windowposchanging (or a similar one)
		//NOTE: neat, here you resize your render target, if I had one or cared to resize windows' https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-size
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOVE: //5th. Sent on startup after WM_SIZE, although possibly sent by DefWindowProc after I let it process WM_SIZE, not sure
	{
		//This msg is received _after_ the window was moved
		//Here you can obtain x and y of your window's client area
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SHOWWINDOW: //6th. On startup I received this cause of WS_VISIBLE flag
	{
		//Sent when window is about to be hidden or shown, doesnt let it clear if we are in charge of that or it's going to happen no matter what we do
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCPAINT://7th
	{
		//Paint non client area, we shouldnt have any
		HDC hdc = GetDCEx(hwnd, (HRGN)wparam, DCX_WINDOW | DCX_USESTYLE);
		ReleaseDC(hwnd, hdc);
		return 0; //we process this message, for now
	} break;
	case WM_ERASEBKGND://8th
	{
		//You receive this msg if you didnt specify hbrBackground  when you registered the class, now it's up to you to draw the background
		HDC dc = (HDC)wparam;
		//TODO(fran): look at https://docs.microsoft.com/en-us/windows/win32/gdi/drawing-a-custom-window-background and SetMapModek, allows for transforms

		return 0; //If you return 0 then on WM_PAINT fErase will be true, aka paint the background there
	} break;
	case EM_LIMITTEXT://Set text limit in _characters_ ; does NOT include the terminating null
		//wparam = unsigned int with max char count ; lparam=0
	{
		//TODO(fran): docs says this shouldnt affect text already inside the control nor text set via WM_SETTEXT, not sure I agree with that
		state->char_max_sz = (u32)wparam;
		return 0;
	} break;
	case WM_SETFONT:
	{
		//TODO(fran): should I make a copy of the font? it seems like windows just tells the user to delete the font only after they deleted the control so probably I dont need a copy
		HFONT font = (HFONT)wparam;
		state->font = font;
		BOOL redraw = LOWORD(lparam);
		if (redraw) RedrawWindow(state->wnd, NULL, NULL, RDW_INVALIDATE);
		return 0;
	} break;
	case WM_DESTROY:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCDESTROY://Last msg. Sent _after_ WM_DESTROY
	{
		//NOTE: the brushes are deleted by whoever created them
		if (state->caret.bmp) {
			DeleteBitmap(state->caret.bmp);
			state->caret.bmp = 0;
		}
		//TODO(fran): we need to manually free the vector and the string, im pretty sure they'll leak after the free()
		free(state);
		return 0;
	}break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rc; GetClientRect(state->wnd, &rc);
		//ps.rcPaint
		HDC dc = BeginPaint(state->wnd, &ps);
		int border_thickness = 1;
		HBRUSH bk_br, txt_br, border_br;
		bool show_default_text = *state->default_text && (GetFocus() != state->wnd) && (state->char_text.length()==0);
		if (IsWindowEnabled(state->wnd)) {
			bk_br = state->brushes.bk;
			txt_br = state->brushes.txt;
			border_br = state->brushes.border;
		}
		else {
			bk_br = state->brushes.bk_dis;
			txt_br = state->brushes.txt_dis;
			border_br = state->brushes.border_dis;
		}
		if (show_default_text) {
			txt_br = state->brushes.txt_dis;
		}
		FillRectBorder(dc, rc, border_thickness, border_br, BORDERLEFT | BORDERTOP | BORDERRIGHT | BORDERBOTTOM);

		{
			//Clip the drawing region to avoid overriding the border
			HRGN restoreRegion = CreateRectRgn(0, 0, 0, 0); if (GetClipRgn(dc, restoreRegion) != 1) { DeleteObject(restoreRegion); restoreRegion = NULL; } defer{ SelectClipRgn(dc, restoreRegion); if (restoreRegion != NULL) DeleteObject(restoreRegion); };
			{
				RECT left = rectNpxL(rc, border_thickness);
				RECT top = rectNpxT(rc, border_thickness);
				RECT right = rectNpxR(rc, border_thickness);
				RECT bottom = rectNpxB(rc, border_thickness);
				ExcludeClipRect(dc, left.left, left.top, left.right, left.bottom);
				ExcludeClipRect(dc, top.left, top.top, top.right, top.bottom);
				ExcludeClipRect(dc, right.left, right.top, right.right, right.bottom);
				ExcludeClipRect(dc, bottom.left, bottom.top, bottom.right, bottom.bottom);
				//TODO(fran): this aint too clever, it'd be easier to set the clipping region to the deflated rect
			}
			//Draw

			RECT bk_rc = rc; InflateRect(&bk_rc, -border_thickness, -border_thickness);
			FillRect(dc, &bk_rc, bk_br);//TODO(fran): we need to clip this where the text was already drawn, this will be the last thing we paint

			{
				HFONT oldfont = SelectFont(dc, state->font); defer{ SelectFont(dc, oldfont); };
				UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };

				COLORREF oldtxtcol = SetTextColor(dc, ColorFromBrush(txt_br)); defer{ SetTextColor(dc, oldtxtcol); };
				COLORREF oldbkcol = SetBkColor(dc, ColorFromBrush(bk_br)); defer{ SetBkColor(dc, oldbkcol); };

				TEXTMETRIC tm;
				GetTextMetrics(dc, &tm);
				// Calculate vertical position for the string so that it will be vertically centered
				// We are single line so we want vertical alignment always
				int yPos = (rc.bottom + rc.top - tm.tmHeight) / 2;
				int xPos;
				LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
				//ES_LEFT ES_CENTER ES_RIGHT
				//TODO(fran): store char positions in the vector
				if (style & ES_CENTER) {
					SetTextAlign(dc, TA_CENTER);
					xPos = (rc.right - rc.left) / 2;
				}
				else if (style & ES_RIGHT) {
					SetTextAlign(dc, TA_RIGHT);
					xPos = rc.right - state->char_pad_x;
				}
				else /*ES_LEFT*/ {//NOTE: ES_LEFT==0, that was their way of defaulting to left
					SetTextAlign(dc, TA_LEFT);
					xPos = rc.left + state->char_pad_x;
				}

				if (show_default_text) {
					TextOut(dc, xPos, yPos, state->default_text, cstr_len(state->default_text));
				}
				else if (style & ES_PASSWORD) {
					//TODO(fran): what's faster, full allocation or for loop drawing one by one
					cstr* pass_text = (cstr*)malloc(state->char_text.length() * sizeof(cstr));
					for (size_t i = 0; i < state->char_text.length(); i++)pass_text[i] = password_char;

					TextOut(dc, xPos, yPos, pass_text, (int)state->char_text.length());
				}
				else {
					TextOut(dc, xPos, yPos, state->char_text.c_str(), (int)state->char_text.length());
				}
			}
		}
		EndPaint(hwnd, &ps);
		return 0;
	} break;
	case WM_ENABLE:
	{//Here we get the new enabled state
		BOOL now_enabled = (BOOL)wparam;
		InvalidateRect(state->wnd, NULL, TRUE);
		//TODO(fran): Hide caret
		return 0;
	} break;
	case WM_CANCELMODE:
	{
		//Stop mouse capture, and similar input related things
		//Sent, for example, when the window gets disabled
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCHITTEST://When the mouse goes over us this is 1st msg received
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
	} break;
	case WM_SETCURSOR://When the mouse goes over us this is 2nd msg received
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
	case WM_MOUSEMOVE /*WM_MOUSEFIRST*/://When the mouse goes over us this is 3rd msg received
	{
		//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEACTIVATE://When the user clicks on us this is 1st msg received
	{
		HWND parent = (HWND)wparam;
		WORD hittest = LOWORD(lparam);
		WORD mouse_msg = HIWORD(lparam);
		SetFocus(state->wnd);//set keyboard focus to us
		return MA_ACTIVATE; //Activate our window and post the mouse msg
	} break;
	case WM_LBUTTONDOWN://When the user clicks on us this is 2nd msg received (possibly, maybe wm_setcursor again)
	{
		//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area
		//TODO(fran): unify with EDITONELINE_calc_caret_p
		POINT caret_p{ 0,EDITONELINE_calc_caret_p(state).y };
		if (state->char_text.empty()) {
			state->char_cur_sel = { 0,0 };
			caret_p.x = state->char_pad_x;//TODO(fran): who calcs the char_pad?
		}
		else {
			int mouse_x = mouse.x;
			int dim_x = state->char_pad_x;
			int char_cur_sel_x;
			if ((mouse_x - dim_x) <= 0) {
				caret_p.x = dim_x;
				char_cur_sel_x = 0;
			}
			else {
				int i = 0;
				for (; i < (int)state->char_dims.size(); i++) {
					dim_x += state->char_dims[i];
					if ((mouse_x - dim_x) < 0) {
						//if (i + 1 < (int)state->char_dims.size()) {//if there were more chars to go
						dim_x -= state->char_dims[i];
						//}
						break;
					}//TODO(fran): there's some bug here, selection change isnt as tight as it should, caret placement is correct but this overextends the size of the char
				}
				caret_p.x = dim_x;
				char_cur_sel_x = i;
			}
			state->char_cur_sel.x = char_cur_sel_x;
		}
		state->caret.pos = caret_p;
		SetCaretPos(state->caret.pos);
		InvalidateRect(state->wnd, NULL, TRUE);//TODO(fran): dont invalidate everything
		return 0;
	} break;
	case WM_LBUTTONUP:
	{
		//TODO(fran):Stop mouse tracking?
	} break;
	case WM_IME_SETCONTEXT://Sent after SetFocus to us
	{//TODO(fran): lots to discover here
		BOOL is_active = (BOOL)wparam;
		//lparam = display options for IME window
		u32 disp_flags = (u32)lparam;
		//NOTE: if we draw the composition window we have to modify some of the lparam values before calling defwindowproc or ImmIsUIMessage

		//NOTE: here you can hide the default IME window and all its candidate windows

		//If we have created an IME window, call ImmIsUIMessage. Otherwise pass this message to DefWindowProc
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}break;
	case WM_SETFOCUS://SetFocus -> WM_IME_SETCONTEXT -> WM_SETFOCUS
	{
		//We gained keyboard focus
		SIZE caret_dim = EDITONELINE_calc_caret_dim(state);
		if (caret_dim.cx != state->caret.dim.cx || caret_dim.cy != state->caret.dim.cy) {
			if (state->caret.bmp) {
				DeleteBitmap(state->caret.bmp);
				state->caret.bmp = 0;
			}
			//TODO(fran): we should probably check the bpp of our control's img
			int pitch = caret_dim.cx * 4/*bytes per pixel*/;//NOTE: windows expects word aligned bmps, 32bits are always word aligned
			int sz = caret_dim.cy * pitch;
			u32* mem = (u32*)malloc(sz); defer{ free(mem); };
			COLORREF color = ColorFromBrush(state->brushes.txt_dis);

			//IMPORTANT: changing caret color is gonna be a pain, docs: To display a solid caret (NULL in hBitmap), the system inverts every pixel in the rectangle; to display a gray caret ((HBITMAP)1 in hBitmap), the system inverts every other pixel; to display a bitmap caret, the system inverts only the white bits of the bitmap.
			//Aka we either calculate how to arrive at the color we want from the caret's bit flipping (will invert bits with 1) or we give up, we should do our own caret
			//NOTE: since the caret is really fucked up we'll average the pixel color so we at least get grays
			//TODO(fran): decide what to do, the other CreateCaret options work as bad or worse so we have to go with this, do we create a separate brush so the user can mix and match till they find a color that blends well with the bk?
			u8 gray = ((u16)(u8)(color >> 16) + (u16)(u8)(color >> 8) + (u16)(u8)color) / 3;
			color = RGB(gray, gray, gray);

			int px_count = sz / 4;
			for (int i = 0; i < px_count; i++) mem[i] = color;
			state->caret.bmp = CreateBitmap(caret_dim.cx, caret_dim.cy, 1, 32, (void*)mem);
			state->caret.dim = caret_dim;
		}
		BOOL caret_res = CreateCaret(state->wnd, state->caret.bmp, 0, 0);
		Assert(caret_res);
		state->caret.pos = EDITONELINE_calc_caret_p(state);
		SetCaretPos(state->caret.pos);
		BOOL showcaret_res = ShowCaret(state->wnd); //TODO(fran): the docs seem to imply you display the caret here but I dont wanna draw outside wm_paint
		Assert(showcaret_res);

		//Check in case we are showing the default text, when we get keyboard focus that text should disappear
		bool show_default_text = *state->default_text && (state->char_text.length() == 0);
		if (show_default_text) InvalidateRect(state->wnd, NULL, TRUE);

		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		//We lost keyboard focus
		//TODO(fran): docs say we should destroy the caret now
		DestroyCaret();
		//Also says to not display/activate a window here cause we can lock the thread
		bool show_default_text = *state->default_text && (state->char_text.length() == 0);
		if(show_default_text) InvalidateRect(state->wnd, NULL, TRUE);
		return 0;
	} break;
	case WM_KEYDOWN://When the user presses a non-system key this is the 1st msg we receive
	{
		//Notifications:
		bool en_change = false;

		//TODO(fran): here we process things like VK_HOME VK_NEXT VK_LEFT VK_RIGHT VK_DELETE
		//NOTE: there are some keys that dont get sent to WM_CHAR, we gotta handle them here, also there are others that get sent to both, TODO(fran): it'd be nice to have all the key things in only one place
		//NOTE: for things like _shortcuts_ you wanna handle them here cause on WM_CHAR things like Ctrl+V get translated to something else
		//		also you want to use the uppercase version of the letter, eg case _t('V'):
		char vk = (char)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		switch (vk) {
		case VK_HOME://Home
		{
			POINT oldcaretp = state->caret.pos;
			state->char_cur_sel.x = 0;
			state->caret.pos = EDITONELINE_calc_caret_p(state);
			if (oldcaretp != state->caret.pos) {
				SetCaretPos(state->caret.pos);
			}
		} break;
		case VK_END://End
		{
			POINT oldcaretp = state->caret.pos;
			state->char_cur_sel.x = (int)state->char_text.length();
			state->caret.pos = EDITONELINE_calc_caret_p(state);
			if (oldcaretp != state->caret.pos) {
				SetCaretPos(state->caret.pos);
			}
		} break;
		case VK_LEFT://Left arrow
		{
			if (state->char_cur_sel.x > 0) {
				state->char_cur_sel.x--;
				state->caret.pos = EDITONELINE_calc_caret_p(state);
				SetCaretPos(state->caret.pos);
			}
		} break;
		case VK_RIGHT://Right arrow
		{
			if (state->char_cur_sel.x < state->char_text.length()) {
				state->char_cur_sel.x++;
				state->caret.pos = EDITONELINE_calc_caret_p(state);
				SetCaretPos(state->caret.pos);
			}
		} break;
		case VK_DELETE://What in spanish is the "Supr" key (delete character ahead of you)
		{
			if (state->char_cur_sel.x < state->char_text.length()) {
				state->char_text.erase(state->char_cur_sel.x, 1);

				//NOTE: there's actually only one case I can think of where you need to update the caret, that is on a ES_CENTER control
				LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
				if (style & ES_CENTER) {
					//Recalc pad_x
					RECT rc; GetClientRect(state->wnd, &rc);
					state->char_pad_x = (RECTWIDTH(rc) - EDITONELINE_calc_text_dim(state).cx) / 2;

					state->caret.pos = EDITONELINE_calc_caret_p(state);
					SetCaretPos(state->caret.pos);
				}
				en_change = true;
			}
		} break;
		case _t('v'):
		case _t('V'):
		{
			if (ctrl_is_down) {
				PostMessage(state->wnd, WM_PASTE, 0, 0);
			}
		} break;
		}
		InvalidateRect(state->wnd, NULL, TRUE);//TODO(fran): dont invalidate everything, NOTE: also on each wm_paint the cursor will stop so we should add here a bool repaint; to avoid calling InvalidateRect when it isnt needed
		if (en_change) EDITONELINE_notify_parent(state, EN_CHANGE); //There was a change in the text
		return 0;
	}break;
	case WM_PASTE:
	{
		//Notifications:
		bool en_change = false;

		//TODO(fran): pasting onto the selected region
		//TODO(fran): if no unicode is available we could get the ansi and convert it, if it is available. //NOTE: docs say the format is converted automatically to the one you ask for
#ifdef UNICODE
		UINT format = CF_UNICODETEXT;
#else
		UINT format = CF_TEXT;
#endif
		if (IsClipboardFormatAvailable(format)) {//NOTE: lines end with \r\n, has null terminator
			if (OpenClipboard(state->wnd)) {
				defer{ CloseClipboard(); };
				HGLOBAL clipboard = GetClipboardData(format);
				if (clipboard) {
					cstr* clipboardtxt = (cstr*)GlobalLock(clipboard);
					if (clipboardtxt)
					{
						defer{ GlobalUnlock(clipboard); };
						bool paste_res = EDITONELINE_paste_from_clipboard(state, clipboardtxt); //TODO(fran): this should be separated into two fns, a general paste fn and first a sanitizer for anything strange that may be in the clipboard txt
						en_change = paste_res;
					}

				}
			}
		}
		if (en_change) EDITONELINE_notify_parent(state, EN_CHANGE); //There was a change in the text
	} break;
	case WM_CHAR://When the user presses a non-system key this is the 2nd msg we receive
	{//NOTE: a WM_KEYDOWN msg was translated by TranslateMessage() into WM_CHAR
		//Notifications:
		bool en_change = false;

		TCHAR c = (TCHAR)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		//lparam = flags
		switch (c) { //https://docs.microsoft.com/en-us/windows/win32/menurc/using-carets
		case VK_BACK://Backspace
		{
			if (!state->char_text.empty() && state->char_cur_sel.x > 0) {
				POINT oldcaretp = state->caret.pos;
				state->char_cur_sel.x--;
				Assert(state->char_cur_sel.x < (int)state->char_text.length() && state->char_cur_sel.x < (int)state->char_dims.size());
				state->char_text.erase(state->char_cur_sel.x, 1);
				state->char_dims.erase(state->char_dims.begin() + state->char_cur_sel.x);

				//TODO(fran): join this calculations into one function since some require others to already by re-calculated
				//Update pad if ES_CENTER
				LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
				if (style & ES_CENTER) {
					//Recalc pad_x
					RECT rc; GetClientRect(state->wnd, &rc);
					state->char_pad_x = (RECTWIDTH(rc) - EDITONELINE_calc_text_dim(state).cx) / 2;

				}

				state->caret.pos = EDITONELINE_calc_caret_p(state);
				if (oldcaretp != state->caret.pos) {
					SetCaretPos(state->caret.pos);
				}
				en_change = true;
			}
		}break;
		case VK_TAB://Tab
		{
			LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
			if (style & WS_TABSTOP) {//TODO(fran): I think we should specify a style that specifically says on tab pressed change to next control, since this style is just to say I want that to happen to me
				SetFocus(GetNextDlgTabItem(GetParent(state->wnd), state->wnd, FALSE));
			}
			//We dont handle tabs for now
		}break;
		case VK_RETURN://Received when the user presses the "enter" key //Carriage Return aka \r
		{
			//NOTE: I wonder, it doesnt seem to send us \r\n so is that something that is manually done by the control?
			//We dont handle "enter" for now
			//if(state->identifier) //TODO(fran): should I only send notifs if I have an identifier? what do the defaults do?
				PostMessage(GetParent(state->wnd), WM_COMMAND, MAKELONG(state->identifier, EN_ENTER), (LPARAM)state->wnd);
		}break;
		case VK_ESCAPE://Escape
		{
			//TODO(fran): should we do something?
			PostMessage(GetParent(state->wnd), WM_COMMAND, MAKELONG(state->identifier, EN_ESCAPE), (LPARAM)state->wnd);
		}break;
		case 0x0A://Linefeed, aka \n
		{
			//I havent found which key triggers this
			printf("WM_CHAR = linefeed\n");
		}break;
		default:
		{
			//TODO(fran): filter more values
			if (c <= (TCHAR)0x14) break;//control chars
			if (c <= (TCHAR)0x1f) break;//IME

			//TODO(fran): maybe this should be the first check for any WM_CHAR?
			if (contains_char(c,state->invalid_chars)) {
				//display tooltip //TODO(fran): make this into a reusable function
				std::wstring tooltip_msg = (RS(LANG_EDIT_USERNAME_INVALIDCHARS) + L" " + state->invalid_chars);
				EDITONELINE_show_tip(state->wnd, tooltip_msg.c_str(), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
				return 0;
			}

			//We have some normal character
			//TODO(fran): what happens with surrogate pairs? I dont even know what they are -> READ
			if (state->char_text.length() < state->char_max_sz) {
#if 0
				state->char_text += c;
				state->char_cur_sel.x++;//TODO: this is wrong
				state->char_dims.push_back(EDITONELINE_calc_char_dim(state, c).cx);//TODO(fran): also wrong, we'll need to insert in the middle if the cur_sel is not a the end
#else 
				state->char_text.insert((size_t)state->char_cur_sel.x,1,c);
				state->char_dims.insert(state->char_dims.begin() + state->char_cur_sel.x, EDITONELINE_calc_char_dim(state, c).cx);
				state->char_cur_sel.x++;
#endif
				LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
				if (style & ES_CENTER) {
					//Recalc pad_x
					RECT rc; GetClientRect(state->wnd, &rc);
					state->char_pad_x = (RECTWIDTH(rc) - EDITONELINE_calc_text_dim(state).cx) / 2;
				}
				state->caret.pos = EDITONELINE_calc_caret_p(state);
				SetCaretPos(state->caret.pos);

				wprintf(L"%s\n", state->char_text.c_str());
				en_change = true;
			}

		}break;
		}
		InvalidateRect(state->wnd, NULL, TRUE);//TODO(fran): dont invalidate everything
		if (en_change) EDITONELINE_notify_parent(state, EN_CHANGE); //There was a change in the text
		return 0;
	} break;
		case WM_TIMER:
		{
			WPARAM timerID = wparam;
			switch (timerID) {
			case EDITONELINE_tooltip_timer_id:
			{
				KillTimer(state->wnd, timerID);
				TOOLINFO toolInfo{ sizeof(toolInfo) };
				toolInfo.hwnd = state->wnd;
				toolInfo.uId = (UINT_PTR)state->wnd;
				SendMessage(state->controls.tooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&toolInfo);
				return 0;
			} break;
			default: return DefWindowProc(hwnd, msg, wparam, lparam);
			}
		} break;
	case WM_KEYUP:
	{
		//TODO(fran): smth to do here?
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETTEXT://the specified char count must include null terminator, since windows' defaults to force writing it to you
	{
		LRESULT res;
		int char_cnt_with_null = max((int)wparam, 0);//Includes null char
		int char_text_cnt_with_null = (int)(state->char_text.length() + 1);
		if (char_cnt_with_null > char_text_cnt_with_null) char_cnt_with_null = char_text_cnt_with_null;
		cstr* buf = (cstr*)lparam;
		if (buf) {//should I check?
			StrCpyN(buf, state->char_text.c_str(), char_cnt_with_null);
			if (char_cnt_with_null < char_text_cnt_with_null) buf[char_cnt_with_null - 1] = (cstr)0;
			res = char_cnt_with_null - 1;
		}
		else res = 0;
		return res;
	} break;
	case WM_GETTEXTLENGTH://does not include null terminator
	{
		return state->char_text.length();
	} break;
	case WM_SETTEXT:
	{
		//Notifications:
		bool en_change = false;

		BOOL res = FALSE;
		cstr* buf = (cstr*)lparam;//null terminated
		if (buf) {
			size_t char_sz = cstr_len(buf);//not including null terminator
			if (char_sz <= (size_t)state->char_max_sz) {
				state->char_text = buf;

				for (size_t i = 0; i < char_sz; i++) state->char_dims.insert(state->char_dims.begin() + state->char_cur_sel.x + i, EDITONELINE_calc_char_dim(state, buf[i]).cx);
				state->char_cur_sel.x = (int)char_sz;

				LONG_PTR  style = GetWindowLongPtr(state->wnd, GWL_STYLE);
				if (style & ES_CENTER) {
					//Recalc pad_x
					RECT rc; GetClientRect(state->wnd, &rc);
					state->char_pad_x = (RECTWIDTH(rc) - EDITONELINE_calc_text_dim(state).cx) / 2;

				}
				state->caret.pos = EDITONELINE_calc_caret_p(state);
				SetCaretPos(state->caret.pos);

				res = TRUE;
				InvalidateRect(state->wnd, NULL, TRUE);
			}
		}
		en_change = res;
		if (en_change) EDITONELINE_notify_parent(state, EN_CHANGE); //There was a change in the text
		return res;
	}break;
	case WM_SYSKEYDOWN://1st msg received after the user presses F10 or Alt+some key
	{
		//TODO(fran): notify the parent?
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SYSKEYUP:
	{
		//TODO(fran): notify the parent?
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_NOTIFY:
	{
		//Notifies about changes to the IME window
		//TODO(fran): process this msgs once we manage the ime window
		u32 command = (u32)wparam;

		//IMN_SETCOMPOSITIONWINDOW IMN_SETSTATUSWINDOWPOS
		//printf("WM_IME_NOTIFY: wparam = 0x%08x\n", command);

		//lparam = command specific data
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_REQUEST://After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive this msg
	{
		//printf("WM_IME_REQUEST: wparam = 0x%08x\n", wparam);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_INPUTLANGCHANGE://After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) and WM_IME_REQUEST we receive this msg
	{
		//wparam = charset of the new locale
		//lparam = input locale identifier... wtf
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_STARTCOMPOSITION://On japanese keyboard, after we press a key to start writing this is 1st msg received
	{
		//doc:Sent immediately before the IME generates the composition string as a result of a keystroke
		//This is a notification to an IME window to open its composition window. An application should process this message if it displays composition characters itself.
		//If an application has created an IME window, it should pass this message to that window.The DefWindowProc function processes the message by passing it to the default IME window.
		EDITONELINE_set_composition_pos(state);
		EDITONELINE_set_composition_font(state);//TODO(fran): should I place this somewhere else?

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_COMPOSITION://On japanese keyboard, after we press a key to start writing this is 2nd msg received
	{//doc: sent when IME changes composition status cause of keystroke
		//wparam = DBCS char for latest change to composition string, TODO(fran): find out about DBCS
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_CHAR://WM_CHAR from the IME window, this are generated once the user has pressed enter on the IME window, so more than one char will probably be coming
	{
		//UNICODE: wparam=utf16 char ; ANSI: DBCS char
		//lparam= the same flags as WM_CHAR
#ifndef UNICODE
		Assert(0);//TODO(fran): DBCS
#endif 
		PostMessage(state->wnd, WM_CHAR, wparam, lparam);

		return 0;//docs dont mention return value so I guess it dont matter
	} break;
	case WM_IME_ENDCOMPOSITION://After the chars are sent from the IME window it hides/destroys itself (idk)
	{
		//TODO: Handle once we process our own IME
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_WINDOWPOSCHANGING:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_WINDOWPOSCHANGED:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	//case WM_IME_CONTROL: //NOTE: I feel like this should be received by the wndproc of the IME, I dont think I can get DefWndProc to send it there for me
	//{
	//	//Used to manually send some msg to the IME window, in my case I use it so DefWindowProc sends my msg to it IME default wnd
	//	return DefWindowProc(hwnd, msg, wparam, lparam);
	//}break;
	case WM_SETDEFAULTTEXT:
	{
		cstr* text = (cstr*)lparam;
		
		memcpy_s(state->default_text, sizeof(state->default_text), text, (cstr_len(text) + 1) * sizeof(*text));

		return 0;
	} break;
	case EM_SETINVALIDCHARS:
	{
		cstr* chars = (cstr*)lparam;
		memcpy_s(state->invalid_chars, sizeof(state->invalid_chars), chars, (cstr_len(chars) + 1) * sizeof(*chars));
	} break;
	case WM_NOTIFYFORMAT://1st msg sent by our tooltip
	{
		switch (lparam) {
		case NF_QUERY: return sizeof(cstr) > 1 ? NFR_UNICODE : NFR_ANSI; //a child of ours has asked us
		case NF_REQUERY: return SendMessage((HWND)wparam/*parent*/, WM_NOTIFYFORMAT, (WPARAM)state->wnd, NF_QUERY); //NOTE: the return value is the new notify format, right now we dont do notifications so we dont care, but this could be a TODO(fran)
		}
		return 0;
	} break;
	case WM_QUERYUISTATE://Strange msg, it wants to know the UI state for a window, I assume that means the tooltip is asking us how IT should look?
	{
		return UISF_ACTIVE | UISF_HIDEACCEL | UISF_HIDEFOCUS; //render as active wnd, dont show keyboard accelerators nor focus indicators
	} break;
	case WM_NOTIFY:
	{
		NMHDR* msg_info = (NMHDR*)lparam;
		if (msg_info->hwndFrom == state->controls.tooltip) {
			switch (msg_info->code) {
			case NM_CUSTOMDRAW:
			{
				NMTTCUSTOMDRAW* cd = (NMTTCUSTOMDRAW*)lparam;
				//INFO: cd->uDrawFlags = flags for DrawText
				switch (cd->nmcd.dwDrawStage) {
					//TODO(fran): probably case CDDS_PREERASE: for SetBkColor for the background
				case CDDS_PREPAINT: 
				{
					return CDRF_NOTIFYITEMDRAW;//TODO(fran): I think im lacking something here, we never get CDDS_ITEMPREPAINT, it's possible the msgs are not sent cause it uses a visual style, in which case it doesnt care about you, we would have to remove it with setwindowtheme, and since there wont be any style we'll have to draw it completely ourselves I guess
				} break;
				case CDDS_ITEMPREPAINT://Setup before painting an item
				{
					SelectFont(cd->nmcd.hdc, state->font);
					SetTextColor(cd->nmcd.hdc, ColorFromBrush(state->brushes.txt));
					SetBkColor(cd->nmcd.hdc, ColorFromBrush(state->brushes.bk));
					return CDRF_NEWFONT;
				} break;
				default: return CDRF_DODEFAULT;
				}
			} break;
			case TTN_GETDISPINFO:
				Assert(0);
			case TTN_LINKCLICK:
				Assert(0);
			case TTN_POP://Tooltip is about to be hidden
				return 0;
			case TTN_SHOW://Tooltip is about to be shown
				return 0;//here we can change the tooltip's position
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETFONT:
	{
		return (LRESULT)state->font;
	} break;
	default:
	{

		if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
			TCHAR arr[256];
			int res = GetClipboardFormatName(msg, arr, 256);
			cstr_printf(arr); printf("\n");
			//After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive "MSIMEQueryPosition"
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

#ifdef _DEBUG
		Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	} break;
	}
	return 0;
}
//TODO(fran): the rest of the controls should also SetFocus to themselves