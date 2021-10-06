#pragma once
#include <windows.h>
#include "unCap_Helpers.h"
#include "unCap_Global.h"
#include "unCap_edit.h"
#include "LANGUAGE_MANAGER.h"
#include <Richedit.h>
#include "protect_search.h"

constexpr TCHAR protect_wndclass_show_passwords[] = TEXT("protect_wndclass_show_passwords");

//TODO(fran): would it be better to use a gridview instead of an edit control?

struct ShowPasswordsSettings {

#define foreach_ShowPasswords_member(op) \
		op(RECT, rc,200,200,700,800 ) \

	foreach_ShowPasswords_member(_generate_member);

	_generate_default_struct_serialize(foreach_ShowPasswords_member);

	_generate_default_struct_deserialize(foreach_ShowPasswords_member);

};

_add_struct_to_serialization_namespace(ShowPasswordsSettings)

struct ShowPasswordsStart {
	u32 key[8];//32 bytes
	text username;
};

struct ShowPasswordsData {
	ShowPasswordsSettings* settings;
	ShowPasswordsStart* start;
};

struct ShowPasswordsState {
	HWND wnd;
	HWND nc_parent;
	struct { //menus
		HMENU menu;
		HMENU menu_file;
		HMENU menu_file_lang;
		HMENU menu_edit;
	};
	union ShowPasswordsControls {
		struct {
			HWND edit_passwords;
		};
		HWND all[1];//REMEMBER TO UPDATE
	}controls;
	ShowPasswordsSettings* settings;
	ShowPasswordsStart* start;

	wchar_t* current_user;
	bool passwords_saved;
};

void SHOWPASSWORDS_set_state(HWND wnd, ShowPasswordsState* state) {
	SetWindowLongPtr(wnd, 0, (LONG_PTR)state);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
}

ShowPasswordsState* SHOWPASSWORDS_get_state(HWND wnd) {
	ShowPasswordsState* state = (ShowPasswordsState*)GetWindowLongPtr(wnd, 0);//INFO: windows recomends to use GWL_USERDATA https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
	return state;
}

void SHOWPASSWORDS_set_passwords_saved(ShowPasswordsState* state, bool new_val) {
	state->passwords_saved = new_val;
	str txt;
	if (state->current_user) {
		txt = state->current_user;
		if (!new_val) txt += sizeof(*txt.c_str()) > 1 ? _t(" ●") : _t(" *");
	}
	SetText_txt_app(state->nc_parent, txt.c_str(), app_name);
}

void SHOWPASSWORDS_resize_controls(ShowPasswordsState* state) {
	RECT r;
	GetClientRect(state->wnd, &r);

	int edit_showpasswords_x = r.left;
	int edit_showpasswords_y = r.top;
	int edit_showpasswords_w = RECTWIDTH(r);
	int edit_showpasswords_h = RECTHEIGHT(r);

	MoveWindow(state->controls.edit_passwords, edit_showpasswords_x, edit_showpasswords_y, edit_showpasswords_w, edit_showpasswords_h, TRUE);
}

