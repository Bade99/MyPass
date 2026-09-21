#pragma once

#include "undo_history.h"

namespace edit_oneline {

//-------------------"API"--------------------:
// edit_oneline::set_theme()
//edit_oneline::maintain_placerholder_when_focussed() : hide or keep showing the placeholder text when the user clicks over the wnd

//-------------"API" (Additional Messages)-------------:
#define editoneline_base_msg_addr (WM_USER + 1500)

//#define EM_SETINVALIDCHARS (editoneline_base_msg_addr+2) /*characters that you dont want the edit control to accept either trough WM_CHAR or WM_PASTE*/ /*lparam=unused ; wparam=pointer to null terminated cstring, each char will be added as invalid*/ /*INFO: overrides the previously invalid characters that were set before*/ /*TODO(fran): what to do on pasting? reject the entire string or just ignore those chars*/
#define WM_SETTEXT_NO_NOTIFY (editoneline_base_msg_addr+3) /*wparam=unused ; lparam=null terminated utf16* */


//-------------"Default Notifications"-------------:
//EN_CHANGE
//EN_KILLFOCUS

//-------------"Non-standard Notifications"-------------:
//Notification msgs, sent to the parent through WM_COMMAND with LOWORD(wparam)=control identifier ; HIWORD(wparam)=msg code ; lparam=HWND of the control
//IMPORTANT: this are sent _only_ if you have set an identifier for the control (hMenu on CreateWindow)
#define _editoneline_notif_base_msg 0x0104
#define EN_ENTER (_editoneline_notif_base_msg+1) /*User has pressed the enter key*/
#define EN_ESCAPE (_editoneline_notif_base_msg+2) /*User has pressed the escape key*/


//-------------Additional Styles-------------:
#define ES_EXPANSIBLE 0x4000L //Text editor asks the parent for resizing if its text doesnt fit vertically on its client area

//-------------Tooltip-------------:
#define EDITONELINE_default_tooltip_duration 3000 /*ms*/
#define EDITONELINE_tooltip_timer_id 0xf1


#define EDITONELINE_default_caret_duration 5000 /*ms*/
#define EDITONELINE_caret_timer_id 0xf2


//---------Differences with default oneline edit control---------:
//·EN_CHANGE is sent when there is a modification to the text, of any type, and it's sent immediately, not after rendering


constexpr auto& wndclass = wndclass_name("text");

constexpr cstr password_char = sizeof(password_char) > 1 ? _t('●') : _t('*');

constexpr UINT clipboard_format =
#ifdef UNICODE
	CF_UNICODETEXT;
#else
	CF_TEXT;
#endif

struct char_sel {
	using type = size_t;
	type anchor;//Eg ABC		anchor=1	anchor is between A and B
	type cursor;//Eg ABC		cursor=1	cursor is between A and B
	//First character of the selection
	type x_min() const {
		type res = (anchor < cursor) ? anchor : cursor;
		return res;
	}
	//First character beyond the selection
	type x_max() const {
		type res = (anchor > cursor) ? anchor : cursor;
		return res;
	}
	type sel_width() const {
		type res = distance(anchor, cursor);
		return res;
	}
	bool has_selection() const { return sel_width(); }
	//void set_both(int new_val) { anchor = cursor = new_val; }

	//Example:
	//			ABCD	EM_SETSEL(1,1)	cursor is placed in between A and B
	//			ABCD	EM_SETSEL(1,2)	selection covers the letter B, cursor is placed between B and C
};

struct Theme {
	union {
		struct {
			brush_group foreground, bk, border, selection, placeholder; //NOTE: for now we use the border color for the caret color
		};
		brush_group all[5];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} brushes;
		
	struct {
		u32 border_thickness = U32MAX;
		UINumber border_radius{ .value = F32INFINITY };
	} dimensions;
	HFONT font = nullptr;

	bool copy_from(const Theme& src) {
		bool repaint = false;
		_theme_copy_all_brushes(src.brushes, this->brushes);

		_theme_copy_u32(src.dimensions.border_thickness, this->dimensions.border_thickness);
		_theme_copy_uinumber(src.dimensions.border_radius, this->dimensions.border_radius);

		_theme_copy_pointer(src.font, this->font);

		return repaint;
	}
};

union Controls {
	struct {
		HWND tooltip;
	};
	HWND all[1];

private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
};

//TODO(fran): now that I think about it I believe this would be much better done once you need the contents from the editbox, you can perform a one time check, send one notification to the user informing of the problem and that's it
struct _has_invalid_chars { bool res; str explanation; };
typedef _has_invalid_chars(*func_has_invalid_chars)(const utf16* txt, size_t char_cnt, void* user_extra);
typedef void(*func_on_change)(void* user_extra, HWND wnd);

union Functions {
	struct {
		func_has_invalid_chars has_invalid_chars;
		func_on_change on_change;
	};
	void* all[2]{ 0 };
private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
};

struct text_edit_entry {
	/*
	TODO: Simpler entry struct, see if we can reduce this one down to this:
		struct text_edit_entry {
			enum class type { insert, erase } type;
			size_t pos;
			str text;
			// Optional: selection before/after
		};
	*/

