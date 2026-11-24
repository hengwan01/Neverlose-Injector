#include "desktop_blur.h"

#include "common.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cfloat>

namespace nl::gfx {
namespace {

// 模糊在 1/6 分辨率上做，配合两遍可分离高斯，等效全分辨率约 15px 的柔化。
constexpr int kDownscale = 6;

// 一次桌面复制失败后暂停重试，避免不支持该功能的机器每帧都去建复制会话。
constexpr unsigned long long kDuplicationRetryMs = 2000;

const char kShaderSource[] = R"(
struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

cbuffer Params : register(b0) {
    float2 uv_min;    // 取样区域左上角（源纹理 uv）
    float2 uv_size;   // 取样区域大小（源纹理 uv）
    float2 texel;     // 1 / 源纹理尺寸
    float2 dir;       // 高斯方向（源纹素单位）
    float  radius;    // 每次采样的步长（源纹素）
    float3 pad;
};

Texture2D    src_tex     : register(t0);
SamplerState src_sampler : register(s0);

VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

// 3x3 双线性取样近似 6x6 盒式降采样。
float4 PSDownsample(VSOut i) : SV_Target {
    float2 uv = uv_min + i.uv * uv_size;
    float2 offset = texel * radius;
    float4 sum = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            sum += src_tex.Sample(src_sampler, uv + float2(x, y) * offset);
        }
    }
    return float4(sum.rgb / 9.0, 1.0);
}

// 9 taps 的高斯权重（sigma ~= 2.5）。
static const float kWeights[5] = { 0.1716, 0.1584, 0.1246, 0.0835, 0.0477 };

float4 PSBlur(VSOut i) : SV_Target {
    float2 uv = uv_min + i.uv * uv_size;
    float3 sum = src_tex.Sample(src_sampler, uv).rgb * kWeights[0];
    [unroll] for (int k = 1; k <= 4; ++k) {
        float2 offset = dir * texel * (radius * k);
        sum += src_tex.Sample(src_sampler, uv + offset).rgb * kWeights[k];
        sum += src_tex.Sample(src_sampler, uv - offset).rgb * kWeights[k];
    }
    return float4(sum, 1.0);
}

// ---------------------------------------------------------------------------
// 液体玻璃：移植自 SoyBeanMilkx/imgui-liquid-glass（原实现为 GLSL，
// 算法来自 AndroidLiquidGlassView, (c) 2025 QmDeve, MIT）。
// 形状用圆角矩形 SDF，靠近边缘的一段做折射位移与色散，再叠一层边缘高光。
// ---------------------------------------------------------------------------

cbuffer GlassParams : register(b0) {
    float4 glass_radii;       // 左上、右上、右下、左下圆角
    float4 glass_refraction;  // 折射带宽度、位移量、深度系数、色散强度
    float4 glass_filter;      // 对比度、白点、饱和度、不透明度
    float4 glass_lighting;    // 内阴影、边缘辉光、高光、外阴影
    float4 glass_light;       // 光方向 xy、阴影范围、高光锐度
    float4 glass_raw_uv;      // 原图在桌面帧里的 uv 区域
    float2 glass_size;        // 玻璃形状尺寸（像素）
    float2 glass_mix;         // x = 模糊图所占比例
};

Texture2D raw_tex : register(t1);

// 原图负责边缘折射的细节，模糊图负责整体的磨砂感，两者按比例混合。
float3 SampleGlass(float2 uv) {
    float3 blurred = src_tex.Sample(src_sampler, uv).rgb;
    float2 raw_uv = glass_raw_uv.xy + uv * glass_raw_uv.zw;
    float3 raw = raw_tex.Sample(src_sampler, raw_uv).rgb;
    return lerp(raw, blurred, glass_mix.x);
}

float sdRoundedRect(float2 p, float2 half_size, float radius) {
    float2 corner = abs(p) - (half_size - radius);
    float outside = length(max(corner, 0.0)) - radius;
    float inside = min(max(corner.x, corner.y), 0.0);
    return outside + inside;
}

float antialiasedCoverage(float signed_distance) {
    float edge_width = max(fwidth(signed_distance), 0.5);
    return 1.0 - smoothstep(-edge_width, edge_width, signed_distance);
}

