// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "user_data.h"

namespace UserData
{
    class Storage
    {
        public:
        explicit Storage(const std::filesystem::path& directory);
        ~Storage();
        Storage(const Storage&) = delete;
        Storage& operator=(const Storage&) = delete;
        std::optional<Document> Load(Group group);
        void Submit(Document document);
        void Flush();
        bool HasError() const noexcept;
        std::string TakeError();

        private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
