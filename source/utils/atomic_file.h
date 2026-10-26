// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace Utils
{
    enum class AtomicWriteResult
    {
        Success,
        CreateFailed,
        WriteFailed,
        DestinationExists,
        ReplaceFailed
    };

    inline AtomicWriteResult AtomicWrite(const std::filesystem::path& path, std::span<const std::uint8_t> bytes, bool overwrite)
    {
        std::filesystem::path temporary;
        HANDLE file = INVALID_HANDLE_VALUE;
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            temporary = path;
            temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L"."
                + std::to_wstring(attempt) + L".tmp";
            file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS)
            {
                break;
            }
        }
        if (file == INVALID_HANDLE_VALUE)
        {
            return AtomicWriteResult::CreateFailed;
        }
        DWORD written = 0;
        const bool complete = bytes.size() <= MAXDWORD
            && ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size()
            && FlushFileBuffers(file);
        const bool closed = CloseHandle(file) != FALSE;
        if (!complete || !closed)
        {
            DeleteFileW(temporary.c_str());
            return AtomicWriteResult::WriteFailed;
        }
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temporary.c_str(), path.c_str(), flags))
        {
            const DWORD failure = GetLastError();
            DeleteFileW(temporary.c_str());
            return failure == ERROR_ALREADY_EXISTS || failure == ERROR_FILE_EXISTS ? AtomicWriteResult::DestinationExists
                                                                                   : AtomicWriteResult::ReplaceFailed;
        }
        return AtomicWriteResult::Success;
    }
}
