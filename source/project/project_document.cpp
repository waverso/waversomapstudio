// Source Repository: https://github.com/waverso/waversomapstudio

#include "project_document.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "../shared/crc32.h"

namespace ProjectDocument
{
    namespace
    {
#include "project_document_binary.inl"
#include "project_document_item_reader.inl"
#include "project_document_item_writer.inl"
    }

    std::vector<std::uint8_t> Encode(const Items& items, const std::optional<Camera>& camera)
    {
        std::string error;
        if (!Validate(items, error))
        {
            throw std::runtime_error(error);
        }
        Writer payload;
        payload.U32(static_cast<std::uint32_t>(items.size()));
        for (const auto& item : items)
        {
            WriteItem(payload, item);
            if (payload.bytes.size() > MaxFileBytes - HeaderBytes)
            {
                throw std::runtime_error("Project file exceeds the 64 MiB size limit.");
            }
        }
        for (const auto& item : items)
        {
            payload.Bool(item.open);
        }
        payload.Bool(camera.has_value());
        if (camera)
        {
            if (!std::all_of(std::begin(camera->position), std::end(camera->position),
                    [](float value)
                    {
                        return std::isfinite(value);
                    })
                || !std::isfinite(camera->yaw) || !std::isfinite(camera->pitch))
            {
                throw std::runtime_error("Project camera contains an invalid numeric value.");
            }
            payload.Vector(camera->position);
            payload.Float(camera->yaw);
            payload.Float(camera->pitch);
        }
        Writer file;
        if (payload.bytes.size() > MaxFileBytes - HeaderBytes)
        {
            throw std::runtime_error("Project file exceeds the 64 MiB size limit.");
        }
        file.bytes.insert(file.bytes.end(), Magic.begin(), Magic.end());
        file.U32(Version);
        file.U32(static_cast<std::uint32_t>(payload.bytes.size()));
        file.U32(Utils::Crc32(payload.bytes));
        file.bytes.insert(file.bytes.end(), payload.bytes.begin(), payload.bytes.end());
        return std::move(file.bytes);
    }

    bool Decode(std::span<const std::uint8_t> bytes, Items& items, std::string& error, std::optional<Camera>* camera)
    {
        try
        {
            if (bytes.size() > MaxFileBytes)
            {
                char message[64];
                std::snprintf(message, sizeof(message), "Project file exceeds the %d MiB size limit.",
                    static_cast<int>(MaxFileBytes / (1024 * 1024)));
                throw std::runtime_error(message);
            }
            if (bytes.size() < Magic.size() || std::memcmp(bytes.data(), Magic.data(), Magic.size()) != 0)
            {
                throw std::runtime_error("Invalid project file signature.");
            }
            Reader r { bytes, Magic.size() };
            const auto version = r.U32();
            if (version != Version)
            {
                throw std::runtime_error("Unsupported project version.");
            }
            const auto payloadSize = r.U32();
            const auto checksum = r.U32();
            if (payloadSize != bytes.size() - r.offset || Utils::Crc32(bytes.subspan(r.offset)) != checksum)
            {
                throw std::runtime_error("Project file is incomplete or corrupted.");
            }
            const auto count = r.U32();
            if (count > MaxItems)
            {
                throw std::runtime_error("Project exceeds the item limit.");
            }
            Items decoded(count);
            for (auto& item : decoded)
            {
                ReadItem(r, item);
            }
            for (auto& item : decoded)
            {
                item.open = r.Bool();
            }
            std::optional<Camera> decodedCamera;
            if (r.Bool())
            {
                decodedCamera.emplace();
                r.Vector(decodedCamera->position);
                decodedCamera->yaw = r.Float();
                decodedCamera->pitch = r.Float();
            }
            if (r.offset != bytes.size())
            {
                throw std::runtime_error("Project file contains unexpected trailing data.");
            }
            if (!Validate(decoded, error))
            {
                return false;
            }
            items = std::move(decoded);
            if (camera)
            {
                *camera = decodedCamera;
            }
            error.clear();
            return true;
        }
        catch (const std::exception& e)
        {
            error = e.what();
            return false;
        }
    }
}
