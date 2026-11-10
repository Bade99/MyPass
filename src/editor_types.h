#pragma once
#include "animation.h"

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
		op(RECT, rc,rdpi(200),rdpi(200),rdpi(700),rdpi(800) ) \

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

struct FileHeaderBase {
	utf8 magic[4]{ 'm','y','p','s' };
	u32 version = 1;
};
static_assert(sizeof(FileHeaderBase) == 8 && sizeof(FileHeaderBase::magic) == 4 && sizeof(FileHeaderBase::version) == 4, "Base file header cannot be modified");

struct FileHeaderV1 : FileHeaderBase {
	utf16 salt[8];
};
static_assert(alignof(FileHeaderV1) % 4 == 0);

struct FileFooterV1 {
	u8 auth_integrity_hash[32];
};

enum class SortOption {
	date_modified_newest = 0, date_modified_oldest,
	alphabetic_az, alphabetic_za,
	date_created_newest, date_created_oldest,

	__last
};

struct SortComboItem {
	SortOption value;
	u32 label_id; 
	private: void _(){ static_assert(sizeof(*this) == sizeof(void*)); }
};

enum class Mode {
	normal = 0, transition_v0
};

namespace custom_message {
	enum CustomMessage {
		show_transition_v0_msgbox = WM_USER + 1,
		show_used_backup_msgbox,
		redraw_after_show,
	};
};

struct CardLayoutMove {
	HWND wnd;
	RECT from, to;
};

struct CardLayoutAnimation {
	std::vector<CardLayoutMove> moves;
	animation::Timeline timeline;
	RECT from_backdrop_bounds{}, to_backdrop_bounds{};
	i32 from_page_height = 0, to_page_height = 0;
	i32 client_width = 0, client_height = 0;
	HWND entering_card = nil;
	bool requested = false;
};

struct CardLayoutState {
	CardLayoutAnimation animation;
	RECT backdrop_bounds{};
	std::vector<HWND> search_pending_reveals;
	bool search_reflow_pending = false;
	bool applying_layout = false;
	bool refreshing_cards = false;
};

struct PasswordEditorRowV1View {
	str_view description, value;
	multiflag<password_editor::ValueCellFlag> flags = 0;
};

struct PasswordEditorV1View {
	str_view title;
	time64 date_created, date_modified;
	multiflag<password_editor::ItemFlag> flags;
	std::vector<PasswordEditorRowV1View> rows;
};

struct CardRowData {
	str description, value;
	multiflag<password_editor::ValueCellFlag> flags = 0;
};

struct CardData {
	str title;
	password_editor::Properties properties{};
	std::vector<CardRowData> rows;
	bool is_open = false;
	bool is_editing = false;
};

struct CardTextViewState {
	UndoHistory<edit_oneline::TextEditEntry> history;
	edit_oneline::UndoMergeState history_helper;
	edit_oneline::CharSel selection;
	v2_i32 scroll{};
	bool trailing = false;
	bool valid = false;
};

struct CardRowTextViewState {
	CardTextViewState description, value;
};

struct CardSlot {
	CardData data;
	HWND wnd = nil;
	bool search_visible = true;
	RECT bounds{};
	CardTextViewState title_view;
	std::vector<CardRowTextViewState> row_views;
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
private: void _() { static_assert(sizeof(all_fixed) == sizeof(*this)); }
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
	std::vector<CardSlot> cards;
	Settings* settings;

	str current_user;
	utf16 salt[8];
	bool passwords_need_save;

	SortOption sorting;

	Mode mode;

	CardLayoutState card_layout;
};
}
