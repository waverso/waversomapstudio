// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

struct ClockSnapshot
{
    unsigned short seconds = 0;
    unsigned char minutes = 0;
    unsigned char hours = 0;
    unsigned char days = 0;
    unsigned char month = 0;
    unsigned char currentDay = 0;
    unsigned int lastClockTick = 0;
};

class GameClockController
{
    public:
    GameClockController();
    ~GameClockController();

    void SetOverride(bool enabled, int hour);
    void UpdateFrameBoundary();

    private:
    static ClockSnapshot CaptureClock();
    static void ApplyClock(const ClockSnapshot& clock, bool refreshTick);
    bool IsClockCanonical(const ClockSnapshot& clock) const;
    void ObserveExternalClock();
    void EnforceOverride();

    bool m_enabled = false;
    unsigned char m_desiredHour = 12;
    ClockSnapshot m_externalClock {};
    ClockSnapshot m_overrideClock {};
};

extern GameClockController* g_gameClockController;
