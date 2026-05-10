#pragma once

namespace search {
	constexpr auto& wndclass = wndclass_name("search");

	//New msgs that this wnd can handle
	#define search_base_msg_addr (WM_USER+300)
	#define SRH_AUTORESIZE (search_base_msg_addr+1)

	#ifdef _UNICODE
	#define _EM_FINDTEXT EM_FINDTEXTW
	#define _EM_FINDTEXTEX EM_FINDTEXTEXW
	#else
	#define _EM_FINDTEXT EM_FINDTEXT
	#define _EM_FINDTEXTEX EM_FINDTEXTEX
	#endif

	/**
	  * Defines the type of control that will be searched through
	  */
	struct ParentType {
		static const u32 edit = 1 << 1;
		static const u32 richedit = 1 << 2;
		static const u32 custom = 1 << 3;
	};

	struct Placement
	{
		static const u32 left = 1 << 1;
		static const u32 right = 1 << 2;
		static const u32 top = 1 << 3;
		static const u32 bottom = 1 << 4;

		//NOTE: top and bottom create a wnd that extends the whole width of the parent control; left and right will occupy 25% of the width or less; they can be combined eg bottom | left
	};

	struct Flag { //TODO(fran): you might like for this flags to be stored for the next execution, we need a simpler way to allow anyone to save stuff, not just the main wnds
		static const u32 search_up = 1 << 1;	//if set then search up, otherwise search down
		static const u32 case_sensitive = 1 << 2;	//
		static const u32 whole_word = 1 << 3;	//
		static const u32 no_wrap = 1 << 4;	//if you didnt find searching up look at what was down and viceversa, this flag is negated cause the usual is you want wrap
		//TODO(fran): add 'Search in current selection'
	};

	struct Range {
		int min; //first position to search, eg to search from the start this would be 0
		int max; //last position to search, that position is included in the search, use -1 to search till the end
	};

	struct Theme {
		union {
			struct {
				brush_group bk, border;
			};
			brush_group all[2];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		struct {
			u32 border_thickness = U32MAX;
			UINumber border_radius{ .value = F32INFINITY };
		} dimensions;

		struct {
			/**
			  * Control is meant to be used as a persistent window, consequences:
			  * - wont hide itself
			  * - wont render the 'close' button
			  */
			u32 persistent : 2 = 2;
			u32 btn_case_sensitive_off : 2 = 2;
			u32 btn_whole_word_off : 2 = 2;
			u32 btn_wrap_off : 2 = 2;
			u32 btn_find_prev_off : 2 = 2;
		} styles;

		button::Theme btn_theme;
		edit_oneline::Theme txt_theme;

		bool copy_from(const Theme& src) {
			bool repaint = false;
			_theme_copy_all_brushes(src.brushes, this->brushes);

			_theme_copy_u32(src.dimensions.border_thickness, this->dimensions.border_thickness);
			_theme_copy_uinumber(src.dimensions.border_radius, this->dimensions.border_radius);

			_theme_copy_theme(src.btn_theme, this->btn_theme);
			_theme_copy_theme(src.txt_theme, this->txt_theme);

			//TODO: find a way to programatically iterate over all the styles so I dont have set them one by one manually
			_theme_copy_trool(src.styles.persistent, this->styles.persistent);
			_theme_copy_trool(src.styles.btn_case_sensitive_off, this->styles.btn_case_sensitive_off);
			_theme_copy_trool(src.styles.btn_whole_word_off, this->styles.btn_whole_word_off);
			_theme_copy_trool(src.styles.btn_wrap_off, this->styles.btn_wrap_off);
			_theme_copy_trool(src.styles.btn_find_prev_off, this->styles.btn_find_prev_off);

			return repaint;
		}
	};

	union Controls {
		struct {
			HWND btn_case_sensitive;
			HWND btn_whole_word;
			HWND btn_wrap;
			HWND edit_match;
			HWND btn_find_next;
			HWND btn_find_prev;
			HWND btn_close;
		};
		HWND all[7];
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};

	// match_len: does not include the null terminator
	typedef void(*func_onsearch)(utf16* match, i32 match_len, multiflag<Flag> search_flags, bool search_in_current_selection, void* user_data);

	union Functions {
		struct {
			func_onsearch on_search;
		};
		void* all[1]{ 0 };
	private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
	};


	struct State : WindowState {
		Controls controls;

		multiflag<Placement> placement_flags;
		u32 parent_type;
		multiflag<Flag> search_flags;

		//HBRUSH br_border, br_bk, br_fore, br_bkpush, br_bkmouseover, br_edit_bk, bkr_edit_txt, br_bkselected;//TODO(fran): no real need to store all, only the childs need most of them

		Theme theme;
		Functions functions;

		void* user_data;
	};

	/**
	  * Sent as a pointer in the last parameter of CreateWindow
	  */
	struct Init {
		multiflag<Placement> SearchPlacement_flags;
		multiflag<Flag> SearchFlag_flags;
		u32 parent_type;
	};
}