#include "app.h"
#include "app_window.h"
#include "app_device.h"
#include "app_frame.h"

namespace nl {

App::App() = default;
App::~App() { Shutdown(); }

bool App::Init(HINSTANCE hInstance) {
    frame_  = std::make_unique<AppFrame>();
    window_ = std::make_unique<AppWindow>();
    device_ = std::make_unique<AppDevice>();
    frame_->AttachWindow(*window_);

    frame_->ConfigureBootAnim(200, 690, 468);
    if (!window_->Create(hInstance, frame_->WindowWidth(), frame_->WindowHeight(),
                         frame_.get()))
        return false;

    window_->ApplyRoundRegion(frame_->TargetWidth(), frame_->TargetHeight(),
                              frame_->WindowWidth(), frame_->WindowHeight(), 17);
    frame_->ClearSizeDirty();

    if (!device_->Init(window_->Hwnd(), frame_->TargetWidth(), frame_->TargetHeight())) {
        Log("AppDevice::Init failed");
        return false;
    }
    if (!frame_->InitImGui(window_->Hwnd(), *device_)) {
        Log("AppFrame::InitImGui failed");
        return false;
    }
    if (!frame_->InitBlurAndTextures(*device_)) {
        Log("AppFrame::InitBlurAndTextures failed");
        return false;
    }

    frame_->LoadModels();
    if (frame_->Store().cheats.empty())
        Log("CheatStore::Load returned no cheats");
    frame_->InitializeLocalRelease();
    frame_->InitClock();
    return true;
}

int App::Run() {
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running || !frame_->TickFrame(*window_, *device_, running))
            break;
    }
    return 0;
}

void App::Shutdown() {
    if (frame_) frame_->ShutdownGraphics();
    if (device_) device_->Cleanup();
    if (window_) window_->Destroy(false);
    frame_.reset();
    device_.reset();
    window_.reset();
}

}  // namespace nl
