#pragma once
#include "win_sdk.h"
#include "platform.h"
#include "helpers.h"

namespace search {

/**
  * Returns zero-based index of the first char of the match or -1 if not found
  */
auto search(State& state, bool search_in_current_selection) { 
	//TODO(fran): extra param, search from sel_start or sel_end, when the user presses the enter key or the find button they want to start from sel_end, while they are writing they want to start from sel_start
	//INFO: from_sel_start: taking by reference point a search downwards if true the search happens from the start of the current selection, otherwise from the end, the same desired behaviour will happen for searching up where from_sel_start true will mean to search from the end, yes confusing, just take into account how it works for searching downwards and it will automatically work perfectly for searching up
	
	//TODO(fran): searching should roll over, say we get to the end of the file and dont find anything then we go from the start, and only then can we say to the user that we didnt find anything

	struct search_result { int p, sz_char; } res, fail_res{.p = -1};

	int match_len = (int)SendMessageW(state.controls.edit_match, WM_GETTEXTLENGTH, 0, 0) + 1;//lenght in characters, includes null terminator
	res.sz_char = match_len-1;
	if (match_len > 1) {
		WCHAR* match = (WCHAR*)malloc(match_len * sizeof(*match)); defer{ free(match); };
		SendMessage(state.controls.edit_match, WM_GETTEXT, match_len, (LPARAM)match);

		switch (state.parent_type) {
		case ParentType::richedit: //TODO(fran): FR_DOWN was added in rich edit 2.0 !!!!!! thanks microsoft, we gotta special case that too
		{
			/*int search_flags = (state.search_flags & Flag::search_up ? 0 : FR_DOWN) | (state.search_flags & Flag::whole_word ? 0 : FR_WHOLEWORD);
			FINDTEXT find;
			find.chrg.cpMin = range.min;
			find.chrg.cpMax = range.max + 1;
			find.lpstrText = match;
			res = SendMessage(state.parent, _EM_FINDTEXT, search_flags, (LPARAM)&find);*/
			Assert(0);
		} break;
		case ParentType::edit:
		{
			HLOCAL loc = (HLOCAL)SendMessage(state.parent, EM_GETHANDLE, 0, 0);
			if (!loc) return fail_res;
			WCHAR* str = (WCHAR*)LocalLock(loc); //INFO: with commctrl version 6 the text is always stored as WCHAR
			if (!str) return fail_res;
			defer{ LocalUnlock(loc); }; 
			int str_len = (int)SendMessage(state.parent, WM_GETTEXTLENGTH, 0, 0)+1;//in characters, includes null terminator
			DWORD sel_start = 0, sel_end = 0;
			SendMessage(state.parent, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);//INFO: even if the selection was made backwards, say the user clicked and dragged to the left, this msg always returns the right facing selection with the lower number on sel_start //TODO(fran): what happens in smth like arabic?
			DWORD sel;

			Range range;
			DWORD search_flags= LINGUISTIC_IGNOREDIACRITIC;
			if (!(state.search_flags & Flag::case_sensitive))
				search_flags |= LINGUISTIC_IGNORECASE; //TODO(fran): study the flags, there are some more complex things that I may want to add
			if (state.search_flags & Flag::search_up) {//search from the beginning of the selection to the first char
				sel = (search_in_current_selection) ? minimum(sel_end+2, (DWORD)str_len/*-1 ?*/) : sel_start;
				//INFO IMPORTANT: searching up is tricky, each time we search we establish a selection and the next time we search up from it, problem with this is that the user is actually looking for something that will potentially be on the right of that selection ("downwards"), therefore we need to extend the selection +1 to the right to avoid the continually scrolling problem, also +1 is probably not enough since you could have those additive codepoints that take more than one wchar
				range.min = 0;
				range.max = sel;// should subtract 1
				search_flags |= FIND_FROMEND;
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

			//TODO(fran): we could implement whole_word by ourselves, simply check what's next to and behind the found text

			if (res.p == -1 && !(state.search_flags & Flag::no_wrap)) {//we didnt find anything in our range, lets test the leftover range but keeping our direction
				if (state.search_flags & Flag::search_up) {
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
		case ParentType::custom:
		{
			res.p = -1;
			if (state.functions.on_search) 
				state.functions.on_search(match, match_len - 1, state.search_flags, search_in_current_selection, state.user_data);
		} break;
		default: Assert(0); break;
		}
	}
	else res.p = -1;
	
	// The custom managed operation may need to know when a search has been cleared
	if (match_len <= 1 && state.parent_type == ParentType::custom && state.functions.on_search)
		state.functions.on_search(0, 0, state.search_flags, search_in_current_selection, state.user_data);
	return res;
}
void make_selection(State& state, int p /*zero-based*/, int char_count) {
	switch (state.parent_type) {
	case ParentType::richedit:
	{
		//CHARRANGE range;
		//range.cpMin = p;
		//range.cpMax = p+char_count;//+1 ?
		//SendMessage(state.parent, EM_EXSETSEL, 0, (LPARAM)&range);
	} break;
	case ParentType::edit:
	{
		SendMessage(state.parent, EM_SETSEL, p, p + char_count);
	} break;
	default: Assert(0); break;
	}
}
void scroll_to_selection(State& state) {//Scrolls the text to the current selection
	//TODO(fran): this scrolls to the specific line, therefore the selection can end up on the first or last visible lines, we would prefer the selection gets a little more centered to the middle of the control
	switch (state.parent_type) {
	case ParentType::richedit:
	{
		SendMessage(state.parent, EM_SCROLLCARET, 0, 0);
	} break;
	case ParentType::edit:
	{
		SendMessage(state.parent, EM_SCROLLCARET, 0, 0);

		//The current selection should be on a visible line, now we move that line a little closer to the center if it is too close to the top or bottom
		int first_visible_line = (int)SendMessage(state.parent, EM_GETFIRSTVISIBLELINE, 0, 0);
		//EM_GETLASTVISIBLELINE
		RECT r; SendMessage(state.parent, EM_GETRECT, 0, (LPARAM)&r);//TODO(fran): make sure this is actually useful when the rect is modified instead of just using getclientrect
		DWORD c = (DWORD)SendMessage(state.parent, EM_CHARFROMPOS, 0, MAKELONG(0,RECTH(r)-1));
		int last_visible_line = HIWORD(c);
		int max_visible_lines = distance(first_visible_line,last_visible_line);
		int current_line = (int)SendMessage(state.parent, EM_LINEFROMCHAR, -1 /*get the line of the current selection*/, 0);
		int dist_to_first_line = distance(current_line, first_visible_line);
		int dist_to_last_line = distance(current_line, last_visible_line);
		if (max_visible_lines > 3 ) {
			if (dist_to_first_line < 3) {//we are too close to the top, we need to go down
				int line_scroll = minimum(2, dist_to_last_line);
				line_scroll = -line_scroll; //strangely enough to scroll down you use negative numbers, and positive for up
				SendMessage(state.parent, EM_LINESCROLL, 0, line_scroll);
			}
			else if (dist_to_last_line < 3) {//we are too close to the bottom, we need to go up
				int line_scroll = minimum(2, dist_to_first_line);
				SendMessage(state.parent, EM_LINESCROLL, 0, line_scroll);
			}
		}
	} break;
	default: Assert(0); break;
	}
}

void search_and_scroll(State& state, bool search_in_current_selection) {
	auto search_res = search(state, search_in_current_selection);
	if (search_res.p != -1) { //TODO(fran): I like visual studio's way to tell the user if they couldnt match, they put a red border around the edit wnd, very simple to implement just change the border brush
		make_selection(state, search_res.p, search_res.sz_char);
		scroll_to_selection(state);
	}
}

auto get_state(HWND wnd) { _control_create_function__get_state }

//TODO(fran): when the parent gets resized it should tell us to resize
void resize_wnd(State& state) {
	Assert(state.placement_flags);

	RECT r; GetWindowRect(state.parent, &r); //INFO: you cant use getclientrect with text editors since they seem to change their client rect when you scroll them and would cause the position of this control to start scrolling too
	int w = RECTW(r);
	int h = RECTH(r);
	int font_h;
	{
		HFONT font = (HFONT)GetWindowFont(state.controls.edit_match);
		HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd, dc); };
		HFONT oldfont = (HFONT)SelectObject(dc, font); defer{ SelectObject(dc, oldfont); };
		TEXTMETRIC tm; GetTextMetrics(dc, &tm);
		font_h = (int)((float)tm.tmHeight * 1.2f); //TODO(fran): what we'd actually need is for the edit control to tell us the caret size, and make our height a little bigger
	}
	int search_x;
	int search_y;
	int search_w;
	int search_h;
	if (state.placement_flags & Placement::left || state.placement_flags & Placement::right) {
		//small wnd
		search_h = font_h * 2;
		//search_w = (int)((float)w * .25f);//TODO(fran): there should be a max size, we just want a little thing in the corner
		HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd, dc); };
		SIZE a; GetTextExtentPoint32(dc, L"a", 1, &a);
		search_w = a.cx * 33;
	}
	else {
		//big wnd
		search_h = font_h;
		search_w = w;
		search_x = 0;
	}

	if (state.placement_flags & Placement::left) {
		search_x = 0;
		search_y = 0;
	}
	if (state.placement_flags & Placement::right) {
		search_x = w - search_w;
		search_y = 0;
	}
	if (state.placement_flags & Placement::top) {
		search_y = 0;
	}
	if (state.placement_flags & Placement::bottom) {
		search_y = h - search_h;
	}

	RECT non_clipped_rc = rectWH(search_x, search_y, search_w, search_h);
	RECT clipped_rc = clip_fit_childs(state.parent, state.wnd, non_clipped_rc);
	move_window(state.wnd, clipped_rc, true);
}

void resize_controls(State& state) {
	RECT r; GetClientRect(state.wnd, &r);
	int w = RECTW(r);
	int h = RECTH(r);
	int w_pad = 2;
	int h_pad = 2;
	bool is_persistent = check_trool(state.theme.styles.persistent);
	bool btn_case_sensitive_off = check_trool(state.theme.styles.btn_case_sensitive_off);
	bool btn_whole_word_off = check_trool(state.theme.styles.btn_whole_word_off);
	bool btn_wrap_off = check_trool(state.theme.styles.btn_wrap_off);
	bool btn_find_prev_off = check_trool(state.theme.styles.btn_find_prev_off);

	int btn_case_sensitive_x, btn_case_sensitive_y, btn_case_sensitive_w, btn_case_sensitive_h;

	int btn_whole_word_x, btn_whole_word_y, btn_whole_word_w, btn_whole_word_h;

	int btn_wrap_x, btn_wrap_y, btn_wrap_w, btn_wrap_h;

	int btn_close_h, btn_close_w, btn_close_x, btn_close_y;

	int btn_find_next_w, btn_find_next_h, btn_find_next_x, btn_find_next_y;

	int btn_find_prev_w, btn_find_prev_h, btn_find_prev_x, btn_find_prev_y;

	int edit_match_x, edit_match_y, edit_match_w, edit_match_h;
	
	if (state.placement_flags & Placement::left || state.placement_flags & Placement::right) { //control is in small state
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
		edit_match_x = btn_case_sensitive_x + btn_case_sensitive_w;
		edit_match_y = btn_case_sensitive_y;
		edit_match_w = btn_find_next_x - edit_match_x;
		edit_match_h = h;
	}
	else {//wnd is in large state, here everything goes next to each other on one line
		int h_sub_pad = h - h_pad * 2;
		int left_button_w = h_sub_pad;

		//TODO(fran): here I could add a little w padding in between each control

		//"case sensitive" goes first on the left
		btn_case_sensitive_x = !btn_case_sensitive_off ? w_pad : 0;
		btn_case_sensitive_y = h_pad;
		btn_case_sensitive_h = h_sub_pad;
		btn_case_sensitive_w = !btn_case_sensitive_off ? left_button_w : 0;

		//then "whole word"
		btn_whole_word_x = btn_case_sensitive_x + btn_case_sensitive_w + !btn_whole_word_off ? w_pad : 0;
		btn_whole_word_y = btn_case_sensitive_y;
		btn_whole_word_w = !btn_whole_word_off ? left_button_w : 0;
		btn_whole_word_h = btn_whole_word_w;

		//"wrap around"
		btn_wrap_x = btn_whole_word_x + btn_whole_word_w + !btn_wrap_off ? w_pad : 0;
		btn_wrap_y = btn_case_sensitive_y;
		btn_wrap_w = !btn_wrap_off ? left_button_w : 0;
		btn_wrap_h = btn_wrap_w;

		//"close" goes last on the right
		btn_close_h = btn_case_sensitive_h / 2;
		btn_close_w = btn_close_h;
		btn_close_x = is_persistent ? w : w - btn_close_w - w_pad; //when persistent we dont show this control, so we dont want it to affect the position of the ones next to it
		btn_close_y = btn_case_sensitive_y;

		//"find prev" goes left of "close"
		btn_find_prev_h = btn_case_sensitive_h;
		SIZE _btn_find_prev_sz{ 0 }; Button_GetIdealSize(state.controls.btn_find_prev, &_btn_find_prev_sz);
		btn_find_prev_w = _btn_find_prev_sz.cx;
		btn_find_prev_x = !btn_find_prev_off ? btn_close_x - btn_find_prev_w - w_pad : btn_close_x;
		btn_find_prev_y = btn_case_sensitive_y;

		//"find next" goes left of "find prev"
		btn_find_next_h = btn_find_prev_h;
		SIZE _btn_find_next_sz{ 0 }; Button_GetIdealSize(state.controls.btn_find_next, &_btn_find_next_sz);
		btn_find_next_w = _btn_find_next_sz.cx;
		btn_find_next_x = btn_find_prev_x - btn_find_next_w - w_pad;
		btn_find_next_y = btn_case_sensitive_y;

		//"match" occupies the rest of the space in the middle
		edit_match_x = btn_wrap_x + btn_wrap_w + w_pad;
		edit_match_y = btn_case_sensitive_y;
		edit_match_w = btn_find_next_x - edit_match_x - w_pad;
		edit_match_h = btn_case_sensitive_h;
	}
	if (!btn_case_sensitive_off) MoveWindow(state.controls.btn_case_sensitive, btn_case_sensitive_x, btn_case_sensitive_y, btn_case_sensitive_w, btn_case_sensitive_h, TRUE);
	if (!btn_whole_word_off) MoveWindow(state.controls.btn_whole_word, btn_whole_word_x, btn_whole_word_y, btn_whole_word_w, btn_whole_word_h, TRUE);
	if (!btn_wrap_off) MoveWindow(state.controls.btn_wrap, btn_wrap_x, btn_wrap_y, btn_wrap_w, btn_wrap_h, TRUE);
	if (!is_persistent) MoveWindow(state.controls.btn_close, btn_close_x, btn_close_y, btn_close_w, btn_close_h, TRUE);
	if(!btn_find_prev_off) MoveWindow(state.controls.btn_find_prev, btn_find_prev_x, btn_find_prev_y, btn_find_prev_w, btn_find_prev_h, TRUE);
	MoveWindow(state.controls.btn_find_next, btn_find_next_x, btn_find_next_y, btn_find_next_w, btn_find_next_h, TRUE);
	MoveWindow(state.controls.edit_match, edit_match_x, edit_match_y, edit_match_w, edit_match_h, TRUE);
}

void add_controls(State& state) {

	auto& controls = state.controls;
	//TODO(fran): which controls to show should be specific to the type of editor available, that's where we need a resizer so we dont have to manually position all this guys and we can replace them, add, remove, easily

	//INFO: instead of using control identifiers as the HMENU param im simply gonna filter them with the HWND, for example on WM_COMMAND
	controls.btn_case_sensitive = create_window(state.wnd, button::wndclass, L"Aa"/*works on any language, maybe*/, WS_VISIBLE | WS_CHILD | WS_TABSTOP);

	controls.btn_whole_word = create_window(state.wnd, button::wndclass, L"' '"/*impossible to describe*/, WS_VISIBLE | WS_CHILD | WS_TABSTOP);

	controls.btn_wrap = create_window(state.wnd, button::wndclass, L"↺", WS_VISIBLE | WS_CHILD | WS_TABSTOP);
	
	//TODO(fran): this should be the new button, rendering text when possible or an img otherwise
	controls.btn_find_prev = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP);
	AWT(controls.btn_find_prev, LANG_SEARCH_FINDPREV);

