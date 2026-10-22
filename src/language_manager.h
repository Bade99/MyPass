#pragma once
#include <windows.h>
#include <CommCtrl.h>
#include <map>
#include <set>
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

// Follows ISO 639-1 standard with c1 and c2, eg c1='e', c2='n' matches with English
// c3 may be a dash (-) or a null terminator (\0), in which case it is ignored. Or a valid character in which case the language search will be from ISO 639-2
// Returns 0 if not found (which is not a valid Language)
Language from_language_name(utf16 c1, utf16 c2, utf16 c3) {
	static_assert(_foreach_language(_generate_count) == 2);
	auto pack_chars_in_u32 = [](utf8 c1, utf8 c2, utf8 c3 = 0) { return (((u32)c1) << 16) | (((u32)c2) << 8) | c3; };
	switch (pack_chars_in_u32(tolower(c1), tolower(c2), isalpha(c3) ? tolower(c3) : 0)) {
	case pack_chars_in_u32('e','n'): return Language::English;
	case pack_chars_in_u32('e','s'): return Language::Español;
	default: return (Language)0;
	}
}


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
			str_view substr = str_view(content).substr(s);
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

#define _foreach_language_manager_member(op) \
		op(Language,language,Language::English) \

	_foreach_language_manager_member(_generate_member)

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
	//·Returns FALSE if invalid hwnd or stringID
	BOOL AddWindowText(HWND hwnd, UINT stringID)
	{
		return AddWindowRegistration(this->Hwnds, hwnd, hwnd, stringID, [this, hwnd, stringID] {
			return UpdateHwnd(hwnd, stringID);
		});
	}

	//·Adds the hwnd to the list of managed hwnds and sets its default text corresponding to stringID and the current language
	//·Many hwnds can use the same stringID
	//·Next time there is a language change the window will be automatically updated
	//·Returns FALSE if invalid hwnd or stringID
	BOOL AddWindowDefaultText(HWND hwnd, u32 stringID) {
		return AddWindowRegistration(this->HwndsDefault, hwnd, hwnd, stringID, [this, hwnd, stringID] {
			return UpdateHwndDefault(hwnd, stringID);
		});
	}

	//·Adds the hwnd to the list of managed hwnds and sets its tooltip text corresponding to stringID and the current language
	//·Many hwnds can use the same stringID
	//·Next time there is a language change the window will be automatically updated
	//·Returns FALSE if invalid hwnd or stringID
	BOOL AddWindowTooltipText(HWND hwnd, u32 stringID) {
		return AddWindowRegistration(this->HwndsTooltip, hwnd, hwnd, stringID, [this, hwnd, stringID] {
			return UpdateHwndTooltip(hwnd, stringID);
		});
	}

	//·Updates all managed objects to the new language, all the ones added after this call will also use the new language
	//·On success returns the new LANGID (language) //TODO(fran): should I return the previous langid? it feels more useful
	//·On failure returns (LANGID)-2 if the language is invalid, (LANGID)-3 if failed to change the language
	LANGID ChangeLanguage(Language newLang)
	{
		BOOL res = this->IsValidLanguage(newLang);
		if (!res) return (LANGID)-2;

		this->language = newLang;
		//INFO: thanks https://www.curlybrace.com/words/2008/06/10/setthreadlocale-and-setthreaduilanguage-for-localization-on-windows-xp-and-vista/
		// SetThreadLocale has no effect on Vista and above
		LANGID newLANGID = this->GetLANGID(newLang);
		LANGID lang_res = SetThreadUILanguage(newLANGID); //If successful returns the same value that you sent it

		ForEachCurrentRegistration(this->Hwnds, [this](HWND hwnd, UINT stringID) {
			UpdateHwnd(hwnd, stringID);
		});

		ForEachCurrentRegistration(this->HwndsDefault, [this](HWND hwnd, UINT stringID) {
			UpdateHwndDefault(hwnd, stringID);
		});

		ForEachCurrentRegistration(this->HwndsTooltip, [this](HWND hwnd, UINT stringID) {
			UpdateHwndTooltip(hwnd, stringID);
		});

		ForEachCurrentRegistration(this->Comboboxes, [this](const std::pair<HWND, UINT>& hwnd_id, UINT stringID) {
			UpdateCombo(hwnd_id.first, hwnd_id.second, stringID);
		});

		//TODO(fran): Notify each managed control's layout owner once after updating translated text,
		//so size-dependent controls relayout and repaint without callers handling language changes.
		//AddDynamicText currently requires manual registration and allows only one message per HWND.
		ForEachCurrentRegistration(this->DynamicHwnds, [this](HWND hwnd, UINT messageID) {
			UpdateDynamicHwnd(hwnd, messageID);
		});

		for (auto const& hmenu_id_sid : this->Menus) {
			this->UpdateMenu(hmenu_id_sid.first.first, hmenu_id_sid.first.second, hmenu_id_sid.second);
		}
		const auto menu_drawers = this->MenuDrawers;
		for (HWND menudrawer : menu_drawers) {
			if (!this->MenuDrawers.contains(menudrawer)) continue;
			DrawMenuBar(menudrawer);
			if (this->MenuDrawers.contains(menudrawer)) UpdateWindow(menudrawer);
		}

		return (lang_res == newLANGID ? lang_res : -3);//is this NULL when failed?
	}

	//·Returns the requested string in the current language
	//·If stringID is invalid returns "INVALID ID " followed by the ID
	//INFO: uses temporary string that lives till the end of the full expression it appears in
	str RequestString(UINT stringID)
	{
		str res;
		const cstr* text;
		auto char_cnt = LoadString(this->hInstance, stringID, (cstr*)&text, 0);
		if (char_cnt) res = str(text, char_cnt);
		else res = str(L"INVALID ID ") + to_str(stringID);
		return res;
	}

	//·Adds the hwnd to the list of managed comboboxes and sets its text for the specified ID(element in the list) corresponding to stringID and the current language
	//·Next time there is a language change the window will be automatically updated
	BOOL AddComboboxText(HWND hwnd, UINT ID, UINT stringID)
	{
		return AddWindowRegistration(this->Comboboxes, hwnd, std::make_pair(hwnd, ID), stringID, [this, hwnd, ID, stringID] {
			return UpdateCombo(hwnd, ID, stringID);
		});
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
		if (!TrackWindow(hwnd)) return FALSE;
		this->DynamicHwnds[hwnd] = messageID;
		this->UpdateDynamicHwnd(hwnd, messageID);
		return TRUE;
	}

	//Menus are not draw by themselves, instead their owner window draws them, so if you want a menu to be redrawn you need to use this function
	//to indicate which window to call for its menus to be redrawn
	BOOL AddMenuDrawingHwnd(HWND MenuDrawer)
	{
		if (!TrackWindow(MenuDrawer)) return FALSE;
		this->MenuDrawers.insert(MenuDrawer);
		return TRUE;
	}

	BOOL AddMenuText(HMENU hmenu, UINT_PTR ID, UINT stringID)
	{
		BOOL res = UpdateMenu(hmenu, ID, stringID);
		if (res) this->Menus[std::make_pair(hmenu, ID)] = stringID;
		return res && HasStringResource(stringID);
	}

	Language GetCurrentLanguage()
	{
		return this->language;
	}

	_generate_default_struct_serialize(_foreach_language_manager_member);

	bool deserialize(str name, const str& content) {
		bool res = false;
		str start = name + _keyvaluesepartor + _structbegin + _newline;
		size_t s = find_identifier(content, 0, start);
		size_t e = find_closing_str(content, s + start.size(), _structbegin, _structend);
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			_foreach_language_manager_member(_deserialize_member);
			res = true;
		}
		else this->language = this->GetBestMatchFromSystemSettings();
		this->ChangeLanguage(this->language);//whatever happens we need to set some language
		return res;
	}

