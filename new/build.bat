@echo off
REM Build newnlinjector.exe with the same llvm-mingw (clang++) toolchain the
REM original injector was built with. Just double-click or run from cmd.
setlocal enableextensions

set "ROOT=%~dp0"
set "OUT=%ROOT%newnlinjector.exe"
set "SRC=%ROOT%src"
set "IMGUI=%ROOT%third_party\imgui"
set "BACKENDS=%IMGUI%\backends"
set "TP=%ROOT%third_party"

where clang++ >nul 2>nul
if errorlevel 1 (
  echo [ERROR] clang++ not found in PATH. Install LLVM-MinGW and re-run.
  exit /b 1
)

set INCLUDES=-I%SRC% -I%IMGUI% -I%BACKENDS% -I%TP%
set DEFS=-DNDEBUG
set FLAGS=-std=c++20 -O2 -municode -mwindows -Wno-unused-parameter -Wno-microsoft-anon-tag
set LIBS=-ld3d11 -ldxgi -ldwmapi -ldcomp -lwindowscodecs -luser32 -lgdi32 -lshell32 -lole32 -ladvapi32 -lshcore -lcomdlg32 -lwinhttp -lbcrypt

echo Building newnlinjector.exe ...
clang++ %FLAGS% %DEFS% %INCLUDES% ^
  "%SRC%\main.cpp" "%SRC%\app.cpp" "%SRC%\app_window.cpp" "%SRC%\app_device.cpp" ^
  "%SRC%\app_frame.cpp" "%SRC%\ui_shell.cpp" "%SRC%\ui_draw.cpp" ^
  "%SRC%\ui_dashboard.cpp" "%SRC%\ui_modal.cpp" "%SRC%\ui_settings.cpp" ^
  "%SRC%\gfx_blur.cpp" "%SRC%\texture_loader.cpp" ^
  "%SRC%\paths.cpp" "%SRC%\data_store.cpp" "%SRC%\profile.cpp" "%SRC%\i18n.cpp" ^
  "%SRC%\remote_payload.cpp" "%SRC%\runtime.cpp" "%SRC%\inject_boundary.cpp" ^
  "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_tables.cpp" "%IMGUI%\imgui_widgets.cpp" ^
  "%BACKENDS%\imgui_impl_win32.cpp" "%BACKENDS%\imgui_impl_dx11.cpp" ^
  %LIBS% -o "%OUT%"

if errorlevel 1 ( echo [ERROR] BUILD FAILED & exit /b 1 )
echo [OK] Built %OUT%
endlocal
