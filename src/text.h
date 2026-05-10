#pragma once
#include "win_sdk.h"
#include "helpers.h"

//TODO(fran): 'show password' button (this needs to be a new element made up from 3 others: border + (editoneline + button)
//TODO(fran): ballon tips, probably handmade since windows doesnt allow it anymore, the ballon tail makes it much clearer what the tip is referring to in cases where there's many controls next to each other
//TODO(fran): paint/handle my own IME window https://docs.microsoft.com/en-us/windows/win32/intl/ime-window-class
//TODO(fran): since WM_SETTEXT doesnt notify by default we should change WM_SETTEXT_NO_NOTIFY to WM_SETTEXT_NOTIFY and reverse the current roles
//TODO(fran): IDEA for multiline with wrap around text, keep a list of wrap around points, be it a new line, word break or word that doesnt fit on its line, then we can go straight from user clicking the wnd to the corresponding line by looking how many lines does the mouse go and input that into the wrap around list to get to that line's first char idx
//TODO(fran): on WM_STYLECHANGING check for password changes, that'd need a full recalculation
//TODO(fran): on a WM_STYLECHANGING we should check if the alignment has changed and recalc/redraw every char, NOTE: I dont think windows' controls bother with this since it's not too common of a use case
//TODO(fran): at some point I got to trigger a secondary IME candidates window which for some reason decided to show up, this BUG is probably related to the multiple candidates window, maybe we need to manually block the secondary ones, it could be that if you write too much text this extra candidate windows appear
//TODO(fran): it seems like the IME window is global or smth like that, if I dont ask for candidate windows on WM_IME_SETCONTEXT then it also wont show them for other controls, wtf
//TODO(fran): change EM_SETINVALIDCHARS to be a function call provided by the user, 2 opts: bool that says tell me if there's anything invalid OR remove anything invalid, 2nd opt return array with indexes of invalid chars

namespace edit_oneline {

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_user_extra(HWND wnd, void* user_extra) {
	State& state = *get_state(wnd);
	if (&state) {
		state.user_extra = user_extra;
		//TODO(fran): redraw?
	}
}

void set_function_has_invalid_chars(HWND wnd, func_has_invalid_chars func) {
	State& state = *get_state(wnd);
	if (&state) {
		state.has_invalid_chars = func;
	}
}

SIZE calc_caret_dim(State& state) {
	SIZE res;
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm;
	GetTextMetrics(dc, &tm);
	int avg_height = tm.tmHeight;
	res.cx = 1;
	res.cy = avg_height;
	return res;
}

int calc_line_height_px(State& state) {
	int res;
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);
	res = tm.tmHeight;
	return res;
}

SIZE calc_char_dim(State& state, cstr c) {
	Assert(c != _t('\t'));

	LONG_PTR  style = GetWindowLongPtr(state.wnd, GWL_STYLE);
	if (style & ES_PASSWORD) c = password_char;

	SIZE res;
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	//UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };
	//SetTextAlign(dc, TA_LEFT);
	GetTextExtentPoint32(dc, &c, 1, &res);
	return res;
}

SIZE calc_text_dim(State& state) { //TODO(fran): @multiline?
	SIZE res;
	LONG_PTR  style = GetWindowLongPtr(state.wnd, GWL_STYLE);
	if (style & ES_PASSWORD) {
		res = calc_char_dim(state, 0);
		res.cx *= (LONG)state.char_text.length();
	}
	else {
		HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
		HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
		//UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };
		//SetTextAlign(dc, TA_LEFT);
		GetTextExtentPoint32(dc, state.char_text.c_str(), (int)state.char_text.length(), &res);
	}

	return res;
}

//"Hello\n\n\n"
// -lines: "Hello\n" "\n "\n"
// -cursor idx mapping: func(char_idx = 1 aka H|ello) -> line_char_idx = 0 aka |Hello ; line_idx = 0 aka Hello|\n
//						func(char_idx = 6 aka Hello\n|\n) -> line_char_idx = 6 Hello\n|\n ; line_idx = 1 aka Hello\n|\n

//Finds out the index of the first character of the line where the input character index belongs to
//(This will be either 0 for the first line, or one character past the \n of the previous line for all other lines)
auto char_idx_to_line_char_idx(const State& state, size_t char_idx) {
	struct _char_idx_to_line_char_idx_res { u64 line_char_idx; u64 line_idx; } res;
	res.line_char_idx = state.char_text.rfind('\n', safe_subtract0(char_idx, 1)) + 1; //TODO(fran): I do this instead of iterating over state.line_breaks cause I think it scales better to larger strings and may even be generally faster or close to it, though there maybe be a fast way to search through state.line_breaks, otherwise having that cache is pointless & I should remove it

	res.line_idx = 0;
	if (res.line_char_idx) {
		u64 line_idx_estimate = (u64)safe_ratio0((f64)res.line_char_idx, (f64)state.char_text.size()) * safe_subtract0(state.line_breaks.size(), 1);
		i64 increment = state.line_breaks[line_idx_estimate] < res.line_char_idx ? +1 : -1;

		for (u64 i = line_idx_estimate; i < state.line_breaks.size(); i += increment) {
			if (state.line_breaks[i] == res.line_char_idx - 1) {
				if (char_idx == state.line_breaks[i]) res.line_idx = i;
				else res.line_idx = i + 1;
				break;
				//TODO(fran): this code sucks, as well as its use in calc_caret_p, this NEEDS a redesign. Look at the selection rendering code for inspiration, since it does the exact same thing, I should be able to reuse it somehow
			}
		}
	}
	//TODO(fran): now I need to know the line idx, I guess the cache wasnt so pointless after all, although this search pattern may even be faster if I now add a hash table to store a mapping line_char_idx -> line_idx (remember to remove the -1 before using it on the hash table though)
		//NOTE: I will usually need both line_idx and line_char_idx, since I use the first one to move in Y and the other to move in X
		//TODO(fran)?: if we dont want to do the hash table thing we could estimate the index: line_idx_estimation = (char_idx / total_chars) * total_lines; and iterate forward or backwards from there

	//TODO(fran): im having this issue with the fact that line_idx = 0 is not always a valid index into state.line_breaks, and we have a dual mapping for 0, it will return 0 when there are no line breaks, but also 0 when we are on the first line break. The code needs to be re-tought, maybe we should return a line count, that is always at least 1. This would fix the dual mapping problem though it would still not fix the access into state.line_breaks
	return res;
}

POINT calc_caret_p(const State& state) {
	Assert(safe_subtract0(state.selection.cursor, (size_t)1) <= state.char_dims.size());
	POINT res = { state.padding.x, state.padding.y };

	auto [line_char_idx, line_idx] = char_idx_to_line_char_idx(state, state.selection.cursor);

	for (size_t i = line_char_idx; i < state.selection.cursor; i++) {
		res.x += state.char_dims[i];
	}

	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);

	res.y += (LONG)(line_idx * tm.tmHeight);
	return res;
}

i32 min_height(State& state) {
	i32 res = state.padding.y + calc_line_height_px(state);
	return res;
}

i32 desired_height(State& state) {
	i32 res = state.padding.y + calc_line_height_px(state) * ((i32)state.line_breaks.size() + 1);
	return res;
}

//TODO(fran): positioning relative to a character offset, eg {3,5} 3rd char of 5th row place on top or left or...
void show_tip(HWND wnd, const cstr* msg, int duration_ms, u32 ETP_flags) {
	State& state = *get_state(wnd); Assert(&state);
	TOOLINFO toolInfo{ sizeof(toolInfo) };
	toolInfo.hwnd = state.wnd;
	toolInfo.uId = (UINT_PTR)state.wnd;
	toolInfo.hinst = GetModuleHandle(NULL);
	toolInfo.lpszText = const_cast<cstr*>(msg);
	SendMessage(state.controls.tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo);
	SendMessage(state.controls.tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&toolInfo);
	POINT tooltip_p{ 0,0 };
	RECT tooltip_r;
	tooltip_r.left = 0;
	tooltip_r.right = 50;
	tooltip_r.top = 0;
	tooltip_r.bottom = calc_line_height_px(state);//The tooltip uses the same font, so we know the height of its text area
	SendMessage(state.controls.tooltip, TTM_ADJUSTRECT, TRUE, (LPARAM)&tooltip_r);//convert text area to tooltip full area

	RECT rc; GetClientRect(state.wnd, &rc);

	if (ETP_flags & ETP::left) {
		tooltip_p.x = 0;
	}

	if (ETP_flags & ETP::right) {
		tooltip_p.x = RECTW(rc);
	}

	if (ETP_flags & ETP::top) {
		tooltip_p.y = -RECTH(tooltip_r);
	}

	if (ETP_flags & ETP::bottom) {
		tooltip_p.y = RECTH(rc);
	}

	ClientToScreen(state.wnd, &tooltip_p);
	SendMessage(state.controls.tooltip, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(tooltip_p.x, tooltip_p.y));
	SetTimer(state.wnd, EDITONELINE_tooltip_timer_id, duration_ms, NULL);
}

void scroll_based_on_caret(State& state) {
	//TODO(fran): test right and center text alignments, @BUG: center alignment is wrong, probably due to my stupid use of padding.x, it should simply indicate the render area, instead Im using it to align the text when that should require a new variable

	RECT rc; GetClientRect(state.wnd, &rc);
	auto w = RECTW(rc), h = RECTH(rc);
	if (!w || !h) return; //TODO(fran): this is probably more accurate by checking for element visibility or smth like that
	//TODO(fran): im sure this can be done branchless
	if (int scroll = (state.caret.pos.x + state.caret.dim.cx) - (w - state.padding.x + state.scroll.x); scroll > 0) {
		//scroll right
		state.scroll.x += scroll;
		//ScrollWindowEx(state.wnd, scroll, 0, 0, 0, 0, 0, SW_ERASE|SW_INVALIDATE);
		ask_window_for_repaint(state.wnd); //TODO(fran): currently we simply invalidate the whole window and redraw everything, this may or may not be optimal
	}
	else if (int scroll = (state.caret.pos.x) - (state.scroll.x + state.padding.x); scroll < 0) {
		//scroll left
		state.scroll.x += scroll;
		ask_window_for_repaint(state.wnd);
	}

	if (int scroll = (state.caret.pos.y + state.caret.dim.cy) - (h - state.padding.y + state.scroll.y); scroll > 0) {
		//scroll down
		state.scroll.y += scroll;
		ask_window_for_repaint(state.wnd);
	}
	else if (int scroll = (state.caret.pos.y) - (state.scroll.y + state.padding.y); scroll < 0) {
		//scroll up
		state.scroll.y += scroll;
		ask_window_for_repaint(state.wnd);
	}
	else if ((GetWindowLongPtr(state.wnd, GWL_STYLE) & ES_EXPANSIBLE) && h >= desired_height(state)) {
		//HACK?: scroll up if you can fit all lines
		state.scroll.y = 0; //TODO(fran): im sure this special case will introduce bugs
		ask_window_for_repaint(state.wnd);
	}
}

