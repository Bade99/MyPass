#pragma once
#include <Windows.h>
#include <windowsx.h>
#include "unCap_Helpers.h"
#include "unCap_Math.h"
#include "unCap_Global.h"
#include "unCap_Renderer.h"
#include "unCap_button.h"
#include "resource.h" //TODO(fran): everything that we take from the resources must be parameterized
#include "windows_undoc.h"
#include "unCap_Serialization.h"
#include "unCap_Reflection.h"
#include "windows_msg_mapper.h"

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
#define UNCAPNC_MINIMIZE 100 //sent through WM_COMMAND
#define UNCAPNC_MAXIMIZE 101 //sent through WM_COMMAND
#define UNCAPNC_CLOSE 102 //sent through WM_COMMAND
#define UNCAPNC_RESTORE 103 //sent through WM_COMMAND

#define UNCAPNC_TIMER_MENUDELAY 0xf1 //A little delay before allowing the user to select a new menu, this prevents problems, eg the user trying to close the menu by selecting it again in the menu bar

//TODO(fran): look at SetWindowLong(hWnd, GWL_STYLE, currStyles | WS_MAXIMIZE); maybe that helps with maximizing correctly?

constexpr TCHAR unCap_wndclass_uncap_nc[] = TEXT("unCap_wndclass_uncap_nc"); //Non client uncap

struct unCapNcLpParam {//NOTE: pass a pointer to unCapNcLpParam to set up the client area, if client_class_name is null no client is created
	const TCHAR* client_class_name;
	void* client_lp_param;
};

struct MY_MARGINS
{
	int cxLeftWidth;
	int cxRightWidth;
	int cyTopHeight;
	int cyBottomHeight;
};

struct unCapNcProcState {
	HWND wnd;
	HWND client;
	//TODO(fran): might be good to store client and wnd rect, find the one place where we get this two. Then we wont have the code littered with those two calls

	MY_MARGINS nc;

	HWND btn_min, btn_max, btn_close;

	RECT rc_caption;
	SIZE caption_btn;
	bool active;

	RECT rc_icon;

	struct {//menu related
		HMENU menu; //The menu that contains all the other menus, the ones that go in the menu bar, and the ones inside of each of those 
					//IMPLEMENTATION: every submenu asks this one for the bk brush
		RECT menubar_items[10];//coords are relative to wnd client
		i32 menubar_itemcnt;
		i32 menubar_mouseover_idx_from1;//IMPLEMENTATION: 0=no item is on mouseover, we'll start from 1 so in case you search by position you'll need to subtract 1
		bool menu_on_delay;//NOTE: the delay is only needed for left click accessed menus //TODO(fran): this is quite a bit of a cheap hack solution
	};
};

