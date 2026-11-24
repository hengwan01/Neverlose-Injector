#pragma once
#include "common.h"
#include <memory>
#include <string>

namespace nl {

extern bool gPreviewMode;
extern std::wstring gCapturePage;
extern std::wstring gCapturePath;

class AppWindow;
class AppDevice;
class AppFrame;

// Owns the window, D3D11 device and ImGui frame; runs the message loop.
class App {
public:
    App();
    ~App();

    bool Init(HINSTANCE hInstance);
    int  Run();
    void Shutdown();

private:
    std::unique_ptr<AppWindow> window_;
    std::unique_ptr<AppDevice> device_;
    std::unique_ptr<AppFrame>  frame_;
};

}  // namespace nl
