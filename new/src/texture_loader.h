#pragma once
#include "common.h"
#include <d3d11.h>
#include <cstddef>

struct ID3D11ShaderResourceView;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace nl {

// Resolve an asset path relative to the executable directory (assets\<name>).
// Falls back to the current working directory so the exe is portable.
std::wstring ResolveAssetPath(const wchar_t* name);

// Loads an image as an RGBA8 D3D11 texture and returns its shader resource view.
// Returns nullptr on failure (size is written when non-null).
ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device,
                                               ID3D11DeviceContext* ctx,
                                               const wchar_t* path,
                                               int* out_w = nullptr,
                                               int* out_h = nullptr);

// Loads an image, applies the original hard-edged circular mask, and generates mips.
ID3D11ShaderResourceView* LoadCircularTextureFromFile(ID3D11Device* device,
                                                       ID3D11DeviceContext* ctx,
                                                       const wchar_t* path,
                                                       int* out_w = nullptr,
                                                       int* out_h = nullptr);

namespace gfx {

struct TextureDims {
    int width = 0;
    int height = 0;
};

bool LoadTextureFromMemory(const void* data,
                           std::size_t size,
                           ID3D11Device* device,
                           ID3D11ShaderResourceView** out_srv,
                           TextureDims* dims = nullptr);

bool LoadCircularTextureFromMemory(const void* data,
                                   std::size_t size,
                                   ID3D11Device* device,
                                   ID3D11DeviceContext* ctx,
                                   ID3D11ShaderResourceView** out_srv,
                                   TextureDims* dims = nullptr);

}  // namespace gfx
}  // namespace nl
