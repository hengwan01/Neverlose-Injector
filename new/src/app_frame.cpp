#include "app_frame.h"
#include "app.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "embed.h"
#include "i18n.h"
#include "paths.h"
#include "texture_loader.h"
#include "ui_draw.h"
#include "ui_shell.h"

#include <commdlg.h>
#include <d3d11.h>
#include <filesystem>
#include <objbase.h>
#include <ocidl.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <limits>
#include <utility>
#include <vector>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam);

namespace nl {
namespace {

ID3D11ShaderResourceView* ToSrv(ImTextureID texture) {
    return reinterpret_cast<ID3D11ShaderResourceView*>(
        static_cast<std::uintptr_t>(texture));
}

ImTextureID ToTexture(ID3D11ShaderResourceView* srv) {
    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(srv));
}

struct ComApartment {
    explicit ComApartment(DWORD model = COINIT_APARTMENTTHREADED)
        : hr(CoInitializeEx(nullptr, model)) {}
    HRESULT hr;
    ~ComApartment() {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};

bool SaveRenderTarget(AppDevice& device, const std::wstring& path) {
    using Microsoft::WRL::ComPtr;

    ComApartment apartment(COINIT_MULTITHREADED);
    ComPtr<ID3D11Texture2D> back_buffer;
    if (FAILED(device.SwapChain()->GetBuffer(
            0, IID_PPV_ARGS(back_buffer.GetAddressOf()))))
        return false;

    D3D11_TEXTURE2D_DESC description{};
    back_buffer->GetDesc(&description);
    if (description.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        description.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
        return false;

    D3D11_TEXTURE2D_DESC staging_description = description;
    staging_description.Usage = D3D11_USAGE_STAGING;
    staging_description.BindFlags = 0;
    staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_description.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device.Device()->CreateTexture2D(
            &staging_description, nullptr, staging.GetAddressOf())))
        return false;
    device.Context()->CopyResource(staging.Get(), back_buffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(device.Context()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return false;

    bool saved = false;
    do {
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(factory.GetAddressOf()))) ||
            FAILED(factory->CreateStream(stream.GetAddressOf())) ||
            FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) ||
            FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr,
                                          encoder.GetAddressOf())) ||
            FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
            FAILED(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr)) ||
            FAILED(frame->Initialize(nullptr)) ||
            FAILED(frame->SetSize(description.Width, description.Height)))
            break;

        WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
        if (FAILED(frame->SetPixelFormat(&pixel_format)) ||
            !IsEqualGUID(pixel_format, GUID_WICPixelFormat32bppBGRA))
            break;

        const UINT stride = description.Width * 4;
        std::vector<unsigned char> pixels(
            static_cast<std::size_t>(stride) * description.Height);
        const auto* source = static_cast<const unsigned char*>(mapped.pData);
        for (UINT y = 0; y < description.Height; ++y) {
            unsigned char* destination = pixels.data() +
                static_cast<std::size_t>(y) * stride;
            std::memcpy(destination,
                        source + static_cast<std::size_t>(y) * mapped.RowPitch,
                        stride);
            if (description.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
                for (UINT x = 0; x < description.Width; ++x)
                    std::swap(destination[x * 4], destination[x * 4 + 2]);
            }
        }

        if (SUCCEEDED(frame->WritePixels(description.Height, stride,
                                         static_cast<UINT>(pixels.size()),
                                         pixels.data())) &&
            SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit()))
            saved = true;
    } while (false);

    device.Context()->Unmap(staging.Get(), 0);
    return saved;
}

bool SelectAvatarFile(HWND hwnd, std::wstring& path, std::string& error) {
    std::vector<wchar_t> file(0x8000, L'\0');
    static const wchar_t filter[] =
        L"Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = 0x98;
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = 0x8000;
    dialog.Flags = 0x1808;
    if (GetOpenFileNameW(&dialog)) {
        path = file.data();
        return true;
    }
    if (CommDlgExtendedError() != 0)
        error = "Could not open the file chooser.";
    return false;
}

