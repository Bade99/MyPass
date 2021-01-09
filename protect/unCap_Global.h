#pragma once
#include <Windows.h>
#include "unCap_Reflection.h"
#include "unCap_Serialization.h"

union UNCAP_COLORS {//TODO(fran): HBRUSH Border
	struct {
		//TODO(fran): macro magic to auto generate the appropriately sized HBRUSH array
		//TODO(fran): add _disabled for at least txt,bk,border
#define foreach_color(op) \
		op(HBRUSH,ControlBk,CreateSolidBrush(RGB(40, 41, 35))) \
		op(HBRUSH,ControlBkPush,CreateSolidBrush(RGB(0, 110, 200))) \
		op(HBRUSH,ControlBkMouseOver,CreateSolidBrush(RGB(0, 120, 215))) \
		op(HBRUSH,ControlTxt,CreateSolidBrush(RGB(248, 248, 242))) \
		op(HBRUSH,ControlTxt_Inactive,CreateSolidBrush(RGB(208, 208, 202))) \
		op(HBRUSH,ControlMsg,CreateSolidBrush(RGB(248, 230, 0))) \
		op(HBRUSH,Scrollbar,CreateSolidBrush(RGB(148, 148, 142))) \
		op(HBRUSH,ScrollbarMouseOver,CreateSolidBrush(RGB(188, 188, 182))) \
		op(HBRUSH,ScrollbarBk,CreateSolidBrush(RGB(50, 51, 45))) \
		op(HBRUSH,Img,CreateSolidBrush(RGB(228, 228, 222))) \
		op(HBRUSH,Img_Inactive,CreateSolidBrush(RGB(198, 198, 192))) \
		op(HBRUSH,CaptionBk,CreateSolidBrush(RGB(20, 21, 15))) \
		op(HBRUSH,CaptionBk_Inactive,CreateSolidBrush(RGB(60, 61, 65))) \
		op(HBRUSH,ControlBk_Disabled,CreateSolidBrush(RGB(35, 36, 30))) \
		op(HBRUSH,ControlTxt_Disabled,CreateSolidBrush(RGB(128, 128, 122))) \
		op(HBRUSH,Img_Disabled,CreateSolidBrush(RGB(98, 98, 92))) \
		op(HBRUSH,Search_Bk,CreateSolidBrush(RGB(30, 31, 25))) \
		op(HBRUSH,Search_BkPush,CreateSolidBrush(RGB(0, 120, 210))) \
		op(HBRUSH,Search_BkMouseOver,CreateSolidBrush(RGB(0, 130, 225))) \
		op(HBRUSH,Search_Txt,CreateSolidBrush(RGB(238, 238, 232))) \
		op(HBRUSH,Search_Edit_Bk,CreateSolidBrush(RGB(60, 61, 65))) \
		op(HBRUSH,Search_Edit_Txt,CreateSolidBrush(RGB(248, 248, 242))) \
		op(HBRUSH,Search_BkSelected,CreateSolidBrush(RGB(60, 61, 55))) \

		foreach_color(_generate_member_no_default_init);

	};
	HBRUSH brushes[20];//REMEMBER to update

	_generate_default_struct_serialize(foreach_color);

	_generate_default_struct_deserialize(foreach_color);
};

void default_colors_if_not_set(UNCAP_COLORS* c) {
#define _default_initialize(type, name,value) if(!c->name) c->name = value;
	foreach_color(_default_initialize);
#undef _default_initialize
}

extern UNCAP_COLORS unCap_colors;

union UNCAP_FONTS {
	struct {
		HFONT General;
		HFONT Menu;
	};
	HFONT fonts[2];//REMEMBER to update
};

extern UNCAP_FONTS unCap_fonts;