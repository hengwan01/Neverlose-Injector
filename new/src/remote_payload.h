#pragma once
#include "common.h"
#include <cstdint>
#include <string>
#include <vector>

// Mirrors injector.exe's nl::RemotePayload module:
//   * resolves the manifest URL (payload-source.json beside the exe overrides)
//   * fetches the GitHub release.json manifest
//   * downloads + SHA-256-verifies the payload DLL into
//     %LOCALAPPDATA%\NLLoader\payloads\<sha256>\NLRelease-<sha256_12>.dll
namespace nl {

struct Manifest {
    int         schema = 0;
    std::string channel;
    std::string version;
    std::string url;          // DLL download URL
    std::string sha256;       // expected hex sha256 (lowercase)
    uint64_t    size = 0;     // expected DLL size in bytes
    int         profile_version = 0;
    uint64_t    profile_rva = 0;
    uint64_t    profile_export_rva = 0;
};

class RemotePayload {
public:
    // Default manifest; overridable via <exedir>\payload-source.json {"manifest_url":...}.
    static constexpr const char* kDefaultManifestUrl =
        "https://github.com/PbLJk/neverlose/releases/latest/download/release.json";

    std::string ResolveManifestUrl();
    bool        FetchManifest(Manifest& out, std::string* err);
    // Download/verify/cache the DLL; returns the cached path on success.
    bool        EnsurePayload(const Manifest& m, std::wstring& out_dll_path, std::string* err);

    // bcrypt SHA-256 -> lowercase hex.
    static std::string Sha256Hex(const void* data, size_t n);
    static std::string Sha256Hex(const std::vector<uint8_t>& v) { return Sha256Hex(v.data(), v.size()); }
};

}  // namespace nl
