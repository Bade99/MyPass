#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include "helpers.h"
#include "reflection.h"
#include "serialization.h"

//Request string
#define RS(stringID) LanguageManager::Instance().RequestString(stringID)

//Request c-string
#define RCS(stringID) LanguageManager::Instance().RequestString(stringID).c_str()

//Add window string
#define AWT(hwnd,stringID) LanguageManager::Instance().AddWindowText(hwnd, stringID)

//Add window default string
#define AWDT(hwnd,stringID) LanguageManager::Instance().AddWindowDefaultText(hwnd, stringID)

//Add combobox string in specific ID (Nth element of the list)
#define ACT(hwnd,ID,stringID) LanguageManager::Instance().AddComboboxText(hwnd, ID, stringID);

//Add menu string
#define AMT(hmenu,ID,stringID) LanguageManager::Instance().AddMenuText(hmenu,ID,stringID);

//Add dynamic window (gets notified with this message when the language changes)
//One common use case could be using a WM_SIZE message to trigger a full re-render
//Or WM_PAINT: which the manager reinterprets into a call to ask_window_for_repaint
#define AWDYN(hwnd,msgID) LanguageManager::Instance().AddDynamicText(hwnd, msgID);

enum Language
{
#define _foreach_language(op) \
				op(English,=1)  \
				op(Español,)  \

	_foreach_language(_generate_enum_member)
};


namespace userial {
	static str serialize(Language v) {
		switch (v) {
			_foreach_language(_string_enum_case);
		default: return L"";
		}
	}
	static bool deserialize(Language& var, str name, const str& content) {
		str start = name + _keyvaluesepartor;
		size_t s = find_identifier(content, 0, start);
		if (str_found(s)) {
			s += start.size();
			str substr = content.substr(s);//TODO(fran):we dont really need a substr and neither till the end of content
			bool found = false;
#define _compare_enum_string_found(member,value) if(!substr.compare(0,str(_t(#member)).size(),_t(#member))){var = member; found=true;} else
			_foreach_language(_compare_enum_string_found) {/*you can do something on the final else case*/ };

			return found;
		}
		return false;
	}
}

class LanguageManager
{
private:

#define _foreach_LANGUAGE_MANAGER_member(op) \
		op(Language,language,Language::English) \

	_foreach_LANGUAGE_MANAGER_member(_generate_member)

public:

	static int GetLanguageStringIndex(Language lang) {
		int idx = 0;
#define _language_add_or_return(member,value) if(lang==member)return idx;else ++idx;
		_foreach_language(_language_add_or_return);
#undef _language_add_or_return
		return 0;
	}

	static bool IsValidLanguage(Language lang) {

		switch (lang) {
			_foreach_language(_isvalid_enum_case)
		default: return false;
		}
	}

	static LanguageManager& Instance()
	{
		static LanguageManager instance;

		return instance;
	}
	LanguageManager(LanguageManager const&) = delete;
	void operator=(LanguageManager const&) = delete;

	//INFO: all Add... functions send the text update message when called, for dynamic hwnds you should create everything first and only then add it

	//·Adds the hwnd to the list of managed hwnds and sets its text corresponding to stringID and the current language
	//·Many hwnds can use the same stringID
	//·Next time there is a language change the window will be automatically updated
	//·Returns FALSE if invalid hwnd or stringID //TODO(fran): do the stringID check
	BOOL AddWindowText(HWND hwnd, UINT stringID)
	{
		BOOL res = UpdateHwnd(hwnd, stringID);
		if (res) this->Hwnds[hwnd] = stringID;
		return res;
	}

	//·Adds the hwnd to the list of managed hwnds and sets its default text corresponding to stringID and the current language
	//·Many hwnds can use the same stringID
	//·Next time there is a language change the window will be automatically updated
	//·Returns FALSE if invalid hwnd or stringID //TODO(fran): do the stringID check
	BOOL AddWindowDefaultText(HWND hwnd, u32 stringID) {
		BOOL res = UpdateHwndDefault(hwnd, stringID);
		if (res) this->HwndsDefault[hwnd] = stringID;
		return res;
	}

	//·Adds the hwnd to the list of managed hwnds and sets its tooltip text corresponding to stringID and the current language
	//·Many hwnds can use the same stringID
	//·Next time there is a language change the window will be automatically updated
	//·Returns FALSE if invalid hwnd or stringID //TODO(fran): do the stringID check
	BOOL AddWindowTooltipText(HWND hwnd, u32 stringID) {
		BOOL res = UpdateHwndTooltip(hwnd, stringID);
		if (res) this->HwndsTooltip[hwnd] = stringID;
		return res;
	}

