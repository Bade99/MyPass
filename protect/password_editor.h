#pragma once
/**
 * Nice looking collection of data for a password, format:
 *	Title (on the right should have a button to allow editing, maybe)
 *	table section that can have rows added
 *	  description (modifiers: is_password) | value 
 *    (by default we should generate two rows, one with desc set to 'user', and the other with desc = 'password' and modifier is_password)
 *	  (is_password means that the value field should remain hidden until you hover over it)
 * 
 * This sections could be closed by default, showing only the title, then you click to open the subitems.
 * Once we have a list of these then allow the user to sort this collections by alphabetical order of the Title asc or desc, by date modified asc or desc
 * Also allow to set a collection as pinned, which will basically be a high priority list that will be sorted and placed on top first.
 * Include an 'Add' button, could be floating on the side or we could have it show up in between collections if you mouseover, as well as on the top most and bottom most parts of the list.
 * Allow to search by title (exact string match, lets not go crazy): we should hide items that dont match, and maybe we should sort the remaining items by the best match?
 * 
 */

 //TODO(fran): virtual window manager, hwnd that handles other hwnds, so we can do scrolling, have the virtual manager be passed a way to generate my windows, in this case the password editor. It will only generate a few windows to cover the visible space, and on demand switch the state of the windows when scrolling to simulate going over the whole list of passwords

namespace password_editor {

	namespace value_cell {
		constexpr auto& wndclass = wndclass_name("password_editor_value_cell");

		struct State : WindowState {
			union Controls {
				struct {
					HWND text, btn_show, btn_copy, btn_lock, btn_del;
				};
				HWND all[5];
			private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
			} controls;

			bool default_hidden;

			void set_default_hidden(bool new_default_hidden) {
				auto& state = *this;
				state.default_hidden = new_default_hidden;

				auto& controls = state.controls;

				LONG_PTR style = GetWindowLongPtr(controls.text, GWL_STYLE);
				if (state.default_hidden) style |= ES_PASSWORD;
				else style &= ~ES_PASSWORD;
				SetWindowLongPtr(controls.text, GWL_STYLE, style);
				ask_window_for_repaint(controls.text);

				button::set_selected(controls.btn_lock, state.default_hidden);

				ShowWindow(controls.btn_show, state.default_hidden ? SW_SHOW : SW_HIDE);
				SendMessage(controls.btn_show, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(state.default_hidden ? bmps.eye_open : bmps.eye_closed));
			}
		};

		auto get_state(HWND wnd) { _control_create_function__get_state }

		LRESULT proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			auto& state = *get_state(wnd);
			if (!&state) return DefWindowProc(wnd, msg, wparam, lparam);
			switch (msg) {
			case WM_SIZE: {
				//TODO(fran): make into reusable function for placing objects horizontally on a toolbar
				auto& controls = state.controls;
				RECT wnd_r; GetClientRect(state.wnd, &wnd_r);
				auto w = RECTW(wnd_r), h = RECTH(wnd_r);
				i32 title_w = w * .75f * .75f;
				i32 toolbar_w = w - title_w;
				i32 pad = DPI(8);

				RECT title_r{
					.left = 0, .top = 0,
					.right = title_r.left + title_w, .bottom = title_r.top + h
				};
				move_window(controls.text, title_r);

				auto tools = { controls.btn_show, controls.btn_copy, controls.btn_lock, controls.btn_del };
				const auto tool_cnt = tools.size();
				auto total_tool_pad = (tool_cnt - 1) * pad;
				auto toolbar_usable_w = toolbar_w - total_tool_pad;
				auto tool_h = RECTH(title_r);
				auto max_tool_w = tool_h;
				auto tool_w = (i32)minimum(toolbar_usable_w / tool_cnt, max_tool_w);
				auto tool_x = w - total_tool_pad - tool_cnt * tool_w;
				for (const auto& c : tools) {
					RECT tool_r;
					tool_r.left = tool_x;
					tool_r.top = title_r.top;
					tool_r.right = tool_x + tool_w;
					tool_r.bottom = title_r.bottom;
					move_window(c, tool_r);
					tool_x = tool_r.right + pad;
				}
			} break;
			case WM_ENABLE_REQUEST: {
				BOOL should_enable = wparam;
				EnableWindow(state.controls.text, should_enable);
			} break;
			case WM_NCDESTROY: {
				if (&state) {
					set_window_state(state.wnd, nil);
					free(&state);
				}
				goto defproc;
			} break;
			default: defproc: return DefWindowProc(wnd, msg, wparam, lparam);
			}
			return 0;
		};
		_init_wndclass_at_startup;
	}

