// Source Repository: https://github.com/waverso/waversomapstudio

static void ClearAllObjectBrowserFilters(ObjectBrowserUiState& ui)
{
    ui.search[0] = '\0';
    ui.favoritesOnly = false;
    ui.lodFilter = BrowserFilter::Hide;
    ui.breakableFilter = BrowserFilter::Any;
    ui.collidableFilter = BrowserFilter::Any;
    ui.animationFilter = BrowserFilter::Any;
    ui.uvAnimFilter = BrowserFilter::Any;
    ui.backfaceFilter = BrowserFilter::Any;
    ui.timeFilter = BrowserFilter::Any;
    ui.physicsFilter = BrowserFilter::Any;
    ui.explosiveFilter = BrowserFilter::Any;
    ui.searchFields = ObjectBrowser::SearchDefault;
    ui.sourceMask = 0;
    ui.categoryMask = 0;
    ui.idMin = ui.idMax = -1;
    ui.dffSizeMinKb = ui.dffSizeMaxKb = -1;
    ui.radiusMin = ui.radiusMax = -1.0f;
    ui.lengthMin = ui.lengthMax = -1.0f;
    ui.widthMin = ui.widthMax = -1.0f;
    ui.heightMin = ui.heightMax = -1.0f;
    ui.drawDistanceMin = ui.drawDistanceMax = -1;
    ui.polyCountMin = ui.polyCountMax = -1;
    ui.sortKey = ObjectBrowser::SortId;
    ui.sortDesc = false;
    ui.dirty = true;
    ui.page = 0;
}

static bool ObjectBrowserFiltersPanelActive(const ObjectBrowserUiState& ui)
{
    return ui.lodFilter != BrowserFilter::Hide || ui.breakableFilter != BrowserFilter::Any
        || ui.collidableFilter != BrowserFilter::Any || ui.animationFilter != BrowserFilter::Any
        || ui.uvAnimFilter != BrowserFilter::Any || ui.backfaceFilter != BrowserFilter::Any || ui.timeFilter != BrowserFilter::Any
        || ui.physicsFilter != BrowserFilter::Any || ui.explosiveFilter != BrowserFilter::Any
        || ui.searchFields != ObjectBrowser::SearchDefault || ui.sourceMask != 0 || ui.categoryMask != 0 || ui.idMin >= 0
        || ui.idMax >= 0 || ui.dffSizeMinKb >= 0 || ui.dffSizeMaxKb >= 0 || ui.radiusMin >= 0.0f || ui.radiusMax >= 0.0f
        || ui.lengthMin >= 0.0f || ui.lengthMax >= 0.0f || ui.widthMin >= 0.0f || ui.widthMax >= 0.0f || ui.heightMin >= 0.0f
        || ui.heightMax >= 0.0f || ui.drawDistanceMin >= 0 || ui.drawDistanceMax >= 0 || ui.polyCountMin >= 0
        || ui.polyCountMax >= 0;
}

static bool ObjectBrowserHasActiveFilters(const ObjectBrowserUiState& ui)
{
    return ui.search[0] || ui.favoritesOnly || ObjectBrowserFiltersPanelActive(ui) || ui.sortKey != ObjectBrowser::SortId
        || ui.sortDesc;
}

static ObjectBrowser::FilterLimits GetObjectFilterLimits()
{
    auto limits = ObjectBrowser::GetFilterLimits();
    NormalizeIntLimits(limits.dffSizeMinKb, limits.dffSizeMaxKb);
    NormalizeFloatLimits(limits.radiusMin, limits.radiusMax);
    NormalizeFloatLimits(limits.lengthMin, limits.lengthMax);
    NormalizeFloatLimits(limits.widthMin, limits.widthMax);
    NormalizeFloatLimits(limits.heightMin, limits.heightMax);
    NormalizeIntLimits(limits.drawDistanceMin, limits.drawDistanceMax);
    NormalizeIntLimits(limits.polyCountMin, limits.polyCountMax);
    if (limits.idMax <= limits.idMin)
    {
        limits.idMin = 321;
        limits.idMax = 19999;
    }
    return limits;
}

