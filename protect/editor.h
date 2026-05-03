#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "global.h"
#include "edit_subclass.h"
#include "language_manager.h"
#include "search.h"

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

auto get_state(HWND wnd) { return get_window_state<State>(wnd); }

void set_passwords_need_save(State& state, bool new_val) {
	state.passwords_need_save = new_val;
	str txt;
	if (state.current_user) txt = state.current_user;
	if (state.passwords_need_save) txt += L" ●";
	SetText_txt_app(state.nc_parent, txt.c_str(), app_name);
}

void on_password_editors_cnt_changed(State& state) {
	ShowWindow(state.controls.btn_add_end, state.controls.password_editors.size() ? SW_SHOW : SW_HIDE);
}

struct props { const utf16* title = nil; time_t date_created = 0, date_modified = 0; };
password_editor::State& add_password_editor(State& state, const props& properties, size_t at_idx) {
	auto wnd = create_window(state.controls.page, password_editor::wndclass, nil, WS_VISIBLE | WS_CHILD, 0, 0, state.wnd);
	password_editor::set_user_data(wnd, &state);
	password_editor::set_properties(wnd, {.date_created = properties.date_created, .date_modified = properties.date_modified});
	auto& res = *password_editor::get_state(wnd);
	AWDT(res.controls.edo_title, LANG_PWD_ED_TITLE);
	password_editor::set_functions(wnd, {
		.on_delete = [](HWND pwd_ed, void* data) {
			auto& state = *(State*)data;
			auto& vec = state.controls.password_editors;
			for (const auto& [i, c] : vec | std::views::enumerate)
				if (c == pwd_ed) {
					DestroyWindow(c);
					vec.erase(vec.begin() + i);
					on_password_editors_cnt_changed(state);
					ask_window_for_resize(state.wnd);
					ask_window_for_repaint(state.wnd);
					//TODO(fran): small BUG with scrolling: if we delete a section while we are scrolled, then we dont get a proper page resize with repositioning and thus the page remains overscrolled when it could have corrected itself.
					break;
				}
		}
	});
	auto& vec = state.controls.password_editors;
	auto at = at_idx < vec.size() ? vec.begin() + at_idx : vec.end();
	vec.insert(at, wnd);
	on_password_editors_cnt_changed(state);
	if (properties.title) SetWindowTextW(res.controls.edo_title, properties.title);
	ask_window_for_resize(state.wnd);
	ask_window_for_repaint(state.wnd);
	return res;
}

void add_password_editor_row(password_editor::State& editor, const utf16* description, const utf16* value) {
	auto& tbl = *table::get_state(editor.controls.tbl_values);
	table::add_row(tbl, description, value);
}

void add_preset_password_editor(State& state, size_t at_idx) {
	props properties{ .title = L"", .date_created = std::time(nil), .date_modified = properties.date_created };
	auto& password_editor = add_password_editor(state, properties, at_idx);
	add_password_editor_row(password_editor, RCS(LANG_PWD_ED_TBL_USER), L"");
	add_password_editor_row(password_editor, RCS(LANG_PWD_ED_TBL_PASSWORD), L"");
	password_editor::set_is_open(password_editor, true);
	ask_window_for_resize(state.wnd);
	ask_window_for_repaint(state.wnd);
}

void setup_combobox_sorting_options(State& state, HWND combo) {
	auto controls = combobox::get_controls(combo);
	Assert(controls.listbox);
	sort_combo_item items[] = {
		{.value = sort_option::date_modified_newest, .label_id = LANG_EDITOR_SORT_DATE_MODIFIED_NEWEST },
		{.value = sort_option::date_modified_oldest, .label_id = LANG_EDITOR_SORT_DATE_MODIFIED_OLDEST },
		{.value = sort_option::alphabetic_az, .label_id = LANG_EDITOR_SORT_ALPHABETIC_AZ },
		{.value = sort_option::alphabetic_za, .label_id = LANG_EDITOR_SORT_ALPHABETIC_ZA },
		{.value = sort_option::date_created_newest, .label_id = LANG_EDITOR_SORT_DATE_CREATED_NEWEST },
		{.value = sort_option::date_created_oldest, .label_id = LANG_EDITOR_SORT_DATE_CREATED_OLDEST },
	};
	listbox::add_elements_by_value(controls.listbox, (void**)items, ARRAYSIZE(items));

	combobox::set_cur_sel(combo, (size_t)state.sorting);
	ask_window_for_repaint(combo);
}

