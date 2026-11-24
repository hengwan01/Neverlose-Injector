#include "app.h"
#include "paths.h"
#include <objbase.h>
#include <shellapi.h>
#include <cwchar>

#pragma comment(lib, "ole32.lib")

namespace nl {
bool gPreviewMode = false;
std::wstring gCapturePage;
std::wstring gCapturePath;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    std::wstring data_dir;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (std::wcscmp(argv[i], L"--preview") == 0) {
                nl::gPreviewMode = true;
            } else if (i + 1 < argc) {
                if (std::wcscmp(argv[i], L"--page") == 0)
                    nl::gCapturePage = argv[++i];
                else if (std::wcscmp(argv[i], L"--data-dir") == 0)
                    data_dir = argv[++i];
                else if (std::wcscmp(argv[i], L"--capture") == 0)
                    nl::gCapturePath = argv[++i];
            }
        }
        LocalFree(argv);
    }
    nl::paths::SetUserDataDirOverride(data_dir);

    // Per-monitor DPI awareness v2 (must be set before any window is created).
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    nl::App app;
    if (!app.Init(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize NL Loader", L"NL Loader", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    int rc = app.Run();
    app.Shutdown();
    CoUninitialize();
    return rc;
}
