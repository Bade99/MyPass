#pragma once
#include "win_sdk.h"
#include "helpers.h"

#ifdef _DEBUG
namespace debug {

	constexpr auto& wndclass = wndclass_name("debug");

	static auto color_btn_border = CreateSolidBrush(RGB(0,0,0));

	struct Controls {
		HWND color_buttons[ARRAYSIZE(colors.brushes)];
	};

	struct State : WindowState {
		HWND* track_wnd;

		Controls controls;

		static auto get_state(HWND wnd) { _control_create_function__get_state }
	
		void add_controls() {

			button::Theme t{};
			auto hollow_br = GetStockBrush(HOLLOW_BRUSH);
			for (auto& br : t.brushes.border.all) br = color_btn_border;
			for (auto& br : t.brushes.foreground.all) br = hollow_br;
			t.dimensions.border_thickness = 1;

			for (const auto& [i, c] : colors.brushes | std::views::enumerate) {
				auto& b = controls.color_buttons[i];
				b = create_window(wnd, button::wndclass, nil, WS_VISIBLE | WS_CHILD);
				for (auto& br : t.brushes.bk.all) br = c;
				button::set_theme(b, t);
				auto col = ColorFromBrush(c);
				auto msg = std::format(L"{}\nRGB({}, {}, {})", known_colors_names[i], GetRValue(col), GetGValue(col), GetBValue(col));
				add_mouseover_tooltip(b, (u64)(void*)msg.c_str(), {.multiline = true, .delay_ms = 100});
			}
		}

		void resize_controls() {
			RECT r; GetClientRect(wnd, &r);
			auto w = RECTW(r), h = RECTH(r);
			auto min_dim = minimum(w, h);

			auto pad = DPI(8);

			auto [rows, cols, dim] = pack_squares(w, h, ARRAYSIZE(controls.color_buttons), pad);

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
	};

	static LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
			SetWindowText(st->parent, L"Debug");
		} break;
		case WM_CREATE:
		{
			state.add_controls();
			state.follow_track_window();
		} break;
		case WM_SIZE:
		{
			LRESULT res = DefWindowProc(wnd, msg, wparam, lparam);
			state.resize_controls();
			return res;
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