void resize_controls(State& state) {
	auto& controls = state.controls;
	RECT r; GetClientRect(state.wnd, &r);
	auto w = RECTW(r), h = RECTH(r);

	i32 offset = 0;
	if constexpr (debug_text_view) {
		rect_i32 edit_showpasswords{ .x = 0, .y = 0, .w = w, .h = (i32)(h * .25f) };

		MoveWindow(controls.edit_passwords, edit_showpasswords);
		offset = h * .25f;
	}

	i32 spacing = DPI(15); //TODO: add to theme

	offset += spacing;

	auto search_max_w = avg_str_dim(GetWindowFont(controls.search), 35).cx;
	bool sort_on_next_line = w <= search_max_w * 1.5f;
	rect_i32 search{ 
		.x = (i32)(w * pad_percent),
		.y = offset, 
		.w = minimum((sort_on_next_line ? w - search.x : w / 2) - search.x, search_max_w),
		.h = (i32)DPI(30)
	};
	MoveWindow(controls.search, search);
	
	i32 sort_w = search.w * .9f;
	rect_i32 sort{ 
		.x = sort_on_next_line ? search.x : (i32)(w * (1 - pad_percent) - sort_w),
		.y = sort_on_next_line ? search.bottom() + spacing : search.y,
		.w = sort_w,
		.h = search.h
	};
	MoveWindow(controls.combo_sort, sort);
		
	offset = sort.bottom() + spacing;

	rect_i32 page_space{ .x = 0, .y = offset, .w = w, .h = h - offset };
	MoveWindow(controls.page_space, page_space, false);

	offset = 0;
	i32 btn_add_dim = DPI(30);
	i32 btn_add_x = (w - btn_add_dim) / 2;

	if (controls.password_editors.size()) {
		auto btn_add_start_y = offset + spacing - btn_add_dim / 2;
		MoveWindow(controls.btn_add_start, btn_add_x, btn_add_start_y, btn_add_dim, btn_add_dim, true);
		auto old_offset = offset;
		offset = btn_add_start_y + btn_add_dim + spacing;
		bool any_visible = false;
		for (const auto& p : controls.password_editors) {
			if (IsWindowVisible(p)) {
				any_visible = true;
				const auto& pstate = *password_editor::get_state(p);
				auto item_h = password_editor::resize_controls(pstate, false);
				MoveWindow(p, w * pad_percent, offset, w * (1 - pad_percent * 2), item_h, true);
				offset += (item_h + spacing);
			}
		}
		if (!any_visible) {
			offset = old_offset;
			goto empty_editor;
		}

		MoveWindow(controls.btn_add_end, btn_add_x, offset, btn_add_dim, btn_add_dim, true);
		offset += btn_add_dim;
	}
	else {
		empty_editor:
		RECT btn_add_start{ .left = (w - btn_add_dim) / 2, .top = offset + spacing - btn_add_dim / 2, .right = btn_add_start.left + btn_add_dim, .bottom = btn_add_start.top + btn_add_dim };
		MoveWindow(controls.btn_add_start, btn_add_x, offset + spacing, btn_add_dim, btn_add_dim, true);
		offset += (spacing + btn_add_dim);

		MoveWindow(controls.btn_add_end, DPI(-100), 0, btn_add_dim, btn_add_dim, true); //HACK: hide end button without having to worry about restoring visibility states
	}
	offset += spacing;
	page::set_wnd_size(controls.page, controls.page_space, offset); //TODO(fran): it may be better to resize the page before all of its children, so that we dont cause issues where the children cant re-render because they arent within the visible area of the parent (this may be a non issue though, and by tracking scrolling events and making visible and rendering the proper children this may fix itself)
}

