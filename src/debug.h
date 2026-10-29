#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "animation.h"
#include <cmath>
#include <cwchar>

#ifdef _DEBUG
namespace debug {

	constexpr auto& wndclass = wndclass_name("debug");

	static auto color_btn_border = CreateSolidBrush(RGB(0,0,0));
	struct SelectorOption { const cstr* name; HBITMAP* bitmap; i32 size; };
#define _debug_bitmap_option(type, name) { _t(#name), &bmps.name, 0 },
	static SelectorOption bitmap_options[]{ foreach_bitmap(_debug_bitmap_option) };
#undef _debug_bitmap_option
	static SelectorOption size_options[]{
		{ _t("16 px"), nil, 16 }, { _t("24 px"), nil, 24 }, { _t("32 px"), nil, 32 },
		{ _t("48 px"), nil, 48 }, { _t("64 px"), nil, 64 }, { _t("96 px"), nil, 96 },
		{ _t("128 px"), nil, 128 },
	};

	struct Controls {
		HWND color_buttons[ARRAYSIZE(colors.brushes)];
		HWND bitmap_selector;
		HWND size_selector;
		HWND bitmap_preview;
		HWND static_bitmap_info;
		HWND static_speed_wnd;
		HWND edit_speed_wnd;
		HWND static_focus_wnd;
	};

	struct State : WindowState {
		HWND* track_wnd;

		HWND focus_wnd;
		rect_i32 raw_preview;

		Controls controls;

		static auto get_state(HWND wnd) { _control_create_function__get_state }

		void update_bitmap_preview() {
			auto listbox_wnd = combobox::get_controls(controls.bitmap_selector).listbox;
			auto option = (SelectorOption*)listbox::get_sel_element(listbox_wnd);
			if (!option) return;
			HBITMAP bitmap = *option->bitmap;
			SendMessage(controls.bitmap_preview, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmap);
			BITMAP info{};
			if (bitmap && GetObject(bitmap, sizeof(info), &info)) {
				auto text = std::format(_t("{} ({}x{}, {} bpp)"), option->name, info.bmWidth, info.bmHeight, info.bmBitsPixel);
				SetWindowText(controls.static_bitmap_info, text.c_str());
			}
			else SetWindowText(controls.static_bitmap_info, option->name);
			ask_window_for_repaint(controls.bitmap_preview);
			ask_window_for_repaint(wnd);
		}
	
