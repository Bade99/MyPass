#pragma once
//TODO: maybe this variables could go inside global::styles
// 
//constexpr DWORD style_button_txt = WS_CHILD | WS_TABSTOP | button::style::roundrect;
//constexpr DWORD style_button_bmp = WS_CHILD | WS_TABSTOP | button::style::roundrect | BS_BITMAP;
//constexpr DWORD style_button_icon = WS_CHILD | WS_TABSTOP | button::style::roundrect | BS_ICON;

str get_app_fontname() {
	//Font guidelines: https://docs.microsoft.com/en-us/windows/win32/uxguide/vis-fonts
	//Stock fonts: https://docs.microsoft.com/en-us/windows/win32/gdi/using-a-stock-font-to-draw-text

	//TODO(fran): can we take the best codepage from each font and create our own? (look at font linking & font fallback)
	//TODO(fran): speed up: use a custom allocator that starts by performing a single wchar_t[2500][22] allocation instead of using individually allocated strings

	//We looked at 2195 fonts, this is what's left
	//Options:
	//Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
	//-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
	//-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
	//-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

	//i64 cnt = StartCounter(); defer{ printf("ELAPSED FIND FONT: %f ms\n",EndCounter(cnt)); };

	HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); };
	struct FontNameLess {
		bool operator()(const str& left, const str& right) const { return _wcsicmp(left.c_str(), right.c_str()) < 0; }
	};
	using FontNames = std::set<str, FontNameLess>;
	FontNames fontnames;
	EnumFontFamiliesEx(dc, nil
		, [](const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam) -> int {
			((FontNames*)lParam)->insert(lpelfe->lfFaceName);
			return TRUE;
		}
	, (LPARAM)&fontnames, 0);

	auto requested_fontname = { _t("Segoe UI"), _t("Arial Unicode MS"), _t("Microsoft YaHei"), _t("Microsoft YaHei UI") };

	for (const auto requested_font : requested_fontname)
		if (auto font = fontnames.find(requested_font); font != fontnames.end())
			return *font;

	return _t("");
}

struct Themes {
	button::Theme base_btn;
	button::Theme login_btn;
	button::Theme login_btn_toggle;
	button::Theme editor_add_btn;
	button::Theme editor_btn_static_transition;
	button::Theme editor_btn_static_bk_transition;
	edit_oneline::Theme editor_static_transition;
	button::Theme password_editor_card_btn;
	button::Theme password_editor_toolbar_btn;
	button::Theme password_editor_toolbar_btn_danger;
	button::Theme password_editor_toolbar_btn_static;
	button::Theme table_toolbar_btn;
	button::Theme table_toolbar_btn_danger;
	button::Theme transition_btn;
	edit_oneline::Theme base_editoneline;
	edit_oneline::Theme clear_editoneline;
	edit_oneline::Theme login_editoneline;
	edit_oneline::Theme login_text_static_error;
	table::Theme base_table;
	search::Theme base_search;
	toast::Theme base_toast;
	scrollbar::Theme base_scrollbar;
} static themes{};

