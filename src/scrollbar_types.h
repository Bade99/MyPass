#pragma once

namespace scrollbar {
	constexpr auto& wndclass = wndclass_name("scrollbar");

	/**
	  * Window Messages
	  */
	namespace custom_message {
		enum custom_message {
			// Auto resize and reposition
			AUTORESIZE = (WM_USER + 300),

			// Sets scrollbar::Placement and auto repositions and resizes
			// wParam=scrollbar::Placement
			SET_PLACEMENT,

			// Sets max scroll range (min is always 0)
			// wParam=int
			SET_RANGEMAX,

			// Sets max lines visible at the same time on the parent control 
			// wParam=int
			SET_PAGESZ,

			// Sets current position in the range
			// wParam=int
			SET_POS,

			// Sets whether the scrollbar auto shows and hides itself when needed
			// wParam=bool
			SET_AUTOHIDE,
		};
	};

	constexpr u32 timer_id_bk_click_held = 1;

	enum class Placement { left, right, top, bottom }; //NOTE: left and right should only be used for vertical scrollbars, top and bottom for horizontal

	struct Theme {
		union {
			struct {
				brush_group bar_bk, bar_border, bk, border;
			};
			brush_group all[4];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		struct {
			u32 border_thickness = U32MAX;
			UINumber border_radius{ .value = F32INFINITY };
		}dimensions;

		bool copy_from(const Theme& src) {
			bool repaint = false;
			_theme_copy_all_brushes(src.brushes, this->brushes);

			_theme_copy_u32(src.dimensions.border_thickness, this->dimensions.border_thickness);
			_theme_copy_uinumber(src.dimensions.border_radius, this->dimensions.border_radius);

			return repaint;
		}
	};

	union Controls {
		struct {
			HWND btn_up;
			HWND btn_down;
		};
		HWND all[2];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	struct State : WindowState {
		Controls controls;
		Placement placement;
		i32 range_min;
		i32 range_max; //1-based, NOT 0
		i32 page_sz;
		i32 p;

		i32 mouseStartDeltaP;

		bool onMouseOverSb;//The mouse is over our bar
		bool onMouseOverControl;//The mouse is over our control
		bool onLMouseClickBk;//The left click is being pressed on the background area
		bool onMouseTrackingSb; //Left click was pressed in our bar and now is still being held, the user will probably be moving the mouse around, so we want to track it to move the scrollbar
		//NOTE: we'll probably also need a onMouseOverBk or just general mouseOver

		bool autohide;

		Theme theme;
	};
}