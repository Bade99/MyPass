#pragma once

namespace editor {

constexpr auto& wndclass = wndclass_name("editor");

constexpr auto pad_percent = .05f;

#ifdef _DEBUG
constexpr auto debug_text_view = true;
#else
constexpr auto debug_text_view = false;
#endif

constexpr auto msgbox_placement = MBP::center | MBP::top;

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
	text username, password;
	bool signup;
};

struct Data {
	Settings* settings;
};

struct file_header_base {
	utf8 magic[4]{ 'm','y','p','s' };
	u32 version = 1;
};
static_assert(sizeof(file_header_base) == 8 && sizeof(file_header_base::magic) == 4 && sizeof(file_header_base::version) == 4, "Base file header cannot be modified");

struct file_header_v1 : file_header_base {
	utf16 salt[8];
};
static_assert(alignof(file_header_v1) % 4 == 0);

struct file_footer_v1 {
	u8 auth_integrity_hash[32];
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

enum class mode {
	normal = 0, transition_v0
};

namespace custom_message {
	enum custom_message {
		show_transition_v0_msgbox = WM_USER + 1, 
	};
};

struct Controls {
	using type = HWND;
	union {
		struct {
			struct {
				union {
					struct {
						type btn_static_bk_transition;
						type btn_static_transition;
						type static_transition;
						type btn_confirm_transfer;
						type edit_passwords;
						type icon_transition;
					};
					type all[6];
				};
			private: void _() { static_assert(sizeof(all) == (sizeof(*this))); }
			} transition_v0;

			type btn_add_start, btn_add_end;
			type search;
			type combo_sort;
			type page_space;
			type page;
		};
		type all_fixed[12];
	};
	std::vector<HWND> password_editors;
private: void _() { static_assert(sizeof(all_fixed) == (sizeof(*this) - sizeof(password_editors))); }
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
	Controls controls;
	Settings* settings;

	str current_user;
	utf16 salt[8];
	bool passwords_need_save;

	sort_option sorting;

	mode mode;

	void init() {
		controls.password_editors = decltype(controls.password_editors)();
		current_user = decltype(current_user)();
	}

	void uninit() {
		controls.password_editors.~vector();
		current_user.~basic_string();
	}
};
}