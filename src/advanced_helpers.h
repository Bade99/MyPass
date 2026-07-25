#pragma once

/**
  * Tooltip
  */

struct tooltip_props {
	bool multiline = false;
	u32 delay_ms = U32MAX; // delay between start of mouse over and showing the tooltip
	u32 duration_ms = U32MAX; // time that the tooltip remains visible
};
static auto add_mouseover_tooltip(HWND target, u64 msg_resource_id, tooltip_props props = {}) {
	//TODO(fran): we could have two types of tooltips, a singular tooltip for each nonclient window, that you can use for generic tooltips, for that one just call the TTM_ADDTOOL with all the controls we want it to look over. Then also allow custom tooltips, which will require the creation of a new tooltip control, eg for cases where it needs to have a different delay_ms
	auto tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		target, nil, nil, nil);
	Assert(tooltip);

	TOOLINFO toolInfo{ TTTOOLINFO_V1_SIZE };
	toolInfo.hwnd = target;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = (decltype(toolInfo.uId))target;
	toolInfo.hinst = GetModuleHandle(nil);
	toolInfo.lpszText = (decltype(toolInfo.lpszText))msg_resource_id;
	auto addtool_res = SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
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

HBITMAP flip_bitmap(HBITMAP srcBitmap, bool flipHorizontal, bool flipVertical) {
	Assert(flipHorizontal || flipVertical);
    if (!srcBitmap) return nil;

    DIBSECTION dib{};
    if (!GetObject(srcBitmap, sizeof(dib), &dib)) return nil;
    if (!dib.dsBm.bmBits) return nil;

    const int width = dib.dsBm.bmWidth;
    const int height = dib.dsBm.bmHeight;
    const int bitsPerPixel = dib.dsBm.bmBitsPixel;
	Assert(bitsPerPixel == 8); // Doesnt properly support all other bpps

    // Row stride aligned to 4 bytes
    const int stride = ((width * bitsPerPixel + 31) / 32) * 4;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = bitsPerPixel;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dstBits = nil;
    HDC screenDC = GetDC(nil);
    HBITMAP dstBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &dstBits, nil, 0);
    ReleaseDC(nil, screenDC);
    if (!dstBitmap || !dstBits) return nil;

    const auto* src = (const u8*)(dib.dsBm.bmBits);
    auto* dst = (u8*)(dstBits);

    for (int y = 0; y < height; y++) {
        const int srcY = flipVertical ? (height - 1 - y) : y;

        const auto* srcRow = src + srcY * stride;
        auto* dstRow = dst + y * stride;

        if (!flipHorizontal) memcpy(dstRow, srcRow, stride);
        else {
            const int bytesPerPixel = bitsPerPixel / 8;

            // Optimized path for 8-bit
            if (bitsPerPixel == 8) for (int x = 0; x < width; x++) dstRow[x] = srcRow[width - 1 - x];
            else {
                // Generic path
                for (int x = 0; x < width; x++) {
                    const int srcX = width - 1 - x;

                    memcpy(dstRow + x * bytesPerPixel, srcRow + srcX * bytesPerPixel, bytesPerPixel);
                }
            }
        }
    }

    return dstBitmap;
}

enum class Rotation { CW90, CW180, CW270 };
static HBITMAP rotate_bitmap1(HBITMAP srcBitmap, Rotation rotation) { //AI generated, unoptimized
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

    if (!srcBitmap) return nil;

    BITMAP bm{};
    if (!GetObject(srcBitmap, sizeof(bm), &bm)) return nil;

    Assert(bm.bmBitsPixel == 1);
    if (bm.bmBitsPixel != 1) return nil;

    const int srcWidth = bm.bmWidth;
    const int srcHeight = bm.bmHeight;

    const int srcStride = ((srcWidth + 15) / 16) * 2;

    const u32 srcSize = srcStride * srcHeight;

    std::vector<u8> srcBits(srcSize);

    if (GetBitmapBits(srcBitmap, srcSize, srcBits.data()) != srcSize) return nil;

    int dstWidth, dstHeight;
    switch (rotation) {
    case Rotation::CW180:
        dstWidth = srcWidth;
        dstHeight = srcHeight;
        break;
    default:
        dstWidth = srcHeight;
        dstHeight = srcWidth;
        break;
    }

    const int dstStride = ((dstWidth + 15) / 16) * 2;

    const u32 dstSize = dstStride * dstHeight;

    std::vector<u8> dstBits(dstSize, 0);

    for (int sy = 0; sy < srcHeight; sy++) {
        for (int sx = 0; sx < srcWidth; sx++)
        {
            const bool pixel = get_bit(srcBits.data(), srcStride, sx, sy);

            int dx, dy;

            switch (rotation) {
            case Rotation::CW90:
                dx = srcHeight - 1 - sy;
                dy = sx;
                break;
            case Rotation::CW180:
                dx = srcWidth - 1 - sx;
                dy = srcHeight - 1 - sy;
                break;
            case Rotation::CW270:
                dx = sy;
                dy = srcWidth - 1 - sx;
                break;
            }

            set_bit(dstBits.data(), dstStride, dx, dy, pixel);
        }
    }

    HBITMAP dstBitmap = CreateBitmap(dstWidth, dstHeight, 1, 1, nil);

    if (!dstBitmap) return nil;

    if (SetBitmapBits(dstBitmap, dstSize, dstBits.data()) != dstSize)
    {
        DeleteObject(dstBitmap);
        return nil;
    }

    return dstBitmap;
}


/**
  * Menu
  */

static bool append_item_to_menu(HMENU menu, u32 item_id, u32 msg_id, HBITMAP img) {
	bool res;
	res = AppendMenu(menu, MF_STRING | MF_OWNERDRAW, item_id, (cstr*)menu);
	res = SetMenuItemString(menu, item_id, FALSE, RCS(msg_id));
	res = SetMenuItemBitmaps(menu, item_id, MF_BYCOMMAND, img, nil);
	return res;
}

static bool append_item_to_menu(HMENU menu, u32 item_id, u32 msg_id, HBITMAP img, bool disabled) {
	bool res;
	res = append_item_to_menu(menu, item_id, msg_id, img);
	if (disabled) res = EnableMenuItem(menu, item_id, MF_BYCOMMAND | MF_GRAYED);
	return res;
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