// 对局部直线边缘做精确像素覆盖率，避免小尺寸时边缘发虚。
float pixelCoverage(float signed_distance, float2 gradient) {
    float gradient_length = length(gradient);
    if (gradient_length < 1e-4)
        return signed_distance < 0.0 ? 1.0 : 0.0;
    float2 normal = abs(gradient) / gradient_length;
    float major = max(normal.x, normal.y);
    float minor = min(normal.x, normal.y);
    float offset = -signed_distance / gradient_length;
    float extent = 0.5 * (major + minor);
    if (offset <= -extent)
        return 0.0;
    if (offset >= extent)
        return 1.0;
    if (minor < 1e-4 || abs(offset) <= 0.5 * (major - minor))
        return clamp(0.5 + offset / major, 0.0, 1.0);
    float corner = extent - abs(offset);
    float area = corner * corner / (2.0 * major * minor);
    return offset < 0.0 ? area : 1.0 - area;
}

float glassSafeSign(float value) {
    return value < 0.0 ? -1.0 : 1.0;
}

float2 glassSafeNormalize(float2 value, float2 fallback) {
    float value_length = length(value);
    return value_length > 0.001 ? value / value_length : fallback;
}

float2 gradSdRoundedRect(float2 p, float2 half_size, float radius) {
    float2 corner = abs(p) - (half_size - radius);
    if (corner.x >= 0.0 || corner.y >= 0.0) {
        float2 outside = max(corner, 0.0);
        float outside_length = length(outside);
        if (outside_length > 0.001)
            return sign(p) * (outside / outside_length);
        float use_x = step(corner.y, corner.x);
        return float2(use_x * glassSafeSign(p.x),
                      (1.0 - use_x) * glassSafeSign(p.y));
    }
    float use_x = step(corner.y, corner.x);
    return sign(p) * float2(use_x, 1.0 - use_x);
}

float circleMap(float value) {
    float mapped = clamp(value, -1.0, 1.0);
    return 1.0 - sqrt(max(0.0, 1.0 - mapped * mapped));
}

float rimIntegral(float t, float h) {
    float u = 1.0 - sqrt(clamp(t / h, 0.0, 1.0));
    float u6 = u * u * u * u * u * u;
    return 2.0 * h * u6 * (u / 7.0 - 1.0 / 6.0);
}

float filteredRim(float signed_distance, float edge_width,
                  float2 distance_gradient) {
    float h = max(edge_width, 0.001);
    float half_pixel_width =
        0.5 * max(abs(distance_gradient.x) + abs(distance_gradient.y), 1e-4);
    float t0 = max(-signed_distance - half_pixel_width, 0.0);
    float t1 = max(-signed_distance + half_pixel_width, 0.0);
    if (t1 <= 0.0)
        return 0.0;
    if (t1 - t0 < 1e-4) {
        float u = 1.0 - sqrt(clamp(t0 / h, 0.0, 1.0));
        return u * u * u * u * u;
    }
    return (rimIntegral(t1, h) - rimIntegral(t0, h)) / (t1 - t0);
}

float3 applyGlassLighting(float3 color, float signed_distance, float2 normal,
                          float edge_width, float2 distance_gradient,
                          float4 lighting, float4 light) {
    float rim = filteredRim(signed_distance, edge_width, distance_gradient);
    float2 light_direction =
        glassSafeNormalize(light.xy, float2(-0.7071, -0.7071));

    float shadow_facing = max(dot(normal, -light_direction), 0.0);
    color *= 1.0 - lighting.x * rim * shadow_facing;
    color += lighting.y * rim * (1.0 - color);

    float specular =
        pow(max(dot(normal, light_direction), 0.0), max(light.w, 1.0));
    color += lighting.z * rim * specular * (1.0 - color);
    return color;
}