bool EncodeAvatarAttempt(IWICImagingFactory* factory, IWICBitmapSource* source,
                         int dimension, float quality_value,
                         std::vector<unsigned char>& output) {
    using Microsoft::WRL::ComPtr;

    ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(scaler.GetAddressOf())) ||
        FAILED(scaler->Initialize(source, static_cast<UINT>(dimension),
                                  static_cast<UINT>(dimension),
                                  static_cast<WICBitmapInterpolationMode>(3))))
        return false;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
        FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat24bppBGR,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
        return false;

    ComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.GetAddressOf())))
        return false;

    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr,
                                      encoder.GetAddressOf())) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
        return false;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    if (FAILED(encoder->CreateNewFrame(frame.GetAddressOf(),
                                       properties.GetAddressOf())))
        return false;

    PROPBAG2 option{};
    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
    VARIANT quality{};
    quality.vt = VT_R4;
    quality.fltVal = quality_value;
    if (FAILED(properties->Write(1, &option, &quality)) ||
        FAILED(frame->Initialize(properties.Get())) ||
        FAILED(frame->SetSize(static_cast<UINT>(dimension),
                              static_cast<UINT>(dimension))) ||
        FAILED(frame->WriteSource(converter.Get(), nullptr)) ||
        FAILED(frame->Commit()) || FAILED(encoder->Commit()))
        return false;

    STATSTG stat{};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) ||
        stat.cbSize.QuadPart < 0 ||
        static_cast<unsigned long long>(stat.cbSize.QuadPart) >
            std::numeric_limits<std::size_t>::max())
        return false;

    HGLOBAL global = nullptr;
    if (FAILED(GetHGlobalFromStream(stream.Get(), &global)))
        return false;
    const std::size_t size = static_cast<std::size_t>(stat.cbSize.QuadPart);
    if (size > GlobalSize(global))
        return false;

    void* locked = GlobalLock(global);
    if (!locked && size != 0)
        return false;
    output.resize(size);
    if (size != 0)
        std::memcpy(output.data(), locked, size);
    if (locked)
        GlobalUnlock(global);
    return true;
}

bool NormalizeAvatar(const std::wstring& path, std::vector<unsigned char>& output,
                     std::string& error) {
    using Microsoft::WRL::ComPtr;
    ComApartment apartment;
    if (FAILED(apartment.hr)) {
        error = "Could not process this image. Choose a PNG or JPEG file.";
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(
                                    factory.GetAddressOf()))) ||
        FAILED(factory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf())) ||
        FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) {
        error = "Could not process this image. Choose a PNG or JPEG file.";
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    if (FAILED(frame->GetSize(&width, &height))) {
        error = "Could not process this image. Choose a PNG or JPEG file.";
        return false;
    }
    const unsigned long long pixels =
        static_cast<unsigned long long>(width) * height;
    if (width == 0 || height == 0 || width > 16384 || height > 16384 ||
        pixels > 64000000ULL) {
        error = "Image is too large. Use an image smaller than 64 megapixels.";
        return false;
    }

    const int side = static_cast<int>(std::min(width, height));
    const WICRect crop = {
        static_cast<INT>((static_cast<int>(width) - side) / 2),
        static_cast<INT>((static_cast<int>(height) - side) / 2),
        side,
        side,
    };
    ComPtr<IWICBitmapClipper> clipper;
    if (FAILED(factory->CreateBitmapClipper(clipper.GetAddressOf())) ||
        FAILED(clipper->Initialize(frame.Get(), &crop))) {
        error = "Could not process this image. Choose a PNG or JPEG file.";
        return false;
    }

    const int dimensions[] = {
        std::min(side, 256), std::min(side, 224), std::min(side, 160),
    };
    const float qualities[] = {0.94f, 0.91f, 0.88f};
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::vector<unsigned char> candidate;
        if (!EncodeAvatarAttempt(factory.Get(), clipper.Get(), dimensions[attempt],
                                 qualities[attempt], candidate)) {
            error = "Could not encode avatar.";
            return false;
        }
        if (candidate.size() > 0x8000)
            continue;
        if (candidate.size() < 4 || candidate[0] != 0xff ||
            candidate[1] != 0xd8 || candidate[candidate.size() - 2] != 0xff ||
            candidate.back() != 0xd9) {
            error = "Could not encode avatar.";
            return false;
        }
        output = std::move(candidate);
        return true;
    }

    error = "Image is too complex. Choose another avatar.";
    return false;
}

std::wstring QuoteWindowsArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : value) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'\"')
            quoted.append(backslashes * 2 + 1, L'\\');
        else
            quoted.append(backslashes, L'\\');
        quoted.push_back(character);
        backslashes = 0;
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

}  // namespace

std::wstring AdminRestartArguments(const std::filesystem::path& user_data,
                                   bool preview) {
    std::wstring arguments = L"--data-dir " + QuoteWindowsArgument(user_data.native());
    if (preview)
        arguments += L" --preview";
    return arguments;
}

