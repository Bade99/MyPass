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
namespace toast {
	void create_controls(State& state) {
		auto& controls = state.controls;
		controls.btn_toast = create_window(state.wnd, button::wndclass, nil, WS_CHILD | WS_VISIBLE | WS_DISABLED);
		//for (const auto& c : controls.all) SetWindowFont(c, fonts.GeneralBold, true);
	}

	void resize_controls(const State& state) {
		auto& controls = state.controls;
		RECT wnd_r; GetClientRect(state.wnd, &wnd_r);
		move_window(controls.btn_toast, wnd_r);
	}

	auto get_state(HWND wnd) { _control_create_function__get_state }

	void show_next_notification(State& state); //Forward declaration to be able to check the queue again for more notifications after the current notification animation has completed

	f32 anim_rise_and_fall(f32 t, f32 low, f32 high, f32 transition_duration, f32 total_duration) {
		Assert(total_duration > transition_duration * 2);
		const f32 a = transition_duration / total_duration;
		const f32 b = (total_duration - transition_duration) / total_duration;

		if (t < a) return lerp(low, smoothstep(t / a), high);
		elif (t <= b) return high;
		else return lerp(high, smoothstep((t - b) / (1.f - b)), low);
	}

	void animate_wndpos(State& state) {
		auto& animation = state.animations.wnd_pos;
		state.is_showing = true;

		animation.t_ms = 0;
		animation.duration_ms = wndpos_animation_duration_ms;
		animation.desired_dt_ms = ceilf(1000.f / refresh_rate_hz(state.wnd));
		animation.counter = StartCounter();

		static void (*anim_proc)(HWND, UINT, UINT_PTR, DWORD) =
			[](HWND wnd, UINT, UINT_PTR anim_id, DWORD) {
			auto& state = *get_state(wnd);
			if (&state) {
				auto& animation = state.animations.wnd_pos;
				bool first_time = !animation.t_ms;

				animation.t_ms = EndCounter(animation.counter);

				RECT bounds{}; GetClientRect(state.parent, &bounds);
				auto bounds_w = RECTW(bounds);
				SIZE toast{}; Button_GetIdealSize(state.controls.btn_toast, &toast);
				auto max_toast_w = bounds_w * .75f;
				if (toast.cx > max_toast_w) {
					// Ask button to recalculate height based on width
					toast.cx = max_toast_w;
					Button_GetIdealSize(state.controls.btn_toast, &toast);
				}
				auto pad = DPI(8);
				
				//TODO: im sure there's some completely mathematical way to describe this parabola type animation
				const auto time_fully_visible = animation.duration_ms / 2;

				auto t = clamp01(EndCounter(animation.counter) / animation.duration_ms);

				constexpr auto transition_duration_ms = 200;
				auto y = anim_rise_and_fall(t, bounds.bottom, bounds.bottom - toast.cy - pad, transition_duration_ms, animation.duration_ms);

				MoveWindow(state.wnd, (bounds_w - toast.cx) / 2, y, toast.cx, toast.cy, true);
				
				if (first_time) {
					BringWindowToTop(state.wnd);
					ShowWindow(state.wnd, SW_SHOW);
					ask_window_for_repaint(state.controls.btn_toast);
				}

				if (animation.t_ms >= animation.duration_ms) goto end;
				else SetTimer(wnd, anim_id, animation.desired_dt_ms, anim_proc);
			}
			else {
				end:
				KillTimer(wnd, anim_id);
				ShowWindow(wnd, SW_HIDE);
				if (&state) {
					state.is_showing = false;
					show_next_notification(state); //check if there are more notifications left in the queue
				}
			}
		};

		SetTimer(state.wnd, wndpos_animation_timer_id, animation.desired_dt_ms, anim_proc);
	}

	void show_next_notification(State& state) {
		if (!state.is_showing && state.notification_queue.size()) {
			auto& notification = state.notification_queue.front();
			SetWindowText(state.controls.btn_toast, notification.msg.c_str());
			auto& theme = notification.type == type::success ? state.theme.success : state.theme.failure;
			button::set_theme(state.controls.btn_toast, theme);
			state.notification_queue.pop();
			animate_wndpos(state);
		}
	}

	LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto& state = *get_state(wnd); _control_validate_state;
		switch (msg) {
		case WM_NCCREATE:
		{
			CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

			Assert(creation_nfo->style & WS_CHILD);

			State* st = (State*)calloc(1, sizeof(State));
			Assert(st);
			st->wnd = wnd;
			st->parent = GetParent(wnd);
			st->init();

			set_window_state(wnd, st);
		} break;
		case WM_NCDESTROY:
		{
			state.uninit();
			set_window_state(state.wnd, nil);
			free(&state);
		} break;
		case WM_CREATE:
		{
			create_controls(state);
		} break;
		case WM_SIZE:
		{
			resize_controls(state);
		} break;
		}
		return DefWindowProc(wnd, msg, wparam, lparam);
	}

	_init_wndclass_at_startup;

	void set_theme(HWND wnd, const Theme& src) { _control_create_function__set_theme }

	void show(HWND child_of_someone, str msg, type type) {
		auto& nc = *nonclient::get_state(GetAncestor(child_of_someone, GA_ROOT));
		auto& state = *get_state(nc.toast);
		state.notification_queue.emplace(msg, type);
		show_next_notification(state);
	}
}
