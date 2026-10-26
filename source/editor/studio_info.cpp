// Source Repository: https://github.com/waverso/waversomapstudio

#include "studio_info.h"

#include <Windows.h>
#include <array>
#include <charconv>
#include <chrono>
#include <future>
#include <memory>
#include <shellapi.h>
#include <string>
#include <string_view>
#include <utility>
#include <winhttp.h>

namespace StudioInfo
{
    namespace
    {
        struct UpdateResult
        {
            UpdateState state;
            std::string version;
        };
        std::future<UpdateResult> g_pending;
        UpdateState g_state = UpdateState::Idle;
        std::string g_latestVersion;
        std::uint64_t g_lastCheckTime = 0;

        std::uint64_t CurrentTime()
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }

        bool UpdateCheckDue(std::uint32_t days, std::uint64_t lastCheck, std::uint64_t now)
        {
            return (days == 1 || days == 7 || days == 30)
                && (!lastCheck || now < lastCheck || now - lastCheck >= std::uint64_t(days) * 86400);
        }

        bool ParseVersion(std::string_view text, std::array<unsigned, 3>& parts)
        {
            if (text.starts_with("\xEF\xBB\xBF"))
            {
                text.remove_prefix(3);
            }
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == text.npos)
            {
                return false;
            }
            text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
            for (unsigned index = 0; index < parts.size(); ++index)
            {
                const auto dot = text.find('.');
                const auto number = text.substr(0, dot);
                const auto parsed = std::from_chars(number.data(), number.data() + number.size(), parts[index]);
                if (parsed.ec != std::errc {} || parsed.ptr != number.data() + number.size())
                {
                    return false;
                }
                if (dot == text.npos)
                {
                    return index >= 1;
                }
                text.remove_prefix(dot + 1);
            }
            return false;
        }

        UpdateState EvaluateVersion(std::string_view text)
        {
            std::array<unsigned, 3> current {}, latest {};
            if (!ParseVersion(Version, current) || !ParseVersion(text, latest))
            {
                return UpdateState::Failed;
            }
            return latest > current ? UpdateState::Available : UpdateState::Current;
        }

        UpdateState FetchVersion(std::string* version = nullptr)
        {
            using Internet = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;
            Internet session(WinHttpOpen(L"waversomapstudio", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0),
                WinHttpCloseHandle);
            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            if (!session || !WinHttpSetTimeouts(session.get(), 4000, 4000, 4000, 4000)
                || !WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
            {
                return UpdateState::Failed;
            }
            Internet connection(
                WinHttpConnect(session.get(), L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT, 0), WinHttpCloseHandle);
            if (!connection)
            {
                return UpdateState::Failed;
            }
            Internet request(WinHttpOpenRequest(connection.get(), L"GET", L"/waverso/waversomapstudio/refs/heads/main/version.txt", nullptr,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE),
                WinHttpCloseHandle);
            DWORD status = 0, size = sizeof(status);
            if (!request
                || !WinHttpSendRequest(request.get(), L"Cache-Control: no-cache\r\n", -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                || !WinHttpReceiveResponse(request.get(), nullptr)
                || !WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)
                || status != 200)
            {
                return UpdateState::Failed;
            }
            std::array<char, 64> body {};
            DWORD total = 0, received = 0;
            do
            {
                if (total == body.size()
                    || !WinHttpReadData(request.get(), body.data() + total, static_cast<DWORD>(body.size()) - total, &received))
                {
                    return UpdateState::Failed;
                }
                total += received;
            } while (received);
            std::string_view text(body.data(), total);
            const auto state = EvaluateVersion(text);
            if (version && state != UpdateState::Failed)
            {
                if (text.starts_with("\xEF\xBB\xBF"))
                {
                    text.remove_prefix(3);
                }
                const auto first = text.find_first_not_of(" \t\r\n");
                *version = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
            }
            return state;
        }
    }

    const char* CompileDateUtc()
    {
        return STUDIO_COMPILE_UTC;
    }

    UpdateState Status()
    {
        return g_state;
    }

    bool CheckUpdate()
    {
        if (g_state == UpdateState::Checking)
        {
            return false;
        }
        try
        {
            g_pending = std::async(std::launch::async,
                []
                {
                    UpdateResult result {};
                    result.state = FetchVersion(&result.version);
                    return result;
                });
            g_state = UpdateState::Checking;
            return true;
        }
        catch (...)
        {
            g_state = UpdateState::Failed;
            return false;
        }
    }

    bool CheckUpdateIfDue(std::uint32_t days, std::uint64_t lastCheck)
    {
        return UpdateCheckDue(days, lastCheck, CurrentTime()) && CheckUpdate();
    }

    const char* LatestVersion()
    {
        return g_latestVersion.c_str();
    }

    std::uint64_t LastCheckTime()
    {
        return g_lastCheckTime;
    }

    bool PollUpdate()
    {
        if (g_state != UpdateState::Checking || g_pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return false;
        }
        try
        {
            auto result = g_pending.get();
            g_state = result.state;
            g_latestVersion = std::move(result.version);
            if (g_state == UpdateState::Available || g_state == UpdateState::Current)
            {
                g_lastCheckTime = CurrentTime();
            }
        }
        catch (...)
        {
            g_state = UpdateState::Failed;
        }
        return true;
    }

    bool OpenLink(const char* url)
    {
        return reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    }

    void Shutdown()
    {
        if (g_pending.valid())
        {
            g_pending.wait();
        }
    }
}