static void OpenObjectFilters()
{
    ObjectBrowserUiState& ui = g_objectBrowserUi;
    const auto limits = GetObjectFilterLimits();
    ui.advLodFilter = ui.lodFilter;
    ui.advBreakableFilter = ui.breakableFilter;
    ui.advCollidableFilter = ui.collidableFilter;
    ui.advAnimationFilter = ui.animationFilter;
    ui.advUVAnimFilter = ui.uvAnimFilter;
    ui.advBackfaceFilter = ui.backfaceFilter;
    ui.advTimeFilter = ui.timeFilter;
    ui.advPhysicsFilter = ui.physicsFilter;
    ui.advExplosiveFilter = ui.explosiveFilter;
    ui.advSearchFields = ui.searchFields ? ui.searchFields : ObjectBrowser::SearchName;
    ui.advSourceMask = ui.sourceMask;
    ui.advCategoryMask = ui.categoryMask;
    ui.advId[0] = ui.idMin < 0 ? limits.idMin : ImClamp(ui.idMin, limits.idMin, limits.idMax);
    ui.advId[1] = ui.idMax < 0 ? limits.idMax : ImClamp(ui.idMax, ui.advId[0], limits.idMax);
    ui.advDffSizeKb[0]
        = ui.dffSizeMinKb < 0 ? limits.dffSizeMinKb : ImClamp(ui.dffSizeMinKb, limits.dffSizeMinKb, limits.dffSizeMaxKb);
    ui.advDffSizeKb[1]
        = ui.dffSizeMaxKb < 0 ? limits.dffSizeMaxKb : ImClamp(ui.dffSizeMaxKb, ui.advDffSizeKb[0], limits.dffSizeMaxKb);
    ui.advRadius[0] = ui.radiusMin < 0.0f ? limits.radiusMin : ImClamp(ui.radiusMin, limits.radiusMin, limits.radiusMax);
    ui.advRadius[1] = ui.radiusMax < 0.0f ? limits.radiusMax : ImClamp(ui.radiusMax, ui.advRadius[0], limits.radiusMax);
    ui.advLength[0] = ui.lengthMin < 0.0f ? limits.lengthMin : ImClamp(ui.lengthMin, limits.lengthMin, limits.lengthMax);
    ui.advLength[1] = ui.lengthMax < 0.0f ? limits.lengthMax : ImClamp(ui.lengthMax, ui.advLength[0], limits.lengthMax);
    ui.advWidth[0] = ui.widthMin < 0.0f ? limits.widthMin : ImClamp(ui.widthMin, limits.widthMin, limits.widthMax);
    ui.advWidth[1] = ui.widthMax < 0.0f ? limits.widthMax : ImClamp(ui.widthMax, ui.advWidth[0], limits.widthMax);
    ui.advHeight[0] = ui.heightMin < 0.0f ? limits.heightMin : ImClamp(ui.heightMin, limits.heightMin, limits.heightMax);
    ui.advHeight[1] = ui.heightMax < 0.0f ? limits.heightMax : ImClamp(ui.heightMax, ui.advHeight[0], limits.heightMax);
    ui.advDrawDistance[0] = ui.drawDistanceMin < 0 ? limits.drawDistanceMin
                                                   : ImClamp(ui.drawDistanceMin, limits.drawDistanceMin, limits.drawDistanceMax);
    ui.advDrawDistance[1] = ui.drawDistanceMax < 0 ? limits.drawDistanceMax
                                                   : ImClamp(ui.drawDistanceMax, ui.advDrawDistance[0], limits.drawDistanceMax);
    ui.advPolyCount[0]
        = ui.polyCountMin < 0 ? limits.polyCountMin : ImClamp(ui.polyCountMin, limits.polyCountMin, limits.polyCountMax);
    ui.advPolyCount[1]
        = ui.polyCountMax < 0 ? limits.polyCountMax : ImClamp(ui.polyCountMax, ui.advPolyCount[0], limits.polyCountMax);
    ui.filtersOpen = true;
}