void SHOWPASSWORDS_add_controls(ShowPasswordsState* state) {

#if 1 //edit control
	//TODO(fran): EM_SETENDOFLINE allows you to change between EC_ENDOFLINE_CRLF EC_ENDOFLINE_CR EC_ENDOFLINE_LF
	state->controls.edit_passwords = CreateWindowExW(NULL, L"Edit", NULL, WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_CLIPCHILDREN | WS_VISIBLE | ES_NOHIDESEL /*to show selection even when you dont have the focus*/ //| WS_VSCROLL | WS_HSCROLL 
		, 0, 0, 0, 0
		, state->wnd
		, NULL, NULL, NULL);

#define EDIT_PASSWORDS_MAX_TEXT_LENGTH (32767*2) //32767 is the default
	SendMessageW(state->controls.edit_passwords, EM_SETLIMITTEXT, (WPARAM)EDIT_PASSWORDS_MAX_TEXT_LENGTH, NULL);

	SetWindowSubclass(state->controls.edit_passwords, EditProc, 0, (DWORD_PTR)calloc(1, sizeof(EditProcState)));

	HWND VScrollControl = CreateWindowExW(NULL, unCap_wndclass_scrollbar, NULL, WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0, state->controls.edit_passwords, NULL, NULL, NULL);
	SendMessage(VScrollControl, U_SB_SET_PLACEMENT, (WPARAM)ScrollBarPlacement::right, 0);

	SendMessageW(state->controls.edit_passwords, EM_SETVSCROLL, (WPARAM)VScrollControl, 0);

	//INFO: I dont yet paint edit controls so you gotta use WM_CTLCOLOREDIT

	SearchInit searchinit;
	searchinit.parent_type = SEARCH_EDIT;
	searchinit.SearchFlag_flags = 0;
#if 0
	searchinit.SearchPlacement_flags = SearchPlacement::right;// SearchPlacement::bottom;
#else
	searchinit.SearchPlacement_flags = SearchPlacement::bottom;
#endif
	HWND SearchControl = CreateWindowExW(NULL, protect_wndclass_search, NULL, WS_CHILD,
		0, 0, 0, 0, state->controls.edit_passwords, NULL, NULL, &searchinit);
	SEARCH_set_brushes(SearchControl, TRUE, unCap_colors.Search_Bk, unCap_colors.Search_Bk, unCap_colors.Search_Txt, unCap_colors.Search_BkPush, unCap_colors.Search_BkMouseOver, unCap_colors.Search_Edit_Bk, unCap_colors.Search_Edit_Txt,unCap_colors.Search_BkSelected);
	SendMessageW(state->controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)SearchControl, 0);

#else //rich edit control
	state->controls.edit_passwords = CreateWindowExW(NULL, get_richedit_classW(), NULL, WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_CLIPCHILDREN | WS_VISIBLE | WS_TABSTOP | ES_NOHIDESEL //| WS_VSCROLL | WS_HSCROLL 
		, 0,0,0,0
		, state->wnd
		, NULL, NULL, NULL);

#define EDIT_PASSWORDS_MAX_TEXT_LENGTH (32767*2) //32767 is the default
	SendMessageW(state->controls.edit_passwords, EM_EXLIMITTEXT,0, EDIT_PASSWORDS_MAX_TEXT_LENGTH); //msg completely different from edit control, good job microsoft

	SetWindowSubclass(state->controls.edit_passwords, EditProc, 0, (DWORD_PTR)calloc(1, sizeof(EditProcState)));

	RICHEDIT_set_bk_color(state->controls.edit_passwords, ColorFromBrush(unCap_colors.ControlBk));
	//RICHEDIT_set_txt_color(state->controls.edit_passwords, ColorFromBrush(unCap_colors.ControlTxt)); //IMPORTANT: this needs to be called each time the text changes
	RICHEDIT_set_txt_bk_color(state->controls.edit_passwords, ColorFromBrush(unCap_colors.ControlBk));

	HWND VScrollControl = CreateWindowExW(NULL, unCap_wndclass_scrollbar, NULL, WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0, state->controls.edit_passwords, NULL, NULL, NULL);
	SendMessage(VScrollControl, U_SB_SET_PLACEMENT, (WPARAM)ScrollBarPlacement::right, 0);
	SendMessageW(state->controls.edit_passwords, EM_SETVSCROLL, (WPARAM)VScrollControl, 0);

	SearchInit searchinit;
	searchinit.parent_type = SEARCH_RICHEDIT;
	searchinit.SearchFlag_flags = 0;
#if 0
	searchinit.SearchPlacement_flags = SearchPlacement::right;// SearchPlacement::bottom;
#else
	searchinit.SearchPlacement_flags = SearchPlacement::bottom;
#endif
	HWND SearchControl = CreateWindowExW(NULL, protect_wndclass_search, NULL, WS_CHILD,
		0, 0, 0, 0, state->controls.edit_passwords, NULL, NULL, &searchinit);
	SEARCH_set_brushes(SearchControl, TRUE, unCap_colors.Search_Bk, unCap_colors.Search_Bk, unCap_colors.Search_Txt, unCap_colors.Search_BkPush, unCap_colors.Search_BkMouseOver, unCap_colors.Search_Edit_Bk, unCap_colors.Search_Edit_Txt);
	SendMessageW(state->controls.edit_passwords, EM_SETSEARCHWND, (WPARAM)SearchControl, 0);
	//TODO(fran): for some reason IME doesnt get correctly set up in the search wnd
	//TODO(fran): enumchildwindows of your parent to make sure you dont overlap with one another
