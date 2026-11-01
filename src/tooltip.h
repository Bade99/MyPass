#pragma once
#include "helpers.h"
#include <map>

struct TooltipProps {
	bool multiline = false;
	u32 delay_ms = U32MAX;
	u32 duration_ms = U32MAX;
};

namespace shared_tooltip {
	enum class Kind { mouseover, tracking };
	using TextProvider = str(*)(void*);

	struct Group {
		HWND wnd;
		Kind kind;
		TooltipProps props;
	};

	struct Tool {
		HWND tooltip;
		str text;
		TextProvider text_provider = nil;
		void* provider_context = nil;
	};

	struct Root {
		std::vector<Group> groups;
		std::map<HWND, Tool> tools;
	};

	inline std::map<HWND, Root>& roots() {
		static std::map<HWND, Root> all;
		return all;
	}

	inline TOOLINFO tool_info(HWND root, HWND target) {
		TOOLINFO info{ TTTOOLINFO_V1_SIZE };
		info.hwnd = root;
		info.uFlags = TTF_IDISHWND;
		info.uId = (UINT_PTR)target;
		info.hinst = GetModuleHandle(nil);
		return info;
	}

	inline TOOLINFO tool_info(HWND target) {
		return tool_info(GetAncestor(target, GA_ROOT), target);
	}

	inline LRESULT CALLBACK target_proc(HWND target, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR ref_data) {
		if (message == WM_NCDESTROY) {
			HWND root = (HWND)ref_data;
			if (auto root_it = roots().find(root); root_it != roots().end()) {
				if (auto tool_it = root_it->second.tools.find(target); tool_it != root_it->second.tools.end()) {
					HWND tooltip = tool_it->second.tooltip;
					if (IsWindow(tooltip)) {
						auto info = tool_info(root, target);
						SendMessage(tooltip, TTM_DELTOOL, 0, (LPARAM)&info);
					}
					root_it->second.tools.erase(tool_it);
				}
			}
			RemoveWindowSubclass(target, target_proc, 0);
		}
		return DefSubclassProc(target, message, wparam, lparam);
	}

	inline LRESULT CALLBACK root_proc(HWND root, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
		if (message == WM_NOTIFY) {
			auto* notification = (NMHDR*)lparam;
			if (notification) {
				if (auto root_it = roots().find(root); root_it != roots().end()) {
					if (notification->code == TTN_GETDISPINFO) {
						HWND target = (HWND)notification->idFrom;
						if (auto tool_it = root_it->second.tools.find(target); tool_it != root_it->second.tools.end() &&
							tool_it->second.tooltip == notification->hwndFrom && tool_it->second.text_provider) {
							auto& tool = tool_it->second;
							tool.text = tool.text_provider(tool.provider_context);
							auto* info = (NMTTDISPINFO*)lparam;
							info->lpszText = tool.text.data();
							info->hinst = nil;
							info->uFlags &= ~TTF_DI_SETITEM;
							return 0;
						}
					}
					for (const auto& group : root_it->second.groups) {
						if (group.kind != Kind::tracking || group.wnd != notification->hwndFrom) continue;
						TOOLINFO current{ TTTOOLINFO_V1_SIZE };
						if (SendMessage(group.wnd, TTM_GETCURRENTTOOL, 0, (LPARAM)&current)) {
							HWND target = (HWND)current.uId;
							if (root_it->second.tools.contains(target) && IsWindow(target))
								return SendMessage(target, message, wparam, lparam);
						}
					}
				}
			}
		}
		if (message == WM_NCDESTROY) RemoveWindowSubclass(root, root_proc, 0);
		LRESULT result = DefSubclassProc(root, message, wparam, lparam);
		if (message == WM_NCDESTROY) roots().erase(root);
		return result;
	}

	inline bool same_props(const TooltipProps& first, const TooltipProps& second) {
		return first.multiline == second.multiline && first.delay_ms == second.delay_ms && first.duration_ms == second.duration_ms;
	}

	inline HWND register_tool(HWND target, Kind kind, u64 resource_id = 0, TooltipProps props = {},
		TextProvider provider = nil, void* provider_context = nil) {
		HWND root = GetAncestor(target, GA_ROOT);
		Assert(root);
		auto [root_it, inserted] = roots().try_emplace(root);
		if (inserted) {
			auto subclassed = SetWindowSubclass(root, root_proc, 0, 0);
			Assert(subclassed);
		}
		auto& entry = root_it->second;
		Group* group = nil;
		for (auto& existing : entry.groups) {
			if (existing.kind == kind && (kind == Kind::tracking || same_props(existing.props, props))) {
				group = &existing;
				break;
			}
		}
		if (!group) {
			HWND tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nil,
				WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				root, nil, nil, nil);
			Assert(tooltip);
			if (props.multiline) SendMessage(tooltip, TTM_SETMAXTIPWIDTH, 0, DPI(1000));
			if (kind == Kind::mouseover) {
				SendMessage(tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, props.delay_ms != U32MAX ? props.delay_ms : 400);
				if (props.duration_ms != U32MAX) SendMessage(tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, props.duration_ms);
			}
			SendMessage(tooltip, TTM_SETTIPTEXTCOLOR, ColorFromBrush(colors.ControlTxt), 0);
			SendMessage(tooltip, TTM_SETTIPBKCOLOR, ColorFromBrush(colors.ControlBk), 0);
			entry.groups.push_back({ tooltip, kind, props });
			group = &entry.groups.back();
		}

		auto info = tool_info(root, target);
		info.uFlags |= kind == Kind::mouseover ? TTF_SUBCLASS : TTF_ABSOLUTE | TTF_TRACK;
		info.lpszText = (cstr*)resource_id;
		auto [tool_it, tool_inserted] = entry.tools.try_emplace(target, Tool{ group->wnd, {}, provider, provider_context });
		Assert(tool_inserted);
		auto added = SendMessage(group->wnd, TTM_ADDTOOL, 0, (LPARAM)&info);
		Assert(added);
		auto subclassed = SetWindowSubclass(target, target_proc, 0, (DWORD_PTR)root);
		Assert(subclassed);
		return tool_it->second.tooltip;
	}

	inline void update_text(HWND target, str_view text) {
		HWND root = GetAncestor(target, GA_ROOT);
		auto root_it = roots().find(root);
		Assert(root_it != roots().end());
		auto tool_it = root_it->second.tools.find(target);
		Assert(tool_it != root_it->second.tools.end());
		tool_it->second.text.assign(text);
		auto info = tool_info(root, target);
		info.lpszText = tool_it->second.text.data();
		SendMessage(tool_it->second.tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&info);
	}
}

inline HWND add_mouseover_tooltip(HWND target, u64 resource_id, TooltipProps props = {}) {
	return shared_tooltip::register_tool(target, shared_tooltip::Kind::mouseover, resource_id, props);
}

inline HWND add_mouseover_tooltip(HWND target, str_view text, TooltipProps props = {}) {
	HWND tooltip = add_mouseover_tooltip(target, 0, props);
	shared_tooltip::update_text(target, text);
	return tooltip;
}

inline HWND add_mouseover_tooltip(HWND target, shared_tooltip::TextProvider provider, void* context, TooltipProps props = {}) {
	return shared_tooltip::register_tool(target, shared_tooltip::Kind::mouseover,
		(u64)(UINT_PTR)LPSTR_TEXTCALLBACK, props, provider, context);
}

inline HWND add_tracking_tooltip(HWND target) {
	return shared_tooltip::register_tool(target, shared_tooltip::Kind::tracking);
}
