#pragma once
//TODO: maybe this variables could go inside global::styles
// 
//constexpr DWORD style_button_txt = WS_CHILD | WS_TABSTOP | button::style::roundrect;
//constexpr DWORD style_button_bmp = WS_CHILD | WS_TABSTOP | button::style::roundrect | BS_BITMAP;
//constexpr DWORD style_button_icon = WS_CHILD | WS_TABSTOP | button::style::roundrect | BS_ICON;

struct Themes {
	button::Theme base_btn;
	button::Theme login_btn;
	button::Theme editor_add_btn;
	button::Theme password_editor_card_btn;
	button::Theme password_editor_toolbar_btn;
	button::Theme password_editor_toolbar_btn_danger;
	button::Theme password_editor_toolbar_btn_static;
	button::Theme table_toolbar_btn;
	button::Theme table_toolbar_btn_danger;
	//static button::Theme img_btn_theme;
	//static button::Theme accent_btn_theme;
	//static static_oneline::Theme base_static_theme;
	//static static_oneline::Theme kanji_static_theme;
	//static navbar::Theme nav_theme;
	//static navbar::Theme sidebar_theme;
	//static button::Theme navbar_btn_theme;
	//static button::Theme navbar_img_btn_theme;
	//static button::Theme dark_btn_theme;
	//static button::Theme dark_nonclickable_btn_theme;
	edit_oneline::Theme base_editoneline;
	edit_oneline::Theme clear_editoneline;
	edit_oneline::Theme login_editoneline;
	//static edit_oneline::Theme hiragana_editoneline_theme;
	//static edit_oneline::Theme kanji_editoneline_theme;
	//static edit_oneline::Theme meaning_editoneline_theme;
	//static page::Theme base_page_theme;
	table::Theme base_table;
	search::Theme base_search;
	toast::Theme base_toast;
} static themes{};

void setup_bmps(HINSTANCE instance = GetModuleHandle(nil)) {
	auto load_bitmap8 = [](HINSTANCE instance, u32 resource_id) {
		return (HBITMAP)LoadImage(instance, MAKEINTRESOURCE(resource_id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	};

	bmps.add = LoadBitmap(instance, MAKEINTRESOURCE(BMP_ADD));
	bmps.edit = LoadBitmap(instance, MAKEINTRESOURCE(BMP_EDIT));
	bmps.clipboard = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CLIPBOARD));
	bmps.padlock = LoadBitmap(instance, MAKEINTRESOURCE(BMP_PADLOCK));
	bmps.arrow_right = LoadBitmap(instance, MAKEINTRESOURCE(BMP_RIGHTARROW));
	bmps.close = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CLOSE));
	bmps.maximize = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MAX));
	bmps.minimize = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MIN));
	bmps.cancel = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CANCEL));
	bmps.eye_open = LoadBitmap(instance, MAKEINTRESOURCE(BMP_EYE_OPEN));
	bmps.eye_closed = LoadBitmap(instance, MAKEINTRESOURCE(BMP_EYE_CLOSED));
	bmps.search = LoadBitmap(instance, MAKEINTRESOURCE(BMP_SEARCH));
	bmps.calendar = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CALENDAR));
	bmps.circle = load_bitmap8(instance, BMP_CIRCLE_A);
	bmps.language = load_bitmap8(instance, BMP_LANGUAGE);


#define create_global_bmps(bmp) bmps.bmp = CreateBitmap(bmp.w, bmp.h,1,bmp.bpp,bmp.mem); Assert(bmps.bmp);

	create_global_bmps(bin);
	create_global_bmps(dropdown);

	atexit([]() { for (auto& bmp : bmps.all) if (bmp) { DeleteObject(bmp); bmp = nil; } });
}


