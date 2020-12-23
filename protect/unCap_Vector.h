#pragma once
#include "unCap_Platform.h"
//NOTE: by default the vectors are to represent f32 values

//V2 (F32)
struct v2 {
	union {
		struct {
			f32 x, y;
		};
		struct {
			f32 u, v;
		};
		f32 comp[2];
	};

	v2& operator+=(const v2& rhs);

	v2& operator-=(const v2& rhs);

	v2& operator*=(f32 rhs_scalar);
};

v2 operator*(v2 v, f32 scalar) {
	v2 res;
	res.x = v.x * scalar;
	res.y = v.y * scalar;
	return res;
}

v2 operator*(f32 scalar, v2 v) { //NOTE: waiting for C++20 where I think they let you define the operation only one way
	v2 res = v * scalar;
	return res;
}

v2 operator/(v2 v, f32 scalar) {
	v2 res;
	res.x = v.x / scalar;
	res.y = v.y / scalar;
	return res;
}

v2 operator-(v2 a, v2 b) {
	v2 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	return res;
}

v2 operator+(v2 a, v2 b) {
	v2 res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	return res;
}

v2 operator-(v2 v) {
	v2 res;
	res.x = -v.x;
	res.y = -v.y;
	return res;
}

v2& v2::operator-=(const v2& rhs) {
	*this = *this - rhs; //NOTE: remember to declare + - * ... before += -=, otherwise it will not find the functions
	return *this;
}

v2& v2::operator+=(const v2& rhs) { //TODO(fran): is it necessary to use const& instead of a simple copy?
	*this = *this + rhs;
	return *this;
}

v2& v2::operator*=(f32 rhs_scalar) {
	*this = *this * rhs_scalar;
	return *this;
}

v2 v2_from_i32(i32 x, i32 y) {
	v2 res;
	res.x = (f32)x;
	res.y = (f32)y;
	return res;
}

//V2_I32
struct v2_i32 {
	i32 x, y;
};

//V3 (F32)
struct v3 {
	union {
		struct {
			f32 x, y, z;
		};
		struct {
			v2 xy;
			f32 z;
		};
		struct {
			f32 r, g, b;
		};
		f32 comp[3];
	};

	v3& operator+=(const v3& rhs);

	v3& operator-=(const v3& rhs);

	v3& operator*=(f32 rhs_scalar);
};

v3 V3(v2 v, f32 z) {
	v3 res;
	res.x = v.x;
	res.y = v.y;
	res.z = z;
	return res;
}

v3 operator/(v3 v, f32 scalar) {
	v3 res;
	res.x = v.x / scalar;
	res.y = v.y / scalar;
	res.z = v.z / scalar;
	return res;
}

v3 operator-(v3 v) {
	v3 res;
	res.x = -v.x;
	res.y = -v.y;
	res.z = -v.z;
	return res;
}

v3 operator*(v3 v, f32 scalar) {
	v3 res;
	res.x = v.x * scalar;
	res.y = v.y * scalar;
	res.z = v.z * scalar;
	return res;
}

v3 operator*(f32 scalar, v3 v) {
	v3 res = v * scalar;
	return res;
}

v3 operator-(v3 a, v3 b) {
	v3 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	res.z = a.z - b.z;
	return res;
}

v3 operator+(v3 a, v3 b) {
	v3 res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;
	return res;
}

v3& v3::operator-=(const v3& rhs) {
	*this = *this - rhs; //NOTE: remember to declare + - * ... before += -=, otherwise it will not find the functions
	return *this;
}

v3& v3::operator+=(const v3& rhs) { //TODO(fran): is it necessary to use const& instead of a simple copy?
	*this = *this + rhs;
	return *this;
}

v3& v3::operator*=(f32 rhs_scalar) {
	*this = *this * rhs_scalar;
	return *this;
}

//V3_I32
union v3_i32 {
	struct {
		i32 x, y, z;
	};
	struct {
		v2_i32 xy;
		i32 z;
	};
};

//V4 (F32)
union v4 {
	struct {
		f32 x, y, z, w;
	};
	struct {
		v3 xyz;
		f32 w;
	};
	struct {
		v2 xy;
		f32 z, w;
	};
	struct {
		f32 r, g, b, a;
	};
	struct {
		v3 rgb;
		f32 a;
	};
	f32 comp[4];

	v4& operator*=(f32 rhs_scalar);
};

v4 operator*(v4 v, f32 scalar) {
	v4 res;
	res.x = v.x * scalar;
	res.y = v.y * scalar;
	res.z = v.z * scalar;
	res.w = v.w * scalar;
	return res;
}

v4 operator*(f32 scalar, v4 v) { //NOTE: waiting for C++20 where I think they let you define the operation only one way
	v4 res = v * scalar;
	return res;
}

v4 operator+(v4 a, v4 b) {
	v4 res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;
	res.w = a.w + b.w;
	return res;
}

v4& v4::operator*=(f32 rhs_scalar) {
	*this = *this * rhs_scalar;
	return *this;
}

v4 operator/(v4 v, f32 scalar) {
	v4 res;
	res.x = v.x / scalar;
	res.y = v.y / scalar;
	res.z = v.z / scalar;
	res.w = v.w / scalar;
	return res;
}
