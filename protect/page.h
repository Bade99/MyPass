#pragma once
#include "win_sdk.h"
#include "platform.h"
#include "helpers.h"
#include "global.h"
#include "math.h"

namespace page {
constexpr auto& wndclass = wndclass_name("page");

struct Theme {
	union {
		struct {
			brush_group bk, border;
		};
		brush_group all[2];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	} brushes;
	struct {
		u32 border_thickness = U32MAX;
	}dimensions;

	bool copy_from(const Theme& src) {
		bool repaint = false;
		_theme_copy_all_brushes(src.brushes, this->brushes);

		_theme_copy_u32(src.dimensions.border_thickness, this->dimensions.border_thickness);

		return repaint;
	}
};

struct State {
	HWND wnd;
	HWND parent;
	Theme theme;

	bool does_scrolling;

	struct {
		bool active; //currently performing animation
		int current_frame; //starts at
		f32 dt;
		int total_frames;
		i64 _time_between; //internal
		f32 time_between_last_two_scroll_events; //ms
	} scroll_anim;
	std::deque<int> scroll_tasks;

	f32 scroll;//vertical scrolling of page //@int NOTE(fran): given the limitations of MoveWindow we must interpret this value as an integer (cast), the float precision is only used for animations

	//anim //@old @delete
	int scroll_frame; //@delete
	f32 scroll_dt;
	f32 scroll_v;
	f32 scroll_a;
	bool scroll_on_anim; //@delete

	void init() {
		scroll_tasks = decltype(scroll_tasks)();
	}

