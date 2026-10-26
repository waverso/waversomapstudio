// Source Repository: https://github.com/waverso/waversomapstudio

static bool SkinBrowserFiltersPanelActive(const SkinBrowserUiState& ui)
{
    const auto& f = ui.filter;
    const SkinBrowser::Filter defaults;
    return f.categoryMask || f.genderMask || f.searchFields != SkinBrowser::SearchDefault || f.idMin != defaults.idMin
        || f.idMax != defaults.idMax || f.radiusMin != defaults.radiusMin || f.radiusMax != defaults.radiusMax
        || f.heightMin != defaults.heightMin || f.heightMax != defaults.heightMax;
}

static void ClearAllSkinBrowserFilters(SkinBrowserUiState& ui)
{
    ui.filter = {};
    ui.search[0] = '\0';
    ui.favoritesOnly = ui.sortDesc = false;
    ui.sortKey = SkinBrowser::SortId;
    ui.page = 0;
    ui.dirty = true;
}

static void OpenSkinFilters()
{
    auto& ui = g_skinBrowserUi;
    ui.draft = ui.filter;
    ui.filtersOpen = true;
    ImGui::ClearActiveID();
}

static void DrawSkinFiltersModal()
{
    auto& ui = g_skinBrowserUi;
    if (!ui.filtersOpen)
    {
        return;
    }
    auto& filter = ui.draft;
    DrawModalBackdrop("##SkinFiltersBackdrop");
    const auto display = ImGui::GetIO().DisplaySize;
    const float width = Ui::Px(520.0f), inner = width - Theme::g_outerPadding * 2;
    ImGui::SetNextWindowPos(ImVec2(display.x * .5f, display.y * .5f), ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(
        ImVec2(width, ModalHeight(6 * (Theme::g_frameHeight + Theme::g_padding) - Theme::g_padding)), ImGuiCond_Always);
    PushModalStyle();
    if (ImGui::Begin("##SkinFilters", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        auto* dl = ImGui::GetWindowDrawList();
        const auto pos = ImGui::GetWindowPos();
        DrawModalTitleBar(dl, pos, width, "Skin Filters");
        float y = pos.y + Theme::g_tabHeight + Theme::g_padding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawMaskFilter(
            "Category", "Category", filter.categoryMask, IM_ARRAYSIZE(SkinCatalog::Categories),
            [](int i)
            {
                return SkinCatalog::Categories[i];
            },
            inner);
        y += Theme::g_frameHeight + Theme::g_padding;
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawMaskFilter(
            "Gender", "Gender", filter.genderMask, 2,
            [](int i)
            {
                return SkinCatalog::Genders[i];
            },
            inner);
        y += Theme::g_frameHeight + Theme::g_padding;

        static const char* searchNames[] = { "Skin Name", "Model ID", "Model Name" };
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawMaskFilter(
            "Search In", "Search In", filter.searchFields, 3,
            [](int i)
            {
                return searchNames[i];
            },
            inner, nullptr, SkinBrowser::SearchName);
        y += Theme::g_frameHeight + Theme::g_padding;
        ImGui::PopStyleVar(2);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        RangeSliderInt("##skin_id_range", "Model ID", &filter.idMin, &filter.idMax, 0, 311, inner, ui.intBuf[0], ui.intBuf[1],
            ui.intFocus[0], ui.intFocus[1]);
        const SkinBrowser::Filter limits;
        auto rangeFloat = [&](const char* id, const char* label, float& low, float& high, float minimum, float maximum, int index)
        {
            y += Theme::g_frameHeight + Theme::g_padding;
            ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
            RangeSliderFloat(id, label, &low, &high, minimum, maximum, inner, ui.floatBuf[index], ui.floatBuf[index + 1],
                ui.floatFocus[index], ui.floatFocus[index + 1], FloatRangeCurve);
        };
        rangeFloat(
            "##skin_radius_range", "Radius (m)", filter.radiusMin, filter.radiusMax, limits.radiusMin, limits.radiusMax, 0);
        rangeFloat(
            "##skin_height_range", "Height (m)", filter.heightMin, filter.heightMax, limits.heightMin, limits.heightMax, 2);
        DrawModalFooter();
        const float bw = (inner - Theme::g_padding) * .5f;
        if (ImGui::Button("Apply Filters##skin", ImVec2(bw, Theme::g_frameHeight))
            || (!ImGui::GetIO().WantTextInput
                && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))))
        {
            ui.filter = filter;
            ui.dirty = true;
            ui.page = 0;
            ui.filtersOpen = false;
        }
        ImGui::SameLine(0, Theme::g_padding);
        if (ImGui::Button("Cancel##skin", ImVec2(bw, Theme::g_frameHeight)))
        {
            ui.filtersOpen = false;
        }
    }
    ImGui::End();
    PopModalStyle();
}

static bool AddActorToScene(int skinId)
{
    if (!SkinCatalog::IsValid(skinId))
    {
        g_toast.ShowError("Skin ID is not available.");
        return false;
    }
    SkinInstance::Request(skinId, eStreamingFlags::PRIORITY_REQUEST);
    CStreaming::LoadAllRequestedModels(true);
    const auto model = SkinBrowser::GetModel(skinId);
    if (model.height <= 0.0f)
    {
        g_toast.ShowError("Could not load the skin idle pose.");
        return false;
    }
    EditorScene& scene = *g_scene;
    const int index = scene.AppendItem(g_modal.contextParent, SceneItemNormal, model.name);
    if (index < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }
    scene.InitItemNormal(index, false);
    AddComponentToItem(scene.data[index], ComponentTypeActor);
    scene.data[index].actor.Edit().skinId = skinId;
    scene.items[index].searchModelId = skinId;
    const CVector anchor(
        (model.boundsMin[0] + model.boundsMax[0]) * .5f, (model.boundsMin[1] + model.boundsMax[1]) * .5f, model.boundsMin[2]);
    const CVector position = g_modal.useMouseRay ? ComputeSpawnPosFromRay(g_modal.rayOrigin, g_modal.rayDir, model.radius, anchor)
                                                 : ComputeCameraSpawnPos(model.radius, anchor);
    const float rotation[3] = {};
    scene.SetWorldTransform(index, position, rotation);
    FinalizeAddedItem(index, g_modal.contextParent);
    return true;
}