float4 PSGlass(VSOut i) : SV_Target {
    float2 size = max(glass_size, 1.0);
    float2 half_size = size * 0.5;
    float2 p = i.uv * size - half_size;

    float radius = min(glass_radii.x, min(half_size.x, half_size.y));
    float signed_distance = sdRoundedRect(p, half_size, radius);
    float smooth_radius =
        min(max(radius * 1.5, 30.0), min(half_size.x, half_size.y));
    float2 shape_gradient = gradSdRoundedRect(p, half_size, smooth_radius);

    float2 distance_gradient = float2(ddx(signed_distance), ddy(signed_distance));
    float coverage = min(size.x, size.y) <= 128.0
                         ? pixelCoverage(signed_distance, distance_gradient)
                         : antialiasedCoverage(signed_distance);
    if (coverage <= 0.0)
        discard;

    float shade_distance = min(signed_distance, -0.001);
    float refraction_height = max(glass_refraction.x, 0.001);
    float3 color;
    if (-shade_distance >= refraction_height) {
        color = SampleGlass(i.uv);
    } else {
        float depth = 1.0 - (-shade_distance / refraction_height);
        float displacement = circleMap(depth) * glass_refraction.y;
        float2 depth_gradient = glassSafeNormalize(p, shape_gradient);
        float2 gradient = glassSafeNormalize(
            shape_gradient + glass_refraction.z * depth_gradient,
            shape_gradient);
        float2 refracted = i.uv + displacement * gradient / size;
        float dispersion = glass_refraction.w *
                           ((p.x * p.y) / max(half_size.x * half_size.y, 0.001));
        float2 dispersed = displacement * gradient * dispersion / size;

        // 七个采样点沿色散方向铺开，合成轻微的彩虹边。
        color = 0.0;
        float3 c1 = SampleGlass(refracted + dispersed);
        color.r += c1.r / 3.5;
        float3 c2 = SampleGlass(refracted + dispersed * (2.0 / 3.0));
        color.r += c2.r / 3.5;
        color.g += c2.g / 7.0;
        float3 c3 = SampleGlass(refracted + dispersed * (1.0 / 3.0));
        color.r += c3.r / 3.5;
        color.g += c3.g / 3.5;
        float3 c4 = SampleGlass(refracted);
        color.g += c4.g / 3.5;
        float3 c5 = SampleGlass(refracted - dispersed * (1.0 / 3.0));
        color.g += c5.g / 3.5;
        color.b += c5.b / 3.0;
        float3 c6 = SampleGlass(refracted - dispersed * (2.0 / 3.0));
        color.b += c6.b / 3.0;
        float3 c7 = SampleGlass(refracted - dispersed);
        color.r += c7.r / 7.0;
        color.b += c7.b / 3.0;
    }

    color = applyGlassLighting(color, signed_distance, shape_gradient,
                               refraction_height, distance_gradient,
                               glass_lighting, glass_light);
    return float4(color, glass_filter.w * coverage);
}
)";

using D3DCompileFn = HRESULT(WINAPI*)(LPCVOID source, SIZE_T source_size,
                                      LPCSTR source_name,
                                      const D3D_SHADER_MACRO* defines,
                                      ID3DInclude* include, LPCSTR entry_point,
                                      LPCSTR target, UINT flags1, UINT flags2,
                                      ID3DBlob** code, ID3DBlob** errors);

// 动态加载 d3dcompiler，避免在不带该 DLL 的系统上连进程都起不来。
D3DCompileFn LoadD3DCompile() {
    static D3DCompileFn compile = [] {
        HMODULE module = LoadLibraryW(L"d3dcompiler_47.dll");
        if (!module) module = LoadLibraryW(L"d3dcompiler.dll");
        if (!module) return static_cast<D3DCompileFn>(nullptr);
        return reinterpret_cast<D3DCompileFn>(
            reinterpret_cast<void*>(GetProcAddress(module, "D3DCompile")));
    }();
    return compile;
}

struct BlurParams {
    float uv_min[2];
    float uv_size[2];
    float texel[2];
    float dir[2];
    float radius;
    float pad[3];
};
static_assert(sizeof(BlurParams) == 48);

struct GlassParams {
    float radii[4];
    float refraction[4];
    float filter[4];
    float lighting[4];
    float light[4];
    float raw_uv[4];
    float size[2];
    float mix[2];
};
static_assert(sizeof(GlassParams) == 112);

bool CompileShader(const char* entry, const char* target,
                   ID3DBlob** blob) {
    const D3DCompileFn compile = LoadD3DCompile();
    if (!compile) {
        nl::Log("DesktopBlur: d3dcompiler unavailable");
        return false;
    }
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT hr = compile(kShaderSource, sizeof(kShaderSource) - 1,
                               "desktop_blur.hlsl", nullptr, nullptr, entry,
                               target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob,
                               errors.GetAddressOf());
    if (FAILED(hr)) {
        if (errors)
            nl::Log("DesktopBlur: %s compile failed: %s", entry,
                    static_cast<const char*>(errors->GetBufferPointer()));
        else
            nl::Log("DesktopBlur: %s compile failed (hr=0x%08X)", entry,
                    static_cast<unsigned int>(hr));
        return false;
    }
    return true;
}

}  // namespace

DesktopBlur::~DesktopBlur() {
    Release();
}

bool DesktopBlur::Init(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!device || !context)
        return false;
    Release();
    device_ = device;
    context_ = context;
    // 着色器只在启动时编译一次；失败则整体降级为纯色外观。
    EnsureShaders();
    return true;
}

