#include "focus/focus_inspector.h"

#include <ole2.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace peek {
namespace {

using Microsoft::WRL::ComPtr;

std::wstring BstrToString(BSTR value) {
    std::wstring result = value ? value : L"";
    if (value) SysFreeString(value);
    return result;
}

WindowInfo ReadWindow(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &info.pid);
    wchar_t value[512]{};
    int length = GetClassNameW(hwnd, value, static_cast<int>(std::size(value)));
    if (length > 0) info.class_name.assign(value, length);
    length = GetWindowTextW(hwnd, value, static_cast<int>(std::size(value)));
    if (length > 0) info.title.assign(value, length);
    GetWindowRect(hwnd, &info.bounds);
    return info;
}

AutomationInfo ReadAutomation(IUIAutomation* automation, POINT point) {
    AutomationInfo info;
    if (!automation) return info;
    ComPtr<IUIAutomationElement> element;
    if (FAILED(automation->ElementFromPoint(point, &element)) || !element) return info;
    info.available = true;
    BSTR text{};
    if (SUCCEEDED(element->get_CurrentAutomationId(&text))) info.automation_id = BstrToString(text);
    text = nullptr;
    if (SUCCEEDED(element->get_CurrentLocalizedControlType(&text))) info.control_type = BstrToString(text);
    text = nullptr;
    if (SUCCEEDED(element->get_CurrentName(&text))) info.name = BstrToString(text);
    element->get_CurrentBoundingRectangle(&info.bounds);

    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &value)) && value.vt == VT_BSTR && value.bstrVal) {
        info.value = value.bstrVal;
    }
    VariantClear(&value);
    return info;
}

AutomationInfo ReadAutomationFromHandle(IUIAutomation* automation, HWND hwnd) {
    AutomationInfo info;
    if (!automation || !hwnd) return info;
    ComPtr<IUIAutomationElement> element;
    if (FAILED(automation->ElementFromHandle(hwnd, &element)) || !element) return info;
    info.available = true;
    BSTR text{};
    if (SUCCEEDED(element->get_CurrentAutomationId(&text))) info.automation_id = BstrToString(text);
    text = nullptr;
    if (SUCCEEDED(element->get_CurrentLocalizedControlType(&text))) info.control_type = BstrToString(text);
    text = nullptr;
    if (SUCCEEDED(element->get_CurrentName(&text))) info.name = BstrToString(text);
    element->get_CurrentBoundingRectangle(&info.bounds);
    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &value)) && value.vt == VT_BSTR && value.bstrVal) {
        info.value = value.bstrVal;
    }
    VariantClear(&value);
    return info;
}

std::wstring HResultText(const wchar_t* operation, HRESULT result) {
    std::wstringstream text;
    text << operation << L" failed (0x" << std::uppercase << std::hex
         << static_cast<unsigned long>(result) << L").";
    return text.str();
}

bool ReadSnapshot(ProcessResolver& resolver, IUIAutomation* automation, POINT point, FocusSnapshot& snapshot) {
    HWND hwnd = WindowFromPoint(point);
    if (!hwnd) return false;
    snapshot.cursor = point;
    snapshot.window = ReadWindow(hwnd);
    snapshot.process = resolver.Resolve(snapshot.window.pid);
    snapshot.automation = ReadAutomation(automation, point);
    if (!snapshot.automation.available) snapshot.automation = ReadAutomationFromHandle(automation, hwnd);
    return true;
}

} // namespace

FocusInspector::FocusInspector(ProcessResolver& resolver, SnapshotCallback callback)
    : resolver_(resolver), callback_(std::move(callback)) {}

FocusInspector::~FocusInspector() { Stop(); }

void FocusInspector::Start() {
    if (active_.exchange(true)) return;
    worker_ = std::thread([this] { Loop(); });
}

void FocusInspector::Stop() {
    active_ = false;
    if (worker_.joinable()) worker_.join();
}

bool FocusInspector::InspectOnce(POINT point, FocusSnapshot& snapshot, std::wstring& error) const {
    error.clear();
    snapshot = {};
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init)) {
        error = HResultText(L"CoInitializeEx", init);
        return false;
    }
    ComPtr<IUIAutomation> automation;
    const HRESULT created = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                              IID_PPV_ARGS(&automation));
    if (FAILED(created)) error = HResultText(L"UI Automation initialization", created);
    const bool sampled = ReadSnapshot(resolver_, automation.Get(), point, snapshot);
    if (!sampled && error.empty()) error = L"WindowFromPoint did not return a window.";
    automation.Reset();
    CoUninitialize();
    return sampled;
}

void FocusInspector::Loop() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IUIAutomation> automation;
    if (SUCCEEDED(init)) CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
    POINT previous{LONG_MIN, LONG_MIN};
    while (active_) {
        POINT point{};
        if (GetCursorPos(&point) && (point.x != previous.x || point.y != previous.y)) {
            previous = point;
            FocusSnapshot snapshot;
            if (ReadSnapshot(resolver_, automation.Get(), point, snapshot)) {
                if (callback_) callback_(std::move(snapshot));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    automation.Reset();
    if (SUCCEEDED(init)) CoUninitialize();
}

} // namespace peek