	//TODO(fran): this should be the new button, rendering text when possible or an img otherwise
	controls.btn_find_next = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_BITMAP);
	SendMessage(controls.btn_find_next, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.search);
	AWT(controls.btn_find_next, LANG_SEARCH_FINDNEXT);
	add_mouseover_tooltip(controls.btn_find_next, LANG_SEARCH_FINDNEXT);

	controls.btn_close = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_BITMAP);
	SendMessage(controls.btn_close, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.close);

	controls.edit_match = create_window(state.wnd, edit_oneline::wndclass, L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP);
	AWDT(controls.edit_match, LANG_SEARCH);
}

void on_style_changed(State& state) {
	ShowWindow(state.controls.btn_close, !check_trool(state.theme.styles.persistent) ? SW_SHOW : SW_HIDE);
	ShowWindow(state.controls.btn_case_sensitive, !check_trool(state.theme.styles.btn_case_sensitive_off) ? SW_SHOW : SW_HIDE);
	ShowWindow(state.controls.btn_whole_word, !check_trool(state.theme.styles.btn_whole_word_off) ? SW_SHOW : SW_HIDE);
	ShowWindow(state.controls.btn_wrap, !check_trool(state.theme.styles.btn_wrap_off) ? SW_SHOW : SW_HIDE);
}

