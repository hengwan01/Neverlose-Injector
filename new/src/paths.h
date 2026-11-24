#pragma once
#include "common.h"
#include <string>

// Filesystem layout mirrored from injector.exe:
//   %LOCALAPPDATA%\NLLoader\            (root; override via env NEVERLOSE_LOCALAPPDATA)
//       payloads\<sha256>\*.dll         cached payload DLLs
//       cheats.json                      user cheat store (mutable selected_branch/last_launch)
//       profile.json                     user profile
//       loader.log                       runtime log
//   <exedir>\payload-source.json         optional manifest URL override (beside the loader)
//   <exedir>\assets\                     shipped read-only assets (seed + images + font)
namespace nl::paths {

std::wstring NlRootDir();
void SetUserDataDirOverride(const std::wstring& dir);
std::wstring CacheDir();
std::wstring UserCheatsJson();
std::wstring ProfileJson();
std::wstring LoaderLog();

std::wstring ExeDir();
std::wstring PayloadSourceJson();
std::wstring AssetDir();
std::wstring AssetPath(const wchar_t* name);

bool EnsureDir(const std::wstring& dir);
bool Exists(const std::wstring& path);

std::string  WtoU8(const std::wstring& w);
std::wstring U8toW(const std::string& u);

// Read an entire file as UTF-8 text. Returns false on error.
bool ReadFileUtf8(const std::wstring& path, std::string& out);
bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out);
bool WriteFileBytes(const std::wstring& path, const void* data, size_t n);
bool WriteFileUtf8(const std::wstring& path, const std::string& text);
}  // namespace nl::paths
