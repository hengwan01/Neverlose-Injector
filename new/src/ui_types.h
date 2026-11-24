#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace nl::data {
struct CheatInfo;
struct CheatStore;
}  // namespace nl::data

namespace nl::ui {

struct Theme {
    ImVec4 panel = ImVec4(0.0f, 0.01f, 0.01f, 1.0f);
    ImVec4 accent = ImVec4(0.364706f, 0.482353f, 0.784314f, 1.0f);
    ImVec4 card = ImVec4(0.016f, 0.035f, 0.059f, 1.0f);
    ImVec4 border = ImVec4(0.067f, 0.075f, 0.09f, 1.0f);
    ImVec4 text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 muted = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    ImVec4 text_dim = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    ImVec4 text_soft = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    ImVec4 text_strong = ImVec4(0.901961f, 0.901961f, 0.901961f, 1.0f);
    ImVec4 muted_alt = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);
    ImVec4 hover = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    ImVec4 warning = ImVec4(0.93f, 0.56f, 0.2f, 1.0f);
    ImVec4 accent_alt = ImVec4(0.423529f, 0.517647f, 0.729412f, 1.0f);
    ImVec4 accent_hover = ImVec4(0.282353f, 0.462745f, 0.839216f, 1.0f);
    ImVec4 text_secondary = ImVec4(0.78f, 0.8f, 0.86f, 1.0f);
    ImVec4 text_bright = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    ImVec4 border_alt = ImVec4(0.04f, 0.05f, 0.07f, 1.0f);
    float radius_small = 8.0f;
    float radius_medium = 12.0f;
    float radius_large = 18.0f;
    float radius_xlarge = 20.0f;
    float spacing_small = 8.0f;
    float spacing_medium = 16.0f;
    float spacing_large = 35.0f;
    float spacing_xlarge = 10.0f;
    float popup_rounding = 5.0f;
    float child_rounding = 8.0f;
    float frame_rounding = 6.0f;
    float button_rounding = 10.0f;
    ImFont* font_body = nullptr;       // 20 px
    ImFont* font_small = nullptr;      // 13 px
    ImFont* font_title = nullptr;      // 40 px
    ImFont* font_semibold = nullptr;   // 22 px
    ImFont* font_heading = nullptr;    // 30 px
};

struct AnimState {
    int stage = 0;
    float boot_progress = 0.0f;
    float reveal_progress = 0.0f;
    float content_opacity = 0.0f;
    float closing_progress = 0.0f;
    float splash_opacity = 1.0f;
    float window_opacity = 0.0f;
    float close_progress = 0.0f;
    float scale_progress = 0.0f;
    float previous_window_opacity = 1.0f;
    int width = 2;
    int height = 2;
    int target_width = 5;
    int target_height = 4;
    bool size_dirty = true;
    bool close_finished = false;
    bool configured = false;
};

struct UiGpuResources {
    ImTextureID avatar = 0;
    ImTextureID cs2 = 0;
    ImTextureID icon_atlas = 0;
    ImTextureID logo = 0;
    ImTextureID reserved_020 = 0;
    ImTextureID reserved_028 = 0;
    ImTextureID reserved_030 = 0;
    std::uint64_t reserved_038 = 0;
    std::uint64_t reserved_040 = 0;

    ~UiGpuResources();
    void Release();
};

void LoadTextures(UiGpuResources& gpu, ID3D11Device* device,
                  ID3D11DeviceContext* context);
void ReleaseTextures(UiGpuResources& gpu);

struct FrameInput {
    std::uint64_t tick_ms = 0;
    std::int64_t unix_time = 0;
    float dt = 0.0f;
    int width = 0;
    int height = 0;
    std::uint32_t reserved_01c = 0;
};

struct DashActions {
    bool close = false;
    bool minimize = false;
    std::uint8_t reserved_002[6] = {};
    const wchar_t* open_url = nullptr;
};

struct ModalState {
    struct DisplayedChangelogBlock {
        std::string date;
        std::vector<std::string> lines;
    };

