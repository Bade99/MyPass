#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "math.h"

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

HRESULT WriteBitmap(HBITMAP bitmap, const wchar_t* pathname) {
	//TODO(fran): solve limitations https://stackoverflow.com/questions/24720451/save-hbitmap-to-bmp-file-using-only-win32
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
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
	CoUninitialize();
}
#else
HRESULT WriteBitmap(HBITMAP bitmap, const wchar_t* pathname = L"") {};
#endif

int FillRectAlpha(HDC dc, const RECT& r, HBRUSH br, u8 alpha) {
	COLORREF color = ColorFromBrush(br);
	color |= (alpha << 24);

	BITMAPINFO nfo;
	nfo.bmiHeader.biSize = sizeof(nfo.bmiHeader);
	nfo.bmiHeader.biWidth = 1;
	nfo.bmiHeader.biHeight = -1;
	nfo.bmiHeader.biPlanes = 1;
	nfo.bmiHeader.biBitCount = 32;
	nfo.bmiHeader.biCompression = BI_RGB;
	nfo.bmiHeader.biSizeImage = 0;
	nfo.bmiHeader.biXPelsPerMeter = 0;
	nfo.bmiHeader.biYPelsPerMeter = 0;
	nfo.bmiHeader.biClrUsed = 0;
	nfo.bmiHeader.biClrImportant = 0;

	int res = StretchDIBits(dc, r.left, r.top, RECTW(r), RECTH(r), 0, 0, 1, 1, &color, &nfo, DIB_RGB_COLORS, SRCCOPY);
	return res;
}

struct img {
	i32 width;
	i32 height;
	i32 pitch;
	u8 bits_per_pixel;
	void* mem;
};

struct mask_bilinear_sample { u8 sample[4]; };
mask_bilinear_sample sample_bilinear_mask(img* mask, i32 x, i32 y) {
	//NOTE: handmade 108 1:20:00 interesting, once you scale an img to half size or more you get sampling errors cause you're no longer sampling all the pixels involved, that's when you need MIPMAPPING

	u8 idx = 7 - (x % 8);

	x /= 8;

	u8* texel_ptr = ((u8*)mask->mem + y * mask->pitch + x * 1);

	u8 tex[4];

	//TODO(fran): remove duplicate pixel reads by applying the "&" afterwards
	tex[0] = (*(u8*)texel_ptr) & (1 << idx); //NOTE: For a better blend we pick the colors around the texel, movement is much smoother
	tex[1] = idx == 0 ? (x < mask->width ? (*(u8*)(texel_ptr + 1)) & (1 << 7) : 0xFF) : (*(u8*)texel_ptr) & (1 << (idx - 1));
	tex[2] = y < mask->height ? (*(u8*)(texel_ptr + mask->pitch)) & (1 << idx) : 0xFF;
	tex[3] = idx == 0 ? (x < mask->width ? (*(u8*)(texel_ptr + mask->pitch + 1)) & (1 << 7) : 0xFF) : (y < mask->height ? (*(u8*)(texel_ptr + mask->pitch)) & (1 << (idx - 1)) : 0xFF);

	mask_bilinear_sample sample;
	for (int i = 0; i < 4; i++)
		sample.sample[i] = tex[i];

	return sample;
}

