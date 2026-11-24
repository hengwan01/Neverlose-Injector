#include "ui_types.h"

#include "data_store.h"
#include "i18n.h"
#include "ui_shell.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <string>
#include <utility>

namespace nl::ui {
namespace {

ImU32 Color(float r, float g, float b, float a) {
    return ImGui::GetColorU32(ImVec4(r, g, b, a));
}

ImU32 TextColor(const Theme& theme, float opacity) {
    return ImGui::GetColorU32(
        ImVec4(theme.text.x, theme.text.y, theme.text.z,
               theme.text.w * opacity));
}

void Animate(float& value, float target, float rate, float dt) {
    const float capped = std::min(0.1f, dt);
    const float exponent = dt < 0.0f ? -0.0f : -rate * capped;
    value += (1.0f - ::expf(exponent)) * (target - value);
}

std::string FormatClockDate(long long value) {
    const __time64_t timestamp = static_cast<__time64_t>(value);
    tm local{};
    if (_localtime64_s(&local, &timestamp) != 0)
        return "Date formatting error";

    char buffer[80];
    if (std::strftime(buffer, sizeof(buffer), "%d.%m.%Y (%H:%M)", &local) == 0)
        return "Date formatting error";
    return buffer;
}

std::string FormatDate(long long value, long long now) {
    if (value == 0)
        return "Never";

    const __time64_t now_timestamp = static_cast<__time64_t>(now);
    tm now_local{};
    if (_localtime64_s(&now_local, &now_timestamp) != 0)
        return "Date formatting error";

    const __time64_t value_timestamp = static_cast<__time64_t>(value);
    tm value_local{};
    if (_localtime64_s(&value_local, &value_timestamp) != 0)
        return "Date formatting error";

    if (now_local.tm_year == value_local.tm_year &&
        now_local.tm_mon == value_local.tm_mon &&
        now_local.tm_mday == value_local.tm_mday)
        return "Today";

    tm yesterday = now_local;
    --yesterday.tm_mday;
    yesterday.tm_isdst = -1;
    if (_mktime64(&yesterday) != static_cast<__time64_t>(-1) &&
        yesterday.tm_year == value_local.tm_year &&
        yesterday.tm_mon == value_local.tm_mon &&
        yesterday.tm_mday == value_local.tm_mday)
        return "Yesterday";

    char buffer[80];
    if (std::strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", &value_local) == 0)
        return "Date formatting error";
    return buffer;
}

std::string AgoElapsed(long long amount, bool hours) {
    if (i18n::Russian())
        return std::to_string(amount) + (hours ? " ч назад" : " мин назад");

    if (hours)
        return std::to_string(amount) + (amount == 1 ? " hour ago" : " hours ago");
    return std::to_string(amount) + (amount == 1 ? " minute ago" : " minutes ago");
}

}  // namespace

void FillDisplayedBranch(UiState& state, const nl::data::CheatInfo& cheat,
                         long long now) {
    const nl::data::BranchInfo* branch = cheat.ActiveBranch();
    state.modal.displayed_branch = "Release";

    if (!branch || branch->build_date == 0)
        state.modal.displayed_updated = "Never";
    else
        state.modal.displayed_updated = FormatClockDate(branch->build_date);

    if (cheat.lifetime) {
        state.modal.displayed_subscription = "Lifetime";
    } else {
        const long long license = static_cast<long long>(cheat.license);
        state.modal.displayed_subscription = license == 0 ? "Never" : FormatClockDate(license);
    }

    if (!branch || branch->last_launch == 0) {
        state.modal.displayed_last_launch = "Never";
    } else {
        const long long delta = now - branch->last_launch;
        if (delta <= 59)
            state.modal.displayed_last_launch = "Just now";
        else if (delta <= 3599)
            state.modal.displayed_last_launch = AgoElapsed(std::max(delta, 0LL) / 60, false);
        else if (delta <= 86399)
            state.modal.displayed_last_launch = AgoElapsed(std::max(delta, 0LL) / 3600, true);
        else if (delta <= 172799)
            state.modal.displayed_last_launch = "Yesterday";
        else
            state.modal.displayed_last_launch = FormatDate(branch->last_launch, now);
    }

    state.modal.changelog_blocks.clear();
    if (!branch)
        return;

    for (const nl::data::ChangelogEntry& entry : branch->changelog) {
        ModalState::DisplayedChangelogBlock block;
        block.date = FormatDate(entry.date,
                                static_cast<long long>(std::time(nullptr)));
        std::istringstream stream(entry.text, std::ios::in);
        std::string line;
        while (std::getline(stream, line))
            block.lines.push_back(line);
        state.modal.changelog_blocks.push_back(std::move(block));
    }
}

void DrawDashboard(Theme& theme, UiState& state, const UiGpuResources& gpu,
                   nl::data::CheatStore& store, float opacity,
                   const FrameInput& input, DashActions& actions) {
    state.modal.changelog_drawn = false;
    Animate(state.primary_anim, 1.0f, 12.0f, input.dt);

    if (!state.modal.open) {
        state.modal.open = true;
        state.modal.cheat_id = "cs2";
        const nl::data::CheatInfo* cheat = store.Find("cs2");
        if (cheat)
            FillDisplayedBranch(state, *cheat, input.unix_time);
    }

    const float width = static_cast<float>(input.width);
    const float height = static_cast<float>(input.height);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), 0, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(width, height), 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("MainWindow", nullptr, 0x20bf);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(width, height),
                        Color(0.145f, 0.153f, 0.212f,
                              (shell::GlassEnabled ? 0.34f : 1.0f) * opacity),
                        17.0f, 0);
    draw->AddRectFilled(ImVec2(205.0f, 0.0f), ImVec2(width, height),
                        Color(0.078f, 0.082f, 0.114f,
                              (shell::GlassEnabled ? 0.34f : 1.0f) * opacity),
                        17.0f, 0);
    draw->AddLine(ImVec2(205.0f, 17.0f), ImVec2(205.0f, height - 17.0f),
                  Color(0.1f, 0.11f, 0.15f, 0.22f * opacity), 1.0f);