	//·Updates all managed objects to the new language, all the ones added after this call will also use the new language
	//·On success returns the new LANGID (language) //TODO(fran): should I return the previous langid? it feels more useful
	//·On failure returns (LANGID)-2 if the language is invalid, (LANGID)-3 if failed to change the language
	LANGID ChangeLanguage(Language newLang)
	{
		//if (newLang == this->CurrentLanguage) return -1;//TODO: negative values wrap around to huge values, I assume not all lcid values are valid, find out if these arent
		BOOL res = this->IsValidLanguage(newLang);
		if (!res) return (LANGID)-2;

		this->language = newLang;
		//LCID previousLang = SetThreadLocale(this->GetLCID(newLang));
		//INFO: thanks https://www.curlybrace.com/words/2008/06/10/setthreadlocale-and-setthreaduilanguage-for-localization-on-windows-xp-and-vista/
		// SetThreadLocale has no effect on Vista and above
		LANGID newLANGID = this->GetLANGID(newLang);
		//INFO: On non-Vista platforms, SetThreadLocale can be used. Instead of a language identifier, it accepts a locale identifier
		LANGID lang_res = SetThreadUILanguage(newLANGID); //If successful returns the same value that you sent it

		for (auto const& hwnd_sid : this->Hwnds)
			this->UpdateHwnd(hwnd_sid.first, hwnd_sid.second);

		for (auto const& hwnd_sid : this->HwndsDefault)
			this->UpdateHwndDefault(hwnd_sid.first, hwnd_sid.second);

		for (auto const& hwnd_sid : this->HwndsTooltip)
			this->UpdateHwndTooltip(hwnd_sid.first, hwnd_sid.second);

		for (auto const& hwnd_id_sid : this->Comboboxes)
			this->UpdateCombo(hwnd_id_sid.first.first, hwnd_id_sid.first.second, hwnd_id_sid.second);

		for (auto const& hwnd_msg : this->DynamicHwnds) {
			this->UpdateDynamicHwnd(hwnd_msg.first, hwnd_msg.second);
		}

		for (auto const& hmenu_id_sid : this->Menus) {
			this->UpdateMenu(hmenu_id_sid.first.first, hmenu_id_sid.first.second, hmenu_id_sid.second);
		}
		for (auto const& menudrawer : this->MenuDrawers) {
			DrawMenuBar(menudrawer);
			UpdateWindow(menudrawer);
		}

		return (lang_res == newLANGID ? lang_res : -3);//is this NULL when failed?
	}

	//·Returns the requested string in the current language
	//·If stringID is invalid returns L"" //TODO(fran): check this is true
	//INFO: uses temporary string that lives till the end of the full expression it appears in
	std::wstring RequestString(UINT stringID)
	{
		std::wstring res;
		const utf16* text;
		auto char_cnt = LoadStringW(this->hInstance, stringID, (LPWSTR)&text, 0);
		if (char_cnt) res = std::wstring(text, char_cnt);
		else res = str(L"INVALID ID ") + to_str(stringID);
		return res;
	}

	//·Adds the hwnd to the list of managed comboboxes and sets its text for the specified ID(element in the list) corresponding to stringID and the current language
	//·Next time there is a language change the window will be automatically updated
	BOOL AddComboboxText(HWND hwnd, UINT ID, UINT stringID)
	{
		BOOL res = UpdateCombo(hwnd, ID, stringID);
		if (res) this->Comboboxes[std::make_pair(hwnd, ID)] = stringID;
		return res;
	}

	//Set the hinstance from where the string resources will be retrieved
	HINSTANCE SetHInstance(HINSTANCE hInst)
	{
		HINSTANCE oldHInst = this->hInstance;
		this->hInstance = hInst;
		return oldHInst;
	}

	HINSTANCE GetHInstance() { return this->hInstance; };

	//Add a control that manages other windows' text where the string changes
	//Each time there is a language change we will send a message with the specified code so the window can update its control's text
	//One hwnd can only be linked to one messageID
	BOOL AddDynamicText(HWND hwnd, UINT messageID)
	{
		if (!hwnd) return FALSE;
		this->DynamicHwnds[hwnd] = messageID;
		this->UpdateDynamicHwnd(hwnd, messageID);
		return TRUE;
	}

	//Menus are not draw by themselves, instead their owner window draws them, so if you want a menu to be redrawn you need to use this function
	//to indicate which window to call for its menus to be redrawn
	BOOL AddMenuDrawingHwnd(HWND MenuDrawer)
	{
		//TODO(fran): check HWND is valid
		if (MenuDrawer) {
			this->MenuDrawers.push_back(MenuDrawer);
			return TRUE;
		}
		return FALSE;
	}

	BOOL AddMenuText(HMENU hmenu, UINT_PTR ID, UINT stringID)
	{
		BOOL res = UpdateMenu(hmenu, ID, stringID);
		if (res) this->Menus[std::make_pair(hmenu, ID)] = stringID;
		return res;
	}

