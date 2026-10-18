#pragma once

#include "win_sdk.h"
#include "helpers.h"
#include "render_diagnostics.h"

#include <new>

namespace text_render {

template<typename Object>
void release_com(Object*& object) {
	if (object) {
		object->Release();
		object = nil;
	}
}

struct DirectWriteFactories {
	IDWriteFactory* write = nil;
	IDWriteGdiInterop* gdi_interop = nil;
	IDWriteBitmapRenderTarget* bitmap_render_target = nil;
	IDWriteRenderingParams* bitmap_rendering_params = nil;
	ID2D1Factory* draw = nil;
	~DirectWriteFactories() {
		release_com(bitmap_render_target);
		release_com(bitmap_rendering_params);
		release_com(gdi_interop);
		release_com(draw);
		release_com(write);
	}
};

DirectWriteFactories& get_directwrite_factories() {
	static DirectWriteFactories factories{};
	if (!factories.write)
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&factories.write);
	if (!factories.draw)
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factories.draw);
	return factories;
}

bool ensure_bitmap_render_target(HDC dc, const RECT& bounds, IDWriteBitmapRenderTarget** target,
	IDWriteRenderingParams** rendering_params) {
	render_diagnostics::Scope diagnostic{ render_diagnostics::Event::bitmap_target };
	*target = nil;
	*rendering_params = nil;
	auto& factories = get_directwrite_factories();
	if (!factories.write || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return false;
	if (!factories.gdi_interop && factories.write->GetGdiInterop(&factories.gdi_interop) != S_OK) return false;
	if (!factories.bitmap_rendering_params &&
		factories.write->CreateRenderingParams(&factories.bitmap_rendering_params) != S_OK) return false;

	UINT32 width = (UINT32)(bounds.right - bounds.left);
	UINT32 height = (UINT32)(bounds.bottom - bounds.top);
	if (!factories.bitmap_render_target) {
		render_diagnostics::Scope create_diagnostic{ render_diagnostics::Event::bitmap_target_create };
		if (factories.gdi_interop->CreateBitmapRenderTarget(dc, width, height, &factories.bitmap_render_target) != S_OK)
			return false;
		if (factories.bitmap_render_target->SetPixelsPerDip(1.f) != S_OK) {
			release_com(factories.bitmap_render_target);
			return false;
		}
	}
	else {
		SIZE current_size{};
		if (factories.bitmap_render_target->GetSize(&current_size) != S_OK) return false;
		if (width > (UINT32)current_size.cx || height > (UINT32)current_size.cy) {
			render_diagnostics::Scope resize_diagnostic{ render_diagnostics::Event::bitmap_target_resize };
			UINT32 new_width = maximum(width, (UINT32)current_size.cx * 2);
			UINT32 new_height = maximum(height, (UINT32)current_size.cy * 2);
			if (factories.bitmap_render_target->Resize(new_width, new_height) != S_OK) return false;
		}
	}
	*target = factories.bitmap_render_target;
	(*target)->AddRef();
	*rendering_params = factories.bitmap_rendering_params;
	return true;
}

struct BitmapTextRenderer final : IDWriteTextRenderer {
	LONG reference_count = 1;
	IDWriteBitmapRenderTarget* target;
	IDWriteRenderingParams* rendering_params;
	COLORREF color;

	BitmapTextRenderer(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams* rendering_params, COLORREF color) :
		target(target), rendering_params(rendering_params), color(color) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id, void** object) override {
		if (!object) return E_POINTER;
		*object = nil;
		if (interface_id == __uuidof(IUnknown) || interface_id == __uuidof(IDWriteTextRenderer))
			*object = static_cast<IDWriteTextRenderer*>(this);
		else if (interface_id == __uuidof(IDWritePixelSnapping))
			*object = static_cast<IDWritePixelSnapping*>(this);
		if (!*object) return E_NOINTERFACE;
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&reference_count); }

	ULONG STDMETHODCALLTYPE Release() override {
		ULONG remaining = (ULONG)InterlockedDecrement(&reference_count);
		if (!remaining) delete this;
		return remaining;
	}

	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
		if (!disabled) return E_POINTER;
		*disabled = FALSE;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
		return target->GetCurrentTransform(transform);
	}

	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels_per_dip) override {
		if (!pixels_per_dip) return E_POINTER;
		*pixels_per_dip = target->GetPixelsPerDip();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baseline_x, FLOAT baseline_y,
		DWRITE_MEASURING_MODE measuring_mode, const DWRITE_GLYPH_RUN* glyph_run,
		const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override {
		render_diagnostics::Scope diagnostic{ render_diagnostics::Event::bitmap_draw_glyph };
		return target->DrawGlyphRun(baseline_x, baseline_y, measuring_mode, glyph_run,
			rendering_params, color, nil);
	}

	HRESULT fill_decoration(FLOAT x, FLOAT baseline_y, FLOAT offset, FLOAT width, FLOAT thickness) {
		RECT bounds{
			(LONG)floorf(x), (LONG)floorf(baseline_y + offset),
			(LONG)ceilf(x + width), (LONG)ceilf(baseline_y + offset + thickness)
		};
		HBRUSH brush = CreateSolidBrush(color);
		if (!brush) return E_FAIL;
		int filled = FillRect(target->GetMemoryDC(), &bounds, brush);
		DeleteObject(brush);
		return filled ? S_OK : E_FAIL;
	}

	HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT baseline_x, FLOAT baseline_y,
		const DWRITE_UNDERLINE* underline, IUnknown*) override {
		return fill_decoration(baseline_x, baseline_y, underline->offset, underline->width, underline->thickness);
	}

	HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT baseline_x, FLOAT baseline_y,
		const DWRITE_STRIKETHROUGH* strikethrough, IUnknown*) override {
		return fill_decoration(baseline_x, baseline_y, strikethrough->offset,
			strikethrough->width, strikethrough->thickness);
	}

	HRESULT STDMETHODCALLTYPE DrawInlineObject(void* context, FLOAT origin_x, FLOAT origin_y,
		IDWriteInlineObject* inline_object, BOOL sideways, BOOL right_to_left, IUnknown* effect) override {
		return inline_object->Draw(context, this, origin_x, origin_y, sideways, right_to_left, effect);
	}
};

