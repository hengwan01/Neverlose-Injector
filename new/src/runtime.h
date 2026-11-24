#pragma once

#include "common.h"
#include "profile.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace nl {

struct RuntimeSnapshot {
    bool running = false;
    bool error = false;
    bool ok = false;
    float overlay_target = 0.0f;
    std::uint32_t revision = 0;
    std::string text;
    bool need_restart_admin = false;
};

class Runtime {
public:
    explicit Runtime(bool preview_mode = false);
    ~Runtime();

    RuntimeSnapshot Status() const;
    bool Start(Profile profile, bool profile_update);
    void Cancel();

private:
    void Run(std::stop_token stop_token, Profile profile, bool profile_update);
    void Publish(std::string text, float overlay_target, bool running,
                 bool error, bool ok, bool need_restart_admin);
    void Log(const std::string& line);

    bool preview_mode_ = false;
    std::jthread thread_;
    mutable std::mutex mtx_;
    RuntimeSnapshot snapshot_;
};

static_assert(sizeof(RuntimeSnapshot) == 0x30, "Unexpected RuntimeSnapshot size");
static_assert(offsetof(RuntimeSnapshot, running) == 0x00, "Unexpected running offset");
static_assert(offsetof(RuntimeSnapshot, error) == 0x01, "Unexpected error offset");
static_assert(offsetof(RuntimeSnapshot, ok) == 0x02, "Unexpected ok offset");
static_assert(offsetof(RuntimeSnapshot, overlay_target) == 0x04, "Unexpected overlay offset");
static_assert(offsetof(RuntimeSnapshot, revision) == 0x08, "Unexpected revision offset");
static_assert(offsetof(RuntimeSnapshot, text) == 0x10, "Unexpected status text offset");
static_assert(offsetof(RuntimeSnapshot, need_restart_admin) == 0x28,
              "Unexpected admin restart offset");

}  // namespace nl
