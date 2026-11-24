#pragma once

#include <cstdint>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "imgui.h"

namespace nl {
class AppFrame;
}

namespace nl::gfx {
class Blur;
}

namespace nl::ui {
bool QueueBlurCapture(gfx::Blur& blur, ImDrawList* draw_list, bool force);
void BlitBlurSolid(gfx::Blur& blur, ImDrawList* draw_list,
                   float x0, float y0, float x1, float y1,
                   float opacity, float texture_width, float texture_height,
                   float rounding);
}

namespace nl::gfx {

class Blur {
public:
    Blur() = default;
    ~Blur();

    Blur(const Blur&) = delete;
    Blur& operator=(const Blur&) = delete;

    bool Init(ID3D11Device* device, ID3D11DeviceContext* context,
              IDXGISwapChain* swap_chain);
    void OnResize(IDXGISwapChain* swap_chain);
    void Shutdown();

    bool QueueCapture(bool force);
    void ExecutePending();

private:
    bool CreateDeviceObjects(IDXGISwapChain* swap_chain);
    void DestroyDeviceObjects();
    bool RunBlur();
    bool EnsureSize();

    // Non-owning D3D objects supplied by AppDevice.
    ID3D11Device* device_ = nullptr;                    // 0x00
    ID3D11DeviceContext* context_ = nullptr;            // 0x08
    IDXGISwapChain* swap_chain_ = nullptr;              // 0x10

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;      // 0x18
    Microsoft::WRL::ComPtr<ID3D11PixelShader> downsample_ps_; // 0x20
    Microsoft::WRL::ComPtr<ID3D11PixelShader> blur_ps_;  // 0x28
    Microsoft::WRL::ComPtr<ID3D11Buffer> input_buffer_;  // 0x30
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_; // 0x38
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_; // 0x40
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_; // 0x48
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;     // 0x50

    Microsoft::WRL::ComPtr<ID3D11Texture2D> capture_texture_; // 0x58
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> capture_srv_; // 0x60

    Microsoft::WRL::ComPtr<ID3D11Texture2D> downsample_texture_; // 0x68
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> downsample_srv_; // 0x70
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> downsample_rtv_; // 0x78

    Microsoft::WRL::ComPtr<ID3D11Texture2D> blur_texture_; // 0x80
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blur_srv_; // 0x88
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> blur_rtv_; // 0x90

    std::uint32_t width_ = 0;                           // 0x98
    std::uint32_t height_ = 0;                          // 0x9C
    std::uint32_t half_width_ = 0;                      // 0xA0
    std::uint32_t half_height_ = 0;                     // 0xA4
    bool ready_ = false;                                // 0xA8
    bool capture_pending_ = false;                      // 0xA9
    bool has_result_ = false;                           // 0xAA

    friend class nl::AppFrame;
    friend bool ui::QueueBlurCapture(Blur&, ImDrawList*, bool);
    friend void ui::BlitBlurSolid(Blur&, ImDrawList*, float, float, float, float,
                                  float, float, float, float);
};

} // namespace nl::gfx