void DesktopBlur::Release() {
    ReleaseDuplication();
    DestroyBlurTargets();
    blend_.Reset();
    depth_.Reset();
    rasterizer_.Reset();
    sampler_.Reset();
    glass_params_.Reset();
    params_.Reset();
    glass_ps_.Reset();
    blur_ps_.Reset();
    downsample_ps_.Reset();
    vs_.Reset();
    shaders_ready_ = false;
    shader_attempted_ = false;
    duplication_retry_ms_ = 0;
    frame_dirty_ = false;
    last_x_ = 0;
    last_y_ = 0;
    last_width_ = 0;
    last_height_ = 0;
    device_ = nullptr;
    context_ = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capture_requested_ = false;
        request_x_ = 0;
        request_y_ = 0;
        request_width_ = 0;
        request_height_ = 0;
    }
}

bool DesktopBlur::EnsureShaders() {
    if (shaders_ready_)
        return true;
    if (shader_attempted_)
        return false;
    shader_attempted_ = true;

    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> downsample_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> blur_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> glass_blob;
    if (!CompileShader("VSMain", "vs_5_0", vs_blob.GetAddressOf()) ||
        !CompileShader("PSDownsample", "ps_5_0",
                       downsample_blob.GetAddressOf()) ||
        !CompileShader("PSBlur", "ps_5_0", blur_blob.GetAddressOf()) ||
        !CompileShader("PSGlass", "ps_5_0", glass_blob.GetAddressOf()))
        return false;

    if (FAILED(device_->CreateVertexShader(vs_blob->GetBufferPointer(),
                                           vs_blob->GetBufferSize(), nullptr,
                                           vs_.GetAddressOf())) ||
        FAILED(device_->CreatePixelShader(downsample_blob->GetBufferPointer(),
                                          downsample_blob->GetBufferSize(),
                                          nullptr,
                                          downsample_ps_.GetAddressOf())) ||
        FAILED(device_->CreatePixelShader(blur_blob->GetBufferPointer(),
                                          blur_blob->GetBufferSize(), nullptr,
                                          blur_ps_.GetAddressOf())) ||
        FAILED(device_->CreatePixelShader(glass_blob->GetBufferPointer(),
                                          glass_blob->GetBufferSize(), nullptr,
                                          glass_ps_.GetAddressOf()))) {
        nl::Log("DesktopBlur: create shaders failed");
        return false;
    }

    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.ByteWidth = sizeof(BlurParams);
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&buffer_desc, nullptr,
                                     params_.GetAddressOf())))
        return false;
    buffer_desc.ByteWidth = sizeof(GlassParams);
    if (FAILED(device_->CreateBuffer(&buffer_desc, nullptr,
                                     glass_params_.GetAddressOf())))
        return false;

    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MaxLOD = FLT_MAX;
    if (FAILED(device_->CreateSamplerState(&sampler_desc,
                                           sampler_.GetAddressOf())))
        return false;

    D3D11_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rasterizer_desc,
                                              rasterizer_.GetAddressOf())))
        return false;

    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&blend_desc, blend_.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_DESC depth_desc = {};
    if (FAILED(device_->CreateDepthStencilState(&depth_desc,
                                                depth_.GetAddressOf())))
        return false;

    shaders_ready_ = true;
    return true;
}

void DesktopBlur::RequestCapture(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    request_x_ = x;
    request_y_ = y;
    request_width_ = width;
    request_height_ = height;
    capture_requested_ = true;
}

void DesktopBlur::SetLiquidGlass(bool enabled) {
    if (liquid_glass_ == enabled)
        return;
    liquid_glass_ = enabled;
    // 模式变了要重新合成，否则会继续复用上一种外观的结果纹理。
    frame_dirty_ = true;
}

void DesktopBlur::ReleaseDuplication() {
    duplication_.Reset();
    output_rect_ = {};
    frame_srv_.Reset();
    frame_texture_.Reset();
    frame_width_ = 0;
    frame_height_ = 0;
    frame_dirty_ = false;
}

