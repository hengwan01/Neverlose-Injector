#include "runtime.h"
#include "i18n.h"
#include "remote_payload.h"
#include "inject_boundary.h"
#include "data_store.h"
#include "paths.h"
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace nl {

Runtime::Runtime(bool preview_mode) : preview_mode_(preview_mode) {}

Runtime::~Runtime() {
    Cancel();
    if (thread_.joinable())
        thread_.join();
}

RuntimeSnapshot Runtime::Status() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return snapshot_;
}

bool Runtime::Start(Profile profile, bool profile_update) {
    if (Status().running)
        return false;
    if (thread_.joinable())
        thread_.join();

    if (preview_mode_) {
        if (profile_update) {
            Publish("Profile saved (preview mode).", 0.0f, false, false, false, false);
        } else {
            Publish("Preview mode: game loading is disabled.", 0.0f,
                    false, true, false, false);
        }
        return false;
    }

    Publish(profile_update ? "Updating profile..." : "Preparing CS2...",
            0.05f, true, false, false, false);
    thread_ = std::jthread(
        [this, profile = std::move(profile), profile_update](std::stop_token stop_token) mutable {
            Run(stop_token, std::move(profile), profile_update);
        });
    return true;
}

void Runtime::Cancel() {
    thread_.request_stop();
}

void Runtime::Publish(std::string text, float overlay_target, bool running,
                      bool error, bool ok, bool need_restart_admin) {
    std::lock_guard<std::mutex> lock(mtx_);
    ++snapshot_.revision;
    snapshot_.running = running;
    snapshot_.error = error;
    snapshot_.ok = ok;
    snapshot_.overlay_target = overlay_target;
    snapshot_.text = std::move(text);
    snapshot_.need_restart_admin = need_restart_admin;
    Log(snapshot_.text);
}

void Runtime::Log(const std::string& line) {
    SYSTEMTIME st; GetSystemTime(&st);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02uZ pid=%lu ",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                  GetCurrentProcessId());
    std::string full = std::string(buf) + line + "\r\n";
    HANDLE h = CreateFileW(paths::LoaderLog().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wrote; WriteFile(h, full.data(), (DWORD)full.size(), &wrote, nullptr);
        CloseHandle(h);
    }
}

// ---- process helpers -----------------------------------------------------
static std::wstring ExeForCheat(const std::string& cheat) {
    if (cheat == "cs2" || cheat == "csgo") return L"cs2.exe";
    if (cheat == "valorant") return L"VALORANT-Win64-Shipping.exe";
    if (cheat == "apex") return L"r5apex.exe";
    return L"cs2.exe";
}
static uint32_t SteamAppIdForCheat(const std::string& cheat) {
    if (cheat == "cs2" || cheat == "csgo") return 730;
    if (cheat == "apex") return 1172470;
    return 0;
}

static uint32_t FindProcessByName(const std::wstring& exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {sizeof(pe)};
    uint32_t pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exe.c_str()) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static uint32_t WaitForProcess(const std::wstring& exe, int timeout_sec, std::stop_token st) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (st.stop_requested()) return 0;
        uint32_t pid = FindProcessByName(exe);
        if (pid) return pid;
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() >= timeout_sec)
            return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

static bool AmIElevated() {
    HANDLE tok = nullptr;
    BOOL elevated = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        DWORD ret = 0;
        GetTokenInformation(tok, TokenElevation, &elevated, sizeof(elevated), &ret);
        CloseHandle(tok);
    }
    return elevated != FALSE;
}

static bool IsTargetElevated(uint32_t pid) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;
    HANDLE tok = nullptr; BOOL elevated = FALSE;
    if (OpenProcessToken(proc, TOKEN_QUERY, &tok)) {
        DWORD ret = 0;
        GetTokenInformation(tok, TokenElevation, &elevated, sizeof(elevated), &ret);
        CloseHandle(tok);
    }
    CloseHandle(proc);
    return elevated != FALSE;
}

static bool IsTargetX64(uint32_t pid) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;
    BOOL wow = FALSE;
    bool x64 = IsWow64Process(proc, &wow) ? (wow == FALSE) : false;
    CloseHandle(proc);
    return x64;
}

// ---- the flow ------------------------------------------------------------
void Runtime::Run(std::stop_token st, Profile profile, bool profile_update) {
    if (profile_update && FindProcessByName(L"cs2.exe") == 0) {
        Publish("Profile saved. It will be applied on the next Load.",
                0.0f, false, false, false, false);
        return;
    }

    const bool ru = i18n::Russian();
    const std::string cheat_id = "cs2";

    RemotePayload rp;
    Manifest m;
    std::string err;
    if (!rp.FetchManifest(m, &err)) {
        Publish(err, 1.0f, false, true, false, false);
        return;
    }

    Publish(i18n::T("Verifying the DLL package..."), 1.0f,
            true, false, false, false);
    std::wstring dll;
    if (!rp.EnsurePayload(m, dll, &err)) {
        Publish(err, 1.0f, false, true, false, false);
        return;
    }

    const std::wstring exe = ExeForCheat(cheat_id);
    const uint32_t appid = SteamAppIdForCheat(cheat_id);

    if (profile.auto_start && appid != 0) {
        Publish(i18n::T("Starting CS2 through Steam..."), 1.0f,
                true, false, false, false);
        std::wstring uri = L"steam://run/" + std::to_wstring(appid);
        ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    Publish(ru ? "Ожидание запуска игры..." : "Waiting for the game to start...",
            1.0f, true, false, false, false);
    uint32_t pid = WaitForProcess(exe, 90, st);
    if (st.stop_requested()) {
        RuntimeSnapshot snapshot = Status();
        Publish(std::move(snapshot.text), 0.0f, false, false, false, false);
        return;
    }
    if (!pid) {
        Publish(i18n::T("Timed out waiting for CS2. Check Steam."), 1.0f,
                false, true, false, false);
        return;
    }

    if (IsTargetElevated(pid) && !AmIElevated()) {
        Publish(i18n::T("CS2 is running as administrator."), 1.0f,
                false, true, false, true);
        return;
    }
    if (!IsTargetX64(pid)) {
        Publish(i18n::T("The target CS2 process must be x64."), 1.0f,
                false, true, false, false);
        return;
    }

    if (!profile.auto_inject) {
        Publish(i18n::T("CS2 launch requested. Press Load when the game is ready."),
                1.0f, false, false, true, false);
        return;
    }

    Publish(i18n::T("Loading Release..."), 1.0f, true, false, false, false);
    std::string inj_err;
    const bool injected = inject::InjectDllAndApplyProfile(pid, dll, profile, m, &inj_err);
    if (injected) {
        Publish(i18n::T("Release loaded. Profile applied."), 1.0f,
                false, false, true, false);
    } else {
        Publish(inj_err.empty() ? std::string(i18n::T("Loading Release...")) : inj_err,
                1.0f, false, true, false, false);
    }
}

}  // namespace nl
