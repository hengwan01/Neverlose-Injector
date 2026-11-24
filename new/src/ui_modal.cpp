#include "ui_modal.h"

#include "data_store.h"
#include "gfx_blur.h"
#include "i18n.h"
#include "ui_shell.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace nl::ui {
namespace {

template <typename T>
T& StateField(UiState& state, std::size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(&state) + offset);
}

ModalState& Modal(UiState& state) {
    return StateField<ModalState>(state, 0xc0);
}

float Approach(float value, float target, float speed, float dt) {
    const float step = 1.0f - expf(-speed * std::clamp(dt, 0.0f, 0.1f));
    return value + (target - value) * step;
}

ImU32 TextColor(const Theme& theme, float alpha) {
    return ImGui::GetColorU32(ImVec4(theme.text.x, theme.text.y, theme.text.z,
                                     theme.text.w * alpha));
}

void BringCurrentWindowToFront() {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    window->WriteAccessed = true;
    ImGui::BringWindowToDisplayFront(window);
}

void ResetLaunchOverlay(ModalState& modal, float& loading_transition) {
    modal.launch_overlay_visible = false;
    modal.launch_overlay_aux = false;
    modal.launch_started_ms = 0;
    modal.launch_overlay_params[0] = 1.0f;
    modal.launch_overlay_params[1] = 450.0f;
    modal.launch_overlay_params[2] = 350.0f;
    modal.launch_overlay_params[3] = 0.0f;
    modal.launch_overlay_alpha = 0.0f;
    loading_transition = 0.0f;
}

static_assert(sizeof(UiState) == 0x238);
static_assert(offsetof(UiState, modal) == 0xc0);
static_assert(offsetof(UiState, modal) + offsetof(ModalState, cheat_id) == 0xe8);
static_assert(offsetof(UiState, modal) + offsetof(ModalState, changelog_blocks) == 0x1a0);
static_assert(offsetof(UiGpuResources, reserved_038) == 0x38);

}  // namespace

