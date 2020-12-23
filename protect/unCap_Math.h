#pragma once
#include "unCap_Platform.h"
#include "unCap_Vector.h"
#include "unCap_Helpers.h" //Only for testing

//i32
static i32 distance(i32 a, i32 b) {
	return abs(a - b);
}

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

static i32 safe_ratioN(i32 dividend, i32 divisor, i32 n) {
	i32 res;
	if (divisor != 0) {
		res = dividend / divisor;
	}
	else {
		res = n;
	}
	return res;
}

static i32 safe_ratio1(i32 dividend, i32 divisor) {
	return safe_ratioN(dividend, divisor, 1);
}

static i32 safe_ratio0(i32 dividend, i32 divisor) {
	return safe_ratioN(dividend, divisor, 0);
}

static i32 clamp(i32 min, i32 n, i32 max) { //clamps between [min,max]
	i32 res = n;
	if (res < min) res = min;
	else if (res > max) res = max;
	return res;
}

//f32
static f32 safe_ratioN(f32 dividend, f32 divisor, f32 n) {
	f32 res;
	if (divisor != 0.f) {
		res = dividend / divisor;
	}
	else {
		res = n;
	}
	return res;
}

static f32 safe_ratio1(f32 dividend, f32 divisor) {
	return safe_ratioN(dividend, divisor, 1.f);
}

static f32 safe_ratio0(f32 dividend, f32 divisor) {
	return safe_ratioN(dividend, divisor, 0.f);
}

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

f32 lerp(f32 n1, f32 t, f32 n2) {
	//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
	return (1.f - t) * n1 + t * n2;
}