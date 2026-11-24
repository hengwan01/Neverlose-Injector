#include "profile.h"

#include "embed.h"
#include "nlohmann/json.hpp"
#include "paths.h"

#include <algorithm>
#include <ctime>
#include <string_view>

namespace nl {
namespace {

constexpr const char* kNicknameError =
    "Nickname: 1-32 characters, no control characters or surrounding spaces.";
constexpr const char* kReadError =
    "Saved profile could not be read. Default profile restored.";

bool ValidNickname(std::string_view value) {
    if (value.empty() || value.front() == ' ' || value.back() == ' ')
        return false;

    std::size_t offset = 0;
    int codepoints = 0;
    while (offset < value.size()) {
        const auto first = static_cast<unsigned char>(value[offset]);
        std::uint32_t codepoint = 0;
        std::size_t length = 0;
        if (first <= 0x7f) {
            codepoint = first;
            length = 1;
        } else if (first >= 0xc2 && first <= 0xdf) {
            codepoint = first & 0x1f;
            length = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            codepoint = first & 0x0f;
            length = 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            codepoint = first & 0x07;
            length = 4;
        } else {
            return false;
        }

        if (offset + length > value.size())
            return false;
        for (std::size_t i = 1; i < length; ++i) {
            const auto next = static_cast<unsigned char>(value[offset + i]);
            if ((next & 0xc0) != 0x80)
                return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }

        if ((length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
            codepoint > 0x10ffff || codepoint <= 0x1f ||
            (codepoint >= 0x7f && codepoint <= 0x9f))
            return false;

        offset += length;
        if (++codepoints > 32)
            return false;
    }
    return codepoints >= 1;
}

bool ValidAvatar(const std::vector<unsigned char>& avatar) {
    return avatar.size() >= 4 && avatar.size() <= 0x8000 &&
           avatar[0] == 0xff && avatar[1] == 0xd8 &&
           avatar[avatar.size() - 2] == 0xff && avatar.back() == 0xd9;
}

}  // namespace

Profile::Profile() : avatar(embed::Avatar()) {}

Profile LoadProfile(std::string& error) {
    error.clear();
    Profile profile;
    const std::wstring path = paths::ProfileJson();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return profile;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        error = kReadError;
        return profile;
    }
    if (size.QuadPart >= 0x100001) {
        CloseHandle(file);
        error = "Profile file too large.";
        return profile;
    }

    std::string text(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const bool read_ok = text.empty() ||
        (ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) &&
         read == text.size());
    CloseHandle(file);
    if (!read_ok) {
        error = kReadError;
        return profile;
    }

    try {
        const nlohmann::json root = nlohmann::json::parse(text);
        if (!root.is_object()) {
            error = kReadError;
            return Profile();
        }

        (void)root.value("version", 1);
        profile.nickname = root.value("nickname", profile.nickname);
        profile.language = root.value("language", profile.language);
        profile.avatar = root.value("avatar", profile.avatar);
        profile.auto_start = root.value("auto_start", profile.auto_start);
        profile.auto_inject = root.value("auto_inject", profile.auto_inject);
        profile.expires_at = root.value("expires_at", profile.expires_at);
        // 兼容旧配置：只有布尔开关时，非纯色视为液体玻璃。
        if (root.contains("appearance"))
            profile.appearance = static_cast<std::uint8_t>(
                std::clamp(root.value("appearance", 1), 0, 2));
        else if (root.value("solid_appearance", false))
            profile.appearance = 2;
    } catch (...) {
        error = kReadError;
        return Profile();
    }

    if (!ValidNickname(profile.nickname))
        profile.nickname = "Awa Subaru";
    if (profile.language != "en" && profile.language != "ru")
        profile.language = "en";
    if (!ValidAvatar(profile.avatar))
        profile.avatar = embed::Avatar();

    const long long now = static_cast<long long>(_time64(nullptr));
    if (profile.expires_at < now || profile.expires_at > now + 0xBBF81E00LL)
        profile.expires_at = 0;
    return profile;
}

bool SaveProfile(const Profile& profile, std::string& error) {
    error.clear();
    if (!ValidNickname(profile.nickname)) {
        error = kNicknameError;
        return false;
    }
    if (profile.language != "en" && profile.language != "ru") {
        error = "Unsupported language.";
        return false;
    }
    if (!ValidAvatar(profile.avatar)) {
        error = "Choose a valid avatar first.";
        return false;
    }

    nlohmann::json root;
    root["version"] = 1;
    root["nickname"] = profile.nickname;
    root["language"] = profile.language;
    root["avatar"] = profile.avatar;
    root["auto_start"] = profile.auto_start;
    root["auto_inject"] = profile.auto_inject;
    root["expires_at"] = profile.expires_at;
    root["appearance"] = static_cast<int>(profile.appearance);

    const std::wstring path = paths::ProfileJson();
    const std::wstring temporary = path + L".tmp";
    if (!paths::WriteFileUtf8(temporary, root.dump(-1))) {
        error = "Could not save profile settings.";
        return false;
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Could not replace profile settings.";
        return false;
    }
    return true;
}

int RemainingProfileDays(const Profile& profile, long long now) {
    if (profile.expires_at <= now)
        return 0;
    const long long remaining = profile.expires_at - now;
    return static_cast<int>((remaining + 86399) / 86400);
}

void SetProfileDays(Profile& profile, int days, long long now) {
    profile.expires_at = days > 0
        ? now + static_cast<long long>(days) * 86400
        : 0;
}

}  // namespace nl
