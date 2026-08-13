#pragma once

#include "core/event_store.h"
#include "core/icon_cache.h"
#include "core/localization.h"
#include "focus/focus_inspector.h"
#include "ui/overlay_window.h"
#include "unlock/unlock_service.h"

#include <shellapi.h>

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace peek {

class MainWindow {
public:
    static constexpr UINT kTrayMessage = WM_APP + 10;
    static constexpr UINT kGhostChanged = WM_APP + 11;
    static constexpr UINT kFocusChanged = WM_APP + 12;
    static constexpr int kGhostHotkey = 1;
    static constexpr int kFocusHotkey = 2;
    static constexpr int kOpenCommand = 1001;
    static constexpr int kExitCommand = 1002;
    static constexpr int kSettingsCommand = 1003;
    static constexpr int kEnglishCommand = 1101;
    static constexpr int kChineseCommand = 1102;

    MainWindow(EventStore& events, UnlockService& unlock, IconCache& icons);
    ~MainWindow();
    bool Create(HINSTANCE instance, int show_command, std::wstring& error);
    int Run();
    void SetFocusInspector(FocusInspector* focus) { focus_ = focus; }
    void NotifyGhost();
    void DeliverFocus(FocusSnapshot snapshot);
    [[nodiscard]] HWND Handle() const { return hwnd_; }
    [[nodiscard]] Language CurrentLanguage() const { return language_; }

private:
    enum class Tab { Ghost, Unlock, Focus };
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Paint(HDC dc);
    void PaintHeader(HDC dc, const RECT& client);
    void PaintGhost(HDC dc, const RECT& body);
    void PaintUnlock(HDC dc, const RECT& body);
    void PaintFocus(HDC dc, const RECT& body);
    void OnClick(POINT point);
    void SwitchTo(Tab tab);
    void ShowMain(Tab tab);
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu(POINT point);
    void HandleCloseRequest();
    void ShowSettings();
    void SetCloseBehavior(CloseBehavior behavior);
    void SetLanguage(Language language);
    void ToggleLanguage();
    void ChooseFile();
    void QueryUnlock(const std::wstring& path);
    void RevealPath(const std::wstring& path);
    void RevealSelectedProcess();
    void TerminateSelectedProcess();
    void ToggleFocus();
    void UpdateFonts(UINT dpi);
    static std::wstring FormatTime(const std::chrono::system_clock::time_point& time);
    std::wstring KindLabel(GhostKind kind) const;
    [[nodiscard]] const Strings& Texts() const { return GetStrings(language_); }
    [[nodiscard]] std::wstring LocalizeUnlockError(const std::wstring& error) const;
    static std::wstring Elide(const std::wstring& text, std::size_t max);

    EventStore& events_;
    UnlockService& unlock_;
    IconCache& icons_;
    FocusInspector* focus_{};
    OverlayWindow overlay_;
    HWND hwnd_{};
    HINSTANCE instance_{};
    HFONT font_{};
    HFONT font_bold_{};
    UINT dpi_{96};
    Language language_{LoadLanguage()};
    CloseBehavior close_behavior_{LoadCloseBehavior()};
    UINT taskbar_created_message_{};
    Tab tab_{Tab::Ghost};
    std::wstring unlock_path_;
    std::wstring unlock_error_;
    std::vector<LockOwner> lock_owners_;
    int selected_owner_{-1};
    std::uint64_t expanded_event_{};
    FocusSnapshot focus_snapshot_{};
    std::mutex pending_focus_mutex_;
    std::optional<FocusSnapshot> pending_focus_;
    bool has_focus_snapshot_{};
    NOTIFYICONDATAW tray_{};
    bool tray_added_{};
};

} // namespace peek