void reposition_caret(State& state, bool nocheck = false) {
	POINT oldcaretp = state.caret.pos;
	state.caret.pos = calc_caret_p(state);
	if (oldcaretp != state.caret.pos || nocheck) {
		scroll_based_on_caret(state);

		//I havent found a way to apply a translation to the caret so that SetCaretPos automatically places it on the correct character even when the characters are scrolled, therefore we gotta apply the transformation manually
		POINT p = state.caret.pos;
		p.x -= state.scroll.x;
		p.y -= state.scroll.y;
		SetCaretPos(p); //TODO(fran): only the focussed element should move the caret
	}
	//if(GetFocus() == state.wnd) SetCaretPos(state.caret.pos); //NOTE: this introduced a bug with settext where calling settext with a null string on the focussed editbox would place the caret on the wrong place
}

void recalculate_char_dims(State& state) {
	state.char_dims.clear();
	for (auto c : state.char_text)
		state.char_dims.push_back(calc_char_dim(state, c).cx);
	reposition_caret(state);
}

void set_theme(HWND wnd, const Theme& src) {
	auto old_font = GetWindowFont(wnd);
	_control_create_function__set_theme;
	if (&state && old_font != state.theme.font) recalculate_char_dims(state);
}

void update_char_pad(State& state) {
	LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
	RECT rc; GetClientRect(state.wnd, &rc);
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);
	if (style & ES_CENTER) {
		//Recalc pad_x
		state.padding.x = (RECTW(rc) - calc_text_dim(state).cx) / 2;
	}
	else {
		state.padding.x = 3; //TODO(fran): user defined and 'dpi pixels' based
	}
	state.padding.y = !state.line_breaks.size() ? (rc.bottom + rc.top - tm.tmHeight) / 2 : rc.top + (RECTH(rc) % tm.tmHeight) / 2; //TODO(fran): provide additional styles so the user can change vertical alignment
}

void recalculate_line_breaks(State& state) {
	//TODO(fran): @speed: provide additional parameters to allow us to only recalculate line breaks after the changed characters
	state.line_breaks.clear();
	size_t linebreak = 0;
	while ((linebreak = state.char_text.find('\n', linebreak)) != std::wstring::npos) {
		state.line_breaks.push_back(linebreak);
		linebreak++;
	}
}

//Any event that is not a Arrow Up/Down event (keyboard strokes, mouse clicks; font/style changes?) should reset the vertical selection's stored width
void reset_vertical_selection_stored_width(State& state) {
	state.vertical_selection_stored_width = 0;
}

void remove_selection(State& state, size_t x_min, size_t x_max) {
	if (!state.char_text.empty()) {
		x_min = clamp((size_t)0, x_min, state.char_text.length());
		x_max = clamp((size_t)0, x_max, state.char_text.length());

		state.char_text.erase(x_min, distance(x_min, x_max));
		state.char_dims.erase(state.char_dims.begin() + x_min, state.char_dims.begin() + x_max);

		update_char_pad(state);

		recalculate_line_breaks(state);

		SendMessage(state.wnd, EM_SETSEL, x_min, x_min);
		//state.selection.anchor = state.selection.cursor = x_min;

		reset_vertical_selection_stored_width(state); //NOTE(fran): this may not be necessary here since most events that remove selections already reset this as a side effect, but we'll do it anyways for completion and because it costs nothing
	}
}

//Removes the current text selection and updates the selection values
void remove_selection(State& state) {
	remove_selection(state, state.selection.x_min(), state.selection.x_max());
}

void check_expansibility(State& state) {
	RECT rc; GetClientRect(state.wnd, &rc);

	if (RECTH(rc) < desired_height(state))
		ask_window_for_resize(state.parent);
}

//true if the current contents of the text were modified, false otherwise
bool insert_character(State& state, utf16_str s, size_t x_min, size_t x_max) {
	//TODO(fran): center alignment get offset wrongly when inserting text of any length
	bool res = false;
	x_min = clamp((size_t)0, x_min, state.char_text.length());
	x_max = clamp((size_t)0, x_max, state.char_text.length());
	char_sel sel{ x_min, x_max };

	if (safe_subtract0(state.char_text.length(), sel.sel_width()) < state.char_max_sz) {

		//check for invalid characters
		if (state.has_invalid_chars) {//TODO(fran): +1 for having always valid function pointers, we could branch on valid on invalid instead we gotta hack in a return statement in the middle of the code
			auto [invalid, explanation] = state.has_invalid_chars(s.str, maximum((size_t)0, s.sz_char() - 1), state.user_extra);
			if (invalid) {
				res = false;
				show_tip(state.wnd, explanation.c_str(), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
				return res;//HACK
			}
		}

		res = true;

		//remove existing selection
		if (sel.has_selection()) remove_selection(state, x_min, x_max);

		//insert new character
		state.char_text.insert(state.selection.cursor, s.str);

		for (size_t i = 0; i < s.sz_char() - 1; i++)
			state.char_dims.insert(state.char_dims.begin() + state.selection.cursor + i, calc_char_dim(state, s[i]).cx);

		recalculate_line_breaks(state);

		size_t anchor = state.selection.cursor + safe_subtract0(s.sz_char(), (size_t)1);
		size_t cursor = anchor;

		SendMessage(state.wnd, EM_SETSEL, anchor, cursor);

		reset_vertical_selection_stored_width(state);

		update_char_pad(state);
		reposition_caret(state);

		LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
		if (style & ES_EXPANSIBLE) check_expansibility(state);
	}
	return res;
}

//inserts character replacing the current selection
bool insert_character(State& state, utf16 c) {//TODO(fran): optimized version for single character insertion
	utf16 txt[2];
	txt[0] = c;
	txt[1] = 0;

	return insert_character(state, to_utf_str(txt), state.selection.x_min(), state.selection.x_max());
}

//inserts string replacing the current selection
bool insert_character(State& state, const utf16* s) {
	bool res = false;
	if (s)
		res = insert_character(state, to_utf_str(const_cast<utf16*>(s)), state.selection.x_min(), state.selection.x_max());
	return res;
}

bool _settext(State& state, cstr* buf /*null terminated*/) {
	bool res = false;
	cstr empty = 0;//INFO: compiler doesnt allow you to set it to L''
	if (!buf) buf = &empty; //NOTE: this is the standard default behaviour
	size_t char_sz = cstr_len(buf);//not including null terminator
	if (char_sz <= (size_t)state.char_max_sz) {
		//TODO(fran): settext should check for invalid characters

		SendMessage(state.wnd, EM_SETSEL, 0, -1);

		res = insert_character(state, buf);

		ask_window_for_repaint(state.wnd);//TODO(fran): probably unnecessary
	}
	return res;
}

//NOTE: pasting from the clipboard establishes a couple of invariants: lines end with \r\n, there's a null terminator, we gotta parse it carefully cause who knows whats inside
bool paste_from_clipboard(State& state, const cstr* txt) { //returns true if it could paste something
	bool res = false;
	size_t char_sz = cstr_len(txt);//does not include null terminator
	if ((size_t)state.char_max_sz >= state.char_text.length() + char_sz) {

	}
	else {
		char_sz -= ((state.char_text.length() + char_sz) - (size_t)state.char_max_sz);
	}
	if (char_sz > 0) {
		//TODO(fran): remove illegal chars (we dont know what could be inside)

		res = insert_character(state, txt);

	}
	return res;
}

void set_composition_pos(State& state)//TODO(fran): this must set the other stuff too
{
	HIMC imc = ImmGetContext(state.wnd);
	if (imc != NULL) {
		defer{ ImmReleaseContext(state.wnd, imc); };
		COMPOSITIONFORM cf;
		//NOTE immgetcompositionwindow fails
#if 0 //IME window no longer draws the unnecessary border with resizing and button, it is placed at cf.ptCurrentPos and extends down after each character
		cf.dwStyle = CFS_POINT;

		if (GetFocus() == state.wnd) {
			//NOTE: coords are relative to your window area/nc area, _not_ screen coords
			cf.ptCurrentPos.x = state.caret.pos.x;
			cf.ptCurrentPos.y = state.caret.pos.y + state.caret.dim.cy;
		}
		else {
			cf.ptCurrentPos.x = 0;
			cf.ptCurrentPos.y = 0;
		}
#else //IME window no longer draws the unnecessary border with resizing and button, it is placed at cf.ptCurrentPos and has a max size of cf.rcArea in the x axis, the y axis can extend a lot longer, basically it does what it wants with y
		if (!state.hide_IME_wnd) {
			cf.dwStyle = CFS_RECT;

			//TODO(fran): should I place the IME in line with the caret or below so the user can see what's already written in that line?
			if (GetFocus() == state.wnd) {
				cf.ptCurrentPos.x = state.caret.pos.x;
				cf.ptCurrentPos.y = state.caret.pos.y + state.caret.dim.cy;
				//TODO(fran): programatically set a good x axis size
				cf.rcArea = { state.caret.pos.x , state.caret.pos.y + state.caret.dim.cy,state.caret.pos.x + 100,state.caret.pos.y + state.caret.dim.cy + 100 };
			}
			else {
				cf.rcArea = { 0,0,0,0 };
				cf.ptCurrentPos.x = 0;
				cf.ptCurrentPos.y = 0;
			}
		}
		else {
			cf.dwStyle = CFS_RECT | CFS_FORCE_POSITION;
			cf.ptCurrentPos.x = state.caret.pos.x;//INFO: it does _not_ respect x that goes beyond the control
			cf.ptCurrentPos.y = state.caret.pos.y + state.caret.dim.cy;//INFO: it does _not_ respect y that goes beyond the control
			//TODO(fran): programatically set a good x axis size
			cf.rcArea = rectWH(cf.ptCurrentPos.x, cf.ptCurrentPos.y, 1, 1);//INFO: it does _not_ respect w & h, if w or h is too small the window isnt shown, otherwise it's shown in one unique size
#if 0 //this doesnt work, candidate window still shows up
			CANDIDATEFORM cnf;
			cnf.dwIndex = 0;
			cnf.dwStyle = CFS_EXCLUDE;
			cnf.ptCurrentPos = { GetSystemMetrics(SM_CXVIRTUALSCREEN) + 1,GetSystemMetrics(SM_CYVIRTUALSCREEN) + 1 };
			cnf.rcArea = rectWH(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN) + 500, GetSystemMetrics(SM_CYVIRTUALSCREEN) + 500);
			ImmSetCandidateWindow(imc, &cnf); //this one seems to be the list!
#endif
			//ShowWindow(ImmGetDefaultIMEWnd(state.wnd),SW_HIDE); //maybe I can hide the window? noup
			//ImmSetStatusWindowPos //no idea what this is
		}
#endif
		BOOL res = ImmSetCompositionWindow(imc, &cf); Assert(res);
	}
}

//TODO(fran): the fact that true is hidden and false is shown is very unintuitive and prone to error, reverse it
void set_IME_wnd(HWND wnd, bool hidden) {
	State& state = *get_state(wnd);
	if (&state) {
		state.hide_IME_wnd = hidden;
	}
}

