// Source Repository: https://github.com/waverso/waversomapstudio

#include "user_storage.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include "../utils/atomic_file.h"

namespace UserData
{
    namespace
    {
        constexpr std::array Names = { L"settings.bin", L"bookmarks.bin", L"favorites.bin", L"themes.bin" };
        constexpr std::array Labels = { "settings", "bookmarks", "favorites", "themes" };
        using Clock = std::chrono::steady_clock;

        struct Handle
        {
            HANDLE value;
            ~Handle()
            {
                if (value != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(value);
                }
            }
        };

        void AtomicWrite(const std::filesystem::path& path, const Bytes& bytes)
        {
            std::filesystem::create_directories(path.parent_path());
            const auto result = Utils::AtomicWrite(path, bytes, true);
            if (result != Utils::AtomicWriteResult::Success)
            {
                throw std::runtime_error(result == Utils::AtomicWriteResult::CreateFailed ? "Cannot create a temporary file."
                        : result == Utils::AtomicWriteResult::WriteFailed
                        ? "File write failed; the previous file was preserved."
                        : "Cannot replace the file; the previous file was preserved.");
            }
        }
    }

    struct Storage::Impl
    {
        struct Slot
        {
            std::shared_ptr<const Document> latest;
            std::shared_ptr<const Bytes> written;
            Clock::time_point due;
            std::uint64_t generation = 0, attempted = 0, flushTarget = 0;
            bool pending = false, blocked = false, reported = false;
        };

        std::filesystem::path directory;
        std::array<Slot, Names.size()> slots;
        std::array<std::string, Names.size()> errors;
        std::mutex mutex;
        std::condition_variable condition;
        bool stopping = false;
        std::atomic_bool hasError = false;
        std::thread worker;

        explicit Impl(const std::filesystem::path& path)
            : directory(path), worker(
                                   [this]
                                   {
                                       Run();
                                   })
        {
        }

        ~Impl()
        {
            {
                std::lock_guard lock(mutex);
                stopping = true;
            }
            condition.notify_all();
            worker.join();
        }

        void Report(std::size_t index, const char* error)
        {
            if (!slots[index].reported)
            {
                errors[index] = "Cannot save/load " + std::string(Labels[index]) + ": " + error;
                slots[index].reported = true;
                hasError.store(true, std::memory_order_relaxed);
            }
        }

        void Run()
        {
            std::unique_lock lock(mutex);
            for (;;)
            {
                std::size_t next = slots.size();
                auto wake = (Clock::time_point::max)();
                const auto now = Clock::now();
                for (std::size_t i = 0; i < slots.size(); ++i)
                {
                    const auto& slot = slots[i];
                    if (!slot.pending)
                    {
                        continue;
                    }
                    if (stopping || slot.attempted < slot.flushTarget || slot.due <= now)
                    {
                        next = i;
                        break;
                    }
                    if (slot.due < wake)
                    {
                        wake = slot.due;
                    }
                }
                if (next == slots.size())
                {
                    if (stopping)
                    {
                        return;
                    }
                    if (wake == (Clock::time_point::max)())
                    {
                        condition.wait(lock);
                    }
                    else
                    {
                        condition.wait_until(lock, wake);
                    }
                    continue;
                }
                auto& slot = slots[next];
                const auto document = slot.latest;
                const auto previous = slot.written;
                const auto generation = slot.generation;
                slot.pending = false;
                lock.unlock();
                Bytes bytes;
                std::shared_ptr<const Bytes> committed;
                std::string error;
                try
                {
                    bytes = Encode(*document);
                    if (!previous || bytes != *previous)
                    {
                        AtomicWrite(directory / Names[next], bytes);
                    }
                    committed = std::make_shared<const Bytes>(std::move(bytes));
                }
                catch (const std::exception& failure)
                {
                    try
                    {
                        error = failure.what();
                    }
                    catch (...)
                    {
                        error = "Out of memory.";
                    }
                }
                lock.lock();
                slot.attempted = generation;
                if (error.empty())
                {
                    slot.written = std::move(committed);
                    slot.reported = false;
                }
                else
                {
                    try
                    {
                        Report(next, error.c_str());
                    }
                    catch (...)
                    {
                    }
                    if (slot.generation == generation)
                    {
                        slot.pending = !stopping;
                        slot.due = Clock::now() + std::chrono::seconds(5);
                    }
                }
                condition.notify_all();
            }
        }
    };