void DrawGameModal(Theme& theme, UiState& state, const UiGpuResources& gpu,
                   nl::data::CheatStore& store, const FrameInput& input,
                   ModalActions& actions) {
    ModalState& modal = Modal(state);
    if (!modal.open || !store.Find("cs2"))
        return;

    float& loading_transition = StateField<float>(state, 0x0c);
    float& launch_overlay_target = StateField<float>(state, 0x234);
    bool& profile_overlay_open = StateField<bool>(state, 0x200);
    std::string& launch_status = StateField<std::string>(state, 0x208);
    bool& launch_cancellable = StateField<bool>(state, 0x220);
    bool& launch_error = StateField<bool>(state, 0x221);
    bool& launch_ready = StateField<bool>(state, 0x222);
    std::uint64_t& completion_started_ms = StateField<std::uint64_t>(state, 0x228);
    bool& need_restart_admin = StateField<bool>(state, 0x230);

    const float master_alpha = state.primary_anim;
    loading_transition = Approach(loading_transition,
                                  modal.launch_overlay_visible ? 1.0f : 0.0f,
                                  11.0f, input.dt);
    const float overlay_target = std::clamp(launch_overlay_target, 0.0f, 1.0f);
    modal.launch_overlay_alpha = Approach(modal.launch_overlay_alpha,
                                          overlay_target, 9.0f, input.dt);
    modal.changelog_alpha = Approach(modal.changelog_alpha,
                                     modal.changelog_open ? 1.0f : 0.0f,
                                     15.0f, input.dt);

    const float screen_w = static_cast<float>(input.width);
    const float screen_h = static_cast<float>(input.height);
    const ImVec2 main_size(screen_w - 231.0f, screen_h - 89.0f);
    ImGui::SetNextWindowPos(ImVec2(219.0f, 77.0f), 0, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(main_size, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##GamePage", nullptr, 0x20af);
    if (!profile_overlay_open)
        BringCurrentWindowToFront();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float alpha = std::max(0.0f, 1.0f - 1.06f * loading_transition) *
                        master_alpha;
    const ImVec2 header_max(origin.x + main_size.x, origin.y + 43.0f);
    const float header_fill_alpha = (shell::GlassEnabled ? 0.28f : 1.0f) * alpha;
    dl->AddRectFilled(origin, header_max,
                      ImGui::GetColorU32(ImVec4(0.11f, 0.11f, 0.15f,
                                                header_fill_alpha)),
                      17.0f, 0);
    dl->AddRect(origin, header_max,
                ImGui::GetColorU32(ImVec4(0.22f, 0.32f, 0.53f, 0.55f * alpha)),
                17.0f, 0, 1.1f);

    const ImU32 blue = ImGui::GetColorU32(ImVec4(0.255f, 0.455f, 0.985f, alpha));
    const ImVec2 info_center(origin.x + 22.0f, origin.y + 21.0f);
    if (shell::IconAtlas) {
        dl->AddImage(shell::IconAtlas,
                     ImVec2(info_center.x - 8.5f, info_center.y - 8.5f),
                     ImVec2(info_center.x + 8.5f, info_center.y + 8.5f),
                     ImVec2(0.875f, 0.0f), ImVec2(1.0f, 1.0f), blue);
    } else {
        dl->AddCircleFilled(info_center, 7.5f, blue, 0);
        dl->AddText(theme.font_semibold, 13.0f,
                    ImVec2(origin.x + 20.0f, origin.y + 14.0f),
                    ImGui::GetColorU32(ImVec4(0.1f, 0.15f, 0.25f, alpha)), "i");
    }
    dl->AddText(theme.font_semibold, 18.0f,
                ImVec2(origin.x + 42.0f, origin.y + 12.0f),
                TextColor(theme, alpha), "Available");

    const ImVec2 content_base(origin.x, origin.y + 62.0f);
    if (gpu.cs2) {
        dl->AddImageRounded(gpu.cs2,
                            ImVec2(content_base.x + 15.0f, content_base.y + 8.0f),
                            ImVec2(content_base.x + 54.0f, content_base.y + 47.0f),
                            ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                            TextColor(theme, alpha), 4.0f, 0);
    }
    dl->AddText(theme.font_semibold, 18.5f,
                ImVec2(content_base.x + 63.0f, content_base.y + 22.0f),
                TextColor(theme, alpha), "Counter-Strike 2");

    const char* const values[] = {
        modal.displayed_updated.c_str(),
        modal.displayed_subscription.c_str(),
        modal.displayed_last_launch.c_str(),
        "Release",
    };
    const char* const labels[] = {"Updated", "Subscription", "Last Launch", "Branch"};
    const float right = origin.x + main_size.x;
    for (std::size_t i = 0; i < 4; ++i) {
        const float row_y = content_base.y + 84.0f + static_cast<float>(i) * 41.0f;
        const ImVec2 icon_center(origin.x + 23.0f, row_y + 9.0f);
        const ImU32 icon_color = ImGui::GetColorU32(
            ImVec4(0.67f, 0.66f, 0.7f, 0.95f * alpha));
        switch (i) {
            case 0: shell::Refresh(dl, icon_center, icon_color); break;
            case 1: shell::History(dl, icon_center, icon_color); break;
            case 2: shell::Hourglass(dl, icon_center, icon_color); break;
            default: shell::Branch(dl, icon_center, icon_color); break;
        }
        dl->AddText(theme.font_body, 17.0f,
                    ImVec2(origin.x + 42.0f, row_y),
                    ImGui::GetColorU32(ImVec4(0.67f, 0.66f, 0.7f, alpha)),
                    labels[i]);
        if (i == 3)
            break;

        const char* value = values[i];
        if (i18n::Russian())
            value = i18n::T(values[i]);
        const float margin = i == 0 ? 59.0f : 13.0f;
        const float value_width = theme.font_body->CalcTextSizeA(
            17.0f, 1000.0f, 0.0f, value).x;
        dl->AddText(theme.font_body, 17.0f,
                    ImVec2(right - margin - value_width, row_y + 1.0f),
                    TextColor(theme, alpha), value);
    }

    const bool enabled = loading_transition < 0.01f &&
                         !modal.launch_overlay_visible &&
                         !profile_overlay_open && master_alpha > 0.95f &&
                         modal.changelog_alpha < 0.01f && !modal.changelog_open;
    const ImVec4 button_color(0.17f, 0.17f, 0.22f, 1.0f);

    const ImVec2 history_pos(right - 52.0f, content_base.y + 72.0f);
    if (shell::Button(dl, "##UpdateHistory", history_pos, ImVec2(39.0f, 39.0f),
                      button_color, alpha, enabled, true)) {
        modal.changelog_blur_captured = false;
        modal.branch_open = false;
        modal.changelog_open = true;
        modal.branch_alpha = 0.0f;
    }
    shell::History(dl, ImVec2(history_pos.x + 19.5f, history_pos.y + 19.5f),
                   TextColor(theme, alpha));

    const ImVec2 branch_pos(right - 116.0f, content_base.y + 194.0f);
    if (shell::Button(dl, "##Branch", branch_pos, ImVec2(103.0f, 37.0f),
                      button_color, alpha, enabled, false)) {
        modal.branch_open = !modal.branch_open;
    }
    const ImU32 branch_blue = ImGui::GetColorU32(
        ImVec4(0.255f, 0.455f, 0.985f, alpha));
    shell::Share(dl, ImVec2(branch_pos.x + 18.0f, branch_pos.y + 18.5f),
                 branch_blue);
    dl->AddText(theme.font_body, 17.0f,
                ImVec2(branch_pos.x + 37.0f, branch_pos.y + 9.0f),
                branch_blue, "Release");

    modal.branch_alpha = Approach(modal.branch_alpha,
                                  modal.branch_open ? 1.0f : 0.0f,
                                  18.0f, input.dt);
    const ImVec2 launch_pos(origin.x + (main_size.x - 178.0f) * 0.5f + 47.0f,
                            screen_h - 68.0f);
    const bool launch_enabled = enabled && !modal.branch_open;
    if (shell::Button(dl, "##Launch", launch_pos, ImVec2(131.0f, 38.0f),
                      shell::Blue(), alpha, launch_enabled, true)) {
        modal.launch_started_ms = 0;
        modal.launch_overlay_params[0] = 1.0f;
        modal.launch_overlay_params[1] = 450.0f;
        modal.launch_overlay_params[2] = 350.0f;
        modal.launch_overlay_params[3] = 0.0f;
        modal.launch_overlay_alpha = 0.0f;
        modal.launch_overlay_visible = true;
        modal.launch_overlay_aux = false;
        modal.launch_started_ms = input.tick_ms;
        actions.start = true;
    }

    const ImVec2 launch_icon(launch_pos.x + 19.0f, launch_pos.y + 19.0f);
    const ImU32 launch_text_color = TextColor(theme, alpha);
    if (shell::IconAtlas) {
        dl->AddImage(shell::IconAtlas,
                     ImVec2(launch_icon.x - 7.2f, launch_icon.y - 7.2f),
                     ImVec2(launch_icon.x + 7.2f, launch_icon.y + 7.2f),
                     ImVec2(0.75f, 0.0f), ImVec2(0.875f, 1.0f),
                     launch_text_color);
    } else {
        dl->AddTriangleFilled(ImVec2(launch_icon.x - 3.9f, launch_icon.y - 6.0f),
                              ImVec2(launch_icon.x + 6.0f, launch_icon.y),
                              ImVec2(launch_icon.x - 3.9f, launch_icon.y + 6.0f),
                              launch_text_color);
    }
    dl->AddText(theme.font_body, 18.0f,
                ImVec2(launch_pos.x + 39.0f, launch_pos.y + 9.0f),
                launch_text_color, "Launch");

    const ImVec2 hardware_pos(launch_pos.x - 47.0f, launch_pos.y);
    const ImVec2 hardware_size(39.0f, 38.0f);
    ImGui::SetCursorScreenPos(hardware_pos);
    ImGui::BeginDisabled(true);
    ImGui::InvisibleButton("##UnavailableHardware", hardware_size, 0);
    ImGui::EndDisabled();
    const bool hardware_hovered =
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    float hardware_alpha = 0.45f * alpha * (hardware_hovered ? 1.0f : 0.95f);
    if (shell::GlassEnabled)
        hardware_alpha *= 0.74f;
    dl->AddRectFilled(hardware_pos,
                      ImVec2(hardware_pos.x + hardware_size.x,
                             hardware_pos.y + hardware_size.y),
                      ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.15f,
                                                hardware_alpha)),
                      11.0f, 0);
    const ImVec4 hardware_icon_color =
        hardware_hovered ? ImVec4(1.0f, 1.0f, 1.0f, alpha)
                         : ImVec4(0.56f, 0.57f, 0.65f, 0.38f * alpha);
    shell::Computer(dl, ImVec2(hardware_pos.x + 19.0f, hardware_pos.y + 19.0f),
                    ImGui::GetColorU32(hardware_icon_color));

    if (modal.branch_alpha > 0.01f) {
        const float choice_alpha = alpha * modal.branch_alpha;
        const ImVec2 choice_pos(
            branch_pos.x, branch_pos.y + 41.0f + (1.0f - modal.branch_alpha) * 5.0f);
        if (shell::Button(dl, "##ReleaseChoice", choice_pos, ImVec2(103.0f, 30.0f),
                          ImVec4(0.18f, 0.19f, 0.25f, 1.0f), choice_alpha,
                          enabled, true)) {
            modal.branch_open = false;
        }
        dl->AddText(theme.font_body, 16.0f,
                    ImVec2(choice_pos.x + 16.0f, choice_pos.y + 7.0f),
                    ImGui::GetColorU32(ImVec4(0.255f, 0.455f, 0.985f,
                                              choice_alpha)),
                    "Release");
        if (enabled && ImGui::IsMouseClicked(0, false) && !ImGui::IsItemHovered(0) &&
            !ImGui::IsMouseHoveringRect(
                branch_pos, ImVec2(branch_pos.x + 103.0f, branch_pos.y + 37.0f), true)) {
            modal.branch_open = false;
        }
    }

    if (modal.changelog_alpha > 0.01f) {
        dl->PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), false);
        gfx::Blur* blur = reinterpret_cast<gfx::Blur*>(gpu.reserved_038);
        if (blur) {
            if (!modal.changelog_blur_captured)
                *(reinterpret_cast<unsigned char*>(blur) + 0xaa) = 0;
            const bool force = !modal.changelog_blur_captured ||
                               modal.changelog_alpha < 0.995f;
            if (QueueBlurCapture(*blur, dl, force)) {
                BlitBlurSolid(*blur, dl, 0.0f, 0.0f, screen_w, screen_h,
                              modal.changelog_alpha, screen_w, screen_h, 17.0f);
                modal.changelog_blur_captured = true;
            }
        }
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h),
                          ImGui::GetColorU32(ImVec4(0.015f, 0.02f, 0.035f,
                                                    0.5f * modal.changelog_alpha)),
                          17.0f, 0);
        dl->PopClipRect();
    } else {
        modal.changelog_blur_captured = false;
    }
    ImGui::End();
    ImGui::PopStyleVar(1);

    if (loading_transition > 0.001f) {
        ImGui::SetNextWindowPos(ImVec2(219.0f, 77.0f), 0, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(main_size, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##LoadingOverlay", nullptr, 0x1af);
        BringCurrentWindowToFront();

        const float overlay_alpha = master_alpha * loading_transition;
        ImDrawList* overlay_dl = ImGui::GetWindowDrawList();
        const ImVec2 overlay_pos = ImGui::GetWindowPos();
        const ImVec2 overlay_size = ImGui::GetWindowSize();
        const float center_x = overlay_pos.x + overlay_size.x * 0.5f;
        const ImVec2 overlay_max(overlay_pos.x + overlay_size.x,
                                 overlay_pos.y + overlay_size.y);
        const float overlay_fill = (shell::GlassEnabled ? 0.96f : 1.0f) * overlay_alpha;
        overlay_dl->AddRectFilled(overlay_pos, overlay_max,
                                  ImGui::GetColorU32(ImVec4(0.078f, 0.083f, 0.116f,
                                                            overlay_fill)),
                                  17.0f, 0);
        overlay_dl->AddRect(overlay_pos, overlay_max,
                            ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.48f,
                                                      0.15f * overlay_alpha)),
                            17.0f, 0, 1.0f);

        const std::uint64_t launch_elapsed = input.tick_ms - modal.launch_started_ms;
        const float pulse = 0.5f - 0.5f * cosf(
            static_cast<float>(launch_elapsed % 1800) * 6.28319f / 1800.0f);
        if (gpu.cs2) {
            const float image_size = 40.0f + 1.5f * pulse;
            const float image_gray = 0.86f + 0.14f * pulse;
            overlay_dl->AddImageRounded(
                gpu.cs2,
                ImVec2(center_x - image_size * 0.5f, overlay_pos.y + 85.0f),
                ImVec2(center_x + image_size * 0.5f, overlay_pos.y + 85.0f + image_size),
                ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImGui::GetColorU32(ImVec4(image_gray, image_gray, image_gray,
                                          overlay_alpha)),
                5.0f, 0);
        }

        const char* const game_name = "Counter-Strike 2";
        const float game_name_width = theme.font_semibold->CalcTextSizeA(
            20.0f, 1000.0f, 0.0f, game_name).x;
        overlay_dl->AddText(theme.font_semibold, 20.0f,
                            ImVec2(center_x - game_name_width * 0.5f,
                                   overlay_pos.y + 134.0f),
                            TextColor(theme, overlay_alpha), game_name);

        const ImVec2 track_min(center_x - 85.0f, overlay_pos.y + 173.0f);
        const ImVec2 track_max(track_min.x + 170.0f, track_min.y + 4.0f);
        overlay_dl->AddRectFilled(track_min, track_max,
                                  ImGui::GetColorU32(ImVec4(0.22f, 0.23f, 0.29f,
                                                            overlay_alpha)),
                                  2.0f, 0);
        if (launch_ready) {
            overlay_dl->AddRectFilled(
                track_min, track_max,
                ImGui::GetColorU32(ImVec4(0.255f, 0.455f, 0.985f, overlay_alpha)),
                2.0f, 0);
            const char* const ready = "Ready";
            const float ready_width = theme.font_small->CalcTextSizeA(
                14.0f, 1000.0f, 0.0f, ready).x;
            overlay_dl->AddText(theme.font_small, 14.0f,
                                ImVec2(center_x - ready_width * 0.5f,
                                       overlay_pos.y + 194.0f),
                                ImGui::GetColorU32(ImVec4(0.56f, 0.57f, 0.65f,
                                                          overlay_alpha)),
                                ready);
        } else if (!launch_error) {
            const float phase = static_cast<float>(launch_elapsed % 1500) / 1500.0f;
            const float segment_center = track_min.x - 68.0f + phase * 238.0f;
            const float segment_min = std::max(segment_center, track_min.x);
            const float segment_max = std::min(segment_center + 68.0f, track_max.x);
            if (segment_max > segment_min) {
                overlay_dl->AddRectFilled(
                    ImVec2(segment_min, track_min.y), ImVec2(segment_max, track_max.y),
                    ImGui::GetColorU32(ImVec4(0.255f, 0.455f, 0.985f,
                                              overlay_alpha)),
                    2.0f, 0);
            }
        }

        if (launch_error) {
            ImGui::SetCursorScreenPos(ImVec2(overlay_pos.x + 22.0f,
                                             overlay_pos.y + 199.0f));
            ImGui::PushTextWrapPos(overlay_size.x - 22.0f);
            ImGui::PushFont(theme.font_small);
            ImGui::TextColored(ImVec4(1.0f, 0.62f, 0.55f, overlay_alpha),
                               "%s", launch_status.c_str());
            ImGui::PopFont();
            ImGui::PopTextWrapPos();

            ImGui::SetCursorScreenPos(ImVec2(center_x - 60.0f, overlay_max.y - 48.0f));
            const bool back = ImGui::Button("Back", ImVec2(120.0f, 32.0f));
            if (back || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                ResetLaunchOverlay(modal, loading_transition);

            if (need_restart_admin) {
                ImGui::SetCursorScreenPos(ImVec2(center_x - 142.0f,
                                                 overlay_max.y - 88.0f));
                if (ImGui::Button("Restart as admin", ImVec2(284.0f, 32.0f)))
                    actions.restart_admin = true;
            }
        } else {
            const float status_y = overlay_max.y - 106.0f;
            const char* const launching = "Launching Steam";
            const float launching_width = theme.font_body->CalcTextSizeA(
                15.0f, 1000.0f, 0.0f, launching).x;
            const ImU32 status_color = ImGui::GetColorU32(
                ImVec4(0.56f, 0.57f, 0.65f, overlay_alpha));
            overlay_dl->AddText(theme.font_body, 15.0f,
                                ImVec2(center_x - launching_width * 0.5f, status_y),
                                status_color, launching);
            const char* const initializing = "Initializing game";
            const float initializing_width = theme.font_body->CalcTextSizeA(
                15.0f, 1000.0f, 0.0f, initializing).x;
            overlay_dl->AddText(theme.font_body, 15.0f,
                                ImVec2(center_x - initializing_width * 0.5f,
                                       status_y + 26.0f),
                                status_color, initializing);
            if (launch_ready) {
                const std::uint64_t elapsed = input.tick_ms >= completion_started_ms
                    ? input.tick_ms - completion_started_ms : 0;
                const float elapsed_f = static_cast<float>(elapsed);
                const float expand_t = std::clamp((elapsed_f - 360.0f) / 1700.0f,
                                                  0.0f, 1.0f);
                const float expand = expand_t * expand_t * (3.0f - 2.0f * expand_t);
                const float fade_t = std::min(elapsed_f / 360.0f, 1.0f);
                const float remaining = powf(1.0f - fade_t, 3.0f);
                const float fade = 1.0f - remaining;
                const float card_alpha = overlay_alpha * fade;
                const float scale = 0.97f + 0.03f * fade;
                const float inset = 9.0f + overlay_size.x * (1.0f - scale) * 0.5f;
                const float y_shift = 9.0f * remaining;
                const float unexpanded = 1.0f - expand;

                const ImVec2 card_min(overlay_pos.x + inset,
                                      overlay_max.y - 42.0f + y_shift);
                const ImVec2 card_max(overlay_max.x - inset,
                                      overlay_max.y - 8.0f + y_shift);
                overlay_dl->AddRectFilled(
                    ImVec2(card_min.x, card_min.y + 2.0f),
                    ImVec2(card_max.x, card_max.y + 2.0f),
                    ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.16f * card_alpha)),
                    10.0f, 0);
                overlay_dl->AddRectFilled(
                    card_min, card_max,
                    ImGui::GetColorU32(ImVec4(0.09f - 0.03f * unexpanded,
                                              0.095f + 0.065f * unexpanded,
                                              0.12f - 0.035f * unexpanded,
                                              0.85f * card_alpha)),
                    10.0f, 0);
                overlay_dl->AddRect(
                    card_min, card_max,
                    ImGui::GetColorU32(ImVec4(0.28f + 0.07f * unexpanded,
                                              0.29f + 0.31f * unexpanded,
                                              0.35f - 0.1f * unexpanded,
                                              (0.13f + 0.22f * unexpanded) * card_alpha)),
                    10.0f, 0, 1.0f);

                const ImVec2 mark_center(card_min.x + 19.0f, card_min.y + 17.0f);
                overlay_dl->AddCircleFilled(
                    mark_center, 7.5f,
                    ImGui::GetColorU32(ImVec4(0.53f, 0.83f, 0.28f, card_alpha)), 0);
                const ImU32 mark_color = ImGui::GetColorU32(
                    ImVec4(0.13f, 0.25f, 0.1f, card_alpha));
                overlay_dl->AddLine(ImVec2(mark_center.x - 3.0f, mark_center.y),
                                    ImVec2(mark_center.x - 1.0f, mark_center.y + 2.0f),
                                    mark_color, 1.5f);
                overlay_dl->AddLine(ImVec2(mark_center.x - 1.0f, mark_center.y + 2.0f),
                                    ImVec2(mark_center.x + 3.0f, mark_center.y - 2.0f),
                                    mark_color, 1.5f);
                overlay_dl->AddText(theme.font_body, 15.0f,
                                    ImVec2(card_min.x + 36.0f, card_min.y + 9.0f),
                                    TextColor(theme, card_alpha),
                                    "Initialization completed");
            }
            if (launch_cancellable && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                actions.cancel = true;
        }
        ImGui::End();
        ImGui::PopStyleVar(1);
    }

    if (modal.changelog_alpha < 0.01f)
        return;

    const float changelog_scale = 0.94f + 0.06f * modal.changelog_alpha;
    const ImVec2 changelog_size(420.0f * changelog_scale, 240.0f * changelog_scale);
    const ImVec2 changelog_pos((screen_w - changelog_size.x) * 0.5f,
                               (screen_h - changelog_size.y) * 0.5f);
    modal.changelog_drawn = true;
    ImGui::SetNextWindowPos(changelog_pos, 0, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(changelog_size, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##LatestChangelog", nullptr, 0x1af);
    BringCurrentWindowToFront();

    ImDrawList* log_dl = ImGui::GetWindowDrawList();
    const ImVec2 changelog_max(changelog_pos.x + changelog_size.x,
                               changelog_pos.y + changelog_size.y);
    // 阴影按设计要溢出面板边界，必须放开窗口方角裁剪，否则会在圆角处被切成硬边。
    log_dl->PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), false);
    log_dl->AddRectFilled(ImVec2(changelog_pos.x - 4.0f, changelog_pos.y - 4.0f),
                          ImVec2(changelog_max.x + 4.0f, changelog_max.y + 4.0f),
                          ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f,
                                                    0.28f * modal.changelog_alpha)),
                          19.0f, 0);
    const float log_fill = (shell::GlassEnabled ? 0.95f : 1.0f) *
                           modal.changelog_alpha;
    log_dl->AddRectFilled(changelog_pos, changelog_max,
                          ImGui::GetColorU32(ImVec4(0.078f, 0.083f, 0.116f, log_fill)),
                          15.0f, 0);
    log_dl->AddRect(changelog_pos, changelog_max,
                    ImGui::GetColorU32(ImVec4(0.6f, 0.63f, 0.75f,
                                              0.18f * modal.changelog_alpha)),
                    15.0f, 0, 1.0f);
    log_dl->PopClipRect();
    log_dl->AddText(theme.font_semibold, 19.0f,
                    ImVec2(changelog_pos.x + 19.0f, changelog_pos.y + 16.0f),
                    TextColor(theme, modal.changelog_alpha), "Latest update");
    log_dl->AddText(theme.font_small, 13.0f,
                    ImVec2(changelog_pos.x + 19.0f, changelog_pos.y + 45.0f),
                    ImGui::GetColorU32(ImVec4(0.56f, 0.57f, 0.65f,
                                              modal.changelog_alpha)),
                    modal.displayed_updated.c_str());

    const ImVec2 close_pos(changelog_max.x - 38.0f, changelog_pos.y + 9.0f);
    ImGui::SetCursorScreenPos(close_pos);
    const bool close_enabled = !modal.changelog_open;
    ImGui::BeginDisabled(close_enabled);
    bool close = ImGui::InvisibleButton("##CloseChangeLogs", ImVec2(29.0f, 29.0f), 0);
    if (!close_enabled && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();

    const ImU32 close_color = ImGui::GetColorU32(
        ImVec4(0.56f, 0.57f, 0.65f, modal.changelog_alpha));
    const float close_x = changelog_max.x - 24.0f;
    const float close_y = changelog_pos.y + 24.0f;
    log_dl->AddLine(ImVec2(close_x - 4.0f, close_y - 4.0f),
                    ImVec2(close_x + 4.0f, close_y + 4.0f), close_color, 1.6f);
    log_dl->AddLine(ImVec2(close_x + 4.0f, close_y - 4.0f),
                    ImVec2(close_x - 4.0f, close_y + 4.0f), close_color, 1.6f);

    ImGui::SetCursorScreenPos(ImVec2(changelog_pos.x + 19.0f,
                                     changelog_pos.y + 74.0f));
    ImGui::BeginChild("##LatestUpdateText",
                      ImVec2(changelog_size.x - 38.0f, changelog_size.y - 89.0f),
                      0, 0x80);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, modal.changelog_alpha);
    ImGui::PushFont(theme.font_small);
    if (!modal.changelog_blocks.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        for (const std::string& line : modal.changelog_blocks.front().lines)
            ImGui::TextUnformatted(line.c_str());
        ImGui::PopTextWrapPos();
    } else {
        ImGui::TextUnformatted("No update notes available.");
    }
    ImGui::PopFont();
    ImGui::PopStyleVar(1);
    ImGui::EndChild();

    if (modal.changelog_open && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        close = true;
    if (modal.changelog_alpha > 0.95f && ImGui::IsMouseClicked(0, false) &&
        !ImGui::IsMouseHoveringRect(changelog_pos, changelog_max, true)) {
        close = true;
    }
    if (close)
        modal.changelog_open = false;

    ImGui::End();
    ImGui::PopStyleVar(1);
}

}  // namespace nl::ui
