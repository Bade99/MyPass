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

struct CharSel {
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
			BrushGroup foreground, bk, border, selection, placeholder; //NOTE: for now we use the border color for the caret color
		};
		BrushGroup all[5];
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
struct InvalidCharactersResult { bool res; str explanation; };
typedef InvalidCharactersResult(*func_has_invalid_chars)(const utf16* txt, size_t char_cnt, void* user_extra);
typedef void(*func_on_change)(void* user_extra, HWND wnd);

union Functions {
	struct {
		func_has_invalid_chars has_invalid_chars;
		func_on_change on_change;
	};
	void* all[2]{ 0 };
private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
};

struct TextEditEntry {
	/*
	TODO: Simpler entry struct, see if we can reduce this one down to this:
		struct TextEditEntry {
			enum class Type { insert, erase } type;
			size_t pos;
			str text;
			// Optional: selection before/after
		};
	*/

	enum class Kind { //TODO: we can add these guys to UndoMergeState::Type as well together with an extra check (can_merge(old type) && old type == new type, where can_merge will only be true for typing, backspace, delete_forward). We could also not add them and still do the same, with can_merge false for Type::none
		typing, backspace, delete_forward, paste, cut, ime, programmatic,
	};
	Kind edit_kind;

	size_t position_x_min;
	str removed_text, inserted_text;

	CharSel selection_before, selection_after; //NOTE: selection_after could be safely inferred
	bool selection_before_trailing, selection_after_trailing;

	size_t memory_size() const { return sizeof(*this) + (removed_text.capacity() + inserted_text.capacity()) * sizeof(decltype(removed_text)::value_type); }
};

struct ImeCompositionState {
	bool active;
	bool document_replaced;
	bool has_result;

	size_t position_x_min;
	str removed_text, committed_text, provisional_text;
	CharSel selection_before;
	bool selection_before_trailing;
};

struct TextCaretStop {
	size_t character_index;
	f32 x;
	bool trailing;
};

struct TextLineLayout {
	size_t text_start;
	size_t text_length;
	SIZE size;
	bool right_to_left;
	std::vector<f32> leading_caret_positions;
	std::vector<f32> trailing_caret_positions;
	std::vector<u8> caret_stops;
	std::vector<u8> whitespace_characters;
	std::vector<TextCaretStop> visual_caret_stops;
};

struct UndoMergeState {
	/*
	Usage:

		if (merge_state.can_merge(edit_type, selection_before))
			history.update_latest(...);
		else
			history.push(...);

		merge_state.record_edit(edit_type, selection_after);
	*/

	using clock = std::chrono::steady_clock;

	enum class Type { other, typing, backspace, delete_forward, }; //NOTE: vscode uses an extra type: space, breaks operations by a space and never by time, in practice that means that, for languages that use spaces, every undo operation removes the last word and space you typed

	TextEditEntry::Kind transient_kind = TextEditEntry::Kind::typing;
	Type active_type = Type::other;
	CharSel selection_after{};
	clock::time_point last_edit_time{};
	static constexpr auto max_delay_ms = std::chrono::milliseconds{ 2000 };

	bool can_merge(Type incoming_type, const CharSel& selection_before) const {
		bool same_mergeable_type = active_type == incoming_type && active_type != Type::other;
		bool selections_are_empty = !selection_after.has_selection() && !selection_before.has_selection();
		bool selection_is_contiguous = selection_after.cursor == selection_before.cursor;
		bool close_in_time = clock::now() - last_edit_time <= max_delay_ms;
		return same_mergeable_type && selections_are_empty && selection_is_contiguous && close_in_time;
	}

	static Type map_kind_to_type(TextEditEntry::Kind kind) {
		switch (kind) {
		case TextEditEntry::Kind::typing: return Type::typing;
		case TextEditEntry::Kind::backspace: return Type::backspace;
		case TextEditEntry::Kind::delete_forward: return Type::delete_forward;
		default: return Type::other;
		}
	};

	bool can_merge(const CharSel& selection_before) const {
		return can_merge(map_kind_to_type(transient_kind), selection_before);
	}

	void record_transient_edit(TextEditEntry::Kind edit_kind) {
		transient_kind = edit_kind;
	}

	void record_edit(Type edit_type, const CharSel& new_selection_after) {
		active_type = edit_type;
		selection_after = new_selection_after;
		last_edit_time = clock::now();
	}
	void record_edit(TextEditEntry::Kind edit_kind, const CharSel& new_selection_after) {
		record_edit(map_kind_to_type(edit_kind), new_selection_after);
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
		active_type = Type::other;
	}
};

struct State : WindowState {
	u32 identifier;

	Theme theme;

	u64 char_max_sz;//doesnt include terminating null, also we wont have terminating null

	CharSel selection;//current selection, in "character" coordinates, zero-based //TODO(fran): cache selection's starting line

	f32 vertical_selection_stored_width;

	struct CaretInfo {
		HBITMAP bmp;
		SIZE dim;
		POINT pos;//client coords, it's also top down so this is the _top_ of the caret
		bool trailing;
	}caret;

	str char_text;//much simpler to work with and debug
	std::vector<size_t> line_breaks; //Indices into the text string where line breaks occur
	std::vector<TextLineLayout> line_layouts;

	UndoHistory<TextEditEntry> history;
	UndoMergeState history_helper;
	ImeCompositionState ime_composition;

	v2_i32 padding; //NOTE: x,y offset from where characters start being placed on the screen, relative to the client area, positive values 'shrink' the rendering area. For a left aligned control this will be offset from the left, for right aligned it'll be offset from the right, and for center alignment it'll be the left most position from where chars will be drawn

	cstr placeholder[100]; //NOTE: uses txt_dis brush for rendering
	bool maintain_placerholder_on_focus;//Hide placeholder text when the user clicks over it

	Controls controls;

	bool on_mouse_tracking;//true when capturing the mouse while the user remains with left click down

	bool hide_ime_window;
	bool ignore_ime_candidates;

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
	enum Menu : u32 {
		undo = 300, redo, cut, copy, paste, del, find, select_all
	};
}
}