void add_controls(State& state) {
	auto& controls = state.controls;

	if constexpr (debug_text_view) {
		//TODO(fran): EM_SETENDOFLINE allows you to change between EC_ENDOFLINE_CRLF EC_ENDOFLINE_CR EC_ENDOFLINE_LF
		controls.edit_passwords = CreateWindowExW(NULL, L"Edit", NULL, WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_CLIPCHILDREN | WS_VISIBLE | ES_NOHIDESEL /*to show selection even when you dont have the focus*/ //| WS_VSCROLL | WS_HSCROLL 
			, 0, 0, 0, 0
			, state.wnd
			, NULL, NULL, NULL);

		constexpr auto EDIT_PASSWORDS_MAX_TEXT_LENGTH = 32767 * 2; //32767 is the default
		SendMessageW(controls.edit_passwords, EM_SETLIMITTEXT, (WPARAM)EDIT_PASSWORDS_MAX_TEXT_LENGTH, NULL);

		//TODO: the window subclass turned out to be the cause of the permanent wm_paint re-renders, not sure why, extract the functionality we need and throw it away
		//SetWindowSubclass(controls.edit_passwords, EditProc, 0, (DWORD_PTR)calloc(1, sizeof(EditProcState)));

		HWND VScrollControl = CreateWindowExW(NULL, scrollbar::wndclass, NULL, WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, controls.edit_passwords, NULL, NULL, NULL);
		SendMessage(VScrollControl, scrollbar::U_SB_SET_PLACEMENT, (WPARAM)scrollbar::Placement::right, 0);

		SendMessageW(controls.edit_passwords, EM_SETVSCROLL, (WPARAM)VScrollControl, 0);

		//INFO: I dont yet paint edit controls so you gotta use WM_CTLCOLOREDIT

		search::Init searchinit;
		searchinit.parent_type = search::ParentType::edit;
		searchinit.SearchFlag_flags = 0;
		searchinit.SearchPlacement_flags = search::Placement::bottom;
		HWND SearchControl = CreateWindowExW(NULL, search::wndclass, NULL, WS_CHILD,
			0, 0, 0, 0, controls.edit_passwords, NULL, NULL, &searchinit);
		SendMessageW(controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)SearchControl, 0);
		SendMessage(SearchControl, WM_SETFONT, (WPARAM)fonts.General, TRUE);
	}

	controls.page_space = create_window(state.wnd, page::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS); //TODO(fran): WS_CLIPCHILDREN?

	controls.page = create_window(controls.page_space, page::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS); //TODO(fran): WS_CLIPCHILDREN?
	page::set_scrolling(controls.page, true);
	//TODO(fran): add support for the page to manage a vertical scrollbar, add page::set_use_scrollbar(controls.page, true or false); // to allow us to select whether we want the scrollbar to be visible to the user when the page can scroll

	auto btn_add_theme = themes.editor_add_btn;

	controls.btn_add_start = create_window(controls.page, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
	button::set_theme(controls.btn_add_start, btn_add_theme);
	SendMessage(controls.btn_add_start, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.add);
	button::set_user_data(controls.btn_add_start, &state);
	button::set_functions(controls.btn_add_start, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			add_preset_password_editor(state, 0);
		}
	});
	add_mouseover_tooltip(controls.btn_add_start, LANG_EDITOR_ADD);

	controls.btn_add_end = create_window(controls.page, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
	button::set_theme(controls.btn_add_end, btn_add_theme);
	SendMessage(controls.btn_add_end, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.add);
	button::set_user_data(controls.btn_add_end, &state);
	button::set_functions(controls.btn_add_end, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			add_preset_password_editor(state, -1);
		}
	});
	add_mouseover_tooltip(controls.btn_add_end, LANG_EDITOR_ADD);

	search::Init global_search_init {
		.SearchPlacement_flags = search::Placement::top,
		.SearchFlag_flags = 0,
		.parent_type = search::ParentType::custom,
	};
	controls.search = CreateWindowExW(WS_EX_COMPOSITED | WS_EX_TRANSPARENT, search::wndclass, NULL, WS_VISIBLE | WS_CHILD,
		0, 0, 0, 0, state.wnd, NULL, NULL, &global_search_init);
	search::set_theme(controls.search, themes.base_search);
	search::set_user_data(controls.search, &state);
	search::set_functions(controls.search, {
		.on_search = [](utf16* match, i32 match_len, multiflag<search::Flag> search_flags, bool search_in_current_selection, void* user_data) {
			auto& state = *(State*)user_data;
			if (!match_len || !match || !*match) // search bar has been cleared, show everything
				for (auto p : state.controls.password_editors) 
					ShowWindow(p, SW_SHOW);
			else
				for (auto p : state.controls.password_editors) {
					auto& pw_ed = *password_editor::get_state(p);
					utf16 title[50]; 
					i32 title_len = GetWindowText(pw_ed.controls.edo_title, title, ARRAYSIZE(title));

					if (!title_len) { ShowWindow(p, SW_SHOW); continue; } //always keep ones that are still being edited
				
					DWORD search_flags = LINGUISTIC_IGNOREDIACRITIC | LINGUISTIC_IGNORECASE | FIND_FROMSTART;
					bool found = FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, search_flags, title, title_len, match, match_len, nil, nil, nil, 0) != -1;
					ShowWindow(p, found ? SW_SHOW : SW_HIDE);
				}
			//TODO(fran): decide whether we want to hide the items that dont match, or just move them to the bottom of the list to be shown as less priority, could even be rendered in semi transparent or something to indicate that they dont match, kinda pointless though, if I do that I have to block input to them too since editing something while it's semi invisible is not good ux
			ask_window_for_resize(state.wnd);
			ask_window_for_repaint(state.wnd);
}
	});
	//AWDYN(controls.search, WM_SIZE); // Make sure that the language strings for search fit in the search button, I dont need this since I do use any of the buttons with text, but could be useful in other situations

	//SendMessageW(controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)controls.search, 0);
	
	controls.combo_sort = create_window(state.wnd, combobox::wndclass);
	auto combo_sort_controls = combobox::get_controls(state.controls.combo_sort);
	AWDYN(combo_sort_controls.button, WM_PAINT);
	AWDYN(combo_sort_controls.listbox, WM_SIZE); // triggers a full backbuffer redraw of all its items
	setup_combobox_sorting_options(state, controls.combo_sort);
	//button::set_theme(combobox::get_controls(state.controls.combo_sort).button, themes.base_btn);
	combobox::set_user_extra(controls.combo_sort, &state);
	//combobox::set_function_render_combobox(controls.combo_sort, [](HDC dc, rect_i32 r, combobox::render_flags flags, void* element, void* user_extra) {
	//	auto item = std::bit_cast<sort_combo_item>(element);
	//	auto& state = *(State*)user_extra;
	//	auto btn = combobox::get_controls(state.controls.combo_sort).button;
	//	auto new_title = RCS(item.label_id);
	//	utf16 current_title[50]; current_title[0] = 0;
	//	GetWindowText(btn, current_title, ARRAYSIZE(current_title));
	//	if (!wcscmp(current_title, new_title)) SetWindowText(btn, new_title); //Hacky way of updating the text also on language change just on redraw, otherwise we would have to explicitly tell the control to change its text on language change
	//	button::set_theme(btn, themes.base_btn);
	//	button::proc(btn, WM_PAINT, 0, 0); //TODO(fran): hack to trigger default button rendering, add support for doing this from the combobox
	//});
	//combobox::set_function_on_listbox_opening(controls.combo_sort, langbox_func_on_listbox_opening);
	//combobox::set_function_desired_size_combobox(controls.combo_sort, langbox_func_desired_size);
	combobox::set_function_on_selection_accepted(controls.combo_sort, [](void* element, void* user_extra) {
		auto& state = *(State*)user_extra;
		auto item = std::bit_cast<sort_combo_item>(element);
		static_assert((i32)sort_option::__last == 6, "Enum element count changed, update sorting logic to consider the new values");

		combobox::set_cur_sel(state.controls.combo_sort, (size_t)item.value);

		struct sort_alphabetic { HWND wnd; std::wstring title; };
		struct sort_date { HWND wnd; time_t date; };

		auto& vec = state.controls.password_editors;
		switch (item.value) {
		case sort_option::alphabetic_az:
		case sort_option::alphabetic_za:
		{
			std::vector<sort_alphabetic> to_sort; to_sort.reserve(vec.size());
			for (auto p : vec) {
				auto title = password_editor::get_state(p)->controls.edo_title;
				auto len = GetWindowTextLength(title) + 1;
				sort_alphabetic element;
				element.wnd = p;
				element.title.resize_and_overwrite(len,
					[=](utf16* buf, size_t buf_size) { return GetWindowText(title, buf, buf_size); });
				to_sort.push_back(std::move(element));
			}
			auto sorting_function = (item.value == sort_option::alphabetic_az) ?
				[](const sort_alphabetic& a, const sort_alphabetic& b) {
				return _wcsicmp(a.title.c_str(), b.title.c_str()) < 0;
				} :
				[](const sort_alphabetic& a, const sort_alphabetic& b) {
					return _wcsicmp(a.title.c_str(), b.title.c_str()) > 0;
				};
			std::sort(to_sort.begin(), to_sort.end(), sorting_function);
			for (const auto& [i, value] : to_sort | std::views::enumerate) vec[i] = value.wnd;
		} break;
		case sort_option::date_modified_newest:
		case sort_option::date_modified_oldest:
		{
			std::vector<sort_date> to_sort; to_sort.reserve(vec.size());
			for (auto p : vec) to_sort.emplace_back(p, password_editor::get_state(p)->properties.date_modified);
			auto sorting_function = (item.value == sort_option::date_modified_newest) ?
				[](sort_date a, sort_date b) { return a.date > b.date; } :
				[](sort_date a, sort_date b) { return a.date < b.date; };
			std::sort(to_sort.begin(), to_sort.end(), sorting_function);
			for (const auto& [i, value] : to_sort | std::views::enumerate) vec[i] = value.wnd;
		} break;
		case sort_option::date_created_newest:
		case sort_option::date_created_oldest:
		{
			std::vector<sort_date> to_sort; to_sort.reserve(vec.size());
			for (auto p : vec) to_sort.emplace_back(p, password_editor::get_state(p)->properties.date_created);
			auto sorting_function = (item.value == sort_option::date_created_newest) ?
				[](sort_date a, sort_date b) { return a.date > b.date; } :
				[](sort_date a, sort_date b) { return a.date < b.date; };
			std::sort(to_sort.begin(), to_sort.end(), sorting_function);
			for (const auto& [i, value] : to_sort | std::views::enumerate) vec[i] = value.wnd;
		} break;
		default: Assert(0);
		}
		ask_window_for_resize(state.wnd);
	});
	combobox::set_function_render_combobox(controls.combo_sort, [](HDC dc, rect_i32 r, combobox::render_flags flags, void* element, void* user_extra) {
		auto& theme = themes.base_btn;
		HFONT font = fonts.General;
		HBRUSH bk_br, txt_br, border_br = nil;
		if (flags.isListboxOpen) bk_br = theme.brushes.bk.normal;
		else if (flags.onClicked) bk_br = theme.brushes.bk.clicked;
		else if (flags.onMouseover) bk_br = theme.brushes.bk.mouseover;
		else bk_br = theme.brushes.bk.disabled;

		int x_pad = avg_str_dim(font, 1).cx;

		urender::draw_background(dc, r.to_RECT(), bk_br, border_br, theme.dimensions);
		
		int icon_x = urender::draw_bitmap_1bpp_right(bmps.dropdown, dc, r, x_pad, theme.brushes.foreground.normal);

		std::wstring txt;
		if (element) {
			txt = RS(std::bit_cast<sort_combo_item>(element).label_id);
			txt_br = colors.ControlTxt;
		} else {
			txt = RS(LANG_EDITOR_SORT);
			txt_br = colors.ControlTxt_Disabled;
		}

		auto oldfont = SelectFont(dc, font); defer{ SelectFont(dc, oldfont); };
		auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };
		SetTextColor(dc, ColorFromBrush(txt_br));

		RECT txt_rc = r.to_RECT();
		txt_rc.left += x_pad;
		txt_rc.right = icon_x - x_pad;

		DrawTextW(dc, txt.c_str(), txt.size(), &txt_rc, DT_EDITCONTROL | DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	});
	combobox::set_function_render_listbox_element(controls.combo_sort, [](HDC dc, rect_i32 r, listbox::renderflags flags, void* element, void* user_extra) {
		int w = r.w, h = r.h;
		auto item = std::bit_cast<sort_combo_item>(element);
		auto txt = RS(item.label_id);

		//Draw bk
		HBRUSH bk_br = colors.ControlBk, txt_br = colors.ControlTxt;
		if (flags.onMouseover || flags.onSelected) bk_br = colors.ControlBkMouseOver;
		if (flags.onClicked) bk_br = colors.ControlBkPush;

		RECT bk_rc = r.to_RECT();
		FillRect(dc, &bk_rc, bk_br);

		//Draw text
		HFONT font = fonts.General;
		RECT txt_rc = bk_rc;

		urender::draw_text(dc, txt_rc, to_utf_str(txt), font, txt_br, urender::txt_align::left, avg_str_dim(font, 1).cx);
	});

	for (auto ctl : controls.all_fixed) SetWindowFont(ctl, (WPARAM)fonts.General, true);

	resize_controls(state);
}

