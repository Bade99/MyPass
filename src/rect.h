#pragma once

//----------------------RECT_i32-----------------------:
union rect_i32 {
	struct { i32 x, y, w, h; };
	struct { i32 left, top, w, h; };
	struct { v2_i32 xy, wh; };

	i32 right() const { return left + w; }
	i32 bottom() const { return top + h; }
	i32 center_x() const { return left + w / 2; }
	i32 center_y() const { return top + h / 2; }
	rect_i32 cut_left(i32 w) {
		rect_i32 cut_section{ this->left, this->top, w, this->h };
		this->left += w;
		this->w -= w;
		return cut_section;
	}
	rect_i32 cut_right(i32 w) {
		this->w -= w;
		rect_i32 cut_section{ this->left + this->w, this->top, w, this->h };
		return cut_section;
	}
	rect_i32 cut_top(i32 h) {
		rect_i32 cut_section{ this->left, this->top, this->w, h };
		this->top += h;
		this->h -= h;
		return cut_section;
	}
	rect_i32 cut_bottom(i32 h) {
		this->h -= h;
		rect_i32 cut_section{ this->left, this->top + this->h, this->w, h };
		return cut_section;
	}

	RECT to_RECT() {
		RECT res;
		res.left = this->left;
		res.top = this->top;
		res.right = this->right();
		res.bottom = this->bottom();
		return res;
	}

	static rect_i32 create_from(RECT r) {
		rect_i32 res;
		res.left = r.left;
		res.top = r.top;
		res.w = abs(r.right - r.left);
		res.h = abs(r.bottom - r.top);

		return res;
	}
};
