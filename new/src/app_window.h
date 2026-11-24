#pragma once
#include "common.h"

namespace nl {

// Interface implemented by AppFrame so the window can ask it about hit-testing.
// Mirrors the original nl::AppWindowHost surface.
struct AppWindowHost {
    virtual ~AppWindowHost() = default;
    virtual bool ForwardRawMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) = 0;
    virtual bool IsDragDisabled() const = 0;
    virtual bool WantCaptureMouse() const = 0;
    virtual bool CanDragAt(POINT p) const = 0;
    virtual void OnBeginDrag() = 0;
    virtual void OnDragMove() = 0;
    virtual void OnEndDrag() = 0;
    virtual void OnRequestClose() = 0;
    virtual void OnDpiChanged(unsigned dpi, int w, int h) = 0;
};

// Borderless, rounded, DWM-Mica Win32 window host.
class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool Create(HINSTANCE hInstance, int width, int height, AppWindowHost* host);
    void Destroy(bool quit);
    void ApplyRoundRegion(int outer_width, int outer_height, int width, int height, int radius);
    void SetLayeredAlpha(unsigned char alpha);
    bool SetBackdropEnabled(bool enabled);

    HWND Hwnd() const { return hwnd_; }
    int  Width() const { return width_; }
    int  Height() const { return height_; }
    unsigned Dpi() const { return dpi_; }
    bool IsDragging() const { return dragging_; }
    // 窗口已被排除在屏幕捕获之外，桌面截图不会把自己的画面拍进去。
    bool CaptureExcluded() const { return capture_excluded_; }

    void BeginDrag();
    void DragTo();
    void EndDrag();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);

    HWND hwnd_ = nullptr;
    AppWindowHost* host_ = nullptr;
    int width_ = 0, height_ = 0;
    unsigned dpi_ = 96;
    bool dragging_ = false;
    bool backdrop_requested_ = false;
    bool backdrop_enabled_ = false;
    bool capture_excluded_ = false;
    unsigned char layered_alpha_ = 0;
    POINT drag_cursor_origin_ = {};
    POINT drag_window_origin_ = {};
};

}  // namespace nl
