#include "ui_shell.h"

#include <cmath>

namespace nl::ui::shell {

ImTextureID IconAtlas = 0;

namespace {

ImVec2 At(const ImVec2& p, float x, float y) {
    return ImVec2(p.x + x, p.y + y);
}

}  // namespace

void Logo(ImDrawList* dl, ImVec2 pos, ImVec4 color, float alpha) {
    const ImVec2 base(floorf(pos.x + 0.5f), floorf(pos.y + 0.5f));
    dl->AddRectFilled(base, At(base, 39.0f, 39.0f),
                      ImGui::GetColorU32(ImVec4(0.025f, 0.055f, 0.09f, alpha)),
                      12.0f, 0);

    const ImVec2 center = At(base, 18.0f, 20.0f);
    for (int radius = 13; radius >= 4; --radius) {
        dl->AddCircleFilled(center, static_cast<float>(radius),
                            ImGui::GetColorU32(ImVec4(0.05f, 0.55f, 0.85f,
                                                      0.012f * alpha)), 32);
    }

    const ImVec2 left[] = {
        At(base, 7.0f, 12.0f),   At(base, 10.2f, 12.0f),
        At(base, 15.8f, 21.6f), At(base, 15.8f, 12.0f),
        At(base, 19.0f, 12.0f), At(base, 19.0f, 28.0f),
        At(base, 15.8f, 28.0f), At(base, 10.2f, 18.4f),
        At(base, 10.2f, 28.0f), At(base, 7.0f, 28.0f),
    };
    const ImVec2 right[] = {
        At(base, 21.4f, 12.0f), At(base, 24.6f, 12.0f),
        At(base, 24.6f, 24.8f), At(base, 31.0f, 24.8f),
        At(base, 31.0f, 28.0f), At(base, 21.4f, 28.0f),
    };
    const ImU32 mark_color = ImGui::GetColorU32(
        ImVec4(color.x, color.y, color.z, color.w * alpha));
    dl->AddConcavePolyFilled(left, 10, mark_color);
    dl->AddConcavePolyFilled(right, 6, mark_color);
}

void Refresh(ImDrawList* dl, ImVec2 pos, ImU32 color) {
    if (IconAtlas) {
        dl->AddImage(IconAtlas, At(pos, -9.0f, -9.0f), At(pos, 9.0f, 9.0f),
                     ImVec2(0.0f, 0.0f), ImVec2(0.125f, 1.0f), color);
        return;
    }

    dl->PathArcTo(pos, 6.5f, 0.25f, 2.9f, 14);
    dl->PathStroke(color, 0, 1.9f);
    dl->PathArcTo(pos, 6.5f, 3.4f, 6.05f, 14);
    dl->PathStroke(color, 0, 1.9f);
    dl->AddTriangleFilled(At(pos, -8.0f, -4.0f), At(pos, -1.0f, -4.0f),
                          At(pos, -5.0f, 1.0f), color);
    dl->AddTriangleFilled(At(pos, 8.0f, 4.0f), At(pos, 1.0f, 4.0f),
                          At(pos, 5.0f, -1.0f), color);
}

void History(ImDrawList* dl, ImVec2 pos, ImU32 color) {
    if (IconAtlas) {
        dl->AddImage(IconAtlas, At(pos, -9.0f, -9.0f), At(pos, 9.0f, 9.0f),
                     ImVec2(0.125f, 0.0f), ImVec2(0.25f, 1.0f), color);
        return;
    }

    dl->PathArcTo(pos, 7.0f, -1.25f, 4.1f, 24);
    dl->PathStroke(color, 0, 1.8f);
    dl->AddTriangleFilled(At(pos, -7.0f, -7.0f), At(pos, -7.0f, -1.0f),
                          At(pos, -1.0f, -4.0f), color);
    dl->AddLine(pos, At(pos, -0.0f, -4.0f), color, 1.7f);
    dl->AddLine(pos, At(pos, 3.0f, 2.0f), color, 1.7f);
}

void Hourglass(ImDrawList* dl, ImVec2 pos, ImU32 color) {
    if (IconAtlas) {
        dl->AddImage(IconAtlas, At(pos, -9.0f, -9.0f), At(pos, 9.0f, 9.0f),
                     ImVec2(0.25f, 0.0f), ImVec2(0.375f, 1.0f), color);
        return;
    }

    dl->AddLine(At(pos, -5.0f, -7.0f), At(pos, 5.0f, -7.0f), color, 1.8f);
    dl->AddLine(At(pos, -5.0f, 7.0f), At(pos, 5.0f, 7.0f), color, 1.8f);
    dl->AddTriangleFilled(At(pos, -4.0f, -5.0f), At(pos, 4.0f, -5.0f),
                          At(pos, 0.0f, -1.0f), color);
    dl->AddTriangleFilled(At(pos, -4.0f, 5.0f), At(pos, 4.0f, 5.0f),
                          At(pos, 0.0f, 1.0f), color);
}

void Branch(ImDrawList* dl, ImVec2 pos, ImU32 color) {
    if (IconAtlas) {
        dl->AddImage(IconAtlas, At(pos, -9.0f, -9.0f), At(pos, 9.0f, 9.0f),
                     ImVec2(0.375f, 0.0f), ImVec2(0.5f, 1.0f), color);
        return;
    }

    dl->AddLine(At(pos, -4.0f, -7.0f), At(pos, -4.0f, 7.0f), color, 1.5f);
    dl->AddBezierCubic(At(pos, -4.0f, 3.0f), At(pos, -4.0f, -2.0f),
                       At(pos, 5.0f, 2.0f), At(pos, 5.0f, -7.0f),
                       color, 1.5f, 0);
    dl->AddCircleFilled(At(pos, -4.0f, -8.0f), 2.4f, color, 0);
    dl->AddCircleFilled(At(pos, -4.0f, 8.0f), 2.4f, color, 0);
    dl->AddCircleFilled(At(pos, 5.0f, -8.0f), 2.4f, color, 0);
}

bool Button(ImDrawList* dl, const char* id, ImVec2 pos, ImVec2 size,
            ImVec4 color, float alpha, bool enabled, bool decorative) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton(id, size, 0);
    if (enabled && ImGui::IsItemHovered(0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::EndDisabled();

    const bool hovered = ImGui::IsItemHovered(0);
    const ImVec2 p_max(pos.x + size.x, pos.y + size.y);
    float fill_alpha = (hovered ? 1.0f : 0.95f) * alpha;
    if (GlassEnabled)
        fill_alpha *= color.z > 0.9f ? 0.98f : 0.74f;
    fill_alpha *= color.w;
    dl->AddRectFilled(pos, p_max,
                      ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, fill_alpha)),
                      11.0f, 0);

