#pragma once
#include "platform.h"
#include "vector.h"
#include "rect.h"
#include <cmath>

//f32

static f32 dot(v2 a, v2 b) {
	f32 res = a.x * b.x + a.y * b.y;
	return res;
}

static f32 length_sq(v2 v) {
	f32 res;
	res = dot(v, v);
	return res;
}

static f32 length(v2 v) {
	f32 res = sqrtf(length_sq(v));
	return res;
}


//i32

static i32 roundNup(i32 multiple, i32 value) {
	Assert(multiple > 0);
	i64 rounded = ((i64)value + (value >= 0 ? (i64)multiple - 1 : 0)) / multiple * multiple;
	return rounded > I32MAX ? I32MAX : (i32)rounded;
}

static i32 round2up(i32 value) {
	return roundNup(2, value);
}

static i32 roundNdown(i32 multiple, i32 value) {
	Assert(multiple > 0);
	i64 rounded = ((i64)value - (value < 0 ? (i64)multiple - 1 : 0)) / multiple * multiple;
	return rounded < I32MIN ? I32MIN : (i32)rounded;
}

//u32

static u32 safe_u64_to_u32(u64 n) {
	Assert(n <= 0xFFFFFFFF);
	return (u32)n;
}


//templates

template <typename T>
requires (std::is_arithmetic_v<T> && !std::same_as<T, bool>)
static T lerp(T from, f32 t, T to) {
	if constexpr (std::floating_point<T>) {
		//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
		return (T(1) - T(t)) * from + T(t) * to;
	}
	else {
		using Float = std::conditional_t<(sizeof(T) <= sizeof(i32)), f32, f64>;
		return (T)std::round(lerp((Float)from, t, (Float)to));
	}
}

static RECT lerp(RECT from, f32 t, RECT to) {
	return { lerp(from.left, t, to.left), lerp(from.top, t, to.top), lerp(from.right, t, to.right), lerp(from.bottom, t, to.bottom) };
}

template <typename T, std::convertible_to<T> U>
static T minimum(T a, U b) {
	return (a < b) ? a : b;
}

template <typename T, std::convertible_to<T> U>
static T maximum(T a, U b) {
	return (a > b) ? a : b;
}

template <typename T>
static T signum(T val) {
	return (T)((T(0) < val) - (val < T(0)));
}

template<std::unsigned_integral T, std::convertible_to<T> U>
static T safe_add(T a, U b) {
	Assert(b > 0);
	constexpr T max_value = std::numeric_limits<T>::max();
	T res = (a > max_value - b) ? max_value : a + b;
	return res;
}

//returns a - b if a >= b otherwise returns n
template <typename T, std::convertible_to<T> U>
static T safe_subtractN(T a, U b, T n) {
	Assert(b >= (decltype(a))0);
	T res;
	if (a >= b) res = a - b;
	else res = n;
	return res;
}

template <typename T, std::convertible_to<T> U>
static T safe_subtract0(T a, U b) {
	return safe_subtractN(a, b, (decltype(a))0);
}

template <typename T>
static T safe_ratioN(T dividend, T divisor, T n) {
	T res;
	if (divisor != (T)0) res = dividend / divisor;
	else res = n;
	return res;
}

template <typename T>
static T safe_ratio1(T dividend, T divisor) {
	return safe_ratioN(dividend, divisor, (T)1);
}

template <typename T>
static T safe_ratio0(T dividend, T divisor) {
	return safe_ratioN(dividend, divisor, (T)0);
}

template <typename T>
static T clamp(T min, T n, T max) { //clamps between [min,max]
	T res = n;
	if (res < min) res = min;
	elif (res > max) res = max;
	return res;
}

template <typename T>
static T clamp01(T n) {
	return clamp((T)0, n, (T)1);
}

template <typename T>
static T distance(T a, T b) {
	T res;
	if constexpr (std::is_signed_v<T>) res = abs(a - b);
	else res = (a > b) ? a - b : b - a;
	return res;
}

template <typename T>
static T squared(T a) {
	T res = a * a;
	return res;
}

template <typename T>
static T cubed(T a) {
	T res = a * a * a;
	return res;
}
