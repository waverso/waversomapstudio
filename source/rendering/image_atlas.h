// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <d3d9.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "../shared/file_formats.h"

namespace ImageAtlas
{
    using Microsoft::WRL::ComPtr;
    using StudioFormat::AtlasEntry;
    using StudioFormat::AtlasHeader;

    AtlasEntry ReadEntry(const BYTE* entries, UINT index) noexcept;
    bool ReadAtlas(const BYTE* data, size_t size, AtlasHeader& header, const BYTE*& entries, const BYTE*& image) noexcept;
    HRESULT DecodeAtlas(IWICImagingFactory* factory, const BYTE* data, const AtlasHeader& header, IWICBitmap** output) noexcept;
    HRESULT CreateTexture(IWICImagingFactory* factory, IDirect3DDevice9* device, IWICBitmapSource* source, UINT width,
        UINT height, UINT levels, IDirect3DTexture9** output) noexcept;

    class Image final
    {
        public:
        explicit Image(UINT resourceId) noexcept;
        ~Image();
        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;

        AtlasHeader header {};
        const BYTE* entries = nullptr;
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICBitmap> bitmap;

        private:
        HRESULT m_comResult;
    };
}