    if (decorative && GlassEnabled) {
        const float border_alpha = (hovered ? 0.24f : 0.12f) * alpha;
        dl->AddRect(pos, p_max,
                    ImGui::GetColorU32(ImVec4(0.82f, 0.88f, 1.0f, border_alpha)),
                    11.0f, 0, 1.0f);
        const ImVec2 line_begin = At(pos, 11.0f, 1.0f);
        dl->AddLine(line_begin, ImVec2(p_max.x - 11.0f, line_begin.y),
                    ImGui::GetColorU32(ImVec4(0.9f, 0.94f, 1.0f, 0.18f * alpha)),
                    1.0f);
    }
    return pressed;
}

void Share(ImDrawList* dl, ImVec2 pos, ImU32 color) {
    if (IconAtlas) {
        dl->AddImage(IconAtlas, At(pos, -9.0f, -9.0f), At(pos, 9.0f, 9.0f),
                     ImVec2(0.5f, 0.0f), ImVec2(0.625f, 1.0f), color);
        return;
    }

    const ImVec2 left = At(pos, -5.0f, 0.0f);
    const ImVec2 top = At(pos, 4.0f, -5.0f);
    const ImVec2 bottom = At(pos, 4.0f, 5.0f);
    dl->AddLine(left, top, color, 1.8f);
    dl->AddLine(left, bottom, color, 1.8f);
    dl->AddCircleFilled(left, 2.7f, color, 16);
    dl->AddCircleFilled(top, 2.7f, color, 16);
    dl->AddCircleFilled(bottom, 2.7f, color, 16);
}

void Computer(ImDrawList* dl, ImVec2 pos, ImU32 color) {
    if (IconAtlas) {
        dl->AddImage(IconAtlas, At(pos, -12.5f, -12.5f), At(pos, 12.5f, 12.5f),
                     ImVec2(0.625f, 0.0f), ImVec2(0.75f, 1.0f), color);
        return;
    }

    dl->AddRect(At(pos, -10.0f, -6.0f), At(pos, 4.0f, 3.0f),
                color, 1.0f, 0, 1.7f);
    dl->AddLine(At(pos, -3.0f, 3.0f), At(pos, -3.0f, 7.0f), color, 1.7f);
    dl->AddLine(At(pos, -7.0f, 7.0f), At(pos, 1.0f, 7.0f), color, 1.7f);
    dl->AddRect(At(pos, 7.0f, -7.0f), At(pos, 11.0f, 7.0f),
                color, 1.0f, 0, 1.7f);
    dl->AddCircleFilled(At(pos, 9.0f, 4.0f), 0.8f, color, 8);
}

}  // namespace nl::ui::shell
