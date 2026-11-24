#pragma once

#include "app_device.h"
#include "app_window.h"
#include "common.h"
#include "data_store.h"
#include "desktop_blur.h"
#include "gfx_blur.h"
#include "profile.h"
#include "runtime.h"
#include "ui_modal.h"
#include "ui_settings.h"
#include "ui_types.h"

#include <cstdint>
#include <string>

namespace nl {

class AppFrame : public AppWindowHost {
public:
    AppFrame();
    ~AppFrame() override;

    void AttachWindow(AppWindow& window) { window_ = &window; }
    void ConfigureBootAnim(int boot_size, int width, int height);
    void InitClock();
    bool InitImGui(HWND hwnd, AppDevice& device);
    void InitFonts();
    bool InitBlurAndTextures(AppDevice& device);
    void RefreshConfigTheme();
    void ReloadAvatar();
    void LoadModels();
    void InitializeLocalRelease();
    bool TickFrame(AppWindow& window, AppDevice& device, bool& running);
    void TeardownImGui();
    void ShutdownGraphics();

    int WindowWidth() const { return anim_.width; }
    int WindowHeight() const { return anim_.height; }
    int TargetWidth() const { return anim_.target_width; }
    int TargetHeight() const { return anim_.target_height; }
    void ClearSizeDirty() { anim_.size_dirty = false; }

    data::CheatStore& Store() { return store_; }
    const data::CheatStore& Store() const { return store_; }

    bool ForwardRawMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    bool IsDragDisabled() const override;
    bool WantCaptureMouse() const override;
    bool CanDragAt(POINT p) const override;
    void OnBeginDrag() override;
    void OnDragMove() override;
    void OnEndDrag() override;
    void OnRequestClose() override;
    void OnDpiChanged(unsigned dpi, int w, int h) override;

private:
    void RenderUi(float dt, AppWindow& window);
    void DrawSettings(const ui::FrameInput& input);
    void ProcessDashActions(const ui::DashActions& actions, AppWindow& window,
                            bool& close_requested);
    void ProcessModalActions(ui::ModalActions& actions, long long now,
                             AppWindow& window);
    void PollRuntimeStatus(unsigned long long tick_ms, long long now);
    void RequestUserClose(AppWindow& window);
    void OpenUrl(const wchar_t* url);
    bool RecreateGraphics(AppDevice& device, AppWindow& window);

    bool ChooseAvatar(Profile& draft, ImTextureID& avatar, std::string& error);
    bool SaveSettingsProfile(const Profile& draft, std::string& error);
    void ProfileSaved(const Profile& profile, bool restart);

    ImGuiContext* ctx_ = nullptr;
    AppDevice* device_ = nullptr;
    AppWindow* window_ = nullptr;
    HWND hwnd_ = nullptr;
    bool initialized_ = false;
    bool win32_initialized_ = false;
    bool dx11_initialized_ = false;

    ui::Theme theme_;
    ui::AnimState anim_;
    ui::UiState state_;
    ui::UiGpuResources gpu_;
    ui::SettingsState settings_;
    ui::SettingsActions settings_actions_;
    gfx::Blur blur_;
    gfx::DesktopBlur desktop_blur_;

    Profile profile_;
    data::CheatStore store_;
    Runtime runtime_;

    ID3D11ShaderResourceView* settings_avatar_preview_ = nullptr;
    std::uint32_t saved_runtime_revision_ = 0;
    std::string deferred_error_text_;
    unsigned long long completion_tick_ms_ = 0;

    LARGE_INTEGER qpc_frequency_{};
    LARGE_INTEGER qpc_last_{};
    float animation_time_ = 0.0f;
    unsigned long long next_theme_refresh_ms_ = 0;
};

}  // namespace nl