static void DrawSkinBrowser()
{
    auto& ui = g_skinBrowserUi;
    if (ui.viewOnly)
    {
        bool selected = false, closed = false;
        DrawCatalogDetailModal<true>(ui, selected, closed);
        if (closed)
        {
            CloseSkinBrowser();
        }
        return;
    }
    if (!ui.open)
    {
        return;
    }
    if (ui.dirty)
    {
        auto filter = ui.filter;
        strncpy_s(filter.search, sizeof(filter.search), ui.search, _TRUNCATE);
        filter.favoritesOnly = ui.favoritesOnly;
        filter.sortKey = ui.sortKey;
        filter.sortDesc = ui.sortDesc;
        SkinBrowser::RunFilter(filter, ui.filtered);
        ui.dirty = false;
    }
    static const char* const sortNames[] = { "Model ID", "Name", "Radius", "Length", "Width", "Height" };
    bool cancel = false;
    const bool filters = SkinBrowserFiltersPanelActive(ui);
    const int id = DrawBrowserCatalog(
        "##SkinBrowser", "Select Skin Model", "Search", "skin", ui, sortNames, filters,
        filters || *ui.search || ui.favoritesOnly || ui.sortDesc || ui.sortKey, ui.detailModelId >= 0 || ui.filtersOpen,
        OpenSkinFilters, ClearAllSkinBrowserFilters, ModelThumbnails::PreparePage,
        [](std::uint32_t key)
        {
            const int id = static_cast<int>(key);
            const auto model = SkinBrowser::GetModel(id);
            BrowserCell cell;
            cell.name = model.name;
            cell.favorite = SkinBrowser::IsFavorite(id);
            cell.image = GetBrowserModelImage(id);
            snprintf(cell.subtitle, sizeof(cell.subtitle), "ID %d", id);
            snprintf(cell.tooltip, sizeof(cell.tooltip), "%s\n%s\n%s", model.name, SkinCatalog::Categories[model.category],
                SkinCatalog::Genders[model.gender]);
            return cell;
        },
        [](std::uint32_t id)
        {
            SkinBrowser::ToggleFavorite(static_cast<int>(id));
        },
        cancel);
    if (id >= 0)
    {
        ui.detailModelId = id;
        ModelPreview::Open(id, true);
    }
    DrawSkinFiltersModal();
    bool selected = false, closed = false, confirmed = false;
    DrawCatalogDetailModal<true>(ui, selected, closed);
    if (selected)
    {
        if (ui.changeItemId >= 0 && InspectionConfirmModel(ui.detailModelId, ComponentTypeActor))
        {
            confirmed = true;
        }
        else if (ui.changeItemId >= 0)
        {
            const int index = g_scene->FindIndexById(ui.changeItemId);
            if (index >= 0
                && (g_scene->data[index].actor.HasValue()
                    || CanAddComponentType(g_scene->items[index], g_scene->data[index], ComponentTypeActor)))
            {
                if (!g_scene->data[index].actor.HasValue())
                {
                    GuiAddComponent(index, ComponentTypeActor);
                }
                g_scene->data[index].actor.Edit().skinId = ui.detailModelId;
                auto& widgets = g_itemWidgets[index];
                widgets.modelHasFocus = false;
                snprintf(widgets.modelBuf, sizeof(widgets.modelBuf), "%d", ui.detailModelId);
            }
            else
            {
                g_toast.ShowError("The item cannot accept an Actor component.");
            }
            confirmed = true;
        }
        else
        {
            confirmed = AddActorToScene(ui.detailModelId);
        }
    }
    if (confirmed)
    {
        CloseSkinBrowser();
        g_modal.activeType = PendingModalNone;
    }
    else if (closed)
    {
        ModelPreview::Close();
        ui.detailModelId = -1;
    }
    else if (cancel)
    {
        CloseSkinBrowser();
    }
}

static void DrawActorComponentInfoCard(int skinId, float innerW)
{
    const auto model = SkinBrowser::GetModel(skinId);
    char height[32];
    snprintf(height, sizeof(height), "%.2f m", model.height);
    const ComponentInfoRow rows[3] = { { "Category", SkinCatalog::Categories[model.category] },
        { "Gender", SkinCatalog::Genders[model.gender] }, { "Height", height } };
    const bool favorite = SkinBrowser::IsFavorite(skinId);
    const auto result = DrawComponentInfoCard(
        "##actor_info", innerW, model.name, nullptr, 0, rows,
        [skinId]
        {
            return GetComponentModelImage(skinId);
        },
        g_icons ? g_icons->heartTexture : nullptr, favorite ? Theme::g_chipFavorited : Theme::g_textDim, model.modelId >= 0,
        model.modelId >= 0);
    if (result.action)
    {
        SkinBrowser::ToggleFavorite(skinId);
        g_skinBrowserUi.dirty = true;
    }
    if (result.details)
    {
        OpenSkinDetails(skinId);
    }
}
