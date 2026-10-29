#pragma once
#include "win_sdk.h"
#include "reflection.h"
#include "serialization.h"

static FILE* console_file_handle;

constexpr auto& serialization_folder = L"\\MyPass";

union KnownColors {
	struct {
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
		op(HBRUSH,ControlTxt_Disabled_Strong,CreateSolidBrush(RGB(188, 188, 182))) \
		op(HBRUSH,Img_Disabled,CreateSolidBrush(RGB(98, 98, 92))) \
		op(HBRUSH,Search_Bk,CreateSolidBrush(RGB(30, 31, 25))) \
		op(HBRUSH,Search_BkPush,CreateSolidBrush(RGB(0, 120, 210))) \
		op(HBRUSH,Search_BkMouseOver,CreateSolidBrush(RGB(0, 130, 225))) \
		op(HBRUSH,Search_Txt,CreateSolidBrush(RGB(238, 238, 232))) \
		op(HBRUSH,Search_Edit_Bk,CreateSolidBrush(RGB(60, 61, 65))) \
		op(HBRUSH,Search_Edit_Txt,CreateSolidBrush(RGB(248, 248, 242))) \
		op(HBRUSH,Search_BkSelected,CreateSolidBrush(RGB(60, 61, 55))) \
		op(HBRUSH,Toast_Success,CreateSolidBrush(RGB(20, 223, 35))) \
		op(HBRUSH,Toast_Failure,CreateSolidBrush(RGB(243, 21, 35))) \
		op(HBRUSH,Btn_Delete_Bk,CreateSolidBrush(RGB(203, 21, 35))) \
		op(HBRUSH,Btn_Delete_BkMouseOver,CreateSolidBrush(RGB(243, 21, 35))) \
		op(HBRUSH,Btn_Delete_BkPush,CreateSolidBrush(RGB(183, 21, 35))) \
		op(HBRUSH,Control_BkPush_Soft,CreateSolidBrush(RGB(58, 93, 156))) \
		op(HBRUSH,Control_BkPush_Strong,CreateSolidBrush(RGB(38, 70, 130))) \
		op(HBRUSH,Card_Bk_Soft,CreateSolidBrush(RGB(209, 246, 255))) \
		op(HBRUSH,Btn_Static_TxtMouseOver,CreateSolidBrush(RGB(29,47,79))) \
		op(HBRUSH,ControlBkColored,CreateSolidBrush(RGB(35, 40, 46))) \

		//op(HBRUSH,ControlBkColored,CreateSolidBrush(RGB(40, 41, 60))) \
		
		//op(HBRUSH,Card_Bk_Soft,CreateSolidBrush(RGB(142, 195, 230))) \
		

		foreach_color(_generate_member_no_default_init);
	};
	HBRUSH brushes[0 + foreach_color(_generate_count)];

	_generate_default_struct_serialize(foreach_color);

	_generate_default_struct_deserialize(foreach_color);
} static colors{};

#ifdef _DEBUG
#define _get_color_name(type, name, value) _t(#name),
constexpr const utf16* known_colors_names[] = { foreach_color(_get_color_name) };
#endif

void default_colors_if_not_set(KnownColors* c) {
#define _default_initialize(type, name,value) if(!c->name) c->name = value;
	foreach_color(_default_initialize);
#undef _default_initialize
}

union KnownFonts {
	struct {
		HFONT General;
		HFONT GeneralBold;
		HFONT Menu;
		HFONT SmallBold;
	};
	HFONT all[4];
	private: void _() { static_assert(sizeof(*this) == sizeof(all)); }
} static fonts{};

#define foreach_bitmap(op) \
	op(HBITMAP, close) \
	op(HBITMAP, minimize) \
	op(HBITMAP, maximize) \
	op(HBITMAP, restore) \
	op(HBITMAP, menu_close) \
	op(HBITMAP, menu_minimize) \
	op(HBITMAP, menu_maximize) \
	op(HBITMAP, menu_restore) \
	op(HBITMAP, solid_arrow_left) \
	op(HBITMAP, solid_arrow_right) \
	op(HBITMAP, solid_arrow_up) \
	op(HBITMAP, solid_arrow_down) \
	op(HBITMAP, line_arrow_right) \
	op(HBITMAP, pointed_line_arrow_right) \
	op(HBITMAP, dropdown) \
	op(HBITMAP, dropdown_up) \
	op(HBITMAP, circle) \
	op(HBITMAP, bin) \
	op(HBITMAP, eye_open) \
	op(HBITMAP, eye_closed) \
	op(HBITMAP, add) \
	op(HBITMAP, edit) \
	op(HBITMAP, clipboard) \
	op(HBITMAP, cut) \
	op(HBITMAP, paste) \
	op(HBITMAP, padlock) \
	op(HBITMAP, cancel) \
	op(HBITMAP, search) \
	op(HBITMAP, menu_search) \
	op(HBITMAP, calendar) \
	op(HBITMAP, language) \
	op(HBITMAP, pin) \
	op(HBITMAP, menu_undo) \
	op(HBITMAP, menu_redo) \
	op(HBITMAP, menu_save) \
	op(HBITMAP, menu_paste) \
	op(HBITMAP, menu_select_all) \
	op(HBITMAP, menu_update)

union KnownBitmaps { //mostly 1bpp 16x16 bitmaps and other small sized bmps
	struct {
		foreach_bitmap(_generate_member_no_default_init)
	};
	HBITMAP all[0 + foreach_bitmap(_generate_count)];

	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
} static bmps{};

/**
  * Design Decision: Shortcut hotkeys are language agnostic. 
  * It is not good to teach the user they should have different 
  * ones for their language as that makes it impossible for them 
  * to reuse their knowledge in another app that doesn't use those
  * same ones, by normalizing to english we increase the chances
  * the shortcuts they know they will be able to use in other apps.
  */
struct KnownShortcuts {
	//Accelerator memory is cleared by the system on close, no need to deallocate
	HACCEL table; 
	HWND listener_wnd;
} static app_shortcuts{};

static void set_app_shortcuts(HWND listener_wnd, u32 accelerator_table_id, HINSTANCE inst = GetModuleHandle(nil)) {
	if (app_shortcuts.table) {
		DestroyAcceleratorTable(app_shortcuts.table);
		app_shortcuts.table = nil;
	}
	app_shortcuts.table = LoadAccelerators(inst, MAKEINTRESOURCE(accelerator_table_id));
	app_shortcuts.listener_wnd = listener_wnd;
}
