#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include "unCap_Helpers.h"
#include "unCap_Reflection.h"
#include "unCap_Serialization.h"

//Request string
#define RS(stringID) LANGUAGE_MANAGER::Instance().RequestString(stringID)

//Request c-string
#define RCS(stringID) LANGUAGE_MANAGER::Instance().RequestString(stringID).c_str()

//Add window string
#define AWT(hwnd,stringID) LANGUAGE_MANAGER::Instance().AddWindowText(hwnd, stringID)

//Add combobox string in specific ID (Nth element of the list)
#define ACT(hwnd,ID,stringID) LANGUAGE_MANAGER::Instance().AddComboboxText(hwnd, ID, stringID);

//Add menu string
#define AMT(hmenu,ID,stringID) LANGUAGE_MANAGER::Instance().AddMenuText(hmenu,ID,stringID);

enum LANGUAGE
{
#define _foreach_language(op) \
				op(English,=1)  \
				op(Español,)  \

	_foreach_language(_generate_enum_member)
};


namespace userial {
	static str serialize(LANGUAGE v) {
		switch (v) {
			_foreach_language(_string_enum_case);
		default: return L"";
		}
	}
	static bool deserialize(LANGUAGE& var, str name, const str& content) {
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

class LANGUAGE_MANAGER
{
public:


private:

#define _foreach_LANGUAGE_MANAGER_member(op) \
		op(LANGUAGE,CurrentLanguage,LANGUAGE::English) \

	_foreach_LANGUAGE_MANAGER_member(_generate_member)

public:

	static int GetLanguageStringIndex(LANGUAGE lang) {
		int idx = 0;
#define _language_add_or_return(member,value) if(lang==member)return idx;else ++idx;
		_foreach_language(_language_add_or_return);
#undef _language_add_or_return
		return 0;
	}

	static bool IsValidLanguage(LANGUAGE lang) {

		switch (lang) {
			_foreach_language(_isvalid_enum_case)
		default: return false;
		}
	}

	static LANGUAGE_MANAGER& Instance()
	{
		static LANGUAGE_MANAGER instance;

		return instance;
	}
	LANGUAGE_MANAGER(LANGUAGE_MANAGER const&) = delete;
	void operator=(LANGUAGE_MANAGER const&) = delete;

	//INFO: all Add... functions send the text update message when called, for dynamic hwnds you should create everything first and only then add it

	//·Adds the hwnd to the list of managed hwnds and sets its text corresponding to stringID and the current language
	//·Many hwnds can use the same stringID
	//·Next time there is a language change the window will be automatically updated
	//·Returns FALSE if invalid hwnd or stringID //TODO(fran): do the stringID check
	BOOL AddWindowText(HWND hwnd, UINT stringID);

	//·Updates all managed objects to the new language, all the ones added after this call will also use the new language
	//·On success returns the new LANGID (language) //TODO(fran): should I return the previous langid? it feels more useful
	//·On failure returns (LANGID)-2 if the language is invalid, (LANGID)-3 if failed to change the language
	LANGID ChangeLanguage(LANGUAGE newLang);

	//·Returns the requested string in the current language
	//·If stringID is invalid returns L"" //TODO(fran): check this is true
	//INFO: uses temporary string that lives till the end of the full expression it appears in
	std::wstring RequestString(UINT stringID);

	//·Adds the hwnd to the list of managed comboboxes and sets its text for the specified ID(element in the list) corresponding to stringID and the current language
	//·Next time there is a language change the window will be automatically updated
	BOOL AddComboboxText(HWND hwnd, UINT ID, UINT stringID);

	//Set the hinstance from where the string resources will be retrieved
	HINSTANCE SetHInstance(HINSTANCE hInst);

	HINSTANCE GetHInstance() { return this->hInstance; };

	//Add a control that manages other windows' text where the string changes
	//Each time there is a language change we will send a message with the specified code so the window can update its control's text
	//One hwnd can only be linked to one messageID
	BOOL AddDynamicText(HWND hwnd, UINT messageID);

	//Menus are not draw by themselves, instead their owner window draws them, so if you want a menu to be redrawn you need to use this function
	//to indicate which window to call for its menus to be redrawn
	BOOL AddMenuDrawingHwnd(HWND MenuDrawer);

	BOOL AddMenuText(HMENU hmenu, UINT_PTR ID, UINT stringID);

	LANGUAGE GetCurrentLanguage();

	_generate_default_struct_serialize(_foreach_LANGUAGE_MANAGER_member);

	bool deserialize(str name, const str& content);

private:
	LANGUAGE_MANAGER() {}
	~LANGUAGE_MANAGER() {}

	HINSTANCE hInstance = NULL;

	std::map<HWND, UINT> Hwnds;
	std::map<HWND, UINT> DynamicHwnds;
	std::map<std::pair<HWND, UINT>, UINT> Comboboxes;

	std::vector<HWND> MenuDrawers;
	std::map<std::pair<HMENU, UINT_PTR>, UINT> Menus;
	//Add list of hwnd that have dynamic text, therefore need to know when there was a lang change to update their text

	BOOL UpdateHwnd(HWND hwnd, UINT stringID)
	{
		BOOL res = SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)this->RequestString(stringID).c_str()) == TRUE;
		InvalidateRect(hwnd, NULL, TRUE);
		return res;
	}

	BOOL UpdateDynamicHwnd(HWND hwnd, UINT messageID)
	{
		return PostMessage(hwnd, messageID, 0, 0);
	}

	BOOL UpdateCombo(HWND hwnd, UINT ID, UINT stringID)
	{
		UINT currentSelection = (UINT)SendMessage(hwnd, CB_GETCURSEL, 0, 0);//TODO(fran): is there truly no better solution than having to do all this just to change a string?
		SendMessage(hwnd, CB_DELETESTRING, ID, 0);
		LRESULT res = SendMessage(hwnd, CB_INSERTSTRING, ID, (LPARAM)this->RequestString(stringID).c_str());
		if (currentSelection != CB_ERR) {
			SendMessage(hwnd, CB_SETCURSEL, currentSelection, 0);
			InvalidateRect(hwnd, NULL, TRUE);
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

	LCID GetLCID(LANGUAGE lang)
	{
		switch (lang) {
		case LANGUAGE::English:
			return MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT); //TODO(fran):this is deprecated and not great for macros, unless we set each enum to this values
		case LANGUAGE::Español:
			return MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH), SORT_DEFAULT);
		default:
			return NULL;
		}
	}

	LANGID GetLANGID(LANGUAGE lang)
	{
		//INFO: https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings
		switch (lang) {
		case LANGUAGE::English:
			return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);//TODO(fran): same as GetLCID
		case LANGUAGE::Español:
			return MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH);
		default:
			return NULL;
		}
	}
};

namespace userial {
	static str serialize(LANGUAGE_MANAGER& var) {//NOTE: we gotta add this manually cause when need a reference since the lang mgr is a singleton and doesnt allow for instancing
		return var.serialize();
	}
	static bool deserialize(LANGUAGE_MANAGER& var, str name, const str& content) {
		return var.deserialize(name, content);
	}
}