#define showpasswords_menu_base_addr	200
#define SHOWPASSWORDS_MENU_SAVE			(showpasswords_menu_base_addr+1)
#define SHOWPASSWORDS_MENU_SEPARATOR1	(showpasswords_menu_base_addr+2)
#define SHOWPASSWORDS_MENU_UNDO			(showpasswords_menu_base_addr+3)
#define SHOWPASSWORDS_MENU_REDO			(showpasswords_menu_base_addr+4)
#define SHOWPASSWORDS_MENU_FIND			(showpasswords_menu_base_addr+5)

void add_menus(State& state) { //TODO(fran): this should be a toolbar (maybe), toolbars are kinda stupid, just useful till you learn shortcuts https://docs.microsoft.com/en-us/windows/win32/controls/create-toolbars
	//NOTE: each menu gets its parent HMENU stored in the itemData part of the struct
	LANGUAGE_MANAGER::Instance().AddMenuDrawingHwnd(state.wnd);

	//INFO: the top 32 bits of an HMENU are random each execution, in a way, they can actually get set to FFFFFFFF or to 00000000, so if you're gonna check two of those you better make sure you cut the top part in BOTH

	HMENU menu = CreateMenu(); state.menu = menu;
	HMENU menu_file = CreateMenu(); state.menu_file = menu_file;
	HMENU menu_file_lang = CreateMenu(); state.menu_file_lang = menu_file_lang;
	HMENU menu_edit = CreateMenu(); state.menu_edit = menu_edit;
	AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)menu_file, (LPCWSTR)menu);
	AMT(menu, (UINT_PTR)menu_file, LANG_MENU_FILE);

	AppendMenuW(menu_file, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_SAVE, (LPCWSTR)menu_file);
	AMT(menu_file, SHOWPASSWORDS_MENU_SAVE, LANG_MENU_SAVE);

	AppendMenuW(menu_file, MF_SEPARATOR | MF_OWNERDRAW, SHOWPASSWORDS_MENU_SEPARATOR1, (LPCWSTR)menu_file);

	AppendMenuW(menu_file, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)menu_file_lang, (LPCWSTR)menu_file);
	AMT(menu_file, (UINT_PTR)menu_file_lang, LANG_MENU_LANGUAGE);
	//TODO(fran): SetMenuItemInfo only accepts UINT, not the UINT_PTR of MF_POPUP, plz dont tell me I have to redo all of it a different way (LANGUAGE_MANAGER just does it normally not caring for the extra 32 bits)