void set_composition_font(State& state)//TODO(fran): this must set the other stuff too
{
	HIMC imc = ImmGetContext(state.wnd);
	if (imc != NULL) {
		defer{ ImmReleaseContext(state.wnd, imc); };
		LOGFONT lf;
		int getobjres = GetObject(state.theme.font, sizeof(lf), &lf); Assert(getobjres == sizeof(lf));

		BOOL setres = ImmSetCompositionFont(imc, &lf); Assert(setres);
	}
}

void notify_parent(State& state, WORD notif_code) {
	PostMessage(state.parent, WM_COMMAND, MAKELONG(state.identifier, notif_code), (LPARAM)state.wnd);
}

bool is_placeholder_visible(State& state) {
	bool res = (state.char_text.length() == 0) && *state.placeholder && (GetFocus() != state.wnd || state.maintain_placerholder_on_focus);
	return res;
}

void maintain_placerholder_when_focussed(HWND wnd, bool maintain) {//TODO(fran): idk if this should be standardized and put in the wparam of WM_SETDEFAULTTEXT, it seems kind of annoying especially since this is something you probably only want to set once, different from the text which you may want to change more often
	State& state = *get_state(wnd);
	if (&state) {
		state.maintain_placerholder_on_focus = maintain;
		ask_window_for_repaint(state.wnd);//TODO(fran): only ask for repaint when it's actually necessary
	}
}

struct _string_traversal { size_t p; bool reached_limit; };//NOTE: string::npos seems worse since it doesnt give you a valid last pos

_string_traversal skip_whitespace(utf16_str s, size_t start_p, int direction) {
	size_t last_valid_i;
	for (size_t i = start_p; i < s.sz_char(); i += direction) {
		last_valid_i = i;
		if (!iswspace(s[i])) {
			return { i,false };
		}
	}
	return { last_valid_i,true };
}

//goes to first character in word or punctuation group (skips whitespaces)
//TODO(fran): what to do if we're already at the start of a group?
_string_traversal goto_start_of_group(utf16_str s, size_t start_p, int direction) {
	Assert(direction < 0);

	auto [x, reached_limit] = skip_whitespace(s, start_p, direction);
	if (reached_limit) return { x, reached_limit };

	bool is_punct = iswpunct(s[x]);//can either be punctuation or alphanumeric

	//go to the start of the previous word or punctuation 'group'
	size_t last_valid_j;
	for (size_t j = x; j < s.sz_char(); j += direction) {
		if ((is_punct ? !iswpunct(s[j]) : !iswalnum(s[j]))) return { j + 1,false };
		last_valid_j = j;
	}
	return { last_valid_j,true };
}

//goes one past the last character in word or punctuation group (skips whitespaces)
//TODO(fran): what to do if we're already at the end of a group?
_string_traversal goto_end_of_group(utf16_str s, size_t start_p, int direction) {
	Assert(direction > 0);

	auto [x, reached_limit] = skip_whitespace(s, start_p, direction);
	if (reached_limit) return { x, reached_limit };

	bool is_punct = iswpunct(s[x]);//can either be punctuation or alphanumeric

	//go to the end of the word or punctuation 'group'
	size_t last_valid_j;
	for (size_t j = x; j < s.sz_char(); j += direction) {
		if ((is_punct ? !iswpunct(s[j]) : !iswalnum(s[j]))) return { j,false };
		last_valid_j = j;
	}
	return { last_valid_j,true };
}

//finds different points where to stop when traversing a string, used for Ctrl+Left/Right Arrows keycombo
size_t find_stopper(utf16_str s, size_t start_p, int direction/*should be +1 or -1*/) {
	Assert(direction);
	//TODO(fran): handle overflow

	//TODO(fran): this doesnt work exactly like we'd like for Ctrl+ Right arrow, we fail to skip to the next word instead stopping at the last character of the current one, and when we do (by placing the cursor past the last character of the word) we go past to the end of that next word instead of stopping at the beginning of it. Extra: Actually I do like that it stops at the end of words (this is not the normal behaviour but I like it more), what is wrong is the second case, whereby starting from a whitespace & going right it skips to the end of the next word instead of stopping at the beginning

	start_p = clamp((decltype(start_p))0, start_p, s.cnt());

	if (iswspace(s[start_p]) || iswcntrl(s[start_p])) {//we're on a whitespace
		//find first non whitespace in the direction
		size_t last_valid_i;
		for (size_t i = start_p; i < s.sz_char(); i += direction) {
			last_valid_i = i;
			if (!iswspace(s[i]) && !iswcntrl(s[i])) {
				//found a non whitespace char, now go to the beginning/end of that new thing

				bool is_punct = iswpunct(s[i]);//can either be punctuation or alphanumeric

				size_t last_valid_j;
				for (size_t j = i; j < s.sz_char(); j += direction) {
					if ((is_punct ? !iswpunct(s[j]) : !iswalnum(s[j]))) return direction >= 0 ? j : j + 1;
					last_valid_j = j;
				}

				return last_valid_j;
			}
		}
		return last_valid_i;
	}
	else {

		if (iswpunct(s[start_p])) {//we're on a punctuation mark
			//TODO(fran):it seems like everybody does smth different with punctuation, look at visual studio & sublime for examples, so just find what I feel works best

			if (size_t i = start_p; direction < 0 && !iswpunct(s[--i])) {//if going left and we're on the first character of the punctuation group

				//go to first character in previous word or punctuation group
				auto [x, _] = goto_start_of_group(s, i, direction);
				return x;
			}
			else {
				//find first non punctuation in the direction
				for (size_t i = start_p; i < s.sz_char(); i += direction) {
					if (!iswpunct(s[i])) {
						return i;
					}
					//TODO(fran): same fix as in whitespace
				}
			}
		}
		else {//we're on a word
			//find first non word in the direction 
			//TODO(fran): what about langs that dont usually separate words, like japanese? looks pretty hard since you'd actually have to comprehend the text to understand where to cut each word

			if (direction > 0) {//if going right
				for (size_t i = start_p; i < s.sz_char(); i += direction) {
					if (!iswalnum(s[i])) {//find first character not in the current word
						return i;
					}
					//TODO(fran): same fix as in whitespace
				}
			}
			else {//if going left

				if (size_t i = start_p; i > 0 && !iswalnum(s[--i])) {//if we're at the first character of the word

					//we go one back (--i)

					if (iswpunct(s[i])) {//if we're on punctuation
						//go to the start of the punctuation 'group' //TODO(fran): make this things into separate functions for reuse
						size_t last_valid_j;
						for (size_t j = i; j < s.sz_char(); j += direction) {
							if (!iswpunct(s[j])) return j + 1;
							last_valid_j = j;
						}
						return last_valid_j;
					}
					else {//else we're on a whitespace
						//skip all whitespaces and go to the start of the previous word or punctuation 'group'
						auto [x, _] = goto_start_of_group(s, i, direction);
						return x;
					}

				}
				else {//else find first character of the word

					size_t last_valid_i;//TODO(fran): initialize?
					for (size_t i = start_p; i < s.sz_char(); i += direction) {
						last_valid_i = i;
						if (!iswalnum(s[i])) {//find first character not in the current word
							return i + 1;
						}
					}
					return last_valid_i;//if for example we get to the start of the string then stop there
				}
			}
		}


	}

	return start_p;
}
size_t find_next_stopper(utf16_str s, size_t start_p) {
	return find_stopper(s, start_p, +1);
}
size_t find_prev_stopper(utf16_str s, size_t start_p) {
	return find_stopper(s, start_p, -1);
}

//Positive direction moves cursor/selection to the right, negative to the left
void move_selection(State& state, int direction, bool shift_is_down, bool ctrl_is_down) {
	Assert(direction == 1 || direction == -1);
	size_t anchor, cursor;

	auto new_cursor = [&]() {
		auto res = clamp(
			(char_sel::type)0,
			state.selection.cursor + ((state.selection.cursor == 0 && direction == -1) ? 0 : direction) /*TODO(fran): safe_add(a, b, ret_on_underflow, ret_on_overflow)*/,
			state.char_text.length());
		return res;
		};

	if (shift_is_down && ctrl_is_down) {
		anchor = state.selection.anchor;
		cursor = find_stopper(to_utf_str(state.char_text), state.selection.cursor, clamp(-1, direction, +1));
	}
	else if (shift_is_down) {
		//User is adding one extra character to the selection
		anchor = state.selection.anchor;
		//cursor = clamp((char_sel::type)0, state.selection.cursor + ((state.selection.cursor == 0 && direction == -1) ? 0 : direction), state.char_text.length());
		cursor = new_cursor();
	}
	else if (ctrl_is_down) {
		anchor = cursor = find_stopper(to_utf_str(state.char_text), state.selection.cursor, clamp(-1, direction, +1));
	}
	else {
		if (state.selection.has_selection()) anchor = cursor = ((direction >= 0) ? state.selection.x_max() : state.selection.x_min());
		//else anchor = cursor = clamp((char_sel::type)0, state.selection.cursor + ((state.selection.cursor==0 && direction == -1) ? 0 : direction), state.char_text.length());
		else anchor = cursor = new_cursor();
	}

	SendMessage(state.wnd, EM_SETSEL, anchor, cursor);
}

/* TODO(fran): complete the implementation of this guys

//Examples:
// "Hello how are u doing\nFine" -> first_char_idx_past_line_idx(-1)      -> |Hello
// "Hello how are u doing\nFine" -> first_char_idx_past_line_idx(n >= 0)  -> doing\n|Fine
// "Hello how are u doing\n"     -> first_char_idx_past_line_idx(0)       -> \n|
//Important: as we can see the value returned can go past the last character of the string, and thus go over the size of arrays, therefore the value should be iterated up to but not including itself
size_t first_char_idx_past_line_idx(State& state, size_t line_idx) {
	size_t res;
	if (line_idx == (size_t)-1) res = 0; //TODO(fran): start using i64 so we can encode the hidden first line at 0
	else if (line_idx >= state.line_breaks.size()) res = state.char_text.size(); //char can be past the end of text, this will cause crashes because I dont think most arrays dependent on char_idx handle that case, TODO(fran): see what to do about that
	else res = state.line_breaks[line_idx] + 1; //again, can be past the end of text, and could even not be a valid character for that line if the line is just made of a single \n, which leads me to believe the user should go up to but not including first_char_idx
	return res;
}


//Cursor will be _behind_ the last letter of the line
//REMEMBER: line indexes are part of the line
//				eg "Hello how are u doing\nFine" -> last_char_idx_before_line_idx(0 or -1) will place the cursor in doin|g
//												    last_char_idx_before_line_idx(n>0) will place the cursor in Fin|e
//				eg "Hello how are u doing"		 -> last_char_idx_before_line_idx(any number) will place the cursor in doin|g
//Idx will be at the last letter of the line, state.char_text[idx] & state.char_dims[idx] is valid //TODO(fran): except for when char_text is empty
size_t last_char_idx_before_line_idx(State& state, size_t line_idx) {
	size_t res;
	if (line_idx == (size_t)-1) res = state.line_breaks.size() ? safe_subtract0(state.line_breaks[0], 1) : safe_subtract0(state.char_text.size(), 1); //cursor will be behind the last letter of the line
	else if (line_idx >= state.line_breaks.size()) res = safe_subtract0(state.char_text.size(),1);
	else res = safe_subtract0(state.line_breaks[line_idx], 1);
	return res; //TODO(fran): check that we dont move to the previous line, eg in the case of Hello\n\n -> last_char_idx_before_line_idx(1) should map to -> Hello\n|\n
}

//Cursor will be _after_ the last letter of the line
//				eg "Hello how are u doing\nFine" -> one_past_last_char_idx_before_line_idx(0 or -1) will place the cursor in doing|
//												    one_past_last_char_idx_before_line_idx(n>0) will place the cursor in Fine|
//				eg "Hello how are u doing"		 -> one_past_last_char_idx_before_line_idx(any number) will place the cursor in doing|
//				eg "Hello how are u doing\nFine" -> one_past_last_char_idx_before_line_idx(0 or -1) will place the cursor in doing|
size_t one_past_last_char_idx_before_line_idx(State& state, size_t line_idx) {
	size_t res = last_char_idx_before_line_idx(state, line_idx);
	res = minimum(res + 1, state.char_text.size());
	return res;

}

*/

