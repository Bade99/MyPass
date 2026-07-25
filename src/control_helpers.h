#pragma once

/*
 * This helpers have knowledge of all the controls in the app (it should be included after all [control]_types.h headers)
 */

static HWND get_dialog_root(HWND wnd) {
	HWND root = GetAncestor(wnd, GA_ROOT);

	if (nonclient::wndclass_atom && nonclient::wndclass_atom == (ATOM)GetClassWord(root, GCW_ATOM))
		if (auto& nc = *nonclient::get_state(root); nc.client)
			root = nc.client;

	// TODO(fran): create the concept of work regions, you can mark any hwnd as a work region (should be placed in my common window header)
	// This will then change to, go up the parent hierarchy till you find an hwnd tagged as work region, use that as the root, if otherwise you find the nonclient window of the desktop window, you go back one layer of the hierarchy and use that one.
    #ifdef _DEBUG
    cstr name[50]; GetClassName(root, name, ARRAYSIZE(name)); wprintf(L"root name: %s\n", name);
    #endif

	return root;
}

void ensure_close_windows(HWND login, HWND editor) {
    /**
     * When a window is closed we destroy it and send the quit message to the app.
     * That does not mean that other open windows are automatically destroyed,
     * so we do it manually to allow them to exit gracefully.
     */
    constexpr auto& nonclient = nonclient::wndclass;
    constexpr auto sz = ARRAYSIZE(nonclient);
    cstr test_class[sz];
    for (auto w : { login, editor })
        if (IsWindow(w) && GetClassName(w, &test_class[0], sz) && !wcsncmp(test_class, nonclient, sz))
            DestroyWindow(w);
}