		void add_controls() {

			button::Theme t{};
			auto hollow_br = GetStockBrush(HOLLOW_BRUSH);
			for (auto& br : t.brushes.border.all) br = color_btn_border;
			for (auto& br : t.brushes.foreground.all) br = hollow_br;
			t.dimensions.border_thickness = 1;

			edit_oneline::Theme static_theme{
				.brushes = {
					.foreground = {.disabled = colors.ControlTxt},
					.bk = {.disabled = hollow_br},
					.border = {.disabled = hollow_br},
				},
				.font = fonts.General,
			};

			for (const auto& [i, c] : colors.brushes | std::views::enumerate) {
				auto& b = controls.color_buttons[i];
				b = create_window(wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD);
				for (auto& br : t.brushes.bk.all) br = c;
				button::set_theme(b, t);
				auto col = ColorFromBrush(c);
				auto msg = std::format(L"{}\nRGB({}, {}, {})", known_colors_names[i], GetRValue(col), GetGValue(col), GetBValue(col));
				add_mouseover_tooltip(b, (u64)(void*)msg.c_str(), {.multiline = true, .delay_ms = 100});
			}

			controls.static_speed_wnd = create_window(wnd, edit_oneline::wndclass, _t("Animation speed (x)"), WS_VISIBLE | WS_CHILD | WS_DISABLED);
			edit_oneline::set_theme(controls.static_speed_wnd, static_theme);
			auto speed_text = std::format(_t("{}"), animation::Timeline::debug_speed_multiplier);
			controls.edit_speed_wnd = create_window(wnd, edit_oneline::wndclass, speed_text.c_str(), WS_VISIBLE | WS_CHILD | WS_TABSTOP);
			auto speed_theme = themes.base_editoneline;
			speed_theme.font = fonts.General;
			edit_oneline::set_theme(controls.edit_speed_wnd, speed_theme);
			edit_oneline::set_functions(controls.edit_speed_wnd, {
				.on_change = [](void*, HWND edit_wnd) {
					cstr text[32];
					i32 length = GetWindowTextLength(edit_wnd);
					if (length <= 0 || length >= ARRAYSIZE(text)) return;
					GetWindowText(edit_wnd, text, ARRAYSIZE(text));
					cstr* end = nil;
					f32 speed = std::wcstof(text, &end);
					if (end != text && !*end && std::isfinite(speed) && speed > 0.f)
						animation::Timeline::debug_speed_multiplier = speed;
				}
			});

			controls.static_focus_wnd = create_window(wnd, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_DISABLED);
			edit_oneline::set_theme(controls.static_focus_wnd, static_theme);

			controls.bitmap_selector = create_window(wnd, combobox::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP);
			controls.size_selector = create_window(wnd, combobox::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP);
			Assert(controls.bitmap_selector && controls.size_selector);
			void* bitmap_items[ARRAYSIZE(bitmap_options)];
			for (size_t i = 0; i < ARRAYSIZE(bitmap_options); i++) bitmap_items[i] = &bitmap_options[i];
			void* size_items[ARRAYSIZE(size_options)];
			for (size_t i = 0; i < ARRAYSIZE(size_options); i++) size_items[i] = &size_options[i];
			listbox::set_elements(combobox::get_controls(controls.bitmap_selector).listbox, bitmap_items, ARRAYSIZE(bitmap_items));
			listbox::set_elements(combobox::get_controls(controls.size_selector).listbox, size_items, ARRAYSIZE(size_items));

			auto render_selector = [](HDC dc, rect_i32 r, combobox::RenderFlags flags, void* element, void*) {
				auto& theme = themes.base_btn;
				HBRUSH bk_br = flags.is_listbox_open ? theme.brushes.bk.normal :
					flags.on_clicked ? theme.brushes.bk.clicked :
					flags.on_mouseover ? theme.brushes.bk.mouseover : theme.brushes.bk.disabled;
				urender::draw_background(dc, r.to_RECT(), bk_br, theme.brushes.foreground.normal, theme.dimensions);
				i32 pad = avg_str_dim(fonts.General, 1).cx;
				HBITMAP arrow = flags.is_listbox_open ? bmps.dropdown_up : bmps.dropdown;
				i32 icon_x = urender::draw_bitmap_1bpp_right(arrow, dc, r, pad, theme.brushes.foreground.normal);
				RECT text_bounds = r.to_RECT();
				text_bounds.left += pad;
				text_bounds.right = icon_x - pad;
				if (element) urender::draw_text(dc, text_bounds, to_utf_str(const_cast<cstr*>(((SelectorOption*)element)->name)),
					fonts.General, colors.ControlTxt, urender::TextAlignment::left, 0);
			};
			auto render_option = [](HDC dc, rect_i32 r, listbox::RenderFlags flags, void* element, void*, listbox::ElementPass pass) -> i32 {
				const cstr* name = ((SelectorOption*)element)->name;
				i32 x_pad = avg_str_dim(fonts.General, 1).cx;
				if (pass == listbox::ElementPass::measure) return urender::measure_text(dc, fonts.General, str_view{ name }).cx + 2 * x_pad;
				HBRUSH background = flags.on_clicked ? colors.ControlBkPush :
					flags.on_mouseover || flags.on_selected ? colors.ControlBkMouseOver :
					colors.ControlBk;
				RECT bounds = r.to_RECT();
				FillRect(dc, &bounds, background);
				urender::draw_text(dc, bounds, to_utf_str(const_cast<cstr*>(name)),
					fonts.General, colors.ControlTxt, urender::TextAlignment::left, x_pad);
				return 0;
			};
			for (HWND selector : { controls.bitmap_selector, controls.size_selector }) {
				combobox::set_user_extra(selector, this);
				combobox::set_function_render_combobox(selector, render_selector);
				combobox::set_function_render_listbox_element(selector, render_option);
				combobox::set_function_on_listbox_opening(selector, [](HWND, HWND listbox, void*) {
					SetWindowPos(listbox, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				});
			}
			combobox::set_function_on_selection_accepted(controls.bitmap_selector, [](void*, void* user_extra) {
				auto& state = *(State*)user_extra;
				state.update_bitmap_preview();
			});
			combobox::set_function_on_selection_accepted(controls.size_selector, [](void*, void* user_extra) {
				auto& state = *(State*)user_extra;
				state.resize_controls();
			});
			combobox::set_cur_sel(controls.bitmap_selector, 0);
			combobox::set_cur_sel(controls.size_selector, 4);

			controls.bitmap_preview = create_window(wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD | BS_BITMAP);
			button::set_theme(controls.bitmap_preview, themes.base_btn);
			controls.static_bitmap_info = create_window(wnd, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_DISABLED);
			edit_oneline::set_theme(controls.static_bitmap_info, static_theme);
			update_bitmap_preview();
		}

		void resize_controls() {
			RECT r; GetClientRect(wnd, &r);
			auto w = RECTW(r), h = RECTH(r);
			auto min_dim = minimum(w, h);

			auto pad = DPI(8);

			rect_i32 bounds{}; bounds.wh = { w, h };

			rect_i32 static_focus_wnd = bounds.cut_bottom(DPI(30));
			rect_i32 speed_label = bounds.cut_bottom(DPI(30));
			speed_label.apply_padding(pad, 0);
			rect_i32 speed_edit = speed_label.cut_right(DPI(80));
			speed_edit.apply_padding(pad, 0, 0, 0);
			rect_i32 bitmap_info = bounds.cut_bottom(DPI(26));
			bitmap_info.apply_padding(pad, 0);
			rect_i32 preview_area = bounds.cut_bottom(size_options[ARRAYSIZE(size_options) - 1].size + 2 * pad);
			rect_i32 bitmap_selectors = bounds.cut_bottom(DPI(32));
			bitmap_selectors.apply_padding(pad, 0);
			rect_i32 size_selector = bitmap_selectors.cut_right(DPI(120));
			size_selector.apply_padding(pad, 0, 0, 0);
			rect_i32 bitmap_selector = bitmap_selectors;

			auto size_listbox = combobox::get_controls(controls.size_selector).listbox;
			auto selected_size = (SelectorOption*)listbox::get_sel_element(size_listbox);
			i32 preview_size = selected_size ? selected_size->size : size_options[4].size;
			rect_i32 button_area = preview_area.cut_left(preview_area.w / 2);
			preview_size = minimum(preview_size, maximum(0, minimum(button_area.w, button_area.h) - 2 * pad));
			preview_size = preview_size / 8 * 8;
			rect_i32 preview_button = button_area.cut_center(preview_size);
			raw_preview = preview_area.cut_center(preview_size);

			auto [rows, cols, dim] = pack_squares(bounds.w, bounds.h, ARRAYSIZE(controls.color_buttons), pad);

			i32 start_offset_x = pad, offset_x = start_offset_x, offset_y = pad;
			for (i32 j = 0; j < rows; j++) {
				for (i32 i = 0; i < cols; i++) {
					auto idx = i + j * cols;
					if (idx >= ARRAYSIZE(controls.color_buttons)) break;
					auto& b = controls.color_buttons[idx];
					MoveWindow(b, rect_i32{ .x = offset_x, .y = offset_y, .w = dim, .h = dim });
					offset_x += dim + pad;
				}
				offset_y += dim + pad;
				offset_x = start_offset_x;
			}

			MoveWindow(controls.static_speed_wnd, speed_label);
			MoveWindow(controls.edit_speed_wnd, speed_edit);
			MoveWindow(controls.static_focus_wnd, static_focus_wnd);
			MoveWindow(controls.bitmap_selector, bitmap_selector);
			MoveWindow(controls.size_selector, size_selector);
			MoveWindow(controls.bitmap_preview, preview_button);
			MoveWindow(controls.static_bitmap_info, bitmap_info);
			ask_window_for_repaint(wnd);
		}

		void follow_track_window() {
			constexpr auto anim_id = 54896, refresh_ms = 1000;

			static void (*anim_proc)(HWND, UINT, UINT_PTR, DWORD) = [](HWND wnd, UINT, UINT_PTR anim_id, DWORD) {
				auto& state = *get_state(wnd);
				if (&state) {
					if (!state.track_wnd) goto set_timer;
					HWND track = *state.track_wnd;
					HWND follower = state.parent;

					if (!IsWindowVisible(track)) {
						ShowWindow(follower, SW_HIDE);
						goto set_timer;
					}
					if (!IsWindowVisible(follower)) ShowWindow(follower, SW_SHOWNOACTIVATE);

					RECT _target{}; GetWindowRect(track, &_target);
					auto target = rect_i32::create_from(_target);
					RECT _follow{}; GetWindowRect(follower, &_follow);
					auto follow_w = RECTW(_follow), follow_h = RECTH(_follow);
					i32 min_dim = DPI(200);
					if (follow_w < min_dim) follow_w = min_dim;
					if (follow_h < min_dim) follow_h = min_dim;
					auto follow = rect_i32{ .x = target.right(), .y = target.y, .w = follow_w, .h = follow_h };

					if (auto monitor = MonitorFromWindow(wnd, MONITOR_DEFAULTTONULL); monitor) {
						MONITORINFOEX info; info.cbSize = sizeof(info);
						if (GetMonitorInfo(monitor, &info)) {
							auto bounds = info.rcWork;
							if (!(test_pt_rc(POINT{ .x = follow.left,.y = follow.top }, bounds) &&
								test_pt_rc(POINT{ .x = follow.left,.y = follow.bottom() }, bounds) &&
								test_pt_rc(POINT{ .x = follow.right(),.y = follow.top }, bounds) &&
								test_pt_rc(POINT{ .x = follow.right(),.y = follow.bottom() }, bounds))) {
								follow.x = target.x - follow.w;
							}
							//TODO: idk what we want to do if maximized, show inside the track wnd?
						}
					}

					MoveWindow(follower, follow);
					SetWindowPos(follower, track, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOACTIVATE);
				}
				else {
					KillTimer(wnd, anim_id);
					return;
				}
				set_timer:
				SetTimer(wnd, anim_id, refresh_ms, anim_proc);
			};

			SetTimer(wnd, anim_id, refresh_ms, anim_proc);
		}
	
		void track_focus_window() {
			constexpr auto anim_id = 49856, refresh_ms = 1000;

			static void (*anim_proc)(HWND, UINT, UINT_PTR, DWORD) = [](HWND wnd, UINT, UINT_PTR anim_id, DWORD) {
				auto& state = *get_state(wnd);
				if (&state) {
					if (!state.track_wnd || !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) goto set_timer;
					POINT cursor; GetCursorPos(&cursor); MapWindowPoints(nil, *state.track_wnd, &cursor, 1);
					HWND track = child_wnd_from_point(cursor, *state.track_wnd);

					if (track != state.focus_wnd) {
						state.focus_wnd = track;
						if (state.focus_wnd) {
							cstr cls[100]; *cls = 0;
							GetClassName(state.focus_wnd, cls, ARRAYSIZE(cls));
							str hex_str = std::format(_t("{} ({:010})"), cls, (void*)state.focus_wnd);
							SetWindowText(state.controls.static_focus_wnd, hex_str.c_str());
						}
						else SetWindowText(state.controls.static_focus_wnd, 0);
					}
				}
				else {
					KillTimer(wnd, anim_id);
					return;
				}
				set_timer:
				SetTimer(wnd, anim_id, refresh_ms, anim_proc);
			};

			SetTimer(wnd, anim_id, refresh_ms, anim_proc);
		}
	};

	LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto& state = *State::get_state(wnd);
		switch (msg)
		{
		case WM_NCCREATE: {
			State* st = (State*)calloc(1, sizeof(State)); Assert(st);
			set_window_state(wnd, st);
			CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;
			st->parent = creation_nfo->hwndParent;
			st->wnd = wnd;
			st->track_wnd = (HWND*)creation_nfo->lpCreateParams;
			SetWindowText(st->parent, _t("Debug"));
		} break;
		case WM_CREATE:
		{
			state.add_controls();
			state.resize_controls();
			state.follow_track_window();
			state.track_focus_window();
		} break;
		case WM_SIZE:
		{
			LRESULT res = DefWindowProc(wnd, msg, wparam, lparam);
			state.resize_controls();
			return res;
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(wnd, &ps); defer{ EndPaint(wnd, &ps); };
			auto bitmap_listbox = combobox::get_controls(state.controls.bitmap_selector).listbox;
			auto selected = (SelectorOption*)listbox::get_sel_element(bitmap_listbox);
			if (!selected) return 0;
			HBITMAP bitmap = *selected->bitmap;
			BITMAP info{};
			if (!bitmap || !GetObject(bitmap, sizeof(info), &info) || state.raw_preview.w <= 0 || state.raw_preview.h <= 0) return 0;
			if (info.bmBitsPixel == 1)
				urender::draw_mask(dc, state.raw_preview.x, state.raw_preview.y, state.raw_preview.w, state.raw_preview.h,
					bitmap, 0, 0, info.bmWidth, info.bmHeight, colors.ControlTxt);
			else if (info.bmBitsPixel == 8)
				urender::draw_menu_mask8(dc, state.raw_preview.x, state.raw_preview.y, state.raw_preview.w, state.raw_preview.h, bitmap, colors.ControlTxt);
			return 0;
		} break;
		case WM_ERASEBKGND:
		{
			auto dc = (HDC)wparam;
			RECT r; GetClientRect(state.wnd, &r);
			FillRect(dc, &r, colors.ControlBk);
			return 1;
		} break;
		}
		return DefWindowProc(wnd, msg, wparam, lparam);
	}
	_init_wndclass_at_startup;
}
#endif
