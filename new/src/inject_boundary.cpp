#include "inject_boundary.h"
#include "i18n.h"

namespace nl::inject {

// ---------------------------------------------------------------------------
// Boundary stub (analysis-only — no cross-process injection code is provided).
//
// What the original injector.exe does here (reconstructed from imports +
// nl::Runtime::Run disassembly + the release manifest):
//
//   1. OpenProcess(PROCESS_ALL_ACCESS) on the target pid (e.g. cs2.exe), after
//      confirming x64 via IsWow64Process and that the required system DLLs are
//      already loaded (K32EnumProcessModulesEx / K32GetModuleBaseNameW).
//   2. Validate the on-disk DLL PE (its import table lists the checked libs:
//      d3d11/d3dcompiler_47/kernel32/user32/gdi32/advapi32/shell32/ole32/
//      winhttp/imm32) and that profile_version / profile_rva /
//      profile_export_rva from the manifest look sane.
//   3. Allocate a small buffer in the target with VirtualAllocEx, write the
//      cached DLL path with WriteProcessMemory, then CreateRemoteThread on
//      LoadLibraryA to load the DLL into the game.
//   4. Wait for the remote thread; verify the DLL stayed loaded by enumerating
//      the target's modules again (K32EnumProcessModulesEx) and matching the
//      base name.
//   5. Serialize the user profile to a blob, allocate + write it into the
//      target at the DLL's profile_rva, and CreateRemoteThread a second time
//      on the resolved export address (image_base + profile_export_rva) to
//      apply it.
//
// Implementing steps 1-5 would reproduce working process injection / remote
// code execution into a live game, so they are intentionally left out. Fill
// this function in to finish the loader; everything else is already wired and
// tested to compile/run.
// ---------------------------------------------------------------------------
bool InjectDllAndApplyProfile(uint32_t /*pid*/,
                              const std::wstring& /*dll_path*/,
                              const Profile& /*profile*/,
                              const Manifest& /*manifest*/,
                              std::string* err) {
    if (err) {
        *err = "Injection primitive is not included in this build "
               "(see src/inject_boundary.cpp). Payload download/verify/cache "
               "and Steam launch are fully implemented.";
    }
    (void)i18n::T("");  // keep i18n linked for localized status text elsewhere
    return false;
}

}  // namespace nl::inject
