#pragma once

namespace password_editor {
	constexpr auto& wndclass = wndclass_name("password_editor");

	typedef void(*func_ondelete)(HWND pwd_ed, void* data);

	typedef void(*func_onchange)(void* data, HWND wnd);

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
			HWND btn_dates, tooltip_btn_dates, btn_pin;
			HWND tbl_values;
		};
		HWND all[9];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};
	#define _password_editor_tool_list(...) { controls.btn_dates, controls.btn_pin, controls.btn_edit, controls.btn_add, controls.btn_del, __VA_ARGS__ }

	union Functions {
		struct {
			func_ondelete on_delete;
		};
		void* all[1]{ 0 };
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	union StatefulFunctions {
		struct {
			StatefulFunction<func_onchange> on_change;
		};
		StatefulFunction<> all[1]{ 0 };
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	struct ItemFlag {
		using type = u32;
		static const type pin = 1 << 0; // Password Editor Element is always pinned to the top of the list
	};

	struct Properties {
		time_t date_created, date_modified;
		multiflag<ItemFlag> flags;
	};

	struct State : WindowState {
		Controls controls;
		Theme theme;
		Functions functions;
		StatefulFunctions stateful_functions;
		Properties properties;

		bool is_open;
		bool is_editing;

		void* user_data;
	};

	struct ValueCellFlag {
		using type = u32;
		static const type lock = 1 << 0; // field hidden by default
	};

	struct DescriptionCell {
		const utf16* text;
		StatefulFunction<func_onchange> on_change;
	};

	struct ValueCell {
		const utf16* text;
		multiflag<ValueCellFlag> flags;
		StatefulFunction<func_onchange> on_change;
	};

	static constexpr auto empty_description_cell = DescriptionCell{ 0 };
	static constexpr auto empty_value_cell = ValueCell{ 0 };
}