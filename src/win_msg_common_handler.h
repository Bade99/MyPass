#pragma once

static LRESULT handle_wm_mouseactivate() { return MA_ACTIVATE; }

static LRESULT handle_wm_setcursor(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam, HCURSOR cursor) {
	//DefWindowProc passes this to its parent to see if it wants to change the cursor settings, we'll make a decision, setting the mouse cursor, and halting proccessing so it stays like that
	//WM_SETCURSOR is sent after getting the result of WM_NCHITTEST, mouse is inside our window and mouse input is not being captured

	/* https://docs.microsoft.com/en-us/windows/win32/learnwin32/setting-the-cursor-image
		if we pass WM_SETCURSOR to DefWindowProc, the function uses the following algorithm to set the cursor image:
		1. If the window has a parent, forward the WM_SETCURSOR message to the parent to handle.
		2. Otherwise, if the window has a class cursor, set the cursor to the class cursor.
		3. If there is no class cursor, set the cursor to the arrow cursor.
	*/
	//NOTE: I think this is good enough for now
	if (cursor && LOWORD(lparam) == HTCLIENT) {
		SetCursor(cursor);
		return TRUE;
	}
	else return DefWindowProc(wnd, msg, wparam, lparam);
}

static LRESULT handle_wm_nchittest(HWND wnd, LPARAM lparam) {
	POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
	RECT r; GetWindowRect(wnd, &r);
	auto hittest = test_pt_rc(mouse, r) ? HTCLIENT : HTNOWHERE;
	return hittest;
}

static LRESULT handle_wm_measureitem(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam, HMENU top_level_menu = nil) {
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
			//Determine text space:
			TCHAR menu_str[64]; *menu_str = 0;
			i32 menu_str_char_cnt = GetMenuString((HMENU)item->itemData, item->itemID, menu_str, ARRAYSIZE(menu_str), MF_BYCOMMAND);

			HDC dc = GetDC(wnd); defer{ ReleaseDC(wnd, dc); };
			HFONT hfntPrev = (HFONT)SelectObject(dc, fonts.Menu); defer{ SelectObject(dc, hfntPrev); };//TODO(fran): theme
			int old_mapmode = GetMapMode(dc); SetMapMode(dc, MM_TEXT); defer{ SetMapMode(dc, old_mapmode); };
			WORD text_width = LOWORD(GetTabbedTextExtent(dc, menu_str, menu_str_char_cnt, 0, nil)); //TODO(fran): make common function for this and the one that does rendering, also look at how tabs work
			WORD space_width = LOWORD(GetTabbedTextExtent(dc, TEXT(" "), 1, 0, nil)); //a space at the beginning

			//Check if we are a "top level" menu
			item->itemWidth = text_width + ((HMENU)item->itemData == top_level_menu ? space_width * 2 : GetSystemMetrics(SM_CXMENUCHECK) + space_width);
			item->itemHeight = GetSystemMetrics(SM_CYMENU);

			return TRUE;
		} break;
		case MF_SEPARATOR:
		{
			item->itemHeight = 3;
			item->itemWidth = 1;
			return TRUE;
		} break;
		default: return DefWindowProc(wnd, msg, wparam, lparam);
		}
	}
	return DefWindowProc(wnd, msg, wparam, lparam);
}

