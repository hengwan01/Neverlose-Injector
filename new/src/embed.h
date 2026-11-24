#pragma once
#include "common.h"
#include "paths.h"
#include <string>
#include <string_view>

// nl::embed — the read-only data shipped inside injector.exe.
// In the original these were linker-embedded byte blobs returned as
// {const uint8_t*, size_t} spans (Avatar/Cs2/Csgo/Valorant/Apex = JPEG,
// CheatsJson = the seed store). Here they live as files under assets/ and are
// loaded on demand, which keeps the project editable.
namespace nl::embed {

inline std::string CheatsJson() {
    static const std::string text = [] {
        std::string value;
        paths::ReadFileUtf8(paths::AssetPath(L"cheats.json"), value);
        return value;
    }();
    return text;
}

inline const std::vector<unsigned char>& Avatar() {
    static const std::vector<unsigned char> bytes = [] {
        std::vector<unsigned char> value;
        paths::ReadFileBytes(paths::AssetPath(L"default_avatar.jpg"), value);
        return value;
    }();
    return bytes;
}

inline const std::vector<unsigned char>& Cs2() {
    static const std::vector<unsigned char> bytes = [] {
        std::vector<unsigned char> value;
        paths::ReadFileBytes(paths::AssetPath(L"game_cs2.jpg"), value);
        return value;
    }();
    return bytes;
}

inline const std::vector<unsigned char>& ReferenceIcons() {
    static const std::vector<unsigned char> bytes = [] {
        std::vector<unsigned char> value;
        paths::ReadFileBytes(paths::AssetPath(L"reference_icons.png"), value);
        return value;
    }();
    return bytes;
}

inline const std::vector<unsigned char>& NeverloseLogo() {
    static const std::vector<unsigned char> bytes = [] {
        std::vector<unsigned char> value;
        paths::ReadFileBytes(paths::AssetPath(L"neverlose_logo.png"), value);
        return value;
    }();
    return bytes;
}

inline const std::vector<unsigned char>& Font() {
    static const std::vector<unsigned char> bytes = [] {
        std::vector<unsigned char> value;
        paths::ReadFileBytes(paths::AssetPath(L"font.ttf"), value);
        return value;
    }();
    return bytes;
}

// Map a cheat id ("cs2"/"csgo"/"valorant"/"apex") to its shipped card image.
inline std::wstring GameImagePath(std::string_view cheat) {
    if (cheat == "cs2")      return paths::AssetPath(L"game_cs2.jpg");
    if (cheat == "csgo")     return paths::AssetPath(L"game_csgo.jpg");
    if (cheat == "valorant") return paths::AssetPath(L"game_valorant.jpg");
    if (cheat == "apex")     return paths::AssetPath(L"game_apex.jpg");
    return paths::AssetPath(L"game_cs2.jpg");
}

inline std::wstring DefaultAvatarPath() {
    return paths::AssetPath(L"default_avatar.jpg");
}

}  // namespace nl::embed