	enum class kind { //TODO: we can add these guys to undo_merge_state::type as well together with an extra check (can_merge(old type) && old type == new type, where can_merge will only be true for typing, backspace, delete_forward). We could also not add them and still do the same, with can_merge false for type::none
		typing, backspace, delete_forward, paste, cut, ime, programmatic,
	};
	kind edit_kind;

	size_t position_x_min;
	str removed_text, inserted_text;

	char_sel selection_before, selection_after; //NOTE: selection_after could be safely inferred

	size_t memory_size() const { return sizeof(*this) + (removed_text.capacity() + inserted_text.capacity()) * sizeof(decltype(removed_text)::value_type); }
};

struct undo_merge_state {
	/*
	Usage:

		auto now = std::time(nil);

		if (merge_state.can_merge(edit_type, selection_before, now))
			history.update_latest(...);
		else
			history.push(...);

		merge_state.record_edit(edit_type, selection_after, now);
	*/

	enum class type { other, typing, backspace, delete_forward, }; //NOTE: vscode uses an extra type: space, breaks operations by a space and never by time, in practice that means that, for languages that use spaces, every undo operation removes the last word and space you typed

	text_edit_entry::kind transient_kind = text_edit_entry::kind::typing;
	type active_type = type::other;
	char_sel selection_after{};
	time64 last_edit_time{};
	static constexpr u32 max_delay_ms{ 2000 };

	bool can_merge(type incoming_type, const char_sel& selection_before, time64 now = std::time(nil)) const {
		bool same_mergeable_type = active_type == incoming_type && active_type != type::other;
		bool selections_are_empty = !selection_after.has_selection() && !selection_before.has_selection();
		bool selection_is_contiguous = selection_after.cursor == selection_before.cursor;
		bool close_in_time = now - last_edit_time <= max_delay_ms;
		return same_mergeable_type && selections_are_empty && selection_is_contiguous && close_in_time;
	}

	static type map_kind_to_type(text_edit_entry::kind kind) {
		switch (kind) {
		case text_edit_entry::kind::typing: return type::typing;
		case text_edit_entry::kind::backspace: return type::backspace;
		case text_edit_entry::kind::delete_forward: return type::delete_forward;
		default: return type::other;
		}
	};

	bool can_merge(const char_sel& selection_before, time64 now = std::time(nil)) const {
		return can_merge(map_kind_to_type(transient_kind), selection_before, now);
	}

	void record_transient_edit(text_edit_entry::kind edit_kind) {
		transient_kind = edit_kind;
	}

	void record_edit(type edit_type, const char_sel& new_selection_after, time64 now = std::time(nil)) {
		active_type = edit_type;
		selection_after = new_selection_after;
		last_edit_time = now;
	}
	void record_edit(text_edit_entry::kind edit_kind, const char_sel& new_selection_after, time64 now = std::time(nil)) {
		record_edit(map_kind_to_type(edit_kind), new_selection_after, now);
	}

	void break_group() {
		/*
		Call break_group() after:
		- Undo or redo.
		- Cursor or selection movement.
		- Focus loss.
		- Paste, cut, autocomplete, or selection replacement.
		- Programmatic text changes.
		- An IME composition is committed or cancelled.
		*/
		active_type = type::other;
	}
};

struct State : WindowState {
	u32 identifier;

	Theme theme;

	u64 char_max_sz;//doesnt include terminating null, also we wont have terminating null

	char_sel selection;//current selection, in "character" coordinates, zero-based //TODO(fran): cache selection's starting line

	f32 vertical_selection_stored_width;

	struct caretnfo {
		HBITMAP bmp;
		SIZE dim;
		POINT pos;//client coords, it's also top down so this is the _top_ of the caret
	}caret;

	str char_text;//much simpler to work with and debug
	std::vector<int> char_dims;//NOTE: specifies, for each character, its width
	std::vector<size_t> line_breaks; //Indices into the text string where line breaks occur

	undo_history<text_edit_entry> history;
	undo_merge_state history_helper;

	v2_i32 padding; //NOTE: x,y offset from where characters start being placed on the screen, relative to the client area, positive values 'shrink' the rendering area. For a left aligned control this will be offset from the left, for right aligned it'll be offset from the right, and for center alignment it'll be the left most position from where chars will be drawn

	cstr placeholder[100]; //NOTE: uses txt_dis brush for rendering
	bool maintain_placerholder_on_focus;//Hide placeholder text when the user clicks over it

	Controls controls;

	bool on_mouse_tracking;//true when capturing the mouse while the user remains with left click down

	HGLOBAL clipboard_handle;

	bool hide_IME_wnd;
	bool ignore_IME_candidates;

	void* user_extra;

	Functions functions;

	v2_i32 scroll;//TODO(fran): v2_i64?
};

namespace ETP {
	enum ETP {//EditOneline_tooltip_placement
		left = (1 << 1),
		top = (1 << 2),
		right = (1 << 3),
		bottom = (1 << 4),
		//current_char = (1 << 5), //instead of placement in relation to the control wnd it will be done relative to a character
	};
}

namespace menu {
	enum menu : u32 {
		undo = 300, redo, cut, copy, paste, del, find, select_all
	};
}
}