#pragma once
#include "win_sdk.h"
#include "helpers.h"

namespace custom { //TODO(fran): remove, good idea, but we can do even better now

	constexpr auto& wndclass = wndclass_name("custom");

	typedef LRESULT(*func_proc)(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	struct State : WindowState { //Header struct, expand it with what you need
		func_proc proc;
	};

	auto get_state(HWND wnd) { _control_create_function__get_state }

	static LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		State& state = *get_state(wnd); 
		//TODO(fran): in order to avoid this whole additional null checks and function redirections I could just register a new window class realtime and point the proc function to what we want. The only thing that this control basically does is just allow us not to waste time registering a window, which may not even be that slow or dangerous of an idea.
		if (&state && state.proc) return state.proc(wnd, msg, wparam, lparam);
		else return DefWindowProc(wnd, msg, wparam, lparam);
	}
	//_init_wndclass_at_startup;
}