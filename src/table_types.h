#pragma once

namespace table {
	constexpr auto& wndclass = wndclass_name("table");

	constexpr u32 MAX_COLUMNS = 5;

	typedef HWND(*func_createcontrol)(u32 column_idx, HWND parent, const void* data);

	struct RowTheme {
		/**
		  * We dont allow for variable row heights
		  * Only pixel and DPI-scaled heights are supported
		  */
		UINumber row_h;
		std::array<UINumber, MAX_COLUMNS> columns_w; //TODO: im not even benefitting from using std::array, its actually screwing me up by having removed the default constructor for Theme. can I just instead use: ColumnTheme columns_w[MAX_COLUMNS]; ?
		u32 get_row_h_in_pixels() const { return (u32)row_h.to_px(); }
	};

	struct Theme {
		union {
			struct {
				BrushGroup bk, border;//NOTE: the page itself renders nothing but the background
			};
			BrushGroup all[2];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		union Dimensions {
			struct { UINumber border_thickness; };
			UINumber all[1];
			Dimensions() : all{} { for (auto& dimension : all) dimension.value = F32INFINITY; }
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} dimensions;
		RowTheme cells;

		auto copy_from(const Theme& src, bool copy_cell_theme = true) {
			struct CopyResult { bool repaint = false, resize = false; } res;
			for (u32 i = 0; auto& b : this->brushes.all)
				res.repaint |= b.copy_from(src.brushes.all[i++]);

			bool dimensions_changed = _theme_copy_all_dimensions(src.dimensions, this->dimensions);
			res.repaint |= dimensions_changed;
			res.resize |= dimensions_changed;

			if (copy_cell_theme) {
				res.repaint = true;
				res.resize = true;
				this->cells = src.cells;
			}
			return res;
		}
	};

	union Functions {
		struct {
			func_createcontrol create_control;
		};
		void* all[1]{ 0 };
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	struct State : WindowState {
		Theme theme;
		Functions functions;
		std::vector<std::array<HWND, MAX_COLUMNS>> rows;
		u32 column_cnt;
		bool suppress_manager_notifications;

		void init() {
			rows = decltype(rows)();
		}

		void uninit() {
			rows.~vector();
		}
	};
}
