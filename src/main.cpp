#include "app.h"
#include "self_test.h"

#include <windows.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 2 && std::wstring(argv[1]) == L"--self-test") {
        const std::wstring report = argc >= 3 ? argv[2] : L"";
        LocalFree(argv);
        return peek::RunSelfTest(report);
    }
    if (argv) LocalFree(argv);
    peek::App app;
    return app.Run(instance, show_command);
}
