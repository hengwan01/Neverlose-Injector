#pragma once
// 本模块不在原版注入器里：系统亚克力按窗口矩形绘制、圆角由 DWM 固定，
// 无法跟随面板的 17px 圆角，所以改为截取窗口背后的桌面画面自行模糊。
//
// 采集走 DXGI Desktop Duplication，降采样与两遍可分离高斯全部在 GPU 上完成，
// 不回读 CPU、也不占用工作线程，因此拖动与动画期间不会再卡顿。

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <mutex>

namespace nl::gfx {

class DesktopBlur {
public:
    DesktopBlur() = default;
    ~DesktopBlur();
    DesktopBlur(const DesktopBlur&) = delete;
    DesktopBlur& operator=(const DesktopBlur&) = delete;

    bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
    void Release();

    // UI 线程：记录需要模糊的窗口矩形（屏幕坐标、物理像素），非阻塞。
    void RequestCapture(int x, int y, int width, int height);
    // UI 线程：切换液体玻璃效果；false 时只做模糊底板。
    void SetLiquidGlass(bool enabled);
    // UI 线程：取回桌面帧并模糊，返回可采样的纹理；不可用时返回 nullptr。
    ID3D11ShaderResourceView* Update();
    bool HasTexture() const { return result_srv_ != nullptr; }

private:
    bool EnsureShaders();
    bool EnsureDuplication(int center_x, int center_y);
    void ReleaseDuplication();
    bool CaptureFrame();
    bool EnsureFrameTexture(const D3D11_TEXTURE2D_DESC& source);
    bool EnsureBlurTargets(int width, int height);
    bool RenderBlur(int x, int y, int width, int height);
    bool RenderGlass(int x, int y, int width, int height);
    bool UploadParams(float uv_min_x, float uv_min_y, float uv_size_x,
                      float uv_size_y, float texel_x, float texel_y,
                      float dir_x, float dir_y, float radius);
    bool DrawPass(ID3D11ShaderResourceView* source,
                  ID3D11ShaderResourceView* second_source,
                  ID3D11RenderTargetView* target, ID3D11PixelShader* shader,
                  ID3D11Buffer* params);
    void DestroyBlurTargets();

    // 设备对象不持有所有权，由 AppDevice 管理。
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> downsample_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> blur_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> glass_ps_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> params_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> glass_params_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_;
    bool shaders_ready_ = false;
    bool shader_attempted_ = false;

    // 整块桌面帧的 GPU 副本（尺寸/格式与桌面一致），窗口移动时可直接重新取样。
    Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> frame_srv_;
    UINT frame_width_ = 0;
    UINT frame_height_ = 0;
    DXGI_FORMAT frame_format_ = DXGI_FORMAT_UNKNOWN;
    bool frame_dirty_ = false;

    // 模糊中间结果（窗口矩形的 1/6）。
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pass_texture_[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pass_srv_[2];
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pass_rtv_[2];
    int blur_width_ = 0;
    int blur_height_ = 0;

    // 液体玻璃合成结果（窗口原始分辨率），最终交给界面采样。
    Microsoft::WRL::ComPtr<ID3D11Texture2D> result_texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> result_srv_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> result_rtv_;
    int glass_width_ = 0;
    int glass_height_ = 0;

    // 桌面复制会话。
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    RECT output_rect_ = {};
    unsigned long long duplication_retry_ms_ = 0;

    std::mutex mutex_;
    bool capture_requested_ = false;
    int request_x_ = 0;
    int request_y_ = 0;
    int request_width_ = 0;
    int request_height_ = 0;

    int last_x_ = 0;
    int last_y_ = 0;
    int last_width_ = 0;
    int last_height_ = 0;
    bool liquid_glass_ = true;
};

}  // namespace nl::gfx