	void uninit() {
		scroll_tasks.~deque();
	}
};

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_theme(HWND wnd, const Theme& src) { _control_create_function__set_theme }

void set_scrolling(HWND wnd, bool does_scrolling) {
	State& state = *get_state(wnd);
	if (&state) {
		state.does_scrolling = does_scrolling;//TODO(fran): reset current scrolling?
	}
}

void set_wnd_size(HWND wnd, HWND parent_page_space, i32 h) {
	State& state = *get_state(wnd);
	if (&state) {
		RECT page_space; GetClientRect(parent_page_space, &page_space);
		RECT page_relative_to_space = get_window_rect_at(wnd, parent_page_space);

		auto page_space_w = RECTW(page_space), page_w = RECTW(page_relative_to_space);
		auto page_space_h = RECTH(page_space), page_h = RECTH(page_relative_to_space);
		auto needs_y_correction = page_h > page_space_h && page_relative_to_space.bottom < page_space.bottom; // True when the page can now be fit better within the page_space. The situation we are targeting is: page is scrolled -> window is resized making page_space larger -> page now has more space at the bottom that it can use to undo some of the scrolling
		auto y_correction = (needs_y_correction ? distance(page_relative_to_space.bottom, page_space.bottom) : 0);
		auto needs_secondary_y_correction = page_h <= page_space_h && page_relative_to_space.top < page_space.top;
		y_correction += (needs_secondary_y_correction ? distance(page_relative_to_space.top, page_space.top) : 0);
		rect_i32 page{
			.x = 0,
			.y = page_relative_to_space.top + y_correction, //retain current scroll, only adjust it if necessary
			.w = page_space_w, //we dont support horizontal scrolling, so we clamp page w to page_space w
			.h = maximum(page_space_h, h),
		};
		state.scroll += y_correction; //Update stored scroll value as well
		MoveWindow(wnd, page, false);
	}
}

void smooth_scroll(State& state, int increment) {

	//TODO(fran): the scrolling is now pretty good & responsive, only thing missing is a bit of smoothing across large distances (speeding up at the beginning and slowing down close to the end, possibly based on the frecuency of new scroll events?)
	state.scroll_anim.time_between_last_two_scroll_events = (f32)EndCounter(state.scroll_anim._time_between); //TODO(fran): it may be good to calculate the 2nd derivative of this, aka the delta between the last two times between two scroll events
	if (state.scroll_anim.time_between_last_two_scroll_events > .200f /*ms expressed in seconds*/)
		state.scroll_anim.time_between_last_two_scroll_events = 0;
	//printf("time between last two scroll events: %f ms\n", (f32)EndCounter(state.scroll_anim._time_between));
	state.scroll_anim._time_between = StartCounter();

	if (state.scroll_tasks.size() && (signum(state.scroll_tasks.back()) != signum(increment))) {
		//IMPORTANT(fran): I think the Timer runs on the same thread, otherwise race conditions accessing & modifying scroll_tasks can crash the app
		auto last = state.scroll_tasks.back();
		state.scroll_tasks.clear();
		state.scroll_tasks.push_back(last);
	}
	state.scroll_tasks.push_back(increment);


	static void (*scroll_timeout)(HWND, UINT, UINT_PTR, DWORD) =
		[](HWND hwnd, UINT, UINT_PTR anim_id, DWORD) {
		State& state = *get_state(hwnd);
		if (&state) {
			if (state.scroll_tasks.size()) {
				auto last = state.scroll_tasks.back();
				state.scroll_tasks.clear();
				state.scroll_tasks.push_back(last);
			}
		}
		};

	//If no new scroll events happen after X ms then stop scrolling, the user is no longer scrolling the mouse wheel and wants scrolling to stop
	SetTimer(state.wnd, 2222, 125/*ms*/, scroll_timeout);

	static auto setup_scroll_anim = [](State& state, TIMERPROC scroll_animation) {
		state.scroll_anim.active = true;

		//auto anim_duration = (50.f + state.scroll_anim.time_between_last_two_scroll_events) / state.scroll_tasks.size(); //ms
		auto anim_duration = 50.f / state.scroll_tasks.size() + state.scroll_anim.time_between_last_two_scroll_events; //ms //TODO(fran): the gradual slowdown effect works for short event chains, long ones, like the ones I can create with my free scrolling mouse still stop dead with no ease out
		state.scroll_anim.dt = 1.f / (f32)refresh_rate_hz(state.wnd);//duration of each frame in seconds
		state.scroll_anim.total_frames = (i32)maximum(anim_duration / (state.scroll_anim.dt * 1000.f), 1.f);
		state.scroll_anim.current_frame = 1;
		SetTimer(state.wnd, 1111, (u32)(state.scroll_anim.dt * 1000), scroll_animation);
		};

	static void (*scroll_anim)(HWND, UINT, UINT_PTR, DWORD) =
		[](HWND hwnd, UINT, UINT_PTR anim_id, DWORD) {
		State& state = *get_state(hwnd);
		if (&state) {

			RECT r; GetWindowRect(state.wnd, &r); MapWindowPoints(0, state.parent, (POINT*)&r, 2);
			RECT parent_r; GetWindowRect(state.parent, &parent_r);
			i32 parent_h = RECTH(parent_r);
			i32 current_h = RECTH(r);
			const i32 min_scroll_y = 0; //prevent scrolling to go above of the page boundaries
			i32 max_scroll_y = -maximum(current_h - parent_h, 0); //prevent scrolling to go below of the page boundaries
			//TODO: when the page itself is being resized we should again check if we need to clamp the current scroll position since it could have changed leaving us in an improper scroll until the user tries to scroll again, at which point we re-apply the correct clamp

			i32 original_wnd_y = r.top - (i32)state.scroll;

			f32 dy = (f32)state.scroll_tasks[0] / (f32)state.scroll_anim.total_frames;
			state.scroll += dy;

			i32 new_y = clamp(max_scroll_y, (i32)(original_wnd_y + state.scroll), min_scroll_y);

			MoveWindow(state.wnd, r.left, new_y, RECTW(r), RECTH(r), TRUE);

			if (state.scroll_anim.current_frame++ <= state.scroll_anim.total_frames) {
				SetTimer(state.wnd, anim_id, (u32)(state.scroll_anim.dt * 1000), scroll_anim);
			}
			else {
				state.scroll_tasks.pop_front();

				if (state.scroll_tasks.empty()) {
					state.scroll_anim.active = false;
					KillTimer(state.wnd, anim_id);
				}
				else {
					setup_scroll_anim(state, scroll_anim);
				}
			}
		}
		};

	if (!state.scroll_anim.active) {
		setup_scroll_anim(state, scroll_anim);
	}
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//static int __cnt; printf("%d:PAGE:%s\n", __cnt++, msgToString(msg));
	State& state = *get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		State* st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		st->wnd = hwnd;
		st->parent = creation_nfo->hwndParent;
		st->init();
		return 1;
	} break;
	case WM_NCDESTROY:
	{
		state.uninit();
		set_window_state(state.wnd, nil);
		free(&state);
		return 0;
	}break;
	case WM_NCHITTEST:
	{
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		RECT r; GetWindowRect(state.wnd, &r);
		auto hittest = test_pt_rc(mouse, r) ? HTCLIENT : HTNOWHERE;
		return hittest;
	} break;
	case WM_PAINT://NOTE: we could also do this on WM_ERASEBACKGROUND and return defwindowproc on WM_PAINT
	{
		return 0; //TODO(fran): i dont think we want to paint anything, just have the page be a helper for scrolling
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };
		RECT rc; GetClientRect(state.wnd, &rc);
		i32 w = RECTW(rc), h = RECTH(rc);
		bool window_enabled = IsWindowEnabled(state.wnd);

