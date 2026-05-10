#pragma once

namespace scrollbar {
	constexpr auto& wndclass = wndclass_name("scrollbar");

	/**
	  * Window Messages
	  */

	  // Auto resize and reposition
	constexpr auto U_SB_AUTORESIZE = (WM_USER + 300);

	// Sets scrollbar::Placement and auto repositions and resizes
	// wParam=scrollbar::Placement
	constexpr auto U_SB_SET_PLACEMENT = (WM_USER + 301);

	// Sets max scroll range (min is always 0)
	// wParam=int
	constexpr auto U_SB_SET_RANGEMAX = (WM_USER + 302);

	// Sets max lines visible at the same time on the parent control 
	// wParam=int
	constexpr auto U_SB_SET_PAGESZ = (WM_USER + 303);

	// Sets current position in the range
	// wParam=int
	constexpr auto U_SB_SET_POS = (WM_USER + 304);


	constexpr u32 timer_id_bk_click_held = 1;

	enum class Placement { left, right, top, bottom }; //NOTE: left and right should only be used for vertical scrollbars, top and bottom for horizontal

	struct State : WindowState {
		Placement placement;
		i32 range_min;
		i32 range_max;
		i32 page_sz;
		i32 p;

		i32 mouseStartDeltaP;

		bool onMouseOverSb;//The mouse is over our bar
		bool onLMouseClickBk;//The left click is being pressed on the background area
		bool OnMouseTrackingSb; //Left click was pressed in our bar and now is still being held, the user will probably be moving the mouse around, so we want to track it to move the scrollbar
		//NOTE: we'll probably also need a onMouseOverBk or just general mouseOver
	};
}