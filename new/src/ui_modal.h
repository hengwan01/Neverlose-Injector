#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ui_types.h"

namespace nl::ui {

struct ModalActions {
    bool start = false;                          // 0x00
    bool cancel = false;                         // 0x01
    bool restart_admin = false;                  // 0x02
    std::uint8_t reserved_003[0x05] = {};
    const wchar_t* open_url = nullptr;            // 0x08
    bool record_launch = false;                  // 0x10
    std::uint8_t reserved_011[0x07] = {};
    std::string launch_cheat;                   // 0x18
    std::string launch_branch;                  // 0x30
    std::int64_t launch_time = 0;                // 0x48
    bool select_branch = false;                  // 0x50
    std::uint8_t reserved_051[0x07] = {};
    std::string select_cheat;                   // 0x58
    std::string select_branch_id;               // 0x70
};

void DrawGameModal(Theme& theme, UiState& state, const UiGpuResources& gpu,
                   nl::data::CheatStore& store, const FrameInput& input,
                   ModalActions& actions);

static_assert(sizeof(ModalState::DisplayedChangelogBlock) == 0x30);
static_assert(offsetof(ModalState::DisplayedChangelogBlock, date) == 0x00);
static_assert(offsetof(ModalState::DisplayedChangelogBlock, lines) == 0x18);

static_assert(sizeof(ModalState) == 0x128);
static_assert(offsetof(ModalState, open) == 0x00);
static_assert(offsetof(ModalState, cheat_id) == 0x28);
static_assert(offsetof(ModalState, branch_open) == 0x40);
static_assert(offsetof(ModalState, changelog_open) == 0x41);
static_assert(offsetof(ModalState, changelog_alpha) == 0x44);
static_assert(offsetof(ModalState, changelog_blur_captured) == 0x48);
static_assert(offsetof(ModalState, branch_alpha) == 0x4c);
static_assert(offsetof(ModalState, displayed_branch) == 0x80);
static_assert(offsetof(ModalState, displayed_updated) == 0x98);
static_assert(offsetof(ModalState, displayed_subscription) == 0xb0);
static_assert(offsetof(ModalState, displayed_last_launch) == 0xc8);
static_assert(offsetof(ModalState, changelog_blocks) == 0xe0);
static_assert(offsetof(ModalState, launch_overlay_visible) == 0xf8);
static_assert(offsetof(ModalState, launch_overlay_aux) == 0xf9);
static_assert(offsetof(ModalState, launch_started_ms) == 0x100);
static_assert(offsetof(ModalState, launch_overlay_params) == 0x108);
static_assert(offsetof(ModalState, launch_overlay_alpha) == 0x118);
static_assert(offsetof(ModalState, changelog_drawn) == 0x120);

static_assert(sizeof(ModalActions) == 0x88);
static_assert(offsetof(ModalActions, start) == 0x00);
static_assert(offsetof(ModalActions, cancel) == 0x01);
static_assert(offsetof(ModalActions, restart_admin) == 0x02);
static_assert(offsetof(ModalActions, open_url) == 0x08);
static_assert(offsetof(ModalActions, record_launch) == 0x10);
static_assert(offsetof(ModalActions, launch_cheat) == 0x18);
static_assert(offsetof(ModalActions, launch_branch) == 0x30);
static_assert(offsetof(ModalActions, launch_time) == 0x48);
static_assert(offsetof(ModalActions, select_branch) == 0x50);
static_assert(offsetof(ModalActions, select_cheat) == 0x58);
static_assert(offsetof(ModalActions, select_branch_id) == 0x70);

}  // namespace nl::ui