#endif

	for (auto ctl : state->controls.all)
		SendMessage(ctl, WM_SETFONT, (WPARAM)unCap_fonts.General, TRUE);

	SHOWPASSWORDS_resize_controls(state);
}

#define showpasswords_menu_base_addr	200
#define SHOWPASSWORDS_MENU_SAVE			(showpasswords_menu_base_addr+1)
#define SHOWPASSWORDS_MENU_SEPARATOR1	(showpasswords_menu_base_addr+2)
#define SHOWPASSWORDS_MENU_UNDO			(showpasswords_menu_base_addr+3)
#define SHOWPASSWORDS_MENU_REDO			(showpasswords_menu_base_addr+4)
#define SHOWPASSWORDS_MENU_FIND			(showpasswords_menu_base_addr+5)

void SHOWPASSWORDS_add_menus(ShowPasswordsState* state) { //TODO(fran): this should be a toolbar (maybe), toolbars are kinda stupid, just useful till you learn shortcuts https://docs.microsoft.com/en-us/windows/win32/controls/create-toolbars
	//NOTE: each menu gets its parent HMENU stored in the itemData part of the struct
	LANGUAGE_MANAGER::Instance().AddMenuDrawingHwnd(state->wnd);

	//INFO: the top 32 bits of an HMENU are random each execution, in a way, they can actually get set to FFFFFFFF or to 00000000, so if you're gonna check two of those you better make sure you cut the top part in BOTH

	HMENU menu = CreateMenu(); state->menu = menu;
	HMENU menu_file = CreateMenu(); state->menu_file = menu_file;
	HMENU menu_file_lang = CreateMenu(); state->menu_file_lang = menu_file_lang;
	HMENU menu_edit = CreateMenu(); state->menu_edit = menu_edit;
	AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)menu_file, (LPCWSTR)menu);
	AMT(menu, (UINT_PTR)menu_file, LANG_MENU_FILE);

	AppendMenuW(menu_file, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_SAVE, (LPCWSTR)menu_file);
	AMT(menu_file, SHOWPASSWORDS_MENU_SAVE, LANG_MENU_SAVE);

	AppendMenuW(menu_file, MF_SEPARATOR | MF_OWNERDRAW, SHOWPASSWORDS_MENU_SEPARATOR1, (LPCWSTR)menu_file);

	AppendMenuW(menu_file, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)menu_file_lang, (LPCWSTR)menu_file);
	AMT(menu_file, (UINT_PTR)menu_file_lang, LANG_MENU_LANGUAGE);
	//TODO(fran): SetMenuItemInfo only accepts UINT, not the UINT_PTR of MF_POPUP, plz dont tell me I have to redo all of it a different way (LANGUAGE_MANAGER just does it normally not caring for the extra 32 bits)

	HBITMAP bTick = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_CIRCLE));

