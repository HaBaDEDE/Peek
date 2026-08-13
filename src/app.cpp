#include "app.h"

#include <commctrl.h>
#include <objbase.h>

namespace peek {

App::App()
    : unlock_(resolver_),
      window_(events_, unlock_, icons_),
      ghost_(events_, resolver_, [this] { window_.NotifyGhost(); }),
      focus_(resolver_, [this](FocusSnapshot snapshot) { window_.DeliverFocus(std::move(snapshot)); }) {
    window_.SetFocusInspector(&focus_);
}

int App::Run(HINSTANCE instance, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                         RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    std::wstring error;
    if (!window_.Create(instance, show_command, error)) {
        MessageBoxW(nullptr, error.c_str(), L"Peek", MB_ICONERROR);
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
    if (!error.empty()) MessageBoxW(window_.Handle(), error.c_str(), L"Peek", MB_ICONINFORMATION);

    std::wstring ghost_error;
    if (!ghost_.Start(ghost_error)) {
        const wchar_t* title = window_.CurrentLanguage() == Language::ChineseSimplified
            ? L"Peek — 闪现不可用" : L"Peek — Ghost unavailable";
        MessageBoxW(window_.Handle(), ghost_error.c_str(), title, MB_ICONWARNING);
    }
    const int result = window_.Run();
    focus_.Stop();
    ghost_.Stop();
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}

} // namespace peek
