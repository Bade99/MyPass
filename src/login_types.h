#pragma once

namespace login {

constexpr auto& wndclass = wndclass_name("login");

//IDEA: why not, instead of having two separate hwnds, nc and cl, just have one, and the nc be a separate function call, immediate problem to solve are msgs that both would need, eg create and such

//USAGE: once the user presses on "Login" we will send a msg to ourselves that will not arrive, it's for the guy on top controlling the msg loop, it will have a state machine type of thing where it knows to current state, the next, and what's needed to get to the next (eg getting data from the previous), and finally to request the current wnd to stop (DestroyWindow)

//TODO(fran): failure after multiple wrong passwords? eg 10 min block

constexpr auto EDIT_USERNAME = 10;
constexpr auto EDIT_PASSWORD = 11;
constexpr auto BTN_LOGIN = 12;

constexpr auto default_text_tooltip_location = edit_oneline::ETP::left | edit_oneline::ETP::top;

constexpr auto invalid_password_message_duration_ms = 3000;

constexpr auto max_input_chars = 32;

constexpr auto msgbox_placement = MBP::center | MBP::top;

struct Settings {

#define foreach_LoginSettings_member(op) \
		op(RECT, rc,200,200,600,450 ) \

	foreach_LoginSettings_member(_generate_member);

	_generate_default_struct_serialize(foreach_LoginSettings_member);

	_generate_default_struct_deserialize(foreach_LoginSettings_member);

	void validate() {
		std::remove_reference<decltype(*this)>::type defaults;
		validate_rect_in_screen(rc, defaults.rc);
	}
};

struct Results {
	text username; //TODO(fran): the control where this is entered shouldnt accept chars that cant be used to create a folder
	text password;
	bool signup;
};

struct Data { //a pointer to this struct is to be sent on CreateWindow, struct MUST be zeroed out
	Settings* settings;
	Results* results;
};

struct State {
	HWND wnd;
	HWND nc_parent;

	union LoginControls {
		struct {
			HWND button_toggle_signup;
			HWND edit_username;
			HWND edit_password;
			HWND static_error_message;
			HWND button_login;
		};
		HWND all[7];
		private: void _() { static_assert(sizeof(*this) == sizeof(all)); }
	}controls;
	Settings* settings;
	Results* results;

	bool signup_mode;
};

enum class AttemptResult {
	success, fail_password, fail_username, fail_signup_username_exists, fail_newer_version, fail_corrupted
};
}