    Storage::Storage(const std::filesystem::path& directory) : m_impl(std::make_unique<Impl>(directory))
    {
    }
    Storage::~Storage() = default;

    std::optional<Document> Storage::Load(Group group)
    {
        const auto index = static_cast<std::size_t>(group);
        if (index >= Names.size())
        {
            throw std::runtime_error("Invalid user data group.");
        }
        try
        {
            Handle file { CreateFileW((m_impl->directory / Names[index]).c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
            if (file.value == INVALID_HANDLE_VALUE)
            {
                const auto error = GetLastError();
                if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                {
                    return std::nullopt;
                }
                throw std::runtime_error("Cannot open the file; automatic saving is disabled for this file.");
            }
            LARGE_INTEGER size {};
            if (!GetFileSizeEx(file.value, &size) || size.QuadPart <= 0 || size.QuadPart > MaxFileBytes)
            {
                throw std::runtime_error("Invalid file size; the file was preserved.");
            }
            Bytes bytes(static_cast<std::size_t>(size.QuadPart));
            DWORD read = 0;
            if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size())
            {
                throw std::runtime_error("Cannot read the file; the file was preserved.");
            }
            Document document = Decode(group, bytes);
            std::lock_guard lock(m_impl->mutex);
            m_impl->slots[index].written = std::make_shared<const Bytes>(std::move(bytes));
            return document;
        }
        catch (const std::exception& error)
        {
            std::lock_guard lock(m_impl->mutex);
            m_impl->slots[index].blocked = true;
            m_impl->Report(index, error.what());
            return std::nullopt;
        }
    }

    void Storage::Submit(Document document)
    {
        const auto index = document.index();
        if (index >= Names.size())
        {
            throw std::runtime_error("Invalid user data document.");
        }
        auto snapshot = std::make_shared<const Document>(std::move(document));
        {
            std::lock_guard lock(m_impl->mutex);
            auto& slot = m_impl->slots[index];
            if (m_impl->stopping || slot.blocked)
            {
                return;
            }
            slot.latest = std::move(snapshot);
            ++slot.generation;
            slot.pending = true;
            slot.due = Clock::now() + std::chrono::milliseconds(600);
        }
        m_impl->condition.notify_all();
    }

    void Storage::Flush()
    {
        std::unique_lock lock(m_impl->mutex);
        std::array<std::uint64_t, Names.size()> targets {};
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            auto& slot = m_impl->slots[i];
            targets[i] = slot.generation;
            slot.flushTarget = slot.generation;
        }
        m_impl->condition.notify_all();
        m_impl->condition.wait(lock,
            [&]
            {
                for (std::size_t i = 0; i < targets.size(); ++i)
                {
                    if (m_impl->slots[i].attempted < targets[i])
                    {
                        return false;
                    }
                }
                return true;
            });
    }

    bool Storage::HasError() const noexcept
    {
        return m_impl->hasError.load(std::memory_order_relaxed);
    }

    std::string Storage::TakeError()
    {
        std::lock_guard lock(m_impl->mutex);
        for (auto& error : m_impl->errors)
        {
            if (!error.empty())
            {
                std::string result = std::exchange(error, {});
                bool pending = false;
                for (const auto& remaining : m_impl->errors)
                {
                    pending |= !remaining.empty();
                }
                m_impl->hasError.store(pending, std::memory_order_relaxed);
                return result;
            }
        }
        return {};
    }
}
