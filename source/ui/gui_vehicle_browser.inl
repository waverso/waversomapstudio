// Source Repository: https://github.com/waverso/waversomapstudio

static bool VehicleBrowserFiltersPanelActive(const VehicleBrowserUiState& ui)
{
    const auto& f = ui.filter;
    const VehicleBrowser::Filter d;
    return f.categoryMask || f.shopMask || f.searchFields != d.searchFields || f.animationFilter != d.animationFilter
        || f.idMin != d.idMin || f.idMax != d.idMax || f.seatsMin != d.seatsMin || f.seatsMax != d.seatsMax
        || f.speedMin != d.speedMin || f.speedMax != d.speedMax || f.colorSlotsMask != d.colorSlotsMask
        || f.radiusMin != d.radiusMin || f.radiusMax != d.radiusMax || f.lengthMin != d.lengthMin || f.lengthMax != d.lengthMax
        || f.widthMin != d.widthMin || f.widthMax != d.widthMax || f.heightMin != d.heightMin || f.heightMax != d.heightMax;
}

static bool VehicleBrowserHasActiveFilters(const VehicleBrowserUiState& ui)
{
    return *ui.search || ui.favoritesOnly || ui.sortDesc || ui.sortKey != VehicleBrowser::SortId
        || VehicleBrowserFiltersPanelActive(ui);
}

static void ClearAllVehicleBrowserFilters(VehicleBrowserUiState& ui)
{
    ui.filter = {};
    ui.search[0] = '\0';
    ui.favoritesOnly = ui.sortDesc = false;
    ui.sortKey = VehicleBrowser::SortId;
    ui.page = 0;
    ui.dirty = true;
}

static void OpenVehicleFilters()
{
    g_vehicleBrowserUi.draft = g_vehicleBrowserUi.filter;
    g_vehicleBrowserUi.filtersOpen = true;
    ImGui::ClearActiveID();
}

