#pragma once

namespace nonclient {

	constexpr auto& wndclass = wndclass_name("nonclient");

	struct LpParam {//NOTE: pass a pointer to LpParam to set up the client area, if client_class_name is null no client is created
		const TCHAR* client_class_name;
		void* client_lp_param;
		bool can_maximize = true;
	};

	struct MY_MARGINS
	{
		int cxLeftWidth;
		int cxRightWidth;
		int cyTopHeight;
		int cyBottomHeight;
	};

	struct State {
		HWND wnd;
		HWND client;
		//TODO(fran): might be good to store client and wnd rect, find the one place where we get this two. Then we wont have the code littered with those two calls

		MY_MARGINS nc;

		HWND btn_min, btn_max, btn_close, toast;

		RECT rc_caption;
		SIZE caption_btn;
		bool active;

		RECT rc_icon;

		LpParam* settings;

		struct {//menu related
			HMENU menu; //The menu that contains all the other menus, the ones that go in the menu bar, and the ones inside of each of those 
			//IMPLEMENTATION: every submenu asks this one for the bk brush
			RECT menubar_items[10];//coords are relative to wnd client
			i32 menubar_itemcnt;
			i32 menubar_mouseover_idx_from1;//IMPLEMENTATION: 0=no item is on mouseover, we'll start from 1 so in case you search by position you'll need to subtract 1
			bool menu_on_delay;//NOTE: the delay is only needed for left click accessed menus //TODO(fran): this is quite a bit of a cheap hack solution

			HDC menu_offscreen_dc;
			HBITMAP menu_backbuffer;
			SIZE menu_backbuffer_dim;
		};
	};
}