void on_internal_state_change(State& state) {
	auto& controls = state.controls;
	button::set_selected(controls.btn_card, state.is_open);

	for (const auto& c : { controls.btn_dates, controls.btn_edit, controls.btn_add, controls.btn_del, controls.tbl_values })
		ShowWindow(c, state.is_open ? SW_SHOW : SW_HIDE);

	bool enable_editing = state.is_open && state.is_editing;
	EnableWindow(controls.edo_title, enable_editing);

	SendMessage(controls.btn_edit, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(enable_editing ? bmps.cancel : bmps.edit));
		
	//TODO: may be better to ask the table to do this child disabling logic by itself
	//Also this is too restrictive, we want to send a message to the individual cells to disable themselves, so they can decide what needs disabling and what not, eg a 'copy field' button should always stay enabled
	auto& tbl = *table::get_state(controls.tbl_values);
	for (auto& r : tbl.rows) {
		EnableWindow(r[0], enable_editing);
		PostMessage(r[1], WM_ENABLE_REQUEST, enable_editing, 0);
	}
	ask_window_for_resize(state.manager_parent);
	ask_window_for_repaint(state.manager_parent);
}

void set_is_open(State& state, bool is_open) {
	if (state.is_open != is_open) {
		state.is_open = is_open;
		on_internal_state_change(state);
	}
}

void on_properties_changed(State& state) {
	//TODO(fran): update tip text on change to modified state (at least this way we dont have to use the msg queue and the proc)
	using namespace std::chrono;
	TOOLINFO toolInfo{ sizeof(toolInfo) };
	toolInfo.hwnd = state.controls.btn_dates;
	toolInfo.uId = (UINT_PTR)state.controls.btn_dates;
	constexpr auto date_format = L"{:%F %T}"; //TODO(fran): we could try to use the local date format of the user
	auto created = std::format(
		date_format,
		zoned_time{ current_zone(), sys_seconds{ seconds(state.properties.date_created) } }
	);
	auto modified = std::format(
		date_format,
		zoned_time{ current_zone(), sys_seconds{ seconds(state.properties.date_modified) } }
	);
	auto dates_msg = std::vformat(
		RS(LANG_PWD_ED_DATES),
		std::make_wformat_args(created, modified)
	);
	toolInfo.lpszText = dates_msg.data();
	SendMessage(state.controls.tooltip_btn_dates, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo);
}

void table_add_row(State& state, const utf16* description, const password_editor::ValueCell* value) {
	auto& tbl = *table::get_state(state.controls.tbl_values);
	table::add_row(tbl, description, value);
}

