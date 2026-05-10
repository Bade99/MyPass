#pragma once

//NOTE: no ASCII support, always utf16

//------------------"API"------------------:
//combobox::wndclass identifies the window class to be used when calling CreateWindow()

//combobox::insert_element()
//combobox::set_function_free_elements() to free the content of N elements in the listbox, this is called when we need to clear the listbox, for example when it's hidden or when the search options change
//combobox::set_function_on_selection_accepted() operation to perform when the user has confirmed a new selection
//combobox::set_function_render_listbox_element()
//combobox::set_function_measure_combobox()
//combobox::set_function_measure_listbox_element() //TODO(fran)
//combobox::set_function_render_combobox()
//combobox::set_user_extra() extra user-defined data to be sent on each function call

//IMPORTANT: everything related to drawing will be left to be decided by the user, we will store nothing at all

namespace combobox {
	constexpr auto& wndclass = wndclass_name("combobox");

	struct render_flags {
		bool isEnabled, onMouseover, onClicked, isListboxOpen;
		//INFO: additional hidden state, the user could have pressed the button and while still holding it pressed have moved the mouse outside it, in this case onMouseover==false and onClicked==true
	};

	//typedef void(*func_free_elements)(ptr<void*> elements, void* user_extra);
	typedef void(*func_on_selection_accepted)(void* element, void* user_extra);
	typedef void(*func_on_listbox_opening)(HWND combo, HWND listbox, void* user_extra);
	typedef void(*func_render_combobox)(HDC dc, rect_i32 r, render_flags flags, void* element, void* user_extra);
	//For Handling WM_DESIRED_SIZE
	typedef int(*func_desired_size_combobox)(SIZE* min, SIZE* max, HDC dc, void* element, void* user_extra);//NOTE: the dc is simply to be used for functions such as GetTextExtentPoint32 that need a dc


	struct State {
		HWND wnd;
		HWND parent;

		struct {
			HWND button;
			HWND listbox;
			//HWND button; //TODO(fran): replace manual handling of combobox to a button with custom rendering, the only thing is we would need to setfocus to the button and intercept vk_up and vk_down to scroll the cb when the listbox isnt open
		}controls;

		void* user_extra;

		//func_free_elements free_elements;
		func_render_combobox render_combobox;
		func_desired_size_combobox desired_size_combobox;
		func_on_selection_accepted on_selection_accepted;
		func_on_listbox_opening on_listbox_opening;

		struct {
			HHOOK hookmouseclick;
		}impl;

		bool reject_button;//do not open listbox on the next button press if the listbox is already open and the button is clicked
	};
}