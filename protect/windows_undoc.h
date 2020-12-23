#pragma once

#define DCX_USESTYLE 0x00010000 /*Windows never disappoints with its crucial undocumented features, HDC dc = GetDCEx(hwnd, 0, DCX_WINDOW | DCX_USESTYLE);*/

#define WM_UAHDESTROYWINDOW 0x90
#define WM_UAHDRAWMENU 0x91
#define WM_UAHDRAWMENUITEM 0x92
#define WM_UAHINITMENU 0x93
#define WM_UAHMEASUREMENUITEM 0x94
#define WM_UAHNCPAINTMENUPOPUP 0x95
#define WM_UAHUPDATE 0x96
