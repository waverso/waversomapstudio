// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <filesystem>

#include "pawn_export.h"
#include "pawn_import.h"

namespace Projects
{
    struct Entry
    {
        std::filesystem::path path;
        std::string name;
        std::size_t itemCount = 0;
        std::filesystem::file_time_type lastSaved {};
    };

    const std::string& Name();
    std::filesystem::path StorageRoot();
    bool IsNamed();
    bool IsCurrent(const Entry& entry);
    bool IsDirty(bool refresh = false);
    void UpdateDirty();
    bool Save(const char* name, std::string& error, bool overwrite = false, bool saveAs = false);
    bool ProjectExists(const char* name, bool& exists, std::string& error);
    bool List(std::vector<Entry>& entries, std::string& error);
    bool Load(const Entry& entry, std::string& error);
    bool Delete(const Entry& entry, std::string& error);
    void New();
    PawnImport::Result PreviewImport(const char* source);
    bool ApplyImport(const PawnImport::Result& result, std::string& error);
    bool Export(const char* name, const PawnExport::Options& options, bool overwrite, std::string& error);
    bool ExportExists(const char* name, bool& exists, std::string& error);
    bool ValidName(const char* name, std::string& error);
    bool NormalizeName(const char* name, std::string& normalized, std::string& error);
}