struct BitmapDrawSession {
	HDC destination = nil;
	RECT bounds{};
	IDWriteBitmapRenderTarget* target = nil;
	BitmapTextRenderer* renderer = nil;
	bool valid = false;

	~BitmapDrawSession() {
		if (renderer) renderer->Release();
		release_com(target);
	}

	bool begin(HDC dc, const RECT& draw_bounds, COLORREF color) {
		IDWriteRenderingParams* rendering_params = nil;
		if (!ensure_bitmap_render_target(dc, draw_bounds, &target, &rendering_params)) {
			render_diagnostics::record_count(render_diagnostics::Event::bitmap_failure);
			return false;
		}
		renderer = new (std::nothrow) BitmapTextRenderer(target, rendering_params, color);
		if (!renderer) {
			render_diagnostics::record_count(render_diagnostics::Event::bitmap_failure);
			return false;
		}
		destination = dc;
		bounds = draw_bounds;
		{
			render_diagnostics::Scope diagnostic{ render_diagnostics::Event::bitmap_copy_in };
			valid = BitBlt(target->GetMemoryDC(), 0, 0, bounds.right - bounds.left,
				bounds.bottom - bounds.top, dc, bounds.left, bounds.top, SRCCOPY) != 0;
		}
		if (!valid) render_diagnostics::record_count(render_diagnostics::Event::bitmap_failure);
		return valid;
	}

	bool draw(IDWriteTextLayout* layout, FLOAT x, FLOAT y) {
		if (!valid) return false;
		render_diagnostics::Scope diagnostic{ render_diagnostics::Event::bitmap_draw_layout };
		valid = layout->Draw(nil, renderer, x - bounds.left, y - bounds.top) == S_OK;
		if (!valid) render_diagnostics::record_count(render_diagnostics::Event::bitmap_failure);
		return valid;
	}

	bool finish() {
		if (!valid) return false;
		bool copied;
		{
			render_diagnostics::Scope diagnostic{ render_diagnostics::Event::bitmap_copy_out };
			copied = BitBlt(destination, bounds.left, bounds.top, bounds.right - bounds.left,
				bounds.bottom - bounds.top, target->GetMemoryDC(), 0, 0, SRCCOPY) != 0;
		}
		if (!copied) render_diagnostics::record_count(render_diagnostics::Event::bitmap_failure);
		return copied;
	}
};

bool create_text_format(HDC dc, HFONT font, IDWriteTextFormat** format) {
	render_diagnostics::Scope diagnostic{ render_diagnostics::Event::text_format };
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
	render_diagnostics::Scope diagnostic{ render_diagnostics::Event::text_layout };
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
	render_diagnostics::Scope diagnostic{ render_diagnostics::Event::dc_target };
	*target = nil;
	auto& factories = get_directwrite_factories();
	if (!factories.draw || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return false;

	D2D1_RENDER_TARGET_PROPERTIES properties{};
	properties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	properties.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
	properties.dpiX = properties.dpiY = 96.f;
	properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
	properties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
	if (factories.draw->CreateDCRenderTarget(&properties, target) != S_OK) {
		render_diagnostics::record_count(render_diagnostics::Event::dc_target_create_failure);
		return false;
	}
	render_diagnostics::record_count(render_diagnostics::Event::dc_target_create);
	if ((*target)->BindDC(dc, &bounds) != S_OK) {
		render_diagnostics::record_count(render_diagnostics::Event::dc_bind_failure);
		release_com(*target);
		return false;
	}
	return true;
}

bool create_text_brush(ID2D1DCRenderTarget* target, COLORREF color, ID2D1SolidColorBrush** brush) {
	render_diagnostics::Scope diagnostic{ render_diagnostics::Event::dc_brush };
	*brush = nil;
	D2D1_COLOR_F brush_color{ GetRValue(color) / 255.f, GetGValue(color) / 255.f, GetBValue(color) / 255.f, 1.f };
	return target->CreateSolidColorBrush(brush_color, brush) == S_OK;
}

}
