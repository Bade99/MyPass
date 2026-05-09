#pragma once

static LRESULT handle_wm_setcursor(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam, HCURSOR cursor) {
	//DefWindowProc passes this to its parent to see if it wants to change the cursor settings, we'll make a decision, setting the mouse cursor, and halting proccessing so it stays like that
	//WM_SETCURSOR is sent after getting the result of WM_NCHITTEST, mouse is inside our window and mouse input is not being captured

	/* https://docs.microsoft.com/en-us/windows/win32/learnwin32/setting-the-cursor-image
		if we pass WM_SETCURSOR to DefWindowProc, the function uses the following algorithm to set the cursor image:
		1. If the window has a parent, forward the WM_SETCURSOR message to the parent to handle.
		2. Otherwise, if the window has a class cursor, set the cursor to the class cursor.
		3. If there is no class cursor, set the cursor to the arrow cursor.
	*/
	//NOTE: I think this is good enough for now
	if (cursor && LOWORD(lparam) == HTCLIENT) {
		SetCursor(cursor);
		return TRUE;
	}
	else return DefWindowProc(wnd, msg, wparam, lparam);
}

static LRESULT handle_wm_nchittest(HWND wnd, LPARAM lparam) {
	POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
	RECT r; GetWindowRect(wnd, &r);
	auto hittest = test_pt_rc(mouse, r) ? HTCLIENT : HTNOWHERE;
	return hittest;
}