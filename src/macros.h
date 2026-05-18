#pragma once

/**
  * My Custom Settings
  */
#ifdef _DEBUG
//TODO(fran): change to logging
#define _SHOWCONSOLE
//#define _DEBUG_HWND_MESSAGES //Stop execution when a window receives a message it isn't handling
#else
//#define _SHOWCONSOLE
#endif


/**
  * Logging
  */
#ifndef _SHOWCONSOLE
#define printf(...)  
#define wprintf(...)  
#endif


/**
  * Assert
  */
#ifdef _DEBUG
#define UNCAP_ASSERTIONS
#endif

#ifdef UNCAP_ASSERTIONS
#define Assert(assertion) if(!(assertion))*(int*)NULL=0
#else 
#define Assert(assertion) 
#endif

//Assert guaranteed to be executed in any build configuration whether assertions are enabled or not
#define runtime_assert(assertion,msg) if (!(assertion))MessageBox(0,msg,TEXT("Error"),MB_OK|MB_ICONWARNING|MB_SETFOREGROUND) || (*(int*)NULL = 0)==0


/**
  * Namespace
  */
#define namespace_start(name) namespace name {
#define namespace_end() }


/**
  * Array/String
  */
#define ArraySizeWithoutTerminator(arr) (ARRAYSIZE(arr) - 1)

#ifdef UNICODE
#define to_str(v) std::to_wstring(v)
#define cstr_printf(...)  wprintf(__VA_ARGS__)
//Get string length, does NOT include the null terminator
#define cstr_len wcslen
#else
#define to_str(v) std::to_string(v)
#define cstr_printf(...)  printf(__VA_ARGS__)
//Get string length, does NOT include the null terminator
#define cstr_len strlen
#endif


/**
  * Quality of Life
  */
//Thanks to https://handmade.network/forums/t/1273-post_your_c_c++_macro_tricks/3
template <typename F> struct Defer { Defer(F f) : f(f) {} ~Defer() { f(); } F f; };
template <typename F> Defer<F> makeDefer(F f) { return Defer<F>(f); };
#define __defer( line ) defer_ ## line
#define _defer( line ) __defer( line )
struct defer_dummy {};
template<typename F> Defer<F> operator+(defer_dummy, F&& f) { return makeDefer<F>(std::forward<F>(f)); }
//usage: defer{block of code;}; //the last defer in a scope gets executed first (LIFO)
#define defer auto _defer( __LINE__ ) = defer_dummy( ) + [ & ]( )

#define nil nullptr

#define elif else if


/**
  * Theme
  */
#define _theme_copy_all_brushes(src, dest) for (u32 i = 0; auto& b : dest.all) repaint |= b.copy_from(src.all[i++])
#define _theme_copy_u32(src, dest) if (src != U32MAX) { dest = src; repaint = true; }
#define _theme_copy_uinumber(src, dest) if (src.value != F32INFINITY) { dest = src; repaint = true; }
#define _theme_copy_pointer(src, dest) if (src) { dest = src; repaint = true; }
#define _theme_copy_theme(src, dest) repaint |= dest.copy_from(src);
//trool = 2bit bool, 2nd bit is used to determine whether to copy or not
#define _theme_copy_trool(src, dest) if (src != 2) { dest = src; repaint = true; }


#define check_trool(trool) (trool & 0x1)


/**
  * Control
  */
#ifdef _DEBUG
#define _control_create_function__get_state \
	TCHAR test[50]; \
	auto cnt = GetClassName(wnd, test, ARRAYSIZE(test)); \
	Assert(cnt && ARRAYSIZE(wndclass)); \
	Assert(!StrCmpN(wndclass, test, ++cnt)); \
	return get_window_state<State>(wnd);
#else
#define _control_create_function__get_state return get_window_state<State>(wnd);
#endif

#define _control_create_function__set_theme \
	State& state = *get_state(wnd); Assert(&state); \
	if (&state) { \
		bool repaint = state.theme.copy_from(src); \
		if (repaint) ask_window_for_repaint(state.wnd); \
	}

#define _control_create_function__set_functions \
	State& state = *get_state(wnd); Assert(&state); \
	for (u32 i = 0; i < ARRAYSIZE(state.functions.all); i++) \
		if (functions.all[i]) state.functions.all[i] = functions.all[i];

#define _control_create_function__set_stateful_functions \
	State& state = *get_state(wnd); Assert(&state); \
	for (u32 i = 0; i < ARRAYSIZE(state.stateful_functions.all); i++) \
		if (functions.all[i].function) state.stateful_functions.all[i] = functions.all[i];

#define _control_create_function__set_user_data \
	State& state = *get_state(wnd); Assert(&state); \
	state.user_data = user_data;

// State must be of type State&, not State*,
// since we want to validate that the pointer to the state (&state) is not null
#define _control_validate_state Assert(msg == WM_NCCREATE || &state);