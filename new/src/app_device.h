#pragma once
#include "common.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>

namespace nl {

// Thin D3D11 wrapper: device, swap chain, render target, clear + present.
// Mirrors the original nl::AppDevice surface area (Init/Resize/Present/ClearAndBind).
class AppDevice {
public:
    AppDevice();
    ~AppDevice();

    bool Init(HWND hwnd, int width, int height);
    void Cleanup();
    bool Resize(int width, int height);
    void UnbindRenderTargets();
    void SetRoundedClip(int center_x, int center_y, int width, int height, int radius);
    void SetOpacity(float opacity);
    void ClearAndBind(const float rgba[4]);
    int  Present(unsigned int vsync = 1);

    ID3D11Device*         Device()      const { return device_; }
    ID3D11DeviceContext*  Context()     const { return context_; }
    IDXGISwapChain*       SwapChain()   const { return swap_; }
    bool                  HasComposition() const { return dcomp_device_ != nullptr; }
    bool                  Valid()       const { return device_ && context_ && swap_ && rtv_; }

private:
    bool InitComposition(HWND hwnd, int width, int height);
    HRESULT InitFallback(HWND hwnd, int width, int height, DXGI_SWAP_EFFECT swap_effect);
    void CreateRenderTarget();
    void CleanupRenderTarget();

    ID3D11Device*           device_  = nullptr;
    ID3D11DeviceContext*    context_ = nullptr;
    IDXGISwapChain*         swap_    = nullptr;
    ID3D11RenderTargetView* rtv_     = nullptr;
    IDCompositionDevice*         dcomp_device_ = nullptr;
    IDCompositionTarget*         dcomp_target_ = nullptr;
    IDCompositionVisual*         dcomp_visual_ = nullptr;
    IDCompositionEffectGroup*    dcomp_effect_ = nullptr;
    IDCompositionRectangleClip*  dcomp_clip_   = nullptr;
    int clip_width_ = -1, clip_height_ = -1;
    int clip_cx_ = -1, clip_cy_ = -1, clip_radius_ = -1;
    float opacity_ = -1.0f;
    int width_ = 0, height_ = 0;
};

}  // namespace nl