#define _language_appendtomenu(member,value_expr) \
		AppendMenuW(menu_file_lang, MF_STRING | MF_OWNERDRAW, LANGUAGE::member, (LPCWSTR)menu_file_lang); \
		SetMenuItemString(menu_file_lang, LANGUAGE::member, FALSE, _t(#member)); \
		SetMenuItemBitmaps(menu_file_lang, LANGUAGE::member, MF_BYCOMMAND, NULL, bTick); \

	_foreach_language(_language_appendtomenu)
#undef _language_appendtomenu
	CheckMenuItem(menu_file_lang, LANGUAGE_MANAGER::Instance().GetCurrentLanguage(), MF_BYCOMMAND | MF_CHECKED);

	HBITMAP bEarth = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(UNCAP_BMP_EARTH));

	SetMenuItemBitmaps(menu_file, (UINT)(UINT_PTR)menu_file_lang, MF_BYCOMMAND, bEarth, bEarth);

	AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)menu_edit, (LPCWSTR)menu);
	AMT(menu, (UINT_PTR)menu_edit, LANG_MENU_EDIT);

	AppendMenuW(menu_edit, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_UNDO, (LPCWSTR)menu_edit);
	AMT(menu_edit, SHOWPASSWORDS_MENU_UNDO, LANG_MENU_EDIT_UNDO);

	AppendMenuW(menu_edit, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_REDO, (LPCWSTR)menu_edit);
	AMT(menu_edit, SHOWPASSWORDS_MENU_REDO, LANG_MENU_EDIT_REDO);

	AppendMenuW(menu_edit, MF_STRING | MF_OWNERDRAW, SHOWPASSWORDS_MENU_FIND, (LPCWSTR)menu_edit);
	AMT(menu_edit, SHOWPASSWORDS_MENU_FIND, LANG_MENU_EDIT_FIND);

	UNCAPNC_set_menu(state->nc_parent, state->menu);
}

void SHOWPASSWORDS_save_settings(ShowPasswordsState* state) {
	RECT r; GetWindowRect(state->wnd, &r);
	state->settings->rc = r;
}

//TODO(fran): this may be better placed in Core.h
static bool save_to_file_user(std::wstring username /*functions as a folder*/, void* content, u32 content_sz) {
	constexpr wchar_t filename[] = L"\\tt";
	std::wstring path = get_general_save_folder() + L"\\user_" + username; //INFO: appending user_ saves us from the trouble of the reserved filenames that windows has, eg COM1, LPT1, ...

	CreateDirectoryW(path.c_str(), 0);//Create the folder where info will be stored, since windows wont do it

	SetFileAttributesW(path.c_str(), GetFileAttributesW(path.c_str()) | FILE_ATTRIBUTE_HIDDEN); //some very basic protection

	path += filename;
	bool res = write_entire_file(path.c_str(), content, content_sz);
	return res;
}

static read_entire_file_res load_file_user(std::wstring username /*functions as a folder*/) {
	constexpr wchar_t filename[] = L"\\tt";
	std::wstring path = get_general_save_folder() + L"\\user_" + username;

	CreateDirectoryW(path.c_str(), 0);//Create the folder where info will be stored, since windows wont do it

	SetFileAttributesW(path.c_str(), GetFileAttributesW(path.c_str()) | FILE_ATTRIBUTE_HIDDEN); //some very basic protection

	path += filename;

	auto res = read_entire_file(path.c_str());
	return res;
}

void SHOWPASSWORDS_save_passwords(ShowPasswordsState* state) {
	int user_len_chars = (int)wcslen(state->current_user);
	int len_chars = user_len_chars + GetWindowTextLength(state->controls.edit_passwords) + 1;
	int len_bytes = next_multiple_of_16(len_chars * sizeof(cstr)); //we pad with extra garbage bytes to get blocks of 16 bytes for encryption
	void* mem = malloc(len_bytes); defer{ free(mem); };
	Assert(sizeof(cstr) > 1); //TODO(fran): what to do with ansi?
	wcscpy_s((cstr*)mem, user_len_chars + 1, state->current_user); //append username so we can check against it in later logins (another idea is to append the key structure that twofish stores ) //TODO(fran): this aint the most clever, there could be collisions, but it's at least a line of defense for now
	GetWindowText(state->controls.edit_passwords, ((cstr*)mem) + user_len_chars, len_chars - user_len_chars);
	twofish_encrypt(mem, len_bytes, mem);
	save_to_file_user(state->current_user, mem, len_bytes);
	SHOWPASSWORDS_set_passwords_saved(state, true);
}