		//Bk
		HBRUSH bk_br = window_enabled ? state.theme.brushes.bk.normal : state.theme.brushes.bk.disabled;
		FillRect(dc, &rc, bk_br);

		//Border
		auto border_thickness = state.theme.dimensions.border_thickness;
		HBRUSH border_br = window_enabled ? state.theme.brushes.border.normal : state.theme.brushes.border.disabled;
		FillRectBorder(dc, rc, border_thickness, border_br, BORDERALL);
		
		return 0;
	} break;
	case WM_MOUSEWHEEL:
	{
		if (state.does_scrolling) {
			auto zDelta = (f32)GET_WHEEL_DELTA_WPARAM(wparam) / (f32)WHEEL_DELTA;
			int dy = avg_str_dim(fonts.General, 1).cy;

			//TODO(fran): SystemParametersInfo(SPI_GETWHEELSCROLLLINES) to scroll based on windows mouse scroll sensitivity the user set?

			int step = zDelta * 3 * dy;
#if 0 //possibly better solution
			UINT flags = MAKELONG(SW_SCROLLCHILDREN | SW_SMOOTHSCROLL, 200);
			ScrollWindowEx(state.wnd, 0, step, nullptr, nullptr, nullptr, nullptr, flags);
#else //handmade solution (no WM_PRINT and friends)
			smooth_scroll(state, step);
#endif
			return 0;
		}
		else return DefWindowProc(hwnd, msg, wparam, lparam);//propagates msg to the parent
	} break;
	/*case WM_ASK_FOR_RESIZE:
	{
		AskForResize(state.parent); //TODO(fran): the page could get the resize info on startup and manage this without bothering the parent
		return 0;
	} break;*/
	case WM_MOUSEACTIVATE:
	{
		return MA_ACTIVATE;
	} break;
	case WM_COMMAND:
	{
		HWND child = (HWND)lparam;
		if (child) return SendMessage(state.parent, msg, wparam, lparam);//Any msg from our childs goes up to our parent
		else Assert(0); //No msg from menus nor accelerators should come here
	} break;
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
	case WM_GETFONT:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	{
		return SendMessage(state.parent, msg, wparam, lparam);
	} break;
	case WM_NCCALCSIZE:
	case WM_NCPAINT:
	case WM_ERASEBKGND:
	case WM_SETFONT:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_GETTEXT:
	case WM_GETTEXTLENGTH:
	case WM_PARENTNOTIFY: //We dont care to send this msgs from our childs up the chain, at least for now
	case WM_IME_SETCONTEXT: //We dont want IME for the page window itself
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
	{
		return 0;
	} break;
	case WM_SETTEXT: //do as if we handled it
	case WM_DELETEITEM: //do as if we handled it
	{
		return 1;
	} break;
#ifdef _DEBUG_HWND_MESSAGES
	case WM_CREATE:
	case WM_SIZE:
	case WM_MOVE:
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
	case WM_SHOWWINDOW:
	case WM_DESTROY:
	case WM_SETCURSOR:
	case WM_MOUSEMOVE:
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
#endif
	default:
#ifdef _DEBUG_HWND_MESSAGES
		Assert(0);
#endif
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}
_init_wndclass_at_startup;
}