unsigned long RestartLoaderAsAdministrator(HWND hwnd,
                                           const std::filesystem::path& user_data,
                                           bool preview) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return 5;
    TOKEN_ELEVATION elevation{};
    DWORD returned = 0;
    const BOOL queried = GetTokenInformation(token, TokenElevation, &elevation,
                                              sizeof(elevation), &returned);
    CloseHandle(token);
    if (!queried || elevation.TokenIsElevated)
        return 5;

    std::vector<wchar_t> module(0x8000, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module.data(), 0x8000);
    if (length > 0x7fff)
        return 122;
    if (length == 0)
        return GetLastError();

    std::error_code filesystem_error;
    const std::filesystem::path module_path = std::filesystem::absolute(
        std::filesystem::path(std::wstring(module.data(), length)), filesystem_error);
    if (filesystem_error)
        return 123;

    const std::wstring arguments = AdminRestartArguments(user_data, preview);
    const std::filesystem::path working_directory = module_path.parent_path();
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = 0x70;
    execute.fMask = 0x540;
    execute.hwnd = hwnd;
    execute.lpVerb = L"runas";
    execute.lpFile = module_path.c_str();
    execute.lpParameters = arguments.c_str();
    execute.lpDirectory = working_directory.c_str();
    execute.nShow = 1;
    if (!ShellExecuteExW(&execute))
        return GetLastError();
    if (execute.hProcess)
        CloseHandle(execute.hProcess);
    return 0;
}

std::string DescribeAdminRestartFailure(unsigned long code) {
    if (code == 1223) {
        return "Administrator request cancelled.\nYou can retry or go Back. The loader is still open.";
    }
    return "Could not restart the loader (Windows error " + std::to_string(code) +
           ").\nTry Run as administrator from File Explorer.";
}

AppFrame::AppFrame() : runtime_(gPreviewMode) {
    settings_actions_.choose_avatar =
        [this](Profile& draft, ImTextureID& avatar, std::string& error) {
            return ChooseAvatar(draft, avatar, error);
        };
    settings_actions_.save_profile =
        [this](const Profile& draft, std::string& error) {
            return SaveSettingsProfile(draft, error);
        };
    settings_actions_.profile_saved =
        [this](const Profile& profile, bool restart) {
            ProfileSaved(profile, restart);
        };
}

AppFrame::~AppFrame() {
    SafeRelease(settings_avatar_preview_);
}

void AppFrame::ConfigureBootAnim(int boot_size, int width, int height) {
    (void)boot_size;
    anim_.configured = true;
    anim_.width = width;
    anim_.height = height;
    anim_.target_width = width;
    anim_.target_height = height;
    anim_.size_dirty = true;
}

void AppFrame::InitClock() {
    QueryPerformanceFrequency(&qpc_frequency_);
    QueryPerformanceCounter(&qpc_last_);
}

bool AppFrame::InitImGui(HWND hwnd, AppDevice& device) {
    hwnd_ = hwnd;
    device_ = &device;
    ctx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    RefreshConfigTheme();
    InitFonts();
    std::string profile_error;
    profile_ = LoadProfile(profile_error);
    profile_.language = "en";
    i18n::SetLanguage(profile_.language);
    state_.nickname = profile_.nickname;

    if (!ImGui_ImplWin32_Init(hwnd)) {
        Log("ImGui_ImplWin32_Init failed");
        return false;
    }
    win32_initialized_ = true;
    if (!ImGui_ImplDX11_Init(device.Device(), device.Context())) {
        Log("ImGui_ImplDX11_Init failed");
        return false;
    }
    dx11_initialized_ = true;
    initialized_ = true;
    return true;
}

void AppFrame::InitFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const auto& bytes = embed::Font();
    if (!bytes.empty()) {
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
        void* data = const_cast<unsigned char*>(bytes.data());
        theme_.font_body = io.Fonts->AddFontFromMemoryTTF(data, bytes.size(), 20.0f,
                                                           &config, ranges);
        theme_.font_small = io.Fonts->AddFontFromMemoryTTF(data, bytes.size(), 13.0f,
                                                            &config, ranges);
        theme_.font_title = io.Fonts->AddFontFromMemoryTTF(data, bytes.size(), 40.0f,
                                                            &config, ranges);
        theme_.font_semibold = io.Fonts->AddFontFromMemoryTTF(data, bytes.size(), 22.0f,
                                                               &config, ranges);
        theme_.font_heading = io.Fonts->AddFontFromMemoryTTF(data, bytes.size(), 30.0f,
                                                              &config, ranges);
    }
    if (!theme_.font_body)
        theme_.font_body = io.Fonts->AddFontDefault();
    if (!theme_.font_small)
        theme_.font_small = theme_.font_body;
    if (!theme_.font_title)
        theme_.font_title = theme_.font_body;
    if (!theme_.font_semibold)
        theme_.font_semibold = theme_.font_body;
    if (!theme_.font_heading)
        theme_.font_heading = theme_.font_body;
    io.FontDefault = theme_.font_body;
}

