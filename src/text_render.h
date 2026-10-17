#pragma once

#include "win_sdk.h"
#include "helpers.h"

namespace text_render {

template<typename Object>
void release_com(Object*& object) {
	if (object) {
		object->Release();
		object = nil;
	}
}

struct DirectWriteFactories {
	IDWriteFactory* write;
	ID2D1Factory* draw;
};

DirectWriteFactories& get_directwrite_factories() {
	static DirectWriteFactories factories{};
	if (!factories.write)
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&factories.write);
	if (!factories.draw)
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factories.draw);
	return factories;
}

bool create_text_format(HDC dc, HFONT font, IDWriteTextFormat** format) {
	*format = nil;
	auto& factories = get_directwrite_factories();
	if (!factories.write || !font) return false;

	LOGFONT logfont{};
	if (GetObject(font, sizeof(logfont), &logfont) != sizeof(logfont)) return false;
	HFONT oldfont = SelectFont(dc, font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC metrics{};
	GetTextMetrics(dc, &metrics);
	f32 font_size = (f32)(logfont.lfHeight < 0 ? -logfont.lfHeight : metrics.tmHeight - metrics.tmInternalLeading);
	font_size = maximum(1.f, font_size);

	utf16 locale_name[LOCALE_NAME_MAX_LENGTH]{};
	if (!GetUserDefaultLocaleName(locale_name, ARRAYSIZE(locale_name))) wcscpy_s(locale_name, _t("en-us"));
	DWRITE_FONT_WEIGHT weight = logfont.lfWeight ? (DWRITE_FONT_WEIGHT)clamp(1L, logfont.lfWeight, 999L) : DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE font_style = logfont.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
	return factories.write->CreateTextFormat(logfont.lfFaceName, nil, weight, font_style,
		DWRITE_FONT_STRETCH_NORMAL, font_size, locale_name, format) == S_OK;
}

bool create_text_layout(HDC dc, HFONT font, str_view text, IDWriteTextLayout** layout,
	DWRITE_READING_DIRECTION reading_direction = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) {
	*layout = nil;
	if (text.size() > (size_t)U32MAX) return false;
	auto& factories = get_directwrite_factories();
	if (!factories.write) return false;

	IDWriteTextFormat* format = nil;
	if (!create_text_format(dc, font, &format)) return false;
	defer{ release_com(format); };
	format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	HRESULT result = factories.write->CreateGdiCompatibleTextLayout(text.data(), (UINT32)text.size(),
		format, 1000000.f, 1000000.f, 1.f, nil, FALSE, layout);
	if (result != S_OK) return false;
	(*layout)->SetReadingDirection(reading_direction);
	(*layout)->SetFlowDirection(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
	DWRITE_TEXT_METRICS metrics{};
	if ((*layout)->GetMetrics(&metrics) == S_OK) {
		(*layout)->SetMaxWidth(maximum(1.f, metrics.widthIncludingTrailingWhitespace));
		(*layout)->SetMaxHeight(maximum(1.f, metrics.height));
	}
	return true;
}

bool create_dc_render_target(HDC dc, const RECT& bounds, ID2D1DCRenderTarget** target) {
	*target = nil;
	auto& factories = get_directwrite_factories();
	if (!factories.draw || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return false;

	D2D1_RENDER_TARGET_PROPERTIES properties{};
	properties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	properties.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
	properties.dpiX = properties.dpiY = 96.f;
	properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
	properties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
	if (factories.draw->CreateDCRenderTarget(&properties, target) != S_OK) return false;
	if ((*target)->BindDC(dc, &bounds) == S_OK) return true;
	release_com(*target);
	return false;
}

bool create_text_brush(ID2D1DCRenderTarget* target, COLORREF color, ID2D1SolidColorBrush** brush) {
	*brush = nil;
	D2D1_COLOR_F brush_color{ GetRValue(color) / 255.f, GetGValue(color) / 255.f, GetBValue(color) / 255.f, 1.f };
	return target->CreateSolidColorBrush(brush_color, brush) == S_OK;
}

}