bool DesktopBlur::EnsureDuplication(int center_x, int center_y) {
    if (duplication_) {
        if (center_x >= output_rect_.left && center_x < output_rect_.right &&
            center_y >= output_rect_.top && center_y < output_rect_.bottom)
            return true;
        // 窗口移到别的显示器，重新建立复制会话。
        ReleaseDuplication();
    }

    const unsigned long long now = GetTickCount64();
    if (now < duplication_retry_ms_)
        return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(device_->QueryInterface(
            IID_PPV_ARGS(dxgi_device.GetAddressOf()))) ||
        !dxgi_device) {
        duplication_retry_ms_ = now + kDuplicationRetryMs;
        return false;
    }
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(adapter.GetAddressOf())) || !adapter) {
        duplication_retry_ms_ = now + kDuplicationRetryMs;
        return false;
    }

    // 选中心点所在的输出；都没有则退化成最近的输出，后续再用包含性检查放弃。
    Microsoft::WRL::ComPtr<IDXGIOutput> target;
    DXGI_OUTPUT_DESC target_desc = {};
    for (UINT index = 0;; ++index) {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        if (adapter->EnumOutputs(index, output.GetAddressOf()) ==
            DXGI_ERROR_NOT_FOUND)
            break;
        if (!output)
            break;
        DXGI_OUTPUT_DESC desc = {};
        if (FAILED(output->GetDesc(&desc)) || !desc.AttachedToDesktop)
            continue;
        const RECT& rect = desc.DesktopCoordinates;
        const bool contains =
            center_x >= rect.left && center_x < rect.right &&
            center_y >= rect.top && center_y < rect.bottom;
        if (contains) {
            target = output;
            target_desc = desc;
            break;
        }
        if (!target) {
            target = output;
            target_desc = desc;
        }
    }
    if (!target) {
        duplication_retry_ms_ = now + kDuplicationRetryMs;
        return false;
    }
    if (target_desc.Rotation != DXGI_MODE_ROTATION_IDENTITY &&
        target_desc.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED) {
        // 旋转显示下桌面坐标系与复制画面不一致，直接降级为纯色。
        duplication_retry_ms_ = now + kDuplicationRetryMs;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    if (FAILED(target.As(&output1))) {
        duplication_retry_ms_ = now + kDuplicationRetryMs;
        return false;
    }
    const HRESULT hr =
        output1->DuplicateOutput(device_, duplication_.GetAddressOf());
    if (FAILED(hr) || !duplication_) {
        nl::Log("DesktopBlur: DuplicateOutput failed (hr=0x%08X)",
                static_cast<unsigned int>(hr));
        duplication_.Reset();
        duplication_retry_ms_ = now + kDuplicationRetryMs;
        return false;
    }
    output_rect_ = target_desc.DesktopCoordinates;
    frame_dirty_ = false;
    return true;
}

bool DesktopBlur::EnsureFrameTexture(const D3D11_TEXTURE2D_DESC& source) {
    if (frame_texture_ && frame_width_ == source.Width &&
        frame_height_ == source.Height && frame_format_ == source.Format)
        return true;

    // 只处理 8 位 UNORM 桌面帧；HDR/浮点帧的色调映射不在本模块范围内，
    // 直接放弃并让界面退回纯色，避免出现颜色明显错误。
    if (source.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
        source.Format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        if (frame_format_ != source.Format) {
            nl::Log("DesktopBlur: unsupported desktop format %d",
                    static_cast<int>(source.Format));
            frame_format_ = source.Format;
        }
        frame_srv_.Reset();
        frame_texture_.Reset();
        frame_width_ = 0;
        frame_height_ = 0;
        return false;
    }

    frame_srv_.Reset();
    frame_texture_.Reset();
    frame_width_ = 0;
    frame_height_ = 0;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = source.Width;
    desc.Height = source.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = source.Format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&desc, nullptr,
                                        frame_texture_.GetAddressOf())))
        return false;
    if (FAILED(device_->CreateShaderResourceView(
            frame_texture_.Get(), nullptr, frame_srv_.GetAddressOf()))) {
        frame_texture_.Reset();
        return false;
    }
    frame_width_ = desc.Width;
    frame_height_ = desc.Height;
    frame_format_ = desc.Format;
    return true;
}

bool DesktopBlur::CaptureFrame() {
    if (!duplication_)
        return false;

    DXGI_OUTDUPL_FRAME_INFO frame_info = {};
    Microsoft::WRL::ComPtr<IDXGIResource> resource;
    const HRESULT hr = duplication_->AcquireNextFrame(
        0, &frame_info, resource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return false;  // 桌面没有变化，沿用上一帧。
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
        ReleaseDuplication();
        return false;
    }
    if (FAILED(hr) || !resource) {
        ReleaseDuplication();
        return false;
    }

    bool copied = false;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (SUCCEEDED(resource.As(&texture)) && texture) {
        D3D11_TEXTURE2D_DESC desc = {};
        texture->GetDesc(&desc);
        if (EnsureFrameTexture(desc)) {
            // 整块复制：窗口位置变化时可以直接从同一帧里重新取样，不必等新帧。
            context_->CopyResource(frame_texture_.Get(), texture.Get());
            copied = true;
        }
    }
    resource.Reset();
    duplication_->ReleaseFrame();
    return copied;
}

