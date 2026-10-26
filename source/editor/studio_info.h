// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>

#if __has_include("studio_info_version.generated.h")
#include "studio_info_version.generated.h"
#elif defined(STUDIO_INFO_TEST_VERSION)
namespace StudioInfo
{
    inline constexpr char Version[] = STUDIO_INFO_TEST_VERSION;
}
#else
#error "studio_info_version.generated.h was not generated"
#endif

namespace StudioInfo
{
    inline constexpr char RepositoryUrl[] = "https://github.com/waverso/waversomapstudio";
    inline constexpr char YouTubeUrl[] = "https://youtube.com/@waverso";
    inline constexpr char ReleasesUrl[] = "https://github.com/waverso/waversomapstudio/releases/latest";

    enum class UpdateState
    {
        Idle,
        Checking,
        Current,
        Available,
        Failed
    };

    const char* CompileDateUtc();
    UpdateState Status();
    bool CheckUpdate();
    bool CheckUpdateIfDue(std::uint32_t days, std::uint64_t lastCheck);
    bool PollUpdate();
    const char* LatestVersion();
    std::uint64_t LastCheckTime();
    bool OpenLink(const char* url);
    void Shutdown();
}
