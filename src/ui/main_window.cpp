#include "ui/main_window.h"

#include "core/privilege_helper.h"

#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>

namespace peek {
namespace {

constexpr COLORREF kBackground = RGB(247, 248, 250);
constexpr COLORREF kSurface = RGB(255, 255, 255);
constexpr COLORREF kText = RGB(28, 31, 36);
constexpr COLORREF kMuted = RGB(101, 108, 119);
constexpr COLORREF kAccent = RGB(48, 112, 232);
constexpr COLORREF kBorder = RGB(224, 227, 232);

constexpr int kMinimizeCloseButton = 2001;
constexpr int kExitCloseButton = 2002;
constexpr int kAskRadio = 2101;
constexpr int kMinimizeRadio = 2102;
constexpr int kExitRadio = 2103;

HRESULT ShowTaskDialog(const TASKDIALOGCONFIG* config, int* button, int* radio, BOOL* verification) {
    using TaskDialogIndirectFunction = HRESULT (WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
    HMODULE common_controls = LoadLibraryW(L"comctl32.dll");
    if (!common_controls) return HRESULT_FROM_WIN32(GetLastError());
    const auto function = reinterpret_cast<TaskDialogIndirectFunction>(
        GetProcAddress(common_controls, "TaskDialogIndirect"));
    if (!function) {
        const HRESULT error = HRESULT_FROM_WIN32(GetLastError());
        FreeLibrary(common_controls);
        return error;
    }
    const HRESULT result = function(config, button, radio, verification);
    FreeLibrary(common_controls);
    return result;
}

int S(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

RECT R(int left, int top, int right, int bottom, UINT dpi) {
    return {S(left, dpi), S(top, dpi), S(right, dpi), S(bottom, dpi)};
}

void Fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void Frame(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(dc, &rect, brush);
    DeleteObject(brush);
}

void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
    SelectObject(dc, old); DeleteObject(pen);
}

void Text(HDC dc, const std::wstring& text, RECT rect, COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) {
    SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
}

bool Inside(const RECT& rect, POINT point) { return PtInRect(&rect, point) != FALSE; }

std::wstring RectText(const RECT& rect) {
    return std::to_wstring(rect.left) + L", " + std::to_wstring(rect.top) + L"  ·  " +
        std::to_wstring(rect.right - rect.left) + L" × " + std::to_wstring(rect.bottom - rect.top);
}

} // namespace

MainWindow::MainWindow(EventStore& events, UnlockService& unlock, IconCache& icons)
    : events_(events), unlock_(unlock), icons_(icons) {}

MainWindow::~MainWindow() {
    RemoveTrayIcon();
    if (font_) DeleteObject(font_);
    if (font_bold_) DeleteObject(font_bold_);
}

bool MainWindow::Create(HINSTANCE instance, int show_command, std::wstring& error) {
    instance_ = instance;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"Peek.MainWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        error = Texts().main_class_error; return false;
    }

    dpi_ = GetDpiForSystem();
    UpdateFonts(dpi_);
    RECT size{0, 0, S(420, dpi_), S(540, dpi_)};
    AdjustWindowRectExForDpi(&size, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            FALSE, WS_EX_ACCEPTFILES, dpi_);
    hwnd_ = CreateWindowExW(WS_EX_ACCEPTFILES, wc.lpszClassName, L"Peek",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left, size.bottom - size.top,
        nullptr, nullptr, instance, this);
    if (!hwnd_) { error = Texts().main_window_error; return false; }

    taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
    const DWORD preference = 2; // DWMWCP_ROUND, ignored by Windows 10.
    DwmSetWindowAttribute(hwnd_, static_cast<DWORD>(33), &preference, sizeof(preference));
    DragAcceptFiles(hwnd_, TRUE);
    overlay_.Create(instance);
    AddTrayIcon();
    if (!RegisterHotKey(hwnd_, kGhostHotkey, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'G')) {
        error = Texts().ghost_hotkey_error;
    }
    RegisterHotKey(hwnd_, kFocusHotkey, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'F');
    ShowWindow(hwnd_, show_command == SW_HIDE ? SW_HIDE : SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

int MainWindow::Run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void MainWindow::NotifyGhost() {
    if (hwnd_) PostMessageW(hwnd_, kGhostChanged, 0, 0);
}

void MainWindow::DeliverFocus(FocusSnapshot snapshot) {
    if (!hwnd_) return;
    {
        std::lock_guard lock(pending_focus_mutex_);
        pending_focus_ = std::move(snapshot);
    }
    PostMessageW(hwnd_, kFocusChanged, 0, 0);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
        tray_added_ = false;
        AddTrayIcon();
        return 0;
    }
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd_, &paint); Paint(dc); EndPaint(hwnd_, &paint); return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_LBUTTONUP: OnClick({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}); return 0;
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wparam); UpdateFonts(dpi_);
            auto* rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd_, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_HOTKEY:
            if (wparam == kGhostHotkey) ShowMain(Tab::Ghost);
            else if (wparam == kFocusHotkey) { ShowMain(Tab::Focus); ToggleFocus(); }
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE && focus_ && focus_->Active()) { focus_->Stop(); KillTimer(hwnd_, 7); overlay_.Hide(); InvalidateRect(hwnd_, nullptr, FALSE); }
            return 0;
        case WM_TIMER:
            if (wparam == 7 && focus_ && focus_->Active() && (GetAsyncKeyState(VK_ESCAPE) & 1)) {
                focus_->Stop(); KillTimer(hwnd_, 7); overlay_.Hide(); InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_DROPFILES: {
            auto drop = reinterpret_cast<HDROP>(wparam);
            wchar_t path[32768]{};
            if (DragQueryFileW(drop, 0, path, static_cast<UINT>(std::size(path)))) QueryUnlock(path);
            DragFinish(drop); return 0;
        }
        case kGhostChanged:
            if (tab_ == Tab::Ghost) InvalidateRect(hwnd_, nullptr, FALSE); return 0;
        case kFocusChanged: {
            {
                std::lock_guard lock(pending_focus_mutex_);
                if (!pending_focus_) return 0;
                focus_snapshot_ = std::move(*pending_focus_);
                pending_focus_.reset();
            }
            has_focus_snapshot_ = true;
            RECT highlight = focus_snapshot_.automation.available ? focus_snapshot_.automation.bounds : focus_snapshot_.window.bounds;
            overlay_.Show(highlight);
            if (tab_ == Tab::Focus) InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        case kTrayMessage:
        {
            // NOTIFYICON_VERSION_4 stores the notification in LOWORD(lParam).
            const UINT tray_event = LOWORD(static_cast<DWORD_PTR>(lparam));
            if (tray_event == WM_LBUTTONUP || tray_event == WM_LBUTTONDBLCLK ||
                tray_event == NIN_SELECT || tray_event == NIN_KEYSELECT) {
                ShowMain(tab_);
            } else if (tray_event == WM_RBUTTONUP || tray_event == WM_CONTEXTMENU) {
                POINT p{};
                GetCursorPos(&p);
                ShowTrayMenu(p);
            }
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == kOpenCommand) ShowMain(Tab::Ghost);
            else if (LOWORD(wparam) == kExitCommand) DestroyWindow(hwnd_);
            else if (LOWORD(wparam) == kSettingsCommand) ShowSettings();
            else if (LOWORD(wparam) == kEnglishCommand) SetLanguage(Language::English);
            else if (LOWORD(wparam) == kChineseCommand) SetLanguage(Language::ChineseSimplified);
            return 0;
        case WM_CLOSE: HandleCloseRequest(); return 0;
        case WM_DESTROY:
            if (focus_ && focus_->Active()) focus_->Stop();
            overlay_.Hide(); RemoveTrayIcon();
            UnregisterHotKey(hwnd_, kGhostHotkey); UnregisterHotKey(hwnd_, kFocusHotkey);
            PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void MainWindow::Paint(HDC dc) {
    RECT client{}; GetClientRect(hwnd_, &client);
    Fill(dc, client, kBackground);
    PaintHeader(dc, client);
    RECT body{0, S(58, dpi_), client.right, client.bottom};
    if (tab_ == Tab::Ghost) PaintGhost(dc, body);
    else if (tab_ == Tab::Unlock) PaintUnlock(dc, body);
    else PaintFocus(dc, body);
}

void MainWindow::PaintHeader(HDC dc, const RECT& client) {
    RECT header{0, 0, client.right, S(58, dpi_)}; Fill(dc, header, kSurface);
    SelectObject(dc, font_bold_);
    const auto& strings = Texts();
    const wchar_t* labels[] = {strings.ghost_tab, strings.unlock_tab, strings.focus_tab};
    for (int i = 0; i < 3; ++i) {
        RECT tab = R(18 + i * 96, 0, 102 + i * 96, 58, dpi_);
        const bool active = static_cast<int>(tab_) == i;
        Text(dc, labels[i], tab, active ? kAccent : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (active) { RECT bar{tab.left + S(14, dpi_), tab.bottom - S(3, dpi_), tab.right - S(14, dpi_), tab.bottom}; Fill(dc, bar, kAccent); }
    }
    RECT language = R(326, 14, 402, 44, dpi_);
    Fill(dc, language, RGB(241, 244, 249));
    Text(dc, strings.language_switch, language, kAccent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    Line(dc, 0, header.bottom - 1, client.right, header.bottom - 1, kBorder);
}

void MainWindow::PaintGhost(HDC dc, const RECT& body) {
    SelectObject(dc, font_);
    auto items = events_.Snapshot();
    RECT intro = R(18, 70, 402, 106, dpi_);
    const auto& strings = Texts();
    Text(dc, items.empty() ? strings.ghost_waiting : strings.ghost_question, intro, kMuted);
    int y = 108;
    const std::size_t count = std::min<std::size_t>(items.size(), 8);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& event = items[i];
        const bool expanded = event.sequence == expanded_event_;
        const int height = expanded ? 126 : 58;
        if (S(y + height + 18, dpi_) > body.bottom) break;
        RECT card = R(18, y, 402, y + height, dpi_); Fill(dc, card, kSurface); Frame(dc, card, kBorder);
        HICON icon = icons_.Get(event.process.exe_path);
        DrawIconEx(dc, card.left + S(10,dpi_), card.top + S(10,dpi_), icon, S(20,dpi_), S(20,dpi_), 0, nullptr, DI_NORMAL);
        const std::wstring name = !event.process.name.empty() ? event.process.name : strings.unknown_process;
        SelectObject(dc, font_bold_);
        RECT name_rect{card.left + S(40, dpi_), card.top + S(5, dpi_), card.right - S(10, dpi_), card.top + S(28, dpi_)};
        Text(dc, name, name_rect, kText);
        SelectObject(dc, font_);
        std::wstring summary = FormatTime(event.timestamp) + L"  ·  " + KindLabel(event.kind);
        if (event.has_lifetime) summary += L"  ·  " + std::to_wstring(event.lifetime.count()) + L" ms";
        if (!event.process.parent_name.empty()) summary += L"  ·  " + std::wstring(strings.from) + L" " + event.process.parent_name;
        RECT detail{card.left + S(12, dpi_), card.top + S(28, dpi_), card.right - S(10, dpi_), card.top + S(51, dpi_)};
        Text(dc, summary, detail, kMuted);
        if (expanded) {
            RECT extra{card.left + S(12, dpi_), card.top + S(53, dpi_), card.right - S(10, dpi_), card.top + S(86, dpi_)};
            std::wstring full = L"PID " + std::to_wstring(event.process.pid) + L"  ·  " + Elide(event.process.exe_path, 52);
            if (!event.process.command_line.empty()) full += L"\n" + Elide(event.process.command_line, 62);
            Text(dc, full, extra, kMuted, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
            RECT reveal{card.left + S(12,dpi_), card.top + S(91,dpi_), card.left + S(178,dpi_), card.bottom - S(7,dpi_)};
            Fill(dc, reveal, RGB(231,234,239)); Text(dc, strings.open_location, reveal, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RECT inspect{card.left + S(188,dpi_), card.top + S(91,dpi_), card.right - S(12,dpi_), card.bottom - S(7,dpi_)};
            Fill(dc, inspect, RGB(232,239,252)); Text(dc, strings.enter_focus, inspect, kAccent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        y += height + 8;
    }
}

void MainWindow::PaintUnlock(HDC dc, const RECT&) {
    const auto& strings = Texts();
    SelectObject(dc, font_);
    RECT intro = R(18, 70, 402, 106, dpi_);
    Text(dc, strings.unlock_question, intro, kMuted);
    RECT drop = R(18, 108, 402, 168, dpi_); Fill(dc, drop, kSurface); Frame(dc, drop, kBorder);
    Text(dc, unlock_path_.empty() ? strings.drop_file : Elide(unlock_path_, 54), drop, unlock_path_.empty() ? kMuted : kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT choose = R(18, 178, 132, 212, dpi_); Fill(dc, choose, kAccent); SelectObject(dc, font_bold_); Text(dc, strings.choose_file, choose, RGB(255,255,255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, font_);
    if (!unlock_error_.empty()) { RECT err = R(144, 178, 402, 212, dpi_); Text(dc, LocalizeUnlockError(unlock_error_), err, RGB(181, 48, 48)); }
    else if (!unlock_path_.empty()) { RECT ok = R(144, 178, 402, 212, dpi_); Text(dc, lock_owners_.empty() ? strings.no_owner : std::to_wstring(lock_owners_.size()) + L" " + strings.owners_found, ok, kMuted); }

    int y = 224;
    for (std::size_t i = 0; i < std::min<std::size_t>(lock_owners_.size(), 4); ++i) {
        const auto& owner = lock_owners_[i];
        RECT row = R(18, y, 402, y + 52, dpi_); Fill(dc, row, selected_owner_ == static_cast<int>(i) ? RGB(235,242,255) : kSurface); Frame(dc, row, kBorder);
        HICON icon = icons_.Get(owner.process.exe_path);
        DrawIconEx(dc, row.left + S(10,dpi_), row.top + S(14,dpi_), icon, S(22,dpi_), S(22,dpi_), 0, nullptr, DI_NORMAL);
        SelectObject(dc, font_bold_); RECT name{row.left + S(42,dpi_), row.top + S(4,dpi_), row.right - S(8,dpi_), row.top + S(27,dpi_)};
        Text(dc, owner.process.name, name, kText);
        SelectObject(dc, font_); RECT detail{row.left + S(42,dpi_), row.top + S(26,dpi_), row.right - S(8,dpi_), row.bottom - S(3,dpi_)};
        Text(dc, L"PID " + std::to_wstring(owner.process.pid) + L"  ·  " + Elide(owner.process.exe_path, 45), detail, kMuted);
        y += 60;
    }
    if (selected_owner_ >= 0) {
        RECT reveal = R(18, 484, 188, 522, dpi_); Fill(dc, reveal, RGB(231,234,239)); Text(dc, strings.open_location, reveal, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT terminate = R(198, 484, 402, 522, dpi_); Fill(dc, terminate, RGB(252,232,232)); Text(dc, strings.end_process, terminate, RGB(166,45,45), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void MainWindow::PaintFocus(HDC dc, const RECT&) {
    const auto& strings = Texts();
    SelectObject(dc, font_);
    RECT intro = R(18, 70, 402, 106, dpi_);
    Text(dc, strings.focus_question, intro, kMuted);
    const bool active = focus_ && focus_->Active();
    RECT action = R(18, 108, 402, 148, dpi_); Fill(dc, action, active ? RGB(232,239,252) : kAccent);
    SelectObject(dc, font_bold_); Text(dc, active ? strings.inspecting : strings.start_inspect, action, active ? kAccent : RGB(255,255,255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, font_);
    if (!has_focus_snapshot_) { RECT empty = R(18, 164, 402, 208, dpi_); Text(dc, strings.move_pointer, empty, kMuted); return; }

    const auto& s = focus_snapshot_;
    int y = 166;
    auto field = [&](const std::wstring& label, const std::wstring& value) {
        RECT label_rect = R(18, y, 128, y + 24, dpi_); Text(dc, label, label_rect, kMuted);
        RECT value_rect = R(128, y, 402, y + 24, dpi_); Text(dc, value.empty() ? L"—" : value, value_rect, kText);
        y += 28;
    };
    field(strings.process, s.process.name + L"  ·  PID " + std::to_wstring(s.process.pid));
    field(strings.exe, Elide(s.process.exe_path, 45));
    field(strings.hwnd, L"0x" + [&]{ std::wstringstream ss; ss << std::hex << reinterpret_cast<std::uintptr_t>(s.window.hwnd); return ss.str(); }());
    field(strings.window_class, s.window.class_name);
    field(strings.title, s.window.title);
    field(strings.rectangle, RectText(s.window.bounds));
    Line(dc, S(18,dpi_), S(y+2,dpi_), S(402,dpi_), S(y+2,dpi_), kBorder); y += 14;
    field(strings.control_type, s.automation.control_type);
    field(strings.name, s.automation.name);
    field(strings.automation_id, s.automation.automation_id);
    field(strings.value, Elide(s.automation.value, 45));
    field(strings.uia_bounds, s.automation.available ? RectText(s.automation.bounds) : strings.unavailable);
    if (!s.process.exe_path.empty()) {
        RECT reveal = R(18, 492, 402, 526, dpi_); Fill(dc, reveal, RGB(231,234,239));
        Text(dc, strings.open_process_location, reveal, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void MainWindow::OnClick(POINT point) {
    if (Inside(R(326,14,402,44,dpi_), point)) { ToggleLanguage(); return; }
    for (int i = 0; i < 3; ++i) {
        RECT tab = R(18 + i * 96, 0, 102 + i * 96, 58, dpi_);
        if (Inside(tab, point)) { SwitchTo(static_cast<Tab>(i)); return; }
    }
    if (tab_ == Tab::Ghost) {
        auto items = events_.Snapshot(); int y = 108; RECT client{}; GetClientRect(hwnd_, &client);
        for (std::size_t i = 0; i < std::min<std::size_t>(items.size(), 8); ++i) {
            const bool expanded = items[i].sequence == expanded_event_;
            const int h = expanded ? 126 : 58;
            if (S(y + h + 18, dpi_) > client.bottom) break;
            if (expanded && Inside(R(30,y+91,196,y+119,dpi_), point)) { RevealPath(items[i].process.exe_path); return; }
            if (expanded && Inside(R(206,y+91,390,y+119,dpi_), point)) { ShowMain(Tab::Focus); if (focus_ && !focus_->Active()) ToggleFocus(); return; }
            if (Inside(R(18,y,402,y+h,dpi_), point)) { expanded_event_ = expanded ? 0 : items[i].sequence; InvalidateRect(hwnd_,nullptr,FALSE); return; }
            y += h + 8;
        }
    } else if (tab_ == Tab::Unlock) {
        if (Inside(R(18,108,402,212,dpi_), point)) { ChooseFile(); return; }
        int y = 224;
        for (std::size_t i = 0; i < std::min<std::size_t>(lock_owners_.size(), 4); ++i, y += 60) {
            if (Inside(R(18,y,402,y+52,dpi_), point)) { selected_owner_ = static_cast<int>(i); InvalidateRect(hwnd_,nullptr,FALSE); return; }
        }
        if (selected_owner_ >= 0 && Inside(R(18,484,188,522,dpi_), point)) RevealSelectedProcess();
        else if (selected_owner_ >= 0 && Inside(R(198,484,402,522,dpi_), point)) TerminateSelectedProcess();
    } else if (Inside(R(18,108,402,148,dpi_), point)) ToggleFocus();
    else if (has_focus_snapshot_ && Inside(R(18,492,402,526,dpi_), point)) RevealPath(focus_snapshot_.process.exe_path);
}

void MainWindow::SwitchTo(Tab tab) { tab_ = tab; InvalidateRect(hwnd_, nullptr, FALSE); }

void MainWindow::ShowMain(Tab tab) {
    tab_ = tab; ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::AddTrayIcon() {
    if (tray_added_ || !hwnd_) return;
    tray_.cbSize = sizeof(tray_); tray_.hWnd = hwnd_; tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = kTrayMessage; tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(tray_.szTip, L"Peek", static_cast<int>(std::size(tray_.szTip)));
    tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
    if (tray_added_) {
        tray_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray_);
    }
}

void MainWindow::RemoveTrayIcon() {
    if (tray_added_) Shell_NotifyIconW(NIM_DELETE, &tray_);
    tray_added_ = false;
    tray_.hWnd = nullptr;
}

void MainWindow::ShowTrayMenu(POINT point) {
    const auto& strings = Texts();
    HMENU menu = CreatePopupMenu();
    HMENU languages = CreatePopupMenu();
    AppendMenuW(languages, MF_STRING | (language_ == Language::English ? MF_CHECKED : 0), kEnglishCommand, L"English");
    AppendMenuW(languages, MF_STRING | (language_ == Language::ChineseSimplified ? MF_CHECKED : 0), kChineseCommand, L"简体中文");
    AppendMenuW(menu, MF_STRING, kOpenCommand, strings.tray_open);
    AppendMenuW(menu, MF_STRING, kSettingsCommand, strings.tray_settings);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(languages), strings.tray_language);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, strings.tray_exit);
    SetForegroundWindow(hwnd_); TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd_, nullptr); DestroyMenu(menu);
}

void MainWindow::HandleCloseRequest() {
    if (close_behavior_ == CloseBehavior::MinimizeToTray) {
        ShowWindow(hwnd_, SW_HIDE);
        return;
    }
    if (close_behavior_ == CloseBehavior::Exit) {
        DestroyWindow(hwnd_);
        return;
    }

    const auto& strings = Texts();
    const TASKDIALOG_BUTTON buttons[] = {
        {kMinimizeCloseButton, strings.minimize_to_tray},
        {kExitCloseButton, strings.exit_peek},
    };
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = hwnd_;
    config.hInstance = instance_;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
    config.pszWindowTitle = L"Peek";
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszMainInstruction = strings.close_question;
    config.pszContent = strings.close_explanation;
    config.cButtons = static_cast<UINT>(std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = kMinimizeCloseButton;
    config.pszVerificationText = strings.remember_choice;

    int button = IDCANCEL;
    BOOL remember = FALSE;
    if (SUCCEEDED(ShowTaskDialog(&config, &button, nullptr, &remember))) {
        if (button == kMinimizeCloseButton) {
            if (remember) SetCloseBehavior(CloseBehavior::MinimizeToTray);
            ShowWindow(hwnd_, SW_HIDE);
        } else if (button == kExitCloseButton) {
            if (remember) SetCloseBehavior(CloseBehavior::Exit);
            DestroyWindow(hwnd_);
        }
        return;
    }

    const std::wstring fallback = std::wstring(strings.close_question) + L"\n\n" +
        strings.minimize_to_tray + L" = Yes\n" + strings.exit_peek + L" = No";
    const int result = MessageBoxW(hwnd_, fallback.c_str(), L"Peek", MB_ICONQUESTION | MB_YESNOCANCEL);
    if (result == IDYES) ShowWindow(hwnd_, SW_HIDE);
    else if (result == IDNO) DestroyWindow(hwnd_);
}

void MainWindow::ShowSettings() {
    const auto& strings = Texts();
    const TASKDIALOG_BUTTON radios[] = {
        {kAskRadio, strings.ask_every_time},
        {kMinimizeRadio, strings.minimize_to_tray},
        {kExitRadio, strings.exit_peek},
    };
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = hwnd_;
    config.hInstance = instance_;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
    config.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
    config.pszWindowTitle = strings.settings_title;
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszMainInstruction = strings.settings_instruction;
    config.pszContent = strings.settings_explanation;
    config.cRadioButtons = static_cast<UINT>(std::size(radios));
    config.pRadioButtons = radios;
    config.nDefaultRadioButton = close_behavior_ == CloseBehavior::MinimizeToTray ? kMinimizeRadio :
        close_behavior_ == CloseBehavior::Exit ? kExitRadio : kAskRadio;

    int button = IDCANCEL;
    int radio = config.nDefaultRadioButton;
    if (SUCCEEDED(ShowTaskDialog(&config, &button, &radio, nullptr)) && button == IDOK) {
        if (radio == kMinimizeRadio) SetCloseBehavior(CloseBehavior::MinimizeToTray);
        else if (radio == kExitRadio) SetCloseBehavior(CloseBehavior::Exit);
        else SetCloseBehavior(CloseBehavior::Ask);
        return;
    }

    const std::wstring fallback = std::wstring(strings.settings_explanation) + L"\n\n" +
        strings.ask_every_time + L" = Yes\n" + strings.minimize_to_tray + L" / " + strings.exit_peek + L" = No";
    const int first = MessageBoxW(hwnd_, fallback.c_str(), strings.settings_title,
                                  MB_ICONQUESTION | MB_YESNOCANCEL);
    if (first == IDYES) SetCloseBehavior(CloseBehavior::Ask);
    else if (first == IDNO) {
        const std::wstring second = std::wstring(strings.minimize_to_tray) + L" = Yes\n" +
            strings.exit_peek + L" = No";
        const int choice = MessageBoxW(hwnd_, second.c_str(), strings.settings_title,
                                       MB_ICONQUESTION | MB_YESNOCANCEL);
        if (choice == IDYES) SetCloseBehavior(CloseBehavior::MinimizeToTray);
        else if (choice == IDNO) SetCloseBehavior(CloseBehavior::Exit);
    }
}

void MainWindow::SetCloseBehavior(CloseBehavior behavior) {
    close_behavior_ = behavior;
    SaveCloseBehavior(behavior);
}

void MainWindow::SetLanguage(Language language) {
    if (language_ == language) return;
    language_ = language;
    SaveLanguage(language_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::ToggleLanguage() {
    SetLanguage(language_ == Language::English ? Language::ChineseSimplified : Language::English);
}

void MainWindow::ChooseFile() {
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = hwnd_; dialog.lpstrFile = path; dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    std::wstring filter = Texts().all_files;
    filter.push_back(L'\0');
    filter += L"*.*";
    filter.push_back(L'\0');
    dialog.lpstrTitle = Texts().picker_title; dialog.lpstrFilter = filter.c_str(); dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&dialog)) QueryUnlock(path);
}

void MainWindow::QueryUnlock(const std::wstring& path) {
    tab_ = Tab::Unlock; unlock_path_ = path; unlock_error_.clear(); selected_owner_ = -1;
    lock_owners_ = unlock_.Query(path, unlock_error_); ShowMain(Tab::Unlock);
}

void MainWindow::RevealPath(const std::wstring& path) {
    if (!path.empty()) ShellExecuteW(hwnd_, L"open", L"explorer.exe", (L"/select,\"" + path + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
}

void MainWindow::RevealSelectedProcess() {
    if (selected_owner_ < 0 || selected_owner_ >= static_cast<int>(lock_owners_.size())) return;
    const auto& path = lock_owners_[selected_owner_].process.exe_path;
    RevealPath(path);
}

void MainWindow::TerminateSelectedProcess() {
    if (selected_owner_ < 0 || selected_owner_ >= static_cast<int>(lock_owners_.size())) return;
    const auto& owner = lock_owners_[selected_owner_];
    const auto& strings = Texts();
    const std::wstring prompt = std::wstring(strings.terminate_question) + L" " + owner.process.name + L" (PID " + std::to_wstring(owner.process.pid) + L")?\n\n" + strings.unsaved_warning;
    if (MessageBoxW(hwnd_, prompt.c_str(), strings.terminate_title, MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2) != IDOK) return;
    std::wstring error;
    if (!PrivilegeHelper::Terminate(owner.process.pid, &owner.process.start_time, error)) MessageBoxW(hwnd_, error.c_str(), L"Peek", MB_ICONERROR);
    QueryUnlock(unlock_path_);
}

void MainWindow::ToggleFocus() {
    if (!focus_) return;
    if (focus_->Active()) { focus_->Stop(); KillTimer(hwnd_, 7); overlay_.Hide(); }
    else { focus_->Start(); SetTimer(hwnd_, 7, 50, nullptr); }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::UpdateFonts(UINT dpi) {
    if (font_) DeleteObject(font_); if (font_bold_) DeleteObject(font_bold_);
    LOGFONTW log{}; log.lfHeight = -S(14, dpi); log.lfQuality = CLEARTYPE_QUALITY; lstrcpyW(log.lfFaceName, L"Segoe UI");
    font_ = CreateFontIndirectW(&log); log.lfWeight = FW_SEMIBOLD; font_bold_ = CreateFontIndirectW(&log);
}

std::wstring MainWindow::FormatTime(const std::chrono::system_clock::time_point& time) {
    const std::time_t raw = std::chrono::system_clock::to_time_t(time); std::tm local{}; localtime_s(&local, &raw);
    wchar_t text[16]{}; wcsftime(text, std::size(text), L"%H:%M:%S", &local); return text;
}

std::wstring MainWindow::KindLabel(GhostKind kind) const {
    const auto& strings = Texts();
    switch (kind) {
        case GhostKind::ProcessStarted: return strings.started; case GhostKind::ProcessExited: return strings.exited;
        case GhostKind::WindowCreated: return strings.window_created; case GhostKind::WindowShown: return strings.shown;
        case GhostKind::WindowHidden: return strings.hidden; case GhostKind::WindowDestroyed: return strings.destroyed;
    }
    return {};
}

std::wstring MainWindow::LocalizeUnlockError(const std::wstring& error) const {
    if (language_ == Language::English || error.empty()) return error;
    const auto suffix = [&](const wchar_t* prefix) {
        const std::size_t length = wcslen(prefix);
        return error.size() > length ? error.substr(length) : std::wstring{};
    };
    if (error.starts_with(L"Restart Manager session failed:")) return L"无法启动重启管理器会话：" + suffix(L"Restart Manager session failed:");
    if (error.starts_with(L"The file could not be registered:")) return L"无法登记该文件：" + suffix(L"The file could not be registered:");
    if (error.starts_with(L"Lock query failed:")) return L"查询占用进程失败：" + suffix(L"Lock query failed:");
    if (error == L"Choose or drop a file first.") return L"请先选择或拖入文件。";
    return error;
}

std::wstring MainWindow::Elide(const std::wstring& text, std::size_t max) {
    if (text.size() <= max) return text; if (max < 2) return text.substr(0, max); return text.substr(0, max - 1) + L"…";
}

} // namespace peek
