#include "app_window.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// Newer DWM attributes that older mingw headers may not declare.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif

namespace nl {

static const wchar_t* kClassName = L"NlLoaderWnd";

AppWindow::AppWindow() = default;
AppWindow::~AppWindow() { Destroy(false); }

bool AppWindow::Create(HINSTANCE hInstance, int width, int height, AppWindowHost* host) {
    host_ = host;
    width_ = width; height_ = height;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &AppWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(201),
                                             IMAGE_ICON, 32, 32, LR_SHARED));
    wc.hIconSm = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(201),
                                               IMAGE_ICON, 16, 16, LR_SHARED));
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Log("RegisterClassExW failed: %lu", GetLastError());
        return false;
    }

    int sx = 0;
    int sy = 0;
    const POINT origin = {};
    const HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfoW(monitor, &monitor_info)) {
        const RECT& work = monitor_info.rcWork;
        sx = work.left + (work.right - work.left - width) / 2;
        sy = work.top + (work.bottom - work.top - height) / 2;
    } else {
        sx = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        sy = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_APPWINDOW,
        kClassName, L"neverlosecs2",
        WS_POPUP | WS_VISIBLE,
        sx, sy, width, height,
        nullptr, nullptr, hInstance, this);
    if (!hwnd_) {
        Log("CreateWindowExW failed: %lu", GetLastError());
        return false;
    }

    dpi_ = GetDpiForWindow(hwnd_);
    if (!dpi_) dpi_ = 96;

    SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
    layered_alpha_ = 0;
    return true;
}

bool AppWindow::SetBackdropEnabled(bool enabled) {
    if (!hwnd_) return false;
    if (backdrop_requested_ == enabled) return backdrop_enabled_;
    backdrop_requested_ = enabled;
    backdrop_enabled_ = false;

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    int corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    MARGINS margins = {};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    int no_backdrop = DWMSBT_NONE;
    DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &no_backdrop, sizeof(no_backdrop));

    HRGN rgn = CreateRectRgn(0, 0, 0, 0);
    if (rgn) {
        if (GetWindowRgn(hwnd_, rgn) != ERROR) {
            DWM_BLURBEHIND bb = {};
            bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
            bb.fEnable = enabled;
            bb.hRgnBlur = rgn;
            DwmEnableBlurBehindWindow(hwnd_, &bb);
        }
        DeleteObject(rgn);
    }

    struct AccentPolicy {
        DWORD state;
        DWORD flags;
        DWORD color;
        DWORD animation;
    };
    struct WindowCompositionAttribData {
        UINT attrib;
        PVOID data;
        SIZE_T size;
    };
    using SetWindowCompositionAttributeFn =
        BOOL(WINAPI*)(HWND, WindowCompositionAttribData*);

    AccentPolicy accent = {};
    accent.state = enabled ? 3 : 0;
    WindowCompositionAttribData data = {};
    data.attrib = 19;
    data.data = &accent;
    data.size = sizeof(accent);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto set_composition = reinterpret_cast<SetWindowCompositionAttributeFn>(
        user32 ? GetProcAddress(user32, "SetWindowCompositionAttribute") : nullptr);
    if (set_composition && set_composition(hwnd_, &data)) {
        backdrop_enabled_ = enabled;
        return backdrop_enabled_;
    }

    int backdrop = enabled ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
    if (SUCCEEDED(DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE,
                                       &backdrop, sizeof(backdrop))))
        backdrop_enabled_ = enabled;
    return backdrop_enabled_;
}

void AppWindow::ApplyRoundRegion(int outer_width, int outer_height,
                                 int width, int height, int radius) {
    if (!hwnd_) return;
    width = std::min(std::max(width, 1), outer_width);
    height = std::min(std::max(height, 1), outer_height);
    const int left = (outer_width - width) / 2;
    const int top = (outer_height - height) / 2;
    const int ellipse = std::min(std::max(radius, 0) * 2,
                                 std::min(width, height));
    HRGN rgn = CreateRoundRectRgn(left, top, left + width, top + height,
                                  ellipse, ellipse);
    if (!rgn) { Log("CreateRoundRectRgn failed"); return; }

    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = backdrop_enabled_;
    bb.hRgnBlur = rgn;
    DwmEnableBlurBehindWindow(hwnd_, &bb);

    if (!SetWindowRgn(hwnd_, rgn, TRUE)) {
        Log("SetWindowRgn failed: %lu", GetLastError());
        DeleteObject(rgn);
    }
}

void AppWindow::SetLayeredAlpha(unsigned char alpha) {
    if (!hwnd_ || !(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE) & WS_EX_LAYERED) ||
        layered_alpha_ == alpha) return;
    if (SetLayeredWindowAttributes(hwnd_, 0, alpha, LWA_ALPHA))
        layered_alpha_ = alpha;
    else
        Log("SetLayeredWindowAttributes failed: %lu", GetLastError());
}

void AppWindow::Destroy(bool quit) {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (quit) PostQuitMessage(0);
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AppWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->Handle(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT AppWindow::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    if (host_ && host_->ForwardRawMessage(hwnd_, msg, wp, lp))
        return 1;

    switch (msg) {
    case WM_SIZE: {
        if (wp != SIZE_MINIMIZED) {
            width_ = LOWORD(lp);
            height_ = HIWORD(lp);
            if (host_) host_->OnDpiChanged(dpi_, width_, height_);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wp);
        auto r = reinterpret_cast<RECT*>(lp);
        width_ = r->right - r->left;
        height_ = r->bottom - r->top;
        SetWindowPos(hwnd_, nullptr, r->left, r->top, width_, height_,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (host_) host_->OnDpiChanged(dpi_, width_, height_);
        return 0;
    }
    case WM_NCHITTEST: {
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd_, &p);
        if (host_ && host_->CanDragAt(p)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_LBUTTONDOWN:
        if (host_ && !host_->WantCaptureMouse() && !host_->IsDragDisabled())
            host_->OnBeginDrag();
        return 0;
    case WM_MOUSEMOVE:
        if (dragging_ && host_) host_->OnDragMove();
        return 0;
    case WM_LBUTTONUP:
        if (dragging_ && host_) host_->OnEndDrag();
        return 0;
    case WM_CAPTURECHANGED:
        if (dragging_ && reinterpret_cast<HWND>(lp) != hwnd_)
            dragging_ = false;
        return 0;
    case WM_CLOSE:
        if (host_) host_->OnRequestClose();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

void AppWindow::BeginDrag() {
    if (dragging_ || (host_ && host_->WantCaptureMouse())) return;
    dragging_ = true;
    SetCapture(hwnd_);
    GetCursorPos(&drag_cursor_origin_);
    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    drag_window_origin_ = {rect.left, rect.top};
}

void AppWindow::DragTo() {
    if (!dragging_) return;
    POINT cursor{};
    GetCursorPos(&cursor);
    SetWindowPos(hwnd_, nullptr,
                 drag_window_origin_.x + cursor.x - drag_cursor_origin_.x,
                 drag_window_origin_.y + cursor.y - drag_cursor_origin_.y,
                 0, 0, 0x15);
}

void AppWindow::EndDrag() {
    dragging_ = false;
    if (GetCapture() == hwnd_)
        ReleaseCapture();
}

}  // namespace nl