void setup_bmps(HINSTANCE instance = GetModuleHandle(nil)) {
	auto load_bitmap8 = [](HINSTANCE instance, u32 resource_id) {
		return (HBITMAP)LoadImage(instance, MAKEINTRESOURCE(resource_id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	};

	bmps.add = LoadBitmap(instance, MAKEINTRESOURCE(BMP_ADD));
	bmps.edit = LoadBitmap(instance, MAKEINTRESOURCE(BMP_EDIT));
	bmps.clipboard = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CLIPBOARD));
	bmps.padlock = LoadBitmap(instance, MAKEINTRESOURCE(BMP_PADLOCK));
	bmps.solid_arrow_right = LoadBitmap(instance, MAKEINTRESOURCE(BMP_SOLID_ARROW_RIGHT));
	bmps.solid_arrow_left = rotate_bitmap1(bmps.solid_arrow_right, Rotation::clockwise_180);
	bmps.solid_arrow_up = rotate_bitmap1(bmps.solid_arrow_right, Rotation::clockwise_270);
	bmps.solid_arrow_down = rotate_bitmap1(bmps.solid_arrow_right, Rotation::clockwise_90);
	bmps.close = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CLOSE));
	bmps.minimize = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MIN));
	bmps.maximize = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MAX));
	bmps.restore = LoadBitmap(instance, MAKEINTRESOURCE(BMP_RESTORE));
	bmps.menu_close = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MENU_CLOSE));
	bmps.menu_minimize = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MENU_MINIMIZE));
	bmps.menu_maximize = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MENU_MAXIMIZE));
	bmps.menu_restore = LoadBitmap(instance, MAKEINTRESOURCE(BMP_MENU_RESTORE));
	bmps.calendar = LoadBitmap(instance, MAKEINTRESOURCE(BMP_CALENDAR));
	bmps.dropdown = LoadBitmap(instance, MAKEINTRESOURCE(BMP_DROPDOWN));
	bmps.dropdown_up = LoadBitmap(instance, MAKEINTRESOURCE(BMP_DROPDOWN_UP));
	bmps.bin = LoadBitmap(instance, MAKEINTRESOURCE(BMP_BIN));
	bmps.line_arrow_right = LoadBitmap(instance, MAKEINTRESOURCE(BMP_LINE_ARROW_RIGHT));
	bmps.pointed_line_arrow_right = LoadBitmap(instance, MAKEINTRESOURCE(BMP_POINTED_LINE_ARROW_RIGHT));

	bmps.circle = load_bitmap8(instance, BMP_CIRCLE);
	bmps.language = load_bitmap8(instance, BMP_LANGUAGE);
	bmps.cancel = load_bitmap8(instance, BMP_CANCEL);
	bmps.eye_open = load_bitmap8(instance, BMP_EYE_OPEN);
	bmps.eye_closed = load_bitmap8(instance, BMP_EYE_CLOSED);
	bmps.search = load_bitmap8(instance, BMP_SEARCH);
	bmps.menu_search = load_bitmap8(instance, BMP_SEARCH);
	bmps.menu_undo = load_bitmap8(instance, BMP_UNDO);
	bmps.menu_redo = flip_bitmap(bmps.menu_undo, true, false);
	bmps.menu_save = load_bitmap8(instance, BMP_SAVE);
	bmps.menu_paste = load_bitmap8(instance, BMP_MENU_PASTE);
	bmps.menu_select_all = load_bitmap8(instance, BMP_MENU_SELECT_ALL);
	bmps.menu_update = load_bitmap8(instance, BMP_UPDATE);
	bmps.pin = load_bitmap8(instance, BMP_PIN);
	bmps.cut = load_bitmap8(instance, BMP_CUT);
	bmps.paste = load_bitmap8(instance, BMP_PASTE);

	atexit([]() { for (auto& bmp : bmps.all) if (bmp) { DeleteObject(bmp); bmp = nil; } });
}


