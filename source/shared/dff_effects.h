// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CStreaming.h"
#include "model_effects.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace ModelEffects::Dff
{
    constexpr std::uint32_t ChunkExtension = 0x03;
    constexpr std::uint32_t ChunkGeometry = 0x0F;
    constexpr std::uint32_t ChunkClump = 0x10;
    constexpr std::uint32_t ChunkGeometryList = 0x1A;
    constexpr std::uint32_t Chunk2DEffect = 0x0253F2F8;

    constexpr std::size_t SectorSize = 2048;
    constexpr std::size_t ChunkHeaderSize = 12;
    constexpr std::uint32_t LightSizeRequired = 76;
    constexpr std::uint32_t LightSizeWithDir = 80;
    constexpr std::uint32_t ParticleNameSize = 24;
    constexpr unsigned MaxEffectsPerChunk = 2048;
    constexpr unsigned MaxChunkDepth = 32;

    struct ParseStats
    {
        unsigned effectChunks = 0;
        unsigned entriesSeen = 0;
        bool malformed = false;
    };

    struct ArchiveRead
    {
        bool ok = false;
        std::uint8_t imgId = 0;
        char imgName[48] {};
    };

    inline std::uint32_t ReadU32(const std::uint8_t* p)
    {
        std::uint32_t value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    inline float ReadF32(const std::uint8_t* p)
    {
        float value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    inline float Sanitized(float value)
    {
        return std::isfinite(value) ? value : 0.0f;
    }

    inline void ParseEntry(const std::uint8_t* payload, std::uint32_t type, const float pos[3], unsigned entryIndex, Summary& out)
    {
        if (type == 0)
        {
            LightInfo light;
            light.pos[0] = Sanitized(pos[0]);
            light.pos[1] = Sanitized(pos[1]);
            light.pos[2] = Sanitized(pos[2]);
            light.color[0] = payload[0] / 255.0f;
            light.color[1] = payload[1] / 255.0f;
            light.color[2] = payload[2] / 255.0f;
            light.alpha = payload[3] / 255.0f;
            light.farClip = std::max(0.0f, Sanitized(ReadF32(payload + 4)));
            light.pointRange = std::max(0.0f, Sanitized(ReadF32(payload + 8)));
            light.coronaSize = std::max(0.0f, Sanitized(ReadF32(payload + 12)));
            light.shadowSize = std::max(0.0f, Sanitized(ReadF32(payload + 16)));
            light.flashType = payload[20];
            light.flareType = payload[22];
            const unsigned short flags = (static_cast<unsigned short>(payload[74]) << 8) | payload[24];
            light.flags = flags;
            std::memcpy(light.textureName, payload + 25, sizeof(light.textureName) - 1);
            light.textureName[sizeof(light.textureName) - 1] = '\0';
            light.effectIndex = entryIndex;
            out.lights.push_back(light);
        }
        else if (type == 1)
        {
            ParticleInfo particle;
            particle.pos[0] = Sanitized(pos[0]);
            particle.pos[1] = Sanitized(pos[1]);
            particle.pos[2] = Sanitized(pos[2]);
            std::memcpy(particle.name, payload, sizeof(particle.name) - 1);
            particle.name[sizeof(particle.name) - 1] = '\0';
            out.particles.push_back(particle);
        }
    }

    inline void Parse2dEffectChunk(const std::uint8_t* body, std::size_t length, Summary& out, ParseStats& stats)
    {
        if (length < 4)
        {
            stats.malformed = true;
            return;
        }
        const std::uint32_t count = ReadU32(body);
        if (count > MaxEffectsPerChunk)
        {
            stats.malformed = true;
            return;
        }
        std::size_t offset = 4;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            if (offset + 20 > length)
            {
                stats.malformed = true;
                return;
            }
            float pos[3];
            std::memcpy(pos, body + offset, sizeof(pos));
            offset += 12;
            const std::uint32_t type = ReadU32(body + offset);
            offset += 4;
            const std::uint32_t sectionSize = ReadU32(body + offset);
            offset += 4;
            if (static_cast<std::size_t>(sectionSize) > length - offset)
            {
                stats.malformed = true;
                return;
            }
            const std::uint8_t* payload = body + offset;
            const bool lightSizeOk = sectionSize == LightSizeRequired || sectionSize == LightSizeWithDir;
            const bool accepted = (type == 0 && lightSizeOk) || (type == 1 && sectionSize == ParticleNameSize);
            if (accepted)
            {
                const unsigned entryIndex = out.entriesSeen;
                ++out.entriesSeen;
                ParseEntry(payload, type, pos, entryIndex, out);
            }
            else if (type >= 2 && type <= 10)
            {
                ++out.entriesSeen;
            }
            offset += sectionSize;
        }
    }

    inline void WalkExtensionChildren(const std::uint8_t* body, std::size_t length, Summary& out, ParseStats& stats)
    {
        std::size_t offset = 0;
        while (offset + ChunkHeaderSize <= length)
        {
            const std::uint32_t id = ReadU32(body + offset);
            const std::uint32_t size = ReadU32(body + offset + 4);
            if (static_cast<std::size_t>(size) > length - offset - ChunkHeaderSize)
            {
                return;
            }
            if (id == Chunk2DEffect)
            {
                ++stats.effectChunks;
                Parse2dEffectChunk(body + offset + ChunkHeaderSize, size, out, stats);
            }
            offset += ChunkHeaderSize + size;
        }
    }

    inline void WalkChunks(
        const std::uint8_t* data, std::size_t length, std::uint32_t parent, Summary& out, ParseStats& stats, unsigned depth = 0)
    {
        std::size_t offset = 0;
        while (offset + ChunkHeaderSize <= length)
        {
            const std::uint32_t id = ReadU32(data + offset);
            const std::uint32_t size = ReadU32(data + offset + 4);
            if (static_cast<std::size_t>(size) > length - offset - ChunkHeaderSize)
            {
                return;
            }
            const std::uint8_t* body = data + offset + ChunkHeaderSize;
            if (id == ChunkClump || id == ChunkGeometryList || id == ChunkGeometry)
            {
                if (depth >= MaxChunkDepth)
                {
                    stats.malformed = true;
                    return;
                }
                WalkChunks(body, size, id, out, stats, depth + 1);
            }
            else if (id == ChunkExtension && parent == ChunkGeometry)
            {
                WalkExtensionChildren(body, size, out, stats);
            }
            offset += ChunkHeaderSize + size;
        }
    }

    inline unsigned ParseDff2dfx(const std::uint8_t* data, std::size_t size, Summary& out, ParseStats& stats)
    {
        if (!data || size < ChunkHeaderSize)
        {
            return 0;
        }
        WalkChunks(data, size, 0, out, stats);
        return stats.effectChunks;
    }

    inline const char* GameRootDir()
    {
        static char path[MAX_PATH] {};
        static bool resolved = false;
        if (!resolved)
        {
            resolved = true;
            const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
            if (length > 0 && length < MAX_PATH)
            {
                char* lastSlash = std::strrchr(path, '\\');
                if (lastSlash)
                {
                    lastSlash[1] = '\0';
                }
                else
                {
                    path[0] = '\0';
                }
            }
        }
        return path;
    }

    inline bool ReadModelDffFromArchive(int modelId, std::vector<std::uint8_t>& out, ArchiveRead& info)
    {
        info = {};
        out.clear();
        if (modelId < 0 || modelId >= 26316)
        {
            return false;
        }
        if (!CStreaming::ms_aInfoForModel || !CStreaming::ms_files)
        {
            return false;
        }
        const CStreamingInfo& streaming = CStreaming::ms_aInfoForModel[modelId];
        if (streaming.m_nCdSize == 0)
        {
            return false;
        }
        const std::uint8_t imgId = streaming.m_nImgId;
        if (imgId >= 8)
        {
            return false;
        }
        const char* archiveName = CStreaming::ms_files[imgId].m_szName;
        if (!archiveName || !archiveName[0])
        {
            return false;
        }

        std::string path = archiveName;
        const bool absolute = !path.empty() && (path.rfind(':', 2) != std::string::npos || path[0] == '\\' || path[0] == '/');
        if (!absolute)
        {
            std::string root = GameRootDir();
            if (root.empty())
            {
                return false;
            }
            path = root + path;
        }

#ifdef _WIN32
        HANDLE file
            = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const std::size_t bytes = static_cast<std::size_t>(streaming.m_nCdSize) * SectorSize;
        LARGE_INTEGER position {};
        position.QuadPart = static_cast<LONGLONG>(streaming.m_nCdPosn) * static_cast<LONGLONG>(SectorSize);
        bool ok = SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != 0;
        out.resize(bytes);
        std::size_t done = 0;
        while (ok && done < bytes)
        {
            DWORD chunk = 0;
            const DWORD request = static_cast<DWORD>(std::min<std::size_t>(bytes - done, 1u << 20));
            if (!ReadFile(file, out.data() + done, request, &chunk, nullptr) || chunk == 0)
            {
                ok = false;
                break;
            }
            done += chunk;
        }
        CloseHandle(file);
        if (!ok)
        {
            out.clear();
            return false;
        }
        info.ok = true;
        info.imgId = imgId;
        std::snprintf(info.imgName, sizeof(info.imgName), "%s", archiveName);
        return true;
#else
        return false;
#endif
    }

    inline bool CollectFromArchiveDff(int modelId, Summary& out, ParseStats& stats, ArchiveRead* readInfo = nullptr)
    {
        std::vector<std::uint8_t> dff;
        ArchiveRead read;
        if (!ReadModelDffFromArchive(modelId, dff, read))
        {
            return false;
        }
        ParseDff2dfx(dff.data(), dff.size(), out, stats);
        if (readInfo)
        {
            *readInfo = read;
        }
        return true;
    }
}
