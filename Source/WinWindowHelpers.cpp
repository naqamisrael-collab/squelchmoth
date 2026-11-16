// Squelchmoth - Windows window helpers (compiled on Windows only).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern "C" void sqmOwnWindow (void* hwnd, void* ownerHwnd)
{
    HWND h = (HWND) hwnd, owner = (HWND) ownerHwnd;
    if (h == nullptr || h == owner) return;
    if (owner == nullptr)
    {
        // release (before the main window goes away: Windows destroys owned windows with their owner)
        if (GetWindowLongPtrW (h, GWLP_HWNDPARENT) != 0) SetWindowLongPtrW (h, GWLP_HWNDPARENT, 0);
        return;
    }
    const LONG_PTR ex = GetWindowLongPtrW (h, GWL_EXSTYLE);
    const bool hadAppWindow = (ex & WS_EX_APPWINDOW) != 0;
    if (! hadAppWindow && (HWND) GetWindowLongPtrW (h, GWLP_HWNDPARENT) == owner) return;   // already done
    // the taskbar only re-reads a window's styles when it is shown, so hide / re-show a visible one
    const bool visible = IsWindowVisible (h) != FALSE;
    if (visible) ShowWindow (h, SW_HIDE);
    SetWindowLongPtrW (h, GWL_EXSTYLE, ex & ~(LONG_PTR) WS_EX_APPWINDOW);
    SetWindowLongPtrW (h, GWLP_HWNDPARENT, (LONG_PTR) owner);   // owned: never on the taskbar, minimises with its owner
    if (visible) ShowWindow (h, SW_SHOWNA);
}
