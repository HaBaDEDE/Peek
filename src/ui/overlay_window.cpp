#include "ui/overlay_window.h"

namespace peek {

bool OverlayWindow::Create(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"Peek.FocusOverlay";
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 220, LWA_ALPHA);
    return true;
}

void OverlayWindow::ApplyHollowRegion(int width, int height) {
    constexpr int border = 3;
    HRGN region = CreateRectRgn(0, 0, width, border);
    HRGN part = CreateRectRgn(0, height - border, width, height);
    CombineRgn(region, region, part, RGN_OR); DeleteObject(part);
    part = CreateRectRgn(0, border, border, height - border);
    CombineRgn(region, region, part, RGN_OR); DeleteObject(part);
    part = CreateRectRgn(width - border, border, width, height - border);
    CombineRgn(region, region, part, RGN_OR); DeleteObject(part);
    if (!SetWindowRgn(hwnd_, region, TRUE)) DeleteObject(region);
}

void OverlayWindow::Show(const RECT& bounds) {
    if (!hwnd_ || IsRectEmpty(&bounds)) return;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    ApplyHollowRegion(width, height);
    SetWindowPos(hwnd_, HWND_TOPMOST, bounds.left, bounds.top, width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void OverlayWindow::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    switch (message) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT rect{}; GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(RGB(57, 133, 255));
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace peek