u8 sample_mask(img* mask, i32 x, i32 y) {
	u8 idx = 7 - (x % 8);
	x /= 8;
	u8* texel_ptr = ((u8*)mask->mem + y * mask->pitch + x * 1);
	u8 tex = (*(u8*)texel_ptr) & (1 << idx);
	return tex;
}

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
	//TODO(fran): use something with alpha (png?) for rendering masks https://stackoverflow.com/questions/1505586/gdi-using-drawimage-to-draw-a-transperancy-mask-of-the-source-image

	//NOTE: the caller is in charge of handling the hbitmap, including deletion
	//NOTE: we add an extra limitation, no offsets, we scale the entire bitmap, at least for now to make it simpler to write
	HBITMAP scale_mask(HBITMAP mask_bmp, int destW, int destH) {

		BITMAP masknfo; GetObject(mask_bmp, sizeof(masknfo), &masknfo); Assert(masknfo.bmBitsPixel == 1);
		img mask;
		mask.width = masknfo.bmWidth;
		mask.height = masknfo.bmHeight;
		mask.bits_per_pixel = 1;
		mask.pitch = masknfo.bmWidthBytes;
		int mask_sz = mask.height * mask.pitch;
		mask.mem = malloc(mask_sz); defer{ free(mask.mem); };

		int getbits = GetBitmapBits(mask_bmp, mask_sz, mask.mem);
		Assert(getbits == mask_sz);

		img scaled_mask;
		scaled_mask.width = destW;
		scaled_mask.height = destH;
		scaled_mask.bits_per_pixel = 1;
		scaled_mask.pitch = round2up((i32)roundf(((f32)scaled_mask.width * (1.f / 8.f/*bytes per pixel*/))));//NOTE: windows expects word aligned bmps
		int scaled_mask_sz = scaled_mask.height * scaled_mask.pitch;
		scaled_mask.mem = (u8*)malloc(scaled_mask_sz); defer{ free(scaled_mask.mem); };


		v2 origin{ 0,0 };
		v2 x_axis{ (f32)scaled_mask.width,0 };
		v2 y_axis{ 0,(f32)scaled_mask.height };

		i32 width_max = scaled_mask.width - 1;
		i32 height_max = scaled_mask.height - 1;

		i32 x_min = scaled_mask.width;
		i32 x_max = 0;
		i32 y_min = scaled_mask.height;
		i32 y_max = 0;

		v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
		for (v2 p : points) {
			i32 floorx = (i32)floorf(p.x);
			i32 ceilx = (i32)ceilf(p.x);
			i32 floory = (i32)floorf(p.y);
			i32 ceily = (i32)ceilf(p.y);

			if (x_min > floorx)x_min = floorx;
			if (y_min > floory)y_min = floory;
			if (x_max < ceilx)x_max = ceilx;
			if (y_max < ceily)y_max = ceily;
		}

		if (x_min < 0)x_min = 0;
		if (y_min < 0)y_min = 0;
		if (x_max > scaled_mask.width) x_max = scaled_mask.width;
		if (y_max > scaled_mask.height)y_max = scaled_mask.height;

		u8* row = (u8*)scaled_mask.mem /*+(x_min *(i32)((f32)scaled_mask.bits_per_pixel/8.f))*/ + y_min * scaled_mask.pitch;

		for (int y = y_min; y < y_max; y++) {//n bits high
			u8* pixel = row;
			u8 index = 7;
			u8 pixels_to_write = 0;
			for (int x = x_min; x < x_max; x++) {//n bits wide

				f32 u = (f32)x / (f32)(scaled_mask.width - 1);
				f32 v = (f32)y / (f32)(scaled_mask.height - 1);

				f32 t_x = u * (f32)(mask.width - 1);
				f32 t_y = v * (f32)(mask.height - 1);

				i32 i_x = (i32)t_x;
				i32 i_y = (i32)t_y;

				f32 f_x = t_x - (f32)i_x;
				f32 f_y = t_y - (f32)i_y;

				Assert(i_x >= 0 && i_x < mask.width);
				Assert(i_y >= 0 && i_y < mask.height);

				//Bilinear filtering
				mask_bilinear_sample sample = sample_bilinear_mask(&mask, i_x, i_y);
				v4 f_sample;
				//TODO(fran): lerp and f_x f_y
				for (int i = 0; i < 4; i++) {
					f_sample.comp[i] = (sample.sample[i] ? 1.f : 0.f);
				}

				u8 texel = (u8)roundf(lerp(lerp(f_sample.x, f_x, f_sample.y), f_y, lerp(f_sample.z, f_x, f_sample.w)));

				pixels_to_write |= (texel << index);

				index--;
				if (index == (u8)-1 || (x + 1) == x_max) {
					index = 7;
					//pixels_to_write = ~pixels_to_write;
					*pixel++ = pixels_to_write;
					pixels_to_write = 0;
				}
			}
			row += scaled_mask.pitch;
		}

		//Create bitmap
		HBITMAP new_mask = CreateBitmap(scaled_mask.width, scaled_mask.height, 1, scaled_mask.bits_per_pixel, scaled_mask.mem);
		//TODO(fran): do I need to set the palette?
		return new_mask;
	}

	void draw_mask(HDC dest, int xDest, int yDest, int wDest, int hDest, HBITMAP mask, int xSrc, int ySrc, int wSrc, int hSrc, HBRUSH colorbr) {
		{BITMAP bmpnfo; GetObject(mask, sizeof(bmpnfo), &bmpnfo); Assert(bmpnfo.bmBitsPixel == 1); }

		HBITMAP scaled_mask = (wDest != wSrc || hDest != hSrc) ? scale_mask(mask, wDest, hDest) : mask; //NOTE: beware of applying any modification to scaled_mask, you could be modifying the original mask
		BITMAP masknfo; GetObject(scaled_mask, sizeof(masknfo), &masknfo); Assert(masknfo.bmBitsPixel == 1);
		img mask_img;
		mask_img.width = masknfo.bmWidth;
		mask_img.height = masknfo.bmHeight;
		mask_img.bits_per_pixel = 1;
		mask_img.pitch = masknfo.bmWidthBytes;
		int mask_sz = mask_img.height * mask_img.pitch;
		mask_img.mem = malloc(mask_sz); defer{ free(mask_img.mem); };
		int getbits = GetBitmapBits(scaled_mask, mask_sz, mask_img.mem); Assert(getbits == mask_sz);

		for (int y = yDest; y < yDest + hDest; y++) {
			for (int x = xDest; x < xDest + wDest; x++)
			{
				u8 sample = sample_mask(&mask_img, x - xDest, y - yDest);
				if (!sample) SetPixel(dest, x, y, ColorFromBrush(colorbr));
			}
		}
		if (scaled_mask != mask) DeleteObject(scaled_mask);
	}

	void draw_icon(HDC dest, int xDest, int yDest, int wDest, int hDest, HICON icon, int xSrc, int ySrc, int wSrc, int hSrc) {
		Gdiplus::Graphics graphics(dest);//Yeah, who cares, icons are small, I just want bilinear filtering
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
		Gdiplus::Bitmap bmp(icon);
		//TODO?(fran): with gdi+ we can do subpixel placement (RectF)
		graphics.DrawImage(&bmp,
			Gdiplus::Rect(xDest, yDest, wDest, hDest),
			xSrc, ySrc, wSrc, hSrc,
			Gdiplus::Unit::UnitPixel);//TODO(fran): get the current unit from the device
	}

	//NOTE: specific procedure for drawing images on windows' menus, other drawing functions may fail on specific situations
	//NOTE: this is set up to work with masks that have black as the color and white as transparency, otherwise you'll need to flip your colors, for example BitBlt(arrowDC, 0, 0, arrowW, arrowH, arrowDC, 0, 0, DSTINVERT)
	void draw_menu_mask(HDC destDC, int xDest, int yDest, int wDest, int hDest, HBITMAP mask, int xSrc, int ySrc, int wSrc, int hSrc, HBRUSH colorbr)
	{
		{BITMAP bmpnfo; GetObject(mask, sizeof(bmpnfo), &bmpnfo); Assert(bmpnfo.bmBitsPixel == 1); }

		//NOTE: I dont know if this will be final, dont really wanna depend on windows, but it's pretty good for now. see https://docs.microsoft.com/en-us/windows/win32/gdi/scaling-an-image maybe some of those stretch modes work better than HALFTONE

		//Create the DCs and Bitmaps we will need
		HDC maskDC = CreateCompatibleDC(destDC);
		HBITMAP fillBitmap = CreateCompatibleBitmap(destDC, wDest, hDest);
		HBITMAP oldFillBitmap = (HBITMAP)SelectObject(maskDC, fillBitmap);

		//Blit the content already stored in dest onto a secondary buffer
		BitBlt(maskDC, 0, 0, wDest, hDest, destDC, xDest, yDest, SRCCOPY);

		//Draw on the new buffer, this is neeeded cause draw_mask uses MaskBlt which sometimes fails on menus 
		//Explanation: I think I know what the problem is, draw_mask doesnt fail on the first submenus because the menu window is already being shown/exists, but for submenus of those guys the window is not yet shown when we draw onto it, this problaly confuses MaskBlt in some strange way
		urender::draw_mask(maskDC, 0, 0, wDest, hDest, mask, 0, 0, wDest, hDest, colorbr);

		//Draw back to dest with the new content
		BitBlt(destDC, xDest, yDest, wDest, hDest, maskDC, 0, 0, SRCCOPY);

		//Clean up
		SelectObject(maskDC, oldFillBitmap);
		DeleteObject(fillBitmap);
		DeleteDC(maskDC);
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
		Gdiplus::GraphicsPath path;
		GetRoundRectPath(&path, r, diameter);
		graphics.DrawPath(&pen, &path);
	}

	//TODO: review why I have this function using gdiplus while the function on top using stretchdibits, does the other one not work?
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
		auto w = RECTW(r), h = RECTH(r);
		auto min_dim = minimum(w, h);
		auto borderSize = dimensions.border_thickness;
		if (dimensions.border_radius.value) {
			auto radius = dimensions.border_radius.to_px(min_dim);
			urender::draw_round_rectangle(dc, r, radius, bk);
			if (borderSize) urender::draw_round_rectangle_outline(dc, r, radius, border, borderSize);
		}
		else {
			if (borderSize) {
				HPEN pen = CreatePen(PS_SOLID, borderSize, ColorFromBrush(border));
				auto oldpen = SelectPen(dc, pen); defer{ SelectPen(dc, oldpen); DeletePen(pen); };
				auto oldbr = SelectBrush(dc, bk); defer{ SelectBrush(dc, oldbr); };

				Rectangle(dc, r.left, r.top, r.right, r.bottom); //uses pen for border and brush for bk
			}
			else {
				FillRect(dc, &r, bk);
			}
		}
	}

	enum class txt_align { center, left, right };

	//Intended for a single line of text
	void draw_text(HDC dc, const RECT& r, utf16_str txt, HFONT f, HBRUSH txt_br, txt_align alignment, i32 pad_x) {
		HFONT oldfont = SelectFont(dc, f); defer{ SelectFont(dc, oldfont); };
		UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };

		COLORREF oldtxtcol = SetTextColor(dc, ColorFromBrush(txt_br)); defer{ SetTextColor(dc, oldtxtcol); };
		auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };

		TEXTMETRIC tm; GetTextMetrics(dc, &tm);
		// Calculate vertical position for the string so that it will be vertically centered
		// We are single line so we want vertical alignment always
		i32 yPos = (r.bottom + r.top - tm.tmHeight) / 2;
		i32 xPos;
		switch (alignment) {
		case decltype(alignment)::center:
		{
			SetTextAlign(dc, TA_CENTER);
			xPos = (r.right - r.left) / 2;
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

		//TODO(fran): TabbedTextOut for completeness
		//TODO(fran): transparent bk color (if possible without gdi+)
	}

	i32 draw_bitmap_1bpp_right(HBITMAP bmp, HDC dc, rect_i32 r, int x_pad, HBRUSH color) {
		//TODO(fran): flicker free
		BITMAP bitmap; GetObject(bmp, sizeof(bitmap), &bitmap);
		Assert(bitmap.bmBitsPixel == 1);
		int max_sz = roundNdown(bitmap.bmWidth, (int)((f32)r.h * .6f)); //HACK: instead use png + gdi+ + color matrices
		if (!max_sz)max_sz = bitmap.bmWidth; //More HACKs

		int bmp_height = max_sz;
		int bmp_width = bmp_height;
		int bmp_align_width = r.left + r.w - bmp_width - x_pad;
		int bmp_align_height = r.top + (r.h - bmp_height) / 2;
		draw_mask(dc, bmp_align_width, bmp_align_height, bmp_width, bmp_height, bmp, 0, 0, bitmap.bmWidth, bitmap.bmHeight, color);

		return bmp_align_width;
	}


	#ifdef USE_GDIPLUS
	struct pre_post_main {
		ULONG_PTR gdiplusToken;

		pre_post_main() {
			// Initialize GDI+
			Gdiplus::GdiplusStartupInput gdiplusStartupInput;
			Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nil);
		}
		~pre_post_main() { 
			//Uninitialize GDI+
			Gdiplus::GdiplusShutdown(gdiplusToken);
		}
	} static const PREMAIN_POSTMAIN;
	#endif
}