void AppFrame::RefreshConfigTheme() {
    const ImVec4 blue = ui::shell::Blue();
    theme_.accent = blue;
    theme_.warning = blue;
    theme_.accent_alt = blue;
    theme_.accent_hover = blue;
    theme_.panel = ui::shell::Panel();
    theme_.card = ui::shell::Card();
    theme_.border = ui::shell::Border();
    theme_.text = ImVec4(0.96f, 0.96f, 0.98f, 1.0f);
    theme_.text_soft = theme_.text;
    theme_.text_strong = theme_.text;
    theme_.text_bright = theme_.text;
    theme_.text_dim = ImVec4(0.70f, 0.71f, 0.77f, 1.0f);
    theme_.text_secondary = theme_.text_dim;
    theme_.muted = ui::shell::Muted();
    theme_.muted_alt = theme_.muted;
    theme_.border_alt = theme_.card;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ChildRounding = theme_.child_rounding;
    style.FrameRounding = theme_.frame_rounding;
    style.PopupRounding = theme_.popup_rounding;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = theme_.text;
    colors[ImGuiCol_TextDisabled] = theme_.muted;
    colors[ImGuiCol_WindowBg] = theme_.panel;
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = theme_.card;
    colors[ImGuiCol_Border] = theme_.border;
    colors[ImGuiCol_FrameBg] = theme_.card;
    colors[ImGuiCol_FrameBgHovered] = theme_.border;
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = theme_.accent;
    colors[ImGuiCol_ScrollbarGrabHovered] = theme_.accent_hover;
    colors[ImGuiCol_ScrollbarGrabActive] = theme_.accent_hover;
    colors[ImGuiCol_CheckMark] = theme_.accent;
    colors[ImGuiCol_SliderGrab] = theme_.accent;
    colors[ImGuiCol_SliderGrabActive] = theme_.accent_hover;
    colors[ImGuiCol_Button] = theme_.accent_hover;
    colors[ImGuiCol_ButtonHovered] = ImVec4(
        theme_.accent_hover.x * 1.1f, theme_.accent_hover.y * 1.1f,
        theme_.accent_hover.z * 1.1f, 1.0f);
    colors[ImGuiCol_ButtonActive] = theme_.accent;
    colors[ImGuiCol_Header] = theme_.card;
    colors[ImGuiCol_HeaderHovered] = theme_.border;
    colors[ImGuiCol_HeaderActive] = theme_.accent;
    colors[ImGuiCol_Separator] = theme_.card;
    colors[ImGuiCol_SeparatorHovered] = theme_.border;
    colors[ImGuiCol_SeparatorActive] = theme_.accent;
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);
}

bool AppFrame::InitBlurAndTextures(AppDevice& device) {
    if (!blur_.Init(device.Device(), device.Context(), device.SwapChain())) {
        Log("Blur::Init failed");
        return false;
    }
    gpu_.reserved_038 = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(&blur_));
    ui::LoadTextures(gpu_, device.Device(), device.Context());
    ReloadAvatar();
    return true;
}

void AppFrame::ReloadAvatar() {
    ID3D11ShaderResourceView* current = ToSrv(gpu_.avatar);
    if (!current || profile_.avatar.empty())
        return;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    current->GetDevice(&device);
    if (device)
        device->GetImmediateContext(&context);

    ID3D11ShaderResourceView* replacement = nullptr;
    if (device && context &&
        gfx::LoadCircularTextureFromMemory(profile_.avatar.data(), profile_.avatar.size(),
                                           device, context, &replacement)) {
        gpu_.avatar = ToTexture(replacement);
        SafeRelease(current);
    } else {
        SafeRelease(replacement);
    }
    SafeRelease(context);
    SafeRelease(device);
}

void AppFrame::LoadModels() {
    store_.Load();
}

void AppFrame::InitializeLocalRelease() {
    long long last_launch = 0;
    if (const data::CheatInfo* cs2 = store_.Find("cs2")) {
        for (const data::BranchInfo& branch : cs2->branches) {
            if (branch.id == "release") {
                last_launch = branch.last_launch;
                break;
            }
        }
    }

    data::CheatStore replacement;
    std::string error;
    if (!data::ParseStoreJson(embed::CheatsJson(), replacement, &error))
        return;
    store_ = std::move(replacement);
    if (data::CheatInfo* cs2 = store_.Find("cs2")) {
        if (!cs2->branches.empty())
            cs2->branches.front().last_launch = last_launch;
    }
}

