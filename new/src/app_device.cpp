#include "app_device.h"
#include <dxgi1_2.h>
#include <dcomp.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

namespace nl {

AppDevice::AppDevice() = default;
AppDevice::~AppDevice() { Cleanup(); }

static HWND g_last_hwnd = nullptr;

bool AppDevice::InitComposition(HWND hwnd, int width, int height) {
    SafeRelease(device_);
    SafeRelease(context_);

    D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   &requested, 1, D3D11_SDK_VERSION,
                                   &device_, &got, &context_);
    if (FAILED(hr)) {
        SafeRelease(device_);
        SafeRelease(context_);
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               &requested, 1, D3D11_SDK_VERSION,
                               &device_, &got, &context_);
        if (FAILED(hr)) return false;
    }

    IDXGIDevice* dxgi_device = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory2* factory = nullptr;
    IDXGISwapChain1* composition_swap = nullptr;

    do {
        if (FAILED(device_->QueryInterface(&dxgi_device)) || !dxgi_device) break;
        if (FAILED(dxgi_device->GetAdapter(&adapter)) || !adapter) break;
        if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))) || !factory) break;

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        hr = factory->CreateSwapChainForComposition(device_, &sd, nullptr, &composition_swap);
        if (FAILED(hr)) break;
        hr = composition_swap->QueryInterface(IID_PPV_ARGS(&swap_));
        if (FAILED(hr)) break;
        hr = DCompositionCreateDevice(dxgi_device, IID_PPV_ARGS(&dcomp_device_));
        if (FAILED(hr)) break;

        LONG_PTR exstyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exstyle &= ~(WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP);
        exstyle |= WS_EX_NOREDIRECTIONBITMAP;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exstyle);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);

        hr = dcomp_device_->CreateTargetForHwnd(hwnd, TRUE, &dcomp_target_);
        if (FAILED(hr)) break;
        hr = dcomp_device_->CreateVisual(&dcomp_visual_);
        if (FAILED(hr)) break;
        hr = dcomp_device_->CreateEffectGroup(&dcomp_effect_);
        if (FAILED(hr)) break;
        hr = dcomp_visual_->SetEffect(dcomp_effect_);
        if (FAILED(hr)) break;
        hr = dcomp_visual_->SetContent(swap_);
        if (FAILED(hr)) break;
        hr = dcomp_target_->SetRoot(dcomp_visual_);
        if (FAILED(hr)) break;
        hr = dcomp_effect_->SetOpacity(0.0f);
        if (FAILED(hr)) break;
        hr = dcomp_device_->Commit();
        if (FAILED(hr)) break;

        opacity_ = 0.0f;
        Log("DirectComposition swap chain initialized");
    } while (false);

    SafeRelease(composition_swap);
    SafeRelease(factory);
    SafeRelease(adapter);
    SafeRelease(dxgi_device);
    return SUCCEEDED(hr) && dcomp_device_ && swap_;
}

HRESULT AppDevice::InitFallback(HWND hwnd, int width, int height,
                                DXGI_SWAP_EFFECT swap_effect) {
    SafeRelease(swap_);
    SafeRelease(device_);
    SafeRelease(context_);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.OutputWindow = hwnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = swap_effect;

    D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &requested, 1, D3D11_SDK_VERSION, &sd, &swap_, &device_, &got, &context_);
    if (FAILED(hr)) {
        Log("Hardware D3D11 initialization failed for swap effect %d: 0x%08lx",
            (int)swap_effect, (unsigned long)hr);
        SafeRelease(swap_);
        SafeRelease(device_);
        SafeRelease(context_);
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &requested, 1, D3D11_SDK_VERSION, &sd, &swap_, &device_, &got, &context_);
    }
    return hr;
}

bool AppDevice::Init(HWND hwnd, int width, int height) {
    g_last_hwnd = hwnd;
    width_ = width;
    height_ = height;
    const LONG_PTR original_exstyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    if (InitComposition(hwnd, width, height)) {
        CreateRenderTarget();
        if (rtv_) return true;
    }

    Log("DirectComposition unavailable; using a windowed swap chain");
    Cleanup();
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, original_exstyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOACTIVATE | SWP_FRAMECHANGED);
    width_ = width;
    height_ = height;

    HRESULT hr = InitFallback(hwnd, width, height, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    if (FAILED(hr)) {
        Log("FLIP_DISCARD unavailable; trying DISCARD");
        hr = InitFallback(hwnd, width, height, DXGI_SWAP_EFFECT_DISCARD);
    }
    if (FAILED(hr)) {
        Log("D3D11CreateDeviceAndSwapChain failed: 0x%08lx", (unsigned long)hr);
        return false;
    }
    CreateRenderTarget();
    return rtv_ != nullptr;
}

void AppDevice::CleanupRenderTarget() {
    SafeRelease(rtv_);
}

