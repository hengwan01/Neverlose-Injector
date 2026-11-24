#pragma once
#include "common.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Mirrors injector.exe's nl::data module:
//   CheatStore / CheatInfo / BranchInfo / ChangelogEntry.
// The store is seeded from the embedded assets/cheats.json and persisted as a
// user-writable %LOCALAPPDATA%\NLLoader\cheats.json (selected_branch /
// last_launch are the mutable fields).
namespace nl::data {

struct ChangelogEntry {
    int64_t     date = 0;
    std::string text;
};

struct BranchInfo {
    std::string id;
    std::string name;
    std::string version;
    int64_t     build_date  = 0;
    int64_t     last_launch = 0;
    std::vector<ChangelogEntry> changelog;
};

struct CheatInfo {
    std::string cheat;          // "cs2" / "csgo" / "valorant" / "apex"
    std::string name;
    std::string selected_branch;
    std::vector<BranchInfo> branches;
    int64_t     license = 0;
    bool        lifetime = false;

    BranchInfo*       ActiveBranch();
    const BranchInfo* ActiveBranch() const;
};

struct CheatStore {
    std::vector<CheatInfo> cheats;

    // Load user cheats.json; seed from embedded asset if missing/unusable.
    void Load();

    CheatInfo*       Find(std::string_view id);
    const CheatInfo* Find(std::string_view id) const;

    bool SaveSelectedBranch(std::string_view cheat, std::string_view branch);
    bool SaveLastLaunch(std::string_view cheat, std::string_view branch, int64_t unix_ts);

    // Serialize the whole store back to user cheats.json.
    bool Persist() const;
};

static_assert(sizeof(BranchInfo) == 0x70, "Unexpected BranchInfo size");
static_assert(offsetof(BranchInfo, build_date) == 0x48, "Unexpected build_date offset");
static_assert(offsetof(BranchInfo, last_launch) == 0x50, "Unexpected last_launch offset");
static_assert(sizeof(CheatInfo) == 0x70, "Unexpected CheatInfo size");
static_assert(offsetof(CheatInfo, selected_branch) == 0x30, "Unexpected branch offset");
static_assert(offsetof(CheatInfo, branches) == 0x48, "Unexpected branches offset");
static_assert(offsetof(CheatInfo, license) == 0x60, "Unexpected license offset");
static_assert(offsetof(CheatInfo, lifetime) == 0x68, "Unexpected lifetime offset");

// Parse a store JSON text into `out`. Returns false (and fills *err) on bad shape.
bool ParseStoreJson(std::string_view text, CheatStore& out, std::string* err);

}  // namespace nl::data
