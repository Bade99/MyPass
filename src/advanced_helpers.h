#pragma once
#include "tooltip.h"


/**
  * Messagebox
  */

namespace MBP {
	//MessageBox Placement
	enum MBP {
		left   = 1 << 1,
		top    = 1 << 2,
		right  = 1 << 3,
		bottom = 1 << 4,
		center = 1 << 5,
	};
}
struct MsgBoxPlacement { 
	HWND relative_to; multiflag<MBP::MBP> flags; bool processed; 

	//INFO: flags defaults: left (if left or right or center isnt selected) and top (if top or bottom or center isnt selected)
	void set(HWND relative_to, multiflag<MBP::MBP> flags) {
		//Set defaults
		if (!(flags & MBP::left || flags & MBP::right || flags & MBP::center)) flags |= MBP::left;
		if (!(flags & MBP::top || flags & MBP::bottom || flags & MBP::center)) flags |= MBP::top;

		this->relative_to = relative_to;
		this->flags = flags;
		this->processed = false;
	}

} static msgbox_placement{ 0 };
static LRESULT CALLBACK __msgbox_hook(int code, WPARAM wparam, LPARAM lparam)
{
	//Thanks https://stackoverflow.com/questions/1530561/set-location-of-messagebox

	if (code == HCBT_CREATEWND)
	{
		CREATESTRUCT* pcs = ((CBT_CREATEWND*)lparam)->lpcs;

		if (auto& placement = msgbox_placement; ((pcs->style & WS_DLGFRAME) || (pcs->style & WS_POPUP)) && !placement.processed)
		{
			placement.processed = true;

			HWND wnd = (HWND)wparam;//Msgbox

			RECT rw; GetWindowRect(placement.relative_to, &rw);
			int rel_x = rw.left;
			int rel_y = rw.top;
			int rel_w = RECTW(rw);
			int rel_h = RECTH(rw);
			POINT p{ 0 };//msgbox's new x and y

			if (placement.flags & MBP::center) {
				p.x = rel_x + rel_w / 2 - pcs->cx / 2;
				p.y = rel_y + rel_h / 2 - pcs->cy / 2;
			}

			if (placement.flags & MBP::top) {
				p.y = rel_y;
			}

			if (placement.flags & MBP::bottom) {
				p.y = rel_y + rel_h - pcs->cy;
			}

			if (placement.flags & MBP::left) {
				p.x = rel_x;
			}

			if (placement.flags & MBP::right) {
				p.x = rel_x + rel_w - pcs->cx;
			}

			//NOTE: this may or may not work, depends on the window, but I do think it always works for msgboxes
			pcs->x = p.x;
			pcs->y = p.y;
		}
	}

	return CallNextHookEx(0 /*NOTE: this param was necessary on the times of win95, not anymore*/, code, wparam, lparam);
}

static int CustomMessageBox(HWND relative_to, const cstr* lpText, const cstr* lpCaption, UINT uType, multiflag<MBP::MBP> placement) {
	msgbox_placement.set(relative_to, placement);
	HHOOK hook_proc = SetWindowsHookEx(WH_CBT, __msgbox_hook, 0, GetCurrentThreadId()); defer{ UnhookWindowsHookEx(hook_proc); };
	int res = MessageBox(relative_to, lpText, lpCaption, uType);
	return res;
}


/**
  * Bitmap
  */

HBITMAP flip_bitmap(HBITMAP source_bitmap, bool flip_horizontal, bool flip_vertical) {
	Assert(flip_horizontal || flip_vertical);
	if (!source_bitmap) return nil;

	DIBSECTION dib{};
	if (!GetObject(source_bitmap, sizeof(dib), &dib)) return nil;
	Assert(dib.dsBm.bmBitsPixel == 8);
	if (!dib.dsBm.bmBits || dib.dsBm.bmBitsPixel != 8) return nil;

	const int width = dib.dsBm.bmWidth;
	const int height = dib.dsBm.bmHeight;
	const int stride = dib.dsBm.bmWidthBytes;

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 8;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* destination_bits = nil;
	HDC screen_dc = GetDC(nil);
	HBITMAP destination_bitmap = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS, &destination_bits, nil, 0);
	ReleaseDC(nil, screen_dc);
	if (!destination_bitmap || !destination_bits) return nil;

	const auto* src = (const u8*)(dib.dsBm.bmBits);
	auto* dst = (u8*)(destination_bits);

	for (int y = 0; y < height; y++) {
		const int source_y = flip_vertical ? (height - 1 - y) : y;

		const auto* source_row = src + source_y * stride;
		auto* destination_row = dst + y * stride;

		if (!flip_horizontal) memcpy(destination_row, source_row, stride);
		else {
			for (int x = 0; x < width; x++) destination_row[x] = source_row[width - 1 - x];
		}
	}

	return destination_bitmap;
}

