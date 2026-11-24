#include "ui_settings.h"

#include "data_store.h"
#include "gfx_blur.h"
#include "i18n.h"
#include "ui_shell.h"

#include <algorithm>
#include <cstring>

namespace nl::ui {
namespace {

ImU32 Color(float r, float g, float b, float a) {
    return ImGui::GetColorU32(ImVec4(r, g, b, a));
}

ImVec2 At(const ImVec2& p, float x, float y) {
    return ImVec2(p.x + x, p.y + y);
}

bool RestartRequired(const Profile& before, const Profile& draft) {
    return before.nickname != draft.nickname ||
           before.avatar != draft.avatar ||
           before.expires_at != draft.expires_at;
}

}  // namespace

void DrawSettings(Theme& theme, UiState& state, SettingsState& settings,
                  const UiGpuResources& gpu, Profile& profile,
                  nl::data::CheatStore& store, gfx::Blur& blur,
                  const FrameInput& input, const SettingsRuntimeStatus& runtime,
                  SettingsActions& actions) {
    if (!state.settings_open) {
        settings.initialized = false;
        return;
    }

    if (!settings.initialized) {
        settings.draft = profile;
        settings.days = RemainingProfileDays(profile, input.unix_time);
        settings.days_edited = false;
        std::memset(settings.nickname, 0, sizeof(settings.nickname));
        const std::size_t nickname_size =
            std::min(settings.draft.nickname.size(), sizeof(settings.nickname) - 1);
        std::memcpy(settings.nickname, settings.draft.nickname.data(), nickname_size);
        settings.avatar = gpu.avatar;
        settings.error.clear();
        settings.initialized = true;
        settings.blur_captured = false;
        ImGui::OpenPopup("##ProfileSetting", 0);
    }

    // UiState+0x1e0 is the shared drag flag (named changelog_drawn here).
    state.modal.changelog_drawn = true;

    const float screen_w = static_cast<float>(input.width);
    const float screen_h = static_cast<float>(input.height);
    ImGui::SetNextWindowPos(ImVec2((screen_w - 450.0f) * 0.5f,
                                   (screen_h - 350.0f) * 0.5f),
                            0, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(450.0f, 350.0f), 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg,
        ImVec4(theme.card.x, theme.card.y, theme.card.z, 0.35f));
    ImGui::PushStyleColor(
        ImGuiCol_PopupBg,
        ImVec4(theme.card.x, theme.card.y, theme.card.z, 0.35f));
    ImGui::PushStyleColor(
        ImGuiCol_FrameBg,
        ImVec4(0.035f, 0.065f, 0.1f, shell::GlassEnabled ? 0.38f : 1.0f));

    const ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground;
    if (ImGui::BeginPopupModal("##ProfileSetting", nullptr, window_flags)) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 window_size = ImGui::GetWindowSize();
        const ImVec2 max(pos.x + window_size.x, pos.y + window_size.y);

        draw->PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), false);
        draw->AddRectFilled(At(pos, -3.0f, -3.0f), At(max, 3.0f, 3.0f),
                            Color(0.0f, 0.0f, 0.0f, 0.2f), 19.0f, 0);
        if (QueueBlurCapture(blur, draw, !settings.blur_captured)) {
            BlitBlurSolid(blur, draw, pos.x, pos.y, max.x, max.y, 1.0f,
                          screen_w, screen_h, 16.0f);
            settings.blur_captured = true;
        }
        draw->AddRectFilled(pos, max, Color(0.045f, 0.05f, 0.075f, 0.4f),
                            16.0f, 0);
        draw->AddRect(pos, max, Color(0.6f, 0.65f, 0.78f, 0.15f),
                      16.0f, 0, 1.0f);
        draw->PopClipRect();

        ImGui::PushFont(theme.font_semibold);
        ImGui::TextUnformatted(i18n::T("Settings"));
        ImGui::PopFont();
        ImGui::SameLine(350.0f, -1.0f);
        if (ImGui::SmallButton(i18n::T("Close")))
            state.settings_open = false;
        ImGui::Spacing();

        const ImVec2 avatar_pos = ImGui::GetCursorScreenPos();
        ImGui::BeginDisabled(runtime.busy);
        const bool edit_avatar =
            ImGui::InvisibleButton("##EditAvatar", ImVec2(60.0f, 60.0f), 0);
        const bool avatar_hovered = ImGui::IsItemHovered(0);
        if (avatar_hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::EndDisabled();

        const float hover_step = (avatar_hovered ? 10.0f : -10.0f) *
                                 ImGui::GetIO().DeltaTime;
        settings.avatar_hover = std::clamp(settings.avatar_hover + hover_step,
                                           0.0f, 1.0f);

        draw = ImGui::GetWindowDrawList();
        if (settings.avatar) {
            draw->AddImage(settings.avatar, avatar_pos, At(avatar_pos, 60.0f, 60.0f),
                           ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), 0xffffffffu);
        }

        if (settings.avatar_hover > 0.001f) {
            const ImVec2 center = At(avatar_pos, 30.0f, 30.0f);
            draw->AddCircleFilled(
                center, 30.0f,
                Color(0.0f, 0.0f, 0.0f, settings.avatar_hover * 0.55f), 48);
            const ImU32 pencil =
                Color(1.0f, 1.0f, 1.0f, settings.avatar_hover);
            draw->AddQuad(At(center, -8.0f, 4.0f), At(center, 4.0f, -8.0f),
                          At(center, 8.0f, -4.0f), At(center, -4.0f, 8.0f),
                          pencil, 1.8f);
            draw->AddTriangleFilled(At(center, -10.0f, 10.0f),
                                    At(center, -8.0f, 4.0f),
                                    At(center, -4.0f, 8.0f), pencil);
            draw->AddLine(At(center, 2.0f, -6.0f), At(center, 6.0f, -2.0f),
                          pencil, 1.8f);
        }

        if (edit_avatar)
            actions.choose_avatar(settings.draft, settings.avatar, settings.error);

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::SetCursorPosY(avatar_pos.y - ImGui::GetWindowPos().y + 18.0f);
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(334.0f);
        ImGui::InputText("##nickname", settings.nickname, sizeof(settings.nickname),
                         0, nullptr, nullptr);
        ImGui::EndGroup();

        ImGui::SetCursorPosY(150.0f);
        ImGui::TextUnformatted(i18n::T("Days"));
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::InputInt("##profile_days", &settings.days, 1, 100, 0)) {
            settings.days = std::clamp(settings.days, 0, 36500);
            settings.days_edited = true;
        }
        ImGui::SameLine(0.0f, -1.0f);
        ImGui::TextDisabled("%s", i18n::T("0 = Lifetime"));

        ImGui::SetCursorPosY(200.0f);
        ImGui::TextUnformatted(i18n::T("Appearance"));
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##appearance",
                              settings.draft.appearance == 1
                                  ? i18n::T("Liquid Glass")
                                  : (settings.draft.appearance == 2
                                         ? i18n::T("Solid")
                                         : i18n::T("Blur")))) {
            if (ImGui::Selectable(i18n::T("Blur"),
                                  settings.draft.appearance == 0))
                settings.draft.appearance = 0;
            if (ImGui::Selectable(i18n::T("Liquid Glass"),
                                  settings.draft.appearance == 1))
                settings.draft.appearance = 1;
            if (ImGui::Selectable(i18n::T("Solid"),
                                  settings.draft.appearance == 2))
                settings.draft.appearance = 2;
            ImGui::EndCombo();
        }

        ImGui::SetCursorPosY(262.0f);
        ImGui::PushFont(theme.font_small);
        ImGui::PushTextWrapPos(425.0f);
        if (!settings.error.empty()) {
            ImGui::TextUnformatted(i18n::T(settings.error.c_str()));
        } else if (runtime.visible) {
            ImGui::TextUnformatted(i18n::T(runtime.text.c_str()));
        }
        ImGui::PopTextWrapPos();
        ImGui::PopFont();

        ImGui::SetCursorPosY(302.0f);
        ImGui::BeginDisabled(runtime.busy);
        if (ImGui::Button(i18n::T("Save"), ImVec2(410.0f, 30.0f))) {
            settings.draft.nickname = settings.nickname;
            if (settings.days_edited)
                SetProfileDays(settings.draft, settings.days, input.unix_time);

            if (actions.save_profile(settings.draft, settings.error)) {
                const bool restart = RestartRequired(profile, settings.draft);
                profile = settings.draft;
                profile.language = "en";
                i18n::SetLanguage(profile.language);

                if (const nl::data::CheatInfo* cheat = store.Find("cs2"))
                    FillDisplayedBranch(state, *cheat, input.unix_time);
                state.nickname = profile.nickname;
                settings.blur_captured = false;
                actions.profile_saved(profile, restart);
            }
        }
        ImGui::EndDisabled();

        if (!state.settings_open)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    } else {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }
}

}  // namespace nl::ui