bool AppFrame::ForwardRawMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!initialized_)
        return false;
    return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) != 0;
}

bool AppFrame::IsDragDisabled() const {
    return state_.modal.changelog_drawn;
}

bool AppFrame::WantCaptureMouse() const {
    if (!initialized_ || !ctx_)
        return false;
    ImGui::SetCurrentContext(ctx_);
    if (ImGui::IsAnyItemHovered())
        return false;
    return ImGui::IsAnyItemActive();
}

bool AppFrame::CanDragAt(POINT point) const {
    if (anim_.stage != 2)
        return false;
    const bool left = static_cast<unsigned>(point.x - 8) < 0xc1 &&
                      static_cast<unsigned>(point.y - 8) < 0x44;
    const bool right = point.x >= 525 && point.x < anim_.target_width - 125 &&
                       static_cast<unsigned>(point.y - 8) < 56;
    return left || right;
}

void AppFrame::OnBeginDrag() {
    if (window_)
        window_->BeginDrag();
}

void AppFrame::OnDragMove() {
    if (window_)
        window_->DragTo();
}

void AppFrame::OnEndDrag() {
    if (window_)
        window_->EndDrag();
}

void AppFrame::OnRequestClose() {
    if (window_)
        RequestUserClose(*window_);
}

void AppFrame::OnDpiChanged(unsigned dpi, int width, int height) {
    (void)dpi;
    if (width <= 0 || height <= 0)
        return;
    if (device_ && device_->Valid() && !device_->Resize(width, height))
        Log("AppDevice::Resize failed");
    anim_.target_width = width;
    anim_.target_height = height;
    anim_.size_dirty = true;
    if (device_)
        blur_.OnResize(device_->SwapChain());
    blur_.has_result_ = false;
}

void AppFrame::RenderUi(float dt, AppWindow& window) {
    ui::FrameInput input;
    input.tick_ms = GetTickCount64();
    input.unix_time = static_cast<long long>(_time64(nullptr));
    input.dt = dt;
    input.width = anim_.target_width;
    input.height = anim_.target_height;

    if (anim_.splash_opacity > 0.01f && anim_.stage <= 1)
        ui::DrawSplash(theme_, animation_time_, anim_.splash_opacity);

    if (anim_.stage == 3 ||
        (anim_.stage == 2 && anim_.content_opacity > 0.01f)) {
        if (gPreviewMode && !gCapturePath.empty()) {
            const std::wstring& page = gCapturePage;
            if (page == L"profile-hover")
                state_.profile_hover = 1.0f;
            if (page == L"settings" || page == L"settings-avatar-hover") {
                state_.settings_open = true;
                if (page == L"settings-avatar-hover")
                    settings_.avatar_hover = 1.0f;
            }

            const bool success = page == L"inject-success-settle" ||
                                 page == L"inject-success" ||
                                 page == L"inject-success-enter";
            const bool injection = success || page == L"inject-bright" ||
                                   page == L"inject-dim";
            const bool modal_page = injection || page == L"game" ||
                                    page == L"branch" || page == L"access-error";
            if (modal_page && !state_.modal.open) {
                state_.modal.open = true;
                state_.modal.cheat_id = "cs2";
                state_.modal.launch_started_ms = input.tick_ms - 1000;
                if (const data::CheatInfo* cheat = store_.Find("cs2"))
                    ui::FillDisplayedBranch(state_, *cheat, input.unix_time);
            }
            if (page == L"branch")
                state_.modal.branch_open = true;
            if (page == L"changelog")
                state_.modal.changelog_open = true;
            if (page == L"access-error") {
                state_.modal.launch_overlay_visible = true;
                state_.launch_error = true;
                state_.need_restart_admin = true;
                state_.launch_status =
                    "CS2 is running as administrator.\n"
                    "Restart the loader as administrator, or restart Steam/CS2 normally.";
            }
            if (injection) {
                state_.modal.launch_overlay_visible = true;
                state_.launch_status = "Initializing...";
                state_.launch_cancellable = true;
                state_.launch_overlay_target = 0.45f;
                state_.modal.launch_started_ms = page == L"inject-bright"
                    ? input.tick_ms - 800 : input.tick_ms;
            }
            if (success) {
                state_.launch_ready = true;
                state_.launch_cancellable = false;
                state_.launch_overlay_target = 1.0f;
                state_.launch_status = "Initialization completed";
                if (page == L"inject-success-enter")
                    state_.completion_started_ms = input.tick_ms - 100;
                else if (page == L"inject-success-settle")
                    state_.completion_started_ms = input.tick_ms - 500;
                else
                    state_.completion_started_ms = input.tick_ms - 2200;
            }
        }

        const float opacity = anim_.stage == 3
            ? std::max(anim_.window_opacity, anim_.content_opacity)
            : anim_.content_opacity;
        ui::DashActions dash_actions;
        ui::ModalActions modal_actions;
        ui::DrawDashboard(theme_, state_, gpu_, store_, opacity, input, dash_actions);
        ui::DrawGameModal(theme_, state_, gpu_, store_, input, modal_actions);

        bool close_requested = false;
        ProcessDashActions(dash_actions, window, close_requested);
        ProcessModalActions(modal_actions, input.unix_time, window);
        if (close_requested)
            RequestUserClose(window);
    }

    const bool modal_close = state_.modal.close_requested;
    state_.modal.close_requested = false;
    if (modal_close)
        RequestUserClose(window);

    DrawSettings(input);
    state_.modal.changelog_drawn =
        state_.modal.changelog_drawn || state_.settings_open;
    if (state_.modal.changelog_drawn)
        window.EndDrag();
    if (window.IsDragging()) {
        window.DragTo();
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
            window.EndDrag();
    }
}