enum class Rotation { clockwise_90, clockwise_180, clockwise_270 };
static HBITMAP rotate_bitmap8(HBITMAP source_bitmap, Rotation rotation) {
	if (!source_bitmap) return nil;

	DIBSECTION source{};
	if (!GetObject(source_bitmap, sizeof(source), &source) || !source.dsBm.bmBits ||
		source.dsBm.bmBitsPixel != 8 || source.dsBm.bmWidth <= 0 || source.dsBm.bmHeight <= 0) return nil;

	i32 source_width = source.dsBm.bmWidth;
	i32 source_height = source.dsBm.bmHeight;
	i32 destination_width, destination_height;
	switch (rotation) {
	case Rotation::clockwise_180:
		destination_width = source_width;
		destination_height = source_height;
		break;
	case Rotation::clockwise_90:
	case Rotation::clockwise_270:
		destination_width = source_height;
		destination_height = source_width;
		break;
	default: return nil;
	}

	BITMAPINFOHEADER info{};
	info.biSize = sizeof(info);
	info.biWidth = destination_width;
	info.biHeight = destination_height;
	info.biPlanes = 1;
	info.biBitCount = 8;
	info.biCompression = BI_RGB;

	void* destination_bits = nil;
	HDC screen_dc = GetDC(nil);
	HBITMAP destination_bitmap = CreateDIBSection(screen_dc, (BITMAPINFO*)&info, DIB_RGB_COLORS, &destination_bits, nil, 0);
	ReleaseDC(nil, screen_dc);
	if (!destination_bitmap || !destination_bits) return nil;

	DIBSECTION destination{};
	if (!GetObject(destination_bitmap, sizeof(destination), &destination)) {
		DeleteObject(destination_bitmap);
		return nil;
	}
	const u8* source_bits = (const u8*)source.dsBm.bmBits;
	u8* destination_pixels = (u8*)destination_bits;
	bool source_bottom_up = source.dsBmih.biHeight > 0;
	for (i32 source_y = 0; source_y < source_height; ++source_y) {
		i32 source_row = source_bottom_up ? source_height - 1 - source_y : source_y;
		for (i32 source_x = 0; source_x < source_width; ++source_x) {
			i32 destination_x, destination_y;
			switch (rotation) {
			case Rotation::clockwise_90:
				destination_x = source_height - 1 - source_y;
				destination_y = source_x;
				break;
			case Rotation::clockwise_180:
				destination_x = source_width - 1 - source_x;
				destination_y = source_height - 1 - source_y;
				break;
			case Rotation::clockwise_270:
				destination_x = source_y;
				destination_y = source_width - 1 - source_x;
				break;
			default: Assert(0); DeleteObject(destination_bitmap); return nil;
			}
			i32 destination_row = destination_height - 1 - destination_y;
			destination_pixels[destination_row * destination.dsBm.bmWidthBytes + destination_x] =
				source_bits[source_row * source.dsBm.bmWidthBytes + source_x];
		}
	}

	return destination_bitmap;
}


/**
  * Menu
  */

static bool append_popup_to_menu(HMENU menu, HMENU submenu, u32 msg_id) {
	if (!AppendMenu(menu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)submenu, (cstr*)menu)) return false;
	return SetMenuItemString(menu, GetMenuItemCount(menu) - 1, TRUE, RCS(msg_id));
}

static bool append_popup_to_menu(HMENU menu, HMENU submenu, u32 msg_id, HBITMAP img, HBITMAP checked_img = nil) {
	if (!append_popup_to_menu(menu, submenu, msg_id)) return false;
	return SetMenuItemBitmaps(menu, (UINT)(UINT_PTR)submenu, MF_BYCOMMAND, img, checked_img);
}

static bool append_item_to_menu(HMENU menu, u32 item_id, const cstr* label, HBITMAP img, HBITMAP checked_img = nil) {
	bool res;
	res = AppendMenu(menu, MF_STRING | MF_OWNERDRAW, item_id, (cstr*)menu);
	res = SetMenuItemString(menu, item_id, FALSE, label);
	res = SetMenuItemBitmaps(menu, item_id, MF_BYCOMMAND, img, checked_img);
	return res;
}

static bool append_item_to_menu(HMENU menu, u32 item_id, u32 msg_id, HBITMAP img, HBITMAP checked_img = nil) {
	return append_item_to_menu(menu, item_id, RCS(msg_id), img, checked_img);
}

static bool append_item_to_menu(HMENU menu, u32 item_id, u32 msg_id, HBITMAP img, bool disabled) {
	bool res;
	res = append_item_to_menu(menu, item_id, msg_id, img);
	if (disabled) res = EnableMenuItem(menu, item_id, MF_BYCOMMAND | MF_GRAYED);
	return res;
}

static void append_language_items_to_menu(HMENU menu) {
#define _append_language_item(member,value_expr) append_item_to_menu(menu, Language::member, _t(#member), nil, bmps.circle);
	_foreach_language(_append_language_item)
#undef _append_language_item
	CheckMenuItem(menu, LanguageManager::Instance().GetCurrentLanguage(), MF_BYCOMMAND | MF_CHECKED);
}

static bool append_separator_to_menu(HMENU menu) {
	bool res;
	res = AppendMenu(menu, MF_SEPARATOR | MF_OWNERDRAW, 0, (LPCWSTR)menu);
	return res;
}

static bool set_menu_background_color(HMENU menu, HBRUSH color) {
	bool res;
	MENUINFO mi{ sizeof(MENUINFO) };
	mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
	mi.hbrBack = color;
	res = SetMenuInfo(menu, &mi);
	return res;
}

/**
  * Shell
  */

void open_link(const cstr* url){
	ShellExecute(nil, _t("open"), url, nil, nil, SW_SHOWNORMAL);
}
