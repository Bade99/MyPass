#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "math.h"
#include "text_render.h"

#define USE_GDIPLUS /*For lazy bilinear filtered drawing*/

#ifdef USE_GDIPLUS
#include <objidl.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")
#endif

#ifdef _DEBUG
#define COBJMACROS

#include <Objbase.h>
#include <wincodec.h>
#include <Winerror.h>

#pragma comment(lib, "Windowscodecs.lib")

HRESULT WriteBitmap(HBITMAP bitmap, const cstr* pathname) {
	//TODO(fran): solve limitations https://stackoverflow.com/questions/24720451/save-hbitmap-to-bmp-file-using-only-win32
	HRESULT initialization_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(initialization_result)) return initialization_result;
	defer{ CoUninitialize(); };
	HRESULT hr = S_OK;

	// (1) Retrieve properties from the source HBITMAP.
	BITMAP bm_info = { 0 };
	if (!GetObject(bitmap, sizeof(bm_info), &bm_info))
		hr = E_FAIL;

	// (2) Create an IWICImagingFactory instance.
	IWICImagingFactory* factory = NULL;
	if (SUCCEEDED(hr))
		hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
			IID_IWICImagingFactory, (LPVOID*)&factory);

	// (3) Create an IWICBitmap instance from the HBITMAP.
	IWICBitmap* wic_bitmap = NULL;
	if (SUCCEEDED(hr))
		hr = factory->CreateBitmapFromHBITMAP(bitmap, NULL,
			WICBitmapIgnoreAlpha,
			&wic_bitmap);

	// (4) Create an IWICStream instance, and attach it to a filename.
	IWICStream* stream = NULL;
	if (SUCCEEDED(hr))
		hr = factory->CreateStream(&stream);
	if (SUCCEEDED(hr))
		hr = stream->InitializeFromFilename(pathname, GENERIC_WRITE);

	// (5) Create an IWICBitmapEncoder instance, and associate it with the stream.
	IWICBitmapEncoder* encoder = NULL;
	if (SUCCEEDED(hr))
		hr = factory->CreateEncoder(GUID_ContainerFormatBmp, NULL,
			&encoder);
	if (SUCCEEDED(hr))
		hr = encoder->Initialize((IStream*)stream,
			WICBitmapEncoderNoCache);

	// (6) Create an IWICBitmapFrameEncode instance, and initialize it
	// in compliance with the source HBITMAP.
	IWICBitmapFrameEncode* frame = NULL;
	if (SUCCEEDED(hr))
		hr = encoder->CreateNewFrame(&frame, NULL);
	if (SUCCEEDED(hr))
		hr = frame->Initialize(NULL);
	if (SUCCEEDED(hr))
		hr = frame->SetSize(bm_info.bmWidth, bm_info.bmHeight);
	if (SUCCEEDED(hr)) {
		GUID pixel_format;
		if (bm_info.bmBitsPixel == 32)		pixel_format = GUID_WICPixelFormat32bppBGRA;
		else if (bm_info.bmBitsPixel == 24)	pixel_format = GUID_WICPixelFormat24bppBGR;
		else if (bm_info.bmBitsPixel == 1)	pixel_format = GUID_WICPixelFormatBlackWhite;
		else Assert(0);

		hr = frame->SetPixelFormat(&pixel_format);
	}

	// (7) Write bitmap data to the frame.
	if (SUCCEEDED(hr))
		hr = frame->WriteSource((IWICBitmapSource*)wic_bitmap,
			NULL);

	// (8) Commit frame and data to stream.
	if (SUCCEEDED(hr))
		hr = frame->Commit();
	if (SUCCEEDED(hr))
		hr = encoder->Commit();

	// Cleanup
	if (frame)
		frame->Release();
	if (encoder)
		encoder->Release();
	if (stream)
		stream->Release();
	if (wic_bitmap)
		wic_bitmap->Release();
	if (factory)
		factory->Release();

	return hr;
}
#else
HRESULT WriteBitmap(HBITMAP bitmap, const cstr* pathname = _t("")) {};
#endif

void GetRoundRectPath(Gdiplus::GraphicsPath* path, RECT r, float diameter) {
	float w = RECTW(r), h = RECTH(r);
	if (diameter > w) diameter = w;
	if (diameter > h) diameter = h;

	Gdiplus::RectF Corner(r.left, r.top, diameter, diameter);

	path->Reset();

	// top left
	path->AddArc(Corner, 180, 90);

	// top right
	Corner.X += (w - diameter - 1);
	path->AddArc(Corner, 270, 90);

	// bottom right
	Corner.Y += (h - diameter - 1);
	path->AddArc(Corner, 0, 90);

	// bottom left
	Corner.X -= (w - diameter - 1);
	path->AddArc(Corner, 90, 90);

	path->CloseFigure();
}

