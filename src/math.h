#pragma once
#include "platform.h"
#include "vector.h"
#include "rect.h"

//i32

static i32 roundNup(i32 N, i32 num) {
	//Thanks https://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number
	//TODO(fran): check to which side it rounds for negatives
	Assert(N);
	i32 isPositive = (i32)(num >= 0);
	i32 res = ((num + isPositive * (N - 1)) / N) * N;
	return res;
}

static i32 round2up(i32 n) {
	return roundNup(2, n);
}

static i32 roundNdown(i32 N, i32 num) {
	//Thanks https://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number
	//TODO(fran): doesnt work for negatives
	Assert(N);
	i32 res = num - (num % N);
	return res;
}


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

static f32 lerp(f32 n1, f32 t, f32 n2) {
	//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
	return (1.f - t) * n1 + t * n2;
}

// Ease in & out
static f32 ParametricBlend(f32 t /*[0.0,1.0]*/) {
	f32 sqt = t * t;
	return sqt / (2.0f * (sqt - t) + 1.0f);
}

// Ease in & out
static f32 smoothstep(f32 t) {
	return t * t * (3.0f - 2.0f * t);
}


//u32

static u32 safe_u64_to_u32(u64 n) {
	Assert(n <= 0xFFFFFFFF);
	return (u32)n;
}


//templates

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