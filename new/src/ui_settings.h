#pragma once

#include "profile.h"
#include "ui_types.h"

#include <functional>
#include <string>

namespace nl::gfx {
class Blur;
}

namespace nl::ui {

struct SettingsState {
    Profile draft;
    char nickname[256] = {};
    int days = 0;
    bool days_edited = false;
    std::string error;
    ImTextureID avatar = 0;  // Non-owning; replaced by choose_avatar.
    bool initialized = false;
    bool blur_captured = false;
    float avatar_hover = 0.0f;
};

struct SettingsRuntimeStatus {
    bool busy = false;
    bool visible = false;
    std::string text;
};

struct SettingsActions {
    // All callbacks run synchronously inside DrawSettings, as in the original.
    std::function<bool(Profile& draft, ImTextureID& avatar, std::string& error)>
        choose_avatar;
    std::function<bool(const Profile& draft, std::string& error)> save_profile;
    // Synchronizes the main avatar and, when restart is true, starts the load flow.
    std::function<void(const Profile& profile, bool restart)> profile_saved;
};

void DrawSettings(Theme& theme, UiState& state, SettingsState& settings,
                  const UiGpuResources& gpu, Profile& profile,
                  nl::data::CheatStore& store, gfx::Blur& blur,
                  const FrameInput& input, const SettingsRuntimeStatus& runtime,
                  SettingsActions& actions);

}  // namespace nl::ui
