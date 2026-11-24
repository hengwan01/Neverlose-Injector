#pragma once

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nl {

struct Profile {
    Profile();

    std::string nickname = "Awa Subaru";
    std::string language = "en";
    std::vector<unsigned char> avatar;
    bool auto_start = true;
    bool auto_inject = true;
    std::int64_t expires_at = 0;
};

Profile LoadProfile(std::string& error);
bool SaveProfile(const Profile& profile, std::string& error);
int RemainingProfileDays(const Profile& profile, long long now);
void SetProfileDays(Profile& profile, int days, long long now);

static_assert(sizeof(Profile) == 0x58, "Unexpected Profile size");
static_assert(offsetof(Profile, nickname) == 0x00, "Unexpected nickname offset");
static_assert(offsetof(Profile, language) == 0x18, "Unexpected language offset");
static_assert(offsetof(Profile, avatar) == 0x30, "Unexpected avatar offset");
static_assert(offsetof(Profile, auto_start) == 0x48, "Unexpected auto_start offset");
static_assert(offsetof(Profile, auto_inject) == 0x49, "Unexpected auto_inject offset");
static_assert(offsetof(Profile, expires_at) == 0x50, "Unexpected expires_at offset");

}  // namespace nl
