#pragma once
#include <windows.h>
#include "unCap_Platform.h"
#include "unCap_Helpers.h"
#include <Richedit.h> //EM_FINDTEXT
#include <commdlg.h> //FR_DOWN,...
#include "windows_undoc.h"

constexpr TCHAR protect_wndclass_search[] = TEXT("unCap_wndclass_search");

void init_wndclass_protect_search(HINSTANCE instance); //call this before creating an HWND of this class

//TODO(fran): on WM_SETFONT I should update all my controls
//TODO(fran): on showing the wnd we must setfocus to the edit control

//Flags for CreateWindow
#define SEARCH_EDIT (1<<1)		//Search an edit control
#define SEARCH_RICHEDIT (1<<2)	//Search a rich edit control

//New msgs that this wnd can handle
#define search_base_msg_addr (WM_USER+300)
#define SRH_AUTORESIZE (search_base_msg_addr+1)

#ifdef _UNICODE
#define _EM_FINDTEXT EM_FINDTEXTW
#define _EM_FINDTEXTEX EM_FINDTEXTEXW
#else
#define _EM_FINDTEXT EM_FINDTEXT
#define _EM_FINDTEXTEX EM_FINDTEXTEX
#endif

//INFO IMPORTANT: this is a great way to replace the stupid enums
struct SearchPlacement
{
	static const int left = 1 << 1;
	static const int right = 1 << 2;
	static const int top = 1 << 3;
	static const int bottom = 1 << 4;

	//NOTE: top and bottom create a wnd that extends the whole width of the parent control; left and right will occupy 25% of the width or less; they can be combined eg bottom | left
};
struct SearchFlag { //TODO(fran): you might like for this flags to be stored for the next execution, we need a simpler way to allow anyone to save stuff, not just the main wnds
	static const int search_up		= 1 << 1;	//if set then search up, otherwise search down
	static const int case_sensitive	= 1 << 2;	//
	static const int whole_word		= 1 << 3;	//
	static const int no_wrap		= 1 << 4;	//if you didnt find searching up look at what was down and viceversa, this flag is negated cause the usual is you want wrap
	//TODO(fran): search in current selection
};
struct SearchRange {
	int min; //first position to search, eg to search from the start this would be 0
	int max; //last position to search, that position is included in the search, use -1 to search till the end
};
struct SearchProcState { //NOTE: must be initialized to zero
	int  placement_flags;
	HWND wnd;
	HWND parent;
	u32 parent_type; // SEARCH_EDIT, SEARCH_RICHEDIT, ...
	int search_flags; //uses SearchFlag

	union SearchControls {
		struct {
			HWND btn_case_sensitive;
			HWND btn_whole_word;
			HWND btn_wrap;
			HWND edit_match;
			HWND btn_find_next;
			HWND btn_find_prev;
			HWND btn_close;
		};
		HWND all[7];//REMEMBER TO UPDATE
	}controls;

	HBRUSH br_border, br_bk, br_fore, br_bkpush, br_bkmouseover, br_edit_bk, br_edit_txt, br_bkselected;//TODO(fran): no real need to store all, only the childs need most of them
};

struct SearchInit{ //Sent as a pointer in the last parameter of CreateWindow
	int SearchPlacement_flags;
	int SearchFlag_flags;
	u32 parent_type; // SEARCH_EDIT, SEARCH_RICHEDIT, ...
};

