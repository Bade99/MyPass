#pragma once

namespace password_editor {
	constexpr auto& wndclass = wndclass_name("password_editor");

	typedef void(*func_ondelete)(HWND pwd_ed, void* data);

	struct Theme {
		union {
			struct {
				brush_group bk, border;
			};
			brush_group all[2];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		struct {
			u32 border_thickness = U32MAX;
		}dimensions;
	};

	union Controls {
		struct {
			HWND btn_card;
			HWND edo_title;
			HWND btn_edit, btn_add, btn_del;
			HWND btn_dates, tooltip_btn_dates;
			HWND tbl_values;
		};
		HWND all[8];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	union Functions {
		struct {
			func_ondelete on_delete;
		};
		void* all[1]{ 0 };
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	struct Properties {
		time_t date_created, date_modified;
	};

	struct State : WindowState {
		Controls controls;
		Theme theme;
		Functions functions;
		Properties properties;

		bool is_open;
		bool is_editing;

		void* user_data;
	};
}