//TODO(fran): combine with move_selection by sending a v2_i32 direction?
void move_selection_vertical(State& state, int direction, bool shift_is_down, bool ctrl_is_down) {
	Assert(direction == 1 || direction == -1);
	size_t anchor, cursor;

	auto new_cursor = [&]() {
		size_t res = 0;

		auto [line_char_idx, line_idx] = char_idx_to_line_char_idx(state, state.selection.cursor);

		//Special cases where it acts like pressing Home and End
		if (!state.line_breaks.size() || (direction > 0 && line_idx >= state.line_breaks.size() || (direction < 0 && line_char_idx == 0))) {
			res = direction > 0 ? state.char_text.size() : 0;
			return res;
		}

		f32 current_line_width = 0;
		for (auto i = line_char_idx; i < state.selection.cursor; i++)
			current_line_width += state.char_dims[i];

		if (state.vertical_selection_stored_width > current_line_width) current_line_width = state.vertical_selection_stored_width;
		else state.vertical_selection_stored_width = current_line_width;

		size_t target_line = (direction > 0) ? line_idx + direction : safe_subtract0(line_idx, abs(direction));

		//TODO(fran): encode this repeating concepts of safe_subtract, finding line ends and so on into specific functions to make this easier to code and understand

		auto idx_end = target_line < state.line_breaks.size() ? state.line_breaks[target_line] : state.char_text.size();
		auto idx_start = char_idx_to_line_char_idx(state, idx_end).line_char_idx;

		f32 test_width = 0;
		auto i = idx_start;

		for (; i < idx_end; i++) {
			f32 d = (f32)state.char_dims[i] / 2.f;
			test_width += d;
			if (test_width >= current_line_width) { res = i; return res; }
			test_width += d;
		}

		if (i == idx_end) res = idx_end;

		return res;
		};

	if (shift_is_down && ctrl_is_down) {
		return;
		//we do nothing, we dont have anything mapped to that. As a note Sublime switches lines when you do this, which is the same that Visual Studio does when pressing Alt + Up/Down. TODO(fran): create a key-to-action mapping table that can be user editable
	}
	else if (shift_is_down) {
		//User is adding one extra character to the selection
		anchor = state.selection.anchor;
		cursor = new_cursor();
	}
	else if (ctrl_is_down) {
		return;
		//TODO(fran): scroll one line up or down
	}
	else {
		if (state.selection.has_selection()) anchor = cursor = ((direction >= 0) ? state.selection.x_max() : state.selection.x_min()); //TODO(fran): I dont think this is what we want, we still want to move to the next or previous line that comes after or before the current selection block
		//else anchor = cursor = clamp((char_sel::type)0, state.selection.cursor + ((state.selection.cursor==0 && direction == -1) ? 0 : direction), state.char_text.length());
		else anchor = cursor = new_cursor();
	}

	SendMessage(state.wnd, EM_SETSEL, anchor, cursor);
}

void select_all(State& state) {
	SendMessage(state.wnd, EM_SETSEL, 0, (size_t)-1);
}

size_t point_to_char(State& state, POINT mouse/*client coords*/) {
	size_t res = 0;
	f32 x = (decltype(x))state.padding.x;

	u64 line = clamp((u64)0, (u64)(mouse.y - state.padding.y + state.scroll.y) / calc_line_height_px(state), state.line_breaks.size());

	//IMPORTANT(fran): Line mapping is as follows:
	//					line = 0 -> "line 0" -> char_idx = 0                
	//						: we manually go to the start of the string cause we dont store the first line in line_breaks
	//					line > 0 -> char_idx = minimum(line_breaks[line - 1] + 1, state.char_text.size()) 
	//						: line 1 maps to idx 0 in line_breaks, and so on, after that we add +1 to go to the first character past the \n
	//					      and min clamp in case \n is the last character of the string

	u64 i = !line ? 0 : minimum(state.line_breaks[line - 1] + 1/*+1 to go to the first character past the \n*/, state.char_text.size());
	u64 line_end = line < state.line_breaks.size() ? state.line_breaks[line] : state.char_text.size();

	for (; i < line_end; i++) {
		f32 d = (f32)state.char_dims[i] / 2.f;
		x += d;
		if ((f32)mouse.x + state.scroll.x < x) { res = i; break; }
		x += d;
	}
	if (i == line_end) res = i;//if the mouse is rightmost of the last character then simply clip to there
	return res;
}

//NOTE: Windows had the brilliant idea of stopping the caret from blinking after a couple of seconds, and the only real fix is via a registry hack, therefore we take the poor man's approach, a 5 sec timer that reblinks the caret
void keep_caret_blinking(State& state) {
	SetTimer(state.wnd, EDITONELINE_caret_timer_id, EDITONELINE_default_caret_duration, NULL);
}

void stop_caret_blinking(State& state) {
	KillTimer(state.wnd, EDITONELINE_caret_timer_id);
}

