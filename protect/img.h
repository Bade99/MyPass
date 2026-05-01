#pragma once


//TODO(fran): this was not a great idea, it's simply easier to work with real bmp files that you can edit with existing tools like photoshop and just embed them as a resource


//INFO: for 1bpp bmps in windows 0 = foreground , 1 = background, so the part you want to be visible has to have a 0

#define lo16(x) ((u16)((x) & 0xffff))
#define hi16(x) ((u16)(((x) >> 16) & 0xffff))

#define bswap16(x) ((u16)((x>>8) | (x<<8)))
#define bswap32(x) ((u32)((((u32)bswap16(lo16(x)))<<16) | ((u32)bswap16(hi16(x)))))

//INFO: unoptimized, probably slow, only use at compile time!
template <typename T>
requires std::unsigned_integral<T>
constexpr T swap_first_last_all(T n)
{
	T res = 0;
	for (size_t i = 0; i < sizeof(n) * 8 / 2; i++) {
		//INFO: it seems like bit shift << and >> always upconvert to 32bit for lower bit variables, and if you want more bits, eg 64, you gotta ask for it explicitly, eg: (u64)1 << i
		//take lower bit and put it higher, eg for 64 bit unsigned: bit 0 -> bit 63, bit 1 -> bit 62
		res |= ((n & (static_cast<T>(1) << i)) << (sizeof(n) * 8 - 1 - i * 2));
		//take higher bit and put it lower, eg for 64 bit unsigned: bit 63 -> bit 0, bit 62 -> bit 1
		res |= ((n & (static_cast<T>(1) << (sizeof(n) * 8 / 2 + i))) >> (i * 2 + 1));
	}

	return res;
}