static void DrawVehicleFiltersModal()
{
    auto& ui = g_vehicleBrowserUi;
    if (!ui.filtersOpen)
    {
        return;
    }
    auto& filter = ui.draft;
    static const VehicleBrowser::Filter filterBounds;
    DrawModalBackdrop("##VehicleFiltersBackdrop");
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = Ui::Px(520.0f);
    const float inner = width - Theme::g_outerPadding * 2;
    const float height = ModalHeight(11 * (Theme::g_frameHeight + Theme::g_padding) - Theme::g_padding);
    ImGui::SetNextWindowPos(ImVec2(display.x * .5f, display.y * .5f), ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    PushModalStyle();
    if (ImGui::Begin("##VehicleFilters", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        auto* dl = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetWindowPos();
        DrawModalTitleBar(dl, pos, width, "Vehicle Filters");
        float y = pos.y + Theme::g_tabHeight + Theme::g_padding;
        const float rowH = Theme::g_frameHeight + Theme::g_padding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawMaskFilter(
            "##veh_category", "Category", filter.categoryMask, VehicleBrowser::CategoryCount(),
            [](int i)
            {
                return VehicleCatalog::Categories[i];
            },
            inner, &ui.categorySearch);
        y += rowH;
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawMaskFilter(
            "##veh_shop", "Mod Shop", filter.shopMask, 4,
            [](int i)
            {
                return VehicleCatalog::ModificationShops[i];
            },
            inner);
        y += rowH;

        static const char* searchNames[] = { "Vehicle Name", "Model ID", "Model Name" };
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawMaskFilter(
            "##veh_search_fields", "Search In", filter.searchFields, 3,
            [](int i)
            {
                return searchNames[i];
            },
            inner, nullptr, VehicleBrowser::SearchName, "Vehicle Name", true);
        y += rowH;

        const float comboWidth = (inner - Theme::g_padding) * 0.5f;
        filter.animationFilter = static_cast<std::uint8_t>(ImClamp(static_cast<int>(filter.animationFilter), 0, 2));
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
        DrawVisibilityFilter("##veh_animation", "Model Anim", filter.animationFilter, comboWidth);
        static const char* const colorSlotNames[] = { "Any", "None", "1", "2" };
        constexpr unsigned allColorSlots = (1u << 3) - 1u;
        unsigned colorSlotMask = filter.colorSlotsMask & allColorSlots;
        if (colorSlotMask == allColorSlots)
        {
            colorSlotMask = 0;
        }
        filter.colorSlotsMask = static_cast<std::uint8_t>(colorSlotMask);
        char colorSlotParts[64] = {};
        if (!colorSlotMask)
        {
            snprintf(colorSlotParts, sizeof(colorSlotParts), "Any");
        }
        else
        {
            for (int slot = 0; slot < 3; ++slot)
            {
                if (colorSlotMask & (1u << slot))
                {
                    AppendLabelPart(colorSlotParts, sizeof(colorSlotParts), colorSlotNames[slot + 1]);
                }
            }
        }
        char colorSlotLabel[64];
        snprintf(colorSlotLabel, sizeof(colorSlotLabel), "Color Slots: %s", colorSlotParts);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding + comboWidth + Theme::g_padding, y));
        if (ImCustom::BeginCombo("##veh_color_slots", colorSlotLabel, comboWidth))
        {
            if (ImCustom::ComboMultiItem(colorSlotNames[0], !colorSlotMask))
            {
                colorSlotMask = 0;
            }
            for (int slot = 0; slot < 3; ++slot)
            {
                if (ImCustom::ComboMultiItem(colorSlotNames[slot + 1], (colorSlotMask & (1u << slot)) != 0))
                {
                    colorSlotMask ^= 1u << slot;
                    if (colorSlotMask == allColorSlots)
                    {
                        colorSlotMask = 0;
                    }
                }
            }
            filter.colorSlotsMask = static_cast<std::uint8_t>(colorSlotMask);
            ImCustom::EndCombo();
        }
        y += rowH;
        ImGui::PopStyleVar(3);
        int intIndex = 0, floatIndex = 0;
        auto rangeInt = [&](const char* label, int& low, int& high, int min, int max)
        {
            const int index = intIndex;
            intIndex += 2;
            ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
            RangeSliderInt(label, label, &low, &high, min, max, inner, ui.intBuf[index], ui.intBuf[index + 1], ui.intFocus[index],
                ui.intFocus[index + 1]);
            y += rowH;
        };
        auto rangeFloat = [&](const char* label, float& low, float& high, float min, float max)
        {
            const int index = floatIndex;
            floatIndex += 2;
            ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, y));
            RangeSliderFloat(label, label, &low, &high, min, max, inner, ui.floatBuf[index], ui.floatBuf[index + 1],
                ui.floatFocus[index], ui.floatFocus[index + 1], FloatRangeCurve);
            y += rowH;
        };
        rangeInt("Model ID", filter.idMin, filter.idMax, filterBounds.idMin, filterBounds.idMax);
        rangeInt("Seats", filter.seatsMin, filter.seatsMax, filterBounds.seatsMin, filterBounds.seatsMax);
        rangeInt("Max Speed (km/h)", filter.speedMin, filter.speedMax, filterBounds.speedMin, filterBounds.speedMax);
        rangeFloat("Radius (m)", filter.radiusMin, filter.radiusMax, filterBounds.radiusMin, filterBounds.radiusMax);
        rangeFloat("Length (m)", filter.lengthMin, filter.lengthMax, filterBounds.lengthMin, filterBounds.lengthMax);
        rangeFloat("Width (m)", filter.widthMin, filter.widthMax, filterBounds.widthMin, filterBounds.widthMax);
        rangeFloat("Height (m)", filter.heightMin, filter.heightMax, filterBounds.heightMin, filterBounds.heightMax);
        DrawModalFooter();
        const float buttonWidth = (inner - Theme::g_padding) * .5f;
        if (ImGui::Button("Apply Filters##veh_filters", ImVec2(buttonWidth, Theme::g_frameHeight))
            || (!ImGui::GetIO().WantTextInput
                && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))))
        {
            ui.filter = filter;
            ui.dirty = true;
            ui.page = 0;
            ui.filtersOpen = false;
        }
        ImGui::SameLine(0, Theme::g_padding);
        if (ImGui::Button("Cancel##veh_filters", ImVec2(buttonWidth, Theme::g_frameHeight)))
        {
            ui.filtersOpen = false;
        }
    }
    ImGui::End();
    PopModalStyle();
}

