// =====================================================================
//  ui_render.cpp  -  the editable loader UI
//
//  Layout mirrors the original injector.exe launcher:
//    - custom title bar : NL logo + "Neverlose" | page title | minimize/close
//    - left rail        : "SUBSCRIPTIONS" + one selectable card per cheat,
//                         user avatar + nickname pinned to the bottom
//    - main surface     : "Available" outline banner, game icon + name,
//                         Updated / Subscription / Last Launch / Branch rows,
//                         blue Launch button (bottom-right)
//
//  Backend wiring (unchanged):
//    Launch -> nl::Runtime,  user card / Settings -> nl::SaveProfile,
//    subscription cards -> SelectCheat,  Branch row -> SelectBranch,
//    all visible strings -> nl::i18n::T.
// =====================================================================
#include "app_frame.h"
#include "ui_shell.h"
#include "ui_icons.h"
#include "i18n.h"
#include "imgui.h"
#include <commdlg.h>
#include <cstring>
#include <cfloat>

#pragma comment(lib, "comdlg32.lib")

namespace nl {

static const char* Tr(const char* s) { return i18n::T(s); }

// Open a PNG/JPEG file chooser and return a UTF-8 path (empty on cancel).
static std::string OpenAvatarDialog() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0All\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, file, -1, out.data(), n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

void AppFrame::RenderUI() {
    UiState& s = state_;
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float W = disp.x, H = disp.y;
    const float TITLE = 56.0f;   // title bar height
    const float SIDE  = 205.0f;  // left rail width

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    ImGuiWindowFlags root_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##neverlosecs2_root", nullptr, root_flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 O = ImGui::GetWindowPos();
    using namespace ui::icons;

    // ---- drawing helpers ------------------------------------------------
    auto U = [](ImVec4 c) { return ImGui::GetColorU32(c); };
    auto rect = [&](float x, float y, float w, float h, ImVec4 c, float r) {
        dl->AddRectFilled({O.x + x, O.y + y}, {O.x + x + w, O.y + y + h}, U(c), r);
    };
    auto recto = [&](float x, float y, float w, float h, ImVec4 c, float r, float th) {
        dl->AddRect({O.x + x, O.y + y}, {O.x + x + w, O.y + y + h}, U(c), r, 0, th);
    };
    auto tsz = [&](ImFont* f, const char* t) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, t); };
    auto text = [&](ImFont* f, float x, float y, const char* t, ImVec4 c) {
        dl->AddText(f, f->FontSize, {O.x + x, O.y + y}, U(c), t);
    };
    // text vertically centered inside a row of height h
    auto textVC = [&](ImFont* f, float x, float rowY, float h, const char* t, ImVec4 c) {
        text(f, x, rowY + (h - f->FontSize) * 0.5f, t, c);
    };
    auto hit = [&](const char* id, float x, float y, float w, float h) {
        ImGui::SetCursorPos({x, y});
        ImGui::InvisibleButton(id, {w, h});
    };
    auto imgRounded = [&](ImTextureID t, float x, float y, float w, float h, float r) {
        if (t) dl->AddImageRounded(t, {O.x + x, O.y + y}, {O.x + x + w, O.y + y + h},
                                   {0, 0}, {1, 1}, IM_COL32_WHITE, r);
        else   rect(x, y, w, h, ui::shell::Card(), r);
    };
    const ImU32 C_TEXT = U(ui::shell::Text()), C_MUTED = U(ui::shell::Muted()), C_BLUE = U(ui::shell::Blue());

    // ---- surfaces -------------------------------------------------------
    const float surface_alpha = ui::shell::GlassEnabled ? ui::shell::GlassAlpha : 1.0f;
    ImVec4 main = ui::shell::Panel();
    main.w = surface_alpha;
    rect(0, 0, W, H, main, 0);
    ImVec4 sbc = ui::shell::Sidebar();
    sbc.w = surface_alpha;
    rect(0, 0, SIDE, H, sbc, 0);

    // ---- title bar ------------------------------------------------------
    imgRounded((ImTextureID)LogoMark(), 16, 14, 28, 28, 7);
    textVC(FontSemi(), 52, 0, TITLE, "Neverlose", ui::shell::Text());
    textVC(FontSemi(), SIDE + 40, 0, TITLE, s.page_title.c_str(), ui::shell::Text());
    {   // minimize
        float bx = W - 66, by = TITLE * 0.5f;
        hit("##min", bx - 15, by - 15, 30, 30);
        bool hov = ImGui::IsItemHovered();
        if (hov) rect(bx - 15, by - 15, 30, 30, ui::shell::CardHov(), 5);
        Minus(dl, O.x + bx, O.y + by, 16, hov ? C_TEXT : C_MUTED);
        if (ImGui::IsItemClicked()) RequestMinimize();
    }
    {   // close
        float bx = W - 32, by = TITLE * 0.5f;
        hit("##close", bx - 15, by - 15, 30, 30);
        bool hov = ImGui::IsItemHovered();
        if (hov) rect(bx - 15, by - 15, 30, 30, ui::shell::Danger(), 5);
        Close(dl, O.x + bx, O.y + by, 16, hov ? C_TEXT : C_MUTED);
        if (ImGui::IsItemClicked()) RequestClose();
    }

    // ---- left rail : subscriptions -------------------------------------
    text(FontSmall(), 16, TITLE + 18, "SUBSCRIPTIONS", ui::shell::Muted());
    float cy = TITLE + 46;
    for (auto& c : store_.cheats) {
        const float cw = SIDE - 28, ch = 40, cx = 14;
        bool sel = (c.cheat == s.profile.cheat);
        std::string id = "##sub_" + c.cheat;
        hit(id.c_str(), cx, cy, cw, ch);
        bool hov = ImGui::IsItemHovered();
        if (sel)       rect(cx, cy, cw, ch, ui::shell::Card(), 6);
        else if (hov)  rect(cx, cy, cw, ch, ui::shell::CardHov(), 6);
        int iw = 0, ih = 0;
        imgRounded((ImTextureID)SubIconFor(c.cheat, &iw, &ih), cx + 8, cy + 6, 28, 28, 5);
        textVC(FontBody(), cx + 44, cy, ch, c.name.c_str(), sel ? ui::shell::Text() : ui::shell::TextDim());
        if (ImGui::IsItemClicked() && !sel) SelectCheat(c.cheat);
        cy += ch + 6;
    }
    {   // user footer -> opens Settings
        float uy = H - 46;
        hit("##user", 8, uy - 4, SIDE - 16, 40);
        if (ImGui::IsItemHovered()) rect(8, uy - 4, SIDE - 16, 40, ui::shell::CardHov(), 6);
        imgRounded((ImTextureID)LogoAvatar(), 16, uy, 32, 32, 16);
        textVC(FontBody(), 56, uy, 32, s.profile.nickname.c_str(), ui::shell::Text());
        if (ImGui::IsItemClicked()) s.settings_open = true;
    }

    // ---- main surface ---------------------------------------------------
    const float cx = SIDE + 40, cw = W - cx - 20;
    {   // Available outline banner
        float y = TITLE + 16, h = 38;
        recto(cx, y, cw, h, ui::shell::Border(), 6, 1.0f);
        Info(dl, O.x + cx + 22, O.y + y + h * 0.5f, 16, C_BLUE);
        textVC(FontBody(), cx + 40, y, h, Tr("Available"), ui::shell::Text());
    }
    {   // game icon + name
        float y = TITLE + 72;
        imgRounded((ImTextureID)GameImage(), cx, y, 44, 44, 8);
        textVC(FontTitle(), cx + 56, y, 44, s.game_name.c_str(), ui::shell::Text());
    }

    const float startY = TITLE + 140, rowH = 40;
    auto rowY = [&](int i) { return startY + i * rowH; };
    {   // Updated (+ history button)
        float ry = rowY(0);
        Refresh(dl, O.x + cx + 2, O.y + ry + rowH * 0.5f, 16, C_MUTED);
        textVC(FontBody(), cx + 26, ry, rowH, Tr("Updated"), ui::shell::Muted());
        float vx = (cx + cw - 28) - tsz(FontBody(), s.updated.c_str()).x;
        textVC(FontBody(), vx, ry, rowH, s.updated.c_str(), ui::shell::Text());
        float hx = cx + cw - 8, hy = ry + rowH * 0.5f;
        hit("##hist", hx - 12, hy - 12, 24, 24);
        History(dl, O.x + hx, O.y + hy, 15, ImGui::IsItemHovered() ? C_TEXT : C_MUTED);
    }
    {   // Subscription
        float ry = rowY(1);
        Clock(dl, O.x + cx + 2, O.y + ry + rowH * 0.5f, 16, C_MUTED);
        textVC(FontBody(), cx + 26, ry, rowH, Tr("Subscription"), ui::shell::Muted());
        float vx = cx + cw - tsz(FontBody(), s.subscription.c_str()).x;
        textVC(FontBody(), vx, ry, rowH, s.subscription.c_str(), ui::shell::Text());
    }
    {   // Last Launch
        float ry = rowY(2);
        Hourglass(dl, O.x + cx + 2, O.y + ry + rowH * 0.5f, 16, C_MUTED);
        textVC(FontBody(), cx + 26, ry, rowH, Tr("Last Launch"), ui::shell::Muted());
        float vx = cx + cw - tsz(FontBody(), s.last_launch.c_str()).x;
        textVC(FontBody(), vx, ry, rowH, s.last_launch.c_str(), ui::shell::Text());
    }
    {   // Branch (clickable)
        float ry = rowY(3);
        Branch(dl, O.x + cx + 2, O.y + ry + rowH * 0.5f, 16, C_MUTED);
        textVC(FontBody(), cx + 26, ry, rowH, Tr("Branch"), ui::shell::Muted());
        float vw = tsz(FontBody(), s.branch.c_str()).x;
        float vx = cx + cw - vw;
        textVC(FontBody(), vx, ry, rowH, s.branch.c_str(), ui::shell::Blue());
        Branch(dl, O.x + vx - 16, O.y + ry + rowH * 0.5f, 13, C_BLUE);
        hit("##branch", vx - 26, ry + 4, vw + 30, rowH - 8);
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##BranchSelect");
    }

    {   // Launch button (bottom-right) + monitor glyph
        const float lw = 130, lh = 38, lx = W - 20 - lw, ly = H - 20 - lh;
        bool busy = runtime_.Running();
        Monitor(dl, O.x + lx - 24, O.y + ly + lh * 0.5f, 18, C_MUTED);
        hit("##launch", lx, ly, lw, lh);
        bool hov = ImGui::IsItemHovered();
        ImVec4 bg = busy ? ui::shell::Card() : (hov ? ui::shell::BlueHov() : ui::shell::Blue());
        rect(lx, ly, lw, lh, bg, 8);
        const char* lbl = busy ? Tr("Launching...") : Tr("Launch");
        float tw = tsz(FontBody(), lbl).x;
        float total = (busy ? 0.0f : 20.0f) + tw;
        float sx = lx + (lw - total) * 0.5f;
        if (!busy) Play(dl, O.x + sx + 6, O.y + ly + lh * 0.5f, 11, C_TEXT);
        textVC(FontBody(), sx + (busy ? 0.0f : 20.0f), ly, lh, lbl, ui::shell::Text());
        if (ImGui::IsItemClicked() && !busy) StartLaunch();
        if ((busy || runtime_.Done()) && !s.status.empty()) {
            float sw = tsz(FontSmall(), s.status.c_str()).x;
            text(FontSmall(), W - 20 - sw, ly - 20, s.status.c_str(), ui::shell::Muted());
        }
        if (s.need_restart_admin)
            text(FontSmall(), cx, ly - 20, Tr("Restart as administrator"), ui::shell::Blue());
    }

    // ---- popups (kept inside the root window for a consistent ID stack) --
    if (ImGui::BeginPopup("##BranchSelect")) {
        const data::CheatInfo* c = store_.Find(s.profile.cheat);
        if (c) for (auto& b : c->branches)
            if (ImGui::Selectable(b.name.c_str(), b.id == c->selected_branch)) SelectBranch(b.id);
        ImGui::EndPopup();
    }

    if (s.settings_open) ImGui::OpenPopup("##ProfileSettings");
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 0));
        if (ImGui::BeginPopupModal("##ProfileSettings", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextColored(ui::shell::Text(), "%s", Tr("Settings"));
            ImGui::Separator();
            ImGui::Spacing();

            char nick[128] = {};
            strncpy(nick, s.profile.nickname.c_str(), sizeof(nick) - 1);
            nick[sizeof(nick) - 1] = '\0';
            ImGui::TextColored(ui::shell::Muted(), "Nickname");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##nickname", nick, sizeof(nick)))
                s.profile.nickname = nick;

            ImGui::TextColored(ui::shell::Muted(), "%s", Tr("Days"));
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("##profile_days", &s.profile.days);
            ImGui::SameLine();
            ImGui::TextColored(ui::shell::Muted(), "(%s)", Tr("0 = Lifetime"));

            ImGui::TextColored(ui::shell::Muted(), "%s", Tr("Language"));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##lang", s.profile.language.c_str())) {
                for (const char* l : { "English", "Russian", "Chinese" })
                    if (ImGui::Selectable(l, s.profile.language == l))
                        s.profile.language = l;
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Auto-start CS2", &s.profile.auto_start);
            ImGui::Checkbox("Auto-inject", &s.profile.auto_inject);

            ImGui::Spacing();
            if (ImGui::Button(Tr("Choose image..."))) {
                std::string p = OpenAvatarDialog();
                if (!p.empty()) { s.profile.avatar_path = p; if (device_) ReloadAvatar(*device_); }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 140);
            if (ImGui::Button(Tr("Save"), ImVec2(64, 0))) SaveProfileFromUi();
            ImGui::SameLine();
            if (ImGui::Button(Tr("Close"), ImVec2(64, 0))) s.settings_open = false;
            ImGui::EndPopup();
        }
    }

    ImGui::End();  // root
}

}  // namespace nl
