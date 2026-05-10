#pragma once

namespace editor {

constexpr auto& wndclass = wndclass_name("editor");

constexpr auto pad_percent = .05f;

#ifdef _DEBUG
constexpr auto debug_text_view = true;
#else
constexpr auto debug_text_view = false;
#endif

struct Settings {

#define foreach_editor_member(op) \
		op(RECT, rc,200,200,700,800 ) \

	foreach_editor_member(_generate_member);

	_generate_default_struct_serialize(foreach_editor_member);

	_generate_default_struct_deserialize(foreach_editor_member);

	void validate() {
		std::remove_reference<decltype(*this)>::type defaults;
		validate_rect_in_screen(rc, defaults.rc);
	}
};

struct Start {
	u32 key[8];//32 bytes
	text username;
	bool signup;
};

struct Data {
	Settings* settings;
	Start* start;
};

enum class sort_option {
	date_modified_newest = 0, date_modified_oldest,
	alphabetic_az, alphabetic_za,
	date_created_newest, date_created_oldest,

	__last
};

struct sort_combo_item { 
	sort_option value; 
	u32 label_id; 
	private: void _(){ static_assert(sizeof(*this) == sizeof(void*)); }
};

struct State {
	HWND wnd;
	HWND nc_parent;
	struct { //menus
		HMENU menu;
		HMENU menu_file;
		HMENU menu_file_lang;
		HMENU menu_edit;
	};
	struct Controls {
		using type = HWND;
		union {
			struct {
				type edit_passwords;
				type btn_add_start, btn_add_end;
				type search;
				type combo_sort;
				type page_space;
				type page;
			};
			type all_fixed[7];
		};
		std::vector<HWND> password_editors;
	private: void _() { static_assert(sizeof(all_fixed) == (sizeof(*this) - sizeof(password_editors))); }
	} controls;
	Settings* settings;
	Start* start;

	wchar_t* current_user;
	bool passwords_need_save;

	sort_option sorting;

	void init() {
		controls.password_editors = decltype(controls.password_editors)();
	}

	void uninit() {
		controls.password_editors.~vector();
	}
};
}