void load_styles() {
	setup_bmps();

	auto hollow_brush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	auto arrow_cursor = LoadCursor(nil, IDC_ARROW);
	auto hand_cursor = LoadCursor(nil, IDC_HAND);
	UINumber percent50 = { .type = UINumber::Type::percent, .value = 50 };

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
		base_btn.dimensions.border_radius = { .type = UINumber::Type::dpi, .value = 10 };
		//TODO(fran): use the extra brushes, fore_push,... , border_mouseover,...
		base_btn.font = fonts.General;
		return base_btn;
	}();

	themes.base_editoneline = [&]()->auto {
		auto t = themes.base_editoneline;
		t.dimensions.border_thickness = 1;
		t.brushes.foreground.normal = colors.ControlTxt;
		t.brushes.foreground.disabled = colors.ControlTxt_Disabled;
		t.brushes.bk.normal = colors.ControlBkColored;
		t.brushes.bk.disabled = colors.ControlBk_Disabled;
		t.brushes.border.normal = colors.Img;
		t.brushes.border.disabled = colors.Img_Disabled;
		t.brushes.placeholder.normal = colors.ControlTxt_Disabled;
		t.brushes.selection.normal = colors.Img;
		t.brushes.selection.disabled = colors.Img_Disabled;
		return t;
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
		auto t = themes.base_editoneline;
		t.dimensions.border_thickness = 0;
		t.dimensions.border_radius = themes.base_btn.dimensions.border_radius;
		t.font = fonts.General;
		return t;
	}();

	themes.login_text_static_error = [&]()->auto {
		auto t = themes.login_text_static_error;
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		for (auto& b : t.brushes.bk.all) b = hollow_brush;
		for (auto& b : t.brushes.foreground.all) b = colors.Toast_Failure;
		t.font = fonts.SmallBold;
		return t;
	}();

	themes.login_btn = [&]()->auto {
		auto t = themes.base_btn;
		t.brushes.bk.normal = colors.ControlBkPush;
		t.dimensions.border_thickness = 0;
		t.dimensions.border_radius = percent50;
		t.font = fonts.GeneralBold;
		return t;
	}();

	themes.login_btn_toggle = [&]()->auto {
		auto t = themes.login_btn_toggle;
		for (auto& b : t.brushes.bk.all) b = hollow_brush;
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		t.brushes.foreground = {
			.normal = themes.login_btn.brushes.bk.normal,
			.disabled = themes.password_editor_toolbar_btn.brushes.foreground.disabled,
			.mouseover = themes.login_btn.brushes.bk.mouseover,
			.clicked = themes.login_btn.brushes.bk.clicked,
		};
		t.cursor = hand_cursor;
		t.font = fonts.Menu;
		return t;
	}();

	themes.editor_add_btn = [&]()->auto {
		auto t = themes.login_btn;
		return t;
	}();

	themes.editor_static_transition = [&]()->auto {
		edit_oneline::Theme t{};
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		for (auto& b : t.brushes.bk.all) b = hollow_brush;
		for (auto& b : t.brushes.foreground.all) b = colors.ControlBkMouseOver;
		t.font = fonts.GeneralBold;
		return t;
	}();

	themes.editor_btn_static_transition = [&]() {
		auto t = themes.editor_btn_static_transition;
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		for (auto& b : t.brushes.foreground.all) b = hollow_brush;
		t.brushes.bk = themes.editor_static_transition.brushes.foreground;
		return t;
	}();

	themes.editor_btn_static_bk_transition = [&]() {
		auto t = themes.editor_btn_static_transition;
		for (auto& b : t.brushes.bk.all) b = colors.ControlBkColored;
		t.dimensions = themes.base_btn.dimensions;
		return t;
	}();

	themes.transition_btn = [&]()->auto {
		button::Theme t{};
		for (auto& b : t.brushes.bk.all) b = colors.ControlBk_Disabled;
		for (auto& b : t.brushes.foreground.all) b = colors.ControlTxt_Disabled_Strong;
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		t.dimensions.border_radius = { .type = UINumber::Type::dpi, .value = 10 };
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
		table_toolbar_btn.dimensions.border_radius = percent50;
		return table_toolbar_btn;
	}();

	themes.table_toolbar_btn_danger = [&]()->auto {
		auto t = themes.login_btn;
		t.brushes.bk.normal = hollow_brush;
		t.brushes.bk.disabled = hollow_brush;
		t.brushes.bk.mouseover = colors.Btn_Delete_BkMouseOver;
		t.brushes.bk.clicked = colors.Btn_Delete_BkPush;
		t.dimensions.border_thickness = 0;
		t.dimensions.border_radius = percent50;
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

	themes.base_scrollbar = [&]()->auto {
		auto t = themes.base_scrollbar;

		t.dimensions.border_thickness = 1;
		t.dimensions.border_radius = percent50;
		for (auto& b : t.brushes.bk.all) b = colors.ScrollbarBk;
		for (auto& b : t.brushes.border.all) b = hollow_brush;
		for (auto& b : t.brushes.bar_bk.all) b = hollow_brush;
		for (auto& b : t.brushes.bar_border.all) b = colors.Scrollbar;
		t.brushes.bar_border.mouseover = colors.ScrollbarMouseOver;

		return t;
		}();
}