    if (gpu.logo) {
        draw->AddImage(gpu.logo, ImVec2(17.0f, 9.0f), ImVec2(56.0f, 48.0f),
                       ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       Color(1.0f, 1.0f, 1.0f, opacity));
    } else {
        shell::Logo(draw, ImVec2(17.0f, 9.0f), theme.text, opacity);
    }

    draw->AddText(theme.font_semibold, 18.5f, ImVec2(65.0f, 21.0f),
                  TextColor(theme, opacity), "Neverlose");

    const ImU32 separator = Color(0.35f, 0.35f, 0.4f, 0.17f * opacity);
    draw->AddLine(ImVec2(11.0f, 61.0f), ImVec2(193.0f, 61.0f),
                  separator, 1.0f);
    draw->AddLine(ImVec2(216.0f, 61.0f), ImVec2(width - 9.0f, 61.0f),
                  separator, 1.0f);
    draw->AddText(theme.font_body, 12.0f, ImVec2(21.0f, 74.0f),
                  Color(0.56f, 0.57f, 0.65f, opacity), "SUBSCRIPTIONS");

    const bool transition_done = !state.modal.changelog_open && state.modal.changelog_alpha <= 0.01f;
    const bool interactive = transition_done && !state.settings_open &&
                             !state.modal.launch_overlay_visible;