void AppFrame::DrawSettings(const ui::FrameInput& input) {
    if (state_.settings_open && !settings_.initialized) {
        SafeRelease(settings_avatar_preview_);
        settings_avatar_preview_ = ToSrv(gpu_.avatar);
        if (settings_avatar_preview_)
            settings_avatar_preview_->AddRef();
    }

    const RuntimeSnapshot snapshot = runtime_.Status();
    ui::SettingsRuntimeStatus runtime_status;
    runtime_status.busy = snapshot.running;
    runtime_status.visible = snapshot.error;
    runtime_status.text = snapshot.text;
    ui::DrawSettings(theme_, state_, settings_, gpu_, profile_, store_, blur_,
                     input, runtime_status, settings_actions_);
}

void AppFrame::ProcessDashActions(const ui::DashActions& actions, AppWindow& window,
                                  bool& close_requested) {
    if (actions.close)
        close_requested = true;
    if (actions.minimize) {
        ShowWindow(window.Hwnd(), SW_MINIMIZE);
        window.EndDrag();
    }
    if (actions.open_url)
        OpenUrl(actions.open_url);
}

void AppFrame::ProcessModalActions(ui::ModalActions& actions, long long now,
                                   AppWindow& window) {
    if (actions.start)
        runtime_.Start(profile_, false);
    if (actions.cancel)
        runtime_.Cancel();

    if (actions.restart_admin && state_.need_restart_admin &&
        state_.launch_error && !state_.launch_cancellable) {
        const unsigned long code = RestartLoaderAsAdministrator(
            window.Hwnd(), std::filesystem::path(paths::NlRootDir()), false);
        if (code != 0)
            deferred_error_text_ = DescribeAdminRestartFailure(code);
        else
            RequestUserClose(window);
    }
    if (actions.open_url)
        OpenUrl(actions.open_url);

    if (actions.record_launch) {
        const bool saved = store_.SaveLastLaunch(
            actions.launch_cheat, actions.launch_branch, actions.launch_time);
        if (const data::CheatInfo* cheat = store_.Find(actions.launch_cheat))
            ui::FillDisplayedBranch(state_, *cheat, now);
        if (!saved)
            Log("Failed to persist last launch for %s", actions.launch_cheat.c_str());
    }

    if (actions.select_branch) {
        if (!store_.SaveSelectedBranch(actions.select_cheat,
                                       actions.select_branch_id))
            Log("Failed to persist selected branch for %s", actions.select_cheat.c_str());
        if (!state_.modal.open) {
            if (const data::CheatInfo* cheat = store_.Find(actions.select_cheat))
                ui::FillDisplayedBranch(state_, *cheat, now);
        }
    }
}