void AppDevice::Cleanup() {
    UnbindRenderTargets();
    if (dcomp_target_) dcomp_target_->SetRoot(nullptr);
    if (dcomp_visual_) dcomp_visual_->SetContent(nullptr);
    if (dcomp_device_) dcomp_device_->Commit();
    SafeRelease(dcomp_clip_);
    SafeRelease(dcomp_effect_);
    SafeRelease(dcomp_visual_);
    SafeRelease(dcomp_target_);
    SafeRelease(dcomp_device_);
    SafeRelease(swap_);
    SafeRelease(context_);
    SafeRelease(device_);
    clip_width_ = clip_height_ = clip_cx_ = clip_cy_ = clip_radius_ = -1;
    opacity_ = -1.0f;
    width_ = height_ = 0;
}

void AppDevice::CreateRenderTarget() {
    CleanupRenderTarget();
    if (!swap_) return;
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swap_->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
        device_->CreateRenderTargetView(back, nullptr, &rtv_);
        back->Release();
    }
}

void AppDevice::UnbindRenderTargets() {
    if (context_) context_->OMSetRenderTargets(0, nullptr, nullptr);
}

bool AppDevice::Resize(int width, int height) {
    if (!swap_ || width <= 0 || height <= 0) return false;
    if (width == width_ && height == height_ && rtv_) return true;

    UnbindRenderTargets();
    CleanupRenderTarget();
    HRESULT hr = swap_->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        Log("SwapChain::ResizeBuffers failed: 0x%08lx", (unsigned long)hr);
        CreateRenderTarget();
        return false;
    }
    width_ = width;
    height_ = height;
    CreateRenderTarget();
    return rtv_ != nullptr;
}

void AppDevice::SetRoundedClip(int center_x, int center_y, int width, int height, int radius) {
    if (!dcomp_device_ || !dcomp_visual_) return;
    if (clip_width_ == width && clip_height_ == height && clip_cx_ == center_x &&
        clip_cy_ == center_y && clip_radius_ == radius) return;

    if (!dcomp_clip_ && FAILED(dcomp_device_->CreateRectangleClip(&dcomp_clip_))) return;
    const float left = (center_x - width) * 0.5f;
    const float top = (center_y - height) * 0.5f;
    const float right = left + width;
    const float bottom = top + height;
    int clamped_radius = (width < height ? width : height) / 2;
    if (radius < clamped_radius) clamped_radius = radius;
    if (clamped_radius < 0) clamped_radius = 0;
    const float r = (float)clamped_radius;

    if (FAILED(dcomp_clip_->SetLeft(left)) || FAILED(dcomp_clip_->SetTop(top)) ||
        FAILED(dcomp_clip_->SetRight(right)) || FAILED(dcomp_clip_->SetBottom(bottom)) ||
        FAILED(dcomp_clip_->SetTopLeftRadiusX(r)) || FAILED(dcomp_clip_->SetTopLeftRadiusY(r)) ||
        FAILED(dcomp_clip_->SetTopRightRadiusX(r)) || FAILED(dcomp_clip_->SetTopRightRadiusY(r)) ||
        FAILED(dcomp_clip_->SetBottomLeftRadiusX(r)) || FAILED(dcomp_clip_->SetBottomLeftRadiusY(r)) ||
        FAILED(dcomp_clip_->SetBottomRightRadiusX(r)) || FAILED(dcomp_clip_->SetBottomRightRadiusY(r)) ||
        FAILED(dcomp_visual_->SetClip(dcomp_clip_)) || FAILED(dcomp_device_->Commit())) return;

    clip_width_ = width; clip_height_ = height;
    clip_cx_ = center_x; clip_cy_ = center_y; clip_radius_ = radius;
}

void AppDevice::SetOpacity(float opacity) {
    if (!dcomp_device_ || !dcomp_effect_) return;
    if (opacity > 1.0f) opacity = 1.0f;
    if (opacity < 0.0f) opacity = 0.0f;
    const float delta = opacity - opacity_;
    if ((delta < 0.0f ? -delta : delta) < 0.0001f) return;
    if (FAILED(dcomp_effect_->SetOpacity(opacity)) || FAILED(dcomp_device_->Commit())) return;
    opacity_ = opacity;
}

void AppDevice::ClearAndBind(const float rgba[4]) {
    if (context_ && rtv_) {
        context_->OMSetRenderTargets(1, &rtv_, nullptr);
        context_->ClearRenderTargetView(rtv_, rgba);
    }
}

int AppDevice::Present(unsigned int vsync) {
    if (!swap_ || !device_) return 2;
    HRESULT hr = swap_->Present(vsync, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED) hr = device_->GetDeviceRemovedReason();
        Log("Present device lost: 0x%08lx", (unsigned long)hr);
        return 1;
    }
    if (FAILED(hr)) {
        Log("Present failed: 0x%08lx", (unsigned long)hr);
        return 2;
    }
    return 0;
}

}  // namespace nl
