#include "remote_payload.h"
#include "paths.h"
#include "nlohmann/json.hpp"
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
#include <cctype>

namespace nl {

// ---- bcrypt SHA-256 ------------------------------------------------------
std::string RemotePayload::Sha256Hex(const void* data, size_t n) {
    std::string out;
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return out;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) { BCryptCloseAlgorithmProvider(alg, 0); return out; }
    uint8_t digest[32] = {};
    bool ok = true;
    if (n && BCryptHashData(hash, (PUCHAR)data, (ULONG)n, 0) != 0) ok = false;
    if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) != 0) ok = false;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) return out;
    static const char* kHex = "0123456789abcdef";
    out.resize(64);
    for (int i = 0; i < 32; ++i) {
        out[i*2]   = kHex[digest[i] >> 4];
        out[i*2+1] = kHex[digest[i] & 0xF];
    }
    return out;
}

// ---- WinHTTP GET ---------------------------------------------------------
static bool HttpGet(const std::string& url, std::vector<uint8_t>& body,
                    size_t max_bytes, std::string* err, DWORD* out_status) {
    body.clear();
    if (out_status) *out_status = 0;
    std::wstring wurl = paths::U8toW(url);

    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[512] = {};
    wchar_t path[4096] = {};
    uc.lpszHostName  = host;  uc.dwHostNameLength  = 511;
    uc.lpszUrlPath   = path;  uc.dwUrlPathLength   = 4095;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        if (err) *err = "Invalid GitHub package URL or size limit.";
        return false;
    }

    HINTERNET hSess = WinHttpOpen(L"neverlose-loader/1.0",
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) { if (err) *err = "Cannot start GitHub download (Windows error " + std::to_string(GetLastError()) + ")"; return false; }
    WinHttpSetTimeouts(hSess, 10000, 10000, 30000, 120000);

    HINTERNET hConn = WinHttpConnect(hSess, host, uc.nPort, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); if (err)*err="Cannot start GitHub download (Windows error "+std::to_string(GetLastError())+")"; return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path, nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    bool ok = false;
    if (hReq) {
        if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hReq, nullptr)) {
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
            if (out_status) *out_status = status;
            if (status / 100 == 2) {
                for (;;) {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(hReq, &avail) || !avail) break;
                    if (body.size() + avail > max_bytes) {
                        if (err) *err = "GitHub package exceeds its size limit.";
                        break;
                    }
                    size_t old = body.size();
                    body.resize(old + avail);
                    DWORD got = 0;
                    if (!WinHttpReadData(hReq, body.data() + old, avail, &got)) break;
                    body.resize(old + got);
                    if (!got) break;
                }
                ok = !body.empty() || (err && err->empty());
                if (body.empty() && err && err->empty()) *err = "GitHub download timed out.";
            } else {
                if (err) *err = "GitHub returned HTTP " + std::to_string(status);
            }
        } else {
            if (err) *err = "GitHub download failed (Windows error " + std::to_string(GetLastError()) + ")";
        }
        WinHttpCloseHandle(hReq);
    }
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return ok;
}

// ---- manifest resolution -------------------------------------------------
static bool IsPublicGitHubUrl(const std::string& u) {
    return u.rfind("https://github.com/", 0) == 0 ||
           u.rfind("https://raw.githubusercontent.com/", 0) == 0;
}

std::string RemotePayload::ResolveManifestUrl() {
    std::string text;
    if (paths::ReadFileUtf8(paths::PayloadSourceJson(), text)) {
        try {
            auto j = nlohmann::json::parse(text);
            if (j.is_object() && j.contains("manifest_url") && j["manifest_url"].is_string()) {
                std::string u = j["manifest_url"].get<std::string>();
                if (IsPublicGitHubUrl(u)) return u;
            }
        } catch (...) {}
        // "payload-source.json requires a public GitHub HTTPS release or raw URL."
    }
    return kDefaultManifestUrl;
}

bool RemotePayload::FetchManifest(Manifest& out, std::string* err) {
    const std::string url = ResolveManifestUrl();
    if (!IsPublicGitHubUrl(url)) { if (err) *err = "Invalid GitHub manifest URL."; return false; }
    std::vector<uint8_t> body;
    DWORD status = 0;
    if (!HttpGet(url, body, 1 << 20, err, &status)) return false;   // manifest capped 1 MB
    if (body.empty()) { if (err) *err = "Invalid GitHub manifest size."; return false; }

    nlohmann::json j;
    try { j = nlohmann::json::parse(body.begin(), body.end()); }
    catch (...) { if (err) *err = "GitHub manifest is not a JSON object."; return false; }
    if (!j.is_object()) { if (err) *err = "GitHub manifest is not a JSON object."; return false; }

    out.schema              = j.value("schema", 0);
    out.channel             = j.value("channel", std::string());
    out.version             = j.value("version", std::string());
    out.url                 = j.value("url", std::string());
    out.sha256              = j.value("sha256", std::string());
    out.size                = (uint64_t)j.value("size", 0);
    out.profile_version     = j.value("profile_version", 0);
    out.profile_rva         = (uint64_t)j.value("profile_rva", 0);
    out.profile_export_rva  = (uint64_t)j.value("profile_export_rva", 0);

    if (out.url.empty() || out.sha256.empty() || out.size == 0) {
        if (err) *err = "GitHub manifest is missing package fields.";
        return false;
    }
    if (!IsPublicGitHubUrl(out.url)) { if (err) *err = "Invalid GitHub package metadata."; return false; }
    if (out.sha256.size() != 64) { if (err) *err = "Invalid GitHub package metadata."; return false; }
    return true;
}

// ---- payload ensure (download + verify + cache) --------------------------
static std::string LowerHex(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

bool RemotePayload::EnsurePayload(const Manifest& m, std::wstring& out_dll_path, std::string* err) {
    const std::string sha = LowerHex(m.sha256);
    const std::string short_sha = sha.substr(0, 12);
    const std::wstring dir  = paths::CacheDir() + L"\\" + paths::U8toW(sha);
    paths::EnsureDir(dir);
    const std::wstring file = dir + L"\\NLRelease-" + paths::U8toW(short_sha) + L".dll";

    auto size_matches = [&](const std::wstring& p) -> bool {
        HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz; sz.QuadPart = 0; GetFileSizeEx(h, &sz); CloseHandle(h);
        return (uint64_t)sz.QuadPart == m.size;
    };

    // Cache hit?
    if (paths::Exists(file) && size_matches(file)) {
        std::vector<uint8_t> bytes;
        if (paths::ReadFileBytes(file, bytes) && Sha256Hex(bytes) == sha) {
            out_dll_path = file;
            return true;
        }
    }

    // Download (cap at manifest size + 8 MiB margin).
    const size_t cap = (size_t)m.size + (8u << 20);
    std::vector<uint8_t> body;
    DWORD status = 0;
    std::string local_err;
    if (!HttpGet(m.url, body, cap, &local_err, &status)) {
        if (err) *err = local_err.empty() ? "GitHub download failed." : local_err;
        return false;
    }
    if (body.size() != m.size || Sha256Hex(body) != sha) {
        if (err) *err = "Downloaded DLL size or SHA-256 does not match the GitHub manifest.";
        return false;
    }

    const std::wstring tmp = file + L".part";
    if (!paths::WriteFileBytes(tmp, body.data(), body.size())) {
        if (err) *err = "The DLL cache is not writable. See loader.log for the loading stage.";
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        if (err) *err = "The DLL cache is not writable.";
        return false;
    }
    out_dll_path = file;
    return true;
}

}  // namespace nl