unCapNcProcState* UNCAPNC_get_state(HWND uncapnc) {
	unCapNcProcState* state = (unCapNcProcState*)GetWindowLongPtr(uncapnc, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

void UNCAPNC_set_state(HWND uncapnc, unCapNcProcState* state) {
	SetWindowLongPtr(uncapnc, 0, (LONG_PTR)state);
}

void UNCAPNC_calc_caption(unCapNcProcState* state) {
	GetClientRect(state->wnd, &state->rc_caption);

	RECT testrc{ 0,0,100,100 }; RECT ncrc = testrc;
	AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE);//no menu

	state->rc_caption.bottom = distance(testrc.top, ncrc.top);

	state->caption_btn.cy = RECTHEIGHT(state->rc_caption); state->caption_btn.cx = state->caption_btn.cy * 16 / 9;;
}

RECT UNCAPNC_calc_nonclient_rc_from_client(RECT uncapcl_rc, BOOL has_menu) {
	RECT res = uncapcl_rc;
	AdjustWindowRect(&res, WS_OVERLAPPEDWINDOW, has_menu);//TODO(fran): standardize this or some other measure
	return res;
}

RECT UNCAPNC_calc_menu_rc(unCapNcProcState* state) {
	RECT rc{ 0 };
	if (state->menu) {
		RECT r; GetClientRect(state->wnd, &r);
		rc.left = r.left;
		rc.top = RECTHEIGHT(state->rc_caption);
		rc.right = r.right;
		rc.bottom = rc.top + GetSystemMetrics(SM_CYMENU);
	}
	return rc;
}

RECT UNCAPNC_calc_client_rc(unCapNcProcState* state) {
	RECT r; GetClientRect(state->wnd, &r);
	RECT menu_rc = UNCAPNC_calc_menu_rc(state);
	RECT rc{ r.left, RECTHEIGHT(state->rc_caption) + RECTHEIGHT(menu_rc), r.right, r.bottom };
	return rc;
}

RECT UNCAPNC_calc_btn_min_rc(unCapNcProcState* state) {
	RECT r; GetClientRect(state->wnd, &r);
	RECT rc{ r.right - 3 * state->caption_btn.cx, r.top, r.right - 2 * state->caption_btn.cx, r.top + state->caption_btn.cy };
	return rc;
}

RECT UNCAPNC_calc_btn_max_rc(unCapNcProcState* state) {
	RECT r; GetClientRect(state->wnd, &r);
	RECT rc{ r.right - 2 * state->caption_btn.cx, r.top, r.right - 1 * state->caption_btn.cx, r.top + state->caption_btn.cy };
	return rc;
}

RECT UNCAPNC_calc_btn_close_rc(unCapNcProcState* state) {
	RECT r; GetClientRect(state->wnd, &r);
	RECT rc{ r.right - 1 * state->caption_btn.cx, r.top, r.right - 0 * state->caption_btn.cx, r.top + state->caption_btn.cy };
	return rc;
}

HMENU UNCAPNC_get_menu(HWND uncapnc) {
	unCapNcProcState* state = UNCAPNC_get_state(uncapnc);
	return state->menu;
}

//NOTE:windows' menu bar is terrible, it doesnt care to update for nothing

//The user should call this instead of SetMenu, and UNCAPNC_get_menu instead of GetMenu
void UNCAPNC_set_menu(HWND uncapnc, HMENU menu) { //TODO(fran): set to NULL should clear the menu
	unCapNcProcState* state = UNCAPNC_get_state(uncapnc);

	if (menu) {
		state->menu = menu;

		MENUINFO mi{ sizeof(mi) };
		mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS; //NOTE: important to also use "apply to submenus", cause the nc area is bigger than 1px and it will look very claustrophobic
		//TODO(fran): MIM_MAXHEIGHT and other fMask params that we should set
		mi.hbrBack = state->active ? unCap_colors.CaptionBk : unCap_colors.CaptionBk_Inactive;
		SetMenuInfo(menu, &mi);

	}
	else {
		//TODO(fran): implement menu removal in case we already have something on state->menu
		state->menu = NULL;
	}
	RECT wndrc; GetWindowRect(state->wnd, &wndrc);
	//Update to accommodate for menu change
	MoveWindow(state->wnd, wndrc.left, wndrc.top, RECTWIDTH(wndrc), RECTHEIGHT(wndrc), FALSE);//NOTE: for some reason specifying TRUE for the last param doesnt force repainting
	RedrawWindow(state->wnd, NULL, NULL, RDW_INVALIDATE);

}

//NOTE: this function request for a UINT since the top 32 bits of an HMENU are not used, if you have an HMENU simply cast down (UINT)hmenu
bool UNCAPNC_is_on_menu_bar(unCapNcProcState* state, UINT menuitem) {
	int cnt = GetMenuItemCount(state->menu);
	bool res = false;
	for (int i = 0; i < cnt; i++) {
		HMENU submenu = GetSubMenu(state->menu, i);

		if (menuitem == (UINT)(UINT_PTR)submenu || menuitem == (UINT)(UINT_PTR)state->menu) {
			res = true;
			break;
		}
	}
	return res;
}

//Mouse in screen coords
void UNCAPNC_show_rclickmenu(unCapNcProcState* state, POINT mouse) {
	HMENU m = CreateMenu();
	HMENU subm = CreateMenu();//IMPORTANT: you need a MF_POPUP submenu for the menu wnd to be rendered properly, thanks https://www.codeproject.com/Questions/334598/Popup-Menu-Problem-is-not-working-properly
	AppendMenuW(m, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)subm, (LPCWSTR)m);

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, UNCAPNC_RESTORE, (LPCWSTR)subm);
	SetMenuItemString(subm, UNCAPNC_RESTORE, FALSE, RCS(LANG_NC_RESTORE));

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, UNCAPNC_MINIMIZE, (LPCWSTR)subm);
	SetMenuItemString(subm, UNCAPNC_MINIMIZE, FALSE, RCS(LANG_NC_MINIMIZE));

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, UNCAPNC_MAXIMIZE, (LPCWSTR)subm);
	SetMenuItemString(subm, UNCAPNC_MAXIMIZE, FALSE, RCS(LANG_NC_MAXIMIZE));

	AppendMenuW(subm, MF_SEPARATOR | MF_OWNERDRAW, 0, (LPCWSTR)subm);

	AppendMenuW(subm, MF_STRING | MF_OWNERDRAW, UNCAPNC_CLOSE, (LPCWSTR)subm);
	SetMenuItemString(subm, UNCAPNC_CLOSE, FALSE, RCS(LANG_NC_CLOSE));

	MENUINFO mi{ sizeof(mi) };
	mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
	mi.hbrBack = unCap_colors.CaptionBk;
	SetMenuInfo(m, &mi);

	//NOTE: using tpm_returncmd would be a quick and simple cheat to get past msg collision problems and the like
	//TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, mouse.x, mouse.y, 0, state->wnd, 0);
	TrackPopupMenuEx(subm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, mouse.x, mouse.y, state->wnd, 0);
	DestroyMenu(m);
}

