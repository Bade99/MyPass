#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "math.h"
#include "global.h"
#include "renderer.h"
#include "button.h"
#include "resource.h" //TODO(fran): everything that we take from the resources must be parameterized
#include "serialization.h"
#include "reflection.h"

// ------------------------ USAGE NOTES ------------------------ //
// - If you need menus to resize, for example when you change languages
//   you'll need to re-create the menu and set it up like new, I havent
//   yet found a way to get windows' menu handler to want to recalculate
//   menu sizes
// - To be able to paint the menus ourselves we need them to be MF_OWNERDRAW,
//   each menu item that has MF_OWNERDRAW must store the HMENU of its 
//   parent into their itemData, hopefully this wont be needed some day
// - Sends WM_CLOSE to the client to consult about closing the wnd
//   a non-zero return value means the client handles it, if 0 
//	 then we destroy the wnd
// ------------------------------------------------------------- //

// ------------------------ INTERNAL MSGS ------------------------ //
constexpr auto NC_MINIMIZE = 100; //sent through WM_COMMAND
constexpr auto NC_MAXIMIZE = 101; //sent through WM_COMMAND
constexpr auto NC_CLOSE = 102; //sent through WM_COMMAND
constexpr auto NC_RESTORE = 103; //sent through WM_COMMAND

//TODO(fran): look at SetWindowLong(hWnd, GWL_STYLE, currStyles | WS_MAXIMIZE); maybe that helps with maximizing correctly?

namespace nonclient {

constexpr auto top_border_thickness = 1;

constexpr bool menu_bottom_border = false;
constexpr auto menu_bottom_border_thickness = 1;

auto get_state(HWND wnd) { _control_create_function__get_state }

void calc_caption(State& state) {
	GetClientRect(state.wnd, &state.rc_caption);

	RECT testrc{ 0,0,100,100 }; RECT ncrc = testrc;
	AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE);//no menu

	state.rc_caption.bottom = distance(testrc.top, ncrc.top);

	state.caption_btn.cy = RECTH(state.rc_caption); state.caption_btn.cx = state.caption_btn.cy * 16 / 9;
}

RECT calc_nonclient_rc_from_client(RECT client_rc, BOOL has_menu) {
	RECT res = client_rc;
	AdjustWindowRect(&res, WS_OVERLAPPEDWINDOW, has_menu);//TODO(fran): standardize this or some other measure
	return res;
}

RECT calc_menu_rc(State& state) {
	RECT rc{ 0 };
	if (state.menu) {
		RECT r; GetClientRect(state.wnd, &r);
		rc.left = r.left;
		rc.top = RECTH(state.rc_caption);
		rc.right = r.right;
		rc.bottom = rc.top + GetSystemMetrics(SM_CYMENU);
	}
	return rc;
}

RECT calc_client_rc(State& state) {
	RECT r; GetClientRect(state.wnd, &r);
	RECT menu_rc = calc_menu_rc(state);
	RECT rc{ r.left, RECTH(state.rc_caption) + RECTH(menu_rc), r.right, r.bottom };
	return rc;
}

RECT calc_btn_min_rc(State& state) {
	RECT r; GetClientRect(state.wnd, &r);
	RECT rc{ r.right - 3 * state.caption_btn.cx, r.top, r.right - 2 * state.caption_btn.cx, r.top + state.caption_btn.cy };
	rc.top += top_border_thickness;
	return rc;
}

RECT calc_btn_max_rc(State& state) {
	RECT r; GetClientRect(state.wnd, &r);
	RECT rc{ r.right - 2 * state.caption_btn.cx, r.top, r.right - 1 * state.caption_btn.cx, r.top + state.caption_btn.cy };
	rc.top += top_border_thickness;
	return rc;
}

RECT calc_btn_close_rc(State& state) {
	RECT r; GetClientRect(state.wnd, &r);
	RECT rc{ r.right - 1 * state.caption_btn.cx, r.top, r.right - 0 * state.caption_btn.cx, r.top + state.caption_btn.cy };
	rc.top += top_border_thickness;
	return rc;
}

HMENU get_menu(HWND wnd) {
	State& state = *get_state(wnd);
	return state.menu;
}

void set_menu(HWND uncapnc, HMENU menu) {
	State& state = *get_state(uncapnc);

	if (menu) {
		state.menu = menu;

		MENUINFO mi{ sizeof(mi) };
		mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS; //NOTE: important to also use "apply to submenus", cause the nc area is bigger than 1px and it will look very claustrophobic
		//TODO(fran): MIM_MAXHEIGHT and other fMask params that we should set
		mi.hbrBack = state.active ? colors.CaptionBk : colors.CaptionBk_Inactive;
		SetMenuInfo(menu, &mi);
	}
	else {
		//TODO(fran): set to NULL should clear the menu, implement menu removal in case we already have something on state.menu
		state.menu = NULL;
	}
	RECT wndrc; GetWindowRect(state.wnd, &wndrc);
	//Update to accommodate for menu change
	MoveWindow(state.wnd, wndrc.left, wndrc.top, RECTW(wndrc), RECTH(wndrc), FALSE);//NOTE: for some reason specifying TRUE for the last param doesnt force repainting
	ask_window_for_repaint(state.wnd);
}

//NOTE: this function request for a UINT since the top 32 bits of an HMENU are not used, if you have an HMENU simply cast down (UINT)hmenu
bool is_on_menu_bar(State& state, UINT menuitem) {
	int cnt = GetMenuItemCount(state.menu);
	bool res = false;
	for (int i = 0; i < cnt; i++) {
		HMENU submenu = GetSubMenu(state.menu, i);

		if (menuitem == (UINT)(UINT_PTR)submenu || menuitem == (UINT)(UINT_PTR)state.menu) {
			res = true;
			break;
		}
	}
	return res;
}

//Mouse in screen coords
void show_rclickmenu(State& state, POINT mouse) {
	HMENU m = CreateMenu();
	HMENU subm = CreateMenu();//IMPORTANT: you need a MF_POPUP submenu for the menu wnd to be rendered properly, thanks https://www.codeproject.com/Questions/334598/Popup-Menu-Problem-is-not-working-properly
	AppendMenuW(m, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)subm, (LPCWSTR)m);

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, NC_RESTORE, (LPCWSTR)subm);
	SetMenuItemString(subm, NC_RESTORE, FALSE, RCS(LANG_NC_RESTORE));

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, NC_MINIMIZE, (LPCWSTR)subm);
	SetMenuItemString(subm, NC_MINIMIZE, FALSE, RCS(LANG_NC_MINIMIZE));

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, NC_MAXIMIZE, (LPCWSTR)subm);
	SetMenuItemString(subm, NC_MAXIMIZE, FALSE, RCS(LANG_NC_MAXIMIZE));

	AppendMenuW(subm, MF_SEPARATOR | MF_OWNERDRAW, 0, (LPCWSTR)subm);

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, NC_CLOSE, (LPCWSTR)subm);
	SetMenuItemString(subm, NC_CLOSE, FALSE, RCS(LANG_NC_CLOSE));

	MENUINFO mi{ sizeof(mi) };
	mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
	mi.hbrBack = colors.CaptionBk;
	SetMenuInfo(m, &mi);

	//NOTE: using tpm_returncmd would be a quick and simple cheat to get past msg collision problems and the like
	//TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, mouse.x, mouse.y, 0, state.wnd, 0);
	TrackPopupMenuEx(subm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, mouse.x, mouse.y, state.wnd, 0);
	DestroyMenu(m);
}