struct SEARCH_search_result {
	int p;
	int sz_char;
};
//TODO(fran): extra param, search from sel_start or sel_end, when the user presses the enter key or the find button they want to start from sel_end, while they are writing they want to start from sel_start
//INFO: from_sel_start: taking by reference point a search downwards if true the search happens from the start of the current selection, otherwise from the end, the same desired behaviour will happen for searching up where from_sel_start true will mean to search from the end, yes confusing, just take into account how it works for searching downwards and it will automatically work perfectly for searching up
SEARCH_search_result SEARCH_search(SearchProcState* state, bool search_in_current_selection) { //returns zero-based index of the first char of the match or -1 if not found
	//TODO(fran): searching should roll over, say we get to the end of the file and dont find anything then we go from the start, and only then can we say to the user that we didnt find anything
	SEARCH_search_result res;

	int match_len = SendMessageW(state->controls.edit_match, WM_GETTEXTLENGTH, 0, 0) + 1;//lenght in characters, includes null terminator
	res.sz_char = match_len-1;
	if (match_len > 1) {
		WCHAR* match = (WCHAR*)malloc(match_len * sizeof(*match)); defer{ free(match); };
		SendMessage(state->controls.edit_match, WM_GETTEXT, match_len, (LPARAM)match);

		switch (state->parent_type) {
		case SEARCH_RICHEDIT: //TODO(fran): FR_DOWN was added in rich edit 2.0 !!!!!! thanks microsoft, we gotta special case that too
		{
			/*int search_flags = (state->search_flags & SearchFlag::search_up ? 0 : FR_DOWN) | (state->search_flags & SearchFlag::whole_word ? 0 : FR_WHOLEWORD);
			FINDTEXT find;
			find.chrg.cpMin = range.min;
			find.chrg.cpMax = range.max + 1;
			find.lpstrText = match;
			res = SendMessage(state->parent, _EM_FINDTEXT, search_flags, (LPARAM)&find);*/
			Assert(0);
		} break;
		case SEARCH_EDIT:
		{
			HLOCAL loc = (HLOCAL)SendMessage(state->parent, EM_GETHANDLE, 0, 0);
			WCHAR* str = (WCHAR*)LocalLock(loc); defer{ LocalUnlock(loc); }; //INFO: with commctrl version 6 the text is always stored as WCHAR
			int str_len = SendMessage(state->parent, WM_GETTEXTLENGTH, 0, 0)+1;//in characters, includes null terminator
			DWORD sel_start, sel_end;
			SendMessage(state->parent, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);//INFO: even if the selection was made backwards, say the user clicked and dragged to the left, this msg always returns the right facing selection with the lower number on sel_start //TODO(fran): what happens in smth like arabic?
			DWORD sel;

			SearchRange range;
			DWORD search_flags= LINGUISTIC_IGNOREDIACRITIC;
			if (!(state->search_flags & SearchFlag::case_sensitive))
				search_flags |= LINGUISTIC_IGNORECASE; //TODO(fran): study the flags, there are some more complex things that I may want to add
			if (state->search_flags & SearchFlag::search_up) {//search from the beginning of the selection to the first char
				sel = (search_in_current_selection) ? min(sel_end+2, str_len/*-1 ?*/) : sel_start;
				//INFO IMPORTANT: searching up is tricky, each time we search we establish a selection and the next time we search up from it, problem with this is that the user is actually looking for something that will potentially be on the right of that selection ("downwards"), therefore we need to extend the selection +1 to the right to avoid the continually scrolling problem, also +1 is probably not enough since you could have those additive codepoints that take more than one wchar
				range.min = 0;
				range.max = sel;// should subtract 1
				search_flags |= FIND_FROMEND ;
				//TODO(fran): we im going up should I set the start of the string to the end or does FindNLSStringEx do that ?
			}
			else {//search from the end of the selection to the last char
				sel = (search_in_current_selection) ? sel_start : sel_end;
				range.min = sel;
				range.max = str_len-1;
				search_flags |= FIND_FROMSTART;
			}
			int __found_len;
			res.p = FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, search_flags, str + range.min, distance(range.min, range.max),match, match_len-1,&__found_len, NULL, NULL,0);//returns -1 if not found, otherwise zero-based idx

			if (res.p != -1) res.p += range.min; //since we offset the string when searching now we gotta reintegrate that offset

			//StrStrIW(str, match);//TODO(fran): look at https://www.codeproject.com/Articles/383185/SSE-accelerated-case-insensitive-substring-search which says to be much faster than this
			//wcsstr(, )//Case sensitive comparison, exact match

			//TODO(fran): I think this should be a flag too, wordwrap or smth like that
			if (res.p == -1 && !(state->search_flags & SearchFlag::no_wrap)) {//we didnt find anything in our range, lets test the leftover range but keeping our direction
				if (state->search_flags & SearchFlag::search_up) {
					range.min = sel;
					range.max = str_len - 1;
				}
				else {
					range.min = 0;
					range.max = sel;// should subtract 1
				}
				res.p = FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, search_flags, str + range.min, distance(range.min, range.max), match, match_len - 1, &__found_len, NULL, NULL, 0);

				if (res.p != -1) res.p += range.min;
			}


		} break;
		default: Assert(0); break;
		}
	}
	else res.p = -1;
	return res;
}
void SEARCH_make_selection(SearchProcState* state, int p /*zero-based*/, int char_count) {
	switch (state->parent_type) {
	case SEARCH_RICHEDIT:
	{
		CHARRANGE range;
		range.cpMin = p;
		range.cpMax = p+char_count;//+1 ?
		SendMessage(state->parent, EM_EXSETSEL, 0, (LPARAM)&range);
	} break;
	case SEARCH_EDIT:
	{
		SendMessage(state->parent, EM_SETSEL, p, p + char_count);
	} break;
	default: Assert(0); break;
	}
}
void SEARCH_scroll_to_selection(SearchProcState* state) {//Scrolls the text to the current selection
	//TODO(fran): this scrolls to the specific line, therefore the selection can end up on the first or last visible lines, we would prefer the selection gets a little more centered to the middle of the control
	switch (state->parent_type) {
	case SEARCH_RICHEDIT:
	{
		SendMessage(state->parent, EM_SCROLLCARET, 0, 0);
	} break;
	case SEARCH_EDIT:
	{
		SendMessage(state->parent, EM_SCROLLCARET, 0, 0);

		//The current selection should be on a visible line, now we move that line a little closer to the center if it is too close to the top or bottom
		int first_visible_line = SendMessage(state->parent, EM_GETFIRSTVISIBLELINE, 0, 0);
		//EM_GETLASTVISIBLELINE
		RECT r; SendMessage(state->parent, EM_GETRECT, 0, (LPARAM)&r);//TODO(fran): make sure this is actually useful when the rect is modified instead of just using getclientrect
		DWORD c = SendMessage(state->parent, EM_CHARFROMPOS, 0, MAKELONG(0,RECTHEIGHT(r)-1));
		int last_visible_line = HIWORD(c);
		int max_visible_lines = distance(first_visible_line,last_visible_line);
		int current_line = SendMessage(state->parent, EM_LINEFROMCHAR, -1 /*get the line of the current selection*/, 0);
		int dist_to_first_line = distance(current_line, first_visible_line);
		int dist_to_last_line = distance(current_line, last_visible_line);
		if (max_visible_lines > 3 ) {
			if (dist_to_first_line < 3) {//we are too close to the top, we need to go down
				int line_scroll = min(2, dist_to_last_line);
				line_scroll = -line_scroll; //strangely enough to scroll down you use negative numbers, and positive for up
				SendMessage(state->parent, EM_LINESCROLL, 0, line_scroll);
			}
			else if (dist_to_last_line < 3) {//we are too close to the bottom, we need to go up
				int line_scroll = min(2, dist_to_first_line);
				SendMessage(state->parent, EM_LINESCROLL, 0, line_scroll);
			}
		}



	} break;
	default: Assert(0); break;
	}
}

