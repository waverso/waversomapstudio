// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "actions.h"

class EditorScene;

namespace PropertyEdit
{
    inline constexpr const char* MixedText = "\xE2\x80\x94";

    struct Component
    {
        ComponentType type = ComponentTypeTransform;
        int slot = -1;
        int sequence = 0;
        bool operator==(const Component&) const = default;
    };

    std::vector<Component> CommonComponents(const std::vector<const ItemData*>& targets, bool& partial);
    bool CanRemove(const ItemData& data, Component component);
    bool SameComponent(const ItemData& first, const ItemData& other, Component component);
    bool ParseVector(const char* text, float values[3], unsigned& parts);
    std::uint64_t Structure(const ItemData& data);

    template <class T>
    struct Value
    {
        T value {};
        bool mixed = false;
    };

    template <class Read>
    auto Aggregate(size_t count, Read&& read, bool dominant = false)
    {
        using T = std::decay_t<decltype(read(size_t {}))>;
        Value<T> result;
        if (!count)
        {
            return result;
        }
        result.value = read(0);
        for (size_t i = 1; i < count; ++i)
        {
            result.mixed |= read(i) != result.value;
        }
        if (result.mixed && dominant)
        {
            std::map<T, std::pair<size_t, size_t>> frequencies;
            for (size_t i = 0; i < count; ++i)
            {
                const T value = read(i);
                auto [entry, inserted] = frequencies.try_emplace(value, 0, i);
                ++entry->second.first;
            }
            size_t bestCount = 0, bestIndex = count;
            for (const auto& [value, frequency] : frequencies)
            {
                if (frequency.first > bestCount || (frequency.first == bestCount && frequency.second < bestIndex))
                {
                    result.value = value;
                    bestCount = frequency.first;
                    bestIndex = frequency.second;
                }
            }
        }
        return result;
    }

    class Session
    {
        public:
        using Patch = std::function<bool(int, ItemData&, std::string&)>;

        bool Write(EditorScene& scene, const std::vector<int>& ids, unsigned owner, const Patch& patch, std::string& error);
        bool Finish(std::string& error);
        bool CaptureAfter(EditorScene& scene, std::string& error);
        void Cancel(EditorScene& scene) noexcept;
        bool Active() const noexcept
        {
            return action != nullptr;
        }
        unsigned Owner() const noexcept
        {
            return owner;
        }
        bool Contains(int id) const noexcept;
        void Reset() noexcept;

        private:
        bool Begin(EditorScene& scene, const std::vector<int>& ids, unsigned control, std::string& error);
        std::unique_ptr<ItemDataEditAction> action;
        std::vector<ItemData> working;
        struct Identity
        {
            int id;
            int model;
            std::array<char, 64> name;
        };
        std::vector<Identity> identities;
        unsigned owner = 0;
    };

    bool SetWorldPoses(EditorScene& scene, const std::vector<int>& ids, const std::vector<TransformData>& poses,
        std::vector<TransformData>& locals, std::string& error);
}
