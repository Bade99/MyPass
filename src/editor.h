#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "global.h"
#include "edit_subclass.h"
#include "language_manager.h"
#include "search.h"
#include "raw_buffer.h"
#include "render_diagnostics.h"
#include "child_layout.h"

namespace editor {

constexpr auto card_animation_timer_id = 3101;

void stop_card_animation(State& state) {
	KillTimer(state.wnd, card_animation_timer_id);
	state.card_animation.timeline.stop();
	state.card_animation.moves.clear();
	state.card_animation.requested = false;
	state.card_animation.entering_card = nil;
}

auto get_state(HWND wnd) { return get_window_state<State>(wnd); }

void set_passwords_need_save(State& state, bool new_val) {
	state.passwords_need_save = new_val;
	str txt;
	if (!state.current_user.empty()) txt = state.current_user;
	if (state.passwords_need_save) txt += L" ●";
	SetText_txt_app(state.nc_parent, txt.c_str(), app_name_with_version);
}

void on_password_editors_cnt_changed(State& state) {
	ShowWindow(state.controls.btn_add_end, state.controls.password_editors.size() ? SW_SHOW : SW_HIDE);
}

static const auto on_password_editor_change = [](void* data, HWND wnd) {
	auto& state = *(State*)data;
	set_passwords_need_save(state, true);

	//Update the modification date of the password editor
	//TODO(fran): allow for more information storage in stateful functions so you can pass in the password_editor associated with this change, or create a on_change function in the password editor, so you can pass in its state instead, then it would know how to call the general editor to tell it that there was an update
	auto pw_ed = get_parent_of_class(wnd, password_editor::wndclass); Assert(pw_ed);
	auto& pw_ed_state = *password_editor::get_state(pw_ed);
	auto updated_props = pw_ed_state.properties;
	updated_props.date_modified = std::time(nil);
	password_editor::set_properties(pw_ed, updated_props);
};

struct Props { const utf16* title = nil; time_t date_created = 0, date_modified = 0; multiflag<password_editor::ItemFlag> flags = 0; };
password_editor::State& add_password_editor(State& state, const Props& properties, size_t at_idx) {
	auto wnd = create_window(state.controls.page, password_editor::wndclass, nil, WS_VISIBLE | WS_CHILD, WS_EX_CONTROLPARENT, 0, state.wnd);

	//TODO(fran): I have to manually move the btn_add_end to the end of the Z order so traversal through pressing tab remains correct, a better solution would actually be to put the password_editors inside yet another empty page container, so that they are placed inside of it, removing the need to update the z-order of btn_add_end which would be on a different layer of the hierarchy
	SetWindowPos(state.controls.btn_add_end, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);

	password_editor::set_user_data(wnd, &state);
	password_editor::set_properties(wnd, {.date_created = properties.date_created, .date_modified = properties.date_modified, .flags = properties.flags});
	auto& res = *password_editor::get_state(wnd);
	password_editor::set_stateful_functions(wnd, {
		.on_change = { .state = &state, .function = on_password_editor_change  },
		.on_layout_change = { .state = &state, .function = [](void* data) {
			auto& state = *(State*)data;
			state.card_animation.requested = true;
		} }
	});
	password_editor::set_functions(wnd, {
		.on_delete = [](HWND pwd_ed, void* data) {
			auto& state = *(State*)data;
			stop_card_animation(state);
			auto& vec = state.controls.password_editors;
			for (const auto& [i, c] : vec | std::views::enumerate)
				if (c == pwd_ed) {
					on_password_editor_change(&state, password_editor::get_state(c)->controls.btn_del); //TODO(fran): small bug, we should actually check if the data in this editor was saved, if not then it shouldnt set need_save to true
					DestroyWindow(c);
					vec.erase(vec.begin() + i);
					on_password_editors_cnt_changed(state);
					ask_window_for_resize(state.wnd);
					ask_window_for_repaint(state.wnd);
					break;
				}
		}
	});
	AWDT(res.controls.edo_title, LANG_PWD_ED_TITLE);
	edit_oneline::set_user_extra(res.controls.edo_title, &state);
	edit_oneline::set_functions(res.controls.edo_title, { .on_change = on_password_editor_change });

	auto& vec = state.controls.password_editors;
	auto at = at_idx < vec.size() ? vec.begin() + at_idx : vec.end();
	vec.insert(at, wnd);
	on_password_editors_cnt_changed(state);
	if (properties.title) set_window_text(res.controls.edo_title, properties.title, false);
	ask_window_for_resize(state.wnd);
	ask_window_for_repaint(state.wnd);
	return res;
}

void add_preset_password_editor(State& state, size_t at_idx) {
	Props properties{ .title = L"", .date_created = std::time(nil), .date_modified = properties.date_created };
	auto& password_editor = add_password_editor(state, properties, at_idx);
	auto usr_txt = RS(LANG_PWD_ED_TBL_USER);
	auto pwd_txt = RS(LANG_PWD_ED_TBL_PASSWORD);
	auto usr = password_editor::empty_description_cell; usr.text = usr_txt.c_str();
	auto pwd = password_editor::empty_description_cell; pwd.text = pwd_txt.c_str();
	auto val = password_editor::empty_value_cell;
	password_editor::table_add_row(password_editor, &usr, &val);
	password_editor::table_add_row(password_editor, &pwd, &val);
	password_editor::set_is_open(password_editor, true);
	password_editor::set_is_editing(password_editor, true);
	state.card_animation.entering_card = password_editor.wnd;
	state.card_animation.requested = true;
	ask_window_for_resize(state.wnd);
	ask_window_for_repaint(state.wnd);
}

struct CardLayout {
	std::vector<child_layout::MoveRequest> moves;
	i32 page_height = 0;