void AppFrame::PollRuntimeStatus(unsigned long long tick_ms, long long now) {
    if (data::CheatInfo* cs2 = store_.Find("cs2")) {
        const int days = RemainingProfileDays(profile_, now);
        const bool lifetime = days == 0;
        const long long license = lifetime ? profile_.expires_at : 0;
        if (cs2->lifetime != lifetime || cs2->license != license) {
            cs2->lifetime = lifetime;
            cs2->license = license;
            ui::FillDisplayedBranch(state_, *cs2, now);
        }
    }

    const RuntimeSnapshot snapshot = runtime_.Status();
    state_.launch_status = snapshot.text;
    state_.launch_cancellable = snapshot.running;
    state_.launch_error = snapshot.error;
    state_.launch_ready = !snapshot.running && !snapshot.error && snapshot.ok;
    state_.need_restart_admin = snapshot.need_restart_admin;
    state_.launch_overlay_target = snapshot.overlay_target;

    if (snapshot.revision != saved_runtime_revision_) {
        deferred_error_text_.clear();
        saved_runtime_revision_ = snapshot.revision;
        if (snapshot.ok && !snapshot.running) {
            completion_tick_ms_ = tick_ms;
            state_.completion_started_ms = tick_ms;
            if (data::CheatInfo* cs2 = store_.Find("cs2")) {
                if (!cs2->branches.empty())
                    cs2->branches.front().last_launch = now;
                ui::FillDisplayedBranch(state_, *cs2, now);
            }
            if (!store_.SaveLastLaunch("cs2", "release", now))
                Log("Failed to persist last launch for cs2");
        }
    }
    if (!deferred_error_text_.empty())
        state_.launch_status = deferred_error_text_;
}

void AppFrame::RequestUserClose(AppWindow& window) {
    if (anim_.stage == 3 || anim_.close_finished)
        return;
    window.EndDrag();
    anim_.stage = 3;
    anim_.close_progress = 0.0f;
    anim_.close_finished = false;
    anim_.previous_window_opacity = anim_.window_opacity;
}

void AppFrame::OpenUrl(const wchar_t* url) {
    if (!url || (wcsncmp(url, L"https://", 8) != 0 &&
                 wcsncmp(url, L"http://", 7) != 0)) {
        Log("OpenUrl failed");
        return;
    }
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", url, nullptr, nullptr,
                                           SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        Log("OpenUrl failed");
}

