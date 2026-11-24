#include "ui_draw.h"

#include "i18n.h"
#include "ui_shell.h"
#include "common.h"
#include "embed.h"
#include "texture_loader.h"

#include <d3d11.h>

#include <algorithm>
#include <cmath>

namespace nl::ui {
namespace {

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float EaseInOutCubic(float value) {
    value = std::max(0.0f, value);
    if (value < 0.5f)
        return 4.0f * value * value * value;
    return 1.0f - 0.5f * std::pow(2.0f - 2.0f * value, 3.0f);
}

ID3D11ShaderResourceView* ToSrv(ImTextureID texture) {
    return reinterpret_cast<ID3D11ShaderResourceView*>(
        static_cast<std::uintptr_t>(texture));
}

ImTextureID ToTexture(ID3D11ShaderResourceView* srv) {
    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(srv));
}

}  // namespace

float CubicBezierEase(float t, float x1, float y1, float x2, float y2) {
    const float x = Clamp01(t);
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;

    float u = x;
    for (int i = 0; i < 8; ++i) {
        const float one_minus_u = 1.0f - u;
        const float three_one_minus_u = 3.0f * one_minus_u;
        const float three_one_minus_u_squared = one_minus_u * three_one_minus_u;
        const float derivative =
            three_one_minus_u_squared * x1 +
            6.0f * one_minus_u * u * (x2 - x1) +
            3.0f * u * u * (1.0f - x2);
        if (std::fabs(derivative) < 0.000001f)
            break;

        const float curve_x =
            u * u * u +
            (three_one_minus_u_squared * u * x1 +
             three_one_minus_u * u * u * x2) - x;
        u = Clamp01(u - curve_x / derivative);
    }

    const float one_minus_u = 1.0f - u;
    const float three_one_minus_u = 3.0f * one_minus_u;
    const float three_one_minus_u_squared = one_minus_u * three_one_minus_u;
    return u * u * u +
        (three_one_minus_u_squared * u * y1 +
         three_one_minus_u * u * u * y2);
}

void TickAnim(AnimState& state, float dt) {
    switch (state.stage) {
    case 0:
    case 1: {
        state.scale_progress = Clamp01(dt / 0.2f + state.scale_progress);
        state.window_opacity = EaseInOutCubic(state.scale_progress);

        if (state.stage == 0) {
            state.boot_progress = Clamp01(dt * 0.5f + state.boot_progress);
            state.splash_opacity = 1.0f;
            state.content_opacity = 0.0f;
            state.closing_progress = 0.0f;

            const int target_width = state.configured ? state.target_width : 200;
            const int target_height = state.configured ? state.target_height : 200;
            if (state.width != target_width || state.height != target_height) {
                state.width = target_width;
                state.height = target_height;
                state.size_dirty = true;
            }
            if (state.boot_progress >= 1.0f) {
                state.stage = 1;
                state.reveal_progress = 0.0f;
            }
            return;
        }

        state.reveal_progress = Clamp01(dt / 0.32f + state.reveal_progress);
        const float reveal = std::max(0.0f, state.reveal_progress);
        float reveal_eased = CubicBezierEase(reveal, 0.22f, 1.0f, 0.36f, 1.0f);
        if (reveal > 0.55f) {
            const float split_eased =
                CubicBezierEase(0.55f, 0.22f, 1.0f, 0.36f, 1.0f);
            const float split = (reveal - 0.55f) / 0.45f;
            const float smooth = split * split * (3.0f - 2.0f * split);
            reveal_eased = split_eased + (1.0f - split_eased) * smooth;
        }

        if (state.configured) {
            state.width = state.target_width;
            state.height = state.target_height;
        } else {
            const int width = static_cast<int>(lroundf(
                (state.target_width - 200) * reveal_eased + 200.0f));
            const int height = static_cast<int>(lroundf(
                (state.target_height - 200) * reveal_eased + 200.0f));
            if (state.width != width || state.height != height) {
                state.width = width;
                state.height = height;
                state.size_dirty = true;
            }
        }

        state.splash_opacity = 1.0f - reveal_eased;
        if (reveal_eased > 0.88f) {
            state.closing_progress =
                Clamp01((reveal_eased - 0.88f) / 0.12f) * 0.08f;
            state.content_opacity =
                EaseInOutCubic(Clamp01(state.closing_progress));
        } else {
            state.closing_progress = 0.0f;
            state.content_opacity = 0.0f;
        }

        if (state.reveal_progress >= 1.0f) {
            state.width = state.target_width;
            state.height = state.target_height;
            state.splash_opacity = 0.0f;
            state.size_dirty = true;
            state.stage = 2;
            if (state.closing_progress < 0.08f)
                state.closing_progress = 0.08f;
            state.content_opacity =
                EaseInOutCubic(Clamp01(state.closing_progress));
        }
        return;
    }

    case 2:
        state.window_opacity = 1.0f;
        state.closing_progress = Clamp01(dt / 0.4f + state.closing_progress);
        state.content_opacity = EaseInOutCubic(state.closing_progress);
        state.splash_opacity = 0.0f;
        return;

    case 3:
        state.close_progress = Clamp01(dt / 0.28f + state.close_progress);
        state.window_opacity = state.previous_window_opacity *
            (1.0f - EaseInOutCubic(state.close_progress));
        if (state.close_progress >= 1.0f) {
            state.window_opacity = 0.0f;
            state.close_finished = true;
        }
        return;

    default:
        return;
    }
}