void manual_maximize(State& state) {//Maximize only works correctly for WM_OVERLAPPEDWINDOW, we gotta do it by hand
	WINDOWPLACEMENT old_placement; GetWindowPlacement(state.wnd, &old_placement);
	HMONITOR mon = MonitorFromWindow(state.wnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monnfo{ sizeof(monnfo) };
	BOOL res = GetMonitorInfo(mon, &monnfo);
	Assert(res);
	RECT work_rc = monnfo.rcWork;//NOTE: im in a 1 monitor setup and it's exactly the same as the other rc so idk when it's different. Strangely enough it still doesnt go over the taskbar area, so I guess there's a check inside the resizing code done by windows somewhere

	//TODO(fran):we're gonna have to store our current size to be able to manually restore if the user tries to move us or presses maximize again. SetWindowPlacement?
	//TODO(fran): gotta decide what we want to do with the resizing borders, the top and bottom ones are still user reachable, both visual studio and chrome dont allow resizing but chrome developers where a little lazy and didnt update the margins of the window, therefore on the bottom the mouse still changes to the resizing one even though they dont allow the resize
	work_rc.left -= state.nc.cxLeftWidth;
	work_rc.right += state.nc.cxRightWidth;
	work_rc.top -= state.nc.cyTopHeight;
	work_rc.bottom += state.nc.cyBottomHeight;
	MoveWindow(state.wnd, work_rc.left, work_rc.top, RECTW(work_rc), RECTH(work_rc), TRUE);
	WINDOWPLACEMENT placement{ sizeof(placement) };
	placement.showCmd = SW_MAXIMIZE;
	placement.flags = WPF_ASYNCWINDOWPLACEMENT;
	placement.ptMaxPosition.x = work_rc.left;
	placement.ptMaxPosition.y = work_rc.top;
	placement.rcNormalPosition = old_placement.rcNormalPosition;//TODO(fran): for non WS_EX_TOOLWINDOW windows all this should be in workspace coords
	SetWindowPlacement(state.wnd, &placement);//TODO(fran): I just wanted to set the placement, not ask this guy to update my window too (maybe I can do some extra resizing to adjust the situation)
}

void resize_menu_backbuffer(State& state) {
	if (state.menu) {
		RECT rc = calc_menu_rc(state);
		SIZE new_dim{ RECTW(rc), RECTH(rc) };
		if (new_dim != state.menu_backbuffer_dim) {
			state.menu_backbuffer_dim = new_dim;
			if (state.menu_backbuffer) DeleteBitmap(state.menu_backbuffer);
			// We need to get the window's dc because it contains the bitmap with the correct bitmap format, CreateCompatibleBitmap uses the currently selected bitmap to create a new one
			HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
			state.menu_backbuffer = CreateCompatibleBitmap(dc, 
				state.menu_backbuffer_dim.cx, state.menu_backbuffer_dim.cy
			);
		}
	}
}

void render_menu_backbuffer(State& state, HDC target_dc, const RECT menurc) {
	auto oldbmp = SelectBitmap(state.menu_offscreen_dc, state.menu_backbuffer); defer{ SelectBitmap(state.menu_offscreen_dc, oldbmp); };
	Assert(state.menu);
	{
		HDC dc = state.menu_offscreen_dc;
		SetViewportOrgEx(dc, -menurc.left, -menurc.top, nil); defer{ SetViewportOrgEx(dc, 0, 0, nil); };

		MENUINFO mi{ sizeof(mi) };
		mi.fMask = MIM_BACKGROUND;
		//TODO(fran): MIM_MAXHEIGHT and other fMask params that we should set
		GetMenuInfo(state.menu, &mi);

		//Set font and colors
		HBRUSH bkbr = mi.hbrBack;
		HBRUSH txtbr = state.active ? colors.ControlTxt : colors.ControlTxt_Inactive;
		SetTextColor(dc, ColorFromBrush(txtbr));
		HFONT oldmenufont = (HFONT)SelectObject(dc, fonts.Menu); defer{ SelectObject(dc,oldmenufont); };

		//Paint background
		RECT bkrc = menurc;
		if (menu_bottom_border) bkrc.bottom -= menu_bottom_border_thickness;
		FillRect(dc, &bkrc, bkbr);

		RECT menuitemrc = menurc;
		int menu_xpad = 4;
		menuitemrc.left += menu_xpad;

		//Store current item count
		state.menubar_itemcnt = GetMenuItemCount(state.menu);//NOTE: this guy returns -1 on errors, thats why the variable is i32
		Assert(state.menubar_itemcnt < ARRAYSIZE(state.menubar_items));

		//Find out what item, if any, is on mouse over
		i32 menubar_mouseover_idx = state.menubar_mouseover_idx_from1 - 1;
		SetBkMode(dc, TRANSPARENT);

		//Set bk brush for drawing text
		for (int i = 0; i < state.menubar_itemcnt; i++) {
			bool on_mouseover = (i == menubar_mouseover_idx);
			//Paint mouseover bk
			if (on_mouseover) {
				//TODO(fran): im having to cheat a little bit here by using the rc from the previous painting, it could be outdated
				FillRect(dc, &state.menubar_items[i], colors.ControlBkMouseOver);
			}

			//Get text lenght
			MENUITEMINFO mtxti{ sizeof(mtxti) };
			mtxti.fMask = MIIM_STRING;
			mtxti.dwTypeData = NULL;
			GetMenuItemInfo(state.menu, i, TRUE, &mtxti);
			//Get actual text
			UINT menu_str_character_cnt = mtxti.cch + 1 + 2; //includes null terminator and 2 extra spaces
			mtxti.cch = menu_str_character_cnt;
			TCHAR* menu_str = (TCHAR*)malloc(menu_str_character_cnt * sizeof(TCHAR)); defer{ free(menu_str); };
			menu_str[0] = TEXT(' ');//initial space
			mtxti.dwTypeData = menu_str + 1; //Offset for initial space
			GetMenuItemInfo(state.menu, i, TRUE, &mtxti);
			menu_str[menu_str_character_cnt - 2] = TEXT(' ');//ending space

			// Calculate vertical and horizontal position for the string so that it will be centered
			//NOTE: TabbedTextOut doesnt care for alignment (SetTextAlign)
			TEXTMETRIC mtm; GetTextMetrics(dc, &mtm);
			//int yPos = menuitemrc.top + (RECTH(menuitemrc) - mtm.tmHeight) / 2;
			int yPos = (menuitemrc.bottom + menuitemrc.top - mtm.tmHeight) / 2;
			int xPos = menuitemrc.left;
			LONG txtsz = TabbedTextOut(dc, xPos, yPos, menu_str, menu_str_character_cnt - 1, 0, 0, xPos);
			WORD txtw = LOWORD(txtsz);

			//Store menu item rc
			state.menubar_items[i] = RECT{ 
				.left = menuitemrc.left, .top = menuitemrc.top, 
				.right = menuitemrc.left + txtw, .bottom = menuitemrc.bottom
			};

			menuitemrc.left += txtw;
		}

		if (menu_bottom_border) {
			RECT menuborder = menurc; menuborder.top = menuborder.bottom - menu_bottom_border_thickness;
			FillRect(dc, &menuborder, txtbr);//TODO(fran):parametric, not everyone might like this
		}
	}
	BitBlt(target_dc, menurc.left, menurc.top, RECTW(menurc), RECTH(menurc),
		state.menu_offscreen_dc, 0, 0, SRCCOPY);
}

static HMENU current_menu = nil;
static POINT current_menu_pt{};
//A little delay before allowing the user to select a new menu, this prevents problems, eg the user trying to close the menu by selecting it again in the menu bar
constexpr auto menu_delay_ms = 50;
constexpr auto menu_open_ms = 34;
static auto menudelay = [](HWND wnd, UINT, UINT_PTR timer_id, DWORD) {
	KillTimer(wnd, timer_id);
	auto& state = *get_state(wnd);
	state.menu_on_delay = false;
};
// Returns true if handled, false if should be sent to default handler
// handle_mode_standard false means we are trying to open a menu because of a mouseover event while another menu is already open
bool handle_nclbuttondown(State& state, POINT mouse, i32 hittest) {
	POINT cl_mouse = mouse; ScreenToClient(state.wnd, &cl_mouse);
	if (hittest == HTMENU || hittest == HTSYSMENU) {//The menu was clicked, NOTE: for menus we dont wait for buttonup
		if (!state.menu_on_delay) {
			for (int i = 0; i < state.menubar_itemcnt; i++) {
				if (test_pt_rc(cl_mouse, state.menubar_items[i])) {
					HMENU submenu = GetSubMenu(state.menu, i);
					if (submenu) {
						POINT menupt{ state.menubar_items[i].left,state.menubar_items[i].bottom - 1 }; ClientToScreen(state.wnd, &menupt);
						if (current_menu && current_menu != submenu) {
							// Try to get any currently open menu of ours to close before opening the new one
							DefWindowProc(state.wnd, WM_CANCELMODE, 0, 0);
							SetForegroundWindow(state.wnd);
						}
						if (current_menu != submenu) {
							current_menu_pt = menupt;
							SetTimer(state.wnd, (UINT_PTR)submenu, menu_open_ms, [](HWND wnd, UINT, UINT_PTR timer_id, DWORD) {
								KillTimer(wnd, timer_id);
								auto& state = *get_state(wnd);
								auto submenu = (HMENU)timer_id;
								if (current_menu != submenu) {
									auto oldcurrentmenu = current_menu;
									current_menu = submenu;
									auto res = TrackPopupMenu(submenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, current_menu_pt.x, current_menu_pt.y, 0, state.wnd, 0);
									//INFO: TrackPopupMenu stalls the thread until it completes! So we cannot do anything after it, if it returned true, meaning it didnt fail. We can only extract knowledge from it if it returns false, meaning it failed, meaning there's already a menu open
									if (!res) {
										current_menu = oldcurrentmenu;
									}
								}
							});
						}
						
						//Set menu delay to stop possible reopening
						state.menu_on_delay = true;
						SetTimer(state.wnd, (UINT_PTR)&menudelay, menu_delay_ms, menudelay);
						//TODO(fran): here we could pass our client wnd as the owner of the menu
						//TODO(fran): TPM_RETURNCMD could be interesting
						//TODO(fran): TPM_RECURSE ?
						break;
					}
				}
			}
		}
		return true;
	}
	else if (hittest == HTCAPTION) {
		if (!state.menu_on_delay) {
			if (test_pt_rc(cl_mouse, state.rc_icon)) {
				POINT mpos{ state.rc_icon.left,state.rc_icon.bottom }; ClientToScreen(state.wnd, &mpos);
				show_rclickmenu(state, mpos);
				//Set menu delay to stop possible reopening
				state.menu_on_delay = true;
				SetTimer(state.wnd, (UINT_PTR)&menudelay, menu_delay_ms, menudelay);
				return true;
			}
		}
	}
	return false;
}

//TODO(fran): add & at the beginning of menu string names, that's how you trigger them by pressing Alt+key https://stackoverflow.com/questions/38338426/meaning-of-ampersand-in-rc-files
//TODO(fran): DPI awareness
//TODO(fran): it'd be nice to have a way to implement good subclassing, eg letting the user assign clip regions where they can draw and we dont, things like that, more communication
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//printf("nonclient: %s\n", msgToString(msg));
	State& state = *get_state(hwnd);

	switch (msg) {
	case WM_WINDOWPOSCHANGED: //TODO(fran): we can use this guy to replace WM_SIZE and WM_MOVE by not calling defwindowproc
	{
		calc_caption(state);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_UNINITMENUPOPUP:
	{
		HMENU menu_destroyed = (HMENU)wparam;
		if (menu_destroyed == current_menu) {
			current_menu = nil;
			//Set menu delay to stop possible reopening
			state.menu_on_delay = true;
			SetTimer(state.wnd, (UINT_PTR)&menudelay, menu_delay_ms, menudelay);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_PAINT://TODO(fran): we gotta offset the painting a few pixels down when maximized, the amount of things that will need an extra condition on calculation is pretty awful, maybe we should just handle maximizing ourselves
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };

		if (rcs_overlap(state.rc_caption, ps.rcPaint)) {
			//We get system painted 1px borders on the left, right and bottom, but not on the top, for now until we find out why we just emulate it
			auto top_border_rc = state.rc_caption; top_border_rc.bottom = top_border_rc.top + top_border_thickness;
			FillRect(dc, &top_border_rc, colors.CaptionBk_Inactive); //TODO(fran): could not find a system default that matches to the brush used for the borders, used mine which matches prety well

			auto bk_rc = state.rc_caption; bk_rc.top += top_border_thickness;
			FillRect(dc, &bk_rc, state.active ? colors.CaptionBk : colors.CaptionBk_Inactive);

			SelectFont(dc, GetStockFont(DEFAULT_GUI_FONT));
			TCHAR title[128]; int sz = GetWindowText(state.wnd, title, ARRAYSIZE(title));
			TEXTMETRIC tm; GetTextMetrics(dc, &tm);


			HICON icon = (HICON)GetClassLongPtr(state.wnd, GCLP_HICONSM);
			int icon_height = (int)((float)tm.tmHeight * 1.5f);
			int icon_width = icon_height;
			int icon_align_height = (RECTH(state.rc_caption) - icon_height) / 2;
			int icon_align_width = icon_align_height;
			state.rc_icon = rectWH(icon_align_height, icon_align_width, icon_width, icon_height);
			auto iconnfo = MyGetIconInfo(icon);
			urender::draw_icon(dc, icon_align_height, icon_align_width, icon_width, icon_height, icon, 0, 0, iconnfo.w, iconnfo.h);

			int yPos = (state.rc_caption.bottom + state.rc_caption.top - tm.tmHeight) / 2;
			HBRUSH txtbr = state.active ? colors.ControlTxt : colors.ControlTxt_Inactive;
			SetTextColor(dc, ColorFromBrush(txtbr)); SetBkMode(dc, TRANSPARENT);

			TextOut(dc, icon_align_width * 2 + icon_width, yPos, title, sz);

			HBRUSH btn_border, btn_bk, btn_fore, btn_bk_push, btn_bk_mouseover;
			if (state.active) {
				btn_border = colors.CaptionBk;
				btn_bk = colors.CaptionBk;
				btn_fore = colors.Img;
				btn_bk_push = colors.ControlBkPush;
				btn_bk_mouseover = colors.ControlBkMouseOver;
			}
			else {
				btn_border = colors.CaptionBk_Inactive;
				btn_bk = colors.CaptionBk_Inactive;
				btn_fore = colors.Img_Inactive;
				btn_bk_push = colors.ControlBkPush;
				btn_bk_mouseover = colors.ControlBkMouseOver;
			}
			button::Theme btn{
				.brushes = {
					.foreground = {.normal = btn_fore},
					.bk = {.normal = btn_bk, .mouseover = btn_bk_mouseover, .clicked = btn_bk_push},
					.border = {.normal = btn_border}
				}
			};
			//TODO: we shouldnt be triggering a redraw while on wm_paint, this can be done elsewhere
			button::set_theme(state.btn_close, btn);
			button::set_theme(state.btn_min, btn);
			if (state.btn_max) button::set_theme(state.btn_max, btn);
		}

		//TODO(fran): allow moving the menu up next to the title to get more screen real state, visual studio differentiates the menu items from the window title by rendering a darker square around the latter
		if (RECT menurc = calc_menu_rc(state); state.menu && rcs_overlap(menurc, ps.rcPaint))
			render_menu_backbuffer(state, dc, menurc);
		return 0;
	} break;
	case WM_CREATE:
	{

#if 1
		//NOTE: we want WS_OVERLAPPEDWINDOW for the niceties on resizing, maximizing not obstructing the taskbar, scaling the window in the y axis when double clicking the top/bottom resizing border (unfortunately doesnt do the same for left and right <-TODO(fran): also it doesnt do it for top and bottom, is this done by hand by every program?), and similar. The one problem is that we'll need to draw differently when maximized, everything will have to be moved down a little
		//TODO(fran): check no strange things happen now we re-enabled OVERLAPPEDWINDOW, at least it doesnt seem to be creating the horrible max/min etc buttons now we dont use WS_OVERLAPPEDWINDOW in CreateWindow
		LONG_PTR  dwStyle = GetWindowLongPtr(state.wnd, GWL_STYLE);
		SetWindowLongPtr(state.wnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
#endif

		CREATESTRUCT* createnfo = (CREATESTRUCT*)lparam;

		SetWindowText(state.wnd, createnfo->lpszName); //NOTE: for handmade classes you have to manually call setwindowtext

		RECT btn_min_rc = state.settings->can_maximize ? calc_btn_min_rc(state) : calc_btn_max_rc(state);
		state.btn_min = CreateWindow(button::wndclass, TEXT(""), WS_CHILD | WS_VISIBLE | BS_BITMAP, btn_min_rc.left, btn_min_rc.top, RECTW(btn_min_rc), RECTH(btn_min_rc), state.wnd, (HMENU)NC_MINIMIZE, 0, 0);
		//UNCAPBTN_set_brushes(state.btn_min, TRUE, colors.CaptionBk, colors.CaptionBk, colors.ControlTxt, colors.ControlBkPush, colors.ControlBkMouseOver); //NOTE: now I do this on WM_PAINT, commenting this actually introduces a bug for the first ms of execution where the button might draw with no brushes first and then be asked to redraw with the brushes loaded, introducing at least one frame of flicker

		if (state.settings->can_maximize) {
			RECT btn_max_rc = calc_btn_max_rc(state);
			state.btn_max = CreateWindow(button::wndclass, TEXT(""), WS_CHILD | WS_VISIBLE | BS_BITMAP, btn_max_rc.left, btn_max_rc.top, RECTW(btn_max_rc), RECTH(btn_max_rc), state.wnd, (HMENU)NC_MAXIMIZE, 0, 0);
			//UNCAPBTN_set_brushes(state.btn_max, TRUE, colors.CaptionBk, colors.CaptionBk, colors.ControlTxt, colors.ControlBkPush, colors.ControlBkMouseOver);
		}

		RECT btn_close_rc = calc_btn_close_rc(state);
		state.btn_close = CreateWindow(button::wndclass, TEXT(""), WS_CHILD | WS_VISIBLE | BS_BITMAP, btn_close_rc.left, btn_close_rc.top, RECTW(btn_close_rc), RECTH(btn_close_rc), state.wnd, (HMENU)NC_CLOSE, 0, 0);
		//UNCAPBTN_set_brushes(state.btn_close, TRUE, colors.CaptionBk, colors.CaptionBk, colors.ControlTxt, colors.ControlBkPush, colors.ControlBkMouseOver);

		//TODO(fran): let the user set the bitmaps, pass them in theme
		SendMessage(state.btn_close, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.close);
		if (state.btn_max) {
			SendMessage(state.btn_max, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.maximize);
		}
		SendMessage(state.btn_min, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmps.minimize);

		//TODO(fran): create the icons, one idea is to ask windows to paint them in some dc, and then I can store the HBITMAP and re-use it all I want

		if (state.settings->client_class_name) {
			RECT rc = calc_client_rc(state);
			state.client = CreateWindowExW(WS_EX_COMPOSITED, state.settings->client_class_name, TEXT(""), WS_CHILD | WS_VISIBLE, rc.left, rc.top, RECTW(rc), RECTH(rc), state.wnd, 0, 0, state.settings->client_lp_param);
		}

		state.toast = create_window(state.client ? state.client : state.wnd, toast::wndclass, nil, WS_CHILD);
		toast::set_theme(state.toast, themes.base_toast);

		return 0;
	} break;
	case WM_NCLBUTTONDBLCLK:
	{
		LRESULT hittest = (LRESULT)wparam;

		if (hittest == HTCAPTION) {
			POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			POINT cl_mouse = mouse; ScreenToClient(state.wnd, &cl_mouse);

			if (test_pt_rc(cl_mouse, state.rc_icon)) {
				PostMessage(state.wnd, WM_COMMAND, (WPARAM)MAKELONG(NC_CLOSE, 0), (LPARAM)state.btn_close);//TODO(fran): should I use WM_CLOSE?
				return 0;
			}

			WINDOWPLACEMENT p{ sizeof(p) }; GetWindowPlacement(state.wnd, &p);

			if (p.showCmd == SW_SHOWMAXIMIZED) ShowWindow(state.wnd, SW_RESTORE);
			else {
#if 1
				//LONG_PTR  dwStyle = GetWindowLongPtr(state.wnd, GWL_STYLE);
				//SetWindowLongPtr(state.wnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);//TODO(fran): now im REALLY confused, I had to specifically take out WS_OVERLAPPEDWINDOW so it wouldnt paint over me and now it doesnt seem to, test more thoroughly //still though sizing isnt perfect, the top is still cut off
				ShowWindow(state.wnd, SW_MAXIMIZE);
#else
				manual_maximize(state);
#endif
			}
		}
		return 0;
	} break;
	case WM_NCHITTEST:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		if (res == HTCLIENT) {
			int top_margin = 8;
			int top_side_margin = 16;
			POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; ScreenToClient(hwnd, &mouse);
			RECT rc; GetClientRect(hwnd, &rc);
			RECT rc_top; rc_top.top = rc.top; rc_top.left = rc.left + top_side_margin; rc_top.bottom = rc_top.top + top_margin; rc_top.right = rc.right - top_side_margin;
			RECT rc_topleft; rc_topleft.top = rc.top; rc_topleft.left = rc.left; rc_topleft.bottom = rc_topleft.top + top_margin; rc_topleft.right = rc_topleft.left + top_side_margin;
			RECT rc_topright; rc_topright.top = rc.top; rc_topright.left = rc.right - top_side_margin; rc_topright.bottom = rc_topright.top + top_margin; rc_topright.right = rc.right;

			RECT rc_menu = calc_menu_rc(state);

			if (test_pt_rc(mouse, rc_top)) res = HTTOP;
			elif (test_pt_rc(mouse, rc_topleft)) res = HTTOPLEFT;
			elif (test_pt_rc(mouse, rc_topright)) res = HTTOPRIGHT;
			/*else if (test_pt_rc(mouse, close_btn)) res = HTCLOSE; //TODO(fran): request this buttons for their rc
			else if (test_pt_rc(mouse, max_btn)) res = HTMAXBUTTON;
			else if (test_pt_rc(mouse, min_btn)) res = HTMINBUTTON;*/
			elif (test_pt_rc(mouse, rc_menu)) res = HTSYSMENU;//HTMENU also works, but it seems a little slower on clicking for some reason //IMPORTANT: this is crucial for the default menu to detect clicks, otherwise no mouse input goes to it
			elif (test_pt_rc(mouse, state.rc_caption)) res = HTCAPTION;
		}

		//printf("%s\n", hittestToString(res));
		return res;
	} break;
	case WM_NCCALCSIZE:
	{
		RECT testrc{ 0,0,100,100 };
		RECT ncrc = testrc;
		AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE); //TODO(fran): somehow link this params with calc_caption

		calc_caption(state);//TODO(fran): find out which is the one and only magical msg where this can be put in

		MY_MARGINS margins;

		margins.cxLeftWidth = distance(testrc.left, ncrc.left);
		margins.cxRightWidth = distance(testrc.right, ncrc.right);
		margins.cyBottomHeight = distance(testrc.bottom, ncrc.bottom);
		margins.cyTopHeight = 0;

		state.nc = margins;

		if ((BOOL)wparam == TRUE) {
			//IMPORTANT: absolutely _everything_ is in screen coords
			NCCALCSIZE_PARAMS* calcsz = (NCCALCSIZE_PARAMS*)lparam; //input and ouput respectively is and has to be in screen coords
			RECT new_wnd_coords_proposed = calcsz->rgrc[0];
			calcsz->rgrc[1];//old wnd coords
			calcsz->rgrc[2];//old client coords

			//new client coords
			calcsz->rgrc[0].left += margins.cxLeftWidth;
			calcsz->rgrc[0].top += margins.cyTopHeight;
			calcsz->rgrc[0].right -= margins.cxRightWidth;
			calcsz->rgrc[0].bottom -= margins.cyBottomHeight;

			//valid dest coords
			calcsz->rgrc[1] = new_wnd_coords_proposed;

			//valid source coords
			calcsz->rgrc[1] = calcsz->rgrc[0];
		}
		else {
			RECT* calcrc = (RECT*)lparam; //input and ouput respectively is and has to be in screen coords
			RECT new_wnd_coords_proposed = *calcrc;

			//new client coords
			calcrc->left += margins.cxLeftWidth;
			calcrc->top += margins.cyTopHeight;
			calcrc->right -= margins.cxRightWidth;
			calcrc->bottom -= margins.cyBottomHeight;
		}
		return 0; //For wparam==TRUE returning 0 means to reuse the old client area's painting and just align it with the top-left corner of the new client area pos
	} break;
	case WM_SIZE:
	{
		RECT rc = calc_client_rc(state); MoveWindow(state.client, rc.left, rc.top, RECTW(rc), RECTH(rc), TRUE);

		RECT btn_min_rc = state.settings->can_maximize ? calc_btn_min_rc(state) : calc_btn_max_rc(state);
		MoveWindow(state.btn_min, btn_min_rc.left, btn_min_rc.top, RECTW(btn_min_rc), RECTH(btn_min_rc), TRUE);//TODO(fran): I dont really need to ask for repaint do I?
		if (state.settings->can_maximize) {
			RECT btn_max_rc = calc_btn_max_rc(state); MoveWindow(state.btn_max, btn_max_rc.left, btn_max_rc.top, RECTW(btn_max_rc), RECTH(btn_max_rc), TRUE);
		}
		RECT btn_close_rc = calc_btn_close_rc(state); MoveWindow(state.btn_close, btn_close_rc.left, btn_close_rc.top, RECTW(btn_close_rc), RECTH(btn_close_rc), TRUE);

		resize_menu_backbuffer(state);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SETTEXT:
	{
		//This function is insane, it actually does painting on it's own without telling nobody, so we need a way to kill that
		//I think there are a couple approaches that work, I took this one which works since windows 95 (from what the article says)
		//Many thanks for yet another hack http://www.catch22.net/tuts/win32/custom-titlebar
		LONG_PTR  dwStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
		// turn off WS_VISIBLE
		SetWindowLongPtr(hwnd, GWL_STYLE, dwStyle & ~WS_VISIBLE);

		// perform the default action, minus painting
		LRESULT ret = DefWindowProc(hwnd, msg, wparam, lparam);

		// turn on WS_VISIBLE
		SetWindowLongPtr(hwnd, GWL_STYLE, dwStyle);

		// perform custom painting, aka dont and do what should be done, repaint, it's really not that expensive for our case, we barely call WM_SETTEXT, and it can be optimized out later
		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);

		return ret;
	} break;
	case WM_NCDESTROY:
	{
		if (&state) {
			if (state.menu_backbuffer) { DeleteBitmap(state.menu_backbuffer); state.menu_backbuffer = nil; }
			DeleteDC(state.menu_offscreen_dc);
			set_window_state(state.wnd, nil);
			free(&state);
		}
	} break;
	case WM_NCCREATE:
	{
		CREATESTRUCT* create_nfo = (CREATESTRUCT*)lparam;
		//TODO(fran): check for minimize and maximize button requirements
		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		st->wnd = hwnd;
		st->settings = (LpParam*)create_nfo->lpCreateParams;
		Assert(st->settings);
		HDC dc = GetDC(st->wnd); defer{ ReleaseDC(st->wnd, dc); };
		st->menu_offscreen_dc = CreateCompatibleDC(dc);
		return TRUE;
	} break;
	case WM_NCACTIVATE:
	{
		//So basically this guy is another WM_NCPAINT, just do exactly the same, but also here we have the option to paint slightly different if we are deactivated, so the user can see the change
		BOOL active = (BOOL)wparam; //Indicates active or inactive state for a title bar or icon that needs to be changed
		state.active = active;
		HRGN opt_upd_rgn = (HRGN)lparam; // handle to optional update region for the nonclient area. If set to -1, do nothing
		if (state.menu) {
			MENUINFO mi{ sizeof(mi) };
			mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS; //TODO(fran): MIM_APPLYTOSUBMENUS is garbage, it only goes one level down, just terrible, make my own
			//TODO(fran): MIM_MAXHEIGHT and other fMask params that we should set

			mi.hbrBack = state.active ? colors.CaptionBk : colors.CaptionBk_Inactive;

			SetMenuInfo(state.menu, &mi);
		}

		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
		return TRUE; //NOTE: it says we can return FALSE to "prevent the change"
	} break;

	case WM_MEASUREITEM: //NOTE: this is so stupid, this guy doesnt send hwndItem like WM_DRAWITEM does, so you have no way of knowing which type of menu you get, gotta do it manually
		//TODO(fran): should this and wm_drawitem go in uncapcl?
	{
		//wparam has duplicate info from item
		MEASUREITEMSTRUCT* item = (MEASUREITEMSTRUCT*)lparam;
		//NOTE: for menus wparam is always 0, not useful at all
		if (item->CtlType == ODT_MENU && item->itemData) { //menu with parent HMENU
			//item->CtlID is not used
			//Determine which type of menu we're to measure
			MENUITEMINFO menu_type;
			menu_type.cbSize = sizeof(menu_type);
			menu_type.fMask = MIIM_FTYPE;
			GetMenuItemInfo((HMENU)item->itemData, item->itemID, FALSE, &menu_type);
			menu_type.fType ^= MFT_OWNERDRAW; //remove ownerdraw since we know all guys should be //TODO(fran): I think we should still check for the flag being there
			switch (menu_type.fType) {
			case MFT_STRING:
			{
				//TODO(fran): check if it has a submenu, in which case, if it opens it to the side, we should leave a little more space for an arrow bmp (though there seems to be some extra space added already)

				//Determine text space:
				MENUITEMINFO menu_nfo; menu_nfo.cbSize = sizeof(menu_nfo);
				menu_nfo.fMask = MIIM_STRING;
				menu_nfo.dwTypeData = NULL;
				GetMenuItemInfo((HMENU)item->itemData, item->itemID, FALSE, &menu_nfo);
				UINT menu_str_character_cnt = menu_nfo.cch + 1; //includes null terminator
				menu_nfo.cch = menu_str_character_cnt;
				TCHAR* menu_str = (TCHAR*)malloc(menu_str_character_cnt * sizeof(TCHAR));
				menu_nfo.dwTypeData = menu_str;
				GetMenuItemInfo((HMENU)item->itemData, item->itemID, FALSE, &menu_nfo);

				//wprintf(L"%s\n", menu_str);

				HDC dc = GetDC(hwnd); //Of course they had to ask for a dc, and not give the option to just provide the font, which is the only thing this function needs
				HFONT hfntPrev = (HFONT)SelectObject(dc, fonts.Menu);//TODO(fran): parametric
				int old_mapmode = GetMapMode(dc);
				SetMapMode(dc, MM_TEXT);
				WORD text_width = LOWORD(GetTabbedTextExtent(dc, menu_str, menu_str_character_cnt - 1, 0, NULL)); //TODO(fran): make common function for this and the one that does rendering, also look at how tabs work
				WORD space_width = LOWORD(GetTabbedTextExtent(dc, TEXT(" "), 1, 0, NULL)); //a space at the beginning
				SetMapMode(dc, old_mapmode);

				SelectObject(dc, hfntPrev);
				ReleaseDC(hwnd, dc);
				free(menu_str);
				//
				//printf("MEASURE: item->itemID=%#016x\n", item->itemID);
				//if (item->itemID == ((UINT)HACK_toplevelmenu & 0xFFFFFFFF)) { //Check if we are a "top level" menu
				//if (is_on_menu_bar(state, item->itemID)) { //Check if we are a "top level" menu
				if ((HMENU)item->itemData == state.menu) { //Check if we are a "top level" menu
					item->itemWidth = text_width + space_width * 2;
				}
				else {
					item->itemWidth = GetSystemMetrics(SM_CXMENUCHECK) + text_width + space_width; /*Extra space for left bitmap*/; //TODO(fran): we'll probably add a 1 space separation between bmp and txt

				}
				item->itemHeight = GetSystemMetrics(SM_CYMENU); //Height of menu

				//printf("width:%d ; height:%d\n", item->itemWidth, item->itemHeight);

				return TRUE;
			} break;
			case MF_SEPARATOR:
			{
				item->itemHeight = 3;
				item->itemWidth = 1;
				return TRUE;
			} break;
			default: return DefWindowProc(hwnd, msg, wparam, lparam);
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DRAWITEM:
	{
		DRAWITEMSTRUCT* item = (DRAWITEMSTRUCT*)lparam;
		switch (wparam) {//wparam specifies the identifier of the control that needs painting
		case 0: //menu
		{ //TODO(fran): handle WM_MEASUREITEM so we can make the menu bigger
			Assert(item->CtlType == ODT_MENU); //TODO(fran): I could use this instead of wparam
			/*NOTES
			- item->CtlID isnt used for menus
			- item->itemID menu item identifier
			- item->itemAction required drawing action //TODO(fran): use this
			- item->hwndItem handle to the menu that contains the item, aka HMENU
			*/

			//NOTE: do not use itemData here, go straight for item->hwndItem

			//Determine which type of menu we're to draw
			MENUITEMINFO menu_type;
			menu_type.cbSize = sizeof(menu_type);
			menu_type.fMask = MIIM_FTYPE | MIIM_SUBMENU;
			GetMenuItemInfo((HMENU)item->hwndItem, item->itemID, FALSE, &menu_type);
			menu_type.fType ^= MFT_OWNERDRAW; //remove ownerdraw since we know all guys should be

			switch (menu_type.fType) {
				//NOTE: MFT_BITMAP, MFT_SEPARATOR, and MFT_STRING cannot be combined with one another, so we know those are separate types
			case MFT_STRING: //Text menu
			{//NOTE: we render the bitmaps inverted, cause I find it easier to edit on external programs, this may change later, not a hard change
				int x, y;

				// Set the appropriate foreground and background colors. 
				HBRUSH txt_br, bk_br;
				if (item->itemState & ODS_SELECTED || item->itemState & ODS_HOTLIGHT /*needed for "top level" menus*/) //TODO(fran): ODS_CHECKED ODS_FOCUS
				{
					txt_br = colors.ControlTxt;
					bk_br = colors.ControlBkMouseOver;
				}
				else
				{
					txt_br = colors.ControlTxt;
					Assert((UINT)(UINT_PTR)item->hwndItem);
					//GetMenuInfo((HMENU)item->hwndItem, &mnfo); //We will not ask our "parent" hmenu since sometimes it decides it doesnt have the hbrush, we'll go straight to the menu bar, if there is one
					if (state.menu) {
						MENUINFO mnfo{ sizeof(mnfo) }; mnfo.fMask = MIM_BACKGROUND;
						GetMenuInfo(state.menu, &mnfo);
						bk_br = mnfo.hbrBack;
					}
					else bk_br = colors.CaptionBk;

				}
				//TODO(fran): separate menu brushes
				auto oldtxtcol = SetTextColor(item->hDC, ColorFromBrush(txt_br)); defer{ SetTextColor(item->hDC, oldtxtcol); };
				auto oldbkcol = SetBkColor(item->hDC, ColorFromBrush(bk_br)); defer{ SetBkColor(item->hDC, oldbkcol); };

				if (item->itemAction & ODA_DRAWENTIRE || item->itemAction & ODA_SELECT) //Draw background
					FillRect(item->hDC, &item->rcItem, bk_br);

				// Select the font and draw the text. //TODO(fran): parametric font
				auto oldfont = (HFONT)SelectObject(item->hDC, fonts.Menu); defer{ SelectObject(item->hDC, oldfont); };

				WORD x_pad = LOWORD(GetTabbedTextExtent(item->hDC, TEXT(" "), 1, 0, NULL)); //an extra 1 space before drawing text (for not top level menus)
				if (item->hwndItem == (HWND)state.menu) { //If we are on the menu bar (then hwndItem, our parent, will be the same as state.menu)
					//we just want to draw the text, nothing more
					//TODO(fran): clean this huge if-else, very bug prone with things being set/initialized in different parts

					//Get text lenght //TODO(fran): use language_mgr method, we dont need to fight with all this garbage
					MENUITEMINFO menu_nfo;
					menu_nfo.cbSize = sizeof(menu_nfo);
					menu_nfo.fMask = MIIM_STRING;
					menu_nfo.dwTypeData = NULL;
					GetMenuItemInfo((HMENU)item->hwndItem, item->itemID, FALSE, &menu_nfo);
					//Get actual text
					UINT menu_str_character_cnt = menu_nfo.cch + 1; //includes null terminator
					menu_nfo.cch = menu_str_character_cnt;
					TCHAR* menu_str = (TCHAR*)malloc(menu_str_character_cnt * sizeof(TCHAR));
					menu_nfo.dwTypeData = menu_str;
					GetMenuItemInfo((HMENU)item->hwndItem, item->itemID, FALSE, &menu_nfo);

					//Thanks https://stackoverflow.com/questions/3478180/correct-usage-of-getcliprgn
					//WINDOWS THIS MAKES NO SENSE!!!!!!!!!
					HRGN restoreRegion = CreateRectRgn(0, 0, 0, 0); if (GetClipRgn(item->hDC, restoreRegion) != 1) { DeleteObject(restoreRegion); restoreRegion = NULL; }

					// Set new region, do drawing
					IntersectClipRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom);//This is also stupid, did they have something against RECT ???????
					UINT old_align = GetTextAlign(item->hDC);
					SetTextAlign(item->hDC, TA_CENTER); //TODO(fran): VTA_CENTER for kanji and the like
					// Calculate vertical and horizontal position for the string so that it will be centered
					TEXTMETRIC tm; GetTextMetrics(item->hDC, &tm);
					int yPos = (item->rcItem.bottom + item->rcItem.top - tm.tmHeight) / 2;
					int xPos = item->rcItem.left + (item->rcItem.right - item->rcItem.left) / 2;
					TextOut(item->hDC, xPos, yPos, menu_str, menu_str_character_cnt - 1);
					//wprintf(L"%s\n",menu_str);
					free(menu_str);
					SetTextAlign(item->hDC, old_align);

					SelectClipRgn(item->hDC, restoreRegion); if (restoreRegion != NULL) DeleteObject(restoreRegion); //Restore old region
				}
				else {

					//Render img on the left
					{
						MENUITEMINFO menu_img;
						menu_img.cbSize = sizeof(menu_img);
						menu_img.fMask = MIIM_CHECKMARKS | MIIM_STATE;
						GetMenuItemInfo((HMENU)item->hwndItem, item->itemID, FALSE, &menu_img);
						HBITMAP hbmp = NULL;
						if (menu_img.fState & MFS_CHECKED) { //If it is checked you can be sure you are going to draw some bmp
							if (!menu_img.hbmpChecked) {
								//TODO(fran): assign default checked bmp
							}
							hbmp = menu_img.hbmpChecked;
						}
						else if (menu_img.fState == MFS_UNCHECKED || menu_img.fState == MFS_HILITE) {//Really Windows? you really needed to set the value to 0? //TODO(fran): maybe it's better to just use else, maybe that's windows' logic for doing this
							if (menu_img.hbmpUnchecked) {
								hbmp = menu_img.hbmpUnchecked;
							}
							//If there's no bitmap we dont draw
						}
						if (hbmp) {
							BITMAP bitmap; GetObject(hbmp, sizeof(bitmap), &bitmap);

							int img_max_x = GetSystemMetrics(SM_CXMENUCHECK);
							int img_max_y = RECTH(item->rcItem);
							//HACK: instead use png + gdi+ + color matrices
							int img_sz = (bitmap.bmBitsPixel == 1) ? 
								roundNdown(bitmap.bmWidth, minimum(img_max_x, img_max_y)) : 16;
							if (!img_sz)img_sz = bitmap.bmWidth; //More HACKs
							int bmp_height = img_sz;
							int bmp_width = bmp_height;
							int bmp_align_width = item->rcItem.left + (img_max_x + x_pad - bmp_width) / 2;
							int bmp_align_height = item->rcItem.top + (img_max_y - bmp_height) / 2;

							//TODO(fran): clipping
							if (bitmap.bmBitsPixel == 1)
								urender::draw_menu_mask(item->hDC, bmp_align_width, bmp_align_height, bmp_width, bmp_height, hbmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, colors.Img);//TODO(fran): parametric brush
							elif (bitmap.bmBitsPixel == 8)
								urender::draw_menu_mask8(item->hDC, bmp_align_width, bmp_align_height, bmp_width, bmp_height, hbmp, colors.Img);
						}
					}

					// Determine where to draw, leave space for a check mark and the extra 1 space
					x = item->rcItem.left;
					y = item->rcItem.top;
					x += GetSystemMetrics(SM_CXMENUCHECK) + x_pad;

					//Get text lenght //TODO(fran): use language_mgr method, we dont need to fight with all this garbage
					MENUITEMINFO menu_nfo;
					menu_nfo.cbSize = sizeof(menu_nfo);
					menu_nfo.fMask = MIIM_STRING;
					menu_nfo.dwTypeData = NULL;
					GetMenuItemInfo((HMENU)item->hwndItem, item->itemID, FALSE, &menu_nfo);
					//Get actual text
					UINT menu_str_character_cnt = menu_nfo.cch + 1; //includes null terminator
					menu_nfo.cch = menu_str_character_cnt;
					TCHAR* menu_str = (TCHAR*)malloc(menu_str_character_cnt * sizeof(TCHAR));
					menu_nfo.dwTypeData = menu_str;
					GetMenuItemInfo((HMENU)item->hwndItem, item->itemID, FALSE, &menu_nfo);

					//Thanks https://stackoverflow.com/questions/3478180/correct-usage-of-getcliprgn
					//WINDOWS THIS MAKES NO SENSE!!!!!!!!!
					HRGN restoreRegion = CreateRectRgn(0, 0, 0, 0);
					if (GetClipRgn(item->hDC, restoreRegion) != 1)
					{
						DeleteObject(restoreRegion);
						restoreRegion = NULL;
					}

					// Set new region, do drawing
					IntersectClipRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom);//This is also stupid, did they have something against RECT ???????
					//TODO(fran): tabs start spacing from the initial x coord, which is completely wrong, we're probably gonna need to do a for loop or just convert the string from tabs to spaces
					TabbedTextOut(item->hDC, x, y, menu_str, menu_str_character_cnt - 1, 0, NULL, x);
					//wprintf(L"%s\n", menu_str);
					free(menu_str);
					//TODO(fran): find a better function, this guy doesnt care  about alignment, only TextOut and ExtTextOut do, but, of course, both cant handle tabs //NOTE: the normal rendering seems to have very long tab spacing so maybe it uses TabbedTextOut with 0 and NULL as the tab params

					SelectClipRgn(item->hDC, restoreRegion);
					if (restoreRegion != NULL)
					{
						DeleteObject(restoreRegion);
					}

					if (menu_type.hSubMenu) { //Draw the submenu arrow
						HBITMAP mask = bmps.solid_arrow_right; //TODO(fran): parametric
						BITMAP bmpnfo; GetObject(mask, sizeof(bmpnfo), &bmpnfo); 
						Assert(bmpnfo.bmBitsPixel == 1);
						int img_max_x = GetSystemMetrics(SM_CXMENUCHECK);
						int img_max_y = RECTH(item->rcItem);
						int img_sz = roundNdown(bmpnfo.bmWidth, minimum(img_max_x, img_max_y));//HACK: instead use png + gdi+ + color matrices
						if (!img_sz) img_sz = bmpnfo.bmWidth; //More HACKs

						int bmp_left = item->rcItem.right - img_sz;
						int bmp_top = item->rcItem.top + (img_max_y - img_sz) / 2;
						int bmp_height = img_sz;
						int bmp_width = bmp_height;
						urender::draw_menu_mask(item->hDC, bmp_left, bmp_top, bmp_width, bmp_height, mask, 0, 0, bmpnfo.bmWidth, bmpnfo.bmHeight, colors.Img);//TODO(fran): parametric brush

						//Prevent windows from drawing what nobody asked it to draw
						//Many thanks to David Sumich https://www.codeguru.com/cpp/controls/menu/miscellaneous/article.php/c13017/Owner-Drawing-the-Submenu-Arrow.htm 
						ExcludeClipRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom);
					}
				}
			} break;
			case MFT_SEPARATOR:
			{
				const int separator_x_padding = 3;

				HBRUSH bk_br;
				if (state.menu) {
					MENUINFO mnfo{ sizeof(mnfo) }; mnfo.fMask = MIM_BACKGROUND;
					GetMenuInfo(state.menu, &mnfo);
					bk_br = mnfo.hbrBack; //TODO: unfinished trash
				}
				else bk_br = colors.CaptionBk;

				FillRect(item->hDC, &item->rcItem, bk_br);
				RECT separator_rc;
				separator_rc.top = item->rcItem.top + RECTH(item->rcItem) / 2;
				separator_rc.bottom = separator_rc.top + 1; //TODO(fran): fancier calc and position
				separator_rc.left = item->rcItem.left + separator_x_padding;
				separator_rc.right = item->rcItem.right - separator_x_padding;
				FillRect(item->hDC, &separator_rc, colors.ControlTxt);
				//TODO(fran): clipping
			}
			default: return DefWindowProc(hwnd, msg, wparam, lparam);
			}

			return TRUE;
		}
		default: return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	} break;
	case WM_COMMAND:
	{
		// Msgs from my controls
		switch (LOWORD(wparam))
		{
		case NC_RESTORE:
		{
			ShowWindow(state.wnd, SW_RESTORE);
			return 0;
		} break;
		case NC_MINIMIZE: //TODO(fran): move away from WM_COMMAND and start using WM_SYSCOMMAND, that probably means getting rid of the individual nc buttons and handling all myself like with the menubar
		{
			ShowWindow(state.wnd, SW_MINIMIZE);
			return 0;
		} break;
		case NC_MAXIMIZE:
		{
			WINDOWPLACEMENT p{ sizeof(p) }; GetWindowPlacement(state.wnd, &p);

			if (p.showCmd == SW_SHOWMAXIMIZED) ShowWindow(state.wnd, SW_RESTORE);
			else ShowWindow(state.wnd, SW_MAXIMIZE); //TODO(fran): maximize covers the whole screen, I dont want that, I want to leave the navbar visible. For this to be done automatically by windows we need the WS_MAXIMIZEBOX style, which decides to draw a maximize box when pressed, if we can hide that we are all set
			return 0;
		} break;
		case NC_CLOSE:
		{
			bool client_handled = SendMessage(state.client, WM_CLOSE, 0, 0);
			if (!client_handled) {
				DestroyWindow(state.wnd);
			}
			return 0;
		} break;
		default: return SendMessage(state.client, msg, wparam, lparam);
		}
	} break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_GETMINMAXINFO:
		//FIRST msg sent to the window
		//Sent when size or position are about to change
		//Can override default maximized size and position, and the tracking size which sets the limit for the minimum & max size your window can be resized to, NOTE: the sizes/positions are relative to the primary monitor, windows automatically adjusts this values for secondary ones
	{
#if 0
		RECT testrc{ 0,0,100,100 };//TODO(fran): since clearly this msg needed to be sent before WM_CREATE or WM_NCCREATE we dont have our state allocated yet, thanks to that and the fact this msg is called all the time afterwards we cant allocate here unless we add a boolean "initialized" or smth the like. For now we are gonna duplicate this code again, for the third time, ugly but we gotta do it
		RECT ncrc = testrc;
		AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE);
		MY_MARGINS margins;
		margins.cxLeftWidth = distance(testrc.left, ncrc.left);
		margins.cxRightWidth = distance(testrc.right, ncrc.right);
		margins.cyBottomHeight = distance(testrc.bottom, ncrc.bottom);
		margins.cyTopHeight = 0;

		//INFO: for OVERLAPPEDWINDOWs any value of minmax->ptMaxPosition.y above or equal to 1 means let me handle it, if 0 or negative windows hardcodes whatever it wants and takes into account the taskbar
		// for NON OVERLAPPEDWINDOWs you can set whatever you want but windows doesnt help, in this case you'll need to take into account the taskbar by hand

		//INFO: even though the sizing box for the top part of the window is embedded inside the caption area windows adds and offset of, for example, -8 on the top part too (minmax->ptMaxPosition.y) and actually everybody draws differently when maximized, eg by default the buttons are shorter, the title and menu seem to get a little closer, others like chrome draw the buttons a little bit below but dont bother with the tabs, they dont change the size of the caption area, maybe they make the tab shorter but still it goes up to the last pixel. In summary, we got a couple options

		//INFO: the plot thickens, as with all the very typical windows hackery you dont get all the info you need and thanks to that even programs like chrome do this wrong, if we put the taskbar on top chrome will still show the resizing mouse even though it doesnt allow for resizing, but the worst thing is that it shows ABOVE the taskbar, you can see the extra pixels cover part of the taskbar, visual studio on the other hand handles all this perfectly

		//INFO: the fact that you cant use the resize borders when maximized is cause the window is actually over sized, that's strange for two reasons, first what happens on multiple monitors? shouldnt the extra size show up in the others?, second is that the size offset is applied to every border, now one of those four is gonna clash with the taskbar, is the taskbar just hiding the fact that the window is below by forcing itself on top?

		printf("WM_GETMINMAXINFO\n");
		MINMAXINFO* minmax = (MINMAXINFO*)lparam;
		minmax->ptMaxPosition.y = 0;
		minmax->ptMaxSize.y -= 16;
		printf("left %d top %d right %d bottom %d width %d height %d\n", minmax->ptMaxPosition.x, minmax->ptMaxPosition.y, minmax->ptMaxSize.x, minmax->ptMaxSize.y, distance(minmax->ptMaxPosition.x, minmax->ptMaxSize.x), distance(minmax->ptMaxPosition.y, minmax->ptMaxSize.y));
		return 0;
#else
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	} break;
	case WM_ERASEBKGND://havent found a good use for this msg yet
	{
		return 1;
		//return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCMOUSEMOVE:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; ScreenToClient(state.wnd, &mouse);
		RECT menurc = calc_menu_rc(state);
		if (wparam == HTMENU || wparam == HTSYSMENU) {
			//Trackmouse to know when it leaves the nc area
			TRACKMOUSEEVENT trackmouse{ sizeof(trackmouse) };
			trackmouse.dwFlags = TME_NONCLIENT | TME_LEAVE;
			trackmouse.hwndTrack = state.wnd;
			TrackMouseEvent(&trackmouse);

			bool hit = false;
			for (int i = 0; i < state.menubar_itemcnt; i++) {
				if (test_pt_rc(mouse, state.menubar_items[i])) {
					state.menubar_mouseover_idx_from1 = i + 1;
					RedrawWindow(state.wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
					hit = true;
					break;
				}
			}
			if (!hit && state.menubar_mouseover_idx_from1) {
				state.menubar_mouseover_idx_from1 = 0;
				RedrawWindow(state.wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
			}
			return 0;
		}
		else {
			//check whether the menu has some item drawn as on mousehover, in which case we need to redraw without it
			if (state.menubar_mouseover_idx_from1) {
				state.menubar_mouseover_idx_from1 = 0;
				RedrawWindow(state.wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCMOUSELEAVE:
	{
		if (state.menubar_mouseover_idx_from1) {
			state.menubar_mouseover_idx_from1 = 0;
			RECT menurc = calc_menu_rc(state);
			RedrawWindow(state.wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCLBUTTONDOWN:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		if (handle_nclbuttondown(state, mouse, wparam)) return 0;
		else return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCRBUTTONUP:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		show_rclickmenu(state, mouse);
		return 0;
	} break;
	case WM_SYSKEYDOWN:
	{
		unsigned char vk = (unsigned char)wparam;
		bool vk_was_down = lparam & (1 << 30); //if true this is a repeated msg, otherwise it's the first time
		bool alt_is_down = lparam & (1 << 29);
		switch (vk) {
		case VK_F4:
		{
			if (!vk_was_down && alt_is_down) {
				PostMessage(state.wnd, WM_COMMAND, (WPARAM)MAKELONG(NC_CLOSE, 0), (LPARAM)state.btn_close);//TODO(fran): should I use WM_CLOSE?
			}
			return 0;
		} break;
		default:return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	} break;
	case WM_ENTERIDLE:
	{
		auto type = wparam;
		if (type == MSGF_MENU) {
			// We are currently in menu selection mode, a menu item from our menu bar is open
			// We should check if the mouse is hovering over any other menu items from our menu bar, in which case we should close the current menu and open that one
			POINT mouse; GetCursorPos(&mouse);
			handle_nclbuttondown(state, mouse, HTMENU);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	#ifdef _DEBUG_HWND_MESSAGES
	case WM_UAHDESTROYWINDOW: //Only called when the window is being destroyed, wparam=lparam=0
	case WM_UAHINITMENU: //After a SetMenu: lParam=some system generated HMENU with your HMENU stored in the ""unused"" member ; wParam=0 (this system HMENU uses the first byte from the top 32bits, no other HMENU does that)
	{
		//NOTE: for some reason, after SetMenu, this msg gets sent more than once after different events, but always with the same params. This does not depend on the number of menus/submenus it has, but it seems like SetMenu checks that your HMENU has at least one submenu, otherwise this msg doesnt get sent
		//NOTE: from tests I can say semi confidently that the menubar uses WM_SIZE for updating, at least sometimes
	}
	case WM_UAHDRAWMENU: //Sent for NON MF_OWNERDRAW menus
	case WM_UAHDRAWMENUITEM:
	case WM_UAHMEASUREMENUITEM:
	case WM_UAHNCPAINTMENUPOPUP:
	case WM_UAHUPDATE:
	{
		printf(msgToString(msg)); printf("\n\n\n\n\n");
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCLBUTTONUP:
	case WM_SYSCOMMAND://Menu and nc buttons related //TODO(fran): maybe I should use this for sending max,min,close
	case WM_CAPTURECHANGED://just a notif
	case WM_ENTERSIZEMOVE://window entered moving/sizing loop
	case WM_SIZING://sent while user is resizing, you can monitor and change size/pos of "drag" rectangle
	case WM_EXITSIZEMOVE:
	case WM_MOVING:
	case WM_NCRBUTTONDOWN:
	case WM_ENTERMENULOOP://notif
	case WM_INITMENU://sent before the menu becomes active, we can modify it before it is displayed <- TODO(fran)
	case WM_MENUSELECT://the user selected a menu item
	case WM_EXITMENULOOP://notif
	case WM_INITMENUPOPUP://sent before a submenu/popup menu is displayed, eg when the user clicks on an item from the menu bar, TODO(fran): here we can modify the menu before it is displayed
	case WM_ENTERIDLE://menu has no more msgs to process in its queue
	case WM_UNINITMENUPOPUP:
	case WM_CANCELMODE://system wants us to stop mouse tracking and similar
	case WM_DWMCOLORIZATIONCOLORCHANGED://why opening spy++ generated this msg? what u doing spy++?
	case WM_KEYUP://Non system key released
	case WM_QUERYOPEN://Sent when we are in icon form, aka minimized, and the user wants us restored //TODO(fran): we can use this
	case WM_SYNCPAINT://Interesting
	case WM_KEYDOWN://here we could parse keyboard shortcuts
	case WM_SETTINGCHANGE://also WM_WININICHANGE for older things (yes, same msg, slightly different params)
		//Sent to all top level windows after someone changes some windows settings with SystemParametersInfo
	case WM_IME_REQUEST://TODO(fran): looks like you can set up the ime wnd from here
	case WM_DEVICECHANGE: //TODO(fran): I keep getting this msg and Im sure that nothing has changed, problaly I secrewed up somewhere
	case WM_ENABLE://Sent when the enabled state is changing //TODO(fran): may be useful
	case WM_DISPLAYCHANGE:
	case WM_MOVE:
	case WM_GETTEXT:
	case WM_GETICON:
	case WM_DWMNCRENDERINGCHANGED:
	case WM_KILLFOCUS: //TODO(fran): in case I should stop tracking the mouse or things the like
	case WM_SETCURSOR:
	case WM_MOUSEACTIVATE:
	case WM_CONTEXTMENU://Interesting (also seems like the defproc of a child asks the parent for it)
	case WM_STYLECHANGING:
	case WM_STYLECHANGED:
	case WM_PARENTNOTIFY://Different notifs about your childs
	case WM_WINDOWPOSCHANGING:
	case WM_SHOWWINDOW:
	case WM_ACTIVATEAPP:
	case WM_ACTIVATE:
	case WM_IME_SETCONTEXT:
	case WM_IME_NOTIFY:
	case WM_GETOBJECT:
	case WM_SETFOCUS:
	case WM_NCPAINT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	#endif
	default:
	#ifdef _DEBUG_HWND_MESSAGES
		if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
			//IMPORTANT: a way to find out the name of 0xC000 through 0xFFFF messages
			//NOTE: the only one I see often is TaskbarButtonCreated, with a different id each time
			TCHAR arr[256];
			int res = GetClipboardFormatName(msg, arr, 256);
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		Assert(0);
	#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
	#endif
	}
	return 0;
}

void init_wndclass(HINSTANCE inst) {
	WNDCLASSEXW wcex{ sizeof(WNDCLASSEX) };
	auto icon = LoadIcon(inst, MAKEINTRESOURCE(ICO_LOGO));
	auto a = GetSystemMetrics(SM_CXSMICON);
	auto b = GetSystemMetrics(SM_CYSMICON);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = proc;
	wcex.cbWndExtra = sizeof(void*);
	wcex.hInstance = inst;
	wcex.hIcon = icon;
	wcex.hCursor = LoadCursor(nil, IDC_ARROW);
	wcex.lpszClassName = wndclass;
	wcex.hIconSm = icon;

	ATOM class_atom = RegisterClassExW(&wcex); Assert(class_atom);
}
struct pre_post_main {
	pre_post_main() { init_wndclass(GetModuleHandleW(nil)); }
	~pre_post_main() {}
} static const PREMAIN_POSTMAIN;
}