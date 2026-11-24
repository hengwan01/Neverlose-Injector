#pragma once
// Thin-stroke vector icons drawn with ImDrawList, in the Lucide/Feather style
// the original loader uses. Every icon is centered at (cx, cy) and fits inside
// a `size`-wide box. `th` is the stroke thickness in pixels.
#include "imgui.h"

namespace nl::ui::icons {

inline void Play(ImDrawList* dl, float cx, float cy, float size, ImU32 col) {
    float r = size * 0.5f;
    dl->AddTriangleFilled(ImVec2(cx - r * 0.45f, cy - r * 0.85f),
                          ImVec2(cx - r * 0.45f, cy + r * 0.85f),
                          ImVec2(cx + r * 0.95f, cy), col);
}

inline void Info(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.6f) {
    float r = size * 0.5f - th * 0.5f;
    dl->AddCircle(ImVec2(cx, cy), r, col, 0, th);
    dl->AddCircleFilled(ImVec2(cx, cy - r * 0.40f), th * 0.62f, col);
    dl->AddLine(ImVec2(cx, cy - r * 0.02f), ImVec2(cx, cy + r * 0.52f), col, th);
}

// rotate-cw : the "Updated" glyph.
inline void Refresh(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.5f) {
    float r = size * 0.5f - th;
    ImVec2 c(cx, cy);
    // arc from ~35deg sweeping clockwise to ~330deg
    dl->PathArcTo(c, r, -0.5f, 4.9f, 24);
    dl->PathStroke(col, 0, th);
    // arrow head at the arc start (top-right)
    dl->AddLine(ImVec2(cx + r * 0.30f, cy - r * 0.98f), ImVec2(cx + r * 0.95f, cy - r * 0.55f), col, th);
    dl->AddLine(ImVec2(cx + r * 0.95f, cy - r * 0.55f), ImVec2(cx + r * 0.10f, cy - r * 0.30f), col, th);
}

// history : clock with a counter-clockwise arrow (small button next to Updated).
inline void History(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.5f) {
    float r = size * 0.5f - th;
    dl->PathArcTo(ImVec2(cx, cy), r, 0.6f, 5.4f, 24);
    dl->PathStroke(col, 0, th);
    dl->AddLine(ImVec2(cx, cy - r * 0.55f), ImVec2(cx, cy), col, th);
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx + r * 0.45f, cy + r * 0.2f), col, th);
    dl->AddLine(ImVec2(cx - r * 0.95f, cy - r * 0.30f), ImVec2(cx - r * 0.95f, cy + r * 0.45f), col, th);
    dl->AddLine(ImVec2(cx - r * 0.95f, cy + r * 0.45f), ImVec2(cx - r * 0.20f, cy + r * 0.45f), col, th);
}

inline void Clock(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.5f) {
    float r = size * 0.5f - th * 0.5f;
    dl->AddCircle(ImVec2(cx, cy), r, col, 0, th);
    dl->AddLine(ImVec2(cx, cy - r * 0.55f), ImVec2(cx, cy), col, th);
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx + r * 0.45f, cy), col, th);
}

inline void Hourglass(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.5f) {
    float r = size * 0.5f - th;
    float w = r * 0.72f;
    dl->AddLine(ImVec2(cx - w, cy - r), ImVec2(cx + w, cy - r), col, th);
    dl->AddLine(ImVec2(cx - w, cy + r), ImVec2(cx + w, cy + r), col, th);
    dl->AddLine(ImVec2(cx - w, cy - r), ImVec2(cx + w, cy + r), col, th);
    dl->AddLine(ImVec2(cx + w, cy - r), ImVec2(cx - w, cy + r), col, th);
}

// git-branch : the "Branch" glyph.
inline void Branch(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.5f) {
    float r = size * 0.5f;
    float n = r * 0.26f;                 // node radius
    float x0 = cx - r * 0.55f, x1 = cx + r * 0.55f;
    float ytop = cy - r * 0.8f, ybot = cy + r * 0.8f, ymid = cy - r * 0.1f;
    dl->AddCircle(ImVec2(x0, ytop), n, col, 0, th);
    dl->AddCircle(ImVec2(x0, ybot), n, col, 0, th);
    dl->AddCircle(ImVec2(x1, ymid), n, col, 0, th);
    dl->AddLine(ImVec2(x0, ytop + n), ImVec2(x0, ybot - n), col, th);
    // curve from right node down to the vertical stem
    dl->PathArcTo(ImVec2(x0, ymid), (x1 - x0), 0.0f, 1.5707f, 12);
    dl->PathStroke(col, 0, th);
}

inline void Monitor(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.5f) {
    float w = size * 0.5f - th, h = w * 0.66f;
    dl->AddRect(ImVec2(cx - w, cy - h - size * 0.06f), ImVec2(cx + w, cy + h - size * 0.06f), col, 1.5f, 0, th);
    dl->AddLine(ImVec2(cx, cy + h - size * 0.06f), ImVec2(cx, cy + h + size * 0.10f), col, th);
    dl->AddLine(ImVec2(cx - w * 0.5f, cy + h + size * 0.10f), ImVec2(cx + w * 0.5f, cy + h + size * 0.10f), col, th);
}

inline void Minus(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.6f) {
    float r = size * 0.32f;
    dl->AddLine(ImVec2(cx - r, cy), ImVec2(cx + r, cy), col, th);
}

inline void Close(ImDrawList* dl, float cx, float cy, float size, ImU32 col, float th = 1.6f) {
    float r = size * 0.30f;
    dl->AddLine(ImVec2(cx - r, cy - r), ImVec2(cx + r, cy + r), col, th);
    dl->AddLine(ImVec2(cx + r, cy - r), ImVec2(cx - r, cy + r), col, th);
}

}  // namespace nl::ui::icons
