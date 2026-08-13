#include "core/localization.h"

#include <windows.h>

namespace peek {
namespace {

constexpr Strings kEnglish{
    L"Ghost", L"Unlock", L"Focus", L"中文",
    L"Waiting for a short process or window…", L"What just flashed past", L"Who is using this file?",
    L"What Windows UI is under the pointer?", L"Unknown process", L"from",
    L"Open location", L"Enter Focus", L"Drop a file here, or choose one", L"Choose file",
    L"No owner reported", L"owner(s) found", L"End process…",
    L"Inspecting — press Esc to stop", L"Start Inspect  ·  Ctrl+Shift+F", L"Move the pointer over a window or control.",
    L"Process", L"EXE", L"HWND", L"Class", L"Title", L"Rectangle", L"Control type", L"Name",
    L"AutomationId", L"Value", L"UIA bounds", L"Unavailable", L"Open process location",
    L"started", L"exited", L"window created", L"shown", L"hidden", L"destroyed",
    L"Open Peek", L"Language", L"Exit", L"Choose a file to inspect", L"All files",
    L"End", L"Unsaved work in that process may be lost.", L"Peek — End process",
    L"Main window class could not be registered.", L"Main window could not be created.",
    L"Ctrl+Shift+G is already in use; Peek will still run from the tray.",
    L"Settings", L"What should Peek do when you close the window?",
    L"You can change this later from Settings in the tray menu.",
    L"Minimize to tray", L"Exit Peek", L"Remember my choice",
    L"Peek Settings", L"Close button behavior",
    L"Choose what happens when you click the window's close button.", L"Ask every time"
};

constexpr Strings kChinese{
    L"闪现", L"解锁", L"探查", L"EN",
    L"正在等待短生命周期进程或窗口…", L"刚才闪过去的是什么？", L"谁正在占用这个文件？",
    L"鼠标下面这个 Windows UI 是什么？", L"未知进程", L"来自",
    L"打开所在位置", L"进入探查", L"把文件拖到这里，或选择文件", L"选择文件",
    L"未发现占用进程", L"个占用进程", L"结束进程…",
    L"正在探查 — 按 Esc 退出", L"开始探查  ·  Ctrl+Shift+F", L"将鼠标移到窗口或控件上。",
    L"进程", L"程序路径", L"窗口句柄", L"窗口类", L"窗口标题", L"矩形", L"控件类型", L"名称",
    L"自动化 ID", L"值", L"UIA 边界", L"不可用", L"打开进程所在位置",
    L"已启动", L"已退出", L"窗口已创建", L"已显示", L"已隐藏", L"已销毁",
    L"打开 Peek", L"语言", L"退出", L"选择要检查的文件", L"所有文件",
    L"结束", L"该进程中未保存的工作可能会丢失。", L"Peek — 结束进程",
    L"无法注册主窗口类。", L"无法创建主窗口。",
    L"Ctrl+Shift+G 已被占用；仍可从托盘打开 Peek。",
    L"设置", L"关闭窗口时，Peek 应该怎么做？",
    L"之后可以在托盘菜单的“设置”中修改。",
    L"最小化到托盘", L"退出 Peek", L"记住我的选择",
    L"Peek 设置", L"关闭按钮行为",
    L"选择点击窗口关闭按钮时要执行的操作。", L"每次询问"
};

constexpr wchar_t kRegistryPath[] = L"Software\\Peek";
constexpr wchar_t kLanguageValue[] = L"Language";
constexpr wchar_t kCloseBehaviorValue[] = L"CloseBehavior";

} // namespace

const Strings& GetStrings(Language language) {
    return language == Language::ChineseSimplified ? kChinese : kEnglish;
}

Language LoadLanguage() {
    wchar_t value[16]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, kLanguageValue, RRF_RT_REG_SZ,
                     nullptr, value, &size) == ERROR_SUCCESS) {
        if (lstrcmpiW(value, L"zh-CN") == 0) return Language::ChineseSimplified;
        if (lstrcmpiW(value, L"en-US") == 0) return Language::English;
    }
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE
        ? Language::ChineseSimplified : Language::English;
}

void SaveLanguage(Language language) {
    const wchar_t* value = language == Language::ChineseSimplified ? L"zh-CN" : L"en-US";
    RegSetKeyValueW(HKEY_CURRENT_USER, kRegistryPath, kLanguageValue, REG_SZ,
                    value, static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
}

CloseBehavior LoadCloseBehavior() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, kCloseBehaviorValue, RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS && value <= 2) {
        return static_cast<CloseBehavior>(value);
    }
    return CloseBehavior::Ask;
}

void SaveCloseBehavior(CloseBehavior behavior) {
    const DWORD value = static_cast<DWORD>(behavior);
    RegSetKeyValueW(HKEY_CURRENT_USER, kRegistryPath, kCloseBehaviorValue, REG_DWORD,
                    &value, sizeof(value));
}

} // namespace peek
