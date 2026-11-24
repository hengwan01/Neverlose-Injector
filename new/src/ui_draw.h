#pragma once

#include "imgui.h"
#include "ui_types.h"

namespace nl::ui {

float CubicBezierEase(float t, float x1, float y1, float x2, float y2);
void TickAnim(AnimState& state, float dt);
void DrawSplash(const Theme& theme, float time, float opacity);

void DrawSpinner(ImDrawList* draw_list, ImVec2 center, float radius,
                 float thickness, ImU32 color, float time);

}  // namespace nl::ui