void SetUpRenderSettings(Gdiplus::Graphics& graphics) {
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics.SetPageUnit(Gdiplus::UnitPixel);
}

Gdiplus::Color HbrushToGdiplusColor(HBRUSH br) {
	Gdiplus::Color c; 
	static auto hollow_brush = GetStockBrush(HOLLOW_BRUSH);
	if (br == hollow_brush) c = c.Transparent;
	else c.SetFromCOLORREF(ColorFromBrush(br));
	return c;
}

namespace urender {
	struct MaskImage {
		i32 width;
		i32 height;
		i32 pitch;
		u8 bits_per_pixel;
		u8* mem;
	};

	i32 bitmap_draw_size(i32 desired, i32 available_width, i32 available_height) {
		return maximum(0, minimum(desired, minimum(available_width, available_height)));
	}

	HBITMAP create_DIB32(int width, int height, u32** pixels) {
		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(bmi);
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)pixels, nullptr, 0);
	}

	struct MaskFilterAxis {
		i32 first;
		i32 last;
		f32 start;
		f32 end;
		f32 scale;
	};

	MaskFilterAxis mask_filter_axis(i32 destination_index, i32 destination_size, i32 source_size) {
		f32 scale = safe_ratio0((f32)source_size, (f32)destination_size);
		if (scale > 1.f) {
			f32 start = destination_index * scale;
			f32 end = (destination_index + 1) * scale;
			return { (i32)floorf(start), minimum(source_size - 1, (i32)ceilf(end) - 1), start, end, scale };
		}
		f32 center = (destination_index + .5f) * scale - .5f;
		i32 first = (i32)floorf(center);
		return { first, first + 1, center - first, 0.f, scale };
	}

	f32 mask_filter_weight(const MaskFilterAxis& axis, i32 source_index) {
		if (axis.scale > 1.f)
			return safe_ratio0(minimum(axis.end, (f32)source_index + 1.f) - maximum(axis.start, (f32)source_index), axis.scale);
		return source_index == axis.first ? 1.f - axis.start : axis.start;
	}

	u8 mask_coverage(const MaskImage& mask, i32 x, i32 y) {
		x = clamp(0, x, mask.width - 1);
		y = clamp(0, y, mask.height - 1);
		u8* row = mask.mem + y * mask.pitch;
		if (mask.bits_per_pixel == 1)
			return row[x / 8] & (0x80 >> (x % 8)) ? 0 : 255;
		return row[x];
	}

	u8 filtered_mask_coverage(const MaskImage& mask, i32 x, i32 y, i32 width, i32 height) {
		if (width == mask.width && height == mask.height) return mask_coverage(mask, x, y);
		MaskFilterAxis x_axis = mask_filter_axis(x, width, mask.width);
		MaskFilterAxis y_axis = mask_filter_axis(y, height, mask.height);
		f32 coverage = 0.f;
		for (i32 source_y = y_axis.first; source_y <= y_axis.last; ++source_y) {
			f32 y_weight = mask_filter_weight(y_axis, source_y);
			for (i32 source_x = x_axis.first; source_x <= x_axis.last; ++source_x)
				coverage += y_weight * mask_filter_weight(x_axis, source_x) * mask_coverage(mask, source_x, source_y);
		}
		return (u8)roundf(clamp(0.f, coverage, 255.f));
	}

	void draw_mask(HDC destination_dc, i32 destination_x, i32 destination_y, i32 destination_width, i32 destination_height, HBITMAP mask, HBRUSH color_brush) {
		if (!mask || destination_width <= 0 || destination_height <= 0) return;
		BITMAP bitmap{};
		if (!GetObject(mask, sizeof(bitmap), &bitmap) || (bitmap.bmBitsPixel != 1 && bitmap.bmBitsPixel != 8) ||
			bitmap.bmWidth <= 0 || bitmap.bmHeight <= 0 || bitmap.bmWidthBytes <= 0 ||
			bitmap.bmHeight > LONG_MAX / bitmap.bmWidthBytes ||
			destination_width > LONG_MAX / 4 || destination_height > LONG_MAX / (destination_width * 4)) return;

		MaskImage source{ bitmap.bmWidth, bitmap.bmHeight, bitmap.bmWidthBytes, (u8)bitmap.bmBitsPixel };
		i32 byte_count = source.height * source.pitch;
		source.mem = (u8*)malloc(byte_count);
		if (!source.mem) return;
		defer{ free(source.mem); };
		if (GetBitmapBits(mask, byte_count, source.mem) != byte_count) return;

		u32* pixels = nullptr;
		HBITMAP image = create_DIB32(destination_width, destination_height, &pixels);
		if (!image || !pixels) return;
		defer{ DeleteObject(image); };
		COLORREF color = ColorFromBrush(color_brush);
		for (i32 y = 0; y < destination_height; ++y) {
			for (i32 x = 0; x < destination_width; ++x) {
				u8 alpha = filtered_mask_coverage(source, x, y, destination_width, destination_height);
				u32 red = (GetRValue(color) * alpha + 127) / 255;
				u32 green = (GetGValue(color) * alpha + 127) / 255;
				u32 blue = (GetBValue(color) * alpha + 127) / 255;
				pixels[y * destination_width + x] = (u32(alpha) << 24) | (red << 16) | (green << 8) | blue;
			}
		}

		HDC memory_dc = CreateCompatibleDC(destination_dc);
		if (!memory_dc) return;
		defer{ DeleteDC(memory_dc); };
		HBITMAP previous_image = (HBITMAP)SelectObject(memory_dc, image);
		if (!previous_image) return;
		defer{ SelectObject(memory_dc, previous_image); };
		BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		AlphaBlend(destination_dc, destination_x, destination_y, destination_width, destination_height,
			memory_dc, 0, 0, destination_width, destination_height, blend);
	}

	void draw_icon(HDC dest, int destination_x, int destination_y, int destination_width, int destination_height, HICON icon, int source_x, int source_y, int source_width, int source_height) {
		Gdiplus::Graphics graphics(dest);//Yeah, who cares, icons are small, I just want bilinear filtering
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
		Gdiplus::Bitmap bmp(icon);
		//TODO?(fran): with gdi+ we can do subpixel placement (RectF)
		graphics.DrawImage(&bmp,
			Gdiplus::Rect(destination_x, destination_y, destination_width, destination_height),
			source_x, source_y, source_width, source_height,
			Gdiplus::Unit::UnitPixel);
	}

	void draw_round_rectangle(HDC dest, RECT r, float radius, HBRUSH color_br) {
		Gdiplus::Graphics graphics(dest);
		SetUpRenderSettings(graphics);
		auto diameter = radius * 2;
		Gdiplus::SolidBrush br(HbrushToGdiplusColor(color_br));
		Gdiplus::GraphicsPath path;
		GetRoundRectPath(&path, r, diameter);
		graphics.FillPath(&br, &path);
	}

	void draw_round_rectangle_outline(HDC dest, RECT r, float radius, HBRUSH color_br, f32 thickness = 1) {
		Gdiplus::Graphics graphics(dest);
		SetUpRenderSettings(graphics);
		auto diameter = radius * 2;
		Gdiplus::Pen pen(HbrushToGdiplusColor(color_br), thickness);
		pen.SetLineJoin(Gdiplus::LineJoinRound); // Helps slightly degenerate roundrects to still render properly. This happens as radius approaches 50%, my roundrect calculation in GetRoundRectPath can generate an empty space between each corner arc of the roundrect, LineJoinRound connects this disjointed sections in a way that makes it unnoticeable
		Gdiplus::GraphicsPath path;

		if (thickness > 1) {
			i32 inset = thickness / 2.f;
			i32 integer_thickness = inset * 2;
			i32 w = RECTW(r), h = RECTH(r);
			r.left += inset;
			r.top += inset;
			r.right = r.left + w - integer_thickness;
			r.bottom = r.top + h - integer_thickness;
			diameter = (radius - inset) * 2; // not sure this is necessary
		}

		GetRoundRectPath(&path, r, diameter);
		graphics.DrawPath(&pen, &path);
	}

	void FillRectAlpha(HDC dc, const RECT& rc, u8 r, u8 g, u8 b, u8 a) {
#ifdef USE_GDIPLUS
		using namespace Gdiplus;
		Graphics graphics(dc);
		SolidBrush brush(Color(a, r, g, b));
		graphics.FillRectangle(&brush, (i32)rc.left, (i32)rc.top, (i32)RECTW(rc), (i32)RECTH(rc));
#endif
	}

	template<typename T>
	void draw_background(HDC dc, const RECT& r, HBRUSH bk, HBRUSH border, const T& dimensions) {
		render_diagnostics::Scope diagnostic{ render_diagnostics::Event::background };
		auto w = RECTW(r), h = RECTH(r);
		auto min_dim = minimum(w, h);
		auto borderSize = dimensions.border_thickness;
		if (dimensions.border_radius.value) {
			auto radius = dimensions.border_radius.to_px(min_dim);
			urender::draw_round_rectangle(dc, r, radius, bk);
			if (borderSize) urender::draw_round_rectangle_outline(dc, r, radius, border, borderSize);
		}
		else {
			static auto hollow_brush = GetStockBrush(HOLLOW_BRUSH);
			if (borderSize && border != hollow_brush) {
				HPEN pen = CreatePen(PS_SOLID | PS_INSIDEFRAME, borderSize, ColorFromBrush(border));
				auto oldpen = SelectPen(dc, pen); defer{ SelectPen(dc, oldpen); DeletePen(pen); };
				auto oldbr = SelectBrush(dc, bk); defer{ SelectBrush(dc, oldbr); };

				Rectangle(dc, r.left, r.top, r.right, r.bottom); //uses pen for border and brush for bk
			}
			else {
				FillRect(dc, &r, bk);
			}
		}
	}

	enum class TextAlignment { center, left, right };
	enum class TextVerticalAlignment { center, top, bottom };

	SIZE measure_text(HDC dc, HFONT font, str_view text) {
		IDWriteTextLayout* layout = nil;
		if (text_render::create_text_layout(dc, font, text, &layout)) {
			defer{ text_render::release_com(layout); };
			DWRITE_TEXT_METRICS metrics{};
			if (layout->GetMetrics(&metrics) == S_OK)
				return { (LONG)ceilf(metrics.widthIncludingTrailingWhitespace), (LONG)ceilf(metrics.height) };
		}
		HFONT oldfont = font ? SelectFont(dc, font) : nil;
		defer{ if (oldfont) SelectFont(dc, oldfont); };
		SIZE size{};
		if (text.size() <= (size_t)I32MAX)
			GetTextExtentPoint32(dc, text.data(), (int)text.size(), &size);
		return size;
	}

	struct MenuText {
		str_view label;
		str_view shortcut;
	};

	MenuText split_menu_text(str_view text) {
		auto tab = text.find(L'\t');
		if (tab == str_view::npos) return { text, {} };
		return { text.substr(0, tab), text.substr(tab + 1) };
	}

	SIZE measure_menu_text(HDC dc, HFONT font, str_view text) {
		auto parts = split_menu_text(text);
		auto label_size = measure_text(dc, font, parts.label);
		if (parts.shortcut.empty()) return label_size;
		auto shortcut_size = measure_text(dc, font, parts.shortcut);
		auto gap = measure_text(dc, font, L"    ").cx;
		auto right_pad = measure_text(dc, font, L" ").cx;
		return { label_size.cx + gap + shortcut_size.cx + right_pad, maximum(label_size.cy, shortcut_size.cy) };
	}

	//Intended for a single line of text
	void draw_text(HDC dc, const RECT& r, utf16_str txt, HFONT f, HBRUSH txt_br, TextAlignment alignment, i32 pad_x,
		TextVerticalAlignment vertical_alignment = TextVerticalAlignment::center, i32 pad_y = 0) {
		if (!txt.str || !txt.cnt() || r.right <= r.left || r.bottom <= r.top) return;
		render_diagnostics::Scope diagnostic{ render_diagnostics::Event::static_text };
		IDWriteTextLayout* layout = nil;
		if (text_render::create_text_layout(dc, f, { txt.str, txt.cnt() }, &layout)) {
			defer{ text_render::release_com(layout); };
			DWRITE_TEXT_METRICS metrics{};
			if (layout->GetMetrics(&metrics) == S_OK) {
				f32 width = (f32)(r.right - r.left);
				f32 height = (f32)(r.bottom - r.top);
				f32 x = alignment == TextAlignment::center ? (width - metrics.widthIncludingTrailingWhitespace) / 2.f
					: alignment == TextAlignment::right ? width - pad_x - metrics.widthIncludingTrailingWhitespace : (f32)pad_x;
				f32 y = vertical_alignment == TextVerticalAlignment::top ? (f32)pad_y
					: vertical_alignment == TextVerticalAlignment::bottom ? height - pad_y - metrics.height
					: (height - metrics.height) / 2.f;
				D2D1_POINT_2F origin{ (f32)r.left + x, (f32)r.top + y };
				if (render_diagnostics::directwrite_gdi()) {
					text_render::BitmapDrawSession bitmap_session;
					if (bitmap_session.begin(dc, r, ColorFromBrush(txt_br)) &&
						bitmap_session.draw(layout, origin.x, origin.y) && bitmap_session.finish()) return;
				}
				ID2D1DCRenderTarget* target = nil;
				if (text_render::create_dc_render_target(dc, r, &target)) {
					defer{ text_render::release_com(target); };
					ID2D1SolidColorBrush* brush = nil;
					if (text_render::create_text_brush(target, ColorFromBrush(txt_br), &brush)) {
						defer{ text_render::release_com(brush); };
						{
							render_diagnostics::Scope draw_diagnostic{ render_diagnostics::Event::dc_begin_draw };
							target->BeginDraw();
						}
						target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
						target->DrawTextLayout({ x, y }, layout, brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
						HRESULT result;
						{
							render_diagnostics::Scope draw_diagnostic{ render_diagnostics::Event::dc_end_draw };
							result = target->EndDraw();
						}
						if (result == S_OK) return;
						render_diagnostics::record_count(render_diagnostics::Event::dc_end_failure);
					}
				}
			}
		}

		render_diagnostics::Scope gdi_diagnostic{ render_diagnostics::Event::gdi_text };
		HFONT oldfont = f ? SelectFont(dc, f) : nil;
		defer{ if (oldfont) SelectFont(dc, oldfont); };
		UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };

		COLORREF oldtxtcol = SetTextColor(dc, ColorFromBrush(txt_br)); defer{ SetTextColor(dc, oldtxtcol); };
		auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };

		TEXTMETRIC tm; GetTextMetrics(dc, &tm);
		i32 yPos = vertical_alignment == TextVerticalAlignment::top ? r.top + pad_y
			: vertical_alignment == TextVerticalAlignment::bottom ? r.bottom - pad_y - tm.tmHeight
			: (r.bottom + r.top - tm.tmHeight) / 2;
		i32 xPos;
		switch (alignment) {
		case decltype(alignment)::center:
		{
			SetTextAlign(dc, TA_CENTER);
			xPos = (r.right + r.left) / 2;
		} break;
		case decltype(alignment)::left:
		{
			SetTextAlign(dc, TA_LEFT);
			xPos = r.left + pad_x;
		} break;
		case decltype(alignment)::right:
		{
			SetTextAlign(dc, TA_RIGHT);
			xPos = r.right - pad_x;
		} break;
		default: Assert(0);
		}
		TextOut(dc, xPos, yPos, txt.str, (i32)(txt.sz_char() - 1));
	}

	void draw_menu_text(HDC dc, const RECT& bounds, str_view text, HFONT font, HBRUSH brush) {
		auto parts = split_menu_text(text);
		RECT label_bounds = bounds;
		if (!parts.shortcut.empty()) {
			auto shortcut_width = measure_text(dc, font, parts.shortcut).cx;
			auto gap = measure_text(dc, font, L"    ").cx;
			auto right_pad = measure_text(dc, font, L" ").cx;
			label_bounds.right = maximum(label_bounds.left, bounds.right - shortcut_width - gap - right_pad);
			utf16_str shortcut{ const_cast<utf16*>(parts.shortcut.data()), (parts.shortcut.size() + 1) * sizeof(utf16) };
			draw_text(dc, bounds, shortcut, font, brush, TextAlignment::right, right_pad);
		}
		utf16_str label{ const_cast<utf16*>(parts.label.data()), (parts.label.size() + 1) * sizeof(utf16) };
		draw_text(dc, label_bounds, label, font, brush, TextAlignment::left, 0);
	}

	i32 draw_bitmap_1bpp_right(HBITMAP bmp, HDC dc, rect_i32 r, int x_pad, HBRUSH color) {
		//TODO(fran): flicker free
		BITMAP bitmap; GetObject(bmp, sizeof(bitmap), &bitmap);
		Assert(bitmap.bmBitsPixel == 1);
		int max_sz = bitmap_draw_size((int)((f32)r.h * .6f), r.w - x_pad, r.h);
		if (!max_sz) return r.left + r.w - x_pad;

		int bmp_height = max_sz;
		int bmp_width = bmp_height;
		int bmp_align_width = r.left + r.w - bmp_width - x_pad;
		int bmp_align_height = r.top + (r.h - bmp_height) / 2;
		draw_mask(dc, bmp_align_width, bmp_align_height, bmp_width, bmp_height, bmp, color);

		return bmp_align_width;
	}


	#ifdef USE_GDIPLUS
	struct GdiplusLifetime {
		ULONG_PTR gdiplus_token;

		GdiplusLifetime() {
			// Initialize GDI+
			Gdiplus::GdiplusStartupInput gdiplus_startup_input;
			Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_startup_input, nil);
		}
		~GdiplusLifetime() {
			//Uninitialize GDI+
			Gdiplus::GdiplusShutdown(gdiplus_token);
		}
	} static const gdiplus_lifetime;
	#endif
}
