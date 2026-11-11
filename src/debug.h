#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "animation.h"
#include <cmath>
#include <cwchar>

#if APP_DIAGNOSTICS_ENABLED
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
	static constexpr std::array resize_events{
		render_diagnostics::Event::window_position_changed,
		render_diagnostics::Event::window_nonclient_recalc,
		render_diagnostics::Event::window_resize,
		render_diagnostics::Event::editor_resize,
		render_diagnostics::Event::card_move,
		render_diagnostics::Event::card_children_batch,
		render_diagnostics::Event::text_paint,
	};
	static constexpr std::array paint_events{
		render_diagnostics::Event::editor_resize,
		render_diagnostics::Event::card_viewport_refresh,
		render_diagnostics::Event::card_mount,
		render_diagnostics::Event::card_create_window,
		render_diagnostics::Event::card_populate_rows,
		render_diagnostics::Event::card_place_window,
		render_diagnostics::Event::card_unmount,
		render_diagnostics::Event::text_paint,
		render_diagnostics::Event::button_paint,
		render_diagnostics::Event::caption_paint,
		render_diagnostics::Event::table_paint,
		render_diagnostics::Event::list_paint,
		render_diagnostics::Event::list_backbuffer,
		render_diagnostics::Event::static_text,
		render_diagnostics::Event::background,
		render_diagnostics::Event::mask_draw,
		render_diagnostics::Event::bitmap_draw_layout,
	};
	constexpr UINT_PTR paint_capture_start_timer_id = 49858;
	constexpr UINT_PTR paint_capture_end_timer_id = 49859;
	constexpr UINT paint_capture_delay_ms = 750;
	constexpr UINT paint_capture_duration_ms = 2000;
	void CALLBACK paint_capture_start_timer_proc(HWND wnd, UINT, UINT_PTR, DWORD);
	void CALLBACK paint_capture_end_timer_proc(HWND wnd, UINT, UINT_PTR, DWORD);

	edit_oneline::Theme static_text_theme() {
		auto hollow_brush = GetStockBrush(HOLLOW_BRUSH);
		return {
			.brushes = {
				.foreground = {.disabled = colors.ControlTxt},
				.bk = {.disabled = hollow_brush},
				.border = {.disabled = hollow_brush},
			},
			.font = fonts.General,
		};
	}

	template<size_t Count>
	HWND create_metrics_table(HWND parent, const std::array<render_diagnostics::Event, Count>& events, u32 row_height) {
		HWND metrics = create_window(parent, table::wndclass, nil, WS_CHILD);
		set_window_manager_parent(metrics, parent);
		table::set_functions(metrics, {
			.create_control = [](u32, HWND cell_parent, const void* text) -> HWND {
				HWND cell = create_window(cell_parent, edit_oneline::wndclass, (const cstr*)text, WS_VISIBLE | WS_CHILD | WS_DISABLED);
				edit_oneline::set_theme(cell, static_text_theme());
				return cell;
			},
		});
		table::set_column_cnt(metrics, 4);
		auto theme = themes.base_table;
		theme.brushes.border.normal = colors.ControlTxt_Disabled;
		theme.cells = {
			.row_h = UINumber{ .value = (f32)row_height },
			.columns_w = {
				UINumber{ .type = UINumber::Type::dyn }, UINumber{ .value = 50 },
				UINumber{ .value = 75 }, UINumber{ .value = 75 }
			}
		};
		table::set_theme(metrics, theme);
	#if RENDER_DIAGNOSTICS_ENABLED
		auto& table_state = *table::get_state(metrics);
		table::add_row(table_state, _t("Event"), _t("Calls"), _t("Total ms"), _t("Max ms"));
		for (auto event : events)
			table::add_row(table_state, render_diagnostics::event_name(event), _t(""), _t(""), _t(""));
	#endif
		return metrics;
	}

	HWND create_user_objects_table(HWND parent) {
		HWND stats = create_window(parent, table::wndclass, nil, WS_CHILD);
		set_window_manager_parent(stats, parent);
		table::set_functions(stats, {
			.create_control = [](u32, HWND cell_parent, const void* text) -> HWND {
				HWND cell = create_window(cell_parent, edit_oneline::wndclass, (const cstr*)text, WS_VISIBLE | WS_CHILD | WS_DISABLED);
				edit_oneline::set_theme(cell, static_text_theme());
				return cell;
			},
		});
		table::set_column_cnt(stats, 4);
		auto theme = themes.base_table;
		theme.brushes.border.normal = colors.ControlTxt_Disabled;
		theme.cells = {
			.row_h = UINumber{ .value = 22 },
			.columns_w = {
				UINumber{ .type = UINumber::Type::dyn }, UINumber{ .type = UINumber::Type::dyn },
				UINumber{ .type = UINumber::Type::dyn }, UINumber{ .type = UINumber::Type::dyn }
			}
		};
		table::set_theme(stats, theme);
		auto& table_state = *table::get_state(stats);
		table::add_row(table_state, _t("App"), _t("Debug"), _t("Total"), _t("Max USER Objs"));
		table::add_row(table_state, _t(""), _t(""), _t(""), _t(""));
		return stats;
	}

	#if RENDER_DIAGNOSTICS_ENABLED
	template<size_t Count>
	void update_metrics_table(HWND metrics, const std::array<render_diagnostics::Event, Count>& events,
		const render_diagnostics::Session& session) {
		const auto& rows = table::get_state(metrics)->rows;
		for (size_t index = 0; index < events.size(); ++index) {
			const auto& row = rows[index + 1];
			const auto& metric = session.metrics[(size_t)events[index]];
			auto calls = session.start ? std::format(_t("{}"), metric.calls) : str{};
			auto total = metric.ticks ? std::format(_t("{:.2f}"), (f64)metric.ticks / GetPCFrequency()) : str{};
			auto max_time = metric.ticks ? std::format(_t("{:.2f}"), (f64)metric.max_ticks / GetPCFrequency()) : str{};
			SetWindowText(row[1], calls.c_str());
			SetWindowText(row[2], total.c_str());
			SetWindowText(row[3], max_time.c_str());
		}
	}
	#endif

	DWORD user_object_limit() {
		HKEY key;
		auto status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			_t("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"), 0, KEY_QUERY_VALUE, &key);
		if (status != ERROR_SUCCESS) return 0;
		DWORD limit = 0, type = 0, size = sizeof(limit);
		status = RegQueryValueEx(key, _t("USERProcessHandleQuota"), nil, &type, (BYTE*)&limit, &size);
		RegCloseKey(key);
		if (status == ERROR_FILE_NOT_FOUND) return 10000;
		return status == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(limit) ? limit : 0;
	}

	struct Controls {
		HWND tabs;
		HWND color_buttons[ARRAYSIZE(colors.brushes)];
		HWND bitmap_selector;
		HWND size_selector;
		HWND bitmap_preview;
		HWND static_bitmap_info;
		HWND static_speed_wnd;
		HWND edit_speed_wnd;
		HWND static_cache_memory_wnd;
		HWND static_cache_hits_wnd;
		HWND user_objects_table;
		HWND static_focus_wnd;
		HWND static_resize_summary_wnd;
		HWND resize_metrics_table;
		HWND paint_capture_button;
		HWND static_paint_summary_wnd;
		HWND paint_metrics_table;
	};

	struct State : WindowState {
		HWND* track_wnd;

		HWND focus_wnd;
		rect_i32 raw_preview;
		size_t displayed_cache_entries;
		size_t displayed_cache_bytes;
		u64 displayed_cache_hits;
		u64 displayed_cache_misses;
		bool cache_stats_initialized;
		DWORD displayed_user_objects;
		DWORD displayed_debug_windows;
		bool user_objects_initialized;
		LONGLONG displayed_resize_start;
		bool resize_stats_initialized;
		bool paint_capture_pending;
		bool paint_stats_initialized;

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

		void update_cache_stats() {
			auto& cache = urender::mask_coverage_cache();
			if (!cache_stats_initialized || displayed_cache_entries != cache.entries.size() || displayed_cache_bytes != cache.used_bytes) {
				auto memory_text = std::format(_t("Mask cache: {} entries, {} / {} KiB"),
					cache.entries.size(), (cache.used_bytes + 1023) / 1024, cache.max_bytes / 1024);
				SetWindowText(controls.static_cache_memory_wnd, memory_text.c_str());
				displayed_cache_entries = cache.entries.size();
				displayed_cache_bytes = cache.used_bytes;
			}
			if (!cache_stats_initialized || displayed_cache_hits != cache.hits || displayed_cache_misses != cache.misses) {
				u64 total = cache.hits + cache.misses;
				f32 hit_rate = total ? 100.f * (f32)cache.hits / (f32)total : 0.f;
				auto hits_text = std::format(_t("Hits: {}  Misses: {}  ({:.1f}%)"), cache.hits, cache.misses, hit_rate);
				SetWindowText(controls.static_cache_hits_wnd, hits_text.c_str());
				displayed_cache_hits = cache.hits;
				displayed_cache_misses = cache.misses;
			}
			cache_stats_initialized = true;
		}

		void update_user_object_stats() {
			static const DWORD limit = user_object_limit();
			auto count = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
			DWORD debug_windows = 0;
			if (HWND root = GetAncestor(wnd, GA_ROOT); root) {
				debug_windows = 1;
				EnumChildWindows(root, [](HWND, LPARAM data) -> BOOL {
					++*(DWORD*)data;
					return TRUE;
				}, (LPARAM)&debug_windows);
			}
			if (user_objects_initialized && displayed_user_objects == count && displayed_debug_windows == debug_windows) return;
			auto app_estimate = count >= debug_windows ? count - debug_windows : 0;
			const auto& values = table::get_state(controls.user_objects_table)->rows[1];
			str app_text = count ? std::format(_t("~{}"), app_estimate) : _t("?");
			str debug_text = std::format(_t("~{}"), debug_windows);
			str total_text = count ? std::format(_t("{}"), count) : _t("?");
			str limit_text = limit ? std::format(_t("{}"), limit) : _t("?");
			SetWindowText(values[0], app_text.c_str());
			SetWindowText(values[1], debug_text.c_str());
			SetWindowText(values[2], total_text.c_str());
			SetWindowText(values[3], limit_text.c_str());
			displayed_user_objects = count;
			displayed_debug_windows = debug_windows;
			user_objects_initialized = true;
		}

		void update_resize_stats() {
		#if RENDER_DIAGNOSTICS_ENABLED
			auto& session = render_diagnostics::session();
			if (session.active || (resize_stats_initialized && displayed_resize_start == session.start)) return;
			if (!session.start) SetWindowText(controls.static_resize_summary_wnd, _t("No move/resize session recorded yet"));
			else {
				auto summary = std::format(_t("Last move/resize: {:.2f} ms"), (f64)session.wall_ticks / GetPCFrequency());
				SetWindowText(controls.static_resize_summary_wnd, summary.c_str());
			}
			update_metrics_table(controls.resize_metrics_table, resize_events, session);
			displayed_resize_start = session.start;
		#else
			if (resize_stats_initialized) return;
			SetWindowText(controls.static_resize_summary_wnd, _t("Render diagnostics are disabled"));
		#endif
			resize_stats_initialized = true;
		}

		void update_paint_stats() {
		#if RENDER_DIAGNOSTICS_ENABLED
			if (paint_capture_pending || render_diagnostics::paint_session().active || paint_stats_initialized) return;
			const auto& session = render_diagnostics::paint_session();
			if (!session.start) SetWindowText(controls.static_paint_summary_wnd, _t("Paint: click Capture, then interact with the app"));
			else {
				auto summary = std::format(_t("Paint capture: {:.2f} ms (inclusive timings)"),
					(f64)session.wall_ticks / GetPCFrequency());
				SetWindowText(controls.static_paint_summary_wnd, summary.c_str());
			}
			update_metrics_table(controls.paint_metrics_table, paint_events, session);
		#else
			if (paint_stats_initialized) return;
			SetWindowText(controls.static_paint_summary_wnd, _t("Render diagnostics are disabled"));
		#endif
			paint_stats_initialized = true;
		}

	#if RENDER_DIAGNOSTICS_ENABLED
		void request_paint_capture() {
			if (paint_capture_pending || render_diagnostics::paint_session().active) return;
			paint_capture_pending = true;
			EnableWindow(controls.paint_capture_button, false);
			if (!SetTimer(wnd, paint_capture_start_timer_id, paint_capture_delay_ms, paint_capture_start_timer_proc)) {
				paint_capture_pending = false;
				EnableWindow(controls.paint_capture_button, true);
				SetWindowText(controls.static_paint_summary_wnd, _t("Could not start paint capture"));
			}
			else SetWindowText(controls.static_paint_summary_wnd, _t("Capture started; interact with the app"));
		}

		void begin_paint_capture() {
			paint_capture_pending = false;
			render_diagnostics::begin_paint_session();
			if (!SetTimer(wnd, paint_capture_end_timer_id, paint_capture_duration_ms, paint_capture_end_timer_proc))
				finish_paint_capture();
		}

		void finish_paint_capture() {
			render_diagnostics::end_paint_session();
			paint_stats_initialized = false;
			update_paint_stats();
			EnableWindow(controls.paint_capture_button, true);
		}
	#endif

		void apply_tab_visibility() {
			bool controls_tab = tabs::get_selected(controls.tabs) == 0;
			if (!controls_tab) {
				combobox::show_listbox(*combobox::get_state(controls.bitmap_selector), false);
				combobox::show_listbox(*combobox::get_state(controls.size_selector), false);
			}
			for (HWND color_button : controls.color_buttons) ShowWindow(color_button, controls_tab ? SW_SHOW : SW_HIDE);
			for (HWND control : { controls.bitmap_selector, controls.size_selector, controls.bitmap_preview,
				controls.static_bitmap_info, controls.static_speed_wnd, controls.edit_speed_wnd,
				controls.static_cache_memory_wnd, controls.static_cache_hits_wnd,
				controls.user_objects_table, controls.static_focus_wnd })
				ShowWindow(control, controls_tab ? SW_SHOW : SW_HIDE);
			ShowWindow(controls.static_resize_summary_wnd, controls_tab ? SW_HIDE : SW_SHOW);
			ShowWindow(controls.resize_metrics_table, controls_tab ? SW_HIDE : SW_SHOW);
			ShowWindow(controls.paint_capture_button, controls_tab ? SW_HIDE : SW_SHOW);
			ShowWindow(controls.static_paint_summary_wnd, controls_tab ? SW_HIDE : SW_SHOW);
			ShowWindow(controls.paint_metrics_table, controls_tab ? SW_HIDE : SW_SHOW);
			if (controls_tab) { update_cache_stats(); update_user_object_stats(); }
			else { update_resize_stats(); update_paint_stats(); }
			ask_window_for_repaint(wnd);
		}
	
		void add_controls() {
			controls.tabs = create_window(wnd, tabs::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_TABSTOP);
			tabs::set_theme(controls.tabs, themes.base_tabs);
			tabs::add_tab(controls.tabs, L"Controls");
			tabs::add_tab(controls.tabs, L"Rendering");

			button::Theme t{};
			auto hollow_br = GetStockBrush(HOLLOW_BRUSH);
			for (auto& br : t.brushes.border.all) br = color_btn_border;
			for (auto& br : t.brushes.foreground.all) br = hollow_br;
			t.dimensions.border_thickness = { .type = UINumber::Type::px, .value = 1 };

			auto static_theme = static_text_theme();

			for (const auto& [i, c] : colors.brushes | std::views::enumerate) {
				auto& b = controls.color_buttons[i];
				b = create_window(wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD);
				for (auto& br : t.brushes.bk.all) br = c;
				button::set_theme(b, t);
				auto col = ColorFromBrush(c);
				auto msg = std::format(L"{}\nRGB({}, {}, {})", known_colors_names[i], GetRValue(col), GetGValue(col), GetBValue(col));
				add_mouseover_tooltip(b, str_view{ msg }, {.multiline = true, .delay_ms = 100});
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
			controls.static_cache_memory_wnd = create_window(wnd, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_DISABLED);
			controls.static_cache_hits_wnd = create_window(wnd, edit_oneline::wndclass, nil, WS_VISIBLE | WS_CHILD | WS_DISABLED);
			controls.user_objects_table = create_user_objects_table(wnd);
			edit_oneline::set_theme(controls.static_cache_memory_wnd, static_theme);
			edit_oneline::set_theme(controls.static_cache_hits_wnd, static_theme);

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
				i32 icon_x = urender::draw_mask_right(arrow, dc, r, pad, theme.brushes.foreground.normal);
				RECT text_bounds = r.to_RECT();
				text_bounds.left += pad;
				text_bounds.right = icon_x - pad;
				if (element) urender::draw_text(dc, text_bounds, to_utf_str(const_cast<cstr*>(((SelectorOption*)element)->name)),
					fonts.General, colors.ControlTxt, urender::TextAlignment::left, 0);
			};
			auto render_option = [](HDC dc, rect_i32 r, listbox::RenderFlags flags, void* element, void*, listbox::ElementPass pass) -> i32 {
				auto option = (SelectorOption*)element;
				const cstr* name = option->name;
				i32 x_pad = avg_str_dim(fonts.General, 1).cx;
				i32 icon_size = option->bitmap ? maximum(0, minimum(r.h - 2 * rdpi(4), rdpi(20))) : 0;
				i32 icon_gap = icon_size ? rdpi(6) : 0;
				if (pass == listbox::ElementPass::measure)
					return urender::measure_text(dc, fonts.General, str_view{ name }).cx + 2 * x_pad + icon_size + icon_gap;
				HBRUSH background = flags.on_clicked ? colors.ControlBkPush :
					flags.on_mouseover || flags.on_selected ? colors.ControlBkMouseOver :
					colors.ControlBk;
				RECT bounds = r.to_RECT();
				FillRect(dc, &bounds, background);
				if (icon_size) {
					HBITMAP bitmap = *option->bitmap;
					if (bitmap) {
						i32 icon_y = r.y + (r.h - icon_size) / 2;
						urender::draw_mask(dc, r.x + x_pad, icon_y, icon_size, icon_size, bitmap, colors.ControlTxt);
					}
				}
				bounds.left += icon_size + icon_gap;
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

			controls.static_resize_summary_wnd = create_window(wnd, edit_oneline::wndclass, nil, WS_CHILD | WS_DISABLED);
			edit_oneline::set_theme(controls.static_resize_summary_wnd, static_theme);
			controls.resize_metrics_table = create_metrics_table(wnd, resize_events, 25);
			controls.paint_capture_button = create_window(wnd, button::wndclass, _t("Capture painting (2 s)"), WS_CHILD);
			button::set_theme(controls.paint_capture_button, themes.base_btn);
		#if RENDER_DIAGNOSTICS_ENABLED
			button::set_user_data(controls.paint_capture_button, this);
			button::set_functions(controls.paint_capture_button, {
				.on_click = [](void* data, HWND) { ((State*)data)->request_paint_capture(); }
			});
		#else
			EnableWindow(controls.paint_capture_button, false);
		#endif
			controls.static_paint_summary_wnd = create_window(wnd, edit_oneline::wndclass, nil, WS_CHILD | WS_DISABLED);
			edit_oneline::set_theme(controls.static_paint_summary_wnd, static_theme);
			controls.paint_metrics_table = create_metrics_table(wnd, paint_events, 22);
			tabs::set_on_selection_changed(controls.tabs, [](void* user_data, HWND, size_t) {
				((State*)user_data)->apply_tab_visibility();
			}, this);
			apply_tab_visibility();
		}

		void resize_controls() {
			RECT r; GetClientRect(wnd, &r);
			auto w = RECTW(r), h = RECTH(r);
			auto min_dim = minimum(w, h);

			auto pad = DPI(8);

			rect_i32 bounds{}; bounds.wh = { w, h };
			rect_i32 tab_bar = bounds.cut_top(DPI(36));
			tab_bar.apply_padding(pad, 0);
			MoveWindow(controls.tabs, tab_bar);

			rect_i32 render_bounds = bounds;
			rect_i32 render_summary = render_bounds.cut_top(DPI(32));
			render_summary.apply_padding(pad, 0);
			MoveWindow(controls.static_resize_summary_wnd, render_summary);
			rect_i32 render_table = render_bounds.cut_top(DPI(25) * (i32)(resize_events.size() + 1));
			render_table.apply_padding(pad, 0);
			MoveWindow(controls.resize_metrics_table, render_table);
			rect_i32 capture_button = render_bounds.cut_top(DPI(32));
			capture_button.apply_padding(pad, 0);
			MoveWindow(controls.paint_capture_button, capture_button);
			rect_i32 paint_summary = render_bounds.cut_top(DPI(26));
			paint_summary.apply_padding(pad, 0);
			MoveWindow(controls.static_paint_summary_wnd, paint_summary);
			rect_i32 paint_table = render_bounds.cut_top(DPI(22) * (i32)(paint_events.size() + 1));
			paint_table.apply_padding(pad, 0);
			MoveWindow(controls.paint_metrics_table, paint_table);

			rect_i32 static_focus_wnd = bounds.cut_bottom(DPI(30));
			rect_i32 user_objects = bounds.cut_bottom(DPI(44));
			user_objects.apply_padding(pad, 0);
			rect_i32 cache_hits = bounds.cut_bottom(DPI(26));
			cache_hits.apply_padding(pad, 0);
			rect_i32 cache_memory = bounds.cut_bottom(DPI(26));
			cache_memory.apply_padding(pad, 0);
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

			i32 start_offset_x = pad, offset_x = start_offset_x, offset_y = bounds.top + pad;
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
			MoveWindow(controls.static_cache_memory_wnd, cache_memory);
			MoveWindow(controls.static_cache_hits_wnd, cache_hits);
			MoveWindow(controls.user_objects_table, user_objects);
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
					#if RENDER_DIAGNOSTICS_ENABLED
					if (render_diagnostics::paint_session().active) goto set_timer;
					#endif
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

		void track_resource_stats() {
			SetTimer(wnd, 49857, 250, [](HWND wnd, UINT, UINT_PTR, DWORD) {
				if (State* state = get_state(wnd)) {
				#if RENDER_DIAGNOSTICS_ENABLED
					if (render_diagnostics::paint_session().active) return;
				#endif
					if (tabs::get_selected(state->controls.tabs) == 0) {
						state->update_cache_stats();
						state->update_user_object_stats();
					}
					else state->update_resize_stats();
				}
			});
		}
	};

	void CALLBACK paint_capture_start_timer_proc(HWND wnd, UINT, UINT_PTR, DWORD) {
		KillTimer(wnd, paint_capture_start_timer_id);
	#if RENDER_DIAGNOSTICS_ENABLED
		if (auto state = State::get_state(wnd)) state->begin_paint_capture();
	#endif
	}

	void CALLBACK paint_capture_end_timer_proc(HWND wnd, UINT, UINT_PTR, DWORD) {
		KillTimer(wnd, paint_capture_end_timer_id);
	#if RENDER_DIAGNOSTICS_ENABLED
		if (auto state = State::get_state(wnd)) state->finish_paint_capture();
	#endif
	}

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
			state.track_resource_stats();
		} break;
		case WM_NCDESTROY:
		{
			KillTimer(wnd, paint_capture_start_timer_id);
			KillTimer(wnd, paint_capture_end_timer_id);
			render_diagnostics::end_paint_session();
			set_window_state(wnd, nil);
			free(&state);
			return DefWindowProc(wnd, msg, wparam, lparam);
		}
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
			if (tabs::get_selected(state.controls.tabs) != 0) return 0;
			auto bitmap_listbox = combobox::get_controls(state.controls.bitmap_selector).listbox;
			auto selected = (SelectorOption*)listbox::get_sel_element(bitmap_listbox);
			if (!selected) return 0;
			HBITMAP bitmap = *selected->bitmap;
			urender::draw_mask(dc, state.raw_preview.x, state.raw_preview.y, state.raw_preview.w, state.raw_preview.h, bitmap, colors.ControlTxt);
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