void DesktopBlur::DestroyBlurTargets() {
    result_rtv_.Reset();
    result_srv_.Reset();
    result_texture_.Reset();
    for (int i = 0; i < 2; ++i) {
        pass_rtv_[i].Reset();
        pass_srv_[i].Reset();
        pass_texture_[i].Reset();
    }
    blur_width_ = 0;
    blur_height_ = 0;
    glass_width_ = 0;
    glass_height_ = 0;
}

bool DesktopBlur::EnsureBlurTargets(int width, int height) {
    const int blur_width = std::max(1, width / kDownscale);
    const int blur_height = std::max(1, height / kDownscale);
    if (blur_width == blur_width_ && blur_height == blur_height_ &&
        width == glass_width_ && height == glass_height_ && result_rtv_)
        return true;

    DestroyBlurTargets();

    auto create_target = [&](const D3D11_TEXTURE2D_DESC& target_desc,
                             Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                             Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
                             Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv) {
        if (FAILED(device_->CreateTexture2D(&target_desc, nullptr,
                                            texture.GetAddressOf())))
            return false;
        if (FAILED(device_->CreateShaderResourceView(
                texture.Get(), nullptr, srv.GetAddressOf())))
            return false;
        return SUCCEEDED(device_->CreateRenderTargetView(
            texture.Get(), nullptr, rtv.GetAddressOf()));
    };

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(blur_width);
    desc.Height = static_cast<UINT>(blur_height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    for (int i = 0; i < 2; ++i) {
        if (!create_target(desc, pass_texture_[i], pass_srv_[i], pass_rtv_[i])) {
            DestroyBlurTargets();
            return false;
        }
    }

    // 液体玻璃按窗口原始分辨率合成，折射细节才够锐利。
    D3D11_TEXTURE2D_DESC glass_desc = desc;
    glass_desc.Width = static_cast<UINT>(std::max(1, width));
    glass_desc.Height = static_cast<UINT>(std::max(1, height));
    if (!create_target(glass_desc, result_texture_, result_srv_, result_rtv_)) {
        DestroyBlurTargets();
        return false;
    }

    blur_width_ = blur_width;
    blur_height_ = blur_height;
    glass_width_ = width;
    glass_height_ = height;
    return true;
}

bool DesktopBlur::UploadParams(float uv_min_x, float uv_min_y, float uv_size_x,
                               float uv_size_y, float texel_x, float texel_y,
                               float dir_x, float dir_y, float radius) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context_->Map(params_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                             &mapped)))
        return false;
    auto* params = static_cast<BlurParams*>(mapped.pData);
    params->uv_min[0] = uv_min_x;
    params->uv_min[1] = uv_min_y;
    params->uv_size[0] = uv_size_x;
    params->uv_size[1] = uv_size_y;
    params->texel[0] = texel_x;
    params->texel[1] = texel_y;
    params->dir[0] = dir_x;
    params->dir[1] = dir_y;
    params->radius = radius;
    params->pad[0] = 0.0f;
    params->pad[1] = 0.0f;
    params->pad[2] = 0.0f;
    context_->Unmap(params_.Get(), 0);
    return true;
}

bool DesktopBlur::DrawPass(ID3D11ShaderResourceView* source,
                           ID3D11ShaderResourceView* second_source,
                           ID3D11RenderTargetView* target,
                           ID3D11PixelShader* shader,
                           ID3D11Buffer* params) {
    if (!source || !target || !shader || !params)
        return false;
    ID3D11ShaderResourceView* null_srvs[2] = {nullptr, nullptr};
    context_->PSSetShaderResources(0, 2, null_srvs);
    // 必须先切换渲染目标再绑 SRV：否则上一次的 RTV 仍绑在同一个纹理上，
    // D3D11 会静默丢弃与之冲突的 SRV 绑定，采样结果会变成全 0（纯黑）。
    context_->OMSetRenderTargets(1, &target, nullptr);
    context_->PSSetShader(shader, nullptr, 0);
    context_->PSSetConstantBuffers(0, 1, &params);
    context_->PSSetShaderResources(0, 1, &source);
    if (second_source)
        context_->PSSetShaderResources(1, 1, &second_source);
    context_->Draw(3, 0);
    return true;
}

