#pragma once

/**
  * Core Principles: simple table, maybe it can scroll, but it just renders all items, there's no sparse/virtualized cells
 */

/**----------------------API-----------------------:
  * 
  * wndclass;                                                        Window class name for CreateWindow.
  * void set_functions(HWND table_wnd, const Functions& functions);  Set custom callback functions to perform various actions, nullptr values are ignored.
  * void set_theme(HWND table_wnd, const Theme& theme);              Set custom theme for bk, borders and interior cells.
  * void set_column_cnt(HWND table_wnd, u32 column_cnt);             Set number of columns.
  * 
  *--------------------API END---------------------:
  */
namespace table {
	auto calc_cell_widths(const std::array<UINumber, MAX_COLUMNS>& column_themes, u32 column_cnt, u32 total_w) {
		std::array<u32, MAX_COLUMNS> res;

		if (!column_cnt || !total_w) return decltype(res){ 0 };

		//1. Resolve basic column types
		u32 w_used = 0, dyn_cnt = 0;
		for (u32 i = 0; i < column_cnt; i++) {
			const auto& col = column_themes[i];
			res[i] = col.to_px();
			if (col.type == UINumber::type::dyn)
				dyn_cnt++;
			w_used += res[i];
		}

		//2. Resolve dynamic columns
		u32 min_dyn_w = DPI(8);
		if (total_w > (w_used + dyn_cnt * min_dyn_w)) {
			if (dyn_cnt) {
				u32 col_w = (total_w - w_used) / dyn_cnt;
				for (u32 i = 0; i < column_cnt; i++)
					if (column_themes[i].type == UINumber::type::dyn)
						res[i] = col_w;
			}
		}
		else {
			//We've run out of space, bail out
			u32 col_w = total_w / column_cnt; //TODO: Implement Scrolling. Will allow us here to define a min column width after which we will be forced to allow the columns to overflow the container and enable scrolling. We can also add a tag to the col to indicate if it allows for smaller values than the one it indicated, which would also condition the final width of the cells
			for (u32 i = 0; i < column_cnt; i++)
				res[i] = col_w;
		}
		return res;
	}

	void resize_controls(State& state) {
		auto& controls = state.rows;
		RECT wnd_r; GetClientRect(state.wnd, &wnd_r);
		auto w = RECTW(wnd_r), h = RECTH(wnd_r);
		auto cell_h = DPI(state.theme.cells.row_h);
		auto col_cnt = state.column_cnt;

		auto cols_w = calc_cell_widths(state.theme.cells.columns_w, col_cnt, w);

		RECT row{ 0 };
		for (const auto& c : state.rows) {
			row.bottom += cell_h;
			RECT col = row;
			for (u32 i = 0; i < col_cnt; i++) {
				col.right += cols_w[i];
				RECT cell = col; InflateRect(&cell, -1, -1);
				move_window(c[i], cell);
				col.left = col.right;
			}
			row.top = row.bottom;
		}
	}

	auto get_state(HWND wnd) { _control_create_function__get_state }

	LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		State& state = *get_state(wnd);
		Assert(&state || (msg == WM_NCCREATE));
		switch (msg) {
		case WM_NCCREATE:
		{
			CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
			Assert(creation_nfo->style & WS_CHILD);

			State* st = (State*)calloc(1, sizeof(State));
			Assert(st);
			st->init();
			st->wnd = wnd;
			st->parent = GetParent(wnd);

			set_window_state(wnd, st);
			return 1;
		} break;
		case WM_NCDESTROY:
		{
			state.uninit();
			free(&state);
			set_window_state(wnd, nullptr);
			goto defproc;
		} break;
		case WM_SIZE:
		{
			LRESULT res = DefWindowProc(wnd, msg, wparam, lparam);
			resize_controls(state);
			return res;
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd,&ps); };
			RECT r; GetClientRect(state.wnd, &r);

			if (!state.rows.empty() && state.column_cnt) {
				
				RECT first = get_window_rect_at(state.rows.front()[0], state.wnd);
				RECT last = get_window_rect_at(state.rows.back()[state.column_cnt-1], state.wnd);
				RECT borders = first;
				borders.right = last.right;
				borders.bottom = last.bottom;
				
				auto exclude_clip_rect = [](HDC dc, RECT r) {return ExcludeClipRect(dc, r.left, r.top, r.right, r.bottom); };
				for (const auto& row : state.rows)
					for (u32 i = 0; i < state.column_cnt; i++)
						exclude_clip_rect(dc, get_window_rect_at(row[i], state.wnd));

				FillRect(dc, &borders, state.theme.brushes.border.normal);
			}
		} break;
		case WM_SETCURSOR:
		{
			return handle_wm_setcursor(wnd, msg, wparam, lparam, (HCURSOR)GetClassLongPtr(wnd, GCLP_HCURSOR));
		} break;
		default:
		defproc:
			return DefWindowProc(wnd, msg, wparam, lparam);
		}
		return 0;
	}

	_init_wndclass_at_startup;


	void set_functions(HWND wnd, const Functions& functions) { _control_create_function__set_functions }

	void set_theme(HWND table_wnd, const Theme& theme) {
		State& state = *get_state(table_wnd); Assert(&state);
		auto [repaint, resize] = state.theme.copy_from(theme);
		if (repaint) ask_window_for_repaint(state.wnd);
		if (resize)  ask_window_for_resize(state.wnd);
	}

	void set_column_cnt(HWND table_wnd, u32 column_cnt) {
		State& state = *get_state(table_wnd); Assert(&state);
		Assert(column_cnt <= MAX_COLUMNS);
		state.column_cnt = minimum(column_cnt, MAX_COLUMNS);
		ask_window_for_repaint(state.wnd);
		ask_window_for_resize(state.wnd);
	}

	template<std::same_as<const utf16*>... Args>
	void add_row(State& state, Args... column_values) {
		static_assert(sizeof...(column_values) <= MAX_COLUMNS);
		u32 idx = 0;
		state.rows.push_back({ state.functions.create_control(idx++, state.wnd, column_values)... });
		ask_window_for_resize(state.manager_parent);
		//TODO(fran): BUG when used with password_editor: adding a row after startup doesnt take into account the is_editing state of the parent, controls that should be disabled arent.
		//Though it can be argued that adding a row should signal the parent to set is_editing to true, but still there's no communication of what the initial state should be. Parent should send more information with the complete initial control configuration through column_values
	}

	void delete_row(State& state, HWND containing_wnd) {
		Assert(containing_wnd);
		auto& vec = state.rows;
		for (const auto& [i, r] : vec | std::views::enumerate)
			for (auto col : r)
				if (col == containing_wnd) {
					for (auto c : r) if(c) DestroyWindow(c);
					vec.erase(vec.begin() + i);
					ask_window_for_resize(state.manager_parent);
					ask_window_for_repaint(state.manager_parent);
					return;
				}
	}
}