void SEARCH_search_and_scroll(SearchProcState* state, bool search_in_current_selection) {
	auto search_res = SEARCH_search(state, search_in_current_selection);
	if (search_res.p != -1) { //TODO(fran): I like visual studio's way to tell the user if they couldnt match, they put a red border around the edit wnd, very simple to implement just change the border brush
		SEARCH_make_selection(state, search_res.p, search_res.sz_char);
		SEARCH_scroll_to_selection(state);
	}
}

SearchProcState* SEARCH_get_state(HWND search) {
	SearchProcState* state = (SearchProcState*)GetWindowLongPtr(search, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

void SEARCH_set_state(HWND search, SearchProcState* state) {
	SetWindowLongPtr(search, 0, (LONG_PTR)state);
}


//TODO(fran): when the parent gets resized it should tell us to resize
void SEARCH_resize_wnd(SearchProcState* state) {
	Assert(state->placement_flags);

	RECT r; GetWindowRect(state->parent, &r); //INFO: you cant use getclientrect with text editors since they seem to change their client rect when you scroll them and would cause the position of this control to start scrolling too
	int w = RECTWIDTH(r);
	int h = RECTHEIGHT(r);
	int font_h;
	{
		HFONT font = (HFONT)SendMessage(state->controls.edit_match, WM_GETFONT, 0, 0);
		HDC dc = GetDC(state->wnd); defer{ ReleaseDC(state->wnd,dc); };
		HFONT oldfont = (HFONT)SelectObject(dc, (HGDIOBJ)font); defer{ SelectObject(dc, (HGDIOBJ)oldfont); };
		TEXTMETRIC tm; GetTextMetrics(dc, &tm);
		font_h = (int)((float)tm.tmHeight * 1.2f); //TODO(fran): what we'd actually need is for the edit control to tell us the caret size, and make our height a little bigger
	}
	int search_x;
	int search_y;
	int search_w;
	int search_h;
	if (state->placement_flags & SearchPlacement::left || state->placement_flags & SearchPlacement::right) {
		//small wnd
		search_h = font_h * 2;
		//search_w = (int)((float)w * .25f);//TODO(fran): there should be a max size, we just want a little thing in the corner
		HDC dc = GetDC(state->wnd);
		SIZE a; GetTextExtentPoint32(dc, L"a", 1, &a);
		search_w = a.cx * 33;
	}
	else {
		//big wnd
		search_h = font_h;
		search_w = w;
		search_x = 0;
	}

	if (state->placement_flags & SearchPlacement::left) {
		search_x = 0;
		search_y = 0;
	}
	if (state->placement_flags & SearchPlacement::right) {
		search_x = w - search_w;
		search_y = 0;
	}
	if (state->placement_flags & SearchPlacement::top) {
		search_y = 0;
	}
	if (state->placement_flags & SearchPlacement::bottom) {
		search_y = h - search_h;
	}

	RECT non_clipped_rc = rectWH(search_x, search_y, search_w, search_h);
	RECT clipped_rc = clip_fit_childs(state->parent, state->wnd, non_clipped_rc);

	MoveWindow(state->wnd, clipped_rc.left, clipped_rc.top, RECTWIDTH(clipped_rc), RECTHEIGHT(clipped_rc), TRUE);
}

void SEARCH_resize_controls(SearchProcState* state) {
	RECT r; GetClientRect(state->wnd, &r);
	int w = RECTWIDTH(r);
	int h = RECTHEIGHT(r);
	int w_pad = 2;
	int h_pad = 2;

	int btn_case_sensitive_x;
	int btn_case_sensitive_y;
	int btn_case_sensitive_w;
	int btn_case_sensitive_h;

	int btn_whole_word_x;
	int btn_whole_word_y;
	int btn_whole_word_w;
	int btn_whole_word_h;

	int btn_wrap_x;
	int btn_wrap_y;
	int btn_wrap_w;
	int btn_wrap_h;

	int btn_close_h;
	int btn_close_w;
	int btn_close_x;
	int btn_close_y;

	int btn_find_next_w;
	int btn_find_next_h; 
	int btn_find_next_x; 
	int btn_find_next_y; 

	int btn_find_prev_w;
	int btn_find_prev_h;
	int btn_find_prev_x;
	int btn_find_prev_y;

	int edit_match_x;
	int edit_match_y;
	int edit_match_w;
	int edit_match_h;
	
	if (state->placement_flags & SearchPlacement::left || state->placement_flags & SearchPlacement::right) { //control is in small state
		int h_half = h / 2;

		//TODO(fran): this UI isnt great, we should do like visual studio and have to levels, on top the edit control and on the bottom all the flag button and find

		//"case sensitive", "whole word", "wrap" go on the left, one on top of the other
		btn_case_sensitive_x = 0;
		btn_case_sensitive_y = 0;
		btn_case_sensitive_w = h_half;
		btn_case_sensitive_h = h_half;

		btn_whole_word_x = btn_case_sensitive_x;
		btn_whole_word_y = btn_case_sensitive_y + btn_case_sensitive_h;
		btn_whole_word_w = btn_case_sensitive_w;
		btn_whole_word_h = btn_case_sensitive_h;

		btn_wrap_x = 0;
		btn_wrap_y = 0;
		btn_wrap_w = 10;
		btn_wrap_h = 10;

		//"close" goes top right
		btn_close_h = btn_case_sensitive_h / 2;
		btn_close_w = btn_close_h;
		btn_close_x = w - btn_close_w;
		btn_close_y = btn_case_sensitive_y;

		//"find next" and "find prev" go left of "close", one on top of the other
		btn_find_next_w = btn_case_sensitive_w;
		btn_find_next_h = btn_case_sensitive_h;
		btn_find_next_x = btn_close_x - btn_find_next_w - w_pad;
		btn_find_next_y = btn_case_sensitive_y;

		btn_find_prev_w = btn_find_next_w;
		btn_find_prev_h = btn_find_next_h;
		btn_find_prev_x = btn_find_next_x;
		btn_find_prev_y = btn_find_next_y + btn_find_next_h;

		//"match" occupies the rest of the space in the middle
		edit_match_x = btn_case_sensitive_x+ btn_case_sensitive_w;
		edit_match_y = btn_case_sensitive_y;
		edit_match_w = btn_find_next_x - edit_match_x;
		edit_match_h = h;
	}
	else {//wnd is in large state, here everything goes next to each other on one line
		int h_sub_pad = h - h_pad * 2;

		//TODO(fran): here I could add a little w padding in between each control

		//"case sensitive" goes first on the left
		btn_case_sensitive_x = w_pad;
		btn_case_sensitive_y = h_pad;
		btn_case_sensitive_h = h_sub_pad;
		btn_case_sensitive_w = btn_case_sensitive_h;
		//then "whole word"
		btn_whole_word_x = btn_case_sensitive_x + btn_case_sensitive_w + w_pad;
		btn_whole_word_y = btn_case_sensitive_y;
		btn_whole_word_w = btn_case_sensitive_w;
		btn_whole_word_h = btn_whole_word_w;

		btn_wrap_x = btn_whole_word_x + btn_whole_word_w + w_pad;
		btn_wrap_y = btn_case_sensitive_y;
		btn_wrap_w = btn_case_sensitive_w;
		btn_wrap_h = btn_wrap_w;

		//"close" goes last on the right
		btn_close_h = btn_case_sensitive_h / 2;
		btn_close_w = btn_close_h;
		btn_close_x = w - btn_close_w - w_pad;
		btn_close_y = btn_case_sensitive_y;

		//"find prev" goes left of "close"
		btn_find_prev_h = btn_case_sensitive_h;
		SIZE _btn_find_prev_sz{ 0 }; Button_GetIdealSize(state->controls.btn_find_prev, &_btn_find_prev_sz);
		btn_find_prev_w = _btn_find_prev_sz.cx;
		btn_find_prev_x = btn_close_x - btn_find_prev_w - w_pad;
		btn_find_prev_y = btn_case_sensitive_y;

		//"find next" goes left of "find prev"
		btn_find_next_h = btn_find_prev_h;
		SIZE _btn_find_next_sz{ 0 }; Button_GetIdealSize(state->controls.btn_find_next, &_btn_find_next_sz);
		btn_find_next_w = _btn_find_next_sz.cx;
		btn_find_next_x = btn_find_prev_x - btn_find_next_w - w_pad;
		btn_find_next_y = btn_case_sensitive_y;

		//"match" occupies the rest of the space in the middle
		edit_match_x = btn_wrap_x + btn_wrap_w + w_pad;
		edit_match_y = btn_case_sensitive_y;
		edit_match_w = btn_find_next_x - edit_match_x - w_pad;
		edit_match_h = btn_case_sensitive_h;
	}
	MoveWindow(state->controls.btn_case_sensitive, btn_case_sensitive_x, btn_case_sensitive_y, btn_case_sensitive_w, btn_case_sensitive_h, TRUE);
	MoveWindow(state->controls.btn_whole_word, btn_whole_word_x, btn_whole_word_y, btn_whole_word_w, btn_whole_word_h, TRUE);
	MoveWindow(state->controls.btn_wrap, btn_wrap_x, btn_wrap_y, btn_wrap_w, btn_wrap_h, TRUE);
	MoveWindow(state->controls.btn_close, btn_close_x, btn_close_y, btn_close_w, btn_close_h, TRUE);
	MoveWindow(state->controls.btn_find_prev, btn_find_prev_x, btn_find_prev_y, btn_find_prev_w, btn_find_prev_h, TRUE);
	MoveWindow(state->controls.btn_find_next, btn_find_next_x, btn_find_next_y, btn_find_next_w, btn_find_next_h, TRUE);
	MoveWindow(state->controls.edit_match, edit_match_x, edit_match_y, edit_match_w, edit_match_h, TRUE);
}

void SEARCH_add_controls(SearchProcState* state) {
	//TODO(fran): which controls to show should be specific to the type of editor available, that's where we need a resizer so we dont have to manually position all this guys and we can replace them, add, remove, easily

	//INFO: instead of using control identifiers as the HMENU param im simply gonna filter them with the HWND, for example on WM_COMMAND
	state->controls.btn_case_sensitive = CreateWindow(unCap_wndclass_button, L"Aa"/*works on any language, maybe*/, WS_VISIBLE | WS_CHILD | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);

	state->controls.btn_whole_word = CreateWindow(unCap_wndclass_button, L"' '"/*impossible to describe*/, WS_VISIBLE | WS_CHILD | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);

	state->controls.btn_wrap = CreateWindow(unCap_wndclass_button, L"↺", WS_VISIBLE | WS_CHILD | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);
	
	//TODO(fran): this should be the new button, rendering text when possible or an img otherwise
	state->controls.btn_find_prev = CreateWindow(unCap_wndclass_button, NULL, WS_VISIBLE | WS_CHILD | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);
	AWT(state->controls.btn_find_prev, LANG_SEARCH_FINDPREV);

	//TODO(fran): this should be the new button, rendering text when possible or an img otherwise
	state->controls.btn_find_next = CreateWindow(unCap_wndclass_button, NULL, WS_VISIBLE | WS_CHILD | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);
	AWT(state->controls.btn_find_next, LANG_CONTROL_SEARCH_FINDNEXT);
	state->controls.btn_close = CreateWindow(unCap_wndclass_button, NULL, WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_BITMAP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);
	HBITMAP bCross = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_CLOSE));//TODO(fran): I dont think the button takes care of freeing this
	SendMessage(state->controls.btn_close, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bCross);

	state->controls.edit_match = CreateWindow(unCap_wndclass_edit_oneline, _t(""), WS_VISIBLE | WS_CHILD | ES_LEFT | WS_TABSTOP
		, 0, 0, 0, 0, state->wnd, NULL, NULL, NULL);
	//SendMessage(state->controls.edit_match, WM_SETDEFAULTTEXT, 0, (LPARAM)RCS(LANG_SEARCH_FIND));

	for (auto ctl : state->controls.all)
		SendMessage(ctl, WM_SETFONT, (WPARAM)unCap_fonts.General, TRUE);

	SEARCH_resize_wnd(state);
}