void DrawSplash(const Theme& theme, float time, float opacity) {
    if (opacity < 0.01f)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), 0, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize, 0);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##splash", nullptr, flags);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 window_pos = ImGui::GetWindowPos();
    const ImVec2 window_size = ImGui::GetWindowSize();
    const ImVec2 center(window_pos.x + window_size.x * 0.5f,
                        window_pos.y + window_size.y * 0.5f);
    const ImVec2 window_max(window_pos.x + window_size.x,
                            window_pos.y + window_size.y);

    const ImU32 background_color = ImGui::GetColorU32(ImVec4(
        0.145f, 0.153f, 0.212f,
        (shell::GlassEnabled ? 0.5f : 1.0f) * opacity));
    if (shell::GlassEnabled && shell::BackdropTexture != 0) {
        draw_list->AddImageRounded(shell::BackdropTexture, window_pos, window_max,
                                   ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                   ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f,
                                                             opacity)),
                                   24.0f, 0);
    }
    draw_list->AddRectFilled(window_pos, window_max, background_color,
                             24.0f, 0);

    const ImVec2 card_min(center.x - 225.0f, center.y - 30.0f);
    const ImVec2 card_max(card_min.x + 450.0f, card_min.y + 60.0f);
    const ImU32 card_color = ImGui::GetColorU32(ImVec4(
        0.18f, 0.19f, 0.26f,
        (shell::GlassEnabled ? 0.55f : 1.0f) * opacity));
    draw_list->AddRectFilled(card_min, card_max, card_color, 18.0f, 0);

    const ImU32 border_color =
        ImGui::GetColorU32(ImVec4(0.24f, 0.25f, 0.33f, opacity));
    draw_list->AddRect(card_min, card_max, border_color, 18.0f, 0, 1.3f);

    ImVec4 accent = theme.accent;
    accent.w *= opacity;
    DrawSpinner(draw_list, ImVec2(card_min.x + 28.0f, card_min.y + 30.0f),
                12.0f, 2.5f, ImGui::GetColorU32(accent), time);

    ImVec4 text = theme.text;
    text.w *= opacity;
    draw_list->AddText(theme.font_heading, 26.0f,
                       ImVec2(card_min.x + 54.0f, card_min.y + 16.0f),
                       ImGui::GetColorU32(text),
                       i18n::T("Connecting to the server"), nullptr, 0.0f,
                       nullptr);
    ImGui::End();
}

void DrawSpinner(ImDrawList* draw_list, ImVec2 center, float radius,
                 float thickness, ImU32 color, float time) {
    const float start = 3.6f * time - 1.5708f;
    const float end = start + 6.28319f *
        (0.18f + 0.56f * (0.5f - 0.5f * cosf(2.6f * time)));

    draw_list->PathArcTo(center, radius, start, end, 40);
    draw_list->AddPolyline(draw_list->_Path.Data, draw_list->_Path.Size,
                           color, 0, thickness);
    draw_list->_Path.Size = 0;

    const float cap_radius = thickness * 0.5f;
    const ImVec2 start_cap(center.x + radius * cosf(start),
                           center.y + radius * sinf(start));
    const ImVec2 end_cap(center.x + radius * cosf(end),
                         center.y + radius * sinf(end));
    draw_list->AddCircleFilled(start_cap, cap_radius, color, 12);
    draw_list->AddCircleFilled(end_cap, cap_radius, color, 12);
}

UiGpuResources::~UiGpuResources() {
    Release();
}

void UiGpuResources::Release() {
    const auto release = [](ImTextureID& texture) {
        ID3D11ShaderResourceView* srv = ToSrv(texture);
        SafeRelease(srv);
        texture = 0;
    };
    release(avatar);
    release(cs2);
    release(icon_atlas);
    release(logo);
    release(reserved_020);
    release(reserved_028);
    release(reserved_030);
    reserved_038 = 0;
}

void LoadTextures(UiGpuResources& gpu, ID3D11Device* device,
                  ID3D11DeviceContext* context) {
    ID3D11ShaderResourceView* srv = nullptr;
    const auto& avatar = embed::Avatar();
    if (gfx::LoadCircularTextureFromMemory(avatar.data(), avatar.size(), device,
                                           context, &srv)) {
        gpu.avatar = ToTexture(srv);
    } else {
        gpu.avatar = 0;
        Log("Failed to load avatar texture");
    }

    srv = nullptr;
    const auto& cs2 = embed::Cs2();
    if (gfx::LoadTextureFromMemory(cs2.data(), cs2.size(), device, &srv)) {
        gpu.cs2 = ToTexture(srv);
    } else {
        gpu.cs2 = 0;
        Log("Failed to load CS2 texture");
    }

    srv = nullptr;
    const auto& atlas = embed::ReferenceIcons();
    if (gfx::LoadTextureFromMemory(atlas.data(), atlas.size(), device, &srv)) {
        gpu.icon_atlas = ToTexture(srv);
        shell::IconAtlas = gpu.icon_atlas;
    } else {
        gpu.icon_atlas = 0;
        shell::IconAtlas = 0;
    }

    srv = nullptr;
    const auto& logo = embed::NeverloseLogo();
    if (gfx::LoadTextureFromMemory(logo.data(), logo.size(), device, &srv))
        gpu.logo = ToTexture(srv);
    else
        gpu.logo = 0;
}

void ReleaseTextures(UiGpuResources& gpu) {
    shell::IconAtlas = 0;
    gpu.Release();
}

}  // namespace nl::ui