struct def_img {
	int bpp;
	int w;
	int h;
	void* mem;
};
#if 0
constexpr u16 empty[16] = //1bpp 16x16
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
#endif
constexpr u16 _close[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1110'1111'1111'0111),
	bswap16(0b1111'0111'1110'1111),
	bswap16(0b1111'1011'1101'1111),
	bswap16(0b1111'1101'1011'1111),
	bswap16(0b1111'1110'0111'1111),
	bswap16(0b1111'1110'0111'1111),
	bswap16(0b1111'1101'1011'1111),
	bswap16(0b1111'1011'1101'1111),
	bswap16(0b1111'0111'1110'1111),
	bswap16(0b1110'1111'1111'0111),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img close{ 1,16,16, (void*)_close };

constexpr u16 _maximize[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1100'0000'0000'0011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1100'0000'0000'0011),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img maximize{ 1,16,16, (void*)_maximize };

constexpr u16 _minimize[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1100'0000'0000'0011),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img minimize{ 1,16,16, (void*)_minimize };

constexpr u16 _tick[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'0011),
	bswap16(0b1111'1111'1110'0111),
	bswap16(0b1111'1111'1100'1111),
	bswap16(0b1111'1111'1001'1111),
	bswap16(0b1100'1111'0011'1111),
	bswap16(0b1110'0110'0111'1111),
	bswap16(0b1111'0000'1111'1111),
	bswap16(0b1111'1001'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img tick{ 1,16,16, (void*)_tick };

constexpr u16 _arrow_right[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1101'1111'1111),
	bswap16(0b1111'1100'1111'1111),
	bswap16(0b1111'1100'0111'1111),
	bswap16(0b1111'1100'0011'1111),
	bswap16(0b1111'1100'0001'1111),
	bswap16(0b1111'1100'0001'1111),
	bswap16(0b1111'1100'0011'1111),
	bswap16(0b1111'1100'0111'1111),
	bswap16(0b1111'1100'1111'1111),
	bswap16(0b1111'1101'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img arrow_right{ 1,16,16, (void*)_arrow_right };

constexpr u16 _dropdown[16] = //TODO(fran): dropdown_inverted
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1001'1111'1111'1001),
	bswap16(0b1100'1111'1111'0011),
	bswap16(0b1110'0111'1110'0111),
	bswap16(0b1111'0011'1100'1111),
	bswap16(0b1111'1001'1001'1111),
	bswap16(0b1111'1100'0011'1111),
	bswap16(0b1111'1110'0111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img dropdown{ 1,16,16, (void*)_dropdown };

constexpr u16 _circle[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1110'0111'1111),
	bswap16(0b1111'1000'0001'1111),
	bswap16(0b1111'0000'0000'1111),
	bswap16(0b1111'0000'0000'1111),
	bswap16(0b1110'0000'0000'0111),
	bswap16(0b1110'0000'0000'0111),
	bswap16(0b1111'0000'0000'1111),
	bswap16(0b1111'0000'0000'1111),
	bswap16(0b1111'1000'0001'1111),
	bswap16(0b1111'1110'0111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img circle{ 1,16,16, (void*)_circle };

constexpr u32 _bin[32] =
{
	bswap32(0b1111'1111'1111'1111'1111'1111'1111'1111),
	bswap32(0b1111'1111'1111'1111'1111'1111'1111'1111),
	bswap32(0b1111'1111'1111'0000'0000'1111'1111'1111),
	bswap32(0b1111'1111'1110'0000'0000'0111'1111'1111),
	bswap32(0b1111'1000'0000'0000'0000'0000'0001'1111),
	bswap32(0b1111'1000'0000'0000'0000'0000'0001'1111),
	bswap32(0b1111'1000'0000'0000'0000'0000'0001'1111),
	bswap32(0b1111'1111'1111'1111'1111'1111'1111'1111),
	bswap32(0b1111'1111'1111'1111'1111'1111'1111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1110'0000'0000'0000'0000'0111'1111),
	bswap32(0b1111'1111'0000'0000'0000'0000'1111'1111),
	bswap32(0b1111'1111'1000'0000'0000'0001'1111'1111),
	bswap32(0b1111'1111'1111'1111'1111'1111'1111'1111),
	bswap32(0b1111'1111'1111'1111'1111'1111'1111'1111),
};
constexpr def_img bin{ 1,32,32, (void*)_bin };

constexpr u16 _arrowLine_left[16] =
{
	//IMPORTANT INFO: because of some weird macro magic using bswap16(swap_first_last_all()) doesnt generate the top byte
	swap_first_last_all(bswap16(0b1111'1111'1111'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1111'1111)),
	swap_first_last_all(bswap16(0b1111'1110'1111'1111)),
	swap_first_last_all(bswap16(0b1111'1111'0111'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1011'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1101'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1110'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1111'0111)),
	swap_first_last_all(bswap16(0b1000'0000'0000'0011)),
	swap_first_last_all(bswap16(0b1111'1111'1111'0111)),
	swap_first_last_all(bswap16(0b1111'1111'1110'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1101'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1011'1111)),
	swap_first_last_all(bswap16(0b1111'1111'0111'1111)),
	swap_first_last_all(bswap16(0b1111'1110'1111'1111)),
	swap_first_last_all(bswap16(0b1111'1111'1111'1111)),
};
constexpr def_img arrowLine_left{ 1,16,16, (void*)_arrowLine_left };

constexpr u16 _arrowSimple_right[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1011'1111'1111),
	bswap16(0b1111'1101'1111'1111),
	bswap16(0b1111'1110'1111'1111),
	bswap16(0b1111'1111'0111'1111),
	bswap16(0b1111'1111'1011'1111),
	bswap16(0b1111'1111'1101'1111),
	bswap16(0b1111'1111'1110'1111),
	bswap16(0b1111'1111'1101'1111),
	bswap16(0b1111'1111'1011'1111),
	bswap16(0b1111'1111'0111'1111),
	bswap16(0b1111'1110'1111'1111),
	bswap16(0b1111'1101'1111'1111),
	bswap16(0b1111'1011'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img arrowSimple_right{ 1,16,16, (void*)_arrowSimple_right };

constexpr u16 _eye[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1000'0001'1111),
	bswap16(0b1110'0001'1000'0111),
	bswap16(0b1100'0011'0000'1011),
	bswap16(0b1011'0000'0000'1101),
	bswap16(0b0011'0000'0000'0110),
	bswap16(0b0111'0000'0000'1110),
	bswap16(0b0111'0000'0000'1110),
	bswap16(0b0111'1000'0001'1110),
	bswap16(0b1011'1110'0111'1101),
	bswap16(0b1101'1111'1111'1011),
	bswap16(0b1110'0111'1110'0111),
	bswap16(0b1111'1000'0001'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img eye{ 1,16,16, (void*)_eye };

constexpr u16 _threeLines[16] =
{
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1000'0000'0000'0001),
	bswap16(0b1000'0000'0000'0001),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1000'0000'0000'0001),
	bswap16(0b1000'0000'0000'0001),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1000'0000'0000'0001),
	bswap16(0b1000'0000'0000'0001),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
	bswap16(0b1111'1111'1111'1111),
};
constexpr def_img threeLines{ 1,16,16, (void*)_threeLines };

