#pragma once
// Theme values and drawing helpers reconstructed from injector.exe.

#include "imgui.h"

namespace nl::ui::shell {

inline bool GlassEnabled = true;
// 自绘的桌面模糊底板，由 AppFrame 每帧写入；0 表示不可用。
inline ImTextureID BackdropTexture = 0;

inline const ImVec4 Blue()    { return ImVec4(0.255f, 0.455f, 0.985f, 1.0f); }
inline const ImVec4 Panel()   { return ImVec4(0.078f, 0.082f, 0.114f, 1.0f); }
inline const ImVec4 Card()    { return ImVec4(0.078f, 0.083f, 0.116f, 1.0f); }
inline const ImVec4 Border()  { return ImVec4(0.160f, 0.170f, 0.220f, 1.0f); }
inline const ImVec4 Muted()   { return ImVec4(0.560f, 0.570f, 0.650f, 1.0f); }
inline const ImVec4 Text()    { return ImVec4(0.960f, 0.960f, 0.980f, 1.0f); }
inline const ImVec4 TextDim() { return ImVec4(0.700f, 0.710f, 0.770f, 1.0f); }
inline const ImVec4 ModalDim(){ return ImVec4(0.000f, 0.000f, 0.000f, 0.650f); }

extern ImTextureID IconAtlas;

void Logo(ImDrawList* dl, ImVec2 pos, ImVec4 color, float alpha);
void Refresh(ImDrawList* dl, ImVec2 pos, ImU32 color);
void History(ImDrawList* dl, ImVec2 pos, ImU32 color);
void Hourglass(ImDrawList* dl, ImVec2 pos, ImU32 color);
void Branch(ImDrawList* dl, ImVec2 pos, ImU32 color);
bool Button(ImDrawList* dl, const char* id, ImVec2 pos, ImVec2 size,
            ImVec4 color, float alpha, bool enabled, bool decorative);
void Share(ImDrawList* dl, ImVec2 pos, ImU32 color);
void Computer(ImDrawList* dl, ImVec2 pos, ImU32 color);

}  // namespace nl::ui::shell