private:
	LanguageManager() {}
	~LanguageManager() {
		for (const auto& tracked : this->TrackedWindows)
			RemoveWindowSubclass(tracked.first, WindowSubclassProc, 0);
	}

	HINSTANCE hInstance = NULL;
	bool HasStringResource(UINT stringID) const {
		const cstr* text;
		return LoadString(this->hInstance, stringID, (cstr*)&text, 0) != 0;
	}

	std::map<HWND, UINT> Hwnds;
	std::map<HWND, UINT> HwndsDefault;
	std::map<HWND, UINT> HwndsTooltip;
	std::map<HWND, UINT> DynamicHwnds;
	std::map<std::pair<HWND, UINT>, UINT> Comboboxes;

	std::map<HWND, size_t> TrackedWindows;
	size_t next_window_registration = 0;
	std::set<HWND> MenuDrawers;
	std::map<std::pair<HMENU, UINT_PTR>, UINT> Menus;

	static LRESULT CALLBACK WindowSubclassProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR ref_data)
	{
		if (message == WM_NCDESTROY) {
			auto& manager = *reinterpret_cast<LanguageManager*>(ref_data);
			manager.UnregisterWindow(hwnd);
			RemoveWindowSubclass(hwnd, WindowSubclassProc, subclass_id);
		}
		return DefSubclassProc(hwnd, message, wparam, lparam);
	}

	size_t TrackWindow(HWND hwnd)
	{
		if (GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId()) return 0;
		if (auto tracked = this->TrackedWindows.find(hwnd); tracked != this->TrackedWindows.end()) return tracked->second;
		if (!SetWindowSubclass(hwnd, WindowSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this))) return 0;
		size_t registration = ++this->next_window_registration;
		this->TrackedWindows[hwnd] = registration;
		return registration;
	}

	bool IsTrackedWindow(HWND hwnd, size_t registration) const
	{
		auto tracked = this->TrackedWindows.find(hwnd);
		return tracked != this->TrackedWindows.end() && tracked->second == registration;
	}

	void UnregisterWindow(HWND hwnd)
	{
		this->TrackedWindows.erase(hwnd);
		this->Hwnds.erase(hwnd);
		this->HwndsDefault.erase(hwnd);
		this->HwndsTooltip.erase(hwnd);
		this->DynamicHwnds.erase(hwnd);
		auto combo = this->Comboboxes.lower_bound(std::make_pair(hwnd, 0u));
		while (combo != this->Comboboxes.end() && combo->first.first == hwnd)
			combo = this->Comboboxes.erase(combo);
		this->MenuDrawers.erase(hwnd);
	}

	template<typename Key, typename Update>
	BOOL AddWindowRegistration(std::map<Key, UINT>& registrations, HWND hwnd, const Key& key, UINT stringID, Update&& update)
	{
		size_t registration = TrackWindow(hwnd);
		if (!registration) return FALSE;
		BOOL res = update();
		if (!IsTrackedWindow(hwnd, registration)) return FALSE;
		if (res) registrations[key] = stringID;
		return res && HasStringResource(stringID);
	}

	static HWND RegisteredHwnd(HWND hwnd) { return hwnd; }
	static HWND RegisteredHwnd(const std::pair<HWND, UINT>& hwnd_id) { return hwnd_id.first; }

	template<typename Registry, typename Update>
	void ForEachCurrentRegistration(const Registry& registrations, Update&& update)
	{
		struct SnapshotEntry {
			typename Registry::key_type key;
			UINT value;
			size_t window_registration;
		};
		std::vector<SnapshotEntry> snapshot;
		snapshot.reserve(registrations.size());
		for (const auto& [key, value] : registrations) {
			auto tracked = this->TrackedWindows.find(RegisteredHwnd(key));
			if (tracked != this->TrackedWindows.end()) snapshot.push_back({ key, value, tracked->second });
		}
		for (const auto& entry : snapshot) {
			if (!IsTrackedWindow(RegisteredHwnd(entry.key), entry.window_registration)) continue;
			auto current = registrations.find(entry.key);
			if (current != registrations.end() && current->second == entry.value) update(entry.key, entry.value);
		}
	}

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
		auto tracked = this->TrackedWindows.find(hwnd);
		if (tracked == this->TrackedWindows.end()) return FALSE;
		size_t registration = tracked->second;
		UINT currentSelection = (UINT)SendMessage(hwnd, CB_GETCURSEL, 0, 0);//TODO(fran): is there truly no better solution than having to do all this just to change a string?
		if (!IsTrackedWindow(hwnd, registration)) return FALSE;
		SendMessage(hwnd, CB_DELETESTRING, ID, 0);
		if (!IsTrackedWindow(hwnd, registration)) return FALSE;
		LRESULT res = SendMessage(hwnd, CB_INSERTSTRING, ID, (LPARAM)this->RequestString(stringID).c_str());
		if (!IsTrackedWindow(hwnd, registration)) return FALSE;
		if (currentSelection != CB_ERR) {
			SendMessage(hwnd, CB_SETCURSEL, currentSelection, 0);
			if (!IsTrackedWindow(hwnd, registration)) return FALSE;
			ask_window_for_repaint(hwnd);
		}
		return res >= 0;
	}

	BOOL UpdateMenu(HMENU hmenu, UINT_PTR ID, UINT stringID)
	{
		MENUITEMINFO menu_setter;
		menu_setter.cbSize = sizeof(MENUITEMINFO);
		menu_setter.fMask = MIIM_STRING;
		str temp_text = this->RequestString(stringID);
		menu_setter.dwTypeData = (cstr*)temp_text.c_str();
		BOOL res = SetMenuItemInfo(hmenu, (UINT)ID, FALSE, &menu_setter);
		return res;
	}

	LANGID GetLANGID(Language lang)
	{
		//INFO: https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings
		switch (lang) {
		case Language::English:
			return MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
		case Language::Español:
			return MAKELANGID(LANG_SPANISH, SUBLANG_NEUTRAL);
		default:
			Assert(0);
			return NULL;
		}
	}

	Language GetBestMatchFromSystemSettings() {
		Language res = Language::English;

		utf16 preferences[128]; preferences[0] = 0;
		ULONG prefsz = ARRAYSIZE(preferences), _;
		auto ret = GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &_, preferences, &prefsz);

		//TODO(fran): fallback to GetUserDefaultUILanguage if OS lower than Windows Vista

		if (ret) {
			for(size_t i = 0; i + 3 < prefsz && preferences[i]; i++) {
				Language lang = from_language_name(preferences[i], preferences[i + 1], preferences[i + 2]);
				if (lang) {
					res = lang;
					break;
				}
				i += wcslen(&preferences[i]);
			}
		}
		return res;
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