LRESULT CALLBACK ShowPasswordsProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	ShowPasswordsState* state = SHOWPASSWORDS_get_state(hwnd);
	switch (msg) {
	case WM_NCCREATE:
	{ //1st msg received
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		Assert(creation_nfo->style & WS_CHILD);

		ShowPasswordsState* st = (ShowPasswordsState*)calloc(1, sizeof(ShowPasswordsState));
		Assert(st);
		st->wnd = hwnd;
		st->nc_parent = GetParent(hwnd);

		st->settings = ((ShowPasswordsData*)creation_nfo->lpCreateParams)->settings;
		st->start = ((ShowPasswordsData*)creation_nfo->lpCreateParams)->start;
		SHOWPASSWORDS_set_passwords_saved(st, true);
		SHOWPASSWORDS_set_state(hwnd, st);
		return TRUE; //continue creation
	} break;
	case WM_CREATE:
	{
		SHOWPASSWORDS_add_controls(state);

		SHOWPASSWORDS_add_menus(state);
	} break;
	case WM_SIZE:
	{
		LRESULT res = DefWindowProc(hwnd, msg, wparam, lparam);
		SHOWPASSWORDS_resize_controls(state);
		return res;
	} break;
	case WM_SAVE:
	{
		HWND ctl = (HWND)wparam;
		if (ctl == state->controls.edit_passwords) {
			SHOWPASSWORDS_save_passwords(state);
			return 0;
		}
	} break;
	case WM_COMMAND:
	{
		HWND child = (HWND)lparam;
		if (child) {//Notif from our childs
			if (child == state->controls.edit_passwords) {
				switch (HIWORD(wparam)) { //Notif code
				case EN_CHANGE:
				{
					SHOWPASSWORDS_set_passwords_saved(state, false);
					return 0;
				}
				}
			}
		}
		else {
			switch (LOWORD(wparam)) {//Menu notifications
			case SHOWPASSWORDS_MENU_SAVE:
			{
				SHOWPASSWORDS_save_passwords(state);
				return 0;
			} break;
#define _generate_enum_cases_language(member,value_expr) case LANGUAGE::member:
			_foreach_language(_generate_enum_cases_language)
			{
				LANGUAGE_MANAGER::Instance().ChangeLanguage((LANGUAGE)LOWORD(wparam));
				HMENU old_menu = GetMenu(state->nc_parent);
				SHOWPASSWORDS_add_menus(state);
				DestroyMenu(old_menu);
				return 0;
			} break;
			case SHOWPASSWORDS_MENU_UNDO:
			{
				SendMessage(state->controls.edit_passwords, EM_UNDO, 0, 0); //TODO(fran): undo is terrible in edit controls, we gotta manage that in the subclass //NOTE: WM_UNDO seems to do the same
			} break;
			case SHOWPASSWORDS_MENU_REDO:
			{

			} break;
			case SHOWPASSWORDS_MENU_FIND:
			{
				SendMessage(state->controls.edit_passwords, EM_SHOWSEARCHWND, TRUE, 0);
				return 0;
			} break;
			default: return DefWindowProc(hwnd, msg, wparam, lparam);
			}
		}
		return 0;
	} break;
	case WM_START:
	{
		twofish_setkey(state->start->key, sizeof(state->start->key));
		ZeroMemory(state->start->key, sizeof(state->start->key));

		//We gotta keep this data to be able to save the file later, probably some more secure/intelligent ways exist
		state->current_user = (wchar_t*)malloc((state->start->username.sz_chars + 1) * sizeof(*state->start->username.str));
		state->current_user[state->start->username.sz_chars] = 0; //append null terminator, unfortunately the rest of the code isnt yet working with the "text" struct
		memcpy(state->current_user, state->start->username.str, state->start->username.sz_chars * sizeof(*state->start->username.str));
		//TODO(fran): free

		auto file_read = load_file_user(state->current_user); defer{ free_file_memory(file_read.mem); };//TODO(fran): zero
		bool set_passwords_saved = true;
		if (file_read.mem) {
			Assert(file_read.sz % 16 == 0);
			twofish_decrypt(file_read.mem, file_read.sz, file_read.mem);
			if (!wcsncmp(state->current_user, (wchar_t*)file_read.mem, min(state->start->username.sz_chars, file_read.sz / 2 /*byte to wchar*/))) { //Valid password, user inputted username matches stored username
				SetWindowText(state->controls.edit_passwords, ((cstr*)file_read.mem) + state->start->username.sz_chars); //TODO(fran): what did I decide for the data's encoding?
				RICHEDIT_set_txt_color(state->controls.edit_passwords, ColorFromBrush(unCap_colors.ControlTxt)); //TODO(fran): do this in a subclass
				//TODO(fran): set keyboard focus to the edit control, SetFocus doesnt work here, maybe cause we arent visible yet?

			}
			else { //Invalid password
				MessageBox(0, RCS(LANG_ERROR_PASSWORD), RCS(LANG_ERROR), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);//IMPORTANT INFO: I have NO IDEA why but if you PostMessage first and then MessageBox the msg will get lost and the WM_NEXT never gets posted
				PostMessage(0, WM_NEXT, 0, 0);
			}
		}
		else {
			//NOTE: if the user previously created an account but didnt save then it will not count as a created account and next time they will be prompted to create the account again, this is a limitation of the fact that we dont save anything inside the user folder till the first time they save what they wrote, therefore we cannot currently do this any other way since there's no information inside the folder to allow us to check whether the second time the user input the same password as the first time
			utf16 tmp[100];
			_snwprintf_s(tmp, ARRAYSIZE(tmp)-(state->start->username.sz_chars+1), RCS(LANG_CREATEACCOUNT), state->current_user);
			auto ret = MessageBox(0, tmp, RCS(LANG_SIGNUP), MB_YESNOCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND);
			bool signup = ret == IDYES;
			if (signup) set_passwords_saved = false;
			else PostMessage(0, WM_NEXT, 0, 0);
			
		}
		SHOWPASSWORDS_set_passwords_saved(state, set_passwords_saved);
		return 0;
	} break;
	case WM_RESET:
	{
		if (state->current_user) {
			free(state->current_user);//No real need to zero the memory before freeing
			state->current_user = nullptr;
		}
		SetWindowText(state->controls.edit_passwords, _t(""));//TODO(fran): yet again, we need to zero the mem also
		return 0;
	} break;
	case WM_CLOSE: //Sent by our parent asking whether we want to handle it
	{
		bool client_handled = false;
		if (!state->passwords_saved) {
			int ret = MessageBox(state->wnd, RCS(LANG_UNSAVEDCHANGES_TXT), RCS(LANG_UNSAVEDCHANGES_TITLE), MB_YESNOCANCEL | MB_ICONWARNING | MB_SETFOREGROUND | MB_APPLMODAL);
			switch (ret) {
			case IDCANCEL:
			{
				client_handled = true;
			} break;
			case IDYES:
			{
				SHOWPASSWORDS_save_passwords(state);
			} break;
			case IDNO:
			{
				//Do nothing and let the nc handle the msg
			} break;
			default:client_handled = true; break;
			}
		}
		return client_handled;
	} break;
	case WM_NCDESTROY:
	{
		SHOWPASSWORDS_save_settings(state);
		if (state) {
			if (state->current_user) {
				free(state->current_user);
				state->current_user = nullptr;
			}
			free(state);
			state = nullptr;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_CTLCOLOREDIT:
	{
		HWND ctl = (HWND)lparam;
		HDC dc = (HDC)wparam;
		if (ctl == state->controls.edit_passwords)
		{
			SetBkColor(dc, ColorFromBrush(unCap_colors.ControlBk));
			SetTextColor(dc, ColorFromBrush(unCap_colors.ControlTxt));
			return (LRESULT)unCap_colors.ControlBk;
		}
		else return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	default: return DefWindowProc(hwnd, msg, wparam, lparam); break;
	}
	return 0;
}

ATOM init_wndclass_protect_show_passwords(HINSTANCE inst) {
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(wcex);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = ShowPasswordsProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(ShowPasswordsState*);
	wcex.hInstance = inst;
	wcex.hIcon = LoadIcon(inst, MAKEINTRESOURCE(MYPASS_ICO_LOGO)); //TODO(fran): LoadImage to choose the best size
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = unCap_colors.ControlBk;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = protect_wndclass_show_passwords;
	wcex.hIconSm = LoadIcon(inst, MAKEINTRESOURCE(MYPASS_ICO_LOGO)); //TODO(fran): LoadImage to choose the best size

	ATOM class_atom = RegisterClassExW(&wcex);
	Assert(class_atom);
	return class_atom;
}