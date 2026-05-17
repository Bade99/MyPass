#pragma once

/**
  * HWND
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
