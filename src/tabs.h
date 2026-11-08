#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "renderer.h"

namespace tabs {
	constexpr auto underline_animation_timer_id = 1;
	constexpr f32 underline_animation_duration_ms = 150.f;

	auto get_state(HWND wnd) { _control_create_function__get_state }

	i32 measure_item_width(const State& state, HDC dc, const Item& item) {
		return maximum(DPI(56), urender::measure_text(dc, state.theme.font, item.label).cx + DPI(28));
	}

	void clamp_scroll(State& state) {
		RECT bounds; GetClientRect(state.wnd, &bounds);
		state.scroll_x = clamp(0, state.scroll_x, maximum(0, state.content_width - RECTW(bounds)));
	}

	void refresh_layout(State& state) {
		HDC dc = GetDC(state.wnd); if (!dc) return;
		defer{ ReleaseDC(state.wnd, dc); };
		state.content_width = 0;
		for (auto& item : state.items) {
			item.width = measure_item_width(state, dc, item);
			state.content_width += item.width + DPI(4);
		}
		if (!state.items.empty()) state.content_width -= DPI(4);
		clamp_scroll(state);
	}

	RECT item_bounds(const State& state, size_t index) {
		RECT client; GetClientRect(state.wnd, &client);
		i32 left = -state.scroll_x;
		for (size_t current = 0; current < index; ++current)
			left += state.items[current].width + DPI(4);
		return { left, client.top, left + state.items[index].width, client.bottom };
	}

	UnderlineBounds underline_bounds(const State& state, size_t index) {
		RECT bounds = item_bounds(state, index);
		return { bounds.left + state.scroll_x + rdpi(6), bounds.right + state.scroll_x - rdpi(6) };
	}

	void stop_underline_animation(State& state) {
		KillTimer(state.wnd, underline_animation_timer_id);
		state.underline.timeline.stop();
	}

	void snap_underline(State& state) {
		stop_underline_animation(state);
		state.underline.current = state.selected == no_selection ? UnderlineBounds{} : underline_bounds(state, state.selected);
	}

	void tick_underline_animation(State& state) {
		auto& transition = state.underline;
		if (!transition.timeline.active) return;
		f32 progress = transition.timeline.progress();
		f32 eased = animation::ease_in_out_smoothstep(progress);
		transition.current = {
			lerp(transition.from.left, eased, transition.to.left),
			lerp(transition.from.right, eased, transition.to.right)
		};
		if (progress >= 1.f) { transition.current = transition.to; stop_underline_animation(state); }
		ask_window_for_repaint(state.wnd);
	}

	void CALLBACK underline_animation_timer_proc(HWND wnd, UINT, UINT_PTR, DWORD) {
		if (auto state = get_state(wnd)) tick_underline_animation(*state);
	}

	void ensure_selected_visible(State& state) {
		clamp_scroll(state);
		if (state.selected == no_selection) return;
		RECT client; GetClientRect(state.wnd, &client);
		RECT selected = item_bounds(state, state.selected);
		if (selected.left < client.left) state.scroll_x += selected.left - client.left;
		else if (selected.right > client.right) state.scroll_x += selected.right - client.right;
		clamp_scroll(state);
	}

	void update_cursor(State& state);

	void set_theme(HWND wnd, const Theme& src) {
		State& state = *get_state(wnd); Assert(&state);
		if (state.theme.copy_from(src)) {
			refresh_layout(state);
			ensure_selected_visible(state);
			snap_underline(state);
			update_cursor(state);
			ask_window_for_repaint(wnd);
		}
	}

