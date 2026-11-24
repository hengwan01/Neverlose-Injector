#pragma once

// Minimal Windows include set shared by every translation unit.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000007
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif

#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

template <typename T>
inline void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

// Tiny debug logger -> %TEMP%\newnlinjector.log (mirrors original loader.log habit).
namespace nl {
inline void Log(const char* fmt, ...) {
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"newnlinjector.log");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
}
}  // namespace nl