    bool open = false;
    std::uint8_t reserved_001[0x27] = {};
    std::string cheat_id;
    bool branch_open = false;
    bool changelog_open = false;
    std::uint8_t reserved_042[0x02] = {};
    float changelog_alpha = 0.0f;
    bool changelog_blur_captured = false;
    std::uint8_t reserved_049[0x03] = {};
    float branch_alpha = 0.0f;
    std::uint8_t reserved_050[0x30] = {};
    std::string displayed_branch;
    std::string displayed_updated;
    std::string displayed_subscription;
    std::string displayed_last_launch;
    std::vector<DisplayedChangelogBlock> changelog_blocks;
    bool launch_overlay_visible = false;
    bool launch_overlay_aux = false;
    std::uint8_t reserved_0fa[0x06] = {};
    std::uint64_t launch_started_ms = 0;
    float launch_overlay_params[4] = {1.0f, 450.0f, 350.0f, 0.0f};
    float launch_overlay_alpha = 0.0f;
    std::uint8_t reserved_11c[0x04] = {};
    bool changelog_drawn = false;
    bool close_requested = false;
    std::uint8_t reserved_122[0x06] = {};
};

struct UiState {
    float primary_anim = 1.0f;
    std::uint32_t reserved_004 = 0;
    float secondary_anim = 0.0f;
    float loading_transition = 0.0f;
    std::uint8_t reserved_010[0x20] = {};
    float profile_hover = 0.0f;
    std::uint8_t reserved_034[0x8c] = {};
    ModalState modal;
    std::string nickname = "Awa Subaru";
    bool settings_open = false;
    std::uint8_t reserved_201[0x07] = {};
    std::string launch_status = "Preparing CS2...";
    bool launch_cancellable = false;
    bool launch_error = false;
    bool launch_ready = false;
    std::uint8_t reserved_223[0x05] = {};
    std::uint64_t completion_started_ms = 0;
    bool need_restart_admin = false;
    std::uint8_t reserved_231[0x03] = {};
    float launch_overlay_target = 0.0f;
};

void DrawDashboard(Theme& theme, UiState& state, const UiGpuResources& gpu,
                   nl::data::CheatStore& store, float opacity,
                   const FrameInput& input, DashActions& actions);

void FillDisplayedBranch(UiState& state, const nl::data::CheatInfo& cheat,
                         long long now);

static_assert(sizeof(void*) == 0x8, "Dashboard layout requires 64-bit pointers");
static_assert(sizeof(ImTextureID) == 0x8, "Unexpected ImTextureID layout");
static_assert(sizeof(ImVec4) == 0x10, "Unexpected ImVec4 layout");
static_assert(sizeof(std::string) == 0x18,
              "Dashboard layout requires the original 24-byte libc++ string");
static_assert(sizeof(std::vector<std::string>) == 0x18,
              "Dashboard layout requires the original 24-byte libc++ vector");
static_assert(sizeof(std::vector<ModalState::DisplayedChangelogBlock>) == 0x18,
              "Unexpected changelog vector layout");

static_assert(sizeof(Theme) == 0x168, "Unexpected Theme size");
static_assert(offsetof(Theme, text) == 0x40, "Unexpected Theme::text offset");
static_assert(offsetof(Theme, font_body) == 0x140, "Unexpected Theme::font_body offset");
static_assert(offsetof(Theme, font_small) == 0x148, "Unexpected Theme font offset");
static_assert(offsetof(Theme, font_title) == 0x150, "Unexpected Theme font offset");
static_assert(offsetof(Theme, font_semibold) == 0x158, "Unexpected Theme::font_semibold offset");
static_assert(offsetof(Theme, font_heading) == 0x160, "Unexpected Theme font offset");
static_assert(sizeof(AnimState) == 0x3c, "Unexpected AnimState size");
static_assert(offsetof(AnimState, content_opacity) == 0x0c, "Unexpected animation offset");
static_assert(offsetof(AnimState, splash_opacity) == 0x14, "Unexpected animation offset");
static_assert(offsetof(AnimState, window_opacity) == 0x18, "Unexpected animation offset");
static_assert(offsetof(AnimState, target_width) == 0x30, "Unexpected animation offset");

