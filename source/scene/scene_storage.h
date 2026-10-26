// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

template <class T, size_t BlockSize = 64>
class SceneStorage
{
    public:
    static constexpr size_t EntriesPerBlock = BlockSize;

    SceneStorage() : blocks(0), pointers(0)
    {
    }

    void Swap(SceneStorage& other) noexcept
    {
        blocks.swap(other.blocks);
        pointers.swap(other.pointers);
        std::swap(lookup, other.lookup);
        std::swap(capacity, other.capacity);
    }

    void EnsureCapacity(size_t count)
    {
        const size_t needed = (count + BlockSize - 1) / BlockSize;
        if (needed <= blocks.size())
        {
            return;
        }
        pointers.reserve(needed);
        lookup = pointers.data();
        blocks.reserve(needed);
        while (blocks.size() < needed)
        {
            auto block = std::make_unique<T[]>(BlockSize);
            T* address = block.get();
            blocks.push_back(std::move(block));
            pointers.push_back(address);
            capacity += BlockSize;
        }
    }
    void Trim(size_t count) noexcept
    {
        const size_t needed = (count + BlockSize - 1) / BlockSize;
        while (blocks.size() > needed)
        {
            blocks.pop_back();
            pointers.pop_back();
            capacity -= BlockSize;
        }
    }
    size_t Capacity() const noexcept
    {
        return capacity;
    }
    __forceinline T& operator[](size_t index) noexcept
    {
        assert(index < capacity);
        return lookup[index / BlockSize][index % BlockSize];
    }
    __forceinline const T& operator[](size_t index) const noexcept
    {
        assert(index < capacity);
        return lookup[index / BlockSize][index % BlockSize];
    }

    private:
    std::vector<std::unique_ptr<T[]>> blocks;
    std::vector<T*> pointers;
    T* const* lookup = nullptr;
    size_t capacity = 0;
};
