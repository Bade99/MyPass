#pragma once

#include "helpers.h"
#include "render_diagnostics.h"
#include <span>

namespace child_layout {

struct MoveRequest {
	HWND wnd;
	RECT bounds;
	UINT flags = 0;
};

static UINT position_flags(HWND wnd, RECT bounds, bool repaint) {
	RECT current = get_window_rect_at(wnd, GetParent(wnd));
	if (EqualRect(&current, &bounds)) {
		render_diagnostics::record_count(render_diagnostics::Event::layout_unchanged);
		return 0;
	}

	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (repaint ? 0 : SWP_NOREDRAW);
	if (current.left == bounds.left && current.top == bounds.top) flags |= SWP_NOMOVE;
	if (RECTW(current) == RECTW(bounds) && RECTH(current) == RECTH(bounds)) {
		render_diagnostics::record_count(render_diagnostics::Event::layout_move_only);
		flags |= SWP_NOSIZE;
	}
	else render_diagnostics::record_count(render_diagnostics::Event::layout_resize);
	return flags;
}

static bool move_if_needed(HWND wnd, RECT bounds, bool repaint = true) {
	UINT flags = position_flags(wnd, bounds, repaint);
	if (!flags) return true;
	if (flags & SWP_NOSIZE) {
		auto moved = SetWindowPos(wnd, nullptr, bounds.left, bounds.top, 0, 0, flags);
		if (moved && repaint) UpdateWindow(wnd);
		return moved != 0;
	}

	return MoveWindow(wnd, bounds.left, bounds.top, RECTW(bounds), RECTH(bounds), repaint) != 0;
}

static bool move_if_needed(HWND wnd, rect_i32 bounds, bool repaint = true) {
	RECT rect{ bounds.x, bounds.y, bounds.right(), bounds.bottom() };
	return move_if_needed(wnd, rect, repaint);
}

static bool move_batch(std::span<MoveRequest> requests, bool repaint = true) {
	size_t changed_count = 0;
	for (auto& request : requests) {
		request.flags = position_flags(request.wnd, request.bounds, repaint);
		if (request.flags) changed_count++;
	}
	if (!changed_count) return true;

	HDWP batch = BeginDeferWindowPos((int)changed_count);
	if (batch) {
		for (auto& request : requests) {
			if (!request.flags) continue;
			batch = DeferWindowPos(batch, request.wnd, nullptr, request.bounds.left, request.bounds.top,
				RECTW(request.bounds), RECTH(request.bounds), request.flags);
			if (!batch) break;
		}
		if (batch && EndDeferWindowPos(batch)) return true;
	}

	render_diagnostics::record_count(render_diagnostics::Event::layout_batch_failure);
	bool moved_all = true;
	for (auto& request : requests)
		if (request.flags)
			moved_all = MoveWindow(request.wnd, request.bounds.left, request.bounds.top,
				RECTW(request.bounds), RECTH(request.bounds), repaint) && moved_all;
	return moved_all;
}

}