static void ApplyObjectFilters()
{
    ObjectBrowserUiState& ui = g_objectBrowserUi;
    const auto limits = GetObjectFilterLimits();
    ui.lodFilter = ui.advLodFilter;
    ui.breakableFilter = ui.advBreakableFilter;
    ui.collidableFilter = ui.advCollidableFilter;
    ui.animationFilter = ui.advAnimationFilter;
    ui.uvAnimFilter = ui.advUVAnimFilter;
    ui.backfaceFilter = ui.advBackfaceFilter;
    ui.timeFilter = ui.advTimeFilter;
    ui.physicsFilter = ui.advPhysicsFilter;
    ui.explosiveFilter = ui.advExplosiveFilter;
    ui.searchFields = ui.advSearchFields ? ui.advSearchFields : ObjectBrowser::SearchName;
    ui.sourceMask = ui.advSourceMask;
    ui.categoryMask = ui.advCategoryMask;
    ui.idMin = ui.advId[0] <= limits.idMin ? -1 : ui.advId[0];
    ui.idMax = ui.advId[1] >= limits.idMax ? -1 : ui.advId[1];
    ui.dffSizeMinKb = ui.advDffSizeKb[0] <= limits.dffSizeMinKb ? -1 : ui.advDffSizeKb[0];
    ui.dffSizeMaxKb = ui.advDffSizeKb[1] >= limits.dffSizeMaxKb ? -1 : ui.advDffSizeKb[1];
    ui.radiusMin = ui.advRadius[0] <= limits.radiusMin ? -1.0f : ui.advRadius[0];
    ui.radiusMax = ui.advRadius[1] >= limits.radiusMax ? -1.0f : ui.advRadius[1];
    ui.lengthMin = ui.advLength[0] <= limits.lengthMin ? -1.0f : ui.advLength[0];
    ui.lengthMax = ui.advLength[1] >= limits.lengthMax ? -1.0f : ui.advLength[1];
    ui.widthMin = ui.advWidth[0] <= limits.widthMin ? -1.0f : ui.advWidth[0];
    ui.widthMax = ui.advWidth[1] >= limits.widthMax ? -1.0f : ui.advWidth[1];
    ui.heightMin = ui.advHeight[0] <= limits.heightMin ? -1.0f : ui.advHeight[0];
    ui.heightMax = ui.advHeight[1] >= limits.heightMax ? -1.0f : ui.advHeight[1];
    ui.drawDistanceMin = ui.advDrawDistance[0] <= limits.drawDistanceMin ? -1 : ui.advDrawDistance[0];
    ui.drawDistanceMax = ui.advDrawDistance[1] >= limits.drawDistanceMax ? -1 : ui.advDrawDistance[1];
    ui.polyCountMin = ui.advPolyCount[0] <= limits.polyCountMin ? -1 : ui.advPolyCount[0];
    ui.polyCountMax = ui.advPolyCount[1] >= limits.polyCountMax ? -1 : ui.advPolyCount[1];
    ui.filtersOpen = false;
    ui.dirty = true;
    ui.page = 0;
}