void SEARCH_set_brushes(HWND search, BOOL repaint, HBRUSH border_br, HBRUSH bk_br, HBRUSH fore_br, HBRUSH bkpush_br, HBRUSH bkmouseover_br, HBRUSH edit_bk_br, HBRUSH edit_txt_br, HBRUSH bkselected_br) {
	SearchProcState* state = SEARCH_get_state(search);
	if (border_br)state->br_border = border_br;
	if (bk_br)state->br_bk = bk_br;
	if (fore_br)state->br_fore = fore_br;
	if (bkpush_br)state->br_bkpush = bkpush_br;
	if (bkmouseover_br)state->br_bkmouseover = bkmouseover_br;
	if (edit_bk_br)state->br_edit_bk = edit_bk_br;
	if (edit_txt_br)state->br_edit_txt = edit_txt_br;
	if (bkselected_br)state->br_bkselected = bkselected_br;
	if (repaint)InvalidateRect(state->wnd, NULL, TRUE);

	HBRUSH bk;
	bk = (state->search_flags & SearchFlag::case_sensitive) ? state->br_bkselected : state->br_bk;
	UNCAPBTN_set_brushes(state->controls.btn_case_sensitive, TRUE, state->br_border, bk, state->br_fore, state->br_bkpush, state->br_bkmouseover);

	bk = (state->search_flags & SearchFlag::whole_word) ? state->br_bkselected : state->br_bk;
	UNCAPBTN_set_brushes(state->controls.btn_whole_word, TRUE, state->br_border, bk, state->br_fore, state->br_bkpush, state->br_bkmouseover);

	bk = !(state->search_flags & SearchFlag::no_wrap) ? state->br_bkselected : state->br_bk;
	UNCAPBTN_set_brushes(state->controls.btn_wrap, TRUE, state->br_border, bk, state->br_fore, state->br_bkpush, state->br_bkmouseover);

	UNCAPBTN_set_brushes(state->controls.btn_find_prev, TRUE, state->br_border, state->br_bk, state->br_fore, state->br_bkpush, state->br_bkmouseover);

	UNCAPBTN_set_brushes(state->controls.btn_find_next, TRUE, state->br_border, state->br_bk, state->br_fore, state->br_bkpush, state->br_bkmouseover);

	UNCAPBTN_set_brushes(state->controls.btn_close, TRUE, state->br_border, state->br_bk, state->br_fore, state->br_bkpush, state->br_bkmouseover);

	EDITONELINE_set_brushes(state->controls.edit_match, TRUE, state->br_edit_txt, state->br_edit_bk, state->br_edit_bk, state->br_edit_txt, state->br_edit_bk, state->br_edit_bk);
}