bool DesktopBlur::RenderBlur(int x, int y, int width, int height) {
    if (!frame_srv_ || !pass_rtv_[0] || !pass_rtv_[1] || !result_rtv_)
        return false;

    const float inv_frame_w = 1.0f / static_cast<float>(frame_width_);
    const float inv_frame_h = 1.0f / static_cast<float>(frame_height_);
    const int local_x = x - output_rect_.left;
    const int local_y = y - output_rect_.top;
    const float blur_texel_x = 1.0f / static_cast<float>(blur_width_);
    const float blur_texel_y = 1.0f / static_cast<float>(blur_height_);

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> old_rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> old_dsv;
    context_->OMGetRenderTargets(1, old_rtv.GetAddressOf(),
                                 old_dsv.GetAddressOf());
    UINT old_viewport_count = 1;
    D3D11_VIEWPORT old_viewport = {};
    context_->RSGetViewports(&old_viewport_count, &old_viewport);

    ID3D11ShaderResourceView* null_srv = nullptr;
    context_->PSSetShaderResources(0, 1, &null_srv);
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    ID3D11SamplerState* sampler = sampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
    context_->RSSetState(rasterizer_.Get());
    context_->OMSetBlendState(blend_.Get(), nullptr, 0xFFFFFFFF);
    context_->OMSetDepthStencilState(depth_.Get(), 0);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(blur_width_);
    viewport.Height = static_cast<float>(blur_height_);
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);

    bool succeeded = false;
    do {
        // 1) 从整块桌面帧里取出窗口区域并降采样。
        if (!UploadParams(local_x * inv_frame_w, local_y * inv_frame_h,
                          width * inv_frame_w, height * inv_frame_h, inv_frame_w,
                          inv_frame_h, 0.0f, 0.0f, 2.0f))
            break;
        if (!DrawPass(frame_srv_.Get(), nullptr, pass_rtv_[0].Get(),
                      downsample_ps_.Get(), params_.Get()))
            break;

        // 2) 横向高斯。
        if (!UploadParams(0.0f, 0.0f, 1.0f, 1.0f, blur_texel_x, blur_texel_y,
                          1.0f, 0.0f, 1.0f))
            break;
        if (!DrawPass(pass_srv_[0].Get(), nullptr, pass_rtv_[1].Get(),
                      blur_ps_.Get(), params_.Get()))
            break;

        // 3) 纵向高斯，得到窗口区域的模糊底板。
        if (!UploadParams(0.0f, 0.0f, 1.0f, 1.0f, blur_texel_x, blur_texel_y,
                          0.0f, 1.0f, 1.0f))
            break;
        if (!DrawPass(pass_srv_[1].Get(), nullptr, pass_rtv_[0].Get(),
                      blur_ps_.Get(), params_.Get()))
            break;

        // 4) 合成到窗口分辨率：液体玻璃做边缘折射与高光，模糊模式只放大底板。
        if (liquid_glass_) {
            if (!RenderGlass(x, y, width, height))
                break;
        } else {
            D3D11_VIEWPORT upscale_viewport = {};
            upscale_viewport.Width = static_cast<float>(std::max(1, width));
            upscale_viewport.Height = static_cast<float>(std::max(1, height));
            upscale_viewport.MaxDepth = 1.0f;
            context_->RSSetViewports(1, &upscale_viewport);
            // dir 与 radius 都为 0 时 9 个采样点重合，权重和为 1，等价于线性放大。
            if (!UploadParams(0.0f, 0.0f, 1.0f, 1.0f, blur_texel_x, blur_texel_y,
                              0.0f, 0.0f, 0.0f))
                break;
            if (!DrawPass(pass_srv_[0].Get(), nullptr, result_rtv_.Get(),
                          blur_ps_.Get(), params_.Get()))
                break;
        }
        succeeded = true;
    } while (false);

    ID3D11ShaderResourceView* null_srvs[2] = {nullptr, nullptr};
    context_->PSSetShaderResources(0, 2, null_srvs);
    context_->PSSetShader(nullptr, nullptr, 0);
    context_->VSSetShader(nullptr, nullptr, 0);
    if (old_rtv)
        context_->OMSetRenderTargets(1, old_rtv.GetAddressOf(), old_dsv.Get());
    else
        context_->OMSetRenderTargets(0, nullptr, nullptr);
    if (old_viewport_count != 0)
        context_->RSSetViewports(old_viewport_count, &old_viewport);
    return succeeded;
}

