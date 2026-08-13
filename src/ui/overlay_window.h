#pragma once

#include <windows.h>

namespace peek {

class OverlayWindow {
public:
    bool Create(HINSTANCE instance);
    void Show(const RECT& bounds);
    void Hide();
    [[nodiscard]] HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    void ApplyHollowRegion(int width, int height);
    HWND hwnd_{};
};

} // namespace peek