static LRESULT CALLBACK SearchProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	SearchProcState* state = SEARCH_get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{
		SearchProcState* st = (SearchProcState*)calloc(1, sizeof(SearchProcState));
		Assert(st);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		SearchInit* init = (SearchInit*)creation_nfo->lpCreateParams;
		Assert(init);
		st->parent = creation_nfo->hwndParent;
		st->wnd = hwnd;
		st->parent_type = init->parent_type;
		st->placement_flags = init->SearchPlacement_flags;
		st->search_flags = init->SearchFlag_flags;
		SEARCH_set_state(hwnd, st);
		return TRUE;
	} break;
	case WM_NCCALCSIZE: {
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
	case WM_CREATE:
	{
		SEARCH_add_controls(state);
		SEARCH_resize_wnd(state);
		return 0;
	} break;
	case SRH_AUTORESIZE: //NOTE: will always trigger a WM_SIZE
	{
		SEARCH_resize_wnd(state);
		return 0;
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		SEARCH_resize_controls(state);
		return res;
	} break;
	case WM_MOVE: //Sent on startup after WM_SIZE, although possibly sent by DefWindowProc after I let it process WM_SIZE, not sure
	{
		//This msg is received _after_ the window was moved
		//Here you can obtain x and y of your window's client area
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_WINDOWPOSCHANGING:
	{
		//Someone calls SetWindowPos with the new values, here you can apply modifications over those
		WINDOWPOS* p = (WINDOWPOS*)lparam;
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_WINDOWPOSCHANGED:
	{
		WINDOWPOS* p = (WINDOWPOS*)lparam; //new window pos, size, etc
		return DefWindowProc(hwnd, msg, wparam, lparam); //TODO(fran): if we handle this msg ourselves instead of calling DefWindowProc we wont need to handle WM_SIZE and WM_MOVE since they wont be sent, also it says it's more efficient https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged
	} break;
	case WM_PARENTNOTIFY://Notifications about my child controls
	{
		return 0;
	} break;
	case WM_PAINT:
	{
		//TODO(fran): maybe I should use WS_CLIPCHILDREN
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		HBRUSH bk = state->br_bk;
		FillRect(dc, &ps.rcPaint, bk);
		EndPaint(hwnd, &ps);
		return 0;
	} break;
	case WM_NCDESTROY:
	{
		if (state) {
			free(state);
			SEARCH_set_state(hwnd, 0);
		}
		return 0;
	} break;
	case WM_DESTROY:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SHOWWINDOW: //On startup I received this cause of WS_VISIBLE flag
	{
		//Sent when window is about to be hidden or shown, doesnt let it clear if we are in charge of that or it's going to happen no matter what we do
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCPAINT:
	{
		//Paint non client area, we shouldnt have any
		HDC hdc = GetDCEx(hwnd, (HRGN)wparam, DCX_WINDOW | DCX_USESTYLE);
		ReleaseDC(hwnd, hdc);
		return 0; //we process this message, for now
	} break;
	case WM_ERASEBKGND:
	{
		return 0; //we dont erase the background here
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
			hittest = HTCLIENT;
		}
		return hittest;
	}
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
	case WM_MOUSEMOVE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_MOUSEACTIVATE:
	{
		//Sent to us when we're still an inactive window and we get a mouse press
		//TODO(fran): we could also ask our parent (wparam) what it wants to do with us
		return MA_ACTIVATE; //Activate our window and post the mouse msg
	} break;
	case WM_LBUTTONDOWN:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_SETCONTEXT://sent the first time on SetFocus
	{
		return 0; //We dont want IME for the general wnd, the childs can decide
	} break;
	case WM_SETFOCUS:
	{
		SetFocus(state->controls.edit_match);
		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		return 0;
	} break;
	case WM_COMMAND://Notifs from our childs
	{
		HWND child = (HWND)lparam;
		if (child) {//a control has sent a notification
			if (child == state->controls.edit_match) {
				WORD notif = HIWORD(wparam);
				switch (notif) {
				case EN_CHANGE:
				{
					int char_len = SendMessage(state->controls.edit_match, WM_GETTEXTLENGTH, 0, 0);//doesnt include null terminator
					if (!char_len) {
						DWORD sel_start;
						SendMessage(state->parent, EM_GETSEL, (WPARAM)&sel_start, 0); //TODO(fran): this should be type dependent, but really everybody should have this same one
						SendMessage(state->parent, EM_SETSEL, sel_start, sel_start); //TODO(fran): this should be type dependent, but really everybody should have this same one
						
					}
					SEARCH_search_and_scroll(state,true);
				} break;
				case EN_ENTER:
				{
					SEARCH_search_and_scroll(state,false);
				} break;
				case EN_ESCAPE:
				{
					ShowWindow(state->wnd, SW_HIDE);
				} break;
				}
			}
			else if(child == state->controls.btn_case_sensitive){
				state->search_flags ^= SearchFlag::case_sensitive; //flip the flag
				
				HBRUSH bk = (state->search_flags & SearchFlag::case_sensitive) ? state->br_bkselected : state->br_bk;
				UNCAPBTN_set_brushes(state->controls.btn_case_sensitive, TRUE, 0, bk, 0, 0, 0);
			}
			else if (child == state->controls.btn_whole_word) {
				state->search_flags ^= SearchFlag::whole_word; //flip the flag

				HBRUSH bk = (state->search_flags & SearchFlag::whole_word) ? state->br_bkselected : state->br_bk;
				UNCAPBTN_set_brushes(state->controls.btn_whole_word, TRUE, 0, bk, 0, 0, 0);
			}
			else if (child == state->controls.btn_wrap) {
				state->search_flags ^= SearchFlag::no_wrap; //flip the flag

				HBRUSH bk = !(state->search_flags & SearchFlag::no_wrap) ? state->br_bkselected : state->br_bk;
				UNCAPBTN_set_brushes(state->controls.btn_wrap, TRUE, 0, bk, 0, 0, 0);
			}
			else if (child == state->controls.btn_find_next) {
				state->search_flags &= ~SearchFlag::search_up; //clear the flag

				SEARCH_search_and_scroll(state, false);
			}
			else if (child == state->controls.btn_find_prev) {
				state->search_flags |= SearchFlag::search_up; //set the flag

				SEARCH_search_and_scroll(state, false);
			}
			else if (child == state->controls.btn_close) {
				ShowWindow(state->wnd, SW_HIDE);
			}
			else Assert(0);
		}
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

void init_wndclass_protect_search(HINSTANCE instance) {
	WNDCLASSEXW scrollbar;

	scrollbar.cbSize = sizeof(scrollbar);
	scrollbar.style = CS_HREDRAW | CS_VREDRAW;
	scrollbar.lpfnWndProc = SearchProc;
	scrollbar.cbClsExtra = 0;
	scrollbar.cbWndExtra = sizeof(SearchProcState*);
	scrollbar.hInstance = instance;
	scrollbar.hIcon = NULL;
	scrollbar.hCursor = LoadCursor(nullptr, IDC_ARROW);
	scrollbar.hbrBackground = NULL;
	scrollbar.lpszMenuName = NULL;
	scrollbar.lpszClassName = protect_wndclass_search;
	scrollbar.hIconSm = NULL;

	ATOM class_atom = RegisterClassExW(&scrollbar);
	Assert(class_atom);
}