#define _language_appendtomenu(member,value_expr) \
		AppendMenuW(menu_file_lang, MF_STRING | MF_OWNERDRAW, LANGUAGE::member, (LPCWSTR)menu_file_lang); \
		SetMenuItemString(menu_file_lang, LANGUAGE::member, FALSE, _t(#member)); \
		SetMenuItemBitmaps(menu_file_lang, LANGUAGE::member, MF_BYCOMMAND, NULL, bmps.circle); \

	_foreach_language(_language_appendtomenu)
#undef _language_appendtomenu
	CheckMenuItem(menu_file_lang, LANGUAGE_MANAGER::Instance().GetCurrentLanguage(), MF_BYCOMMAND | MF_CHECKED);

	SetMenuItemBitmaps(menu_file, (UINT)(UINT_PTR)menu_file_lang, MF_BYCOMMAND, bmps.language, bmps.language);

	AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)menu_edit, (LPCWSTR)menu);
	AMT(menu, (UINT_PTR)menu_edit, LANG_MENU_EDIT);

	AppendMenuW(menu_edit, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_UNDO, (LPCWSTR)menu_edit);
	AMT(menu_edit, SHOWPASSWORDS_MENU_UNDO, LANG_MENU_EDIT_UNDO);

	AppendMenuW(menu_edit, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_REDO, (LPCWSTR)menu_edit);
	AMT(menu_edit, SHOWPASSWORDS_MENU_REDO, LANG_MENU_EDIT_REDO);

	AppendMenuW(menu_edit, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_FIND, (LPCWSTR)menu_edit);
	AMT(menu_edit, SHOWPASSWORDS_MENU_FIND, LANG_MENU_EDIT_FIND);

	nonclient::set_menu(state.nc_parent, state.menu);
}