static_assert(sizeof(UiGpuResources) == 0x48, "Unexpected UiGpuResources size");
static_assert(offsetof(UiGpuResources, avatar) == 0x00, "Unexpected avatar offset");
static_assert(offsetof(UiGpuResources, cs2) == 0x08, "Unexpected cs2 offset");
static_assert(offsetof(UiGpuResources, icon_atlas) == 0x10, "Unexpected icon_atlas offset");
static_assert(offsetof(UiGpuResources, logo) == 0x18, "Unexpected logo offset");
static_assert(offsetof(UiGpuResources, reserved_038) == 0x38, "Unexpected resource offset");
static_assert(offsetof(UiGpuResources, reserved_040) == 0x40, "Unexpected resource offset");

static_assert(sizeof(FrameInput) == 0x20, "Unexpected FrameInput size");
static_assert(offsetof(FrameInput, tick_ms) == 0x00, "Unexpected tick_ms offset");
static_assert(offsetof(FrameInput, unix_time) == 0x08, "Unexpected unix_time offset");
static_assert(offsetof(FrameInput, dt) == 0x10, "Unexpected dt offset");
static_assert(offsetof(FrameInput, width) == 0x14, "Unexpected width offset");
static_assert(offsetof(FrameInput, height) == 0x18, "Unexpected height offset");

static_assert(sizeof(DashActions) == 0x10, "Unexpected DashActions size");
static_assert(offsetof(DashActions, close) == 0x00, "Unexpected close offset");
static_assert(offsetof(DashActions, minimize) == 0x01, "Unexpected minimize offset");
static_assert(offsetof(DashActions, open_url) == 0x08, "Unexpected open_url offset");

static_assert(sizeof(ModalState::DisplayedChangelogBlock) == 0x30,
              "Unexpected DisplayedChangelogBlock size");
static_assert(offsetof(ModalState::DisplayedChangelogBlock, date) == 0x00,
              "Unexpected changelog date offset");
static_assert(offsetof(ModalState::DisplayedChangelogBlock, lines) == 0x18,
              "Unexpected changelog lines offset");
static_assert(sizeof(ModalState) == 0x128, "Unexpected ModalState size");
static_assert(offsetof(ModalState, cheat_id) == 0x28, "Unexpected cheat_id offset");
static_assert(offsetof(ModalState, displayed_branch) == 0x80, "Unexpected branch offset");
static_assert(offsetof(ModalState, changelog_blocks) == 0xe0, "Unexpected changelog offset");
static_assert(offsetof(ModalState, launch_overlay_visible) == 0xf8, "Unexpected overlay offset");
static_assert(offsetof(ModalState, changelog_drawn) == 0x120, "Unexpected drawn offset");
static_assert(offsetof(ModalState, close_requested) == 0x121, "Unexpected close offset");

static_assert(sizeof(UiState) == 0x238, "Unexpected UiState size");
static_assert(offsetof(UiState, primary_anim) == 0x000, "Unexpected primary_anim offset");
static_assert(offsetof(UiState, secondary_anim) == 0x008, "Unexpected secondary_anim offset");
static_assert(offsetof(UiState, loading_transition) == 0x00c, "Unexpected loading offset");
static_assert(offsetof(UiState, profile_hover) == 0x030, "Unexpected profile_hover offset");
static_assert(offsetof(UiState, modal) == 0x0c0, "Unexpected modal offset");
static_assert(offsetof(UiState, nickname) == 0x1e8, "Unexpected nickname offset");
static_assert(offsetof(UiState, settings_open) == 0x200, "Unexpected settings_open offset");
static_assert(offsetof(UiState, launch_status) == 0x208, "Unexpected launch_status offset");
static_assert(offsetof(UiState, launch_cancellable) == 0x220, "Unexpected launch flag offset");
static_assert(offsetof(UiState, completion_started_ms) == 0x228, "Unexpected completion offset");
static_assert(offsetof(UiState, need_restart_admin) == 0x230, "Unexpected restart offset");
static_assert(offsetof(UiState, launch_overlay_target) == 0x234, "Unexpected overlay target offset");

}  // namespace nl::ui
