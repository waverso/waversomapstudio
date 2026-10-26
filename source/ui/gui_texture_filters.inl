// Source Repository: https://github.com/waverso/waversomapstudio

static void ClearAllTexBrowserFilters(TexBrowserUiState& ui)
{
    ui.search[0] = '\0';
    ui.sourceMask = 0;
    ui.tagFilter = -1;
    ui.categoryMask = 0;
    ui.dimensionMask = 0;
    ui.searchFields = TextureBrowser::SearchDefault;
    ui.seamlessFilter = -1;
    ui.hideDuplicates = true;
    ui.lodFilter = BrowserFilter::Hide;
    ui.mipMin = ui.mipMax = -1;
    ui.sizeMinKb = ui.sizeMaxKb = -1;
    ui.wMin = ui.wMax = -1;
    ui.hMin = ui.hMax = -1;
    ui.aspectW = ui.aspectH = 0;
    ui.favoritesOnly = false;
    ui.sortKey = 0;
    ui.sortDesc = false;
    ui.dirty = true;
    ui.page = 0;
}

static bool TexBrowserFiltersPanelActive(const TexBrowserUiState& ui)
{
    if (ui.sourceMask != 0)
    {
        return true;
    }
    if (ui.categoryMask != 0)
    {
        return true;
    }
    if (ui.dimensionMask != 0)
    {
        return true;
    }
    if (ui.searchFields != TextureBrowser::SearchDefault)
    {
        return true;
    }
    if (ui.seamlessFilter >= 0)
    {
        return true;
    }
    if (!ui.hideDuplicates)
    {
        return true;
    }
    if (ui.lodFilter != BrowserFilter::Hide)
    {
        return true;
    }
    if (ui.mipMin >= 0 || ui.mipMax >= 0)
    {
        return true;
    }
    if (ui.sizeMinKb >= 0 || ui.sizeMaxKb >= 0)
    {
        return true;
    }
    if (ui.wMin >= 0 || ui.wMax >= 0)
    {
        return true;
    }
    if (ui.hMin >= 0 || ui.hMax >= 0)
    {
        return true;
    }
    if (ui.aspectW > 0 && ui.aspectH > 0)
    {
        return true;
    }
    return false;
}

static bool TexBrowserHasActiveFilters(const TexBrowserUiState& ui)
{
    return ui.search[0] || ui.favoritesOnly || ui.sortKey != 0 || ui.sortDesc || TexBrowserFiltersPanelActive(ui);
}

static void OpenAdvFilters()
{
    TexBrowserUiState& ui = g_textureBrowserUi;
    ui.advOpen = true;
    ui.advMip[0] = ui.mipMin < 0 ? 1 : ui.mipMin;
    ui.advMip[1] = ui.mipMax < 0 ? 10 : ui.mipMax;
    ui.advSize[0] = ui.sizeMinKb < 0 ? 1 : ui.sizeMinKb;
    ui.advSize[1] = ui.sizeMaxKb < 0 ? 256 : ui.sizeMaxKb;
    ui.advW[0] = ui.wMin < 0 ? 1 : ui.wMin;
    ui.advW[1] = ui.wMax < 0 ? 512 : ui.wMax;
    ui.advH[0] = ui.hMin < 0 ? 1 : ui.hMin;
    ui.advH[1] = ui.hMax < 0 ? 512 : ui.hMax;
    ui.advHideDups = ui.hideDuplicates;
    ui.advLodFilter = ui.lodFilter;
    ui.advTagFilter = ui.tagFilter;
    ui.advSourceMask = ui.sourceMask;
    ui.advSeamlessFilter = ui.seamlessFilter;
    ui.advCategoryMask = ui.categoryMask;
    ui.advDimensionMask = ui.dimensionMask;
    ui.advSearchFields = ui.searchFields ? ui.searchFields : TextureBrowser::SearchTexture;
    ui.advAspectIdx = 0;
}