static bool AddVehicleToScene(int modelId)
{
    if (!VehicleCatalog::IsValid(modelId))
    {
        g_toast.ShowError("Invalid vehicle model ID.");
        return false;
    }
    if (!plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId))
    {
        plugin::Command<plugin::Commands::REQUEST_MODEL>(modelId);
        plugin::Command<plugin::Commands::LOAD_ALL_MODELS_NOW>();
    }

    float boundRadius = 2.0f;
    const VehicleBrowser::ModelInfo browserModel = VehicleBrowser::GetModel(modelId);
    if (browserModel.modelId >= 0 && browserModel.radius > 0.0f)
    {
        boundRadius = browserModel.radius;
    }
    else if (CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelId); mi && mi->m_pColModel)
    {
        boundRadius = mi->m_pColModel->m_boundSphere.m_fRadius;
    }

    char itemName[64];
    snprintf(itemName, sizeof(itemName), "%s", VehicleCatalog::Get(modelId)->name);
    EditorScene& sc = *g_scene;
    const int idx = sc.AppendItem(g_modal.contextParent, SceneItemNormal, itemName);
    if (idx < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }

    sc.InitItemNormal(idx, false);
    AddComponentToItem(sc.data[idx], ComponentTypeVehicle);
    sc.data[idx].vehicle.Edit().modelId = modelId;
    const float zeroRot[3] = { 0.0f, 0.0f, 0.0f };
    const CVector groundAnchor = SpawnGroundAnchor(modelId);
    const CVector spawnPos = g_modal.useMouseRay
        ? ComputeSpawnPosFromRay(g_modal.rayOrigin, g_modal.rayDir, boundRadius, groundAnchor)
        : ComputeCameraSpawnPos(boundRadius, groundAnchor);
    sc.SetWorldTransform(idx, spawnPos, zeroRot);
    FinalizeAddedItem(idx, g_modal.contextParent);
    return true;
}
static void DrawVehicleBrowser()
{
    VehicleBrowserUiState& ui = g_vehicleBrowserUi;
    if (ui.viewOnly)
    {
        bool selected = false, closed = false;
        DrawCatalogDetailModal<false>(ui, selected, closed);
        if (closed)
        {
            CloseVehicleBrowser();
        }
        return;
    }
    if (!ui.open)
    {
        return;
    }

    if (ui.dirty)
    {
        VehicleBrowser::Filter filter = ui.filter;
        strncpy_s(filter.search, sizeof(filter.search), ui.search, _TRUNCATE);
        filter.favoritesOnly = ui.favoritesOnly;
        filter.sortKey = ui.sortKey;
        filter.sortDesc = ui.sortDesc;
        VehicleBrowser::RunFilter(filter, ui.filtered);
        ui.dirty = false;
    }
    static const char* const sortNames[] = { "Model ID", "Name", "Max Speed", "Radius", "Length", "Width", "Height", "Seats" };
    bool cancel = false;
    const int selectedId = DrawBrowserCatalog(
        "##VehicleBrowser", "Select Vehicle Model", "Search", "model", ui, sortNames, VehicleBrowserFiltersPanelActive(ui),
        VehicleBrowserHasActiveFilters(ui), ui.detailModelId >= 0 || ui.filtersOpen, OpenVehicleFilters,
        ClearAllVehicleBrowserFilters, ModelThumbnails::PreparePage,
        [](std::uint32_t key)
        {
            const int id = static_cast<int>(key);
            const auto model = VehicleBrowser::GetModel(id);
            BrowserCell cell;
            cell.name = model.name;
            cell.favorite = VehicleBrowser::IsFavorite(id);
            cell.image = GetBrowserModelImage(id);
            snprintf(cell.subtitle, sizeof(cell.subtitle), "ID %d", id);
            snprintf(cell.tooltip, sizeof(cell.tooltip), "%s\n%s\nmax %d km/h\n%d seats", model.name,
                VehicleBrowser::CategoryName(model.category), model.maxSpeed, model.seats);
            return cell;
        },
        [](std::uint32_t id)
        {
            VehicleBrowser::ToggleFavorite(static_cast<int>(id));
        },
        cancel);
    if (selectedId >= 0)
    {
        ui.detailModelId = selectedId;
        ModelPreview::Open(selectedId);
    }
    DrawVehicleFiltersModal();
    bool selected = false, closed = false;
    DrawCatalogDetailModal<false>(ui, selected, closed);
    bool confirmed = false;
    if (selected)
    {
        if (ui.changeItemId >= 0 && InspectionConfirmModel(ui.detailModelId, ComponentTypeVehicle))
        {
            confirmed = true;
        }
        else if (ui.changeItemId >= 0)
        {
            const int ci = g_scene->FindIndexById(ui.changeItemId);
            if (ci >= 0 && ci < g_scene->count
                && (g_scene->data[ci].vehicle.HasValue()
                    || CanAddComponentType(g_scene->items[ci], g_scene->data[ci], ComponentTypeVehicle)))
            {
                if (!g_scene->data[ci].vehicle.HasValue())
                {
                    GuiAddComponent(ci, ComponentTypeVehicle);
                }
                VehicleData& model = g_scene->data[ci].vehicle.Edit();
                model.modelId = ui.detailModelId;
                ItemWidgetState& w = g_itemWidgets[ci];
                w.modelHasFocus = false;
                snprintf(w.modelBuf, sizeof(w.modelBuf), "%d", model.modelId);
                confirmed = true;
            }
            else
            {
                g_toast.ShowError("The item being edited no longer exists.");
                confirmed = true;
            }
        }
        else
        {
            confirmed = AddVehicleToScene(ui.detailModelId);
        }
    }
    if (confirmed)
    {
        CloseVehicleBrowser();
        g_modal.activeType = PendingModalNone;
    }
    else if (closed)
    {
        if (ui.viewOnly)
        {
            CloseVehicleBrowser();
        }
        else
        {
            ModelPreview::Close();
            ui.detailModelId = -1;
        }
    }
    else if (cancel)
    {
        CloseVehicleBrowser();
    }
}
static void DrawVehicleComponentInfoCard(int modelId, float innerW)
{
    const auto model = VehicleBrowser::GetModel(modelId);
    const bool valid = model.modelId >= 0;
    char speed[32], seats[16];
    if (valid)
    {
        snprintf(speed, sizeof(speed), "%d km/h", model.maxSpeed);
        snprintf(seats, sizeof(seats), "%d", model.seats);
    }
    else
    {
        snprintf(speed, sizeof(speed), "Unknown");
        snprintf(seats, sizeof(seats), "Unknown");
    }
    const ComponentInfoRow rows[3] = { { "Category", valid ? VehicleBrowser::CategoryName(model.category) : "Unknown" },
        { "Max Speed", speed }, { "Seats", seats } };
    const ComponentInfoChip animationChip[] = { { "Model Anim", Theme::g_chipAnimation } };
    const bool favorite = VehicleBrowser::IsFavorite(modelId);
    const auto result = DrawComponentInfoCard(
        "##vehicle_info", innerW, valid ? model.name : "Unknown", model.animation ? animationChip : nullptr,
        model.animation ? 1 : 0, rows,
        [modelId]
        {
            return GetComponentModelImage(modelId);
        },
        g_icons ? g_icons->heartTexture : nullptr, favorite ? Theme::g_chipFavorited : Theme::g_textDim, valid, valid);
    if (result.action)
    {
        VehicleBrowser::ToggleFavorite(modelId);
        g_vehicleBrowserUi.dirty = true;
    }
    if (result.details)
    {
        OpenVehicleDetails(modelId);
    }
}
