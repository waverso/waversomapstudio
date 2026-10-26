// Source Repository: https://github.com/waverso/waversomapstudio

struct GizmoPanelLayout
{
    float gridWidth = 86.0f;
    float toggleWidth = 78.0f;
    float sectionGap = Theme::g_popupPadding;
    float rowGap = Theme::g_popupPadding;

    float ModeHeight() const
    {
        return Theme::g_frameHeight * 2.0f + rowGap;
    }
    float ModeWidth() const
    {
        return Theme::g_frameHeight * 2.0f + rowGap;
    }
    float Width() const
    {
        return Theme::g_outerPadding * 2.0f + gridWidth + sectionGap + ModeWidth() + sectionGap + toggleWidth;
    }
    float Height() const
    {
        return Theme::g_accentSize + Theme::g_outerPadding * 2.0f + ModeHeight();
    }

    void UpdateMetrics()
    {
        gridWidth = Ui::Px(86.0f);
        toggleWidth = Ui::Px(78.0f);
        sectionGap = Theme::g_popupPadding;
        rowGap = Theme::g_popupPadding;
    }

    ImRect Rect(const ImVec2& display) const
    {
        const ImVec2 size(Width(), Height());
        const ImVec2 pos(Theme::g_outerPadding, ImMax(0.0f, display.y - Theme::g_outerPadding - size.y));
        return ImRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    }
};
static GizmoPanelLayout g_gizmoPanelLayout {};

struct UtilityPanelLayout
{
    float buttonSize = Theme::g_frameHeight;
    float buttonGap = Theme::g_popupPadding;
    float screenMargin = Theme::g_outerPadding;
    float popupWidth = 250.0f;

    float Width() const
    {
        return Theme::g_outerPadding * 2.0f + buttonSize * 3.0f + buttonGap * 2.0f;
    }
    float Height() const
    {
        return Theme::g_accentSize + Theme::g_outerPadding * 2.0f + buttonSize * 2.0f + buttonGap;
    }

    void UpdateMetrics()
    {
        buttonSize = Theme::g_frameHeight;
        buttonGap = Theme::g_popupPadding;
        screenMargin = Theme::g_outerPadding;
        popupWidth = Ui::Px(250.0f);
    }

    ImRect Rect(const ImVec2& display, float sidebarWidth) const
    {
        if (sidebarWidth <= 0.0f)
        {
            sidebarWidth = display.x * 0.15f;
        }
        const ImVec2 size(Width(), Height());
        const float right = display.x - sidebarWidth - screenMargin;
        const ImVec2 pos(ImMax(0.0f, right - size.x), ImMax(0.0f, display.y - screenMargin - size.y));
        return ImRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    }
};

struct UtilityPanelState
{
    bool timeEnabled = false;
    bool weatherEnabled = false;
    int timeValue = 12;
    int timeDraft = 12;
    int weatherValue = 0;
    int weatherDraft = 0;
    bool weatherApplied = false;
    short lastWeatherForcedByServer = 0;
    short lastWeatherForcedByReset = 0;
    short lastWeatherToSet = 0;
    SceneTools::CloneOffsetSettings cloneOffset;
    std::mt19937 cloneOffsetRandom { static_cast<std::mt19937::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count()) };
    char cloneOffsetRangeBuf[12][16] {};
    bool cloneOffsetRangeFocus[12] {};
};

static UtilityPanelLayout g_utilityPanelLayout {};
static UtilityPanelState g_utilityPanelState;

static void UpdateLayoutMetrics()
{
    Theme::UpdateMetrics();
    g_gizmoPanelLayout.UpdateMetrics();
    g_utilityPanelLayout.UpdateMetrics();
}
