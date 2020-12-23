#pragma once
#include "unCap_Platform.h"
#include "unCap_Helpers.h"
#include <string>
#include <Windows.h>
#include <Shlobj.h>//SHGetKnownFolderPath
#include "unCap_Reflection.h"

//--------------------------------------------------------
//Defines serialization and deserialization for every type (preferably ones not expected to change, for complex structs we have "Reflection for serialization")
//--------------------------------------------------------

//struct nc{
//	str serialize() {...}
//
//	bool deserialize(str name, const str& content) {...}
//	}
//
//	namespace userial {
//		str serialize(nc var) {
//			return var.serialize();
//		}
//		bool deserialize(nc& var, str name, const str& content) {
//			return var.deserialize(name, content);
//		}
//	}

//Each serialize-deserialize function pair can decide the string format of the variable, eg SIZE might be encoded "{14,15}"

//If the string does not contain the proper encoding for the variable it is ignored, and nothing is modified, 
//therefore everything is expected to be preinitialized if you want to guarantee valid values

//NOTE: for complex structs inside complex structs define them separately:
//struct smth {
//	struct orth {
//		int a;
//		RECT b;
//	} o;
//	SIZE s;
//};
//Instead do this:
//struct orth {
//	int a;
//	RECT b;
//};
//struct smth {
//	orth o;
//	SIZE s;
//};


//NOTE: I'm using str because of the ease of use around the macros, but actually every separator-like macro MUST be only 1 character long, not all functions were made with more than 1 character in mind, though most are

extern i32 n_tabs;//TODO(fran): use n_tabs for deserialization also, it would help to know how many tabs should be before an identifier to do further filtering //NOTE:in a big project this variable will need to be declared as extern

#define _BeginSerialize() n_tabs=0
#define _BeginDeserialize() n_tabs=0
#define _AddTab() n_tabs++
#define _RemoveTab() n_tabs--
#define _keyvaluesepartor str(TEXT("="))
#define _structbegin str(TEXT("{"))
#define _structend str(TEXT("}"))
#define _memberseparator str(TEXT(","))
#define _newline str(TEXT("\n"))
#define _tab TEXT('\t')
#define _tabs str(n_tabs,_tab)

//We use the Reflection standard
#define _serialize_member(type,name,...) + _tabs + str(TEXT(#name)) + _keyvaluesepartor + userial::serialize(name) + _newline /*TODO:find a way to remove this last +*/
#define _serialize_struct(var) str(TEXT(#var)) + _keyvaluesepartor + (var).serialize() + _newline
#define _deserialize_struct(var,serialized_content) (var).deserialize(str(TEXT(#var)),serialized_content);
#define _deserialize_member(type,name,...) userial::deserialize(name,str(TEXT(#name)),substr); /*NOTE: requires for the variable: str substr; to exist and contain the string for deserialization*/

#define _generate_default_struct_serialize(foreach_op) \
	str serialize() { \
		_AddTab(); \
		str res = _structbegin + _newline foreach_op(_serialize_member); \
		_RemoveTab(); \
		res += _tabs + _structend; \
		return res; \
	} \

#define _generate_default_struct_deserialize(foreach_op) \
	bool deserialize(str name, const str& content) { \
		str start = name + _keyvaluesepartor + _structbegin + _newline; \
		size_t s = find_identifier(content, 0, start); \
		size_t e = find_closing_str(content, s + start.size(), _structbegin, _structend); \
		if (str_found(s) && str_found(e)) { \
			s += start.size(); \
			str substr = content.substr(s, e - s); \
			foreach_op(_deserialize_member); \
			return true; \
		} \
		return false; \
	} \

#define _add_struct_to_serialization_namespace(struct_type) \
	namespace userial {  \
		static str serialize(struct_type var) { \
			return var.serialize(); \
		} \
		static bool deserialize(struct_type& var, str name, const str& content) { \
			return var.deserialize(name, content); \
		} \
	} \

//NOTE: offset corresponds to "s"
//the char behind an identifier should only be one of this: "begin of file", _tab, _newline, _memberseparator, _structbegin
static size_t find_identifier(str s, size_t offset, str compare) {
	for (size_t p; str_found(p = s.find(compare, offset));) {
		if (p == 0) return p;
		if (s[p - 1] == _tab) return p;
		if (s[p - 1] == _newline[0]) return p;
		if (s[p - 1] == _memberseparator[0]) return p;
		if (s[p - 1] == _structbegin[0]) return p;
		offset = p + 1;
	}
	return str::npos;
}

static str load_file_serialized(std::wstring folder = L"\\unCap", std::wstring filename = L"\\serialized.txt") {
	PWSTR general_folder;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &general_folder);//NOTE: I dont think this has an ansi version that isnt deprecated
	std::wstring file = general_folder + folder + filename;
	CoTaskMemFree(general_folder);

	str res;
	HANDLE hFile = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER sz;
		if (GetFileSizeEx(hFile, &sz)) {
			u32 sz32 = (u32)sz.QuadPart;
			TCHAR* buf = (TCHAR*)malloc(sz32);
			if (buf) {
				DWORD bytes_read;
				if (ReadFile(hFile, buf, sz32, &bytes_read, 0) && sz32 == bytes_read) {
					res = buf;//TODO(fran): write straight to str, we are doing two copy unnecessarily
				}
				free(buf);
			}
		}
		CloseHandle(hFile);
	}
	return res;
}

