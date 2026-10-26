// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    std::wstring Wide(const char* text)
    {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
        if (length <= 0)
        {
            throw std::runtime_error("File name contains invalid UTF-8 text.");
        }
        std::wstring result(length, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), length);
        result.pop_back();
        return result;
    }

    std::string Utf8(const std::filesystem::path& path)
    {
        const auto name = path.u8string();
        return std::string(reinterpret_cast<const char*>(name.data()), name.size());
    }

    std::filesystem::path Directory(const wchar_t* folder)
    {
        std::wstring executable(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!length || length >= executable.size())
        {
            throw std::runtime_error("Cannot locate the game directory.");
        }
        executable.resize(length);
        const auto root = std::filesystem::path(executable).parent_path() / Wide(MOD_NAME);
        for (const wchar_t* name : { L"projects", L"exports" })
        {
            const auto path = root / name;
            std::error_code error;
            std::filesystem::create_directories(path, error);
            if (error)
            {
                throw std::runtime_error("Cannot create the " + Utf8(path.filename()) + " folder: " + error.message());
            }
            WIN32_FIND_DATAW data {};
            const HANDLE search = FindFirstFileW(path.c_str(), &data);
            if (search == INVALID_HANDLE_VALUE)
            {
                throw std::runtime_error("Cannot inspect the project storage folders.");
            }
            FindClose(search);
            if (std::wcscmp(data.cFileName, name) != 0 && !MoveFileExW((root / data.cFileName).c_str(), path.c_str(), 0))
            {
                throw std::runtime_error("Cannot rename the storage folder to lowercase.");
            }
        }
        return root / folder;
    }

    std::filesystem::path NamedPath(const wchar_t* folder, const char* name, const wchar_t* extension)
    {
        std::string error;
        std::string normalized;
        if (!NormalizeName(name, normalized, error))
        {
            throw std::runtime_error(error);
        }
        return Directory(folder) / (Wide(normalized.c_str()) + extension);
    }

    std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            throw std::runtime_error("Cannot open the project file.");
        }
        const auto size = stream.tellg();
        if (size < 0 || size > ProjectDocument::MaxFileBytes)
        {
            throw std::runtime_error("Invalid project file size.");
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            throw std::runtime_error("Cannot read the complete project file.");
        }
        return bytes;
    }

    bool HasProjectSignature(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        char signature[ProjectDocument::Magic.size()];
        return stream.read(signature, sizeof(signature))
            && std::memcmp(signature, ProjectDocument::Magic.data(), sizeof(signature)) == 0;
    }

    void AtomicWrite(const std::filesystem::path& path, std::span<const std::uint8_t> bytes, bool overwrite)
    {
        const auto result = Utils::AtomicWrite(path, bytes, overwrite);
        if (result != Utils::AtomicWriteResult::Success)
        {
            throw std::runtime_error(result == Utils::AtomicWriteResult::CreateFailed
                    ? "Cannot create a temporary file in the destination folder."
                    : result == Utils::AtomicWriteResult::WriteFailed ? "File write failed; the previous file was preserved."
                    : result == Utils::AtomicWriteResult::DestinationExists
                    ? "A file with this name already exists."
                    : "Cannot replace the destination file; the previous file was preserved.");
        }
    }
}

std::filesystem::path StorageRoot()
{
    return Directory(L"");
}

bool ValidName(const char* name, std::string& error)
{
    std::string normalized;
    return NormalizeName(name, normalized, error);
}

bool NormalizeName(const char* name, std::string& normalized, std::string& error)
{
    if (!ProjectFilename::Normalize(name ? name : "", normalized, error))
    {
        return false;
    }
    try
    {
        Wide(normalized.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

bool ProjectExists(const char* name, bool& exists, std::string& error)
{
    try
    {
        exists = std::filesystem::exists(NamedPath(L"projects", name, L".bin"));
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

bool List(std::vector<Entry>& entries, std::string& error)
{
    entries.clear();
    try
    {
        for (const auto& file : std::filesystem::directory_iterator(Directory(L"projects")))
        {
            if (!file.is_regular_file() || _wcsicmp(file.path().extension().c_str(), L".bin") != 0
                || !HasProjectSignature(file.path()))
            {
                continue;
            }
            ProjectDocument::Items items;
            std::string reason;
            try
            {
                if (!ProjectDocument::Decode(ReadFile(file.path()), items, reason))
                {
                    continue;
                }
                entries.push_back({ file.path(), Utf8(file.path().stem()), items.size(), file.last_write_time() });
            }
            catch (const std::exception&)
            {
            }
        }
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        entries.clear();
        error = e.what();
        return false;
    }
}
