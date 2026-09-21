#pragma once

struct _string_traversal { size_t p; bool reached_limit; }; //NOTE: string::npos seems worse since it doesnt give you a valid last pos

static _string_traversal skip_whitespace(utf16_str s, size_t start_p, int direction) {
	size_t last_valid_i;
	for (size_t i = start_p; i < s.sz_char(); i += direction) {
		last_valid_i = i;
		if (!iswspace(s[i])) {
			return { i,false };
		}
	}
	return { last_valid_i,true };
}

//goes to first character in word or punctuation group (skips whitespaces)
//TODO(fran): what to do if we're already at the start of a group?
static _string_traversal goto_start_of_group(utf16_str s, size_t start_p, int direction) {
	Assert(direction < 0);

	auto [x, reached_limit] = skip_whitespace(s, start_p, direction);
	if (reached_limit) return { x, reached_limit };

	bool is_punct = iswpunct(s[x]);//can either be punctuation or alphanumeric

	//go to the start of the previous word or punctuation 'group'
	size_t last_valid_j;
	for (size_t j = x; j < s.sz_char(); j += direction) {
		if ((is_punct ? !iswpunct(s[j]) : !iswalnum(s[j]))) return { j + 1,false };
		last_valid_j = j;
	}
	return { last_valid_j,true };
}

//goes one past the last character in word or punctuation group (skips whitespaces)
//TODO(fran): what to do if we're already at the end of a group?
static _string_traversal goto_end_of_group(utf16_str s, size_t start_p, int direction) {
	Assert(direction > 0);

	auto [x, reached_limit] = skip_whitespace(s, start_p, direction);
	if (reached_limit) return { x, reached_limit };

	bool is_punct = iswpunct(s[x]);//can either be punctuation or alphanumeric

	//go to the end of the word or punctuation 'group'
	size_t last_valid_j;
	for (size_t j = x; j < s.sz_char(); j += direction) {
		if ((is_punct ? !iswpunct(s[j]) : !iswalnum(s[j]))) return { j,false };
		last_valid_j = j;
	}
	return { last_valid_j,true };
}

//finds different points where to stop when traversing a string, used for Ctrl+Left/Right Arrows keycombo
static size_t find_stopper(utf16_str s, size_t start_p, int direction/*should be +1 or -1*/) {
	Assert(direction);
	//TODO(fran): handle overflow

	//TODO(fran): this doesnt work exactly like we'd like for Ctrl+ Right arrow, we fail to skip to the next word instead stopping at the last character of the current one, and when we do (by placing the cursor past the last character of the word) we go past to the end of that next word instead of stopping at the beginning of it. Extra: Actually I do like that it stops at the end of words (this is not the normal behaviour but I like it more), what is wrong is the second case, whereby starting from a whitespace & going right it skips to the end of the next word instead of stopping at the beginning

	start_p = clamp((decltype(start_p))0, start_p, s.cnt());

	if (iswspace(s[start_p]) || iswcntrl(s[start_p])) {//we're on a whitespace
		//find first non whitespace in the direction
		size_t last_valid_i;
		for (size_t i = start_p; i < s.sz_char(); i += direction) {
			last_valid_i = i;
			if (!iswspace(s[i]) && !iswcntrl(s[i])) {
				//found a non whitespace char, now go to the beginning/end of that new thing

				bool is_punct = iswpunct(s[i]);//can either be punctuation or alphanumeric

				size_t last_valid_j;
				for (size_t j = i; j < s.sz_char(); j += direction) {
					if ((is_punct ? !iswpunct(s[j]) : !iswalnum(s[j]))) return direction >= 0 ? j : j + 1;
					last_valid_j = j;
				}

				return last_valid_j;
			}
		}
		return last_valid_i;
	}
	else {

		if (iswpunct(s[start_p])) {//we're on a punctuation mark
			//TODO(fran):it seems like everybody does smth different with punctuation, look at visual studio & sublime for examples, so just find what I feel works best

			if (size_t i = start_p; direction < 0 && !iswpunct(s[--i])) {//if going left and we're on the first character of the punctuation group

				//go to first character in previous word or punctuation group
				auto [x, _] = goto_start_of_group(s, i, direction);
				return x;
			}
			else {
				//find first non punctuation in the direction
				for (size_t i = start_p; i < s.sz_char(); i += direction) {
					if (!iswpunct(s[i])) {
						return i;
					}
					//TODO(fran): same fix as in whitespace
				}
			}
		}
		else {//we're on a word
			//find first non word in the direction 
			//TODO(fran): what about langs that dont usually separate words, like japanese? looks pretty hard since you'd actually have to comprehend the text to understand where to cut each word

			if (direction > 0) {//if going right
				for (size_t i = start_p; i < s.sz_char(); i += direction) {
					if (!iswalnum(s[i])) {//find first character not in the current word
						return i;
					}
					//TODO(fran): same fix as in whitespace
				}
			}
			else {//if going left

				if (size_t i = start_p; i > 0 && !iswalnum(s[--i])) {//if we're at the first character of the word

					//we go one back (--i)

					if (iswpunct(s[i])) {//if we're on punctuation
						//go to the start of the punctuation 'group' //TODO(fran): make this things into separate functions for reuse
						size_t last_valid_j;
						for (size_t j = i; j < s.sz_char(); j += direction) {
							if (!iswpunct(s[j])) return j + 1;
							last_valid_j = j;
						}
						return last_valid_j;
					}
					else {//else we're on a whitespace
						//skip all whitespaces and go to the start of the previous word or punctuation 'group'
						auto [x, _] = goto_start_of_group(s, i, direction);
						return x;
					}

				}
				else {//else find first character of the word

					size_t last_valid_i = 0;
					for (size_t i = start_p; i < s.sz_char(); i += direction) {
						last_valid_i = i;
						if (!iswalnum(s[i])) {//find first character not in the current word
							return i + 1;
						}
					}
					return last_valid_i;//if for example we get to the start of the string then stop there
				}
			}
		}


	}

	return start_p;
}

static size_t find_next_stopper(utf16_str s, size_t start_p) { return find_stopper(s, start_p, +1); }

static size_t find_prev_stopper(utf16_str s, size_t start_p) { return find_stopper(s, start_p, -1); }