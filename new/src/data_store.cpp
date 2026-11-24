#include "data_store.h"
#include "embed.h"
#include "paths.h"
#include "nlohmann/json.hpp"
#include <algorithm>

namespace nl::data {

BranchInfo* CheatInfo::ActiveBranch() {
    for (auto& b : branches) if (b.id == selected_branch) return &b;
    return branches.empty() ? nullptr : &branches.front();
}
const BranchInfo* CheatInfo::ActiveBranch() const {
    for (auto& b : branches) if (b.id == selected_branch) return &b;
    return branches.empty() ? nullptr : &branches.front();
}

static bool IsString(const nlohmann::json& j) { return j.is_string(); }

bool ParseStoreJson(std::string_view text, CheatStore& out, std::string* err) {
    out.cheats.clear();
    nlohmann::json root;
    try { root = nlohmann::json::parse(text); }
    catch (const std::exception& e) { if (err) *err = e.what(); return false; }
    if (!root.is_array()) { if (err) *err = "cheats.json must be an array"; return false; }

    for (const auto& cj : root) {
        if (!cj.is_object()) { if (err) *err = "cheat entry not object"; return false; }
        CheatInfo ci;
        if (!cj.contains("cheat") || !cj["cheat"].is_string()) { if(err)*err="missing cheat id"; return false; }
        ci.cheat  = cj["cheat"].get<std::string>();
        ci.name   = cj.value("name", ci.cheat);
        ci.lifetime = cj.value("lifetime", false);
        ci.license  = cj.value("license", int64_t{0});
        ci.selected_branch = cj.value("selected_branch", std::string());
        if (cj.contains("branches") && cj["branches"].is_array()) {
            for (const auto& bj : cj["branches"]) {
                if (!bj.is_object()) continue;
                BranchInfo bi;
                bi.id      = bj.value("id", std::string());
                bi.name    = bj.value("name", bi.id);
                bi.version = bj.value("version", std::string());
                bi.build_date  = (int64_t)bj.value("build_date", 0);
                bi.last_launch = (int64_t)bj.value("last_launch", 0);
                if (bj.contains("changelog") && bj["changelog"].is_array()) {
                    for (const auto& ej : bj["changelog"]) {
                        if (!ej.is_object()) continue;
                        ChangelogEntry e;
                        e.date = (int64_t)ej.value("date", 0);
                        e.text = ej.value("changelog", std::string());
                        bi.changelog.push_back(std::move(e));
                    }
                }
                ci.branches.push_back(std::move(bi));
            }
        }
        if (ci.selected_branch.empty() && !ci.branches.empty())
            ci.selected_branch = ci.branches.front().id;
        out.cheats.push_back(std::move(ci));
    }
    if (out.cheats.empty()) { if (err) *err = "no cheats parsed"; return false; }
    return true;
}

CheatInfo* CheatStore::Find(std::string_view id) {
    for (auto& c : cheats) if (c.cheat == id) return &c;
    return nullptr;
}
const CheatInfo* CheatStore::Find(std::string_view id) const {
    for (auto& c : cheats) if (c.cheat == id) return &c;
    return nullptr;
}

static nlohmann::json StoreToJson(const CheatStore& s) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& c : s.cheats) {
        nlohmann::json cj = {
            {"cheat", c.cheat}, {"name", c.name},
            {"lifetime", c.lifetime}, {"license", c.license},
            {"selected_branch", c.selected_branch},
        };
        nlohmann::json bs = nlohmann::json::array();
        for (const auto& b : c.branches) {
            nlohmann::json bj = {
                {"id", b.id}, {"name", b.name}, {"version", b.version},
                {"build_date", b.build_date}, {"last_launch", b.last_launch},
            };
            nlohmann::json cl = nlohmann::json::array();
            for (const auto& e : b.changelog)
                cl.push_back({{"date", e.date}, {"changelog", e.text}});
            bj["changelog"] = cl;
            bs.push_back(bj);
        }
        cj["branches"] = bs;
        arr.push_back(cj);
    }
    return arr;
}

bool CheatStore::Persist() const {
    const std::string text = StoreToJson(*this).dump(2);
    return paths::WriteFileUtf8(paths::UserCheatsJson(), text);
}

void CheatStore::Load() {
    const std::wstring path = paths::UserCheatsJson();
    std::string text;
    std::string err;
    bool ok = false;
    if (paths::Exists(path) && paths::ReadFileUtf8(path, text)) {
        ok = ParseStoreJson(text, *this, &err);
        if (!ok) {
            // Quarantine the bad file (mirrors "Quarantined unusable cheats.json").
            std::wstring bak = path + L".bad";
            MoveFileExW(path.c_str(), bak.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
    if (!ok) {
        // Seed from embedded asset and write the user file.
        text = embed::CheatsJson();
        if (ParseStoreJson(text, *this, &err)) {
            paths::WriteFileUtf8(path, StoreToJson(*this).dump(2));
        }
    }
}

bool CheatStore::SaveSelectedBranch(std::string_view cheat, std::string_view branch) {
    CheatInfo* c = Find(cheat);
    if (!c) return false;
    bool valid = false;
    for (auto& b : c->branches) if (b.id == branch) { valid = true; break; }
    if (!valid) return false;
    c->selected_branch = std::string(branch);
    return Persist();
}

bool CheatStore::SaveLastLaunch(std::string_view cheat, std::string_view branch, int64_t unix_ts) {
    CheatInfo* c = Find(cheat);
    if (!c) return false;
    for (auto& b : c->branches) {
        if (b.id == branch) {
            b.last_launch = unix_ts;
            return Persist();
        }
    }
    return false;
}

}  // namespace nl::data
