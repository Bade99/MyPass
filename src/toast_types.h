#pragma once

namespace toast {
	constexpr auto& wndclass = wndclass_name("toast");

	constexpr auto wndpos_animation_timer_id = 0xf1;

	constexpr auto wndpos_animation_duration_ms = 3000;

	union Controls {
		struct {
			HWND btn_toast;
		};
		HWND all[1];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	union Theme {
		struct {
			button::Theme success, failure;
		};
		button::Theme all[2];

		bool copy_from(const Theme& src) {
			bool repaint = false;
			for(auto i = 0; i < ARRAYSIZE(this->all); i++) _theme_copy_theme(src.all[i], this->all[i]);
			return repaint;
		}

		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	struct Animations {
		struct AnimWndPos {
			f32 t_ms;
			f32 duration_ms;
			u32 desired_dt_ms; //framerate at which the animation should run
			i64 counter;
		} wnd_pos;
	};

	enum class type { success, failure };

	struct QueueItem { str msg; toast::type type; };

	struct State : WindowState {
		Controls controls;
		Theme theme;
		Animations animations;

		std::queue<QueueItem> notification_queue;

		bool is_showing;

		void init() { notification_queue = decltype(notification_queue)(); }

		void uninit() { notification_queue.~queue(); }
	};

	/**
	  * API
	  */
	auto get_state(HWND wnd);
	void set_theme(HWND wnd, const Theme& src);
}