void create_controls(State& state) {
	//TODO(fran): this is an interesting case where I would not want password_editor itself to be and hwnd at all, just a manager for this btn_card which should be the main hwnd from which others are created inside of. Can I just have this object not exist and be just a handler, or maybe a worse solution would be have it be a subclass of button?
	auto& controls = state.controls;
	controls.btn_card = create_window(state.wnd, button::wndclass);
	button::set_theme(controls.btn_card, themes.password_editor_card_btn);
	button::set_user_data(controls.btn_card, &state);
	button::set_functions(controls.btn_card, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			set_is_open(state, !state.is_open);
		}
	});
	
	//TODO(fran): this controls being dynamic, with the possibility of the parent being eliminated and thus them too, poses a big issue for the language_manager, it could be setting the text of another control without knowing. We would need to manually unregister the control from the language_manager on delete, a huge pain.

	controls.edo_title = create_window(controls.btn_card, edit_oneline::wndclass);
	auto hollow_brush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	edit_oneline::set_theme(controls.edo_title, themes.clear_editoneline);

	controls.btn_dates = create_window(controls.btn_card, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
	SendMessage(controls.btn_dates, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.calendar);
	button::set_theme(controls.btn_dates, themes.password_editor_toolbar_btn_static);
	controls.tooltip_btn_dates = add_mouseover_tooltip(controls.btn_dates, 0, 
		{.multiline = true, .delay_ms = 200, .duration_ms = 15000}
	);

	controls.btn_edit = create_window(controls.btn_card, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
	SendMessage(controls.btn_edit, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.edit);
	button::set_theme(controls.btn_edit, themes.password_editor_toolbar_btn);
	add_mouseover_tooltip(controls.btn_edit, LANG_PWD_ED_EDIT);
	button::set_user_data(controls.btn_edit, &state);
	button::set_functions(controls.btn_edit, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			state.is_editing = !state.is_editing;
			on_internal_state_change(state);
		}
	});

	controls.btn_add = create_window(controls.btn_card, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
	button::set_theme(controls.btn_add, themes.password_editor_toolbar_btn);
	SendMessage(controls.btn_add, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.add);
	add_mouseover_tooltip(controls.btn_add, LANG_PWD_ED_ADD);
	button::set_user_data(controls.btn_add, &state);
	button::set_functions(controls.btn_add, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			auto& tbl = *table::get_state(state.controls.tbl_values);
			table_add_row(state, L"", &empty_value_cell);
			// When a row is added we go into editing mode to allow the user to edit directly
			state.is_editing = true;
			on_internal_state_change(state);
		}
	});
	
	controls.btn_del = create_window(controls.btn_card, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
	SendMessage(controls.btn_del, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.bin);
	button::set_theme(controls.btn_del, themes.password_editor_toolbar_btn_danger);
	add_mouseover_tooltip(controls.btn_del, LANG_PWD_ED_DELETE);
	button::set_user_data(controls.btn_del, &state);
	button::set_functions(controls.btn_del, {
		.on_click = [](void* data) {
			auto& state = *(State*)data;
			state.functions.on_delete(state.wnd, state.user_data);
		}
	});

	controls.tbl_values = create_window(controls.btn_card, table::wndclass);
	set_window_manager_parent(controls.tbl_values, state.manager_parent); //TODO: this does not look like a good solution, again depends on the parent properly managing the resizing, meaning that we again are inverting the responsibility, the control shouldnt know who resizes it. On the other hand, if we expand the manager_parent all the way through the window stack then it would be ok, here we would do: table::set_manager_parent(controls.tbl_values, state.manager_parent);
	table::Functions tbl_values_functions{
		.create_control = [](u32 column_idx, HWND parent, const void* data) -> HWND {
			//TODO: first hurdle with this architecture, I dont have an initialization stage, where I can tell the table that I actually want the first two cells in the first column to, by default have title 'username', and the second one 'password' (we can though fix this not via initialization but by passing extra info to this function like row_idx, or the complete table state, or a pointer to some additional setup parameters)
			Assert(data);
			auto tbl_font = fonts.General;
			auto hollow_brush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
			switch (column_idx) {
				case 0: 
				{
					auto text = (const utf16*)data;
					auto wnd = create_window(parent, edit_oneline::wndclass);
					SetWindowTextW(wnd, text);
					edit_oneline::set_theme(wnd, themes.clear_editoneline);
					AWDT(wnd, LANG_PWD_ED_TBL_FIELD);
					SetWindowFont(wnd, fonts.General, true);
					return wnd;
				}
				case 1: 
				{
					auto cell = (const ValueCell*)data;

					auto& state = *(value_cell::State*)calloc(1, sizeof(value_cell::State));
					state.parent = parent;
					state.wnd = create_window(parent, value_cell::wndclass);
					
					auto& controls = state.controls;

					controls.text = create_window(state.wnd, edit_oneline::wndclass);
					AWDT(controls.text, LANG_PWD_ED_TBL_VALUE);
					SetWindowTextW(controls.text, cell->text);
					edit_oneline::set_theme(controls.text, themes.clear_editoneline);

					controls.btn_show = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
					SendMessage(controls.btn_show, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.eye_open);
					add_mouseover_tooltip(controls.btn_show, LANG_PWD_ED_TBL_SHOW);
					button::set_user_data(controls.btn_show, &state);
					button::set_functions(controls.btn_show, {
						.on_click = [](void* data) {
							value_cell::State& state = *(value_cell::State*)data;
							auto& controls = state.controls;
							LONG_PTR style = GetWindowLongPtr(controls.text, GWL_STYLE) ^ ES_PASSWORD;
							SetWindowLongPtr(controls.text, GWL_STYLE, style);
							ask_window_for_repaint(controls.text);
							SendMessage(controls.btn_show, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(style & ES_PASSWORD ? bmps.eye_open : bmps.eye_closed));
							//TODO: move to listener like set_default_hidden, and modify btn colors based on state
						}
					});

					controls.btn_copy = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
					SendMessage(controls.btn_copy, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.clipboard);
					add_mouseover_tooltip(controls.btn_copy, LANG_PWD_ED_TBL_COPY);
					button::set_user_data(controls.btn_copy, &state);
					button::set_functions(controls.btn_copy, {
						.on_click = [](void* data) {
							value_cell::State& state = *(value_cell::State*)data;
							auto& text = state.controls.text;
							auto& text_state = *edit_oneline::get_state(text);
							auto res = copy_text_to_clipboard(text_state.char_text.c_str(), text_state.char_text.length());
							if (res == -1) {
								toast::show(state.wnd, RS(LANG_COPY_TO_CLIPBOARD_ERROR), toast::type::failure);
							} elif(res != 0) {
								toast::show(state.wnd, RS(LANG_COPY_TO_CLIPBOARD), toast::type::success);
							}
						}
					});

					controls.btn_lock = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
					SendMessage(controls.btn_lock, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.padlock);
					add_mouseover_tooltip(controls.btn_lock, LANG_PWD_ED_TBL_LOCK);
					button::set_user_data(controls.btn_lock, &state);
					button::set_functions(controls.btn_lock, {
						.on_click = [](void* data) {
							value_cell::State& state = *(value_cell::State*)data;
							state.set_default_hidden(!state.default_hidden);
						}
					});

					controls.btn_del = create_window(state.wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
					SendMessage(controls.btn_del, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.bin);
					add_mouseover_tooltip(controls.btn_del, LANG_PWD_ED_TBL_DELETE);
					button::set_user_data(controls.btn_del, &state);
					button::set_functions(controls.btn_del, {
						.on_click = [](void* data) {
							value_cell::State& state = *(value_cell::State*)data;
							auto& tbl_state = *table::get_state(state.parent);
							table::delete_row(tbl_state, state.wnd);
						}
					});

					for (const auto& c : { controls.btn_show, controls.btn_copy, controls.btn_lock}) 
						button::set_theme(c, themes.table_toolbar_btn);
					button::set_theme(controls.btn_del, themes.table_toolbar_btn_danger);
					for (const auto& c : controls.all) SetWindowFont(c, tbl_font, true);
					SetWindowFont(controls.text, fonts.GeneralBold, true);

					set_window_state(state.wnd, &state);

					state.set_default_hidden(cell->flags & ValueCellFlag::lock);

					return state.wnd;
				}
				default: Assert(0); return nullptr;
			}
		}
	};
	table::set_functions(controls.tbl_values, tbl_values_functions);
	table::set_column_cnt(controls.tbl_values, 2);
	table::Theme tbl_theme = themes.base_table; 
	tbl_theme.cells = {
		.row_h = 30,
		.columns_w = {UINumber{.value = 100}, UINumber{.type = UINumber::type::dyn}}
	};
	table::set_theme(controls.tbl_values, tbl_theme);

	for (const auto& c : controls.all)
		if (c != controls.edo_title && c != controls.tooltip_btn_dates) 
			SetWindowFont(c, fonts.General, true);
	SetWindowFont(controls.edo_title, fonts.GeneralBold, true);

	on_internal_state_change(state);
}

i32 resize_controls(const State& state, bool resize = true) {
	auto& controls = state.controls;
	RECT wnd_r; GetClientRect(state.wnd, &wnd_r);
	auto w = RECTW(wnd_r), h = RECTH(wnd_r);
	i32 title_h = DPI(30);
	i32 margin_x = title_h;
	i32 title_w = w * .75f * .75f;
	i32 toolbar_w = (w - title_h * 2) - title_w;
	i32 pad = title_h / 4;

	if (resize) move_window(controls.btn_card, wnd_r);

	if (!state.is_open) {
		RECT title_r { 
			.left = margin_x, 
			.top = maximum((h - title_h) / 2, 0),
			.right = title_r.left + title_w, 
			.bottom = title_r.top + title_h
		};
		if (resize) move_window(controls.edo_title, title_r);
		return RECTH(title_r);
	}
	else {

		RECT title_r { 
			.left = margin_x, 
			.top = title_h / 2,
			.right = title_r.left + title_w, 
			.bottom = title_r.top + title_h };

		const auto tools = { controls.btn_dates, controls.btn_edit, controls.btn_add, controls.btn_del };
		const auto tool_cnt = tools.size();
		auto total_tool_pad = (tool_cnt - 1) * pad;
		auto toolbar_usable_w = toolbar_w - total_tool_pad;
		auto max_tool_w = DPI(80);
		auto tool_w = (i32)minimum(toolbar_usable_w / tool_cnt, max_tool_w);
		auto tool_x = w - margin_x - total_tool_pad - tool_cnt * tool_w;
		for (const auto& c : tools) {
			RECT tool_r;
			tool_r.left = tool_x;
			tool_r.top = title_r.top;
			tool_r.right = tool_x + tool_w;
			tool_r.bottom = title_r.bottom;
			move_window(c, tool_r);
			tool_x = tool_r.right + pad;
		}
		const auto& tbl = *table::get_state(controls.tbl_values);
		RECT values_r {
			.left = title_r.left,
			.top = title_r.bottom + pad,
			.right = w - values_r.left,
			.bottom = values_r.top + (i32)(tbl.rows.size() * tbl.theme.cells.get_row_h_in_pixels())
		};

		if (resize) {
			move_window(controls.edo_title, title_r);
			move_window(controls.tbl_values, values_r);
		}
		return values_r.bottom + pad * 2;
	}
}

auto get_state(HWND wnd) { return get_window_state<State>(wnd); }

LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	auto& state = *get_state(wnd);
	Assert(&state || (msg == WM_NCCREATE));
	switch (msg) {
	case WM_NCCREATE:
	{
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		Assert(creation_nfo->style & WS_CHILD);

		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		st->wnd = wnd;
		st->parent = GetParent(wnd);
		st->manager_parent = (HWND)creation_nfo->lpCreateParams; //TODO(fran): iron out the concept of the manager parent, since some windows, like this one, aim to pass this value along to its controls, and its controls are created immediately when create window is called, we cannot really use an extra call to the new set_window_manager_parent since at that point it would be too late. One solution would be to have per namespace set_manager_parent functions, so that we can also update the manager parent of our sub controls

		set_window_state(wnd, st);
		return 1;
	} break;
	case WM_NCDESTROY:
	{
		free(&state);
		set_window_state(wnd, nullptr);
		goto defproc;
	} break;
	case WM_CREATE:
	{
		create_controls(state);
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(wnd, msg, wparam, lparam);
		resize_controls(state);
		return res;
	} break;
	//case WM_NOTIFY:
	//{
	//	//TODO: look into this for custom coloring the tooltips
	//	return L"FFF";
	//	//add_mouseover_tooltip(controls.btn_dates, LANG_PWD_ED_DATES);
	//	NMHDR* msg_info = (NMHDR*)lparam;
	//	if (msg_info->hwndFrom == state.controls.tooltip) {
	//		switch (msg_info->code) {
	//		case NM_CUSTOMDRAW:
	//		{
	//			NMTTCUSTOMDRAW* cd = (NMTTCUSTOMDRAW*)lparam;
	//			//INFO: cd->uDrawFlags = flags for DrawText
	//			switch (cd->nmcd.dwDrawStage) {
	//				//TODO(fran): probably case CDDS_PREERASE: for SetBkColor for the background
	//			case CDDS_PREPAINT:
	//			{
	//				return CDRF_NOTIFYITEMDRAW;//TODO(fran): I think im lacking something here, we never get CDDS_ITEMPREPAINT, it's possible the msgs are not sent cause it uses a visual style, in which case it doesnt care about you, we would have to remove it with setwindowtheme, and since there wont be any style we'll have to draw it completely ourselves I guess
	//			} break;
	//			case CDDS_ITEMPREPAINT://Setup before painting an item
	//			{
	//				//SelectFont(cd->nmcd.hdc, state.theme.font);
	//				//SetTextColor(cd->nmcd.hdc, ColorFromBrush(state.theme.brushes.foreground.normal));
	//				//SetBkColor(cd->nmcd.hdc, ColorFromBrush(state.theme.brushes.bk.normal));
	//				return CDRF_NEWFONT;
	//			} break;
	//			default: return CDRF_DODEFAULT;
	//			}
	//		} break;
	//		case TTN_GETDISPINFO:
	//			Assert(0);
	//		case TTN_LINKCLICK:
	//			Assert(0);
	//		case TTN_POP://Tooltip is about to be hidden
	//			return 0;
	//		case TTN_SHOW://Tooltip is about to be shown
	//			return 0;//here we can change the tooltip's position
	//		}
	//	}
	//	return DefWindowProc(hwnd, msg, wparam, lparam);
	//} break;
	default:
	defproc:
		return DefWindowProc(wnd, msg, wparam, lparam);
	}
	return 0;
}

_init_wndclass_at_startup;

void set_user_data(HWND wnd, void* user_data) { _control_create_function__set_user_data }

void set_functions(HWND wnd, const Functions& functions) { _control_create_function__set_functions }

void set_properties(HWND wnd, const Properties& properties) {
	State& state = *get_state(wnd);
	state.properties = properties; //TODO: allow for only setting some properties and not other like we do with functions and theme
	on_properties_changed(state);
}

}