	void add_move(HWND wnd, rect_i32 bounds) { moves.push_back({ wnd, bounds.to_RECT() }); }
};

CardLayout measure_card_layout(State& state, i32 width, i32 spacing, i32 width_padding) {
	CardLayout layout;
	auto& controls = state.controls;
	layout.moves.reserve(controls.password_editors.size() + 2);

	i32 offset_y = 0;
	i32 button_dimension = DPI(30);
	i32 button_x = (width - button_dimension) / 2;
	bool any_visible = false;
	for (auto card : controls.password_editors)
		if (IsWindowVisible(card)) { any_visible = true; break; }

	if (any_visible) {
		i32 button_start_y = spacing - button_dimension / 2;
		layout.add_move(controls.btn_add_start, { button_x, button_start_y, button_dimension, button_dimension });
		offset_y = button_start_y + button_dimension + spacing;
		//TODO(fran): defer descendant layout for offscreen cards during width changes, then refresh it before a card enters the viewport; preserve scrolling and keyboard focus.
		for (auto card : controls.password_editors) {
			if (!IsWindowVisible(card)) continue;
			const auto& card_state = *password_editor::get_state(card);
			i32 card_height = password_editor::resize_controls(card_state, false);
			layout.add_move(card, { width_padding, offset_y, width - 2 * width_padding, card_height });
			offset_y += card_height + spacing;
		}
		layout.add_move(controls.btn_add_end, { button_x, offset_y, button_dimension, button_dimension });
		offset_y += button_dimension;
	}
	else {
		layout.add_move(controls.btn_add_start, { button_x, spacing, button_dimension, button_dimension });
		offset_y = spacing + button_dimension;
		layout.add_move(controls.btn_add_end, { (i32)DPI(-100), 0, button_dimension, button_dimension }); //HACK: hide end button without having to worry about restoring visibility states
	}
	layout.page_height = offset_y + spacing;
	return layout;
}

void apply_card_layout(State& state, CardLayout& layout, bool redraw_backdrop = false, bool force_page_update = true) {
	RECT page_bounds; GetClientRect(state.controls.page, &page_bounds);
	RECT viewport_bounds; GetClientRect(state.controls.page_space, &viewport_bounds);
	i32 page_height = maximum(RECTH(viewport_bounds), layout.page_height);
	if (page_height > RECTH(page_bounds))
		page::set_wnd_size(state.controls.page_space, layout.page_height);
	{
		render_diagnostics::Scope move_diagnostic{ render_diagnostics::Event::card_move };
		child_layout::move_batch(layout.moves);
	}
	if (page_height < RECTH(page_bounds) || (force_page_update && page_height == RECTH(page_bounds)))
		page::set_wnd_size(state.controls.page_space, layout.page_height);
	if (redraw_backdrop)
		RedrawWindow(state.wnd, nil, nil, RDW_INVALIDATE | RDW_ERASE);
}

void tick_card_animation(State& state) {
	auto& transition = state.card_animation;
	if (!transition.timeline.active) return;
	f32 progress = transition.timeline.progress();
	f32 eased = animation::ease_out_cubic(progress);
	CardLayout layout;
	layout.moves.reserve(transition.moves.size());
	for (const auto& move : transition.moves)
		if (IsWindow(move.wnd))
			layout.moves.push_back({ move.wnd, lerp(move.from, eased, move.to) });
	layout.page_height = lerp(transition.from_page_height, eased, transition.to_page_height);
	apply_card_layout(state, layout, true, false);
	if (progress >= 1.f) stop_card_animation(state);
}

void CALLBACK card_animation_timer_proc(HWND wnd, UINT, UINT_PTR, DWORD) {
	if (auto state = get_state(wnd)) tick_card_animation(*state);
}

void start_card_animation(State& state, CardLayout& target) {
	auto& transition = state.card_animation;
	RECT viewport_bounds; GetClientRect(state.controls.page_space, &viewport_bounds);
	i32 target_page_height = maximum(RECTH(viewport_bounds), target.page_height);
	if (transition.timeline.active && !transition.entering_card && transition.to_page_height == target_page_height &&
		transition.moves.size() == target.moves.size()) {
		bool same_target = true;
		for (size_t index = 0; index < target.moves.size(); ++index)
			if (transition.moves[index].wnd != target.moves[index].wnd ||
				!EqualRect(&transition.moves[index].to, &target.moves[index].bounds)) { same_target = false; break; }
		if (same_target) return;
	}

	RECT page_bounds; GetClientRect(state.controls.page, &page_bounds);
	transition.from_page_height = RECTH(page_bounds);
	transition.to_page_height = target_page_height;
	transition.moves.clear();
	transition.moves.reserve(target.moves.size());
	RECT entering_bounds{};
	for (const auto& move : target.moves)
		if (move.wnd == transition.entering_card) { entering_bounds = move.bounds; break; }
	bool any_change = transition.from_page_height != transition.to_page_height;
	for (const auto& move : target.moves) {
		RECT from = get_window_rect_at(move.wnd, state.controls.page);
		if (move.wnd == transition.entering_card) {
			from = move.bounds;
			from.bottom = from.top + 1;
		}
		else if (transition.entering_card && entering_bounds.right > entering_bounds.left &&
			move.wnd == state.controls.btn_add_end && from.left < 0) {
			from = move.bounds;
			from.top = entering_bounds.top + 1 + DPI(15);
			from.bottom = from.top + RECTH(move.bounds);
		}
		any_change |= !EqualRect(&from, &move.bounds);
		transition.moves.push_back({ move.wnd, from, move.bounds });
	}
	if (!any_change) {
		stop_card_animation(state);
		apply_card_layout(state, target, true);
		return;
	}
	transition.timeline.start(200.f);
	if (!SetTimer(state.wnd, card_animation_timer_id, 16, card_animation_timer_proc)) {
		stop_card_animation(state);
		apply_card_layout(state, target, true);
		return;
	}
	tick_card_animation(state);
}

void resize_controls(State& state, bool animate_cards = false) {
	render_diagnostics::Scope diagnostic{ render_diagnostics::Event::editor_resize };
	RECT r; GetClientRect(state.wnd, &r);
	auto w = RECTW(r), h = RECTH(r);
	i32 spacing = DPI(15); //TODO: add to theme
	i32 w_pad = DPI(20);

	i32 offset_y = 0;
	{
		i32 offset_x = 0;
		if (state.mode == Mode::transition_v0) {
			auto& controls = state.controls.transition_v0;
			rect_i32 edit_showpasswords{ .x = 0, .y = 0, .w = w / 2, .h = h };
			offset_x = edit_showpasswords.right();
			w -= offset_x;

			auto btn_transition_w = DPI(20);
			auto icon_transition = edit_showpasswords.cut_right(spacing / 2 + btn_transition_w).cut_right(btn_transition_w);

			rect_i32 transition_title_section = edit_showpasswords.cut_top(DPI(40));
			rect_i32 static_transition = transition_title_section.cut_center(transition_title_section.w - DPI(8), DPI(30));
			rect_i32 btn_static_bk_transition = static_transition;
			static_transition.xy = {};
			rect_i32 btn_static_transition = static_transition.cut_left(DPI(6));
			v2_i32 btn_confirm_dim{}; Button_GetIdealSize(controls.btn_confirm_transfer, &btn_confirm_dim);
			rect_i32 btn_confirm_transfer = static_transition.cut_right(clamp(0, btn_confirm_dim.x, static_transition.w / 2));

			child_layout::move_if_needed(controls.btn_static_bk_transition, btn_static_bk_transition);
			child_layout::move_if_needed(controls.btn_static_transition, btn_static_transition);
			child_layout::move_if_needed(controls.static_transition, static_transition);
			child_layout::move_if_needed(controls.btn_confirm_transfer, btn_confirm_transfer);
			child_layout::move_if_needed(controls.edit_passwords, edit_showpasswords);
			child_layout::move_if_needed(controls.icon_transition, icon_transition);
		}

		auto& controls = state.controls;
		offset_y = spacing;

		auto search_max_w = avg_str_dim(GetWindowFont(controls.search), 35).cx;
		bool sort_on_next_line = w <= search_max_w * 1.5f;
		rect_i32 search{
			.x = offset_x + w_pad,
			.y = offset_y,
			.w = minimum((sort_on_next_line ? w - w_pad : w / 2) - w_pad, search_max_w),
			.h = (i32)DPI(30)
		};
		child_layout::move_if_needed(controls.search, search);

		i32 sort_w = search.w * .9f;
		rect_i32 sort{
			.x = sort_on_next_line ? search.x : offset_x + w - w_pad - sort_w,
			.y = sort_on_next_line ? search.bottom() + spacing : search.y,
			.w = sort_w,
			.h = search.h
		};
		child_layout::move_if_needed(controls.combo_sort, sort);

		offset_y = sort.bottom() + spacing;

		rect_i32 page_space{ .x = offset_x, .y = offset_y, .w = w, .h = h - offset_y };
		child_layout::move_if_needed(controls.page_space, page_space, false);
	}

	auto target = measure_card_layout(state, w, spacing, w_pad);
	if (animate_cards) start_card_animation(state, target);
	else {
		stop_card_animation(state);
		apply_card_layout(state, target);
	}
	state.card_animation.requested = false;
	state.card_animation.entering_card = nil;
	RECT client_bounds; GetClientRect(state.wnd, &client_bounds);
	state.card_animation.client_width = RECTW(client_bounds);
	state.card_animation.client_height = RECTH(client_bounds);
}

void add_controls_transition_v0(State& state); //forward declaration to solve circular dependency with set_mode

void set_mode(State& state, Mode new_mode) {
	if (new_mode != state.mode) {
		auto old_mode = state.mode;
		state.mode = new_mode;

		switch (state.mode) {
		case Mode::transition_v0:
		{
			//v0 data format detected, enabling UI to facilitate user transition to the new data format from their old data
			add_controls_transition_v0(state);
		} break;
		case Mode::normal:
		{
			if (old_mode == Mode::transition_v0) {
				for (auto& c : state.controls.transition_v0.all) { DestroyWindow(c); c = nil; }
			}

			ask_window_for_resize(state.wnd);
			ask_window_for_repaint(state.wnd);
		} break;
		default: Assert(0);
		}
	}
}

void add_controls(State& state) {
	auto& controls = state.controls;

	search::Init global_search_init {
		.SearchPlacement_flags = search::Placement::top,
		.SearchFlag_flags = 0,
		.parent_type = search::ParentType::custom,
	};
	controls.search = CreateWindowEx(WS_EX_COMPOSITED | WS_EX_TRANSPARENT | WS_EX_CONTROLPARENT, search::wndclass, NULL, WS_VISIBLE | WS_CHILD,
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
					utf16 title[64];
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

	//SendMessage(controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)controls.search, 0);
	
	controls.combo_sort = create_window(state.wnd, combobox::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP);
	auto combo_sort_controls = combobox::get_controls(state.controls.combo_sort);
	AWDYN(combo_sort_controls.button, WM_PAINT);
	AWDYN(combo_sort_controls.listbox, WM_SIZE); // triggers a full backbuffer redraw of all its items
	auto setup_combobox_sorting_options = [](State& state, HWND combo) {
		auto controls = combobox::get_controls(combo);
		Assert(controls.listbox);
		SortComboItem items[] = {
			{.value = SortOption::date_modified_newest, .label_id = LANG_EDITOR_SORT_DATE_MODIFIED_NEWEST },
			{.value = SortOption::date_modified_oldest, .label_id = LANG_EDITOR_SORT_DATE_MODIFIED_OLDEST },
			{.value = SortOption::alphabetic_az, .label_id = LANG_EDITOR_SORT_ALPHABETIC_AZ },
			{.value = SortOption::alphabetic_za, .label_id = LANG_EDITOR_SORT_ALPHABETIC_ZA },
			{.value = SortOption::date_created_newest, .label_id = LANG_EDITOR_SORT_DATE_CREATED_NEWEST },
			{.value = SortOption::date_created_oldest, .label_id = LANG_EDITOR_SORT_DATE_CREATED_OLDEST },
		};
		listbox::add_elements_by_value(controls.listbox, (void**)items, ARRAYSIZE(items));

		//combobox::set_cur_sel(combo, (size_t)state.sorting); // we wont apply sorting on startup
		//ask_window_for_repaint(combo);
	};
	setup_combobox_sorting_options(state, controls.combo_sort);
	combobox::set_user_extra(controls.combo_sort, &state);
	//combobox::set_function_desired_size_combobox(controls.combo_sort, langbox_func_desired_size);
	combobox::set_function_on_selection_accepted(controls.combo_sort, [](void* element, void* user_extra) {
		auto& state = *(State*)user_extra;
		auto item = std::bit_cast<SortComboItem>(element);
		static_assert((i32)SortOption::__last == 6, "Enum element count changed, update sorting logic to consider the new values");

		combobox::set_cur_sel(state.controls.combo_sort, (size_t)item.value);

		struct SortAlphabetic { HWND wnd; str title; };
		struct SortDate { HWND wnd; time_t date; };

		auto& vec = state.controls.password_editors;
		switch (item.value) {
		case SortOption::alphabetic_az:
		case SortOption::alphabetic_za:
		{
			std::vector<SortAlphabetic> to_sort_pinned;
			std::vector<SortAlphabetic> to_sort; to_sort.reserve(vec.size());
			for (auto p : vec) {
				auto& controls = password_editor::get_state(p)->controls;
				bool pin = button::get_state(controls.btn_pin)->selected;
				auto title = controls.edo_title;
				auto len = GetWindowTextLength(title) + 1;
				SortAlphabetic element;
				element.wnd = p;
				element.title.resize_and_overwrite(len,
					[=](utf16* buf, size_t) { return GetWindowText(title, buf, len); });
				if (pin) to_sort_pinned.push_back(std::move(element));
				else to_sort.push_back(std::move(element));
			}
			auto sorting_function = (item.value == SortOption::alphabetic_az) ?
				[](const SortAlphabetic& a, const SortAlphabetic& b) {
				return _wcsicmp(a.title.c_str(), b.title.c_str()) < 0;
				} :
				[](const SortAlphabetic& a, const SortAlphabetic& b) {
					return _wcsicmp(a.title.c_str(), b.title.c_str()) > 0;
				};
			std::sort(to_sort_pinned.begin(), to_sort_pinned.end(), sorting_function);
			std::sort(to_sort.begin(), to_sort.end(), sorting_function);
			for (const auto& [i, value] : to_sort_pinned | std::views::enumerate) vec[i] = value.wnd;
			for (const auto& [i, value] : to_sort | std::views::enumerate) vec[i + to_sort_pinned.size()] = value.wnd;
		} break;
		case SortOption::date_modified_newest:
		case SortOption::date_modified_oldest:
		{
			std::vector<SortDate> to_sort_pinned;
			std::vector<SortDate> to_sort; to_sort.reserve(vec.size());
			for (auto p : vec) {
				auto& state = *password_editor::get_state(p);
				bool pin = button::get_state(state.controls.btn_pin)->selected;
				if (pin) to_sort_pinned.emplace_back(p, state.properties.date_modified);
				else to_sort.emplace_back(p, state.properties.date_modified);
			}
			auto sorting_function = (item.value == SortOption::date_modified_newest) ?
				[](SortDate a, SortDate b) { return a.date > b.date; } :
				[](SortDate a, SortDate b) { return a.date < b.date; };
			std::sort(to_sort_pinned.begin(), to_sort_pinned.end(), sorting_function);
			std::sort(to_sort.begin(), to_sort.end(), sorting_function);
			for (const auto& [i, value] : to_sort_pinned | std::views::enumerate) vec[i] = value.wnd;
			for (const auto& [i, value] : to_sort | std::views::enumerate) vec[i + to_sort_pinned.size()] = value.wnd;
		} break;
		case SortOption::date_created_newest:
		case SortOption::date_created_oldest:
		{
			std::vector<SortDate> to_sort_pinned;
			std::vector<SortDate> to_sort; to_sort.reserve(vec.size());
			for (auto p : vec) {
				auto& state = *password_editor::get_state(p);
				bool pin = button::get_state(state.controls.btn_pin)->selected;
				if (pin) to_sort_pinned.emplace_back(p, state.properties.date_created);
				else to_sort.emplace_back(p, state.properties.date_created);
			}
			auto sorting_function = (item.value == SortOption::date_created_newest) ?
				[](SortDate a, SortDate b) { return a.date > b.date; } :
				[](SortDate a, SortDate b) { return a.date < b.date; };
			std::sort(to_sort_pinned.begin(), to_sort_pinned.end(), sorting_function);
			std::sort(to_sort.begin(), to_sort.end(), sorting_function);
			for (const auto& [i, value] : to_sort_pinned | std::views::enumerate) vec[i] = value.wnd;
			for (const auto& [i, value] : to_sort | std::views::enumerate) vec[i + to_sort_pinned.size()] = value.wnd;
		} break;
		default: Assert(0);
		}
		state.card_animation.requested = true;
		ask_window_for_resize(state.wnd);
	});
	combobox::set_function_render_combobox(controls.combo_sort, [](HDC dc, rect_i32 r, combobox::RenderFlags flags, void* element, void* user_extra) {
		auto& theme = themes.base_btn;
		HFONT font = fonts.General;
		HBRUSH bk_br, txt_br, border_br = theme.brushes.foreground.normal;
		if (flags.is_listbox_open) bk_br = theme.brushes.bk.normal;
		else if (flags.on_clicked) bk_br = theme.brushes.bk.clicked;
		else if (flags.on_mouseover) bk_br = theme.brushes.bk.mouseover;
		else bk_br = theme.brushes.bk.disabled;

		int x_pad = avg_str_dim(font, 1).cx;

		urender::draw_background(dc, r.to_RECT(), bk_br, border_br, theme.dimensions);
		
		auto bmp = flags.is_listbox_open ? bmps.dropdown_up : bmps.dropdown;
		int icon_x = urender::draw_bitmap_1bpp_right(bmp, dc, r, x_pad, theme.brushes.foreground.normal);

		str txt;
		if (element) {
			txt = RS(std::bit_cast<SortComboItem>(element).label_id);
			txt_br = colors.ControlTxt;
		} else {
			txt = RS(LANG_EDITOR_SORT);
			txt_br = colors.ControlTxt_Disabled;
		}

		RECT txt_rc = r.to_RECT();
		txt_rc.left += x_pad;
		txt_rc.right = icon_x - x_pad;

		urender::draw_text(dc, txt_rc, to_utf_str(txt), font, txt_br, urender::TextAlignment::left, 0);
	});
	combobox::set_function_render_listbox_element(controls.combo_sort, [](HDC dc, rect_i32 r, listbox::RenderFlags flags, void* element, void* user_extra) {
		int w = r.w, h = r.h;
		auto item = std::bit_cast<SortComboItem>(element);
		auto txt = RS(item.label_id);

		//Draw bk
		HBRUSH bk_br = colors.ControlBk, txt_br = colors.ControlTxt;
		if (flags.on_mouseover || flags.on_selected) bk_br = colors.ControlBkMouseOver;
		if (flags.on_clicked) bk_br = colors.ControlBkPush;

		RECT bk_rc = r.to_RECT();
		FillRect(dc, &bk_rc, bk_br);

		//Draw text
		HFONT font = fonts.General;
		RECT txt_rc = bk_rc;

		urender::draw_text(dc, txt_rc, to_utf_str(txt), font, txt_br, urender::TextAlignment::left, avg_str_dim(font, 1).cx);
	});

	auto scroll_area = page::create_scrollable_area(state.wnd);

	controls.page_space = scroll_area.scrollable_area;

	controls.page = scroll_area.children.content_area;

	auto btn_add_theme = themes.editor_add_btn;

	controls.btn_add_start = create_window(controls.page, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP | WS_TABSTOP);
	button::set_theme(controls.btn_add_start, btn_add_theme);
	SendMessage(controls.btn_add_start, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.add);
	button::set_user_data(controls.btn_add_start, &state);
	button::set_functions(controls.btn_add_start, {
		.on_click = [](void* data, HWND wnd) {
			auto& state = *(State*)data;
			add_preset_password_editor(state, 0);
		}
		});
	add_mouseover_tooltip(controls.btn_add_start, LANG_EDITOR_ADD);

	controls.btn_add_end = create_window(controls.page, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP | WS_TABSTOP);
	button::set_theme(controls.btn_add_end, btn_add_theme);
	SendMessage(controls.btn_add_end, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.add);
	button::set_user_data(controls.btn_add_end, &state);
	button::set_functions(controls.btn_add_end, {
		.on_click = [](void* data, HWND wnd) {
			auto& state = *(State*)data;
			add_preset_password_editor(state, -1);
		}
	});
	add_mouseover_tooltip(controls.btn_add_end, LANG_EDITOR_ADD);

	for (auto ctl : controls.all_fixed) SetWindowFont(ctl, (WPARAM)fonts.General, true);

	resize_controls(state);
}

#define showpasswords_menu_base_addr	200
#define SHOWPASSWORDS_MENU_SAVE			(showpasswords_menu_base_addr+1)
#define SHOWPASSWORDS_MENU_UNDO			(showpasswords_menu_base_addr+3)
#define SHOWPASSWORDS_MENU_REDO			(showpasswords_menu_base_addr+4)
#define SHOWPASSWORDS_MENU_FIND			(showpasswords_menu_base_addr+5)
#define SHOWPASSWORDS_MENU_SAVE_V0		(showpasswords_menu_base_addr+6) // Debug only
#define SHOWPASSWORDS_MENU_CHECK_FOR_UPDATES (showpasswords_menu_base_addr+7)

void add_menus(State& state) { //TODO(fran): this should be a toolbar (maybe), toolbars are kinda stupid, just useful till you learn shortcuts https://docs.microsoft.com/en-us/windows/win32/controls/create-toolbars
	//NOTE: each menu gets its parent HMENU stored in the itemData part of the struct

	HMENU menu = CreateMenu(); state.menu = menu;
	HMENU menu_file = CreateMenu(); state.menu_file = menu_file;
	HMENU menu_file_lang = CreateMenu(); state.menu_file_lang = menu_file_lang;
	HMENU menu_edit = CreateMenu(); state.menu_edit = menu_edit;
	append_popup_to_menu(menu, menu_file, LANG_MENU_FILE);

	append_item_to_menu(menu_file, SHOWPASSWORDS_MENU_SAVE, LANG_MENU_SAVE, bmps.menu_save, bmps.menu_save);

	#ifdef _DEBUG
	append_item_to_menu(menu_file, SHOWPASSWORDS_MENU_SAVE_V0, _t("Save V0"), bmps.menu_save, bmps.menu_save);
	#endif

	append_separator_to_menu(menu_file);

	append_popup_to_menu(menu_file, menu_file_lang, LANG_MENU_LANGUAGE, bmps.language, bmps.language);
	append_language_items_to_menu(menu_file_lang);

	append_item_to_menu(menu_file, SHOWPASSWORDS_MENU_CHECK_FOR_UPDATES, LANG_MENU_CHECK_FOR_UPDATES, bmps.menu_update, bmps.menu_update);

	append_popup_to_menu(menu, menu_edit, LANG_MENU_EDIT);

	append_item_to_menu(menu_edit, SHOWPASSWORDS_MENU_UNDO, LANG_MENU_EDIT_UNDO, bmps.menu_undo, bmps.menu_undo);

	append_item_to_menu(menu_edit, SHOWPASSWORDS_MENU_REDO, LANG_MENU_EDIT_REDO, bmps.menu_redo, bmps.menu_redo);

	append_item_to_menu(menu_edit, SHOWPASSWORDS_MENU_FIND, LANG_MENU_EDIT_FIND, bmps.menu_search, bmps.menu_search);

	nonclient::set_menu(state.nc_parent, state.menu);
}

void save_settings(State& state) {
	RECT r; GetWindowRect(state.wnd, &r);
	state.settings->rc = r;
}

constexpr auto& primary_filename = _t("\\tt");
constexpr auto& temp_filename = _t("\\temp");
constexpr auto& last_filename = _t("\\last");

// Returns an empty path if the work folder is unavailable.
str get_save_path(str username) { 
	constexpr auto& user_prefix = _t("\\user_");
	str general_folder = get_general_save_folder();
	if (general_folder.empty()) return {};
	str res = general_folder + user_prefix + username;
	//INFO: appending user_ saves us from the trouble of the reserved filenames that windows has, eg COM1, LPT1, ...
	return res;
}

///username: serves as the user folder name
bool save_to_file_user(str username, void* content, u32 content_sz) {

	str path = get_save_path(username);
	if (path.empty()) return false;

	//Create the folder where info will be stored, since windows wont do it
	if (!CreateDirectory(path.c_str(), nil)) 
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			return false;

	if (DWORD attrs = GetFileAttributes(path.c_str()); attrs != INVALID_FILE_ATTRIBUTES && attrs & FILE_ATTRIBUTE_DIRECTORY)
		SetFileAttributes(path.c_str(), attrs | FILE_ATTRIBUTE_HIDDEN); //surface level protection, we do not fail if we cant do this since it's not essential

	str full_path_temp = path + temp_filename;

	if (!write_entire_file(full_path_temp.c_str(), content, content_sz, true))
		return false;

	//For security critical data such as what we save in this app we re-validate the entirety of the saved content
	//This is mostly a just a defensive guard since we are likely just reading from the cache and not from disk
	if (!file_matches_memory(full_path_temp.c_str(), content, content_sz))
		return false;

	str full_path_last = path + last_filename;
	str full_path_primary = path + primary_filename;

	bool replace_ok = false;

	if (file_exists(full_path_primary.c_str())) {
		replace_ok = ReplaceFile(full_path_primary.c_str(), full_path_temp.c_str(), full_path_last.c_str(),
			REPLACEFILE_IGNORE_MERGE_ERRORS, 0, 0);

		if (replace_ok) replace_ok = flush_existing_file(full_path_primary.c_str());
	}
	else {
		replace_ok = MoveFileEx(full_path_temp.c_str(), full_path_primary.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	}

	if (!replace_ok) return false;

	//re-validate again now on the moved file
	return file_matches_memory(full_path_primary.c_str(), content, content_sz);
}

ReadEntireFileResult load_file_user(str username, bool load_backup = false) {
	str path = get_save_path(username); //username functions as the folder name
	if (path.empty()) return { .status = ReadEntireFileStatus::error };

	CreateDirectory(path.c_str(), 0);//Create the folder where info will be stored, since windows wont do it

	SetFileAttributes(path.c_str(), GetFileAttributes(path.c_str()) | FILE_ATTRIBUTE_HIDDEN); //some very basic protection

	path += load_backup ? last_filename : primary_filename;

	auto res = read_entire_file(path.c_str());
	return res;
}

void get_controls_data_for_saving(State& state, raw_buffer& out) {
	//data structure: editors count (u64) | each editor
	// editor: title (str, all str are null terminated utf16) | date created (u64) | date modified (u64) | multiflag (u32) | table row count (u64) | each table row
	//  table row: desc (str) | val (str) | multiflag (u32)
	auto append_control_text_to_buffer = [](raw_buffer& out, HWND wnd) {
		auto old_len_bytes = out.size();
		auto added_len = GetWindowTextLength(wnd) + 1;
		auto added_len_bytes = added_len * sizeof(cstr);
		
		//TODO(fran): boolean to strip the text, though it will need another copy if stripping is needed unfortunately. Currently we instead do stripping when reading the data instead of when writing it

		out.resize_and_overwrite(old_len_bytes + added_len_bytes,
			[&](char* buf, size_t new_size) {
				GetWindowText(wnd, (cstr*)(buf + old_len_bytes), added_len);
				return new_size;
			}
		);
	};

	//TODO(fran): we have arch dependent sizes for types, size_t time_t, normalize to 64bit and 32bit depending on needs

	append_to_buffer(out, state.controls.password_editors.size());
	for (auto& e : state.controls.password_editors) {
		//TODO(fran): we may want to move the data extraction logic inside password_editor. Though that would mean that either it would also need to be aware of the data format we expect; or we would need to create an intermediate data format that we would then parse into the final output
		auto& ed = *password_editor::get_state(e);
		auto title = ed.controls.edo_title;
		
		append_control_text_to_buffer(out, title);
		
		multiflag<password_editor::ItemFlag> pwed_flags = 0;
		//TODO(fran): should we maintain ed.properties.flags up to date and use it directly instead?
		bool pin = button::get_state(ed.controls.btn_pin)->selected;
		set_flag_bit(pwed_flags, pin, password_editor::ItemFlag::pin);

		append_to_buffer(out, ed.properties.date_created);
		append_to_buffer(out, ed.properties.date_modified);
		append_to_buffer(out, pwed_flags);

		auto& tbl = *table::get_state(ed.controls.tbl_values);

		append_to_buffer(out, tbl.rows.size());
		for (auto& r : tbl.rows) {
			auto& description_cell = r[0];
			append_control_text_to_buffer(out, description_cell);
			
			auto& value_cell = *password_editor::value_cell::get_state(r[1]);
			append_control_text_to_buffer(out, value_cell.controls.text);

			multiflag<password_editor::ValueCellFlag> flags = 0;
			bool lock = button::get_state(value_cell.controls.btn_lock)->selected;
			set_flag_bit(flags, lock, password_editor::ValueCellFlag::lock);

			append_to_buffer(out, flags);
		}
	}
}

void save_passwords_v0(State& state) {
	auto& controls = state.controls.transition_v0;
	int user_len_chars = (int)state.current_user.size();
	Assert(controls.edit_passwords);
	int len_chars = user_len_chars + GetWindowTextLength(controls.edit_passwords) + 1;
	// Pad with extra garbage bytes to get blocks of 16 bytes for encryption
	int len_bytes = next_multiple_of_16(len_chars * sizeof(cstr)); 
	void* mem = malloc(len_bytes); defer{ free(mem); };
	Assert(sizeof(cstr) > 1);
	wcscpy_s((cstr*)mem, user_len_chars + 1, state.current_user.c_str()); //append username so we can check against it in later logins
	GetWindowText(controls.edit_passwords, ((cstr*)mem) + user_len_chars, len_chars - user_len_chars);
	twofish_encrypt(mem, len_bytes, mem);

	bool res = save_to_file_user(state.current_user, mem, len_bytes);
	set_passwords_need_save(state, !res);
	if (!res) CustomMessageBox(state.wnd, RCS(LANG_ERROR_SAVEFILE_PASSWORDS), RCS(LANG_ERROR), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND, msgbox_placement);
}

void generate_salt(utf16 (&salt)[8]) {
	static_assert(sizeof(salt) == 16);
	std::srand(std::time(nil));
	for (i32 i = 0; i < 8; i++) salt[i] = std::rand();
}

raw_buffer generate_save_data_v1(State& state) {
	//V1
	//header: [magic(4bytes "myps" ascii no terminator) | version(4bytes u32) | salt(16bytes, random, new one generated on every login)]
	//data: [structured block data | padding | integrity hash(header + unencrypted data)]

	FileHeaderV1 header;
	static_assert(sizeof(header.salt) == sizeof(state.salt));
	memcpy(header.salt, state.salt, sizeof(header.salt));

	FileFooterV1 footer;
	static_assert(sizeof(FileFooterV1) % 16 == 0, "Footer size must be a multiple of 16 bytes for encryption");

	raw_buffer data;
	append_to_buffer(data, header);
	get_controls_data_for_saving(state, data);

	// Pad with extra garbage bytes to get blocks of 16 bytes for encryption
	auto size_for_encryption = sizeof(header) + next_multiple_of_16(data.size() - sizeof(header));
	data.resize(size_for_encryption);

	sha256(data.data(), data.size(), footer.auth_integrity_hash);
	append_to_buffer(data, footer);

	//if constexpr (debug_text_view) SetWindowText(state.controls.transition_v0.edit_passwords, data.c_str() + wcslen(state.current_user));

	auto data_ptr = data.data() + sizeof(header);
	twofish_encrypt(data_ptr, data.size() - sizeof(header), data_ptr);

	return data;
}

bool save_passwords(State& state, bool complete_version_transition = false) {
	bool res;
	if (state.mode == Mode::transition_v0 && !complete_version_transition) {
		//Prevent manual user saving while on a version transition until they confirm that the transition has been completed
		res = false;
		CustomMessageBox(state.wnd, RCS(LANG_ERROR_SAVE_ON_TRANSITION_TEXT), RCS(LANG_ERROR_SAVE_ON_TRANSITION_TITLE), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND, msgbox_placement);
		return res;
	}

	auto data = generate_save_data_v1(state);
	
	res = data.size() <= U32MAX && save_to_file_user(state.current_user, data.data(), (u32)data.size());
	set_passwords_need_save(state, !res);
	if (!res) CustomMessageBox(state.wnd, RCS(LANG_ERROR_SAVEFILE_PASSWORDS), RCS(LANG_ERROR), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND, msgbox_placement);
	return res;
}

template<typename T> //I give up trying to write down the entire type of a string view subrange coming from std::views::split
void terminate_string_view(const T& str) {
	*const_cast<utf16*>(str.data() + str.size()) = 0;
}

void cleanup_password_editors_v1(State& state) {
	// If while loading password editors we find a corrupted section then we destroy any controls that were created in the process
	auto& vec = state.controls.password_editors;
	for (auto& ed : vec) DestroyWindow(ed);
	vec.clear();
}

bool create_password_editors_v1(State& state, RawBufferReader& data) {
	struct PasswordEditorV1 {
		str_view title;
		time64 date_created;
		time64 date_modified;
		multiflag<password_editor::ItemFlag> flags;
	};

	struct PasswordEditorRowV1 {
		str_view description, value;
		multiflag<password_editor::ValueCellFlag> flags = 0;
	};

	u64 editor_cnt;
	if (!data.read(editor_cnt)) return false; //TODO(fran): for complex data reading it may be a good idea to use exceptions to make the code simpler and avoid if checks all over
	for (u64 e = 0; e < editor_cnt; e++) {
		PasswordEditorV1 editor;
		if (!(data.read(editor.title) && data.read(editor.date_created) && data.read(editor.date_modified) && data.read(editor.flags)))
			return false;
		Props properties{
			.title = editor.title.data(),
			.date_created = editor.date_created, 
			.date_modified = editor.date_modified, 
			.flags = editor.flags 
		};

		auto password_editor = add_password_editor(state, properties, -1);

		u64 row_cnt;
		if (!data.read(row_cnt)) return false;
		for (u64 r = 0; r < row_cnt; r++) {
			PasswordEditorRowV1 row;
			if (!(data.read(row.description) && data.read(row.value) && data.read(row.flags))) return false;

			auto description_cell = password_editor::empty_description_cell;
			auto value_cell = password_editor::empty_value_cell;
			description_cell.text = strip(const_cast<utf16*>(row.description.data())); //TODO(fran): I think we should strip on data save not data load
			value_cell.text = row.value.data(); // We dont strip the value/secret cell
			value_cell.flags = row.flags;
			password_editor::table_add_row(password_editor, &description_cell, &value_cell);
		}
	}
	resize_controls(state);
	return true;
}

void add_controls_transition_v0(State& state) {
	auto& controls = state.controls.transition_v0;
	if (!controls.edit_passwords) {
		controls.btn_static_bk_transition = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD);
		button::set_theme(controls.btn_static_bk_transition, themes.editor_btn_static_bk_transition);

		controls.btn_static_transition = create_window(controls.btn_static_bk_transition, button::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_DISABLED);
		button::set_theme(controls.btn_static_transition, themes.editor_btn_static_transition);

		controls.static_transition = create_window(controls.btn_static_bk_transition, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_DISABLED);
		edit_oneline::set_theme(controls.static_transition, themes.editor_static_transition);
		AWT(controls.static_transition, LANG_TRANSITION_V0);

		controls.btn_confirm_transfer = create_window(controls.btn_static_bk_transition, button::wndclass, nil, WS_VISIBLE | WS_CHILD);
		button::set_theme(controls.btn_confirm_transfer, themes.login_btn);
		button::set_user_data(controls.btn_confirm_transfer, &state);
		button::set_functions(controls.btn_confirm_transfer, {
			.on_click = [](void* data, HWND wnd) {
				auto& state = *(State*)data;
				if (CustomMessageBox(state.wnd,
					RCS(LANG_MSG_CONFIRM_COMPLETE_TRANSITION_V0_TEXT), RCS(LANG_MSG_CONFIRM_COMPLETE_TRANSITION_V0_TITLE),
					MB_YESNOCANCEL | MB_ICONWARNING | MB_SETFOREGROUND, msgbox_placement) == IDYES)
					if (save_passwords(state, true))
						set_mode(state, Mode::normal);
			}
			});
		AWT(controls.btn_confirm_transfer, LANG_TRANSITION_BTN_CONFIRM);

		controls.edit_passwords = create_window(state.wnd, L"Edit", nil, WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_CLIPCHILDREN | ES_NOHIDESEL /*to show selection even when you dont have the focus*/);

		constexpr auto EDIT_PASSWORDS_MAX_TEXT_LENGTH = 32767 * 2; //32767 is the default
		SendMessage(controls.edit_passwords, EM_SETLIMITTEXT, (WPARAM)EDIT_PASSWORDS_MAX_TEXT_LENGTH, NULL);

		SetWindowSubclass(controls.edit_passwords, EditProc, 0, (DWORD_PTR)calloc(1, sizeof(EditProcState)));

		HWND vscroll = CreateWindow(scrollbar::wndclass, nil, WS_CHILD,
			0, 0, 0, 0, controls.edit_passwords, nil, nil, nil);
		SendMessage(vscroll, scrollbar::custom_message::SET_PLACEMENT, (WPARAM)scrollbar::Placement::right, 0);
		SendMessage(vscroll, scrollbar::custom_message::SET_AUTOHIDE, true, 0);
		scrollbar::set_theme(vscroll, themes.base_scrollbar);

		SendMessage(controls.edit_passwords, EM_SETVSCROLL, (WPARAM)vscroll, 0);
		SetWindowFont(controls.edit_passwords, (WPARAM)fonts.General, true);

		//INFO: I dont yet paint edit controls so you gotta use WM_CTLCOLOREDIT

		/*search::Init searchinit;
		searchinit.parent_type = search::ParentType::edit;
		searchinit.SearchFlag_flags = 0;
		searchinit.SearchPlacement_flags = search::Placement::bottom;
		HWND SearchControl = CreateWindowEx(NULL, search::wndclass, NULL, WS_CHILD,
			0, 0, 0, 0, controls.edit_passwords, NULL, NULL, &searchinit);
		SendMessage(controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)SearchControl, 0);
		SendMessage(SearchControl, WM_SETFONT, (WPARAM)fonts.General, TRUE);*/

		controls.icon_transition = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
		button::set_theme(controls.icon_transition, themes.transition_btn);
		SendMessage(controls.icon_transition, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.pointed_line_arrow_right);
		add_mouseover_tooltip(controls.icon_transition, LANG_TRANSITION_V0);

	}
	resize_controls(state);
}

u32 detect_save_version(void* data, u32 sz_bytes) {
	RawBufferReader r(data, sz_bytes);
	FileHeaderBase header;
	return r.read(header) && !memcmp(header.magic, FileHeaderBase{}.magic, sizeof(FileHeaderBase::magic)) ? header.version : 0;
}

login::AttemptResult startup(State& state, const Start* start, bool start_from_backup) {
	login::AttemptResult start_attempt;

	auto file_read = load_file_user(state.current_user, start_from_backup); defer{ free_file_memory(file_read.mem); };
	if (file_read.status == ReadEntireFileStatus::success && file_read.mem) {
		if (start->signup) { // Can't signup, user already exists
			start_attempt = login::AttemptResult::fail_signup_username_exists;
		}
		else {
			auto version = detect_save_version(file_read.mem, file_read.sz);

			switch (version) {
			case 0:
			{
				if (file_read.sz % 16 == 0) {
					u32 key[8];
					sha256(start->password.str, start->password.sz_chars * sizeof(*start->password.str), key);
					twofish_setkey(key, sizeof(key));
					SecureZeroMemory(key, sizeof(key));
					twofish_decrypt(file_read.mem, file_read.sz, file_read.mem);

					std::span decrypted((const cstr*)file_read.mem, file_read.sz / sizeof(cstr));
					auto username_size = state.current_user.size();
					if (decrypted.size() > username_size &&
						!wcsncmp(state.current_user.c_str(), decrypted.data(), username_size) &&
						std::ranges::contains(std::views::reverse(decrypted.subspan(username_size)), cstr{})) {
						set_mode(state, Mode::transition_v0);

						SetWindowText(state.controls.transition_v0.edit_passwords, decrypted.data() + username_size);
						SecureZeroMemory(file_read.mem, file_read.sz);
						start_attempt = login::AttemptResult::success;
					}
					else
						start_attempt = login::AttemptResult::fail_password;
				} 
				else 
					start_attempt = login::AttemptResult::fail_corrupted;
			} break;
			case 1:
			{
				RawBufferReader r(file_read.mem, file_read.sz);
				if (FileHeaderV1 header; r.read(header)) {
					u32 key[8];
					hash_pwd_and_salt(start->password, header.salt, sizeof(header.salt), key);
					twofish_setkey(key, sizeof(key));
					SecureZeroMemory(key, sizeof(key));
					auto data_and_footer_section = r.get_current_subspan();
					if (data_and_footer_section.size() % 16 == 0) {
						auto section_ptr = const_cast<u8*>(data_and_footer_section.data());
						twofish_decrypt(section_ptr, data_and_footer_section.size(), section_ptr);

						if (FileFooterV1 footer; r.cut_from_end(footer)) {
							r.restart_position();
							u8 auth_integrity_hash[32];
							static_assert(sizeof(auth_integrity_hash) == sizeof(footer.auth_integrity_hash));
							sha256(r.bytes.data(), r.bytes.size(), auth_integrity_hash);
							r.skip(sizeof(header));
							if (!memcmp(footer.auth_integrity_hash, auth_integrity_hash, sizeof(footer.auth_integrity_hash))) {
								if (create_password_editors_v1(state, r))
									start_attempt = login::AttemptResult::success;
								else {
									cleanup_password_editors_v1(state);
									start_attempt = login::AttemptResult::fail_corrupted;
								}
								SecureZeroMemory(file_read.mem, file_read.sz);
							}
							else
								start_attempt = login::AttemptResult::fail_password;
						}
						else
							start_attempt = login::AttemptResult::fail_corrupted;
					} else
						start_attempt = login::AttemptResult::fail_corrupted;
				}
				else
					start_attempt = login::AttemptResult::fail_corrupted;
			} break;
			default:
				start_attempt = login::AttemptResult::fail_newer_version;
			}
		}
	}
	else if (file_read.status == ReadEntireFileStatus::missing) {
		//NOTE: if the user previously created an account but didnt save then it will not count as a created account and next time they will be prompted to create the account again, this is a limitation of the fact that we dont save anything inside the user folder till the first time they save what they wrote, therefore we cannot currently do this any other way since there's no information inside the folder to allow us to check whether the second time the user input the same password as the first time
		bool signup = start->signup;
		start_attempt = signup ? login::AttemptResult::success : login::AttemptResult::fail_username;
	}
	else start_attempt = login::AttemptResult::fail_corrupted;
	return start_attempt;
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	State& state = *get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
		Assert(creation_nfo->style & WS_CHILD);

		State* st = new State{};
		st->wnd = hwnd;
		st->nc_parent = GetParent(hwnd);

		st->settings = ((Data*)creation_nfo->lpCreateParams)->settings;
		set_window_state(hwnd, st);
		return 1;
	} break;
	case WM_CREATE:
	{
		add_controls(state);

		add_menus(state);
		LanguageManager::Instance().AddLanguageLayoutHwnd(state.wnd, [](HWND hwnd) {
			resize_controls(*get_state(hwnd));
		});
		LanguageManager::Instance().AddMenuCreatorHwnd(state.wnd, [](HWND hwnd) {
			add_menus(*get_state(hwnd));
		});
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		RECT client_bounds; GetClientRect(hwnd, &client_bounds);
		auto& transition = state.card_animation;
		bool same_size = transition.client_width == RECTW(client_bounds) && transition.client_height == RECTH(client_bounds);
		bool animate = same_size && (transition.requested || transition.timeline.active) &&
			state.mode == Mode::normal && IsWindowVisible(hwnd) && animation::enabled_by_system();
		resize_controls(state, animate);
		return res;
	} break;
	case WM_COMMAND:
	{
		HWND child = (HWND)lparam;
		if (child) { // Child notifications
			if constexpr (debug_text_view) {
				if (child == state.controls.transition_v0.edit_passwords) {
					switch (HIWORD(wparam)) { //Notif code
					case EN_CHANGE:
					{
						set_passwords_need_save(state, true);
						return 0;
					}
					}
				}
			}
		}
		else {
			switch (LOWORD(wparam)) { // Menu & App Shortcut Notifications
			case WMN_SAVE:
			case SHOWPASSWORDS_MENU_SAVE:
			{
				save_passwords(state);
				return 0;
			} break;
			#ifdef _DEBUG
			case SHOWPASSWORDS_MENU_SAVE_V0:
			{
				save_passwords_v0(state);
				return 0;
			} break;
			#endif
			WM_COMMAND_LANGUAGE_CASES(wparam)
			case SHOWPASSWORDS_MENU_UNDO:
			{
				if constexpr (debug_text_view) SendMessage(state.controls.transition_v0.edit_passwords, EM_UNDO, 0, 0); //TODO(fran): undo is terrible in edit controls, we gotta manage that in the subclass //NOTE: WM_UNDO seems to do the same
			} break;
			case SHOWPASSWORDS_MENU_REDO:
			{

			} break;
			case WMN_FIND:
			case SHOWPASSWORDS_MENU_FIND:
			{
				//SendMessage(state.controls.edit_passwords, EM_SHOWSEARCHWND, TRUE, 0);
				auto& text = *edit_oneline::get_state(search::get_state(state.controls.search)->controls.edit_match);
				SetFocus(text.wnd);
				edit_oneline::select_all(text);
				return 0;
			} break;
			case SHOWPASSWORDS_MENU_CHECK_FOR_UPDATES:
			{
				//In the spirit of being totally a offline app we will not provide an automatic updater
				open_link(releases_url);
			} break;
			default: return DefWindowProc(hwnd, msg, wparam, lparam);
			}
		}
		return 0;
	} break;
	case WM_STATE_START:
	{
		/**
		  * Data Formats:
		  * 
		  * V0:
		  *  - file structure: [username | data + null terminator | padding]
		  *  - file encryption: twofish (whole file encrypted)
		  *  - password hashing: sha256
		  *  - integrity/authorization check: username retrieved from file is compared against the username entered by the user
		  *  - data format: plain text, retrieved from a single edit control
		  * 
		  * V1: (first extendible structure, no major encryption changes)
		  *  - file structure: 
		  *    - header: [magic(4bytes "myps" ascii no terminator) | version (4bytes u32) | salt (16bytes, random, new one generated on every login)]
		  *    - data: [structured block data | padding | integrity hash (header + unencrypted data)]
		  *  - file encryption: twofish (only data section encrypted)
		  *  - password hashing: sha256 (+ salt)
		  *  - integrity hashing: sha256 (header + whole unencrypted data section (data + padding))
		  *  - integrity/authorization check: check match against decrypted integrity hash, entire file (header + data section except for integrity hash itself)
		  *  - data format: combination of null terminated strings and multiflags, retrieved from a list of password_editor controls
		  *    - TODO(fran): how to know when a block ends and another begins?
		  * 
		  * V2: 
		  *  - TODO: new random salt generated after every save, not only after every login
		  *  - TODO: v2 should be the security improvement (using Argon/AES-256-GCM, or similar, have previously encrypted chunks affect the encryption of the next chunk, proper authentication and integrity checking, proper password hashing also)
		  */
		login::AttemptResult start_attempt = login::AttemptResult::fail_username;
		auto start = (const Start*)wparam; Assert(start);
		
		if (start) {
			//We gotta keep this data to be able to save the file later, probably some more secure/intelligent ways exist
			state.current_user = str(start->username.str, start->username.sz_chars);

			bool used_backup = false;

			start_attempt = startup(state, start, false);
			if (start_attempt == login::AttemptResult::success && start->signup) {
				auto backup_attempt = startup(state, start, true);
				if (backup_attempt != login::AttemptResult::success) start_attempt = backup_attempt;
			}
			else if (start_attempt != login::AttemptResult::success && start_attempt != login::AttemptResult::fail_signup_username_exists) {
				auto backup_attempt = startup(state, start, true);
				if (backup_attempt == login::AttemptResult::success) {
					start_attempt = backup_attempt;
					used_backup = true;
				}
				else if ((start_attempt == login::AttemptResult::fail_username && backup_attempt != login::AttemptResult::fail_username) ||
					(start_attempt == login::AttemptResult::fail_corrupted && backup_attempt == login::AttemptResult::fail_newer_version))
					start_attempt = backup_attempt;
			}
		
			if (start_attempt == login::AttemptResult::success) {
				if (start->signup) set_passwords_need_save(state, true);
				set_app_shortcuts(state.wnd, ED_SHORTCUTS);
				generate_salt(state.salt); // New salt to be used for all future saves during the current session
				u32 key[8];
				hash_pwd_and_salt(start->password, state.salt, sizeof(state.salt), key);
				twofish_setkey(key, sizeof(key));
				SecureZeroMemory(key, sizeof(key));

				// TODO: Let the page paint its own backdrop instead of forcing a subtree redraw after showing the editor.
				PostMessage(state.wnd, custom_message::redraw_after_show, 0, 0);
				if (state.mode == Mode::transition_v0) PostMessage(state.wnd, custom_message::show_transition_v0_msgbox, 0, 0); //Delay show the messagebox so that our window has time to be shown first
				if (used_backup) PostMessage(state.wnd, custom_message::show_used_backup_msgbox, 0, 0);
			}
		}
		return (LRESULT)start_attempt;
	} break;
	case WM_STATE_RESET:
	{
		state.current_user.clear();
		SetWindowText(state.controls.transition_v0.edit_passwords, nil);//TODO(fran): yet again, we need to zero the mem also
		return 0;
	} break;
	case WM_CLOSE: //Sent by our parent asking whether we want to handle it
	{
		bool client_handled = false;
		if (state.passwords_need_save) {
			int ret = CustomMessageBox(state.wnd, RCS(LANG_UNSAVEDCHANGES_TXT), RCS(LANG_UNSAVEDCHANGES_TITLE), MB_YESNOCANCEL | MB_ICONWARNING | MB_SETFOREGROUND, msgbox_placement);
			switch (ret) {
			case IDCANCEL:
			{
				client_handled = true;
			} break;
			case IDYES:
			{
				client_handled = !save_passwords(state);
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
		stop_card_animation(state);
		save_settings(state);
		if (&state) {
			set_window_state(state.wnd, nil);
			delete &state;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CTLCOLOREDIT:
	{
		HWND ctl = (HWND)lparam;
		HDC dc = (HDC)wparam;
		if (ctl == state.controls.transition_v0.edit_passwords)
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
			i32 w = RECTW(r), offset_x = 0, w_pad = 0;
			auto spacing = DPI(10);
			if (state.mode == Mode::transition_v0) {
				offset_x = w / 2;
				w -= offset_x;
			}
			w_pad = w * pad_percent;

			card.left = offset_x + w_pad;
			card.top = btn_add_start.top - spacing;
			card.right = offset_x + w - w_pad;
			card.bottom = btn_add_start.bottom + spacing;
			//TODO: improve this so it properly matches how it looks like when there are items inside
		}
		urender::draw_round_rectangle(dc, card, card_extent, colors.Card_Bk_Soft);
		return 1;
	} break;
	case WM_LBUTTONDOWN:
	{
		auto res = DefWindowProc(hwnd, msg, wparam, lparam);
		SetFocus(nil);
		return res;
	} break;
	case custom_message::redraw_after_show:
	{
		RedrawWindow(state.wnd, nil, nil,
			RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
	} break;
	case custom_message::show_transition_v0_msgbox:
	{
		CustomMessageBox(state.wnd, RCS(LANG_MSG_TRANSITION_V0_TEXT), RCS(LANG_MSG_TRANSITION_V0_TITLE), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND, msgbox_placement);
	} break;
	case custom_message::show_used_backup_msgbox:
	{
		CustomMessageBox(state.wnd, RCS(LANG_MSG_USED_BACKUP_TEXT), RCS(LANG_MSG_USED_BACKUP_TITLE), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND, msgbox_placement);
	} break;
	default: return DefWindowProc(hwnd, msg, wparam, lparam); break;
	}
	return 0;
}

_init_wndclass_at_startup
}
_add_struct_to_serialization_namespace(editor::Settings)