void UNCAPNC_manual_maximize(unCapNcProcState* state) {//Maximize only works correctly for WM_OVERLAPPEDWINDOW, we gotta do it by hand
	WINDOWPLACEMENT old_placement; GetWindowPlacement(state->wnd, &old_placement);
	HMONITOR mon = MonitorFromWindow(state->wnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monnfo{ sizeof(monnfo) };
	BOOL res = GetMonitorInfo(mon, &monnfo);
	Assert(res);
	RECT work_rc = monnfo.rcWork;//NOTE: im in a 1 monitor setup and it's exactly the same as the other rc so idk when it's different. Strangely enough it still doesnt go over the taskbar area, so I guess there's a check inside the resizing code done by windows somewhere

	//TODO(fran):we're gonna have to store our current size to be able to manually restore if the user tries to move us or presses maximize again. SetWindowPlacement?
	//TODO(fran): gotta decide what we want to do with the resizing borders, the top and bottom ones are still user reachable, both visual studio and chrome dont allow resizing but chrome developers where a little lazy and didnt update the margins of the window, therefore on the bottom the mouse still changes to the resizing one even though they dont allow the resize
	work_rc.left -= state->nc.cxLeftWidth;
	work_rc.right += state->nc.cxRightWidth;
	work_rc.top -= state->nc.cyTopHeight;
	work_rc.bottom += state->nc.cyBottomHeight;
	MoveWindow(state->wnd, work_rc.left, work_rc.top, RECTWIDTH(work_rc), RECTHEIGHT(work_rc), TRUE);
	WINDOWPLACEMENT placement{ sizeof(placement) };
	placement.showCmd = SW_MAXIMIZE;
	placement.flags = WPF_ASYNCWINDOWPLACEMENT;
	placement.ptMaxPosition.x = work_rc.left;
	placement.ptMaxPosition.y = work_rc.top;
	placement.rcNormalPosition = old_placement.rcNormalPosition;//TODO(fran): for non WS_EX_TOOLWINDOW windows all this should be in workspace coords
	SetWindowPlacement(state->wnd, &placement);//TODO(fran): I just wanted to set the placement, not ask this guy to update my window too (maybe I can do some extra resizing to adjust the situation)
}

//TODO(fran): add & at the beginning of menu string names, that's how you trigger them by pressing Alt+key https://stackoverflow.com/questions/38338426/meaning-of-ampersand-in-rc-files
//TODO(fran): it'd also be interesting to see how it goes if we do SetMenu on the client
//TODO(fran): UNDO support for comment removal
//TODO(fran): DPI awareness
//TODO(fran): it'd be nice to have a way to implement good subclassing, eg letting the user assign clip regions where they can draw and we dont, things like that, more communication
LRESULT CALLBACK UncapNcProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//printf(msgToString(msg)); printf("\n");
	unCapNcProcState* state = UNCAPNC_get_state(hwnd);

	{
#if 0
		RECT rw; GetWindowRect(hwnd, &rw);
		printf("left %d top %d right %d bottom %d width %d height %d\n", rw.left, rw.top, rw.right, rw.bottom, RECTWIDTH(rw), RECTHEIGHT(rw));
#endif
	}

	constexpr int menu_delay_ms = 100;

	switch (msg)
	{
	case WM_UAHDESTROYWINDOW: //Only called when the window is being destroyed, wparam=lparam=0
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_UAHDRAWMENU: //Sent for NON MF_OWNERDRAW menus
	case WM_UAHDRAWMENUITEM:
	case WM_UAHMEASUREMENUITEM:
	case WM_UAHNCPAINTMENUPOPUP:
	case WM_UAHUPDATE:
	{
		printf(msgToString(msg)); printf("\n\n\n\n\n");
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_UAHINITMENU: //After a SetMenu: lParam=some system generated HMENU with your HMENU stored in the ""unused"" member ; wParam=0
							//NOTE: this system HMENU uses the first byte from the top 32bits, no other HMENU does that
	{
		//NOTE: for some reason, after SetMenu, this msg gets sent more than once after different events, but always with the same params. This does not depend on the number of menus/submenus it has, but it seems like SetMenu checks that your HMENU has at least one submenu, otherwise this msg doesnt get sent
		//NOTE: from tests I can say semi confidently that the menubar uses WM_SIZE for updating, at least sometimes
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_WINDOWPOSCHANGED: //TODO(fran): we can use this guy to replace WM_SIZE and WM_MOVE by not calling defwindowproc
	{
		UNCAPNC_calc_caption(state);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_PAINT://TODO(fran): we gotta offset the painting a few pixels down when maximized, the amount of things that will need an extra condition on calculation is pretty awful, maybe we should just handle maximizing ourselves
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state->wnd, &ps);

		if (rcs_overlap(state->rc_caption, ps.rcPaint)) {

			FillRect(dc, &state->rc_caption, state->active ? unCap_colors.CaptionBk : unCap_colors.CaptionBk_Inactive);

			SelectFont(dc, GetStockFont(DEFAULT_GUI_FONT));
			TCHAR title[256]; int sz = GetWindowText(state->wnd, title, ARRAYSIZE(title));
			TEXTMETRIC tm; GetTextMetrics(dc, &tm);


			HICON icon = (HICON)GetClassLongPtr(state->wnd, GCLP_HICONSM);
			int icon_height = (int)((float)tm.tmHeight * 1.5f);
			int icon_width = icon_height;
			int icon_align_height = (RECTHEIGHT(state->rc_caption) - icon_height) / 2;
			int icon_align_width = icon_align_height;
			state->rc_icon = rectWH(icon_align_height, icon_align_width, icon_width, icon_height);
			MYICON_INFO iconnfo = MyGetIconInfo(icon);
			//TODO(fran): the last pixels from the circle dont seem to get rendered
			urender::draw_icon(dc, icon_align_height, icon_align_width, icon_width, icon_height, icon, 0, 0, iconnfo.nWidth, iconnfo.nHeight);

			int yPos = (state->rc_caption.bottom + state->rc_caption.top - tm.tmHeight) / 2;
			HBRUSH txtbr = state->active ? unCap_colors.ControlTxt : unCap_colors.ControlTxt_Inactive;
			SetTextColor(dc, ColorFromBrush(txtbr)); SetBkMode(dc, TRANSPARENT);


			TextOut(dc, icon_align_width * 2 + icon_width, yPos, title, sz);

			HBRUSH btn_border, btn_bk, btn_fore, btn_bk_push, btn_bk_mouseover;
			if (state->active) {
				btn_border = unCap_colors.CaptionBk;
				btn_bk = unCap_colors.CaptionBk;
				btn_fore = unCap_colors.Img;
				btn_bk_push = unCap_colors.ControlBkPush;
				btn_bk_mouseover = unCap_colors.ControlBkMouseOver;
			}
			else {
				btn_border = unCap_colors.CaptionBk_Inactive;
				btn_bk = unCap_colors.CaptionBk_Inactive;
				btn_fore = unCap_colors.Img_Inactive;
				btn_bk_push = unCap_colors.ControlBkPush;
				btn_bk_mouseover = unCap_colors.ControlBkMouseOver;
			}
			UNCAPBTN_set_brushes(state->btn_close, TRUE, btn_border, btn_bk, btn_fore, btn_bk_push, btn_bk_mouseover);
			UNCAPBTN_set_brushes(state->btn_min, TRUE, btn_border, btn_bk, btn_fore, btn_bk_push, btn_bk_mouseover);
			UNCAPBTN_set_brushes(state->btn_max, TRUE, btn_border, btn_bk, btn_fore, btn_bk_push, btn_bk_mouseover);
		}

		//TODO(fran): move the menu up to get more screen real state, visual studio differentiates the menu items from the window title by rendering a darker square around the latter
		if (RECT menurc = UNCAPNC_calc_menu_rc(state); state->menu && rcs_overlap(menurc, ps.rcPaint)) {

			MENUINFO mi{ sizeof(mi) };
			mi.fMask = MIM_BACKGROUND;
			//TODO(fran): MIM_MAXHEIGHT and other fMask params that we should set
			GetMenuInfo(state->menu, &mi);

			//Set font and colors
			HBRUSH bkbr = mi.hbrBack;
			HBRUSH txtbr = state->active ? unCap_colors.ControlTxt : unCap_colors.ControlTxt_Inactive;
			COLORREF oldbkclr = SetBkColor(dc, ColorFromBrush(bkbr)); defer{ if (oldbkclr != CLR_INVALID)SetBkColor(dc, oldbkclr); };
			COLORREF oldtxtclr = SetTextColor(dc, ColorFromBrush(txtbr)); defer{ if (oldtxtclr != CLR_INVALID)SetTextColor(dc, oldtxtclr); };
			HFONT oldmenufont = (HFONT)SelectObject(dc, unCap_fonts.Menu); defer{ SelectObject(dc,oldmenufont); };

			//Paint background
			FillRect(dc, &menurc, bkbr);

			RECT menuitemrc = menurc;
			int menu_xpad = 4;
			menuitemrc.left += menu_xpad;

			//Store current item count
			state->menubar_itemcnt = GetMenuItemCount(state->menu);//NOTE: this guy returns -1 on errors, thats why the variable is i32
			Assert(state->menubar_itemcnt < ARRAYSIZE(state->menubar_items));//TODO(fran): linked list?

			//Find out what item, if any, is on mouse over
			i32 menubar_mouseover_idx = state->menubar_mouseover_idx_from1 - 1;

			//Clip the drawing region //Thanks https://stackoverflow.com/questions/3478180/correct-usage-of-getcliprgn //WINDOWS THIS MAKES NO SENSE!!!!!!!!!
			HRGN restoreRegion = CreateRectRgn(0, 0, 0, 0); if (GetClipRgn(dc, restoreRegion) != 1) { DeleteObject(restoreRegion); restoreRegion = NULL; }
			IntersectClipRect(dc, menuitemrc.left, menuitemrc.top, menurc.right, menurc.bottom);

			//Set bk brush for drawing text
			for (int i = 0; i < state->menubar_itemcnt; i++) {
				bool on_mouseover = (i == menubar_mouseover_idx);
				if (on_mouseover) {
					//Paint mouseover bk
					FillRect(dc, &state->menubar_items[i], unCap_colors.ControlBkMouseOver);//TODO(fran): im having to cheat a little bit here by using the rc from a previous painting, it could be outdated
					SetBkColor(dc, ColorFromBrush(unCap_colors.ControlBkMouseOver));
				}
				else {
					SetBkColor(dc, ColorFromBrush(bkbr));
				}

				//Get text lenght
				MENUITEMINFO mtxti{ sizeof(mtxti) };
				mtxti.fMask = MIIM_STRING;
				mtxti.dwTypeData = NULL;
				GetMenuItemInfo(state->menu, i, TRUE, &mtxti);
				//Get actual text
				UINT menu_str_character_cnt = mtxti.cch + 1 + 2; //includes null terminator and 2 extra spaces
				mtxti.cch = menu_str_character_cnt;
				TCHAR* menu_str = (TCHAR*)malloc(menu_str_character_cnt * sizeof(TCHAR)); defer{ free(menu_str); };
				menu_str[0] = TEXT(' ');//initial space
				mtxti.dwTypeData = menu_str + 1; //Offset for initial space
				GetMenuItemInfo(state->menu, i, TRUE, &mtxti);
				menu_str[menu_str_character_cnt - 2] = TEXT(' ');//ending space

				// Calculate vertical and horizontal position for the string so that it will be centered
				//NOTE: TabbedTextOut doesnt care for alignment (SetTextAlign)
				TEXTMETRIC mtm; GetTextMetrics(dc, &mtm);
				//int yPos = menuitemrc.top + (RECTHEIGHT(menuitemrc) - mtm.tmHeight) / 2;
				int yPos = (menuitemrc.bottom + menuitemrc.top - mtm.tmHeight) / 2;
				int xPos = menuitemrc.left;
				LONG txtsz = TabbedTextOut(dc, xPos, yPos, menu_str, menu_str_character_cnt - 1, 0, 0, xPos);
				WORD txtw = LOWORD(txtsz);

				//Store menu item rc
				state->menubar_items[i] = { menuitemrc.left,menuitemrc.top,menuitemrc.left + txtw,menuitemrc.bottom };

				menuitemrc.left += txtw;
			}
			SelectClipRgn(dc, restoreRegion); if (restoreRegion != NULL) DeleteObject(restoreRegion); //Restore old region

			RECT menuborder = menurc; menuborder.top = menuborder.bottom - 1;
			FillRect(dc, &menuborder, txtbr);//TODO(fran):parametric, not everyone might like this
		}
		EndPaint(state->wnd, &ps);

		return 0;
	} break;
	case WM_CREATE:
	{

#if 1
		//NOTE: we want WS_OVERLAPPEDWINDOW for the niceties on resizing, maximizing not obstructing the taskbar, scaling the window in the y axis when double clicking the top/bottom resizing border (unfortunately doesnt do the same for left and right <-TODO(fran): also it doesnt do it for top and bottom, is this done by hand by every program?), and similar. The one problem is that we'll need to draw differently when maximized, everything will have to be moved down a little
		//TODO(fran): check no strange things happen now we re-enabled OVERLAPPEDWINDOW, at least it doesnt seem to be creating the horrible max/min etc buttons now we dont use WS_OVERLAPPEDWINDOW in CreateWindow
		LONG_PTR  dwStyle = GetWindowLongPtr(state->wnd, GWL_STYLE);
		SetWindowLongPtr(state->wnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
#endif

		CREATESTRUCT* createnfo = (CREATESTRUCT*)lparam;

		unCapNcLpParam* lpParam = (unCapNcLpParam*)createnfo->lpCreateParams;

		SetWindowText(state->wnd, createnfo->lpszName); //NOTE: for handmade classes you have to manually call setwindowtext

		RECT btn_min_rc = UNCAPNC_calc_btn_min_rc(state);
		state->btn_min = CreateWindow(unCap_wndclass_button, TEXT(""), WS_CHILD | WS_VISIBLE | BS_BITMAP, btn_min_rc.left, btn_min_rc.top, RECTWIDTH(btn_min_rc), RECTHEIGHT(btn_min_rc), state->wnd, (HMENU)UNCAPNC_MINIMIZE, 0, 0);
		//UNCAPBTN_set_brushes(state->btn_min, TRUE, unCap_colors.CaptionBk, unCap_colors.CaptionBk, unCap_colors.ControlTxt, unCap_colors.ControlBkPush, unCap_colors.ControlBkMouseOver); //NOTE: now I do this on WM_PAINT, commenting this actually introduces a bug for the first ms of execution where the button might draw with no brushes first and then be asked to redraw with the brushes loaded, introducing at least one frame of flicker

		RECT btn_max_rc = UNCAPNC_calc_btn_max_rc(state);
		state->btn_max = CreateWindow(unCap_wndclass_button, TEXT(""), WS_CHILD | WS_VISIBLE | BS_BITMAP, btn_max_rc.left, btn_max_rc.top, RECTWIDTH(btn_max_rc), RECTHEIGHT(btn_max_rc), state->wnd, (HMENU)UNCAPNC_MAXIMIZE, 0, 0);
		//UNCAPBTN_set_brushes(state->btn_max, TRUE, unCap_colors.CaptionBk, unCap_colors.CaptionBk, unCap_colors.ControlTxt, unCap_colors.ControlBkPush, unCap_colors.ControlBkMouseOver);

		RECT btn_close_rc = UNCAPNC_calc_btn_close_rc(state);
		state->btn_close = CreateWindow(unCap_wndclass_button, TEXT(""), WS_CHILD | WS_VISIBLE | BS_BITMAP, btn_close_rc.left, btn_close_rc.top, RECTWIDTH(btn_close_rc), RECTHEIGHT(btn_close_rc), state->wnd, (HMENU)UNCAPNC_CLOSE, 0, 0);
		//UNCAPBTN_set_brushes(state->btn_close, TRUE, unCap_colors.CaptionBk, unCap_colors.CaptionBk, unCap_colors.ControlTxt, unCap_colors.ControlBkPush, unCap_colors.ControlBkMouseOver);

		HBITMAP bCross = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_CLOSE));//TODO(fran): let the user set this guys, store them in state
		HBITMAP bMax = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_MAX));
		HBITMAP bMin = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_MIN));
		SendMessage(state->btn_close, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bCross);
		SendMessage(state->btn_max, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bMax);
		SendMessage(state->btn_min, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bMin);

		//TODO(fran): create the icons, one idea is to ask windows to paint them in some dc, and then I can store the HBITMAP and re-use it all I want

		if (lpParam->client_class_name) {
			RECT rc = UNCAPNC_calc_client_rc(state);
			state->client = CreateWindow(lpParam->client_class_name, TEXT(""), WS_CHILD | WS_VISIBLE, rc.left, rc.top, RECTWIDTH(rc), RECTHEIGHT(rc), state->wnd, 0, 0, lpParam->client_lp_param);
		}
		return 0;
	} break;
	case WM_NCLBUTTONDBLCLK:
	{
		LRESULT hittest = (LRESULT)wparam;

		if (hittest == HTCAPTION) {
			POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			POINT cl_mouse = mouse; ScreenToClient(state->wnd, &cl_mouse);

			if (test_pt_rc(cl_mouse, state->rc_icon)) {
				PostMessage(state->wnd, WM_COMMAND, (WPARAM)MAKELONG(UNCAPNC_CLOSE, 0), (LPARAM)state->btn_close);//TODO(fran): should I use WM_CLOSE?
				return 0;
			}

			WINDOWPLACEMENT p{ sizeof(p) }; GetWindowPlacement(state->wnd, &p);

			if (p.showCmd == SW_SHOWMAXIMIZED) ShowWindow(state->wnd, SW_RESTORE);
			else {
#if 1
				//LONG_PTR  dwStyle = GetWindowLongPtr(state->wnd, GWL_STYLE);
				//SetWindowLongPtr(state->wnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);//TODO(fran): now im REALLY confused, I had to specifically take out WS_OVERLAPPEDWINDOW so it wouldnt paint over me and now it doesnt seem to, test more thoroughly //still though sizing isnt perfect, the top is still cut off
				ShowWindow(state->wnd, SW_MAXIMIZE);
#else
				UNCAPNC_manual_maximize(state);
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

			RECT rc_menu = UNCAPNC_calc_menu_rc(state);

			if (test_pt_rc(mouse, rc_top)) res = HTTOP;
			else if (test_pt_rc(mouse, rc_topleft)) res = HTTOPLEFT;
			else if (test_pt_rc(mouse, rc_topright)) res = HTTOPRIGHT;
			/*else if (test_pt_rc(mouse, close_btn)) res = HTCLOSE; //TODO(fran): request this buttons for their rc
			else if (test_pt_rc(mouse, max_btn)) res = HTMAXBUTTON;
			else if (test_pt_rc(mouse, min_btn)) res = HTMINBUTTON;*/
			else if (test_pt_rc(mouse, rc_menu)) res = HTSYSMENU;//HTMENU also works, but it seems a little slower on clicking for some reason //IMPORTANT: this is crucial for the default menu to detect clicks, otherwise no mouse input goes to it
			else if (test_pt_rc(mouse, state->rc_caption)) res = HTCAPTION;

		}

		//printf("%s\n", hittestToString(res));
		return res;
	} break;
	case WM_NCCALCSIZE:
	{
		RECT testrc{ 0,0,100,100 };
		RECT ncrc = testrc;
		AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE); //TODO(fran): somehow link this params with UNCAPNC_calc_caption

		UNCAPNC_calc_caption(state);//TODO(fran): find out which is the one and only magical msg where this can be put in

		MY_MARGINS margins;

		margins.cxLeftWidth = distance(testrc.left, ncrc.left);
		margins.cxRightWidth = distance(testrc.right, ncrc.right);
		margins.cyBottomHeight = distance(testrc.bottom, ncrc.bottom);
		margins.cyTopHeight = 0;

		state->nc = margins;

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
		RECT rc = UNCAPNC_calc_client_rc(state); MoveWindow(state->client, rc.left, rc.top, RECTWIDTH(rc), RECTHEIGHT(rc), TRUE);

		RECT btn_min_rc = UNCAPNC_calc_btn_min_rc(state); MoveWindow(state->btn_min, btn_min_rc.left, btn_min_rc.top, RECTWIDTH(btn_min_rc), RECTHEIGHT(btn_min_rc), TRUE);//TODO(fran): I dont really need to ask for repaint do I?
		RECT btn_max_rc = UNCAPNC_calc_btn_max_rc(state); MoveWindow(state->btn_max, btn_max_rc.left, btn_max_rc.top, RECTWIDTH(btn_max_rc), RECTHEIGHT(btn_max_rc), TRUE);
		RECT btn_close_rc = UNCAPNC_calc_btn_close_rc(state); MoveWindow(state->btn_close, btn_close_rc.left, btn_close_rc.top, RECTWIDTH(btn_close_rc), RECTHEIGHT(btn_close_rc), TRUE);

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
		if (state) {
			free(state);
			state = nullptr;
		}
	} break;
	case WM_NCCREATE:
	{
		CREATESTRUCT* create_nfo = (CREATESTRUCT*)lparam;
		//TODO(fran): check for minimize and maximize button requirements
		unCapNcProcState* st = (unCapNcProcState*)calloc(1, sizeof(unCapNcProcState));
		Assert(st);
		UNCAPNC_set_state(hwnd, st);
		st->wnd = hwnd;
		return TRUE;
	} break;
	case WM_NCACTIVATE:
	{
		//So basically this guy is another WM_NCPAINT, just do exactly the same, but also here we have the option to paint slightly different if we are deactivated, so the user can see the change
		BOOL active = (BOOL)wparam; //Indicates active or inactive state for a title bar or icon that needs to be changed
		state->active = active;
		HRGN opt_upd_rgn = (HRGN)lparam; // handle to optional update region for the nonclient area. If set to -1, do nothing
		if (state->menu) {
			MENUINFO mi{ sizeof(mi) };
			mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS; //TODO(fran): MIM_APPLYTOSUBMENUS is garbage, it only goes one level down, just terrible, make my own
			//TODO(fran): MIM_MAXHEIGHT and other fMask params that we should set

			mi.hbrBack = state->active ? unCap_colors.CaptionBk : unCap_colors.CaptionBk_Inactive;

			SetMenuInfo(state->menu, &mi);
		}

		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
		return TRUE; //NOTE: it says we can return FALSE to "prevent the change"
	} break;
	//NOTE: WM_NCPAINT: check https://chromium.googlesource.com/chromium/chromium/+/5db69ae220c803e9c1675219b5cc5766ea3bb698/chrome/views/window.cc they block drawing so windows doesnt draw on top of them, cause the non client area is also painted in other msgs like settext
	//also check https://social.msdn.microsoft.com/Forums/windows/en-US/a407591a-4b1e-4adc-ab0b-3c8b3aec3153/the-evil-wmncpaint?forum=windowsuidevelopment I took the implementation from there, but there's also two others I can try

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
				HFONT hfntPrev = (HFONT)SelectObject(dc, unCap_fonts.Menu);//TODO(fran): parametric
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
				//if (UNCAPNC_is_on_menu_bar(state, item->itemID)) { //Check if we are a "top level" menu
				if ((HMENU)item->itemData == state->menu) { //Check if we are a "top level" menu
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
	case WM_DRAWITEM: //TODO(fran): goes in uncapcl
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

				COLORREF clrPrevText, clrPrevBkgnd;
				HFONT hfntPrev;
				int x, y;

				// Set the appropriate foreground and background colors. 
				HBRUSH txt_br, bk_br;
				if (item->itemState & ODS_SELECTED || item->itemState & ODS_HOTLIGHT /*needed for "top level" menus*/) //TODO(fran): ODS_CHECKED ODS_FOCUS
				{
					txt_br = unCap_colors.ControlTxt;
					bk_br = unCap_colors.ControlBkMouseOver;
				}
				else
				{
					txt_br = unCap_colors.ControlTxt;
					Assert((UINT)(UINT_PTR)item->hwndItem);
					//GetMenuInfo((HMENU)item->hwndItem, &mnfo); //We will not ask our "parent" hmenu since sometimes it decides it doesnt have the hbrush, we'll go straight to the menu bar, if there is one
					if (state->menu) {
						MENUINFO mnfo{ sizeof(mnfo) }; mnfo.fMask = MIM_BACKGROUND;
						GetMenuInfo(state->menu, &mnfo);
						bk_br = mnfo.hbrBack;
					}
					else bk_br = unCap_colors.CaptionBk;

				}
				clrPrevText = SetTextColor(item->hDC, ColorFromBrush(txt_br));//TODO(fran): separate menu brushes
				clrPrevBkgnd = SetBkColor(item->hDC, ColorFromBrush(bk_br));

				if (item->itemAction & ODA_DRAWENTIRE || item->itemAction & ODA_SELECT) { //Draw background
					FillRect(item->hDC, &item->rcItem, bk_br);
					//printf("PAINTED BK: %#08X\n", ColorFromBrush(bk_br));
				}

				// Select the font and draw the text. 
				hfntPrev = (HFONT)SelectObject(item->hDC, unCap_fonts.Menu);//TODO(fran): parametric

				WORD x_pad = LOWORD(GetTabbedTextExtent(item->hDC, TEXT(" "), 1, 0, NULL)); //an extra 1 space before drawing text (for not top level menus)
				//printf("item->hwndItem=%#016x GetMenu(hwnd)=%#016x state->menu=%#016x\n",item->hwndItem,GetMenu(hwnd),state->menu);
				//if (item->hwndItem == (HWND)GetMenu(hwnd)) { //If we are a "top level" menu
				if (item->hwndItem == (HWND)state->menu) { //If we are on the menu bar (then hwndItem, our parent, will be the same as state->menu)
				//Assert(GetMenu(hwnd) == state->menu);
				//if (UNCAPNC_is_on_menu_bar(state, (UINT)item->hwndItem)) { //If we are a "top level" menu

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

							if (bitmap.bmBitsPixel == 1) {

								int img_max_x = GetSystemMetrics(SM_CXMENUCHECK);
								int img_max_y = RECTHEIGHT(item->rcItem);
								int img_sz = roundNdown(bitmap.bmWidth, min(img_max_x, img_max_y));//HACK: instead use png + gdi+ + color matrices
								if (!img_sz)img_sz = bitmap.bmWidth; //More HACKs
								int bmp_height = img_sz;
								int bmp_width = bmp_height;
								int bmp_align_width = item->rcItem.left + (img_max_x + x_pad - bmp_width) / 2;
								int bmp_align_height = item->rcItem.top + (img_max_y - bmp_height) / 2;

								//NOTE: for some insane and nonsensical reason we cant use MaskBlt here, so no urender::draw_mask.
								urender::draw_menu_mask(item->hDC, bmp_align_width, bmp_align_height, bmp_width, bmp_height, hbmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, unCap_colors.Img);//TODO(fran): parametric brush
								//TODO(fran): clipping
							}
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
						HBITMAP mask = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_RIGHTARROW)); //TODO(fran): parametric
						defer{ DeleteBitmap(mask); };
						BITMAP bmpnfo; GetObject(mask, sizeof(bmpnfo), &bmpnfo); Assert(bmpnfo.bmBitsPixel == 1);
						int img_max_x = GetSystemMetrics(SM_CXMENUCHECK);
						int img_max_y = RECTHEIGHT(item->rcItem);
						int img_sz = roundNdown(bmpnfo.bmWidth, min(img_max_x, img_max_y));//HACK: instead use png + gdi+ + color matrices
						if (!img_sz) img_sz = bmpnfo.bmWidth; //More HACKs

						int bmp_left = item->rcItem.right - img_sz;
						int bmp_top = item->rcItem.top + (img_max_y - img_sz) / 2;
						int bmp_height = img_sz;
						int bmp_width = bmp_height;
						urender::draw_menu_mask(item->hDC, bmp_left, bmp_top, bmp_width, bmp_height, mask, 0, 0, bmpnfo.bmWidth, bmpnfo.bmHeight, unCap_colors.Img);//TODO(fran): parametric brush

						//Prevent windows from drawing what nobody asked it to draw
						//Many thanks to David Sumich https://www.codeguru.com/cpp/controls/menu/miscellaneous/article.php/c13017/Owner-Drawing-the-Submenu-Arrow.htm 
						ExcludeClipRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom);
					}
				}
				// Restore the original font and colors. 
				SelectObject(item->hDC, hfntPrev);
				SetTextColor(item->hDC, clrPrevText);
				SetBkColor(item->hDC, clrPrevBkgnd);
			} break;
			case MFT_SEPARATOR:
			{
				const int separator_x_padding = 3;

				HBRUSH bk_br;
				if (state->menu) {
					MENUINFO mnfo{ sizeof(mnfo) }; mnfo.fMask = MIM_BACKGROUND;
					GetMenuInfo(state->menu, &mnfo);
					bk_br = mnfo.hbrBack; //TODO: unfinished trash
				}
				else bk_br = unCap_colors.CaptionBk;

				FillRect(item->hDC, &item->rcItem, bk_br);
				RECT separator_rc;
				separator_rc.top = item->rcItem.top + RECTHEIGHT(item->rcItem) / 2;
				separator_rc.bottom = separator_rc.top + 1; //TODO(fran): fancier calc and position
				separator_rc.left = item->rcItem.left + separator_x_padding;
				separator_rc.right = item->rcItem.right - separator_x_padding;
				FillRect(item->hDC, &separator_rc, unCap_colors.ControlTxt);
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
		//TODO(fran): reserved msg range for UNCAPNC

		// Msgs from my controls
		switch (LOWORD(wparam))
		{
		case UNCAPNC_RESTORE:
		{
			ShowWindow(state->wnd, SW_RESTORE);
			return 0;
		} break;
		case UNCAPNC_MINIMIZE: //TODO(fran): move away from WM_COMMAND and start using WM_SYSCOMMAND, that probably means getting rid of the individual nc buttons and handling all myself like with the menubar
		{
			ShowWindow(state->wnd, SW_MINIMIZE);
			return 0;
		} break;
		case UNCAPNC_MAXIMIZE:
		{
			WINDOWPLACEMENT p{ sizeof(p) }; GetWindowPlacement(state->wnd, &p);

			if (p.showCmd == SW_SHOWMAXIMIZED) ShowWindow(state->wnd, SW_RESTORE);
			else ShowWindow(state->wnd, SW_MAXIMIZE); //TODO(fran): maximize covers the whole screen, I dont want that, I want to leave the navbar visible. For this to be done automatically by windows we need the WS_MAXIMIZEBOX style, which decides to draw a maximize box when pressed, if we can hide that we are all set
			return 0;
		} break;
		case UNCAPNC_CLOSE:
		{
			bool client_handled = SendMessage(state->client, WM_CLOSE, 0, 0);
			if (!client_handled) {
				DestroyWindow(state->wnd);
			}
			return 0;
		} break;
		default: return SendMessage(state->client, msg, wparam, lparam);
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
	case WM_STYLECHANGING:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_STYLECHANGED:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_PARENTNOTIFY://Different notifs about your childs
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_WINDOWPOSCHANGING:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SHOWWINDOW:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ACTIVATEAPP:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ACTIVATE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_SETCONTEXT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_NOTIFY:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETOBJECT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SETFOCUS:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCPAINT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ERASEBKGND://havent found a good use for this msg yet
	{
		return 1;
		//return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOVE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETTEXT:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETICON:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DWMNCRENDERINGCHANGED:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_KILLFOCUS: //TODO(fran): in case I should stop tracking the mouse or things the like
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SETCURSOR:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCMOUSEMOVE:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; ScreenToClient(state->wnd, &mouse);
		RECT menurc = UNCAPNC_calc_menu_rc(state);
		if (wparam == HTMENU || wparam == HTSYSMENU) {
			//Trackmouse to know when it leaves the nc area
			TRACKMOUSEEVENT trackmouse{ sizeof(trackmouse) };
			trackmouse.dwFlags = TME_NONCLIENT | TME_LEAVE;
			trackmouse.hwndTrack = state->wnd;
			TrackMouseEvent(&trackmouse);

			bool hit = false;
			for (int i = 0; i < state->menubar_itemcnt; i++) {
				if (test_pt_rc(mouse, state->menubar_items[i])) {
					state->menubar_mouseover_idx_from1 = i + 1;
					RedrawWindow(state->wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
					hit = true;
					break;
				}
			}
			if (!hit && state->menubar_mouseover_idx_from1) {
				state->menubar_mouseover_idx_from1 = 0;
				RedrawWindow(state->wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
			}
			return 0;
		}
		else {
			//check whether the menu has some item drawn as on mousehover, in which case we need to redraw without it
			if (state->menubar_mouseover_idx_from1) {
				state->menubar_mouseover_idx_from1 = 0;
				RedrawWindow(state->wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCMOUSELEAVE:
	{
		if (state->menubar_mouseover_idx_from1) {
			state->menubar_mouseover_idx_from1 = 0;
			RECT menurc = UNCAPNC_calc_menu_rc(state);
			RedrawWindow(state->wnd, &menurc, NULL, RDW_INVALIDATE);//TODO(fran): we only need to redraw the menu rc
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEACTIVATE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CONTEXTMENU://Interesting (also seems like the defproc of a child asks the parent for it)
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_TIMER:
	{
		WPARAM timerID = wparam;
		switch (timerID) {
		case UNCAPNC_TIMER_MENUDELAY:
		{
			state->menu_on_delay = false;
			KillTimer(state->wnd, timerID);
			return 0;
		} break;
		default: return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	} break;
	case WM_NCLBUTTONDOWN:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		POINT cl_mouse = mouse; ScreenToClient(state->wnd, &cl_mouse);
		if (wparam == HTMENU || wparam == HTSYSMENU) {//The menu was clicked, NOTE: for menus we dont wait for buttonup
			if (!state->menu_on_delay) {
				for (int i = 0; i < state->menubar_itemcnt; i++) {

					if (test_pt_rc(cl_mouse, state->menubar_items[i])) {
						HMENU submenu = GetSubMenu(state->menu, i);
						if (submenu) {
							POINT menupt{ state->menubar_items[i].left,state->menubar_items[i].bottom - 1 }; ClientToScreen(state->wnd, &menupt);
							TrackPopupMenu(submenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, menupt.x, menupt.y, 0, state->wnd, 0);
							//Set menu delay to stop possible reopening
							state->menu_on_delay = true;
							SetTimer(state->wnd, UNCAPNC_TIMER_MENUDELAY, menu_delay_ms, NULL);
							//TODO(fran): here we could pass our client wnd as the owner of the menu
							//TODO(fran): TPM_RETURNCMD could be interesting
							//TODO(fran): TPM_RECURSE ?
							break;
						}
					}
				}
			}
			return 0;
		}
		else if (wparam == HTCAPTION) {
			if (!state->menu_on_delay) {
				if (test_pt_rc(cl_mouse, state->rc_icon)) {
					POINT mpos{ state->rc_icon.left,state->rc_icon.bottom }; ClientToScreen(state->wnd, &mpos);
					UNCAPNC_show_rclickmenu(state, mpos);
					//Set menu delay to stop possible reopening
					state->menu_on_delay = true;
					SetTimer(state->wnd, UNCAPNC_TIMER_MENUDELAY, menu_delay_ms, NULL);
					return 0;
				}
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCLBUTTONUP:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SYSCOMMAND://Menu and nc buttons related //TODO(fran): maybe I should use this for sending max,min,close
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CAPTURECHANGED://just a notif
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ENTERSIZEMOVE://window entered moving/sizing loop
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SIZING://sent while user is resizing, you can monitor and change size/pos of "drag" rectangle
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_EXITSIZEMOVE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOVING:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCRBUTTONDOWN:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCRBUTTONUP:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		UNCAPNC_show_rclickmenu(state, mouse);
		return 0;
	} break;
	case WM_ENTERMENULOOP://notif
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_INITMENU://sent before the menu becomes active, we can modify it before it is displayed <- TODO(fran)
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MENUSELECT://the user selected a menu item
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_EXITMENULOOP://notif
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_INITMENUPOPUP://sent before a submenu/popup menu is displayed, eg when the user clicks on an item from the menu bar, TODO(fran): here we can modify the menu before it is displayed
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ENTERIDLE://menu has no more msgs to process in its queue
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_UNINITMENUPOPUP:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CANCELMODE://system wants us to stop mouse tracking and similar
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DWMCOLORIZATIONCOLORCHANGED://why opening spy++ generated this msg? what u doing spy++?
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_KEYUP://Non system key released
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_QUERYOPEN://Sent when we are in icon form, aka minimized, and the user wants us restored //TODO(fran): we can use this
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SYNCPAINT://Interesting
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_KEYDOWN://here we could parse keyboard shortcuts
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SETTINGCHANGE://also WM_WININICHANGE for older things (yes, same msg, slightly different params)
		//Sent to all top level windows after someone changes some windows settings with SystemParametersInfo
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_REQUEST://TODO(fran): looks like you can set up the ime wnd from here
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_DEVICECHANGE: //TODO(fran): I keep getting this msg and Im sure that nothing has changed, problaly I secrewed up somewhere
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_ENABLE://Sent when the enabled state is changing
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);//TODO(fran): may be useful
	} break;
	case WM_SYSKEYDOWN:
	{
		unsigned char vk = (unsigned char)wparam;
		bool vk_was_down = lparam & (1 << 30); //if true this is a repeated msg, otherwise it's the first time
		bool alt_is_down = lparam & (1<<29);
		switch (vk) {
		case VK_F4:
		{
			if (!vk_was_down && alt_is_down) {
				PostMessage(state->wnd, WM_COMMAND, (WPARAM)MAKELONG(UNCAPNC_CLOSE, 0), (LPARAM)state->btn_close);//TODO(fran): should I use WM_CLOSE?
			}
			return 0;
		} break;
		default:return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	} break;
	case WM_DISPLAYCHANGE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	default:
		if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
			//IMPORTANT: a way to find out the name of 0xC000 through 0xFFFF messages
			//NOTE: the only one I see often is TaskbarButtonCreated, with a different id each time
			TCHAR arr[256];
			int res = GetClipboardFormatName(msg, arr, 256);
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
#ifdef _DEBUG
			Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	}
	return 0;
}

ATOM init_wndclass_unCap_uncapnc(HINSTANCE inst) {
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = UncapNcProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(unCapNcProcState*);
	wcex.hInstance = inst;
	wcex.hIcon = LoadIcon(inst, MAKEINTRESOURCE(MYPASS_ICO_LOGO)); //TODO(fran): LoadImage to choose the best size
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = unCap_wndclass_uncap_nc;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(MYPASS_ICO_LOGO)); //TODO(fran): LoadImage to choose the best size

	ATOM class_atom = RegisterClassExW(&wcex);
	Assert(class_atom);
	return class_atom;
}