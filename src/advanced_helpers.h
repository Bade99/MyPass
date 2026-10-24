#pragma once

/**
  * Tooltip
  */

struct TooltipProps {
	bool multiline = false;
	u32 delay_ms = U32MAX; // delay between start of mouse over and showing the tooltip
	u32 duration_ms = U32MAX; // time that the tooltip remains visible
};
static auto add_mouseover_tooltip(HWND target, u64 msg_resource_id, TooltipProps props = {}) {
	//TODO(fran): we could have two types of tooltips, a singular tooltip for each nonclient window, that you can use for generic tooltips, for that one just call the TTM_ADDTOOL with all the controls we want it to look over. Then also allow custom tooltips, which will require the creation of a new tooltip control, eg for cases where it needs to have a different delay_ms
	auto tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		target, nil, nil, nil);
	Assert(tooltip);

	TOOLINFO tool_info{ TTTOOLINFO_V1_SIZE };
	tool_info.hwnd = target;
	tool_info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	tool_info.uId = (decltype(tool_info.uId))target;
	tool_info.hinst = GetModuleHandle(nil);
	tool_info.lpszText = (decltype(tool_info.lpszText))msg_resource_id;
	auto addtool_res = SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM)&tool_info);
	Assert(addtool_res);

	if (props.multiline) 
		SendMessage(tooltip, TTM_SETMAXTIPWIDTH, 0, DPI(1000)); //Enables multiline
	SendMessage(tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, (LPARAM)props.delay_ms != U32MAX ? props.delay_ms : 400);
	if (props.duration_ms != U32MAX)
		SendMessage(tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)props.duration_ms);

	SendMessage(tooltip, TTM_SETTIPTEXTCOLOR, ColorFromBrush(colors.ControlTxt), 0);
	SendMessage(tooltip, TTM_SETTIPBKCOLOR, ColorFromBrush(colors.ControlBk), 0);

	return tooltip;

}


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
	if (!dib.dsBm.bmBits) return nil;

	const int width = dib.dsBm.bmWidth;
	const int height = dib.dsBm.bmHeight;
	const int bits_per_pixel = dib.dsBm.bmBitsPixel;
	Assert(bits_per_pixel == 8); // Doesnt properly support all other bpps

	// Row stride aligned to 4 bytes
	const int stride = ((width * bits_per_pixel + 31) / 32) * 4;

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = bits_per_pixel;
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
			const int bytes_per_pixel = bits_per_pixel / 8;

			// Optimized path for 8-bit
			if (bits_per_pixel == 8) for (int x = 0; x < width; x++) destination_row[x] = source_row[width - 1 - x];
			else {
				// Generic path
				for (int x = 0; x < width; x++) {
					const int source_x = width - 1 - x;

					memcpy(destination_row + x * bytes_per_pixel, source_row + source_x * bytes_per_pixel, bytes_per_pixel);
				}
			}
		}
	}

	return destination_bitmap;
}

enum class Rotation { clockwise_90, clockwise_180, clockwise_270 };
static HBITMAP rotate_bitmap1(HBITMAP source_bitmap, Rotation rotation) { //AI generated, unoptimized
	auto get_bit = [](const u8* bits, int stride, int x, int y) {
		u8 byte = bits[y * stride + (x >> 3)];
		return (byte & (0x80 >> (x & 7))) != 0;
	};

	auto set_bit = [](u8* bits, int stride, int x, int y, bool value) {
		u8& byte = bits[y * stride + (x >> 3)];

		const u8 mask = (u8)(0x80 >> (x & 7));

		if (value) byte |= mask;
		else byte &= ~mask;
	};

	if (!source_bitmap) return nil;

	BITMAP bm{};
	if (!GetObject(source_bitmap, sizeof(bm), &bm)) return nil;

	Assert(bm.bmBitsPixel == 1);
	if (bm.bmBitsPixel != 1) return nil;

	const int source_width = bm.bmWidth;
	const int source_height = bm.bmHeight;

	const int source_stride = ((source_width + 15) / 16) * 2;

	const u32 source_size = source_stride * source_height;

	std::vector<u8> source_bits(source_size);

	if (GetBitmapBits(source_bitmap, source_size, source_bits.data()) != source_size) return nil;

	int destination_width, destination_height;
	switch (rotation) {
	case Rotation::clockwise_180:
		destination_width = source_width;
		destination_height = source_height;
		break;
	default:
		destination_width = source_height;
		destination_height = source_width;
		break;
	}

	const int destination_stride = ((destination_width + 15) / 16) * 2;

	const u32 destination_size = destination_stride * destination_height;

	std::vector<u8> destination_bits(destination_size, 0);

	for (int sy = 0; sy < source_height; sy++) {
		for (int sx = 0; sx < source_width; sx++)
		{
			const bool pixel = get_bit(source_bits.data(), source_stride, sx, sy);

			int dx, dy;

			switch (rotation) {
			case Rotation::clockwise_90:
				dx = source_height - 1 - sy;
				dy = sx;
				break;
			case Rotation::clockwise_180:
				dx = source_width - 1 - sx;
				dy = source_height - 1 - sy;
				break;
			case Rotation::clockwise_270:
				dx = sy;
				dy = source_width - 1 - sx;
				break;
			}

			set_bit(destination_bits.data(), destination_stride, dx, dy, pixel);
		}
	}

	HBITMAP destination_bitmap = CreateBitmap(destination_width, destination_height, 1, 1, nil);

	if (!destination_bitmap) return nil;

	if (SetBitmapBits(destination_bitmap, destination_size, destination_bits.data()) != destination_size)
	{
		DeleteObject(destination_bitmap);
		return nil;
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