void save_settings(State& state) {
	RECT r; GetWindowRect(state.wnd, &r);
	state.settings->rc = r;
}

///username: serves as the user folder name
static bool save_to_file_user(std::wstring username, void* content, u32 content_sz) {
	constexpr wchar_t filename[] = L"\\tt";
	constexpr wchar_t temp_filename[] = L"\\temp";
	constexpr wchar_t last_filename[] = L"\\last"; //TODO(fran): Saving the last state of the file is good in case the new one has gotten corrupted and needs to be recovered. But on the other hand it is a security concern because it shows the state change between saves, giving away information about our type of encryption (eg making it obvious that our encryption is made in independent chunks)

	std::wstring path = get_general_save_folder() + L"\\user_" + username; //INFO: appending user_ saves us from the trouble of the reserved filenames that windows has, eg COM1, LPT1, ...

	CreateDirectoryW(path.c_str(), 0);//Create the folder where info will be stored, since windows wont do it

	SetFileAttributesW(path.c_str(), GetFileAttributesW(path.c_str()) | FILE_ATTRIBUTE_HIDDEN); //some very basic protection

	std::wstring full_path_temp = path + temp_filename;

	bool res = write_entire_file(full_path_temp.c_str(), content, content_sz);

	if (res) {
		auto file_read = read_entire_file(full_path_temp.c_str()); defer{ free_file_memory(file_read.mem); };
		res = file_read.mem && (file_read.sz == content_sz) && (memcmp(content, file_read.mem, content_sz) == 0); //TODO(fran): there's no real need to check the entirety of the contents, we could simply check a few tens of bytes from the beginning, middle and end
		if (res) {
			std::wstring full_path_last = path + last_filename;
			path += filename;
			MoveFileExW(path.c_str(), full_path_last.c_str(), MOVEFILE_REPLACE_EXISTING);
			res = MoveFileExW(full_path_temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
		}
	}

	return res;
}

static read_entire_file_res load_file_user(std::wstring username /*functions as a folder*/) {
	constexpr wchar_t filename[] = L"\\tt";
	std::wstring path = get_general_save_folder() + L"\\user_" + username;

	CreateDirectoryW(path.c_str(), 0);//Create the folder where info will be stored, since windows wont do it

	SetFileAttributesW(path.c_str(), GetFileAttributesW(path.c_str()) | FILE_ATTRIBUTE_HIDDEN); //some very basic protection

	path += filename;

	auto res = read_entire_file(path.c_str());
	return res;
}

void save_passwords(State& state) {
	int user_len_chars = (int)wcslen(state.current_user);
	int len_chars = user_len_chars + GetWindowTextLength(state.controls.edit_passwords) + 1;
	int len_bytes = next_multiple_of_16(len_chars * sizeof(cstr)); //we pad with extra garbage bytes to get blocks of 16 bytes for encryption
	void* mem = malloc(len_bytes); defer{ free(mem); };
	Assert(sizeof(cstr) > 1); //TODO(fran): what to do with ansi?
	wcscpy_s((cstr*)mem, user_len_chars + 1, state.current_user); //append username so we can check against it in later logins (another idea is to append the key structure that twofish stores ) //TODO(fran): this aint the most clever, there could be collisions, but it's at least a line of defense for now
	GetWindowText(state.controls.edit_passwords, ((cstr*)mem) + user_len_chars, len_chars - user_len_chars);
	twofish_encrypt(mem, len_bytes, mem);
	
	bool res = save_to_file_user(state.current_user, mem, len_bytes);
	set_passwords_need_save(state, !res);
	if (!res) MessageBox(state.wnd, RCS(LANG_ERROR_SAVEFILE_PASSWORDS), RCS(LANG_ERROR), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
}

template<typename T> //I give up trying to write down the entire type of a string view subrange coming from std::views::split
void terminate_string_view(const T& str) {
	*const_cast<utf16*>(str.data() + str.size()) = 0;
}

void create_password_editors(State& state, utf16* data) {
	using chrtype = utf16;
	using strtype = chrtype*;
	using std::operator""sv;
	const auto item_separator = L"\r\n\r\n"sv;
	for (const auto& item : std::views::split(std::wstring_view(data), item_separator)) {
		terminate_string_view(item);
		const auto title_separator = L'\n', title_prop_separator = L':';
		auto title_end = StrChrW(item.data(), title_separator);
		if (title_end) {
			*title_end = 0;
			props properties;
			for (const auto& [i, title_prop] : std::views::split(std::wstring_view(item.data(), title_end), title_prop_separator) | std::views::enumerate) {
				auto val = const_cast<strtype>(title_prop.data());
				terminate_string_view(title_prop);
				switch (i) {
				case 0: properties.title = strip(val); continue;
				case 1: properties.date_created = wcstoll(val, nil, 10); continue;
				case 2: properties.date_modified = wcstoll(val, nil, 10); continue;
				}
				break;
			}
			if (properties.date_created == 0 || properties.date_created == I64MAX || properties.date_created == I64MIN)
				properties.date_created = std::time(nil);
			if (properties.date_modified == 0 || properties.date_modified == I64MAX || properties.date_modified == I64MIN)
				properties.date_modified = properties.date_created;

			title_end++;
			auto password_editor = add_password_editor(state, properties, -1);
			const auto row_separator = title_separator;
			for (const auto& row : std::views::split(std::wstring_view(title_end), row_separator)) {
				const auto col_separator = title_prop_separator;
				strtype desc_value[2]{0};
				for (const auto& [i, col] : std::views::split(row, col_separator) | std::views::enumerate) {
					if (i >= 2) break;
					desc_value[i] = const_cast<strtype>(col.data());
					terminate_string_view(col);
					desc_value[i] = strip(desc_value[i]);
				}
				add_password_editor_row(password_editor, desc_value[0], desc_value[1]);
			}
		}
	}
	resize_controls(state);
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		Assert(creation_nfo->style & WS_CHILD);

		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		st->init();
		st->wnd = hwnd;
		st->nc_parent = GetParent(hwnd);

		st->settings = ((Data*)creation_nfo->lpCreateParams)->settings;
		st->start = ((Data*)creation_nfo->lpCreateParams)->start;
		set_window_state(hwnd, st);
		return 1;
	} break;
	case WM_CREATE:
	{
		add_controls(state);

		add_menus(state);
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		resize_controls(state);
		return res;
	} break;
	case WM_SAVE:
	{
		HWND ctl = (HWND)wparam;
		if (ctl == state.controls.edit_passwords) {
			save_passwords(state);
			return 0;
		}
	} break;
	case WM_COMMAND:
	{
		HWND child = (HWND)lparam;
		if (child) { // Child notifications
			if (child == state.controls.edit_passwords) {
				switch (HIWORD(wparam)) { //Notif code
				case EN_CHANGE:
				{
					set_passwords_need_save(state, true);
					return 0;
				}
				}
			}
		}
		else {
			switch (LOWORD(wparam)) { // Menu notifications
			case SHOWPASSWORDS_MENU_SAVE:
			{
				save_passwords(state);
				return 0;
			} break;
#define _generate_enum_cases_language(member,value_expr) case LANGUAGE::member:
			_foreach_language(_generate_enum_cases_language)
			{
				LANGUAGE_MANAGER::Instance().ChangeLanguage((LANGUAGE)LOWORD(wparam));
				HMENU old_menu = GetMenu(state.nc_parent);
				add_menus(state);
				DestroyMenu(old_menu);
				return 0;
			} break;
			case SHOWPASSWORDS_MENU_UNDO:
			{
				SendMessage(state.controls.edit_passwords, EM_UNDO, 0, 0); //TODO(fran): undo is terrible in edit controls, we gotta manage that in the subclass //NOTE: WM_UNDO seems to do the same
			} break;
			case SHOWPASSWORDS_MENU_REDO:
			{

			} break;
			case SHOWPASSWORDS_MENU_FIND:
			{
				SendMessage(state.controls.edit_passwords, EM_SHOWSEARCHWND, TRUE, 0);
				return 0;
			} break;
			default: return DefWindowProc(hwnd, msg, wparam, lparam);
			}
		}
		return 0;
	} break;
	case WM_START:
	{
		login::AttemptResult start_attempt;
		twofish_setkey(state.start->key, sizeof(state.start->key));
		ZeroMemory(state.start->key, sizeof(state.start->key));

		//We gotta keep this data to be able to save the file later, probably some more secure/intelligent ways exist
		state.current_user = (wchar_t*)malloc((state.start->username.sz_chars + 1) * sizeof(*state.start->username.str));
		state.current_user[state.start->username.sz_chars] = 0; //append null terminator, unfortunately the rest of the code isnt yet working with the "text" struct
		memcpy(state.current_user, state.start->username.str, state.start->username.sz_chars * sizeof(*state.start->username.str));
		//TODO(fran): free

		auto file_read = load_file_user(state.current_user); defer{ free_file_memory(file_read.mem); };//TODO(fran): zero
		bool passwords_need_save = false;
		if (file_read.mem) {
			Assert(file_read.sz % 16 == 0);
			twofish_decrypt(file_read.mem, file_read.sz, file_read.mem);
			if (!wcsncmp(state.current_user, (wchar_t*)file_read.mem, minimum(state.start->username.sz_chars, file_read.sz / 2 /*byte to wchar*/))) { //Valid password, user inputted username matches stored username
				SetWindowTextW(state.controls.edit_passwords, ((cstr*)file_read.mem) + state.start->username.sz_chars); //TODO(fran): what did I decide for the data's encoding?
				//TODO(fran): set keyboard focus to the edit control, SetFocus doesnt work here, maybe cause we arent visible yet?
				create_password_editors(state, ((cstr*)file_read.mem) + state.start->username.sz_chars);
				start_attempt = login::AttemptResult::success;
			}
			else { //Invalid password
				start_attempt = login::AttemptResult::fail_password;
			}
		} else {
			//NOTE: if the user previously created an account but didnt save then it will not count as a created account and next time they will be prompted to create the account again, this is a limitation of the fact that we dont save anything inside the user folder till the first time they save what they wrote, therefore we cannot currently do this any other way since there's no information inside the folder to allow us to check whether the second time the user input the same password as the first time
			bool signup = state.start->signup;
			start_attempt = signup ? login::AttemptResult::success : login::AttemptResult::fail_username;
			passwords_need_save = signup;
		}
		set_passwords_need_save(state, passwords_need_save);
		return (LRESULT)start_attempt;
	} break;
	case WM_RESET:
	{
		if (state.current_user) {
			free(state.current_user);//No real need to zero the memory before freeing
			state.current_user = nullptr;
		}
		SetWindowText(state.controls.edit_passwords, _t(""));//TODO(fran): yet again, we need to zero the mem also
		return 0;
	} break;
	case WM_CLOSE: //Sent by our parent asking whether we want to handle it
	{
		bool client_handled = false;
		if (state.passwords_need_save) {
			int ret = MessageBox(state.wnd, RCS(LANG_UNSAVEDCHANGES_TXT), RCS(LANG_UNSAVEDCHANGES_TITLE), MB_YESNOCANCEL | MB_ICONWARNING | MB_SETFOREGROUND | MB_APPLMODAL);
			switch (ret) {
			case IDCANCEL:
			{
				client_handled = true;
			} break;
			case IDYES:
			{
				save_passwords(state);
			} break;
			case IDNO:
			{
				//Do nothing and let the nc handle the msg
			} break;
			default:client_handled = true; break;
			}
		}
		return client_handled;
	} break;
	case WM_NCDESTROY:
	{
		save_settings(state);
		if (&state) {
			state.uninit();
			if (state.current_user) {
				free(state.current_user);
				state.current_user = nullptr;
			}
			set_window_state(state.wnd, nil);
			free(&state);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CTLCOLOREDIT:
	{
		HWND ctl = (HWND)lparam;
		HDC dc = (HDC)wparam;
		if (ctl == state.controls.edit_passwords)
		{
			SetBkColor(dc, ColorFromBrush(colors.ControlBk));
			SetTextColor(dc, ColorFromBrush(colors.ControlTxt));
			return (LRESULT)colors.ControlBk;
		}
		else return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ERASEBKGND:
	{
		auto dc = (HDC)wparam;
		RECT r; GetClientRect(state.wnd, &r);
		FillRect(dc, &r, colors.ControlBk);
		auto card_extent = DPI(10); //TODO: add to theme
		RECT card;

		auto old_rgn = CreateRectRgn(0, 0, 0, 0); GetClipRgn(dc, old_rgn); defer{ DeleteObject(old_rgn); };
		auto clip = get_window_rect_at(state.controls.page_space, state.wnd);
		IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom); defer{ SelectClipRgn(dc, old_rgn); };

		HWND first_visible_pw_ed = nil; 
		for (auto p : state.controls.password_editors) if (IsWindowVisible(p)) { first_visible_pw_ed = p; break; }

		if (first_visible_pw_ed) {
			RECT btn_add_start = get_window_rect_at(state.controls.btn_add_start, state.wnd);
			RECT btn_add_end = get_window_rect_at(state.controls.btn_add_end, state.wnd);
			RECT first = get_window_rect_at(first_visible_pw_ed, state.wnd);
			card = first;
			card.top = btn_add_start.top + RECTH(btn_add_start) / 2;
			card.bottom = btn_add_end.top + RECTH(btn_add_end) / 2;
			InflateRect(&card, card_extent, 0);
		}
		else {
			RECT btn_add_start = get_window_rect_at(state.controls.btn_add_start, state.wnd);
			auto w = RECTW(r);
			auto spacing = DPI(10);
			card.left = w * pad_percent;
			card.top = btn_add_start.top - spacing;
			card.right = w * (1 - pad_percent);
			card.bottom = btn_add_start.bottom + spacing;
			//TODO: improve this so it properly matches how it looks like when there are items inside
		}
		urender::draw_round_rectangle(dc, card, card_extent, colors.Card_Bk_Soft);
		return 1;
	} break;
	default: return DefWindowProc(hwnd, msg, wparam, lparam); break;
	}
	return 0;
}

_init_wndclass_at_startup
}
_add_struct_to_serialization_namespace(editor::Settings)