static void ApplyAdvFilters()
{
    TexBrowserUiState& ui = g_textureBrowserUi;
    ui.mipMin = ui.advMip[0] <= 1 ? -1 : ui.advMip[0];
    ui.mipMax = ui.advMip[1] >= 10 ? -1 : ui.advMip[1];
    ui.sizeMinKb = ui.advSize[0] <= 1 ? -1 : ui.advSize[0];
    ui.sizeMaxKb = ui.advSize[1] >= 256 ? -1 : ui.advSize[1];
    ui.wMin = ui.advW[0] <= 1 ? -1 : ui.advW[0];
    ui.wMax = ui.advW[1] >= 512 ? -1 : ui.advW[1];
    ui.hMin = ui.advH[0] <= 1 ? -1 : ui.advH[0];
    ui.hMax = ui.advH[1] >= 512 ? -1 : ui.advH[1];
    ui.hideDuplicates = ui.advHideDups;
    ui.lodFilter = ui.advLodFilter;
    ui.tagFilter = -1;
    ui.categoryMask = ui.advCategoryMask;
    ui.sourceMask = ui.advSourceMask;
    ui.seamlessFilter = ui.advSeamlessFilter;
    ui.dimensionMask = ui.advDimensionMask;
    ui.searchFields = ui.advSearchFields ? ui.advSearchFields : TextureBrowser::SearchTexture;
    ui.aspectW = ui.aspectH = 0;
    ui.dirty = true;
    ui.page = 0;
    ui.advOpen = false;
}