// 液体玻璃合成。调用前 shared 管线状态（VS/采样器/光栅化/混合）已经由
// RenderBlur 绑定，这里只切换视口和常量。
bool DesktopBlur::RenderGlass(int x, int y, int width, int height) {
    if (!pass_srv_[0] || !frame_srv_ || !result_rtv_ || !glass_ps_ ||
        !glass_params_)
        return false;

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(std::max(1, width));
    viewport.Height = static_cast<float>(std::max(1, height));
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);

    // 参数取自该库的默认样式，按 1x 像素密度换算。
    GlassParams params = {};
    const float radius = 17.0f;
    params.radii[0] = radius;
    params.radii[1] = radius;
    params.radii[2] = radius;
    params.radii[3] = radius;
    params.refraction[0] = 28.0f;   // 边缘折射带宽度
    params.refraction[1] = -40.0f;  // 折射位移（负值向内收）
    params.refraction[2] = 0.3f;    // 深度系数
    params.refraction[3] = 0.5f;    // 色散强度
    params.filter[0] = 0.0f;        // 对比度
    params.filter[1] = 0.0f;        // 白点
    params.filter[2] = 1.0f;        // 饱和度
    params.filter[3] = 1.0f;        // 不透明度
    params.lighting[0] = 0.12f;     // 内阴影
    params.lighting[1] = 0.20f;     // 边缘辉光
    params.lighting[2] = 0.35f;     // 高光
    params.light[0] = -0.7071f;
    params.light[1] = -0.7071f;
    params.light[2] = 5.0f;
    params.light[3] = 28.0f;        // 高光锐度
    params.size[0] = static_cast<float>(std::max(1, width));
    params.size[1] = static_cast<float>(std::max(1, height));

    // 同一时刻的原始桌面帧里，窗口矩形对应的 uv 区域。
    const float inv_frame_w = 1.0f / static_cast<float>(frame_width_);
    const float inv_frame_h = 1.0f / static_cast<float>(frame_height_);
    params.raw_uv[0] = static_cast<float>(x - output_rect_.left) * inv_frame_w;
    params.raw_uv[1] = static_cast<float>(y - output_rect_.top) * inv_frame_h;
    params.raw_uv[2] = static_cast<float>(width) * inv_frame_w;
    params.raw_uv[3] = static_cast<float>(height) * inv_frame_h;
    params.mix[0] = 0.7f;  // 模糊图占七成，原图保留边缘折射细节
    params.mix[1] = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context_->Map(glass_params_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                             &mapped)))
        return false;
    *static_cast<GlassParams*>(mapped.pData) = params;
    context_->Unmap(glass_params_.Get(), 0);

    return DrawPass(pass_srv_[0].Get(), frame_srv_.Get(), result_rtv_.Get(),
                    glass_ps_.Get(), glass_params_.Get());
}

ID3D11ShaderResourceView* DesktopBlur::Update() {
    if (!device_ || !context_)
        return nullptr;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!capture_requested_)
            return result_srv_.Get();
        x = request_x_;
        y = request_y_;
        width = request_width_;
        height = request_height_;
    }
    if (width <= 0 || height <= 0)
        return result_srv_.Get();
    if (!EnsureShaders())
        return nullptr;

    // 窗口必须完整落在所选输出内，否则 uv 映射会错位（跨屏/移出屏幕时退回纯色）。
    if (!EnsureDuplication(x + width / 2, y + height / 2))
        return result_srv_.Get();
    if (x < output_rect_.left || y < output_rect_.top ||
        x + width > output_rect_.right || y + height > output_rect_.bottom)
        return nullptr;

    if (CaptureFrame())
        frame_dirty_ = true;
    if (!frame_srv_)
        return nullptr;

    // 桌面画面和窗口矩形都没变时直接复用上一帧的模糊结果。
    if (!frame_dirty_ && result_srv_ && x == last_x_ && y == last_y_ &&
        width == last_width_ && height == last_height_)
        return result_srv_.Get();

    if (!EnsureBlurTargets(width, height) || !RenderBlur(x, y, width, height))
        return result_srv_.Get();

    frame_dirty_ = false;
    last_x_ = x;
    last_y_ = y;
    last_width_ = width;
    last_height_ = height;
    return result_srv_.Get();
}

}  // namespace nl::gfx
