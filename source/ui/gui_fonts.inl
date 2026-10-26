// Source Repository: https://github.com/waverso/waversomapstudio

static constexpr ImWchar StudioGlyphRanges[] = { 0x0020, 0x00FF, 0x2014, 0x2014, 0 };

void Gui::LoadFonts(float rasterizerDensity)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontAtlas* atlas = io.Fonts;
    atlas->Clear();

    char winDir[MAX_PATH] {};
    UINT dirLen = GetWindowsDirectoryA(winDir, MAX_PATH);
    if (dirLen == 0 || dirLen >= MAX_PATH)
    {
        strcpy_s(winDir, "C:\\Windows");
    }

    char pathReg[MAX_PATH], pathBold[MAX_PATH];
    sprintf_s(pathReg, "%s\\Fonts\\segoeui.ttf", winDir);
    sprintf_s(pathBold, "%s\\Fonts\\segoeuib.ttf", winDir);

    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.RasterizerDensity = std::clamp(rasterizerDensity, 0.5f, 2.0f);
    cfg.PixelSnapH = true;
    cfg.GlyphRanges = StudioGlyphRanges;

    auto canReadFont = [](const char* path)
    {
        HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        CloseHandle(file);
        return true;
    };

    const bool regularFontAvailable = canReadFont(pathReg);
    const bool boldFontAvailable = canReadFont(pathBold);
    m_fontRegular = regularFontAvailable ? atlas->AddFontFromFileTTF(pathReg, 18.0f, &cfg) : nullptr;
    m_fontBold = boldFontAvailable ? atlas->AddFontFromFileTTF(pathBold, 18.0f, &cfg) : nullptr;
    if (!m_fontRegular)
    {
        ImFontConfig fallbackCfg = cfg;
        fallbackCfg.SizePixels = 18.0f;
        m_fontRegular = atlas->AddFontDefault(&fallbackCfg);
    }
    if (!m_fontBold)
    {
        m_fontBold = m_fontRegular;
    }
    m_fontSmall = regularFontAvailable ? atlas->AddFontFromFileTTF(pathReg, 15.0f, &cfg) : nullptr;
    if (!m_fontSmall)
    {
        m_fontSmall = m_fontRegular;
    }

    io.FontDefault = m_fontRegular;
}

void Gui::RebuildFontAtlas(float rasterizerDensity)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    LoadFonts(rasterizerDensity);
    ImGui::GetIO().Fonts->Build();
    ImGui_ImplDX9_CreateDeviceObjects();
}
