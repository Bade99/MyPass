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