bool AppFrame::TickFrame(AppWindow& window, AppDevice& device, bool& running) {
    if (qpc_frequency_.QuadPart == 0) {
        Log("TickFrame: QPC frequency unavailable");
        running = false;
        PostQuitMessage(0);
        return false;
    }

    LARGE_INTEGER now_qpc{};
    QueryPerformanceCounter(&now_qpc);
    float dt = static_cast<float>(now_qpc.QuadPart - qpc_last_.QuadPart) /
               static_cast<float>(qpc_frequency_.QuadPart);
    qpc_last_ = now_qpc;
    dt = std::clamp(dt, 0.0001f, 0.05f);
    animation_time_ += dt;
    ui::TickAnim(anim_, dt);

    if (anim_.close_finished) {
        running = false;
        PostQuitMessage(0);
        return false;
    }
    if (anim_.size_dirty) {
        window.ApplyRoundRegion(anim_.target_width, anim_.target_height,
                                anim_.width, anim_.height, 17);
        anim_.size_dirty = false;
    }
    device.SetRoundedClip(anim_.target_width, anim_.target_height,
                          anim_.width, anim_.height, 17);

    if (device.HasComposition())
        device.SetOpacity(anim_.window_opacity);
    else
        window.SetLayeredAlpha(static_cast<unsigned char>(anim_.window_opacity * 255.0f));

    const bool backdrop = window.SetBackdropEnabled(device.HasComposition());
    gpu_.reserved_040 = backdrop ? 1 : 0;
    ui::shell::GlassEnabled = backdrop;

    const unsigned long long tick_ms = GetTickCount64();
    const long long now = static_cast<long long>(_time64(nullptr));
    if (tick_ms >= next_theme_refresh_ms_) {
        RefreshConfigTheme();
        next_theme_refresh_ms_ = tick_ms + 1500;
    }
    PollRuntimeStatus(tick_ms, now);

    if (completion_tick_ms_ != 0 && tick_ms - completion_tick_ms_ >= 2400) {
        const bool minimized = IsIconic(window.Hwnd()) != 0;
        const bool overlay_delay = !minimized && state_.modal.launch_overlay_visible &&
                                   state_.modal.launch_overlay_alpha < 0.995f;
        if (!overlay_delay) {
            completion_tick_ms_ = 0;
            if (minimized) {
                running = false;
                PostQuitMessage(0);
                return false;
            }
            RequestUserClose(window);
        }
    }

    if (IsIconic(window.Hwnd()) && anim_.stage != 3) {
        MsgWaitForMultipleObjectsEx(0, nullptr, 200, 0x4ff, 4);
        return true;
    }
    if (!initialized_ || !device.Valid()) {
        Log("Frame skipped: missing rtv/device/imgui");
        running = false;
        PostQuitMessage(0);
        return false;
    }

    ImGui::SetCurrentContext(ctx_);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderUi(dt, window);
    ImGui::Render();

    float clear[4] = {};
    if (!device.HasComposition()) {
        clear[0] = theme_.panel.x;
        clear[1] = theme_.panel.y;
        clear[2] = theme_.panel.z;
        clear[3] = 1.0f;
    }
    device.ClearAndBind(clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (gPreviewMode && !gCapturePath.empty()) {
        static int readyFrames = 0;
        const bool connecting = gCapturePage == L"connecting";
        const bool ready = connecting
            ? anim_.stage == 0 && anim_.boot_progress > 0.3f
            : anim_.stage == 2 && anim_.content_opacity >= 0.999f;
        if (ready && ++readyFrames >= 30) {
            const bool saved = SaveRenderTarget(device, gCapturePath);
            PostQuitMessage(saved ? 0 : 2);
            running = false;
            return saved;
        }
    }

    const int present = device.Present(1);
    if (present == 1) {
        if (!RecreateGraphics(device, window)) {
            running = false;
            PostQuitMessage(0);
            return false;
        }
        return true;
    }
    if (present == 2)
        Log("Present failed; continuing");
    return true;
}

bool AppFrame::RecreateGraphics(AppDevice& device, AppWindow& window) {
    (void)window;
    ui::ReleaseTextures(gpu_);
    blur_.Shutdown();
    const bool restore_dx11 = dx11_initialized_;
    if (dx11_initialized_) {
        ImGui_ImplDX11_Shutdown();
        dx11_initialized_ = false;
    }

    device.Cleanup();
    if (!device.Init(hwnd_, anim_.target_width, anim_.target_height))
        return false;
    device_ = &device;
    if (restore_dx11) {
        if (!ImGui_ImplDX11_Init(device.Device(), device.Context()))
            return false;
        dx11_initialized_ = true;
    }
    if (!InitBlurAndTextures(device))
        return false;
    gpu_.reserved_040 = 0;
    ui::shell::GlassEnabled = false;
    return true;
}

void AppFrame::TeardownImGui() {
    if (!ctx_)
        return;
    ImGui::SetCurrentContext(ctx_);
    if (dx11_initialized_) {
        ImGui_ImplDX11_Shutdown();
        dx11_initialized_ = false;
    }
    if (win32_initialized_) {
        ImGui_ImplWin32_Shutdown();
        win32_initialized_ = false;
    }
    ImGui::DestroyContext(ctx_);
    ctx_ = nullptr;
    initialized_ = false;
}

void AppFrame::ShutdownGraphics() {
    runtime_.Cancel();
    SafeRelease(settings_avatar_preview_);
    theme_.font_body = nullptr;
    theme_.font_small = nullptr;
    theme_.font_title = nullptr;
    theme_.font_semibold = nullptr;
    theme_.font_heading = nullptr;
    if (ctx_)
        ImGui::GetIO().FontDefault = nullptr;
    ui::ReleaseTextures(gpu_);
    TeardownImGui();
    blur_.Shutdown();
}

bool AppFrame::ChooseAvatar(Profile& draft, ImTextureID& avatar,
                            std::string& error) {
    std::wstring path;
    if (!SelectAvatarFile(hwnd_, path, error))
        return false;

    error.clear();
    std::vector<unsigned char> encoded;
    if (!NormalizeAvatar(path, encoded, error))
        return false;

    ID3D11ShaderResourceView* main_avatar = ToSrv(gpu_.avatar);
    if (!main_avatar)
        return false;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    main_avatar->GetDevice(&device);
    if (device)
        device->GetImmediateContext(&context);

    ID3D11ShaderResourceView* preview = nullptr;
    const bool loaded = device && context &&
        gfx::LoadCircularTextureFromMemory(encoded.data(), encoded.size(), device,
                                           context, &preview);
    SafeRelease(context);
    SafeRelease(device);
    if (!loaded) {
        SafeRelease(preview);
        return false;
    }

    SafeRelease(settings_avatar_preview_);
    settings_avatar_preview_ = preview;
    avatar = ToTexture(preview);
    draft.avatar = std::move(encoded);
    return true;
}

bool AppFrame::SaveSettingsProfile(const Profile& draft, std::string& error) {
    if (!nl::SaveProfile(draft, error))
        return false;
    settings_.blur_captured = false;
    blur_.has_result_ = false;
    return true;
}

void AppFrame::ProfileSaved(const Profile& profile, bool restart) {
    ID3D11ShaderResourceView* preview = settings_avatar_preview_;
    if (preview)
        preview->AddRef();
    ID3D11ShaderResourceView* old_avatar = ToSrv(gpu_.avatar);
    gpu_.avatar = ToTexture(preview);
    SafeRelease(old_avatar);
    if (restart)
        runtime_.Start(profile, true);
}

}  // namespace nl
