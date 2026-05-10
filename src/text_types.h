#pragma once

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

//TODO(fran): now that I think about it I believe this would be much better done once you need the contents from the editbox, you can perform a one time check, send one notification to the user informing of the problem and that's it
struct _has_invalid_chars { bool res; std::wstring explanation; };
typedef _has_invalid_chars(*func_has_invalid_chars)(const utf16* txt, size_t char_cnt, void* user_extra);

struct char_sel {
	using type = size_t;
	type anchor;//Eg ABC		anchor=1	anchor is between A and B
	type cursor;//Eg ABC		cursor=1	cursor is between A and B
	//First character of the selection
	type x_min() {
		type res = (anchor < cursor) ? anchor : cursor;
		return res;
	}
	//First character beyond the selection
	type x_max() {
		type res = (anchor > cursor) ? anchor : cursor;
		return res;
	}
	type sel_width() {
		type res = distance(anchor, cursor);
		return res;
	}
	bool has_selection() { return sel_width(); }
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

struct State {
	HWND wnd;
	HWND parent;
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

	std::wstring char_text;//much simpler to work with and debug
	std::vector<int> char_dims;//NOTE: specifies, for each character, its width
	std::vector<size_t> line_breaks; //Indices into the text string where line breaks occur

	v2_i32 padding; //NOTE: x,y offset from where characters start being placed on the screen, relative to the client area, positive values 'shrink' the rendering area. For a left aligned control this will be offset from the left, for right aligned it'll be offset from the right, and for center alignment it'll be the left most position from where chars will be drawn

	cstr placeholder[100]; //NOTE: uses txt_dis brush for rendering
	bool maintain_placerholder_on_focus;//Hide placeholder text when the user clicks over it

	union EditOnelineControls {
		struct {
			HWND tooltip;
		};
		HWND all[1];

	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	}controls;

	bool on_mouse_tracking;//true when capturing the mouse while the user remains with left click down

	HGLOBAL clipboard_handle;

	bool hide_IME_wnd;
	bool ignore_IME_candidates;

	void* user_extra;

	func_has_invalid_chars has_invalid_chars;

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
}