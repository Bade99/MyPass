#pragma once

namespace table {
	constexpr auto& wndclass = wndclass_name("table");

	constexpr auto MAX_COLUMNS = 5;

	typedef HWND(*func_createcontrol)(u32 column_idx, HWND parent, const void* data);

	struct RowTheme {
		/**
		  * We dont allow for variable row heights
		  * Calculated as: final_h = DPI(row_h)
		  */
		u32 row_h;
		std::array<UINumber, MAX_COLUMNS> columns_w; //TODO: im not even benefitting from using std::array, its actually screwing me up by having removed the default constructor for Theme. can I just instead use: ColumnTheme columns_w[MAX_COLUMNS]; ?
		u32 get_row_h_in_pixels() const { return DPI(row_h); }
	};

	struct Theme {
		union {
			struct {
				brush_group bk, border;//NOTE: the page itself renders nothing but the background
			};
			brush_group all[2];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		struct {
			u32 border_thickness = U32MAX;
		} dimensions;
		RowTheme cells;

		struct copy_res { bool repaint = false, resize = false; } copy_from(const Theme& src, bool copy_cell_theme = true) {
			copy_res res;
			for (u32 i = 0; auto& b : this->brushes.all)
				res.repaint |= b.copy_from(src.brushes.all[i++]);

			if (src.dimensions.border_thickness != U32MAX) {
				this->dimensions.border_thickness = src.dimensions.border_thickness;
				res.repaint = true;
				res.resize = true;
			}

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

		void init() {
			rows = decltype(rows)();
		}

		void uninit() {
			rows.~vector();
		}
	};
}
