#pragma once
#include "common.h"
#include "profile.h"
#include "remote_payload.h"
#include <cstdint>
#include <string>

// INTENTIONAL BOUNDARY.
//
// injector.exe's real injection primitive is implemented inside nl::Runtime::Run
// and uses: OpenProcess -> VirtualAllocEx -> WriteProcessMemory ->
// CreateRemoteThread(LoadLibraryA) to load the payload DLL into the target
// game, then a second remote call into the DLL export at profile_export_rva to
// apply the user profile (writes the serialized profile into the image at
// profile_rva first).
//
// That remote-thread / cross-process memory-write code is intentionally NOT
// reproduced here. Replace the stub in inject_boundary.cpp to complete the
// loader; the rest of the pipeline (manifest fetch, download, SHA-256 verify,
// cache, Steam launch, process wait, status reporting) is fully wired.
namespace nl::inject {

bool InjectDllAndApplyProfile(uint32_t pid,
                              const std::wstring& dll_path,
                              const Profile& profile,
                              const Manifest& manifest,
                              std::string* err);

}  // namespace nl::inject