void load_styles() {
	setup_bmps();

	auto hollow_brush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	auto arrow_cursor = LoadCursor(nullptr, (TCHAR*)IDC_ARROW);
	auto hand_cursor = LoadCursor(nullptr, (TCHAR*)IDC_HAND);

	themes.base_table = [&]()->auto {
		table::Theme base_table{};
		base_table.brushes.bk.normal = colors.ControlBk;
		base_table.brushes.border.normal = colors.ControlTxt;
		base_table.dimensions.border_thickness = 1;
		return base_table;
	}();
	
	themes.base_btn = [&]()->auto {
		button::Theme base_btn{};
		base_btn.dimensions.border_thickness = 1;
		base_btn.brushes.bk.normal = colors.ControlBk;
		base_btn.brushes.bk.disabled = colors.ControlBk_Disabled;
		base_btn.brushes.bk.clicked = colors.ControlBkPush;
		base_btn.brushes.bk.mouseover = colors.ControlBkMouseOver;
		base_btn.brushes.foreground.normal = colors.ControlTxt;
		base_btn.brushes.foreground.mouseover = colors.ControlTxt;
		base_btn.brushes.foreground.clicked = colors.ControlTxt;
		base_btn.brushes.foreground.disabled = colors.ControlTxt_Disabled;
		base_btn.brushes.border.normal = colors.Img;//TODO(fran): global::colors.ControlBorder
		base_btn.cursor = hand_cursor;
		base_btn.dimensions.border_radius = { .type = UINumber::type::dpi, .value = 10 };
		//TODO(fran): use the extra brushes, fore_push,... , border_mouseover,...
		base_btn.font = fonts.General;
		return base_btn;
	}();

	themes.base_editoneline = [&]()->auto {
		edit_oneline::Theme base_editoneline{};
		base_editoneline.dimensions.border_thickness = 1;
		base_editoneline.brushes.foreground.normal = colors.ControlTxt;
		base_editoneline.brushes.foreground.disabled = colors.ControlTxt_Disabled;
		base_editoneline.brushes.bk.normal = colors.ControlBk;
		base_editoneline.brushes.bk.disabled = colors.ControlBk_Disabled;
		base_editoneline.brushes.border.normal = colors.Img;
		base_editoneline.brushes.border.disabled = colors.Img_Disabled;
		base_editoneline.brushes.placeholder.normal = colors.ControlTxt_Disabled;
		//base_editoneline.brushes.selection.normal = colors.Selection;
		//base_editoneline.brushes.selection.disabled = colors.Selection_Disabled;
		return base_editoneline;
	}();

	themes.clear_editoneline = [&]()->auto {
		auto clear_editoneline = themes.base_editoneline;
		clear_editoneline.dimensions.border_thickness = 0;
		clear_editoneline.brushes.foreground.disabled = clear_editoneline.brushes.foreground.normal;
		clear_editoneline.brushes.placeholder.normal = colors.ControlTxt_Disabled_Strong;
		for (auto& b : clear_editoneline.brushes.bk.all) b = hollow_brush;
		for (auto& b : clear_editoneline.brushes.border.all) b = hollow_brush;
		return clear_editoneline;
	}();

	themes.login_editoneline = [&]()->auto {
		auto login_editoneline = themes.base_editoneline;
		login_editoneline.dimensions.border_thickness = 0;
		login_editoneline.dimensions.border_radius = themes.base_btn.dimensions.border_radius;
		return login_editoneline;
	}();

	themes.login_btn = [&]()->auto {
		auto login_btn = themes.base_btn;
		login_btn.brushes.bk.normal = colors.ControlBkPush;
		login_btn.dimensions.border_thickness = 0;
		login_btn.dimensions.border_radius = {.type = UINumber::type::percent, .value = 50};
		return login_btn;
	}();

	themes.editor_add_btn = [&]()->auto {
		auto t = themes.login_btn;
		return t;
	}();

	themes.password_editor_card_btn = [&]()->auto {
		auto t = themes.base_btn;
		t.dimensions.border_thickness = 0;
		t.brushes.bk.normal = colors.Search_Bk;
		t.brushes.bk.clicked = colors.Control_BkPush_Soft;
		return t;
	}();

	themes.password_editor_toolbar_btn = [&]()->auto {
		auto password_editor_toolbar_btn = themes.login_btn;
		return password_editor_toolbar_btn;
	}();

	themes.password_editor_toolbar_btn_danger = [&]()->auto {
		auto t = themes.password_editor_toolbar_btn;
		t.brushes.bk.normal = colors.Btn_Delete_Bk;
		t.brushes.bk.mouseover = colors.Btn_Delete_BkMouseOver;
		t.brushes.bk.clicked = colors.Btn_Delete_BkPush;
		t.brushes.bk.disabled = hollow_brush;
		return t;
	}();

	themes.password_editor_toolbar_btn_static = [&]()->auto {
		auto t = themes.password_editor_toolbar_btn_static;
		for (auto& b : t.brushes.bk.all) b = hollow_brush;
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		t.brushes.foreground = {
			.normal = themes.password_editor_toolbar_btn.brushes.foreground.normal,
			.disabled = themes.password_editor_toolbar_btn.brushes.foreground.disabled,
			.mouseover = colors.Btn_Static_TxtMouseOver,
			.clicked = colors.Btn_Static_TxtMouseOver,
		};
		t.cursor = arrow_cursor;
		return t;
	}();

	themes.table_toolbar_btn = [&]()->auto {
		auto table_toolbar_btn = themes.login_btn;
		table_toolbar_btn.brushes.bk.normal = hollow_brush;
		table_toolbar_btn.brushes.bk.disabled = hollow_brush;
		table_toolbar_btn.dimensions.border_thickness = 0;
		table_toolbar_btn.dimensions.border_radius = { .type = UINumber::type::percent, .value = 50 };
		return table_toolbar_btn;
	}();

	themes.table_toolbar_btn_danger = [&]()->auto {
		auto t = themes.login_btn;
		t.brushes.bk.normal = hollow_brush;
		t.brushes.bk.disabled = hollow_brush;
		t.brushes.bk.mouseover = colors.Btn_Delete_BkMouseOver;
		t.brushes.bk.clicked = colors.Btn_Delete_BkPush;
		t.dimensions.border_thickness = 0;
		t.dimensions.border_radius = { .type = UINumber::type::percent, .value = 50 };
		return t;
	}();

	themes.base_search = [&]()->auto {
		search::Theme base_search{};

		base_search.brushes.bk.normal = colors.Search_Bk;
		base_search.brushes.border = base_search.brushes.bk;

		base_search.dimensions.border_radius = themes.base_btn.dimensions.border_radius;

		base_search.btn_theme = themes.base_btn;
		base_search.btn_theme.brushes.bk = {
			.normal = colors.Search_Bk, 
			.mouseover = colors.Search_BkMouseOver,
			.clicked = colors.Search_BkPush, //I think that it is clearer like this than using 'Search_BkSelected' 
		};
		base_search.btn_theme.brushes.border = base_search.brushes.border;
		base_search.btn_theme.brushes.foreground.normal = colors.Search_Txt;

		base_search.txt_theme = themes.base_editoneline;
		base_search.txt_theme.brushes.bk.normal = colors.Search_Edit_Bk;
		base_search.txt_theme.brushes.foreground.normal = colors.Search_Edit_Txt;
		base_search.txt_theme.brushes.border = base_search.txt_theme.brushes.bk;
		base_search.txt_theme.dimensions.border_radius = base_search.dimensions.border_radius;

		base_search.styles.persistent = true;
		base_search.styles.btn_case_sensitive_off = true;
		base_search.styles.btn_whole_word_off = true;
		base_search.styles.btn_wrap_off = true;
		base_search.styles.btn_find_prev_off = true;

		return base_search;
	}();

	themes.base_toast = [&]()->auto {
		toast::Theme base_toast{};

		base_toast.success.dimensions.border_thickness = 0;
		base_toast.success.dimensions.border_radius = themes.base_btn.dimensions.border_radius;
		for (auto& b : base_toast.success.brushes.foreground.all) b = colors.ControlTxt;
		for (auto& b : base_toast.success.brushes.bk.all) b = colors.Toast_Success;
		base_toast.success.font = fonts.GeneralBold;

		base_toast.failure.dimensions.border_thickness = 0;
		base_toast.failure.dimensions.border_radius = themes.base_btn.dimensions.border_radius;
		for (auto& b : base_toast.failure.brushes.foreground.all) b = colors.ControlTxt;
		for (auto& b : base_toast.failure.brushes.bk.all) b = colors.Toast_Failure;
		base_toast.failure.font = fonts.GeneralBold;

		return base_toast;
	}();

	/*

	img_btn_theme = [&]()->auto {
		auto img_btn_theme = base_btn;
		img_btn_theme.brushes.foreground.normal = global::colors.Img;
		return img_btn_theme;
		}();

	accent_btn_theme = [&]()->auto {
		auto accent_btn_theme = base_btn;
		accent_btn_theme.brushes.foreground.normal = global::colors.Accent;
		accent_btn_theme.brushes.border.normal = global::colors.Accent;
		return accent_btn_theme;
		}();

	base_static_theme = [&]()->auto {
		static_oneline::Theme base_static_theme;
		base_static_theme.brushes.foreground.normal = global::colors.ControlTxt;
		base_static_theme.brushes.foreground.disabled = global::colors.ControlTxt_Disabled;
		base_static_theme.brushes.bk.normal = global::colors.ControlBk;
		base_static_theme.brushes.bk.disabled = global::colors.ControlBk_Disabled;
		return base_static_theme;
		}();

	kanji_static_theme = [&]()->auto {
		auto kanji_static_theme = base_static_theme;
		kanji_static_theme.brushes.foreground.normal = brush_for(learnt_word_elem::kanji);
		return kanji_static_theme;
		}();

	nav_theme = [&]()->auto {
		navbar::Theme nav_theme;
		nav_theme.brushes.bk.normal = global::colors.ControlBk_Disabled;//TODO(fran): darker color than bk
		nav_theme.dimensions.spacing = 3;
		nav_theme.dimensions.is_vertical = false;
		return nav_theme;
		}();

	sidebar_theme = [&]()->auto {
		auto sidebar_theme = nav_theme;
		sidebar_theme.dimensions.spacing = 0;
		sidebar_theme.dimensions.is_vertical = true;
		return sidebar_theme;
		}();

	navbar_btn_theme = [&]()->auto {
		auto navbar_btn_theme = base_btn;
		navbar_btn_theme.brushes.bk.normal = nav_theme.brushes.bk.normal;
		navbar_btn_theme.brushes.border = navbar_btn_theme.brushes.bk;
		return navbar_btn_theme;
		}();

	navbar_img_btn_theme = [&]()->auto {
		auto navbar_img_btn_theme = img_btn_theme;
		navbar_img_btn_theme.brushes.bk.normal = nav_theme.brushes.bk.normal;
		navbar_img_btn_theme.brushes.border = navbar_img_btn_theme.brushes.bk;
		return navbar_img_btn_theme;
		}();

	dark_btn_theme = [&]()->auto {
		auto dark_btn_theme = base_btn;
		dark_btn_theme.brushes.bk.normal = global::colors.ControlBk_Dark;
		return dark_btn_theme;
		}();

	dark_nonclickable_btn_theme = [&]()->auto {
		auto dark_nonclickable_btn_theme = dark_btn_theme;
		for (auto& b : dark_nonclickable_btn_theme.brushes.bk.all) b = dark_nonclickable_btn_theme.brushes.bk.normal;
		for (auto& b : dark_nonclickable_btn_theme.brushes.border.all) b = dark_nonclickable_btn_theme.brushes.border.normal;
		return dark_nonclickable_btn_theme;
		}();

	base_page_theme = [&]()->auto {
		page::Theme base_page_theme;
		base_page_theme.brushes.bk.normal = global::colors.ControlBk;
		base_page_theme.brushes.border = base_page_theme.brushes.bk;
		base_page_theme.dimensions.border_thickness = 0;
		return base_page_theme;
		}();
	*/
}