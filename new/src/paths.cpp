#include "paths.h"
#include <shlobj.h>

namespace nl::paths {

static std::wstring g_user_data_dir_override;

void SetUserDataDirOverride(const std::wstring& dir) {
    g_user_data_dir_override = dir;
}

static std::wstring ModuleDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p = buf;
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

std::wstring NlRootDir() {
    if (!g_user_data_dir_override.empty()) {
        EnsureDir(g_user_data_dir_override);
        return g_user_data_dir_override;
    }

    // Mirror of nl::UserDataDirOverride(): env override wins (used for tests).
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"NEVERLOSE_LOCALAPPDATA", buf, MAX_PATH);
    std::wstring base;
    if (n > 0 && n < MAX_PATH) {
        base = buf;
    } else {
        n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) base = buf;
        else base = L".";
    }
    std::wstring root = base + L"\\NLLoader";
    EnsureDir(root);
    return root;
}

std::wstring CacheDir() {
    std::wstring d = NlRootDir() + L"\\payloads";
    EnsureDir(d);
    return d;
}
std::wstring UserCheatsJson()  { return NlRootDir() + L"\\cheats.json"; }
std::wstring ProfileJson()     { return NlRootDir() + L"\\profile.json"; }
std::wstring LoaderLog()       { return NlRootDir() + L"\\loader.log"; }
std::wstring ExeDir()          { return ModuleDir(); }
std::wstring PayloadSourceJson(){ return ModuleDir() + L"\\payload-source.json"; }
std::wstring AssetDir()        { return ModuleDir() + L"\\assets"; }
std::wstring AssetPath(const wchar_t* name) { return AssetDir() + L"\\" + name; }

bool EnsureDir(const std::wstring& dir) {
    if (dir.empty()) return false;
    if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    // Create intermediate parents.
    std::wstring acc;
    for (size_t i = 0; i < dir.size(); ++i) {
        wchar_t c = dir[i];
        acc.push_back(c);
        if (c == L'\\' || c == L'/' || i + 1 == dir.size()) {
            if (!acc.empty() && acc.back() != L'\\' && acc.back() != L':') {
                CreateDirectoryW(acc.c_str(), nullptr);
            }
        }
    }
    return GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool Exists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string WtoU8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}
std::wstring U8toW(const std::string& u) {
    if (u.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), out.data(), n);
    return out;
}

bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; sz.QuadPart = 0;
    GetFileSizeEx(h, &sz);
    out.resize((size_t)sz.QuadPart);
    DWORD got = 0; bool ok = true;
    if (sz.QuadPart && !ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr)) ok = false;
    CloseHandle(h);
    return ok;
}
bool ReadFileUtf8(const std::wstring& path, std::string& out) {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes)) return false;
    // Strip UTF-8 BOM if present.
    size_t off = (bytes.size() >= 3 && bytes[0]==0xEF && bytes[1]==0xBB && bytes[2]==0xBF) ? 3 : 0;
    out.assign((const char*)bytes.data() + off, bytes.size() - off);
    return true;
}
bool WriteFileBytes(const std::wstring& path, const void* data, size_t n) {
    EnsureDir([&]{ std::wstring p=path; size_t s=p.find_last_of(L"\\/"); return s==std::wstring::npos?L".":p.substr(0,s); }());
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0; bool ok = true;
    if (n && !WriteFile(h, data, (DWORD)n, &wrote, nullptr)) ok = false;
    CloseHandle(h);
    return ok && wrote == n;
}
bool WriteFileUtf8(const std::wstring& path, const std::string& text) {
    return WriteFileBytes(path, text.data(), text.size());
}

}  // namespace nl::paths