void set_theme(HWND wnd, const Theme& src) {
	_control_create_function__set_theme;
	auto& controls = state.controls;
	auto& theme = state.theme;

	button::set_theme(controls.btn_case_sensitive, theme.btn_theme);
	button::set_theme(controls.btn_whole_word, theme.btn_theme);
	button::set_theme(controls.btn_wrap, theme.btn_theme);
	button::set_theme(controls.btn_find_prev, theme.btn_theme);
	button::set_theme(controls.btn_find_next, theme.btn_theme);
	button::set_theme(controls.btn_close, theme.btn_theme);
	edit_oneline::set_theme(state.controls.edit_match, theme.txt_theme);

	on_style_changed(state);
}

void hide_window(State& state) {
	SetFocus(state.parent);
	ShowWindow(state.wnd, SW_HIDE);
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{
		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		Init* init = (Init*)creation_nfo->lpCreateParams;
		Assert(init);
		st->parent = creation_nfo->hwndParent;
		st->wnd = hwnd;
		st->parent_type = init->parent_type;
		st->placement_flags = init->SearchPlacement_flags;
		st->search_flags = init->SearchFlag_flags;
		set_window_state(hwnd, st);
		return TRUE;
	} break;
	case WM_CREATE:
	{
		add_controls(state);
		resize_wnd(state);
		return 0;
	} break;
	case WM_NCDESTROY:
	{
		if (&state) {
			set_window_state(state.wnd, 0);
			free(&state);
		}
		return 0;
	} break;
	case SRH_AUTORESIZE: //NOTE: will always trigger a WM_SIZE
	{
		resize_wnd(state);
		return 0;
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		resize_controls(state);
		return res;
	} break;
	case WM_NCPAINT:
	{
		return 0;
	} break;
	case WM_ERASEBKGND: //Use WM_ERASEBKGND instead of WM_PAINT so that child controls magically get the areas they dont draw in/use transparency covered with this background
	{
		auto dc = (HDC)wparam;
		RECT r; GetClientRect(state.wnd, &r);
		HBRUSH bk = state.theme.brushes.bk.normal;
		HBRUSH border = state.theme.brushes.border.normal;
		urender::draw_background(dc, r, bk, border, state.theme.dimensions);
		return 1;
	} break;
	case WM_SETFONT:
	{
		for (auto ctl : state.controls.all) SetWindowFont(ctl, wparam, lparam);
		return 0;
	} break;
	case WM_NCHITTEST:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		RECT rcWindow; GetWindowRect(state.wnd, &rcWindow);

		LRESULT hittest = test_pt_rc(mouse, rcWindow) ? HTCLIENT : HTNOWHERE;

		return hittest;
	}
	case WM_MOUSEACTIVATE:
	{
		return MA_ACTIVATE;
	} break;
	case WM_IME_SETCONTEXT:
	{
		return 0; //We dont want IME for the general wnd, the childs can decide
	} break;
	case WM_SETFOCUS:
	{
		SetFocus(state.controls.edit_match);
		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		return 0;
	} break;
	case WM_PARENTNOTIFY://Notifications about my child controls
	{
		return 0;
	} break;
	case WM_COMMAND:
	{
		HWND child = (HWND)lparam;
		if (child) {
			if (child == state.controls.edit_match) {
				WORD notif = HIWORD(wparam);
				switch (notif) {
				case EN_CHANGE:
				{
					int char_len = (int)SendMessage(state.controls.edit_match, WM_GETTEXTLENGTH, 0, 0);//doesnt include null terminator
					if (!char_len) {
						DWORD sel_start;
						SendMessage(state.parent, EM_GETSEL, (WPARAM)&sel_start, 0); //TODO(fran): this should be type dependent, but really everybody should have this same one
						SendMessage(state.parent, EM_SETSEL, sel_start, sel_start); //TODO(fran): this should be type dependent, but really everybody should have this same one
					}
					search_and_scroll(state,true);
				} break;
				case EN_ENTER:
				{
					search_and_scroll(state,false);
				} break;
				case EN_ESCAPE:
				{
					if (!check_trool(state.theme.styles.persistent)) hide_window(state);
				} break;
				}
			}
			else if(child == state.controls.btn_case_sensitive && !check_trool(state.theme.styles.btn_case_sensitive_off)){
				state.search_flags ^= Flag::case_sensitive;
				button::set_selected(state.controls.btn_case_sensitive, state.search_flags& Flag::case_sensitive);
			}
			else if (child == state.controls.btn_whole_word && !check_trool(state.theme.styles.btn_whole_word_off)) {
				state.search_flags ^= Flag::whole_word;
				button::set_selected(state.controls.btn_whole_word, state.search_flags& Flag::whole_word);
			}
			else if (child == state.controls.btn_wrap && !check_trool(state.theme.styles.btn_wrap_off)) {
				state.search_flags ^= Flag::no_wrap;
				button::set_selected(state.controls.btn_wrap, state.search_flags& Flag::no_wrap);
			}
			else if (child == state.controls.btn_find_next) {
				state.search_flags &= ~Flag::search_up;
				search_and_scroll(state, false);
			}
			else if (child == state.controls.btn_find_prev && !check_trool(state.theme.styles.btn_find_prev_off)) {
				state.search_flags |= Flag::search_up;
				search_and_scroll(state, false);
			}
			else if (child == state.controls.btn_close && !check_trool(state.theme.styles.persistent)) {
				hide_window(state);
			}
			else Assert(0);
		}
		return 0;
	} break;
#ifdef _DEBUG_HWND_MESSAGES
	case WM_NCCALCSIZE:
	case WM_MOVE:
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
	case WM_DESTROY:
	case WM_SHOWWINDOW:
	case WM_SETCURSOR:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
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

void set_user_data(HWND wnd, void* user_data) { _control_create_function__set_user_data }

void set_functions(HWND wnd, const Functions& functions) { _control_create_function__set_functions }
}