static LRESULT handle_wm_drawitem(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam, HMENU top_level_menu = nil) {
	DRAWITEMSTRUCT* item = (DRAWITEMSTRUCT*)lparam;
	switch (wparam) {//wparam specifies the identifier of the control that needs painting
	case 0: //menu
	{
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
			if (item->itemState & ODS_GRAYED) {
				txt_br = colors.ControlTxt_Disabled;
				bk_br = colors.CaptionBk;
			}
			elif(item->itemState & ODS_SELECTED || item->itemState & ODS_HOTLIGHT /*needed for "top level" menus*/) //TODO(fran): ODS_CHECKED ODS_FOCUS
			{
				txt_br = colors.ControlTxt;
				bk_br = colors.ControlBkMouseOver;
			}
			else
			{
				txt_br = colors.ControlTxt;
				Assert((UINT)(UINT_PTR)item->hwndItem);
				//GetMenuInfo((HMENU)item->hwndItem, &mnfo); //We will not ask our "parent" hmenu since sometimes it decides it doesnt have the hbrush, we'll go straight to the menu bar, if there is one
				if (top_level_menu) {
					MENUINFO mnfo{ sizeof(mnfo) }; mnfo.fMask = MIM_BACKGROUND;
					GetMenuInfo(top_level_menu, &mnfo);
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
			if (item->hwndItem == (HWND)top_level_menu) { //If we are on the menu bar (then hwndItem, our parent, will be the same as top_level_menu)
				//we just want to draw the text, nothing more
				//TODO(fran): clean this huge if-else, very bug prone with things being set/initialized in different parts

				TCHAR menu_str[64]; *menu_str = 0;
				i32 menu_str_char_cnt = GetMenuString((HMENU)item->hwndItem, item->itemID, menu_str, ARRAYSIZE(menu_str), MF_BYCOMMAND);

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
				TextOut(item->hDC, xPos, yPos, menu_str, menu_str_char_cnt);
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
					HBITMAP hbmp = nil;
					if (menu_img.fState & MFS_CHECKED) {
						Assert(menu_img.hbmpChecked);
						hbmp = menu_img.hbmpChecked;
					}
					else {
						hbmp = menu_img.hbmpUnchecked;
					}
					if (hbmp) {
						BITMAP bitmap; GetObject(hbmp, sizeof(bitmap), &bitmap);

						int img_max_x = GetSystemMetrics(SM_CXMENUCHECK);
						int img_max_y = RECTH(item->rcItem);
						auto img_min_dim = minimum(img_max_x, img_max_y);
						//HACK: instead use png + gdi+ + color matrices
						int img_sz = (bitmap.bmBitsPixel == 1) ?
							roundNdown(bitmap.bmWidth, img_min_dim) : clamp(0, (i32)bitmap.bmWidth, img_min_dim);
						if (!img_sz) img_sz = img_min_dim; //More HACKs
						int bmp_height = img_sz;
						int bmp_width = bmp_height;
						int bmp_align_width = item->rcItem.left + (img_max_x + x_pad - bmp_width) / 2;
						int bmp_align_height = item->rcItem.top + (img_max_y - bmp_height) / 2;

						auto img_br = item->itemState & ODS_GRAYED ? txt_br : colors.Img;

						//TODO(fran): clipping
						if (bitmap.bmBitsPixel == 1)
							urender::draw_menu_mask(item->hDC, bmp_align_width, bmp_align_height, bmp_width, bmp_height, hbmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, img_br);//TODO(fran): parametric brush
						elif(bitmap.bmBitsPixel == 8)
							urender::draw_menu_mask8(item->hDC, bmp_align_width, bmp_align_height, bmp_width, bmp_height, hbmp, img_br);
					}
				}

				// Determine where to draw, leave space for a check mark and the extra 1 space
				x = item->rcItem.left;
				y = item->rcItem.top;
				x += GetSystemMetrics(SM_CXMENUCHECK) + x_pad;

				TCHAR menu_str[64]; *menu_str = 0;
				i32 menu_str_char_cnt = GetMenuString((HMENU)item->hwndItem, item->itemID, menu_str, ARRAYSIZE(menu_str), MF_BYCOMMAND);

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
				TabbedTextOut(item->hDC, x, y, menu_str, menu_str_char_cnt, 0, nil, x);

				SelectClipRgn(item->hDC, restoreRegion);
				if (restoreRegion) DeleteObject(restoreRegion);

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
			if (top_level_menu) {
				MENUINFO mnfo{ sizeof(mnfo) }; mnfo.fMask = MIM_BACKGROUND;
				GetMenuInfo(top_level_menu, &mnfo);
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
		default: return DefWindowProc(wnd, msg, wparam, lparam);
		}

		return TRUE;
	}
	default: return DefWindowProc(wnd, msg, wparam, lparam);
	}
}