	bool set_selected(HWND wnd, size_t index) {
		State& state = *get_state(wnd); Assert(&state);
		if (index >= state.items.size() || index == state.selected) return false;
		if (state.underline.timeline.active) tick_underline_animation(state);
		bool had_selection = state.selected != no_selection;
		state.selected = index;
		ensure_selected_visible(state);
		auto& transition = state.underline;
		stop_underline_animation(state);
		transition.from = transition.current;
		transition.to = underline_bounds(state, index);
		if (had_selection && animation::enabled_by_system() && IsWindowVisible(wnd) &&
			(transition.from.left != transition.to.left || transition.from.right != transition.to.right)) {
			transition.timeline.start(underline_animation_duration_ms);
			if (SetTimer(wnd, underline_animation_timer_id, animation::timer_interval_ms(wnd), underline_animation_timer_proc))
				tick_underline_animation(state);
			else snap_underline(state);
		}
		else snap_underline(state);
		update_cursor(state);
		ask_window_for_repaint(wnd);
		if (state.on_selection_changed) state.on_selection_changed(state.user_data, wnd, index);
		return true;
	}

	size_t get_selected(HWND wnd) { return get_state(wnd)->selected; }

	void set_on_selection_changed(HWND wnd, OnSelectionChanged callback, void* user_data = nil) {
		State& state = *get_state(wnd); Assert(&state);
		state.on_selection_changed = callback;
		state.user_data = user_data;
	}

	size_t add_tab(HWND wnd, str_view label) {
		State& state = *get_state(wnd); Assert(&state);
		state.items.push_back({ str(label) });
		refresh_layout(state);
		if (state.selected != no_selection) snap_underline(state);
		update_cursor(state);
		size_t index = state.items.size() - 1;
		if (state.selected == no_selection) set_selected(wnd, index);
		else ask_window_for_repaint(wnd);
		return index;
	}

	bool rename_tab(HWND wnd, size_t index, str_view label) {
		State& state = *get_state(wnd); Assert(&state);
		if (index >= state.items.size()) return false;
		state.items[index].label = label;
		refresh_layout(state);
		ensure_selected_visible(state);
		snap_underline(state);
		update_cursor(state);
		ask_window_for_repaint(wnd);
		return true;
	}

	bool remove_tab(HWND wnd, size_t index) {
		State& state = *get_state(wnd); Assert(&state);
		if (index >= state.items.size()) return false;
		bool removed_selected = state.selected == index;
		state.items.erase(state.items.begin() + index);
		refresh_layout(state);
		if (state.hovered == index) state.hovered = no_selection;
		else if (state.hovered != no_selection && state.hovered > index) --state.hovered;
		if (state.items.empty()) state.selected = no_selection;
		else if (state.selected > index) --state.selected;
		else if (state.selected == index) state.selected = minimum(index, state.items.size() - 1);
		ensure_selected_visible(state);
		snap_underline(state);
		update_cursor(state);
		ask_window_for_repaint(wnd);
		if (removed_selected && state.on_selection_changed) state.on_selection_changed(state.user_data, wnd, state.selected);
		return true;
	}

	size_t hit_test(State& state, POINT point) {
		i32 left = -state.scroll_x;
		RECT client; GetClientRect(state.wnd, &client);
		if (point.y < client.top || point.y >= client.bottom) return no_selection;
		for (size_t index = 0; index < state.items.size(); ++index) {
			i32 width = state.items[index].width;
			if (point.x >= left && point.x < left + width) return index;
			left += width + DPI(4);
		}
		return no_selection;
	}

	void update_cursor(State& state) {
		static const HCURSOR arrow_cursor = LoadCursor(nil, IDC_ARROW);
		POINT point{};
		if (!GetCursorPos(&point) || WindowFromPoint(point) != state.wnd) return;
		ScreenToClient(state.wnd, &point);
		size_t index = hit_test(state, point);
		HCURSOR cursor = index != no_selection && index != state.selected ? state.theme.cursor : nil;
		SetCursor(cursor ? cursor : arrow_cursor);
	}

	LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		State* state = get_state(wnd);
		switch (msg) {
		case WM_NCCREATE: {
			auto* created = new State{};
			created->wnd = wnd;
			created->parent = ((CREATESTRUCT*)lparam)->hwndParent;
			set_window_state(wnd, created);
			return TRUE;
		}
		case WM_NCDESTROY:
			stop_underline_animation(*state);
			set_window_state(wnd, nil);
			delete state;
			return DefWindowProc(wnd, msg, wparam, lparam);
		case WM_SIZE:
			refresh_layout(*state);
			ensure_selected_visible(*state);
			ask_window_for_repaint(wnd);
			return DefWindowProc(wnd, msg, wparam, lparam);
		case WM_ERASEBKGND: return 1;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC dc = BeginPaint(wnd, &paint); defer{ EndPaint(wnd, &paint); };
			RECT client; GetClientRect(wnd, &client);
			if (state->theme.brushes.bk.normal) FillRect(dc, &client, state->theme.brushes.bk.normal);
			i32 left = -state->scroll_x;
			for (size_t index = 0; index < state->items.size(); ++index) {
				i32 width = state->items[index].width;
				RECT bounds{ left, 0, left + width, client.bottom };
				bool selected = index == state->selected;
				HBRUSH background = !selected && index == state->hovered ?
					state->theme.brushes.bk.mouseover : state->theme.brushes.bk.normal;
				if (bounds.right > client.left && bounds.left < client.right) {
					if (background) urender::draw_round_rectangle(dc, bounds, (f32)DPI(6), background);
					urender::draw_text(dc, bounds, to_utf_str(state->items[index].label), state->theme.font,
						state->theme.brushes.foreground.normal, urender::TextAlignment::center, 0);
				}
				left += width + DPI(4);
			}
			if (state->selected != no_selection && state->theme.brushes.accent.normal) {
				RECT underline{
					state->underline.current.left - state->scroll_x, client.bottom - rdpi(3),
					state->underline.current.right - state->scroll_x, client.bottom
				};
				if (underline.right > underline.left) FillRect(dc, &underline, state->theme.brushes.accent.normal);
			}
			return 0;
		}
		case WM_LBUTTONDOWN: {
			SetFocus(wnd);
			POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			size_t index = hit_test(*state, point);
			if (index != no_selection) set_selected(wnd, index);
			return 0;
		}
		case WM_SETCURSOR:
			if (LOWORD(lparam) == HTCLIENT) {
				update_cursor(*state);
				return TRUE;
			}
			return DefWindowProc(wnd, msg, wparam, lparam);
		case WM_MOUSEMOVE: {
			if (!state->tracking_mouse) {
				TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, wnd, 0 };
				state->tracking_mouse = TrackMouseEvent(&tracking) != 0;
			}
			POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			size_t hovered = hit_test(*state, point);
			if (hovered != state->hovered) {
				state->hovered = hovered;
				update_cursor(*state);
				ask_window_for_repaint(wnd);
			}
			return 0;
		}
		case WM_MOUSELEAVE:
			state->tracking_mouse = false;
			state->hovered = no_selection;
			ask_window_for_repaint(wnd);
			return 0;
		case WM_MOUSEWHEEL: {
			state->scroll_x -= GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA * DPI(80);
			clamp_scroll(*state);
			update_cursor(*state);
			ask_window_for_repaint(wnd);
			return 0;
		}
		case WM_GETDLGCODE: return DLGC_WANTARROWS;
		case WM_KEYDOWN:
			if (state->selected != no_selection) {
				size_t index = state->selected;
				if (wparam == VK_LEFT && index > 0) --index;
				else if (wparam == VK_RIGHT && index + 1 < state->items.size()) ++index;
				else if (wparam == VK_HOME) index = 0;
				else if (wparam == VK_END) index = state->items.size() - 1;
				else return DefWindowProc(wnd, msg, wparam, lparam);
				set_selected(wnd, index);
				return 0;
			}
			break;
		case WM_GETFONT: return (LRESULT)state->theme.font;
		case WM_SETFONT:
			state->theme.font = (HFONT)wparam;
			refresh_layout(*state);
			ensure_selected_visible(*state);
			snap_underline(*state);
			if (lparam) ask_window_for_repaint(wnd);
			return 0;
		}
		return DefWindowProc(wnd, msg, wparam, lparam);
	}

	_init_wndclass_at_startup;
}