static void DrawObjectFiltersModal()
{
    ObjectBrowserUiState& ui = g_objectBrowserUi;
    if (!ui.filtersOpen)
    {
        return;
    }

    const auto limits = GetObjectFilterLimits();
    const ImGuiIO& io = ImGui::GetIO();
    DrawModalBackdrop("##ObjectFiltersBackdrop");
    const float winW = Ui::Px(520.0f);
    const float rowH = Theme::g_frameHeight + Theme::g_padding;
    const float winH = ModalHeight(rowH * 14.0f - Theme::g_padding);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(winW, ceilf(winH)), ImGuiCond_Always);
    PushModalStyle();
    bool apply = false, cancel = false;

    if (ImGui::Begin("##ObjectFilters", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 winPos = ImGui::GetWindowPos();
        DrawModalTitleBar(dl, winPos, winW, "Object Filters");
        const float padX = Theme::g_outerPadding;
        const float sliderX = winPos.x + padX;
        const float sliderW = winW - padX * 2.0f;
        const float fullW = winW - padX * 2.0f;
        float y = winPos.y + Theme::g_tabHeight + Theme::g_padding;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        {
            const float cgap = Theme::g_padding;
            const float cW = (fullW - cgap * 2.0f) / 3.0f;
            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter("##obj_adv_source", "Source", ui.advSourceMask, CatalogDatabase::ArchiveCount,
                TextureBrowser::SourceImgName, fullW);
            y += rowH;

            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter("##obj_adv_category", "Category", ui.advCategoryMask, ImMin(32, ObjectBrowser::CategoryCount()),
                ObjectBrowser::CategoryName, fullW, &ui.categorySearch);
            y += rowH;

            static const char* searchNames[] = { "Model Name", "Model ID", "Description" };
            ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, y));
            DrawMaskFilter(
                "##obj_adv_search", "Search In", ui.advSearchFields, 3,
                [](int i)
                {
                    return searchNames[i];
                },
                fullW, nullptr, ObjectBrowser::SearchName, "Model Name", true);
            y += rowH;

            auto visibilityCombo = [&](const char* id, const char* title, std::uint8_t& value, int column)
            {
                value = static_cast<std::uint8_t>(ImClamp(static_cast<int>(value), 0, 2));
                ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX + column * (cW + cgap), y));
                DrawVisibilityFilter(id, title, value, cW);
            };

            visibilityCombo("##obj_adv_lods", "Low LODs", ui.advLodFilter, 0);
            visibilityCombo("##obj_adv_breakable", "Breakable", ui.advBreakableFilter, 1);
            visibilityCombo("##obj_adv_collidable", "Collidable", ui.advCollidableFilter, 2);
            y += rowH;

            visibilityCombo("##obj_adv_animation", "Model Anim", ui.advAnimationFilter, 0);
            visibilityCombo("##obj_adv_uvanim", "UV Anim", ui.advUVAnimFilter, 1);
            visibilityCombo("##obj_adv_backface", "Two-Sided", ui.advBackfaceFilter, 2);
            y += rowH;

            visibilityCombo("##obj_adv_physics", "Physics", ui.advPhysicsFilter, 0);
            visibilityCombo("##obj_adv_explosive", "Explosive", ui.advExplosiveFilter, 1);
            visibilityCombo("##obj_adv_time", "Time", ui.advTimeFilter, 2);
            y += rowH;
        }
        ImGui::PopStyleVar(3);

        auto intRow = [&](const char* rowLabel, const char* id, int* values, int lo, int hi, int bufferIndex)
        {
            ImGui::SetCursorScreenPos(ImVec2(sliderX, y));
            RangeSliderInt(id, rowLabel, &values[0], &values[1], lo, hi, sliderW, ui.advIntBuf[bufferIndex],
                ui.advIntBuf[bufferIndex + 1], ui.advIntFocus[bufferIndex], ui.advIntFocus[bufferIndex + 1]);
            y += rowH;
        };
        auto floatRow = [&](const char* rowLabel, const char* id, float* values, float lo, float hi, int bufferIndex)
        {
            ImGui::SetCursorScreenPos(ImVec2(sliderX, y));
            RangeSliderFloat(id, rowLabel, &values[0], &values[1], lo, hi, sliderW, ui.advFloatBuf[bufferIndex],
                ui.advFloatBuf[bufferIndex + 1], ui.advFloatFocus[bufferIndex], ui.advFloatFocus[bufferIndex + 1],
                FloatRangeCurve);
            y += rowH;
        };
        intRow("Model ID", "##obj_range_id", ui.advId, limits.idMin, limits.idMax, 0);
        intRow("DFF Size (KB)", "##obj_range_dff", ui.advDffSizeKb, limits.dffSizeMinKb, limits.dffSizeMaxKb, 2);
        floatRow("Radius (m)", "##obj_range_radius", ui.advRadius, limits.radiusMin, limits.radiusMax, 2);
        floatRow("Length (m)", "##obj_range_length", ui.advLength, limits.lengthMin, limits.lengthMax, 4);
        floatRow("Width (m)", "##obj_range_width", ui.advWidth, limits.widthMin, limits.widthMax, 6);
        floatRow("Height (m)", "##obj_range_height", ui.advHeight, limits.heightMin, limits.heightMax, 8);
        intRow("Draw Dist", "##obj_range_draw", ui.advDrawDistance, limits.drawDistanceMin, limits.drawDistanceMax, 6);
        intRow("Polygons", "##obj_range_poly", ui.advPolyCount, limits.polyCountMin, limits.polyCountMax, 8);

        const float btnY = DrawModalFooter();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        const float bw = (winW - padX * 2.0f - Theme::g_padding) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, btnY));
        if (ImGui::Button("Apply Filters##object", ImVec2(bw, Theme::g_frameHeight)))
        {
            apply = true;
        }
        ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX + bw + Theme::g_padding, btnY));
        if (ImGui::Button("Cancel##object", ImVec2(bw, Theme::g_frameHeight)))
        {
            cancel = true;
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
        ApplyObjectFilters();
    }
    else if (cancel)
    {
        ui.filtersOpen = false;
    }
}