//Renders the selection box corresponding to only one line
void render_selection(HDC dc, HBRUSH brush, char_sel sel, State& state, int yPos, size_t line_start) {
	Assert(sel.has_selection());
	RECT selection;
	selection.top = yPos;
	selection.bottom = selection.top + state.caret.dim.cy;
	selection.left = state.padding.x;
	for (size_t i = line_start; i < sel.x_min(); i++)
		selection.left += state.char_dims[i];
	selection.right = selection.left;
	for (size_t i = sel.x_min(); i < sel.x_max(); i++)
		selection.right += state.char_dims[i];
	//FillRect(dc, &selection, selection_br);
	COLORREF sel_col = ColorFromBrush(brush);
	urender::FillRectAlpha(dc, selection, GetRValue(sel_col), GetGValue(sel_col), GetBValue(sel_col), 128);
	//TODO(fran): benchmark whether doing all the calculations and only rendering one polygon is faster, in which case render_selection would have to take the entire (multiline) selection and convert it into one big polygon
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//static int __c; printf("%d:EDITONELINE:%s\n",__c++, msgToString(msg));

	State& state = *get_state(hwnd); _control_validate_state;
	switch (msg) {
	case WM_NCCREATE:
	{ //1st msg received
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		Assert(!(creation_nfo->style & ES_RIGHT));//TODO(fran)

		auto st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		st->wnd = hwnd;
		st->parent = creation_nfo->hwndParent;
		st->identifier = (u32)(UINT_PTR)creation_nfo->hMenu;
		st->char_max_sz = -1;//default established by windows: 32767
		*st->placeholder = 0;
		st->char_text = str();//REMEMBER: this c++ objects dont like being calloc-ed, they need their constructor, or, in this case, someone else's, otherwise they are badly initialized
		st->char_dims = std::vector<int>();
		st->line_breaks = decltype(st->line_breaks)();

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
		state.controls.tooltip = CreateWindowEx(WS_EX_TOPMOST/*make sure it can be seen in any wnd config*/, TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,//INFO(fran): TTS_BALLOON doesnt work on windows10, you need a registry change https://superuser.com/questions/1349414/app-wont-show-balloon-tip-notifications-in-windows-10 simply amazing
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			state.wnd, NULL, NULL, NULL);
		Assert(state.controls.tooltip);

		TOOLINFO toolInfo{ sizeof(toolInfo) };
		toolInfo.hwnd = state.wnd;
		toolInfo.uFlags = TTF_IDISHWND | TTF_ABSOLUTE | TTF_TRACK /*allows to show the tooltip when we please*/; //TODO(fran): TTF_TRANSPARENT might be useful, mouse msgs that go to the tooltip are sent to the parent aka us
		toolInfo.uId = (UINT_PTR)state.wnd;
		toolInfo.rect = { 0 };
		toolInfo.hinst = GetModuleHandle(NULL);
		toolInfo.lpszText = 0;
		toolInfo.lParam = 0;
		toolInfo.lpReserved = 0;
		BOOL addtool_res = (BOOL)SendMessage(state.controls.tooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
		Assert(addtool_res);

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SIZE: {//4th, strange, I though this was sent only if you didnt handle windowposchanging (or a similar one)
		//NOTE: neat, here you resize your render target, if I had one or cared to resize windows' https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-size
		//This msg is received _after_ the window was resized
		update_char_pad(state);
		reposition_caret(state, true);

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
		BOOL show = (BOOL)wparam;
		if (!show) {
			//We're going to be hidden, we must also hide our tooltips
			SendMessage(state.wnd, WM_TIMER, EDITONELINE_tooltip_timer_id, 0); //TODO(fran): #robustness, we should know which timers are active to only disable those, also replace this crude SendMessage with a hide_tooltips()
		}
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
		state.char_max_sz = (u32)wparam;
		return 0;
	} break;
	case WM_SETFONT:
	{
		HFONT font = (HFONT)wparam;

		Theme new_theme {.font = font };
		set_theme(state.wnd, new_theme);

		return 0;
	} break;
	case WM_DESTROY:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCDESTROY://Last msg. Sent _after_ WM_DESTROY
	{
		stop_caret_blinking(state);//TODO(fran): not sure I need this
		if (state.caret.bmp) {
			DeleteBitmap(state.caret.bmp);
			state.caret.bmp = 0;
		}
		state.char_dims.~vector();
		state.char_text.~basic_string();
		state.line_breaks.~vector();
		set_window_state(state.wnd, nil);
		free(&state);
		return 0;
	}break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		//ps.rcPaint
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };

		RECT rc; GetClientRect(state.wnd, &rc);
		int w = RECTW(rc), h = RECTH(rc);

		LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
		bool show_placeholder = is_placeholder_visible(state);
		bool is_enabled = IsWindowEnabled(state.wnd);

		u32 border_thickness = state.theme.dimensions.border_thickness;

		HBRUSH bk_br, txt_br, border_br, selection_br;
		HFONT font = state.theme.font;

		if (is_enabled) {
			bk_br = state.theme.brushes.bk.normal;
			txt_br = state.theme.brushes.foreground.normal;
			border_br = state.theme.brushes.border.normal;
			selection_br = state.theme.brushes.selection.normal;
		}
		else {
			bk_br = state.theme.brushes.bk.disabled;
			txt_br = state.theme.brushes.foreground.disabled;
			border_br = state.theme.brushes.border.disabled;
			selection_br = state.theme.brushes.selection.disabled;
		}
		if (show_placeholder) {
			txt_br = state.theme.brushes.placeholder.normal;
			font = fonts.General; //TODO(fran): add placeholder specific font in theme
		}

		urender::draw_background(dc, rc, bk_br, border_br, state.theme.dimensions);

		//TODO(fran): clip text rendering inside drawing area of the background and border (must take into account rounded borders as well)
		{
			HFONT oldfont = SelectFont(dc, font); defer{ SelectFont(dc, oldfont); };
			UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };

			COLORREF oldtxtcol = SetTextColor(dc, ColorFromBrush(txt_br)); defer{ SetTextColor(dc, oldtxtcol); };
			auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };

			TEXTMETRIC tm; GetTextMetrics(dc, &tm);
			// Calculate vertical position for the string so that it will be vertically centered
			// We are single line so we want vertical alignment always
			//TODO(fran): allow the user to select the vertical alignment (top, center, bottom)
			//TODO(fran): BUG: vertical centering doesnt automatically occur when going from 2 lines to 1 by deleting the \n character, it remains with top centering until another letter is written by the user
			int yPos = state.padding.y;
			int xPos;

			//TODO(fran): create clip region so the text cant go over the border

			//TODO(fran): continue exploring world transformations for scrolling, 
			POINT old_origin; SetViewportOrgEx(dc, -state.scroll.x, -state.scroll.y, &old_origin); defer{ SetViewportOrgEx(dc, old_origin.x, old_origin.y, 0); };
			//NOTE: even if you dont reset the origin back to 0,0 windows automatically does

			{ //Render Selection
				if (state.selection.has_selection() && (style & ES_PASSWORD)) {
					render_selection(dc, selection_br, state.selection, state, yPos, 0);
				}
			}

			if (style & ES_CENTER) {
				SetTextAlign(dc, TA_CENTER);
				xPos = (rc.right - rc.left) / 2;
			}
			else if (style & ES_RIGHT) {
				SetTextAlign(dc, TA_RIGHT);
				xPos = rc.right - state.padding.x;
			}
			else /*ES_LEFT*/ {//NOTE: ES_LEFT==0, that was their way of defaulting to left
				SetTextAlign(dc, TA_LEFT);
				xPos = rc.left + state.padding.x;
			}

			if (show_placeholder) {
				TextOut(dc, xPos, yPos, state.placeholder, (int)cstr_len(state.placeholder));
			}
			else if (style & ES_PASSWORD) { //TODO(fran): ES_PASSWORD should only be taken into account for single-line edit controls
				//TODO(fran): benchmark: full allocation vs for loop drawing characters one by one
				cstr* pass_text = (cstr*)malloc(state.char_text.length() * sizeof(cstr)); defer{ free(pass_text); };
				for (size_t i = 0; i < state.char_text.length(); i++)pass_text[i] = password_char;

				TextOut(dc, xPos, yPos, pass_text, (int)state.char_text.length());
			}
			else {
				//TODO(fran): make a common path for all three and allow the placeholder to also have linebreaks, what we could do is have another variable that stores the currently used text, if placeholder is active it'll point to the placeholder, otherwise it'll point to the password if password style is active, or to the normal text

				//TextOut(dc, xPos, yPos, state.char_text.c_str(), (int)state.char_text.length());

				state.line_breaks.push_back(state.char_text.length()); defer{ state.line_breaks.pop_back(); }; //TODO(fran): find better solution to render the last line, which potentially doesnt have a line break
				const u64 lines = state.line_breaks.size();
				u64 off = 0;
				for (u64 i = 0; i < lines; i++) { //TODO(fran): @speed: only render lines visible on screen
					const size_t len = state.line_breaks[i];

					{
						const size_t start = off, end = len + 1 /*+1 so the user can visually select the \n character at the end of the line*/;
						char_sel selection{ clamp(start,state.selection.x_min(),end), clamp(start,state.selection.x_max(),end) };
						if (selection.has_selection())
							render_selection(dc, selection_br, selection, state, yPos, start);
					}
					//TODO(fran): use ExtTextOut which has support for font fallback
					TextOut(dc, xPos, yPos, state.char_text.c_str() + off, (int)(len - off));
					off = len + 1;
					yPos += tm.tmHeight;
					//TODO(fran): all alignment related code needs to be re-done
						//NOTE: I could cheat by simply using DrawText but I think this will be more insightful
				}
			}
		}

		return 0;
	} break;
	case WM_DESIRED_SIZE:
	{
		SIZE* min = (decltype(min))wparam;
		SIZE* max = (decltype(max))lparam;
		min->cy = maximum(min_height(state), (i32)min->cy);
		max->cy = minimum(maximum(desired_height(state), (i32)min->cy), (i32)max->cy);

		//TODO(fran): this needs to be redone and we should return smth like this: struct{padding, increment, increment_cnt}
		// where increment basically indicates the size of a line, and increment_cnt the number of desired lines.
		// Or obviously even better would be if we, as the edit control, could manually select our size based on limitations imposed by our creator

		return desired_size::flexible;
	} break;
	case WM_STYLECHANGED:
	{
		auto style_change = wparam == GWL_STYLE;
		if (style_change) {
			auto cmp = (STYLESTRUCT*)lparam;
			if ((cmp->styleOld & ES_PASSWORD) != (cmp->styleNew & ES_PASSWORD)) {
				// style changed between using/not using password view
				recalculate_char_dims(state);
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_ENABLE:
	{//Here we get the new enabled state
		BOOL now_enabled = (BOOL)wparam;
		InvalidateRect(state.wnd, NULL, TRUE);
		//TODO(fran): Hide caret
		return 0;
	} break;
	case WM_CANCELMODE:
	{
		//Stop mouse capture, and similar input related things
		ReleaseCapture();
		state.on_mouse_tracking = false;
		//Sent, for example, when the window gets disabled
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCHITTEST:
	{
		return handle_wm_nchittest(state.wnd, lparam);
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
			
		return handle_wm_setcursor(hwnd, msg, wparam, lparam, (HCURSOR)GetClassLongPtr(hwnd, GCLP_HCURSOR));
	} break;
	case WM_MOUSEMOVE /*WM_MOUSEFIRST*/://When the mouse goes over us this is 3rd msg received
	{
		//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

		if (state.on_mouse_tracking) {
			//user is trying to make a selection
			size_t cursor = point_to_char(state, mouse);
			SendMessage(state.wnd, EM_SETSEL, state.selection.anchor, cursor);
		}

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEACTIVATE://When the user clicks on us this is 1st msg received (any click: left, right, etc)
	{
		HWND parent = (HWND)wparam;
		WORD hittest = LOWORD(lparam);
		WORD mouse_msg = HIWORD(lparam);
		SetFocus(state.wnd);//set keyboard focus to us
		reset_vertical_selection_stored_width(state);
		return MA_ACTIVATE; //Activate our window and post the mouse msg
	} break;
	case WM_LBUTTONDOWN://When the user clicks on us this is 2nd msg received (possibly, maybe wm_setcursor again)
	{
		//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

		SetCapture(state.wnd);//We want to keep capturing the mouse while the user is still pressing some button, even if the mouse leaves our client area
		state.on_mouse_tracking = true;

		size_t cursor = point_to_char(state, mouse);
		SendMessage(state.wnd, EM_SETSEL, cursor, cursor);

		return 0;
	} break;
	case WM_LBUTTONDBLCLK:
	{
		//TODO(fran): select entire word
		auto text = to_utf_str(state.char_text);
		//HACK: We assume the mouse hasnt moved much since the first click, therefore we do not need to check the mouse position to find out where the cursor is since the first click already set the cursor position
		auto left = find_stopper(text, state.selection.cursor + 1, -1);
		auto right = find_stopper(text, state.selection.cursor ? state.selection.cursor - 1 : 0, +1);
		SendMessage(state.wnd, EM_SETSEL, left, right);
	} break;
	case WM_LBUTTONUP:
	{
		ReleaseCapture();
		state.on_mouse_tracking = false;
	} break;
	case WM_SETFOCUS://SetFocus -> WM_IME_SETCONTEXT -> WM_SETFOCUS
	{
		//We gained keyboard focus
		SIZE caret_dim = calc_caret_dim(state);
		if (caret_dim.cx != state.caret.dim.cx || caret_dim.cy != state.caret.dim.cy) {
			if (state.caret.bmp) {
				DeleteBitmap(state.caret.bmp);
				state.caret.bmp = 0;
			}
			//TODO(fran): we should probably check the bpp of our control's img
			int pitch = caret_dim.cx * 4/*bytes per pixel*/;//NOTE: windows expects word aligned bmps, 32bits are always word aligned
			int sz = caret_dim.cy * pitch;
			u32* mem = (u32*)malloc(sz); defer{ free(mem); };
			COLORREF color = ColorFromBrush(state.theme.brushes.foreground.disabled);

			//IMPORTANT: changing caret color is gonna be a pain, docs: To display a solid caret (NULL in hBitmap), the system inverts every pixel in the rectangle; to display a gray caret ((HBITMAP)1 in hBitmap), the system inverts every other pixel; to display a bitmap caret, the system inverts only the white bits of the bitmap.
			//Aka we either calculate how to arrive at the color we want from the caret's bit flipping (will invert bits with 1) or we give up, we should do our own caret
			//NOTE: since the caret is really fucked up we'll average the pixel color so we at least get grays
			//TODO(fran): decide what to do, the other CreateCaret options work as bad or worse so we have to go with this, do we create a separate brush so the user can mix and match till they find a color that blends well with the bk?
			u8 gray = ((u16)(u8)(color >> 16) + (u16)(u8)(color >> 8) + (u16)(u8)color) / 3;
			color = RGB(gray, gray, gray);

			int px_count = sz / 4;
			for (int i = 0; i < px_count; i++) mem[i] = color;
			state.caret.bmp = CreateBitmap(caret_dim.cx, caret_dim.cy, 1, 32, (void*)mem);
			state.caret.dim = caret_dim;
		}
		BOOL caret_res = CreateCaret(state.wnd, state.caret.bmp, 0, 0);
		Assert(caret_res);

		reposition_caret(state, true);

		BOOL showcaret_res = ShowCaret(state.wnd);
		Assert(showcaret_res);

		//We ask for repainting so placeholders and other focus dependent elements con be re-renderered o hidden
		ask_window_for_repaint(state.wnd);

		keep_caret_blinking(state);

		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		//We lost keyboard focus
		//NOTE(fran): docs says to never display/activate a window here cause we can lock the thread

		DestroyCaret();

		//We ask for repainting so placeholders and other focus dependent elements con be re-renderered o hidden
		ask_window_for_repaint(state.wnd);

		PostMessage(GetParent(state.wnd), WM_COMMAND, MAKELONG(state.identifier, EN_KILLFOCUS), (LPARAM)state.wnd);

		stop_caret_blinking(state);

		return 0;
	} break;
	case WM_KEYDOWN://When the user presses a non-system key this is the 1st msg we receive
	{
		//Notifications:
		bool en_change = false;

		//Vertical Selection Stored Width:
		bool reset_v_sel_stored_w = true;

		//TODO(fran): here we process things like VK_HOME VK_NEXT VK_LEFT VK_RIGHT VK_DELETE
		//NOTE: there are some keys that dont get sent to WM_CHAR, we gotta handle them here, also there are others that get sent to both, TODO(fran): it'd be nice to have all the key things in only one place
		//NOTE: for things like _shortcuts_ you wanna handle them here cause on WM_CHAR things like Ctrl+V get translated to something else
		//		also you want to use the uppercase version of the letter, eg case _t('V'):
		char vk = (char)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		bool shift_is_down = HIBYTE(GetKeyState(VK_SHIFT));
		//printf("%c : %d\n", vk, (i32)vk);
		switch (vk) {
		case VK_HOME://Home //TODO(fran): @multiline
		{
			size_t anchor, cursor;

			if (shift_is_down && ctrl_is_down) {//Make a selection to the start of the text
				anchor = state.selection.anchor;
				cursor = 0;
			}
			else if (shift_is_down) {//Make a selection to the start of the line
				anchor = state.selection.anchor;
				cursor = char_idx_to_line_char_idx(state, state.selection.cursor).line_char_idx;
				//cursor = 0;
			}
			else if (ctrl_is_down) {//Remove selection and Go to the start of the text
				anchor = cursor = 0;
			}
			else {//Remove selection and Go to the start of the line (since we're singleline go to start of text)
				//anchor = cursor = 0;
				anchor = cursor = char_idx_to_line_char_idx(state, state.selection.cursor).line_char_idx;
			}

			SendMessage(state.wnd, EM_SETSEL, anchor, cursor);
		} break;
		case VK_END://End //TODO(fran): @multiline
		{
			size_t anchor, cursor;

			auto last_cursor_in_line = [](State& state) {
				auto line_idx = char_idx_to_line_char_idx(state, state.selection.cursor).line_idx;
				return line_idx < state.line_breaks.size() ? state.line_breaks[line_idx] : state.char_text.length();
				};

			if (shift_is_down && ctrl_is_down) {//Make a selection to the end of the text
				anchor = state.selection.anchor;
				cursor = state.char_text.length();
			}
			else if (shift_is_down) {//Make a selection to the end of the line
				anchor = state.selection.anchor;
				//cursor = state.char_text.length();
				cursor = last_cursor_in_line(state);
			}
			else if (ctrl_is_down) {//Remove selection and Go to the end of the text
				anchor = cursor = state.char_text.length();
			}
			else {//Remove selection and Go to the end of the line (since we're singleline go to start of text)
				//anchor = cursor = state.char_text.length();
				anchor = cursor = last_cursor_in_line(state);
			}

			SendMessage(state.wnd, EM_SETSEL, anchor, cursor);
		} break;
		case VK_LEFT://Left arrow
		{
			move_selection(state, -1, shift_is_down, ctrl_is_down);
		} break;
		case VK_RIGHT://Right arrow
		{
			move_selection(state, +1, shift_is_down, ctrl_is_down);
		} break;
		case VK_DOWN://Down arrow
		{
			move_selection_vertical(state, +1, shift_is_down, ctrl_is_down);
			reset_v_sel_stored_w = false;
		} break;
		case VK_UP://Up arrow
		{
			move_selection_vertical(state, -1, shift_is_down, ctrl_is_down);
			reset_v_sel_stored_w = false;
		} break;
		//TODO(fran): do we want to update scrolling to the left when deleting causes the text to get shorter than the right side of the element?
		case VK_DELETE://What in spanish is the "Supr" key (delete character ahead of you)
		{
			if (!state.char_text.empty()) {
				if (state.selection.has_selection())remove_selection(state);
				else {
					if (ctrl_is_down && shift_is_down) {//delete everything til end of the line
						remove_selection(state, state.selection.cursor, state.char_text.length());
					}
					else if (ctrl_is_down) {//delete everything up to the next stopper
						remove_selection(state, state.selection.cursor, find_stopper(to_utf_str(state.char_text), state.selection.cursor, +1));
					}
					else if (shift_is_down) {//save whole line to clipboard and then delete it
						SendMessage(state.wnd, EM_SETSEL, 0, -1);
						SendMessage(state.wnd, WM_COPY, 0, 0);
						remove_selection(state);
					}
					else remove_selection(state, state.selection.cursor, state.selection.cursor + 1);//delete character in front of the cursor
				}
				en_change = true;
			}
		} break;
		case VK_BACK://Backspace
		{
			//TODO(fran): @feature: if an accent has been selected the delete should cancel the accent instead of deleting a character
			//		eg: keyboard sequence: a´a -> aá ; a´backspace -> a
			if (!state.char_text.empty()) {
				if (state.selection.has_selection())remove_selection(state);
				else {

					if (ctrl_is_down && shift_is_down) remove_selection(state, 0, state.selection.cursor); //Remove every character from cursor to line start
					else if (ctrl_is_down) remove_selection(state, find_stopper(to_utf_str(state.char_text), state.selection.cursor, -1), state.selection.cursor);
					else remove_selection(state, state.selection.cursor - 1, state.selection.cursor);
				}
				en_change = true;
			}
		} break;
		case _t('a'):
		case _t('A'):
		{
			if (ctrl_is_down) select_all(state);
		} break;
		case _t('v'):
		case _t('V'):
		{
			if (ctrl_is_down) {
				SendMessage(state.wnd, WM_PASTE, 0, 0);
			}
		} break;
		case _t('c'):
		case _t('C'):
		{
			if (ctrl_is_down) {
				SendMessage(state.wnd, WM_COPY, 0, 0);
			}
		} break;
		case _t('x'):
		case _t('X'):
		{
			if (ctrl_is_down) {
				SendMessage(state.wnd, WM_CUT, 0, 0);
			}
		} break;
		case (char)VK_PROCESSKEY://TODO(fran): WTF if you dont cast to (char) vk doesnt match ?!
		{
			//UINT conv_vk = MapVirtualKeyW(lparam>>16, MAPVK_VSC_TO_VK_EX);//doesnt work for arrow keys, thanks windows
			u16 scancode = (decltype(scancode))(lparam >> 16);

			//TODO(fran): check that the candidates window has some candidates, if it doesnt then we can simply continue as if nothing happened
			if (scancode == 0x148/*up arrow*/) {
				state.ignore_IME_candidates = true;
				//PostMessage(state.wnd, WM_KEYDOWN, VK_UP, lparam); TODO(fran): uncomment this and find out what it was for
				reset_v_sel_stored_w = false;
			}
			if (scancode == 0x150/*down arrow*/) {
				state.ignore_IME_candidates = true;
				//PostMessage(state.wnd, WM_KEYDOWN, VK_DOWN, lparam); TODO(fran): uncomment this and find out what it was for
				reset_v_sel_stored_w = false;
			}

		} break;
		default:
		{
			reset_v_sel_stored_w = false;
		} break;
		}

		if (reset_v_sel_stored_w)  reset_vertical_selection_stored_width(state);
		ask_window_for_repaint(state.wnd);//TODO(fran): dont invalidate everything, NOTE: also on each wm_paint the cursor will stop so we should add here a bool repaint; to avoid calling InvalidateRect when it isnt needed
		if (en_change) notify_parent(state, EN_CHANGE); //There was a change in the text
		return 0;
	}break;
	case WM_CUT:
	{
		bool en_change = false;
		SendMessage(state.wnd, WM_COPY, 0, 0);
		if (state.selection.has_selection()) en_change = true;
		remove_selection(state);
		if (en_change) notify_parent(state, EN_CHANGE);
	} break;
	case WM_COPY:
	{
#ifdef UNICODE
		UINT format = CF_UNICODETEXT;
#else
		UINT format = CF_TEXT;
#endif
		//Copy text from current selection to clipboard
		if (state.selection.has_selection()) {
			if (OpenClipboard(state.wnd)) {
				defer{ CloseClipboard(); };
				HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (state.selection.sel_width() + 1) * sizeof(state.char_text[0])); Assert(mem);//TODO(fran): runtime_assert ?

				{
					void* txt = GlobalLock(mem); Assert(txt); defer{ GlobalUnlock(mem); };

					memcpy(txt, state.char_text.c_str() + state.selection.x_min(), state.selection.sel_width() * sizeof(state.char_text[0]));//copy selection
					((decltype(&state.char_text[0]))txt)[state.selection.sel_width() + 1] = 0;//null terminate
				}

				EmptyClipboard();
				auto setclipret = SetClipboardData(format, mem);

				if (!setclipret) GlobalFree(mem);//free mem if for some reason we fail to set the clipboard with our data
				else state.clipboard_handle = mem;//store handle so we can free it on WM_DESTROYCLIPBOARD
			}
		}

	} break;
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
			if (OpenClipboard(state.wnd)) {
				defer{ CloseClipboard(); };
				HGLOBAL clipboard = GetClipboardData(format);
				if (clipboard) {
					cstr* clipboardtxt = (cstr*)GlobalLock(clipboard);
					if (clipboardtxt)
					{
						defer{ GlobalUnlock(clipboard); };
						bool paste_res = paste_from_clipboard(state, clipboardtxt); //TODO(fran): this should be separated into two fns, a general paste fn and first a sanitizer for anything strange that may be in the clipboard txt
						en_change = paste_res;
					}

				}
			}
		}
		if (en_change) notify_parent(state, EN_CHANGE); //There was a change in the text
	} break;
	case WM_CHAR://When the user presses a non-system key this is the 2nd msg we receive
	{//NOTE: a WM_KEYDOWN msg was translated by TranslateMessage() into WM_CHAR
		//Notifications:
		bool en_change = false;

		TCHAR c = (TCHAR)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		LONG_PTR  style = GetWindowLongPtr(state.wnd, GWL_STYLE);

		//lparam = flags
		switch (c) { //https://docs.microsoft.com/en-us/windows/win32/menurc/using-carets
		case 127://Ctrl + Backspace
		{
			/*if (!state.char_text.empty()) {
				if (state.selection.has_selection())remove_selection(state);
				else remove_selection(state, find_stopper(to_utf_str(state.char_text), state.selection.cursor, -1), state.selection.cursor);

				en_change = true;
			}*/
			//do nothing, we already handled it on WM_KEYDOWN
		} break;
		case VK_BACK://Backspace (for some reason it gets sent as WM_CHAR even though it can be handled in WM_KEYDOWN)
		{
			//if (!state.char_text.empty()) {
			//	if (state.selection.has_selection())remove_selection(state);
			//	else remove_selection(state, state.selection.cursor - 1, state.selection.cursor);

			//	en_change = true;
			//}
			//do nothing, we already handled it on WM_KEYDOWN
		}break;
		case VK_TAB://Tab
		{
			if (style & WS_TABSTOP) {//TODO(fran): I think we should specify a style that specifically says on tab pressed change to next control, since this style is just to say I want that to happen to me
				SetFocus(GetNextDlgTabItem(GetParent(state.wnd), state.wnd, FALSE));
			}
			else {
				//We dont handle tabs for now
				Assert(0);
				goto insert_control_char;
			}
		}break;
		case VK_RETURN://Received when the user presses the "enter" key //Carriage Return aka \r
		{
			//NOTE: I wonder, it doesnt seem to send us \r\n so is that something that is manually done by the control?
			//We dont handle "enter" for now
			//if(state.identifier) //TODO(fran): should I only send notifs if I have an identifier? what do the defaults do?
			PostMessage(GetParent(state.wnd), WM_COMMAND, MAKELONG(state.identifier, EN_ENTER), (LPARAM)state.wnd);

			if (style & ES_MULTILINE) {
				//Windows: CRLF ; Unix/Mac: LF ; Machintosh (pre-OS X aka old): CR | CR = \r = 0x0D = VK_RETURN ; LF = \n = 0x0A
				//IMPORTANT: For the sake of not adding pointless complexity internally the control will always use LF, text pasted that uses CRLF or CR will be converted to LF
				c = '\n';
				goto insert_control_char;
			}
		}break;
		case VK_ESCAPE://Escape
		{
			//TODO(fran): should we do something?
			PostMessage(GetParent(state.wnd), WM_COMMAND, MAKELONG(state.identifier, EN_ESCAPE), (LPARAM)state.wnd);
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

		insert_control_char:

			//TODO(fran): maybe this should be the first check for any WM_CHAR?

			//if (contains_char(c, state.invalid_chars)) {
			//	//display tooltip //TODO(fran): make this into a reusable function
			//	std::wstring tooltip_msg = (RS(10) + L" " + state.invalid_chars);
			//	show_tip(state.wnd, tooltip_msg.c_str(), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
			//	return 0;
			//}

			//We have some normal character
			//TODO(fran): what happens with surrogate pairs? I dont even know what they are -> READ
			if (safe_subtract0(state.char_text.length(), state.selection.sel_width()) < state.char_max_sz) {
				en_change = insert_character(state, c);

				//wprintf(L"%s\n", state.char_text.c_str());
			}

		}break;
		}
		ask_window_for_repaint(state.wnd); //TODO(fran): dont invalidate everything
		if (en_change) notify_parent(state, EN_CHANGE); //There was a change in the text
		return 0;
	} break;
	case WM_TIMER:
	{
		WPARAM timerID = wparam;
		switch (timerID) {
		case EDITONELINE_tooltip_timer_id:
		{
			KillTimer(state.wnd, timerID);
			TOOLINFO toolInfo{ sizeof(toolInfo) };
			toolInfo.hwnd = state.wnd;
			toolInfo.uId = (UINT_PTR)state.wnd;
			SendMessage(state.controls.tooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&toolInfo);
			return 0;
		} break;
		case EDITONELINE_caret_timer_id:
		{
			KillTimer(state.wnd, timerID);

			//TODO(fran): timing isnt quite right, there's a slight delay between each caret "re-blinking"
			//			  also we should check the registry to find out the caret timeout, 5 sec is the default on w10 but idk about other OS versions, or maybe the user changed it
			HideCaret(state.wnd);
			ShowCaret(state.wnd);

			SetTimer(state.wnd, timerID, EDITONELINE_default_caret_duration, NULL);
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
		auto char_cnt_with_null = maximum((i32)wparam, 0); // Includes null char
		auto char_text_cnt_with_null = (i32)(state.char_text.length() + 1);
		if (char_cnt_with_null > char_text_cnt_with_null) char_cnt_with_null = char_text_cnt_with_null;
		cstr* buf = (cstr*)lparam;
		if (buf) {//should I check?
			StrCpyN(buf, state.char_text.c_str(), char_cnt_with_null);
			if (char_cnt_with_null < char_text_cnt_with_null) buf[char_cnt_with_null - 1] = (cstr)0;
			res = char_cnt_with_null - 1;
		}
		else res = 0;
		return res;
	} break;
	case WM_GETTEXTLENGTH://does not include null terminator
	{
		return state.char_text.length();
	} break;
	case WM_SETTEXT:
	{
		//Notifications:
		bool en_change = false;

		cstr* buf = (cstr*)lparam;//null terminated

		BOOL res = edit_oneline::_settext(state, buf);
		SendMessage(state.wnd, EM_SETSEL, 0, 0); //When setting the whole element's text we want to keep the cursor at the beginning

		en_change = res;
		if (en_change) notify_parent(state, EN_CHANGE); //There was a change in the text

		return res;
	}break;
	case WM_SETTEXT_NO_NOTIFY:
	{
		cstr* buf = (cstr*)lparam;//null terminated

		BOOL res = edit_oneline::_settext(state, buf);
		SendMessage(state.wnd, EM_SETSEL, 0, 0);

		return res;
	} break;
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
	case WM_IME_SETCONTEXT://Sent after SetFocus to us (sent the first time on SetFocus)
	{//TODO(fran): lots to discover here
		BOOL is_active = (BOOL)wparam;
		//lparam = display options for IME window
		u32 disp_flags = (u32)lparam;
		//NOTE: if we draw the composition window we have to modify some of the lparam values before calling defwindowproc or ImmIsUIMessage

		//NOTE: here you can hide the default IME window and all its candidate windows

		//TODO(fran): with this I get to hide the candidate window & in set_composition_pos I can "hide" the composition window, only problem left is the up and down arrow keys are still bound to the IME window and interact with the now hidden candidate window, I need to block the up&down arrows from getting to the IME and use them myself, or Create my own IME window that's more configurable
		//TODO(fran): maybe I can intercept the "new candidate request" through WM_IME_NOTIFY or somewhere else, stop it from getting to the IME window and transforming it into a WM_LBUTTONDOWN with up or down arrow as key
#if 1
		if (state.hide_IME_wnd) {
			lparam = 0;//TODO(fran): after this is executed, it takes two WM_IME_SETCONTEXT from two other different controls that _do_ want IME visible for the IME window to fix itself
		}
#endif
		printf("WM_IME_SETCONTEXT: lparam = 0x%x\n", (u32)lparam);
		//If we have created an IME window, call ImmIsUIMessage. Otherwise pass this message to DefWindowProc
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}break;
	case WM_IME_NOTIFY:
	{
		//Notifies about changes to the IME window
		//TODO(fran): process this msgs once we manage the ime window
		u32 command = (u32)wparam;
		//lparam = command specific data
#if 0
		const char* notif;
		switch (command) {
		case IMN_CHANGECANDIDATE:	   notif = "IMN_CHANGECANDIDATE"; break;
		case IMN_CLOSECANDIDATE:	   notif = "IMN_CLOSECANDIDATE"; break;
		case IMN_CLOSESTATUSWINDOW:	   notif = "IMN_CLOSESTATUSWINDOW"; break;
		case IMN_GUIDELINE:			   notif = "IMN_GUIDELINE"; break;
		case IMN_OPENCANDIDATE:		   notif = "IMN_OPENCANDIDATE"; break;
		case IMN_OPENSTATUSWINDOW:	   notif = "IMN_OPENSTATUSWINDOW"; break;
		case IMN_SETCANDIDATEPOS:	   notif = "IMN_SETCANDIDATEPOS"; break;
		case IMN_SETCOMPOSITIONFONT:   notif = "IMN_SETCOMPOSITIONFONT"; break;
		case IMN_SETCOMPOSITIONWINDOW: notif = "IMN_SETCOMPOSITIONWINDOW"; break;
		case IMN_SETCONVERSIONMODE:	   notif = "IMN_SETCONVERSIONMODE"; break;
		case IMN_SETOPENSTATUS:		   notif = "IMN_SETOPENSTATUS"; break;
		case IMN_SETSENTENCEMODE:	   notif = "IMN_SETSENTENCEMODE"; break;
		case IMN_SETSTATUSWINDOWPOS:   notif = "IMN_SETSTATUSWINDOWPOS"; break;
		case 0xf:					   notif = "HIDDEN IME NOTIF?! 0xf"; break;//probably IME_SETCOMPOSITION or smth like that, happens when you press a key
		case 0x10d:					   notif = "HIDDEN IME NOTIF?! 0x10d"; break;
		case 0x10e:					   notif = "HIDDEN IME NOTIF?! 0x10e"; break;//probably IME_CANCEL or smth like that, happens when the ime window is closed eg by pressing escape
		default: notif = 0; Assert(0);
		}
		printf("WM_IME_NOTIFY: %s\n", notif);
#endif
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_REQUEST://After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive this msg
	{
#if 0
		const char* req;
		switch (wparam) {
		case IMR_CANDIDATEWINDOW:			req = "IMR_CANDIDATEWINDOW"; break;
		case IMR_COMPOSITIONFONT:			req = "IMR_COMPOSITIONFONT"; break;
		case IMR_COMPOSITIONWINDOW:			req = "IMR_COMPOSITIONWINDOW"; break;
		case IMR_CONFIRMRECONVERTSTRING:	req = "IMR_CONFIRMRECONVERTSTRING"; break;
		case IMR_DOCUMENTFEED:				req = "IMR_DOCUMENTFEED"; break;
		case IMR_QUERYCHARPOSITION:			req = "IMR_QUERYCHARPOSITION"; break;
		case IMR_RECONVERTSTRING:			req = "IMR_RECONVERTSTRING"; break;
		default:req = 0; Assert(0);
		}
		printf("WM_IME_REQUEST: %s\n", req);
#endif

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
		set_composition_pos(state);
		set_composition_font(state);//TODO(fran): should I place this somewhere else?

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_COMPOSITION://On japanese keyboard, after we press a key to start writing this is 2nd msg received
	{//doc: sent when IME changes composition status cause of keystroke
		//wparam = DBCS char for latest change to composition string, TODO(fran): find out about DBCS

#if 0 //At this point there's no way of knowing if the change was caused by the user writing or changing candidates
		const char* change; static int cnt;
		printf("WM_IME_COMPOSITION %d:\n", cnt++);
		if (lparam & GCS_COMPATTR) { change = "GCS_COMPATTR"; printf("%s\n", change); }
		if (lparam & GCS_COMPCLAUSE) { change = "GCS_COMPCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_COMPREADSTR) { change = "GCS_COMPREADSTR"; printf("%s\n", change); }
		if (lparam & GCS_COMPREADATTR) { change = "GCS_COMPREADATTR"; printf("%s\n", change); }
		if (lparam & GCS_COMPREADCLAUSE) { change = "GCS_COMPREADCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_COMPSTR) { change = "GCS_COMPSTR"; printf("%s\n", change); }
		if (lparam & GCS_CURSORPOS) { change = "GCS_CURSORPOS"; printf("%s\n", change); }
		if (lparam & GCS_DELTASTART) { change = "GCS_DELTASTART"; printf("%s\n", change); }
		if (lparam & GCS_RESULTCLAUSE) { change = "GCS_RESULTCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_RESULTREADCLAUSE) { change = "GCS_RESULTREADCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_RESULTREADSTR) { change = "GCS_RESULTREADSTR"; printf("%s\n", change); }
		if (lparam & GCS_RESULTSTR) { change = "GCS_RESULTSTR"; printf("%s\n", change); }
		if (!lparam) { change = "CANCEL IME"; printf("%s\n", change); }
#endif

		if (state.hide_IME_wnd && lparam & GCS_RESULTSTR) {//the content of the IME has been accepted by the user
			SendMessage(state.wnd, EM_SETSEL, state.selection.cursor, state.selection.cursor);//clear selection
			//TODO(fran): I think I should do state.ignore_IME_candidates = false; here
			state.ignore_IME_candidates = false;
			return 0;//we already have the result string in the editbox
		}

		if (state.hide_IME_wnd && state.ignore_IME_candidates) {
			state.ignore_IME_candidates = false;
			//TODO(fran): join this with the other cases, for example we probably want to have default handling on lparam==0
			HIMC imc = ImmGetContext(state.wnd);
			if (imc != NULL) {
				defer{ ImmReleaseContext(state.wnd, imc); };
				//we wanna restore the IME's comp string to what it was before the candidate change, we do it by simulating an ESC key press which tells the IME to cancel the candidate selection

				//auto simres = ImmSimulateHotKey(state.wnd, IME_JHOTKEY_CLOSE_OPEN); Assert(simres);//TODO(fran): there-s only this one hotkey for jp, really?

				INPUT ip;

				// Set up a generic keyboard event.
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = 0; // hardware scan code for key
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;

				// Press the key
				ip.ki.wVk = VK_ESCAPE; // virtual-key code for the key
				ip.ki.dwFlags = 0; // 0 for key press
				SendInput(1, &ip, sizeof(ip));

				// Release the key
				ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
				SendInput(1, &ip, sizeof(ip));
			}
			//TODO(fran): if the IME input has been accepted (probably GCS_RESULTSTR) then we gotta manually change the selection so only the cursor remains
			return 0;
		}


		DefWindowProc(hwnd, msg, wparam, lparam);//TODO(fran): this will generate WM_CHAR msgs that I dont really want, I can retrieve the composition string right from here, I shouldnt call DefWindowProc
		bool en_change = false;

		//TODO(fran): find a way to know when the IME was accepted, we dont really want to receive the accepted text as WM_CHAR messages since we already have the text written into the control
		if (lparam == 0) {
			//IME was cancelled, delete whatever was written with it
			if (state.selection.has_selection()) {
				remove_selection(state);
				en_change = true;
			}
		}
		else {//TODO(fran): check lparam, we may not always want to update the text depending on the code it has
			HIMC imc = ImmGetContext(state.wnd);
			if (imc != NULL) {
				defer{ ImmReleaseContext(state.wnd, imc); };
				//INFO: ImmGetCompositionStringW: https://cpp.hotexamples.com/examples/-/-/ImmGetCompositionStringW/cpp-immgetcompositionstringw-function-examples.html
				int szbytes = ImmGetCompositionStringW(imc, GCS_COMPSTR, 0, 0);//excluding null terminator
				if (szbytes > 0) {//otherwise gotta handle possible errors
					utf16* txt;
					//szbytes += (1 * sizeof(*txt));//include null terminator
					txt = (decltype(txt))malloc(szbytes + sizeof(*txt)); defer{ free(txt); };

					auto len = ImmGetCompositionStringW(imc, GCS_COMPSTR, txt, szbytes) / sizeof(*txt);
					txt[len] = 0;//ImmGetCompositionStringW does _not_ write the null terminator

					en_change = insert_character(state, txt);

					SendMessage(state.wnd, EM_SETSEL, safe_subtract0(state.selection.cursor, len), state.selection.cursor);
				}
			}
		}
		if (en_change) notify_parent(state, EN_CHANGE); //There was a change in the text
		return 0;
	} break;
	case WM_IME_CHAR://WM_CHAR from the IME window, this are generated once the user has pressed enter on the IME window, so more than one char will probably be coming
	{
		//UNICODE: wparam=utf16 char ; ANSI: DBCS char
		//lparam= the same flags as WM_CHAR
#ifndef UNICODE
		Assert(0);//TODO(fran): DBCS
#endif 
		if (!state.hide_IME_wnd) {
			PostMessage(state.wnd, WM_CHAR, wparam, lparam);
		}

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

		memcpy_s(state.placeholder, sizeof(state.placeholder), text, (cstr_len(text) + 1) * sizeof(*text));
		//TODO(fran): check whether we need to redraw
		return 1;
	} break;
	//case EM_SETINVALIDCHARS:
	//{
	//	cstr* chars = (cstr*)lparam;
	//	memcpy_s(state.invalid_chars, sizeof(state.invalid_chars), chars, (cstr_len(chars) + 1) * sizeof(*chars));
	//} break;
	case WM_NOTIFYFORMAT://1st msg sent by our tooltip
	{
		switch (lparam) {
		case NF_QUERY: return sizeof(cstr) > 1 ? NFR_UNICODE : NFR_ANSI; //a child of ours has asked us
		case NF_REQUERY: return SendMessage((HWND)wparam/*parent*/, WM_NOTIFYFORMAT, (WPARAM)state.wnd, NF_QUERY); //NOTE: the return value is the new notify format, right now we dont do notifications so we dont care, but this could be a TODO(fran)
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
		if (msg_info->hwndFrom == state.controls.tooltip) {
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
					SelectFont(cd->nmcd.hdc, state.theme.font);
					SetTextColor(cd->nmcd.hdc, ColorFromBrush(state.theme.brushes.foreground.normal));
					SetBkColor(cd->nmcd.hdc, ColorFromBrush(state.theme.brushes.bk.normal));
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
		return (LRESULT)state.theme.font;
	} break;
	case WM_DEADCHAR://Very interesting, this is the way that TranslateMessage notifies about things like a tilde being pressed, since this keys arent expected to be directly made into characters on the screen
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_PARENTNOTIFY: //Msgs from our potential childs
	{
		return 0;//what the childs do is completely opaque to us
	} break;
	case WM_COMMAND:
	{
		LRESULT res;
		HWND child = (HWND)lparam;
		if (child) {//Notifs from our childs
			//NOTE: for now childs are completely opaque, we simply work as "pasamanos", TODO(fran): we shouldnt have to do this, what we need is some sort of observer, aka something happens and the control can send the msg directly to a place where it'll be answered, having this parent child relationships is not enough, for example in cases like this were there are lots of things that can be appended to edit controls, searchbar, button, scrollbar, ...
			res = SendMessage(state.parent, WM_COMMAND, wparam, lparam);
		}
		else {//otherwise it's a notif from an accelerator or menu
			Assert(0);
		}
		return res;
	} break;
	case EM_SETSEL:
	{
		size_t _start = (size_t)wparam;
		size_t _end = (size_t)lparam;
		//int anchor = _start; //TODO(fran): store the anchor, useful for extending selection eg when the user clicks while pressing shift

		if (_start == (size_t)-1 || (i32)_start == (i32)-1) {//TODO(fran): find out the best check for this since -1 usually gets automapped to i32 and thus will be different from (size_t)-1 (I think)
			//Remove current selection
			if (state.selection.anchor != state.selection.cursor) {
				state.selection.anchor = state.selection.cursor;
				ask_window_for_repaint(state.wnd);
			}
		}
		else {
			size_t end_max = (size_t)state.char_text.length();
			if (_end == (size_t)-1 || (i32)_end == (i32)-1) {
				_end = end_max; //Set _end to one past the last valid char
			}

			_start = clamp((size_t)0, _start, end_max);
			_end = clamp((size_t)0, _end, end_max);


			if (state.selection.anchor != _start || state.selection.cursor != _end) {
				state.selection.anchor = _start;
				state.selection.cursor = _end;
				ask_window_for_repaint(state.wnd);
			}
		}

		reposition_caret(state);//TODO(fran): only do it if cursor changed

		return 0;
	} break;
	case EM_GETSEL://TODO(fran): EM_GETSEL_EX (to be able to request for size_t (possibly 64bit) values)
	{
		DWORD* start = (decltype(start))wparam;
		DWORD* end = (decltype(end))lparam;

		if (start) *start = (DWORD)state.selection.x_min();
		if (end) *end = (DWORD)state.selection.x_max();

		return -1;//TODO(fran): support for 16bit, should return zero-based value with the starting position of the selection in the LOWORD and the position of the first TCHAR after the last selected TCHAR in the HIWORD. If either of these values exceeds 65535 then the return value is -1.
	} break;
	case WM_CAPTURECHANGED://We're losing mouse capture
	{
		state.on_mouse_tracking = false;
		return 0;
	} break;
	case WM_DESTROYCLIPBOARD:
	{
		GlobalFree(state.clipboard_handle);//TODO(fran): should I zero clipboard_handle?
		//TODO(fran): this and storing the clipboard_handle are actually pointless, windows now owns and knows how to free our clipboard data so we should actually _not_ give it our handle when we OpenClipboard() and this extra work should solve itself
		return 0;
	} break;
	case WM_RENDERALLFORMATS:
	{
		//When our application is about to be closed windows requests that we "render" all the clipboard formats that we have previously set
		//Problem is this is called when you use Delayed Rendering, by calling SetClipboardData with a null hMem parameter, we never use that. So I can only assume two things: either this is a windows bug or TODO(fran): we got a bug
		//Extra info on 'rendering': https://docs.microsoft.com/en-us/windows/win32/dataxchg/using-the-clipboard?redirectedfrom=MSDN#_win32_Copying_Information_to_the_Clipboard
		return 0;
	} break;
	case WM_MOUSEWHEEL:
	{
		//no reason for processing mousewheel input. TODO(fran): we may want to process it if the text is longer that what fits on our box, we could provide sideways scrolling
		return DefWindowProc(hwnd, msg, wparam, lparam);//propagates the msg to the parent
	} break;

	default:
	{

#ifdef _DEBUG_HWND_MESSAGES
		if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
			TCHAR arr[256];
			int res = GetClipboardFormatName(msg, arr, 256);
			cstr_printf(arr); printf("\n");
			//After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive "MSIMEQueryPosition"
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		//Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	} break;
	}
	return 0;
}

struct pre_post_main {
	pre_post_main() { init_wndclass(wndclass, proc, CS_DBLCLKS, IDC_IBEAM); }
	~pre_post_main() {}
} static const PREMAIN_POSTMAIN;
}