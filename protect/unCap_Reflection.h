#pragma once
#include "unCap_Platform.h"

//
//Here we establish the standard for implementing reflection in this project
//
//	struct nc{
//	#define foreach_nc_member(op) \
//		op(i32,a,5) \
//		op(HBRUSH,br,CreateSolidBrush(RGB(123,48,68))) \
//
//	foreach_nc_member(_generate_member);
//	}

//TODO(fran): define enum standard

#ifdef UNICODE
#define _t(quote) L##quote
#else
#define _t(quote) quote
#endif

#define _generate_member(type,name,...) type name {__VA_ARGS__};

#define _generate_member_no_default_init(type,name,...) type name;

#define _generate_count(...) +1

#define _isvalid_enum_case(member,value) case member:return true;
#define _string_enum_case(member,value) case member:return _t(#member);

//NOTE: thanks to no one bothering to add {} initialization to enums we require for the user to write the expression "= 5" or whatever, we need the equals to be written so we can also support enum members that dont have a specified value
#define _generate_enum_member(member,value_expr) member value_expr,