    ImGui::SetCursorScreenPos(ImVec2(12.0f, 93.0f));
    ImGui::BeginDisabled(!interactive);
    const bool software_pressed =
        ImGui::InvisibleButton("##SoftwareCs2", ImVec2(179.0f, 38.0f), 0);
    if (interactive && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();
    if (software_pressed)
        state.modal.changelog_open = false;

    Animate(state.secondary_anim, 1.0f, 14.0f, input.dt);

    draw->AddRectFilled(ImVec2(12.0f, 93.0f), ImVec2(191.0f, 131.0f),
                        Color(0.24f, 0.24f, 0.29f,
                              (shell::GlassEnabled ? 0.7f : 1.0f) * opacity),
                        10.0f, 0);
    if (gpu.cs2) {
        draw->AddImageRounded(gpu.cs2, ImVec2(18.0f, 99.0f),
                              ImVec2(42.0f, 123.0f), ImVec2(0.0f, 0.0f),
                              ImVec2(1.0f, 1.0f), TextColor(theme, opacity),
                              3.0f, 0);
    }
    draw->AddText(theme.font_body, 16.5f, ImVec2(50.0f, 104.0f),
                  TextColor(theme, opacity), "Counter-Strike 2");

    draw->AddLine(ImVec2(12.0f, height - 77.0f),
                  ImVec2(193.0f, height - 77.0f), separator, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(11.0f, height - 65.0f));
    ImGui::BeginDisabled(!interactive);
    const bool profile_pressed =
        ImGui::InvisibleButton("##ProfileCard", ImVec2(183.0f, 54.0f), 0);
    if (interactive && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();
    if (profile_pressed)
        state.settings_open = true;

    Animate(state.profile_hover, ImGui::IsItemHovered(0) ? 1.0f : 0.0f,
            14.0f, input.dt);

    draw->AddRectFilled(ImVec2(11.0f, height - 65.0f),
                        ImVec2(194.0f, height - 11.0f),
                        Color(0.3f, 0.3f, 0.36f,
                              state.profile_hover * opacity * 0.22f),
                        10.0f, 0);
    if (gpu.avatar) {
        draw->AddImage(gpu.avatar, ImVec2(17.0f, height - 57.0f),
                       ImVec2(55.0f, height - 19.0f), ImVec2(0.0f, 0.0f),
                       ImVec2(1.0f, 1.0f), TextColor(theme, opacity));
    }

    draw->PushClipRect(ImVec2(65.0f, height - 54.0f),
                       ImVec2(193.0f, height - 20.0f), true);
    draw->AddText(theme.font_body, 17.0f, ImVec2(65.0f, height - 43.0f),
                  TextColor(theme, opacity), state.nickname.c_str());
    draw->PopClipRect();

    const bool api_enabled = transition_done && !state.settings_open;
    ImGui::SetCursorScreenPos(ImVec2(226.0f, 15.0f));
    ImGui::BeginDisabled(!api_enabled);
    const bool api_pressed =
        ImGui::InvisibleButton("##ApiDocs", ImVec2(202.0f, 31.0f), 0);
    if (api_enabled && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();
    if (api_pressed)
        actions.open_url = L"https://github.com/PbLJk/neverlose-lua-api";

    draw->AddText(theme.font_body, 18.5f, ImVec2(226.0f, 21.0f),
                  Color(theme.text.x, theme.text.y, theme.text.z,
                        0.86f * opacity * theme.text.w),
                  "API Documentation");

    ImGui::SetCursorScreenPos(ImVec2(width - 77.0f, 10.0f));
    ImGui::BeginDisabled(state.settings_open);
    const bool minimize_pressed =
        ImGui::InvisibleButton("##Minimize", ImVec2(30.0f, 32.0f), 0);
    if (!state.settings_open && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();
    if (minimize_pressed)
        actions.minimize = true;

    const ImU32 minimize_control = Color(
        theme.text.x, theme.text.y, theme.text.z,
        theme.text.w * (opacity * 0.85f));
    draw->AddLine(ImVec2(width - 67.0f, 28.0f),
                  ImVec2(width - 54.0f, 28.0f), minimize_control, 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(width - 42.0f, 10.0f));
    ImGui::BeginDisabled(state.settings_open);
    const bool close_pressed =
        ImGui::InvisibleButton("##Close", ImVec2(30.0f, 32.0f), 0);
    if (!state.settings_open && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();
    if (close_pressed)
        actions.close = true;

    const ImU32 close_control = Color(
        theme.text.x, theme.text.y, theme.text.z,
        theme.text.w * (opacity * 0.85f));
    draw->AddLine(ImVec2(width - 30.0f, 24.0f),
                  ImVec2(width - 22.0f, 32.0f), close_control, 2.0f);
    draw->AddLine(ImVec2(width - 22.0f, 24.0f),
                  ImVec2(width - 30.0f, 32.0f), close_control, 2.0f);

    ImGui::End();
    ImGui::PopStyleVar(1);
}

}  // namespace nl::ui
