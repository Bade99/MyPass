#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint> //uint8_t
#include <format>
#include <type_traits>
#include <queue>
#include <deque>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <span>
#include <utility>
#include <vector>
#include "reflection.h"
#include "version.h"

#define _APP_NAME _t(SOLUTION_PRODUCTNAME)
#define _APP_VERSION _t(SOLUTION_PRODUCTVERSION_STRING)
constexpr auto& app_name = _APP_NAME;
constexpr auto& app_name_with_version = _APP_NAME _t(" ") _APP_VERSION;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

constexpr u8  U8MAX  = (u8)0xff;
constexpr u16 U16MAX = (u16)0xffff;
constexpr u32 U32MAX = (u32)0xffffffff;
constexpr u64 U64MAX = (u64)0xffffffffffffffff;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

constexpr i8  I8MAX  = std::numeric_limits<i8>::max();
constexpr i8  I8MIN  = std::numeric_limits<i8>::min();
constexpr i16 I16MAX = std::numeric_limits<i16>::max();
constexpr i16 I16MIN = std::numeric_limits<i16>::min();
constexpr i32 I32MAX = std::numeric_limits<i32>::max();
constexpr i32 I32MIN = std::numeric_limits<i32>::min();
constexpr i64 I64MAX = std::numeric_limits<i64>::max();
constexpr i64 I64MIN = std::numeric_limits<i64>::min();

typedef i64 time64;

typedef float  f32;
typedef double f64;

constexpr f32 F32INFINITY = std::numeric_limits<f32>::infinity();
constexpr f32 F64INFINITY = std::numeric_limits<f64>::infinity();

typedef char     utf8;
typedef wchar_t  utf16; //TODO(fran): wchar_t is 4bytes in unix systems, switch to char16_t
typedef char32_t utf32;

#ifndef UNICODE
#error "Must use Unicode (UTF16 in Windows)"
#endif

static_assert(sizeof(long long) == 8, "We expect external functions that use ll to be 64bits");


typedef std::wstring str;
typedef std::wstring_view str_view;
typedef wchar_t cstr;

static_assert(sizeof(wchar_t) == 2, "We expect wchar_t to be 16bits");

struct text { //A NON null terminated cstring
	cstr* str;
	size_t sz_chars; //TODO(fran): better is probably size in bytes
};

#if !defined(__x86_64__) && !defined(_M_X64)
#error "Unsupported architecture. Please validate functionality, then add it here."
// Areas of concern to test in the new arch:
// - raw_buffer encoding methodology: 
//  + we try to standardise it with padding for 2 and 4 byte alignment for some types, but allocators for data that is passed to the raw_buffer_reader should also provide aligned memory
//  + we dont handle different endianness, we assume little-endian, which should be fine for most use cases
#endif

//TODO(fran): this probably can be replaced by std string view
template<typename T>
struct _str {
	using type = T;
	using value_type = T;
	type* str;
	size_t sz;/*bytes*/

	operator type* () const { return str; }

	//Includes null terminator
	size_t sz_char() const { return sz / sizeof(*str); }//TODO(fran): it's probably better to not include the null terminator, or give two functions

	//Does _not_ include null terminator
	size_t cnt() const { return (sz ? sz - 1 * sizeof(*str) : sz) / sizeof(*str); }

	//Use for quick debugging and checking for null terminator
	type last_char() const { return str[cnt()]; }

	//Includes null terminator
	size_t cntn() const { return sz / sizeof(*str); }

	type& operator[](size_t i) const { return str[i]; }

	type* begin() const { return this->sz ? &(*this)[0] : nullptr; }

	//TODO(fran): does this include the null terminator? if so we gotta fix it, null terminator shouldnt appear
	type* end() const { return this->sz ? (type*)((u8*)&(*this)[0] + this->sz) : nullptr; /*ptr one past the last element*/ }
};

typedef _str<utf16> utf16_str;

static utf16_str to_utf_str(utf16* s) { return { s, (wcslen(s) + 1) * sizeof(*s) }; }

static utf16_str to_utf_str(std::wstring& s) { return { const_cast<decltype(&s[0])>(s.c_str()),(s.length() + 1) * sizeof(s[0]) }; }


template <typename T>
using multiflag = u32;

template <typename T>
requires std::same_as<T, u32>
void set_flag_bit(T& flags, bool set, T flag) {
	static_assert(std::is_unsigned_v<T>, "Flag value type needs to be unsigned for the bit trick we do for branchless assignment");
	using signedt = std::make_signed_t<T>;
	flags ^= flag & ((-(signedt)set) ^ flags);
}