static void save_to_file_serialized(str content, std::wstring folder = L"\\unCap", std::wstring filename = L"\\serialized.txt") {
	PWSTR general_folder;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &general_folder);
	std::wstring dir = general_folder + folder;
	std::wstring path = general_folder + folder + filename;
	CoTaskMemFree(general_folder);

	//SUPERTODO(fran): gotta create the folder first, if the folder isnt there the function fails
	CreateDirectoryW(dir.c_str(), 0);//Create the folder where info will be stored, since windows wont do it

	HANDLE file_handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle != INVALID_HANDLE_VALUE) {
		DWORD bytes_written;
		BOOL write_res = WriteFile(file_handle, (TCHAR*)content.c_str(), (DWORD)content.size() * sizeof(TCHAR), &bytes_written, NULL);
		CloseHandle(file_handle);
	}
}

//Sending everyone into a namespace avoids having to include a multitude of files in the soon to be serialization.h since you can re-open the namespace from other files
namespace userial {

	static str serialize(str var) {
		str res = _t("`") + var + _t("´");
		return res;
	}

	static str serialize(bool var) {
		str res = var ? _t("true") : _t("false");
		return res;
	}

	static str serialize(i32 var) {//Basic types are done by hand since they are the building blocks
		str res = to_str(var);
		return res;
	}
	static str serialize(f32 var) {
		str res = to_str(var);
		return res;
	}

	static str serialize(RECT var) {//Also simple structs that we know wont change
		str res = _structbegin + str(TEXT("left")) + _keyvaluesepartor + serialize(var.left) + _memberseparator + str(TEXT("top")) + _keyvaluesepartor + serialize(var.top) + _memberseparator + str(TEXT("right")) + _keyvaluesepartor + serialize(var.right) + _memberseparator + str(TEXT("bottom")) + _keyvaluesepartor + serialize(var.bottom) + _structend;
		return res;
	}

	static str serialize(HBRUSH var) {//You can store not the pointer but some of its contents
		COLORREF col = ColorFromBrush(var);
		str res = _structbegin + str(TEXT("R")) + _keyvaluesepartor + serialize(GetRValue(col)) + _memberseparator + str(TEXT("G")) + _keyvaluesepartor + serialize(GetGValue(col)) + _memberseparator + str(TEXT("B")) + _keyvaluesepartor + serialize(GetBValue(col)) + _structend;
		return res;
	}

	static bool deserialize(str& var, str name, const str& content) {
		str start = name + _keyvaluesepartor + _t("`");
		size_t s = find_identifier(content, 0, start);
		size_t e = find_closing_str(content, s + start.size(), _t("`"), _t("´"));
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			var = substr;
			return true;
		}
		return false;
	}

	static bool deserialize(bool& var, str name, const str& content) {
		str start = name + _keyvaluesepartor;
		size_t s = find_identifier(content, 0, start);
		if (str_found(s)) {
			s += start.size();
			str substr = content.substr(s, 5/*false is 5 chars*/);
			if (!substr.compare(_t("true"))) { var = true; return true; }
			else if (!substr.compare(_t("false"))) { var = false; return true; }
		}
		return false;
	}

	static bool deserialize(i32& var, str name, const str& content) {//We use references in deserialization to simplify the macros
		str start = name + _keyvaluesepartor;
		str numeric_i = str(TEXT("1234567890.-"));
		size_t s = find_identifier(content, 0, start);
		size_t e = find_till_no_match(content, s + start.size(), numeric_i);
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			try {
				i32 temp = std::stoi(substr);
				var = temp;
				return true;
			}
			catch (...) {}
		}
		return false;
	}

	static bool deserialize(f32& var, str name, const str& content) {
		str start = name + _keyvaluesepartor;
		str numeric_f = str(TEXT("1234567890.-"));
		size_t s = find_identifier(content, 0, start);
		size_t e = find_till_no_match(content, s + start.size(), numeric_f);
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			try {
				f32 temp = std::stof(substr);//TODO: check whether stof is locale dependent
				var = temp;
				return true;
			}
			catch (...) {}
		}
		return false;
	}

	static bool deserialize(HBRUSH& var, str name, const str& content) {
		str start = name + _keyvaluesepartor + _structbegin;
		size_t s = find_identifier(content, 0, start);
		size_t e = find_closing_str(content, s + start.size(), _structbegin, _structend);
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			i32 R, G, B;
			bool r1 = _deserialize_member(0, R, 0);
			bool r2 = _deserialize_member(0, G, 0);
			bool r3 = _deserialize_member(0, B, 0);
			if (r1 && r2 && r3) {
				COLORREF col = RGB(R, G, B);
				var = CreateSolidBrush(col);
				return true;
			}
		}
		return false;
	}

	static bool deserialize(RECT& var, str name, const str& content) {
		str start = name + _keyvaluesepartor + _structbegin;
		size_t s = find_identifier(content, 0, start);
		size_t e = find_closing_str(content, s + start.size(), _structbegin, _structend);
		if (str_found(s) && str_found(e)) {
			s += start.size();
			str substr = content.substr(s, e - s);
			i32 left, top, right, bottom;
			bool r1 = _deserialize_member(0, left, 0);
			bool r2 = _deserialize_member(0, top, 0);
			bool r3 = _deserialize_member(0, right, 0);
			bool r4 = _deserialize_member(0, bottom, 0);
			if (r1 && r2 && r3 && r4) {
				var = { left,top,right,bottom };
				return true;
			}
		}
		return false;
	}

}