	Language GetCurrentLanguage()
	{
		return this->language;
	}

	_generate_default_struct_serialize(_foreach_LANGUAGE_MANAGER_member);

	bool deserialize(str name, const str& content) {
		bool res = false;
		str start = name + _keyvaluesepartor + _structbegin + _newline;
		size_t s = find_identifier(content, 0, start);
		size_t e = find_closing_str(content, s + start.size(), _structbegin, _structend);
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			_foreach_LANGUAGE_MANAGER_member(_deserialize_member);
			res = true;
		}
		this->ChangeLanguage(this->language);//whatever happens we need to set some language
		return res;
	}

private:
	LanguageManager() {}
	~LanguageManager() {}

	HINSTANCE hInstance = NULL;

	std::map<HWND, UINT> Hwnds;
	std::map<HWND, UINT> HwndsDefault;
	std::map<HWND, UINT> HwndsTooltip;
	std::map<HWND, UINT> DynamicHwnds;
	std::map<std::pair<HWND, UINT>, UINT> Comboboxes;

	std::vector<HWND> MenuDrawers;
	std::map<std::pair<HMENU, UINT_PTR>, UINT> Menus;
	//Add list of hwnd that have dynamic text, therefore need to know when there was a lang change to update their text

	BOOL UpdateHwnd(HWND hwnd, UINT stringID)
	{
		BOOL res = SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)this->RequestString(stringID).c_str()) == TRUE;
		ask_window_for_repaint(hwnd);
		return res;
	}

	BOOL UpdateHwndDefault(HWND hwnd, u32 stringID)
	{
		BOOL res = SendMessage(hwnd, WM_SETDEFAULTTEXT, 0, (LPARAM)this->RequestString(stringID).c_str()) == TRUE;
		ask_window_for_repaint(hwnd);
		return res;
	}

	BOOL UpdateHwndTooltip(HWND hwnd, u32 stringID)
	{
		BOOL res = SendMessage(hwnd, WM_SETTOOLTIPTEXT, 0, (LPARAM)this->RequestString(stringID).c_str()) == TRUE;
		return res;
	}

	BOOL UpdateDynamicHwnd(HWND hwnd, UINT messageID)
	{
		if (messageID == WM_PAINT) { ask_window_for_repaint(hwnd); return true; }
		else return PostMessage(hwnd, messageID, 0, 0);
	}

	BOOL UpdateCombo(HWND hwnd, UINT ID, UINT stringID)
	{
		UINT currentSelection = (UINT)SendMessage(hwnd, CB_GETCURSEL, 0, 0);//TODO(fran): is there truly no better solution than having to do all this just to change a string?
		SendMessage(hwnd, CB_DELETESTRING, ID, 0);
		LRESULT res = SendMessage(hwnd, CB_INSERTSTRING, ID, (LPARAM)this->RequestString(stringID).c_str());
		if (currentSelection != CB_ERR) {
			SendMessage(hwnd, CB_SETCURSEL, currentSelection, 0);
			ask_window_for_repaint(hwnd);
		}
		return res != CB_ERR && res != CB_ERRSPACE;//TODO(fran): can I check for >=0 with lresult?
	}

	BOOL UpdateMenu(HMENU hmenu, UINT_PTR ID, UINT stringID)
	{
		MENUITEMINFOW menu_setter;
		menu_setter.cbSize = sizeof(menu_setter);
		menu_setter.fMask = MIIM_STRING;
		std::wstring temp_text = this->RequestString(stringID);
		//menu_setter.dwTypeData = _wcsdup(this->RequestString(stringID).c_str()); //TODO(fran): can we avoid dupping, if not free memory
		menu_setter.dwTypeData = (LPWSTR)temp_text.c_str();
		BOOL res = SetMenuItemInfoW(hmenu, (UINT)ID, FALSE, &menu_setter);
		return res;
	}

	LCID GetLCID(Language lang)
	{
		switch (lang) {
		case Language::English:
			return MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT); //TODO(fran):this is deprecated and not great for macros, unless we set each enum to this values
		case Language::Español:
			return MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH), SORT_DEFAULT);
		default:
			return NULL;
		}
	}

	LANGID GetLANGID(Language lang)
	{
		//INFO: https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings
		switch (lang) {
		case Language::English:
			return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);//TODO(fran): same as GetLCID
		case Language::Español:
			return MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH);
		default:
			return NULL;
		}
	}
};

namespace userial {
	static str serialize(LanguageManager& var) {//NOTE: we gotta add this manually cause when need a reference since the lang mgr is a singleton and doesnt allow for instancing
		return var.serialize();
	}
	static bool deserialize(LanguageManager& var, str name, const str& content) {
		return var.deserialize(name, content);
	}
}
