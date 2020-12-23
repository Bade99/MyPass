#include "LANGUAGE_MANAGER.h"


bool LANGUAGE_MANAGER::deserialize(str name, const str& content) {
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
	this->ChangeLanguage(this->CurrentLanguage);//whatever happens we need to set some language
	return res;
}


HINSTANCE LANGUAGE_MANAGER::SetHInstance(HINSTANCE hInst)
{
	HINSTANCE oldHInst = this->hInstance;
	this->hInstance = hInst;
	return oldHInst;
}

BOOL LANGUAGE_MANAGER::AddDynamicText(HWND hwnd, UINT messageID)
{
	if (!hwnd) return FALSE;
	this->DynamicHwnds[hwnd] = messageID;
	this->UpdateDynamicHwnd(hwnd, messageID);
	return TRUE;
}

BOOL LANGUAGE_MANAGER::AddMenuDrawingHwnd(HWND MenuDrawer)
{
	//TODO(fran): check HWND is valid
	if (MenuDrawer) {
		this->MenuDrawers.push_back(MenuDrawer);
		return TRUE;
	}
	return FALSE;
}

BOOL LANGUAGE_MANAGER::AddMenuText(HMENU hmenu, UINT_PTR ID, UINT stringID)
{
	BOOL res = UpdateMenu(hmenu, ID, stringID);
	if (res) this->Menus[std::make_pair(hmenu, ID)] = stringID;
	return res;
}

LANGUAGE LANGUAGE_MANAGER::GetCurrentLanguage()
{
	return this->CurrentLanguage;
}

BOOL LANGUAGE_MANAGER::AddWindowText(HWND hwnd, UINT stringID)
{
	BOOL res = UpdateHwnd(hwnd, stringID);
	if (res) this->Hwnds[hwnd] = stringID;
	return res;
}

BOOL LANGUAGE_MANAGER::AddComboboxText(HWND hwnd, UINT ID, UINT stringID)
{
	BOOL res = UpdateCombo(hwnd, ID, stringID);
	if (res) this->Comboboxes[std::make_pair(hwnd, ID)] = stringID;
	return res;
}

LANGID LANGUAGE_MANAGER::ChangeLanguage(LANGUAGE newLang)
{
	//if (newLang == this->CurrentLanguage) return -1;//TODO: negative values wrap around to huge values, I assume not all lcid values are valid, find out if these arent
	BOOL res = this->IsValidLanguage(newLang);
	if (!res) return (LANGID)-2;

	this->CurrentLanguage = newLang;
	//LCID previousLang = SetThreadLocale(this->GetLCID(newLang));
	//INFO: thanks https://www.curlybrace.com/words/2008/06/10/setthreadlocale-and-setthreaduilanguage-for-localization-on-windows-xp-and-vista/
	// SetThreadLocale has no effect on Vista and above
	LANGID newLANGID = this->GetLANGID(newLang);
	//INFO: On non-Vista platforms, SetThreadLocale can be used. Instead of a language identifier, it accepts a locale identifier
	LANGID lang_res = SetThreadUILanguage(newLANGID); //If successful returns the same value that you sent it

	for (auto const& hwnd_sid : this->Hwnds)
		this->UpdateHwnd(hwnd_sid.first, hwnd_sid.second);

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

std::wstring LANGUAGE_MANAGER::RequestString(UINT stringID)
{
	WCHAR text[500];
	LoadStringW(this->hInstance, stringID, text, 500);//INFO: last param is size of buffer in characters
	return text;
}

//BOOL LANGUAGE_MANAGER::UpdateHwnd(HWND hwnd, UINT stringID)
//{
//	BOOL res = SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)this->RequestString(stringID).c_str()) == TRUE;
//	InvalidateRect(hwnd, NULL, TRUE);
//	return res;
//}
//
//BOOL LANGUAGE_MANAGER::UpdateDynamicHwnd(HWND hwnd, UINT messageID)
//{
//	return PostMessage(hwnd, messageID, 0, 0);
//}
//
//BOOL LANGUAGE_MANAGER::UpdateCombo(HWND hwnd, UINT ID, UINT stringID)
//{
//	UINT currentSelection = (UINT)SendMessage(hwnd, CB_GETCURSEL, 0, 0);//TODO(fran): is there truly no better solution than having to do all this just to change a string?
//	SendMessage(hwnd, CB_DELETESTRING, ID, 0);
//	LRESULT res = SendMessage(hwnd, CB_INSERTSTRING, ID, (LPARAM)this->RequestString(stringID).c_str());
//	if (currentSelection != CB_ERR) {
//		SendMessage(hwnd, CB_SETCURSEL, currentSelection, 0);
//		InvalidateRect(hwnd, NULL, TRUE);
//	}
//	return res != CB_ERR && res != CB_ERRSPACE;//TODO(fran): can I check for >=0 with lresult?
//}
//
//BOOL LANGUAGE_MANAGER::UpdateMenu(HMENU hmenu, UINT_PTR ID, UINT stringID)
//{
//	MENUITEMINFOW menu_setter;
//	menu_setter.cbSize = sizeof(menu_setter);
//	menu_setter.fMask = MIIM_STRING;
//	std::wstring temp_text = this->RequestString(stringID);
//	//menu_setter.dwTypeData = _wcsdup(this->RequestString(stringID).c_str()); //TODO(fran): can we avoid dupping, if not free memory
//	menu_setter.dwTypeData = (LPWSTR)temp_text.c_str();
//	BOOL res = SetMenuItemInfoW(hmenu, (UINT)ID, FALSE, &menu_setter);
//	return res;
//}

//LCID LANGUAGE_MANAGER::GetLCID(LANGUAGE lang)
//{
//	switch (lang) {
//	case LANGUAGE::English: 
//		return MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT); //TODO(fran):this is deprecated and not great for macros, unless we set each enum to this values
//	case LANGUAGE::Español:
//		return MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH), SORT_DEFAULT);
//	default:
//		return NULL;
//	}
//}

//LANGID LANGUAGE_MANAGER::GetLANGID(LANGUAGE lang)
//{
//	//INFO: https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings
//	switch (lang) {
//	case LANGUAGE::English:
//		return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);//TODO(fran): same as GetLCID
//	case LANGUAGE::Español:
//		return MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH);
//	default:
//		return NULL;
//	}
//}

