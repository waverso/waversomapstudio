// Source Repository: https://github.com/waverso/waversomapstudio

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "image_atlas.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <d3d9.h>
#include <limits>
#include <memory>
#include <new>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace ImageAtlas
{
    AtlasEntry ReadEntry(const BYTE* entries, UINT index) noexcept
    {
        AtlasEntry entry;
        std::memcpy(&entry, entries + static_cast<size_t>(index) * sizeof(entry), sizeof(entry));
        return entry;
    }

    bool ReadAtlas(const BYTE* data, size_t size, AtlasHeader& header, const BYTE*& entries, const BYTE*& image) noexcept
    {
        if (!data || size < sizeof(header))
        {
            return false;
        }
        std::memcpy(&header, data, sizeof(header));
        if (std::memcmp(header.magic, StudioFormat::AtlasMagic.data(), StudioFormat::AtlasMagic.size()) != 0
            || header.version != StudioFormat::AtlasVersion || header.width == 0 || header.width > 16384 || header.height == 0
            || header.height > 16384 || header.count == 0 || header.count > (size - sizeof(header)) / sizeof(AtlasEntry))
        {
            return false;
        }

        const size_t imageOffset = sizeof(header) + static_cast<size_t>(header.count) * sizeof(AtlasEntry);
        if (header.imageSize == 0 || header.imageSize != size - imageOffset)
        {
            return false;
        }
        entries = data + sizeof(header);
        image = data + imageOffset;
        for (UINT i = 0; i < header.count; ++i)
        {
            const AtlasEntry entry = ReadEntry(entries, i);
            if (!entry.name[0] || !std::memchr(entry.name, '\0', sizeof(entry.name)) || entry.width == 0 || entry.height == 0
                || entry.width > header.width || entry.height > header.height || entry.x > header.width - entry.width
                || entry.y > header.height - entry.height)
            {
                return false;
            }
            for (UINT j = 0; j < i; ++j)
            {
                if (std::strcmp(entry.name, ReadEntry(entries, j).name) == 0)
                {
                    return false;
                }
            }
        }
        return true;
    }

    HRESULT GetPixelBufferLayout(UINT width, UINT height, UINT& stride, UINT& size) noexcept
    {
        if (width == 0 || height == 0 || width > std::numeric_limits<UINT>::max() / 4u)
        {
            return E_INVALIDARG;
        }

        stride = width * 4u;
        if (height > std::numeric_limits<UINT>::max() / stride)
        {
            return E_INVALIDARG;
        }

        size = stride * height;
        return S_OK;
    }

    HRESULT DecodePixels(
        IWICBitmapSource* source, UINT width, UINT height, std::unique_ptr<BYTE[]>& pixels, UINT& stride) noexcept
    {
        UINT size = 0;
        HRESULT result = GetPixelBufferLayout(width, height, stride, size);
        if (FAILED(result))
        {
            return result;
        }

        pixels.reset(new (std::nothrow) BYTE[size]);
        if (!pixels)
        {
            return E_OUTOFMEMORY;
        }

        result = source->CopyPixels(nullptr, stride, size, pixels.get());
        if (FAILED(result))
        {
            pixels.reset();
        }

        return result;
    }

    HRESULT UploadTextureLevel(
        IDirect3DTexture9* texture, UINT level, const BYTE* pixels, UINT width, UINT height, UINT stride) noexcept
    {
        D3DSURFACE_DESC description {};
        HRESULT result = texture->GetLevelDesc(level, &description);
        if (FAILED(result))
        {
            return result;
        }

        if (description.Width != width || description.Height != height || description.Format != D3DFMT_A8R8G8B8)
        {
            return E_FAIL;
        }

        D3DLOCKED_RECT locked {};
        result = texture->LockRect(level, &locked, nullptr, 0);
        if (FAILED(result))
        {
            return result;
        }

        if (!locked.pBits || locked.Pitch < 0 || static_cast<UINT>(locked.Pitch) < stride)
        {
            texture->UnlockRect(level);
            return E_FAIL;
        }

        BYTE* destination = static_cast<BYTE*>(locked.pBits);
        for (UINT y = 0; y < height; ++y)
        {
            std::memcpy(destination + static_cast<size_t>(y) * locked.Pitch, pixels + static_cast<size_t>(y) * stride, stride);
        }

        return texture->UnlockRect(level);
    }

    void GenerateMipLevel(const BYTE* source, UINT sourceWidth, UINT sourceHeight, UINT sourceStride, BYTE* destination,
        UINT destinationWidth, UINT destinationHeight, UINT destinationStride) noexcept
    {
        for (UINT y = 0; y < destinationHeight; ++y)
        {
            const UINT sourceYBegin = static_cast<UINT>(static_cast<std::uint64_t>(y) * sourceHeight / destinationHeight);
            UINT sourceYEnd = static_cast<UINT>(static_cast<std::uint64_t>(y + 1u) * sourceHeight / destinationHeight);
            if (sourceYEnd <= sourceYBegin)
            {
                sourceYEnd = sourceYBegin + 1u;
            }

            BYTE* destinationRow = destination + static_cast<size_t>(y) * destinationStride;
            for (UINT x = 0; x < destinationWidth; ++x)
            {
                const UINT sourceXBegin = static_cast<UINT>(static_cast<std::uint64_t>(x) * sourceWidth / destinationWidth);
                UINT sourceXEnd = static_cast<UINT>(static_cast<std::uint64_t>(x + 1u) * sourceWidth / destinationWidth);
                if (sourceXEnd <= sourceXBegin)
                {
                    sourceXEnd = sourceXBegin + 1u;
                }

                UINT channels[4] {};
                UINT sampleCount = 0;
                for (UINT sourceY = sourceYBegin; sourceY < sourceYEnd; ++sourceY)
                {
                    const BYTE* sourceRow = source + static_cast<size_t>(sourceY) * sourceStride;
                    for (UINT sourceX = sourceXBegin; sourceX < sourceXEnd; ++sourceX)
                    {
                        const BYTE* sourcePixel = sourceRow + static_cast<size_t>(sourceX) * 4u;
                        channels[0] += sourcePixel[0];
                        channels[1] += sourcePixel[1];
                        channels[2] += sourcePixel[2];
                        channels[3] += sourcePixel[3];
                        ++sampleCount;
                    }
                }

                BYTE* destinationPixel = destinationRow + static_cast<size_t>(x) * 4u;
                destinationPixel[0] = static_cast<BYTE>((channels[0] + sampleCount / 2u) / sampleCount);
                destinationPixel[1] = static_cast<BYTE>((channels[1] + sampleCount / 2u) / sampleCount);
                destinationPixel[2] = static_cast<BYTE>((channels[2] + sampleCount / 2u) / sampleCount);
                destinationPixel[3] = static_cast<BYTE>((channels[3] + sampleCount / 2u) / sampleCount);
            }
        }
    }

    HRESULT DecodeAtlas(IWICImagingFactory* factory, const BYTE* data, const AtlasHeader& header, IWICBitmap** output) noexcept
    {
        *output = nullptr;
        ComPtr<IWICStream> stream;
        HRESULT result = factory->CreateStream(stream.GetAddressOf());
        if (FAILED(result))
        {
            return result;
        }

        result = stream->InitializeFromMemory(const_cast<BYTE*>(data), header.imageSize);
        if (FAILED(result))
        {
            return result;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        result = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
        if (FAILED(result))
        {
            return result;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        result = decoder->GetFrame(0, frame.GetAddressOf());
        if (FAILED(result))
        {
            return result;
        }

        UINT width = 0;
        UINT height = 0;
        result = frame->GetSize(&width, &height);
        if (FAILED(result) || width != header.width || height != header.height)
        {
            return FAILED(result) ? result : E_FAIL;
        }

        ComPtr<IWICFormatConverter> converter;
        result = factory->CreateFormatConverter(converter.GetAddressOf());
        if (FAILED(result))
        {
            return result;
        }
        result = converter->Initialize(
            frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(result))
        {
            return result;
        }
        return factory->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, output);
    }

    HRESULT CreateTexture(IWICImagingFactory* factory, IDirect3DDevice9* device, IWICBitmapSource* source, UINT width,
        UINT height, UINT levels, IDirect3DTexture9** output) noexcept
    {
        *output = nullptr;
        UINT sourceWidth = 0;
        UINT sourceHeight = 0;
        HRESULT result = source->GetSize(&sourceWidth, &sourceHeight);
        if (FAILED(result))
        {
            return result;
        }

        ComPtr<IWICBitmapScaler> scaler;
        if (sourceWidth != width || sourceHeight != height)
        {
            result = factory->CreateBitmapScaler(scaler.GetAddressOf());
            if (FAILED(result))
            {
                return result;
            }

            result = scaler->Initialize(source, width, height, WICBitmapInterpolationModeFant);
            if (FAILED(result))
            {
                return result;
            }

            source = scaler.Get();
        }

        std::unique_ptr<BYTE[]> pixels;
        UINT stride = 0;
        result = DecodePixels(source, width, height, pixels, stride);
        if (FAILED(result))
        {
            return result;
        }

        ComPtr<IDirect3DTexture9> texture;
        result
            = device->CreateTexture(width, height, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture.GetAddressOf(), nullptr);
        if (FAILED(result))
        {
            return result;
        }

        UINT currentWidth = width;
        UINT currentHeight = height;
        UINT currentStride = stride;
        const UINT levelCount = texture->GetLevelCount();
        for (UINT level = 0; level < levelCount; ++level)
        {
            result = UploadTextureLevel(texture.Get(), level, pixels.get(), currentWidth, currentHeight, currentStride);
            if (FAILED(result))
            {
                return result;
            }

            if (level + 1u == levelCount)
            {
                break;
            }

            D3DSURFACE_DESC nextDescription {};
            result = texture->GetLevelDesc(level + 1u, &nextDescription);
            if (FAILED(result))
            {
                return result;
            }

            UINT nextStride = 0;
            UINT nextSize = 0;
            result = GetPixelBufferLayout(nextDescription.Width, nextDescription.Height, nextStride, nextSize);
            if (FAILED(result))
            {
                return result;
            }

            std::unique_ptr<BYTE[]> nextPixels(new (std::nothrow) BYTE[nextSize]);
            if (!nextPixels)
            {
                return E_OUTOFMEMORY;
            }

            GenerateMipLevel(pixels.get(), currentWidth, currentHeight, currentStride, nextPixels.get(), nextDescription.Width,
                nextDescription.Height, nextStride);

            pixels.swap(nextPixels);
            currentWidth = nextDescription.Width;
            currentHeight = nextDescription.Height;
            currentStride = nextStride;
        }

        *output = texture.Detach();
        return S_OK;
    }

    Image::Image(UINT resourceId) noexcept : m_comResult(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&ReadAtlas), &module))
        {
            return;
        }
        HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(resourceId), RT_RCDATA);
        HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
        const BYTE* data = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
        const BYTE* image = nullptr;
        if (!ReadAtlas(data, resource ? SizeofResource(module, resource) : 0, header, entries, image))
        {
            header = {};
            entries = nullptr;
            return;
        }
        if (FAILED(m_comResult) && m_comResult != RPC_E_CHANGED_MODE)
        {
            return;
        }
        if (SUCCEEDED(
                CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()))))
        {
            DecodeAtlas(factory.Get(), image, header, bitmap.GetAddressOf());
        }
    }

    Image::~Image()
    {
        bitmap.Reset();
        factory.Reset();
        if (SUCCEEDED(m_comResult))
        {
            CoUninitialize();
        }
    }
}
