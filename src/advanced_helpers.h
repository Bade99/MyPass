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
//INFO: flags defaults: left (if left or right or center isnt selected) and top (if top or bottom or center isnt selected)
static auto __msgbox_store_placement(HWND relative_to = (HWND)I32MIN, multiflag<MBP::MBP> flags = I32MIN) { //TODO(fran): look for more elegant ways to send data from MessageBox to Hook_MsgBox
	struct MsgBoxPlacement { HWND relative_to; multiflag<MBP::MBP> flags; } static placement{ 0 };
	if ((relative_to != (HWND)I32MIN) && (flags != I32MIN)) {
		//Set defaults
		if (!(flags & MBP::left || flags & MBP::right || flags & MBP::center)) flags |= MBP::left;
		if (!(flags & MBP::top || flags & MBP::bottom || flags & MBP::center)) flags |= MBP::top;

		placement.relative_to = relative_to;
		placement.flags = flags;
	}
	return placement;
}
static LRESULT CALLBACK __msgbox_hook(int code, WPARAM wparam, LPARAM lparam)
{
	//Thanks https://stackoverflow.com/questions/1530561/set-location-of-messagebox

	if (code == HCBT_CREATEWND)
	{
		CREATESTRUCT* pcs = ((CBT_CREATEWND*)lparam)->lpcs;

		if ((pcs->style & WS_DLGFRAME) || (pcs->style & WS_POPUP))//TODO(fran): this checks dont seem too robust
		{
			HWND wnd = (HWND)wparam;//Msgbox

			auto placement = __msgbox_store_placement();
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
	__msgbox_store_placement(relative_to, placement);
	HHOOK hook_proc = SetWindowsHookEx(WH_CBT, __msgbox_hook, 0, GetCurrentThreadId()); defer{ UnhookWindowsHookEx(hook_proc); };
	int res = MessageBox(relative_to, lpText, lpCaption, uType);
	return res;
}
