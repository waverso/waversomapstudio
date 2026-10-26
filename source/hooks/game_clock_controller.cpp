// Source Repository: https://github.com/waverso/waversomapstudio

#include "game_clock_controller.h"

#include <algorithm>

#include "CClock.h"
#include "CTimer.h"

GameClockController* g_gameClockController = nullptr;

GameClockController::GameClockController()
{
}

GameClockController::~GameClockController()
{
    if (m_enabled)
    {
        ObserveExternalClock();
        ApplyClock(m_externalClock, true);
    }
}

ClockSnapshot GameClockController::CaptureClock()
{
    ClockSnapshot clock;
    clock.seconds = CClock::ms_nGameClockSeconds;
    clock.minutes = CClock::ms_nGameClockMinutes;
    clock.hours = CClock::ms_nGameClockHours;
    clock.days = CClock::ms_nGameClockDays;
    clock.month = CClock::ms_nGameClockMonth;
    clock.currentDay = CClock::CurrentDay;
    clock.lastClockTick = CClock::ms_nLastClockTick;
    return clock;
}

void GameClockController::ApplyClock(const ClockSnapshot& clock, bool refreshTick)
{
    CClock::ms_nGameClockSeconds = clock.seconds;
    CClock::ms_nGameClockMinutes = clock.minutes;
    CClock::ms_nGameClockHours = clock.hours;
    CClock::ms_nGameClockDays = clock.days;
    CClock::ms_nGameClockMonth = clock.month;
    CClock::CurrentDay = clock.currentDay;
    CClock::ms_nLastClockTick = refreshTick ? CTimer::m_snTimeInMilliseconds : clock.lastClockTick;
}

bool GameClockController::IsClockCanonical(const ClockSnapshot& clock) const
{
    return clock.hours == m_overrideClock.hours && clock.minutes == m_overrideClock.minutes && clock.days == m_overrideClock.days
        && clock.month == m_overrideClock.month && clock.currentDay == m_overrideClock.currentDay;
}

void GameClockController::ObserveExternalClock()
{
    if (!m_enabled)
    {
        return;
    }
    const ClockSnapshot current = CaptureClock();
    if (IsClockCanonical(current))
    {
        return;
    }
    m_externalClock = current;
}

void GameClockController::EnforceOverride()
{
    if (!m_enabled)
    {
        return;
    }
    m_overrideClock.hours = m_desiredHour;
    m_overrideClock.minutes = 0;
    m_overrideClock.seconds = 0;
    m_overrideClock.lastClockTick = CTimer::m_snTimeInMilliseconds;
    ApplyClock(m_overrideClock, false);
}

void GameClockController::SetOverride(bool enabled, int hour)
{
    const unsigned char desiredHour = static_cast<unsigned char>(std::clamp(hour, 0, 23));
    if (m_enabled && (!enabled || desiredHour != m_desiredHour))
    {
        ObserveExternalClock();
    }
    if (enabled && !m_enabled)
    {
        m_externalClock = CaptureClock();
        m_overrideClock = m_externalClock;
        m_desiredHour = desiredHour;
        m_enabled = true;
        EnforceOverride();
        return;
    }

    if (!enabled && m_enabled)
    {
        m_enabled = false;
        ApplyClock(m_externalClock, true);
        return;
    }

    if (!enabled)
    {
        return;
    }
    if (desiredHour != m_desiredHour)
    {
        m_desiredHour = desiredHour;
        EnforceOverride();
    }
}

void GameClockController::UpdateFrameBoundary()
{
    ObserveExternalClock();
    EnforceOverride();
}