static void DrawAdvFiltersModal()
{
    TexBrowserUiState& ui = g_textureBrowserUi;
    if (!ui.advOpen)
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    DrawModalBackdrop("##AdvFiltersBackdrop");

    const float winW = Ui::Px(520.0f);
    const float rowH = Theme::g_frameHeight + Theme::g_padding;
    const float comboRowH = Theme::g_frameHeight + Theme::g_padding;
    const float winH = ModalHeight(comboRowH * 5.0f + rowH * 4.0f - Theme::g_padding);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(winW, ceilf(winH)), ImGuiCond_Always);
    PushModalStyle();

    bool apply = false, cancel = false;

    if (ImGui::Begin("##AdvFilters", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 winPos = ImGui::GetWindowPos();
        DrawModalTitleBar(dl, winPos, winW, "Texture Filters");

        const float padX = Theme::g_outerPadding;
        const float sliderX = winPos.x + padX;
        const float sliderW = winW - padX * 2.0f;
        float y = winPos.y + Theme::g_tabHeight + Theme::g_padding;

        static const char* kSeamlessNames[] = { "Vertical", "Horizontal", "Both" };

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        {
            const float cgap = Theme::g_padding;
            const float cW = (winW - padX * 2.0f - cgap * 2.0f) / 3.0f;
            const float fullW = winW - padX * 2.0f;
            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter<192, 224>("##adv_source_multi", "Source", ui.advSourceMask, CatalogDatabase::ArchiveCount,
                TextureBrowser::SourceImgName, fullW);
            y += comboRowH;

            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter<192, 224>("##adv_cat_multi", "Category", ui.advCategoryMask, TextureBrowser::TagCount(),
                TextureBrowser::TagName, fullW, &ui.categorySearch);
            y += comboRowH;

            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter<192, 224>("##adv_dim_multi", "Aspect Ratio", ui.advDimensionMask,
                TextureBrowser::DimensionRatioCount(), TextureBrowser::DimensionRatioName, fullW);
            y += comboRowH;

            static const char* kSearchFieldNames[] = { "Texture Name", "Library Name", "Description" };
            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter<192, 224>(
                "##adv_search_index", "Search In", ui.advSearchFields, 3,
                [](int i)
                {
                    return kSearchFieldNames[i];
                },
                fullW, nullptr, TextureBrowser::SearchTexture, "Texture Name", true);
            y += comboRowH;

            auto triStateCombo = [&](const char* id, const char* title, std::uint8_t& value, float comboX)
            {
                value = static_cast<std::uint8_t>(ImClamp(static_cast<int>(value), 0, 2));
                ImGui::SetCursorScreenPos(ImVec2(comboX, y));
                DrawVisibilityFilter(id, title, value, cW);
            };

            const float rowX = winPos.x + padX;
            triStateCombo("##adv_lods", "Low LODs", ui.advLodFilter, rowX);

            char duplicatesLabel[64];
            snprintf(duplicatesLabel, sizeof(duplicatesLabel), "Duplicates: %s", ui.advHideDups ? "Hide" : "Show");
            ImGui::SetCursorScreenPos(ImVec2(rowX + cW + cgap, y));
            if (ImCustom::BeginCombo("##adv_dups", duplicatesLabel, cW))
            {
                if (ImCustom::ComboItem("Show", !ui.advHideDups))
                {
                    ui.advHideDups = false;
                }
                if (ImCustom::ComboItem("Hide", ui.advHideDups))
                {
                    ui.advHideDups = true;
                }
                ImCustom::EndCombo();
            }

            const char* seamlessValue = ui.advSeamlessFilter < 0 ? "Any" : kSeamlessNames[ImClamp(ui.advSeamlessFilter, 0, 2)];
            char seamlessLabel[64];
            snprintf(seamlessLabel, sizeof(seamlessLabel), "Seamless: %s", seamlessValue);
            ImGui::SetCursorScreenPos(ImVec2(rowX + (cW + cgap) * 2.0f, y));
            if (ImCustom::BeginCombo("##adv_seamless", seamlessLabel, cW))
            {
                if (ImCustom::ComboItem("Any", ui.advSeamlessFilter < 0))
                {
                    ui.advSeamlessFilter = -1;
                }
                for (int option = 0; option < 3; ++option)
                {
                    if (ImCustom::ComboItem(kSeamlessNames[option], ui.advSeamlessFilter == option))
                    {
                        ui.advSeamlessFilter = option;
                    }
                }
                ImCustom::EndCombo();
            }
            y += comboRowH;
        }
        ImGui::PopStyleVar(3);

        auto sliderRow = [&](const char* label, const char* sid, int* v, int lo, int hi, int b)
        {
            ImGui::SetCursorScreenPos(ImVec2(sliderX, y));
            RangeSliderInt(
                sid, label, &v[0], &v[1], lo, hi, sliderW, ui.advBuf[b], ui.advBuf[b + 1], ui.advFocus[b], ui.advFocus[b + 1]);
            y += rowH;
        };

        sliderRow("Mipmaps", "##adv_mip", ui.advMip, 1, 10, 0);
        sliderRow("Size (KB)", "##adv_size", ui.advSize, 1, 256, 2);
        sliderRow("Width (px)", "##adv_w", ui.advW, 1, 512, 4);
        sliderRow("Height (px)", "##adv_h", ui.advH, 1, 512, 6);

        const float btnY = DrawModalFooter();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        {
            const float bgap = Theme::g_padding;
            const float bw = (winW - padX * 2.0f - bgap) / 2.0f;
            float bx = winPos.x + padX;

            ImGui::SetCursorScreenPos(ImVec2(bx, btnY));
            if (ImGui::Button("Apply Filters##adv", ImVec2(bw, Theme::g_frameHeight)))
            {
                apply = true;
            }
            bx += bw + bgap;

            ImGui::SetCursorScreenPos(ImVec2(bx, btnY));
            if (ImGui::Button("Cancel##adv", ImVec2(bw, Theme::g_frameHeight)))
            {
                cancel = true;
            }
        }
        ImGui::PopStyleVar();

        if (!io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        {
            apply = true;
        }
    }
    ImGui::End();
    PopModalStyle();

    if (apply)
    {
        ApplyAdvFilters();
    }
    else if (cancel)
    {
        ui.advOpen = false;
    }
}
