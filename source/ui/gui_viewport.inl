// Source Repository: https://github.com/waverso/waversomapstudio

void Gui::UpdateUtilityModifiers()
{
    UtilityPanelState& state = g_utilityPanelState;

    const bool weatherDesired = state.weatherEnabled;
    if (weatherDesired)
    {
        const short incomingWeather = CWeather::NewWeatherType;
        const short desiredWeather = static_cast<short>(state.weatherValue);
        if (incomingWeather != desiredWeather)
        {
            if (incomingWeather != state.lastWeatherToSet)
            {
                state.lastWeatherForcedByServer = incomingWeather;
            }
            CWeather::ForceWeatherNow(desiredWeather);
        }
        state.lastWeatherToSet = desiredWeather;
    }
    else if (state.weatherApplied)
    {
        state.lastWeatherForcedByReset = state.lastWeatherForcedByServer;
        CWeather::ForceWeatherNow(state.lastWeatherForcedByServer);
    }
    else if (CWeather::NewWeatherType != state.lastWeatherForcedByReset)
    {
        state.lastWeatherForcedByServer = CWeather::NewWeatherType;
    }
    state.weatherApplied = weatherDesired;

    if (g_gameClockController)
    {
        g_gameClockController->SetOverride(state.timeEnabled, state.timeValue);
        g_gameClockController->UpdateFrameBoundary();
    }
}

bool Gui::IsMouseOverGizmoPanel() const
{
    if (!m_initialized || !g_freeCamera || !g_freeCamera->m_active || !ImGui::GetCurrentContext() || !RsGlobal.ps)
    {
        return false;
    }

    POINT cursor;
    if (!GetCursorPos(&cursor) || !ScreenToClient(RsGlobal.ps->window, &cursor))
    {
        return false;
    }

    const ImRect panelRect = g_gizmoPanelLayout.Rect(ImGui::GetIO().DisplaySize);
    return cursor.x >= panelRect.Min.x && cursor.x < panelRect.Max.x && cursor.y >= panelRect.Min.y && cursor.y < panelRect.Max.y;
}

bool Gui::IsMouseOverUtilityPanel() const
{
    if (!m_initialized || !g_freeCamera || !g_freeCamera->m_active || !ImGui::GetCurrentContext() || !RsGlobal.ps)
    {
        return false;
    }

    POINT cursor;
    if (!GetCursorPos(&cursor) || !ScreenToClient(RsGlobal.ps->window, &cursor))
    {
        return false;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImRect panelRect = g_utilityPanelLayout.Rect(display, display.x * GetSidebarVisualWidthRatio());
    return cursor.x >= panelRect.Min.x && cursor.x < panelRect.Max.x && cursor.y >= panelRect.Min.y && cursor.y < panelRect.Max.y;
}

void Gui::DrawGizmoPanel()
{
    if (!g_gizmos || !g_icons)
    {
        return;
    }

    const GizmoPanelLayout& layout = g_gizmoPanelLayout;
    const ImRect panelRect = layout.Rect(ImGui::GetIO().DisplaySize);
    const ImVec2 panelSize(panelRect.GetWidth(), panelRect.GetHeight());

    ImGui::SetNextWindowPos(panelRect.Min, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::g_panelBackground);
    ImGui::Begin("##GizmoPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 panelMax = wp + ImGui::GetWindowSize();
    dl->AddRect(wp, panelMax, Theme::g_border, 0.0f, 0, Ui::Hairline());
    dl->AddRectFilled(wp, ImVec2(panelMax.x, wp.y + Theme::g_accentSize), Theme::g_accent);

    const float contentY = Theme::g_accentSize + Theme::g_outerPadding;
    static GizmoGridEditState translationEdit;
    static GizmoGridEditState rotationEdit;

    ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, contentY));
    float translationGrid = g_gizmos->GetTranslationGridSnap();
    if (GizmoGridDragFloat("##PositionGrid", &translationGrid, 0.1f, 0.0f, 1000.0f, layout.gridWidth,
            g_icons->gridPositionTexture, Theme::g_iconSize, "Position Grid Snap (Ctrl)", translationEdit))
    {
        g_gizmos->SetTranslationGridSnap(translationGrid);
    }
    const bool translationDeactivated = ImGui::IsItemDeactivated();
    CommitGizmoGridEdit(GizmoGridSnap::Translation, translationEdit, translationGrid, ImGui::IsItemDeactivatedAfterEdit());
    if (translationDeactivated)
    {
        translationEdit.active = false;
    }

    ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, contentY + Theme::g_frameHeight + layout.rowGap));
    float rotationGrid = g_gizmos->GetRotationGridSnap();
    if (GizmoGridDragFloat("##RotationGrid", &rotationGrid, 0.2f, 0.0f, 180.0f, layout.gridWidth, g_icons->gridRotationTexture,
            Theme::g_iconSize, "Rotation Snap (Ctrl)", rotationEdit))
    {
        g_gizmos->SetRotationGridSnap(rotationGrid);
    }
    const bool rotationDeactivated = ImGui::IsItemDeactivated();
    CommitGizmoGridEdit(GizmoGridSnap::Rotation, rotationEdit, rotationGrid, ImGui::IsItemDeactivatedAfterEdit());
    if (rotationDeactivated)
    {
        rotationEdit.active = false;
    }

    const float modesX = Theme::g_outerPadding + layout.gridWidth + layout.sectionGap;
    ImGui::SetCursorPos(ImVec2(modesX, contentY));
    const ImVec2 modesMin = ImGui::GetCursorScreenPos();
    const ImVec2 modesMax(modesMin.x + layout.ModeWidth(), modesMin.y + layout.ModeHeight());
    const float modeSize = layout.ModeHeight() * 0.5f;
    const float modeIconSize = Ui::Px(18.0f);
    const GizmoMode mode = g_gizmos->GetGizmoTransformMode();
    if (UtilityIconButton(
            "##GizmoNone", g_icons->handTexture, mode == GizmoMode::None, modeSize, "None (1)", false, modeIconSize))
    {
        Curves::SetHandle(-1, -1);
        g_gizmos->SetGizmoTransformMode(GizmoMode::None);
    }

    ImGui::SameLine(0.0f, 0.0f);
    if (UtilityIconButton("##GizmoPosition", g_icons->positionTexture, mode == GizmoMode::Position, modeSize, "Position (2)",
            false, modeIconSize))
    {
        g_gizmos->SetGizmoTransformMode(GizmoMode::Position);
    }

    ImGui::SetCursorPos(ImVec2(modesX, contentY + modeSize));
    if (UtilityIconButton("##GizmoRotation", g_icons->rotationTexture, mode == GizmoMode::Rotation, modeSize, "Rotation (3)",
            false, modeIconSize))
    {
        g_gizmos->SetGizmoTransformMode(GizmoMode::Rotation);
    }

    ImGui::SameLine(0.0f, 0.0f);
    if (UtilityIconButton(
            "##GizmoBoth", g_icons->transformTexture, mode == GizmoMode::Both, modeSize, "Both (4)", false, modeIconSize))
    {
        g_gizmos->SetGizmoTransformMode(GizmoMode::Both);
    }

    const float modesShadowOffset = 1.0f;
    dl->AddRect(ImVec2(modesMin.x + modesShadowOffset, modesMin.y + modesShadowOffset),
        ImVec2(modesMax.x + modesShadowOffset, modesMax.y + modesShadowOffset), Theme::g_borderShadow, 0.0f, 0, Ui::Hairline());
    dl->AddRect(modesMin, modesMax, Theme::g_border, 0.0f, 0, Ui::Hairline());

    const float togglesX = modesX + layout.ModeWidth() + layout.sectionGap;
    const GizmoPivot pivot = Selection::GetPivotMode();
    const bool isPivot = pivot == GizmoPivot::Pivot;
    ImGui::SetCursorPos(ImVec2(togglesX, contentY));
    if (IconLabelButton("##GizmoPivot", isPivot ? "Pivot" : "Center", isPivot ? g_icons->pivotTexture : g_icons->centerTexture,
            layout.toggleWidth, Theme::g_iconSize))
    {
        Selection::TogglePivotMode();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip("Gizmo Pivot (Z)");
    }

    const GizmoSpace space = g_gizmos->GetGizmoSpace();
    const bool isGlobal = space == GizmoSpace::Global;
    ImGui::SetCursorPos(ImVec2(togglesX, contentY + Theme::g_frameHeight + layout.rowGap));
    if (IconLabelButton("##GizmoSpace", isGlobal ? "Global" : "Local", isGlobal ? g_icons->globalTexture : g_icons->localTexture,
            layout.toggleWidth, Theme::g_iconSize))
    {
        g_gizmos->SetGizmoSpace(isGlobal ? GizmoSpace::Local : GizmoSpace::Global);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip("Gizmo Space (X)");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

static void DrawCloneOffsetPopup(const ImRect& panelRect)
{
    const float disableWidth = ImGui::CalcTextSize("Disable").x + Theme::g_padding * 2.0f;
    const float resetWidth = ImGui::CalcTextSize("Reset").x + Theme::g_padding * 2.0f;
    const float spaceWidth = ImGui::CalcTextSize("Global").x + Theme::g_padding * 2.0f + Theme::g_frameHeight;
    const float modeWidth = ImGui::CalcTextSize("Random Range").x + Theme::g_padding * 4.0f + Theme::g_frameHeight;
    const float popupWidth
        = ceilf(ImMax(Ui::Px(370.0f), modeWidth + spaceWidth + disableWidth + resetWidth + Theme::g_popupPadding * 7.0f));
    ImGui::SetNextWindowPos(
        ImVec2(panelRect.Max.x, panelRect.Min.y - Theme::g_popupPadding), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(popupWidth, 0.0f), ImVec2(popupWidth, FLT_MAX));
    if (!ImCustom::BeginPopup(
            "##CloneOffsetPopup", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        return;
    }

    auto& state = g_utilityPanelState;
    auto& settings = state.cloneOffset;
    const float contentX = ImGui::GetCursorPosX();
    const float width = floorf(ImGui::GetContentRegionAvail().x);
    const char* title = "Clone Offset";
    ImGui::SetCursorPosX(contentX + floorf((width - ImGui::CalcTextSize(title).x) * 0.5f));
    ImGui::TextUnformatted(title);
    ImGui::SetCursorPosX(contentX);
    const char* labels[] = { "Position X", "Position Y", "Position Z", "Rotation X", "Rotation Y", "Rotation Z" };
    const char* formats[] = { "Position X: %.3f", "Position Y: %.3f", "Position Z: %.3f", "Rotation X: %.3f", "Rotation Y: %.3f",
        "Rotation Z: %.3f" };
    bool changed = false;
    for (int i = 0; i < 6; ++i)
    {
        const float limit = i < 3 ? 2000.0f : 360.0f;
        ImGui::SetCursorPosX(contentX);
        ImGui::PushID(i);
        if (settings.mode == SceneTools::CloneOffsetMode::Fixed)
        {
            ImGui::SetNextItemWidth(width);
            changed |= ImGui::DragFloat(
                "##Offset", &settings.fixed[i], i < 3 ? 0.1f : 0.2f, -limit, limit, formats[i], ImGuiSliderFlags_AlwaysClamp);
            LockMouseCursorForActiveDrag();
        }
        else
        {
            changed |= RangeSliderFloat("##Offset", labels[i], &settings.ranges[i][0], &settings.ranges[i][1], -limit, limit,
                width, state.cloneOffsetRangeBuf[i * 2], state.cloneOffsetRangeBuf[i * 2 + 1], state.cloneOffsetRangeFocus[i * 2],
                state.cloneOffsetRangeFocus[i * 2 + 1], RangeSliderCurve::SignedCubicIn, Ui::Px(64.0f), Theme::g_popupPadding);
        }
        ImGui::PopID();
    }
    const char* modes[] = { "Fixed", "Random Range" };
    ImGui::SetCursorPosX(contentX);
    if (ImCustom::BeginCombo("##CloneOffsetMode", modes[static_cast<int>(settings.mode)],
            width - Theme::g_popupPadding * 3.0f - spaceWidth - disableWidth - resetWidth))
    {
        for (int mode = 0; mode < IM_ARRAYSIZE(modes); ++mode)
        {
            if (ImGui::Selectable(modes[mode], mode == static_cast<int>(settings.mode)))
            {
                settings.mode = static_cast<SceneTools::CloneOffsetMode>(mode);
                changed = true;
                std::fill(std::begin(state.cloneOffsetRangeFocus), std::end(state.cloneOffsetRangeFocus), false);
            }
        }
        ImCustom::EndCombo();
    }
    ImGui::SameLine(0.0f, Theme::g_popupPadding);
    const char* spaces[] = { "Global", "Local" };
    if (ImCustom::BeginCombo("##CloneOffsetSpace", spaces[static_cast<int>(settings.space)], spaceWidth))
    {
        for (int space = 0; space < IM_ARRAYSIZE(spaces); ++space)
        {
            if (ImGui::Selectable(spaces[space], space == static_cast<int>(settings.space)))
            {
                settings.space = static_cast<SceneTools::CloneOffsetSpace>(space);
                changed = true;
            }
        }
        ImCustom::EndCombo();
    }
    if (changed)
    {
        settings.enabled = true;
    }
    ImGui::SameLine(0.0f, Theme::g_popupPadding);
    ImGui::BeginDisabled(!settings.enabled);
    const bool disable = ImGui::Button("Disable", ImVec2(disableWidth, Theme::g_frameHeight));
    ImGui::EndDisabled();
    if (disable)
    {
        settings.enabled = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine(0.0f, Theme::g_popupPadding);
    if (ImGui::Button("Reset", ImVec2(resetWidth, Theme::g_frameHeight)))
    {
        settings = {};
        std::fill(std::begin(state.cloneOffsetRangeFocus), std::end(state.cloneOffsetRangeFocus), false);
    }
    ImCustom::EndPopup();
}

void Gui::DrawUtilityPanel()
{
    if (!g_icons)
    {
        return;
    }

    const UtilityPanelLayout& layout = g_utilityPanelLayout;
    UtilityPanelState& state = g_utilityPanelState;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImRect panelRect = layout.Rect(display, display.x * GetSidebarVisualWidthRatio());
    const ImVec2 panelSize(panelRect.GetWidth(), panelRect.GetHeight());

    ImGui::SetNextWindowPos(panelRect.Min, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::g_panelBackground);
    ImGui::Begin("##UtilityPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 panelMax = wp + ImGui::GetWindowSize();
    dl->AddRect(wp, panelMax, Theme::g_border, 0.0f, 0, Ui::Hairline());
    dl->AddRectFilled(wp, ImVec2(panelMax.x, wp.y + Theme::g_accentSize), Theme::g_accent);

    const float contentY = Theme::g_accentSize + Theme::g_outerPadding;
    ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, contentY));
    if (UtilityIconButton("##RenderInteriors", g_icons->lampTexture, g_renderInteriors, layout.buttonSize, "Render Interiors"))
    {
        g_renderInteriors = !g_renderInteriors;
        if (!g_renderInteriors && g_scenePicker)
        {
            g_scenePicker->DropOutOfAreaSelection();
        }
    }

    ImGui::SameLine(0.0f, layout.buttonGap);
    const bool renderColliders = g_collisionRenderer && g_collisionRenderer->IsEnabled();
    if (g_collisionRenderer
        && UtilityIconButton("##RenderColliders", g_icons->cubeTexture, renderColliders, layout.buttonSize, "Render Colliders"))
    {
        g_collisionRenderer->SetEnabled(!renderColliders);
    }

    ImGui::SameLine(0.0f, layout.buttonGap);
    if (g_gizmos
        && UtilityIconButton(
            "##RenderBounds", g_icons->boundsTexture, g_gizmos->IsSelectionBoundsVisible(), layout.buttonSize, "Render Bounds"))
    {
        g_gizmos->SetSelectionBoundsVisible(!g_gizmos->IsSelectionBoundsVisible());
    }

    ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, contentY + layout.buttonSize + layout.buttonGap));
    if (UtilityIconButton("##TimeOverride", g_icons->clockTexture, state.timeEnabled, layout.buttonSize, "Time Override"))
    {
        state.timeDraft = state.timeEnabled ? state.timeValue : static_cast<int>(CClock::ms_nGameClockHours);
        ImGui::OpenPopup("##TimeOverridePopup");
    }

    ImGui::SameLine(0.0f, layout.buttonGap);
    if (UtilityIconButton(
            "##WeatherOverride", g_icons->cloudTexture, state.weatherEnabled, layout.buttonSize, "Weather Override"))
    {
        state.weatherDraft = state.weatherEnabled ? state.weatherValue : static_cast<int>(CWeather::NewWeatherType);
        ImGui::OpenPopup("##WeatherOverridePopup");
    }

    ImGui::SameLine(0.0f, layout.buttonGap);
    if (UtilityIconButton("##CloneOffset", g_icons->copyTexture, state.cloneOffset.enabled, layout.buttonSize, "Clone Offset"))
    {
        std::fill(std::begin(state.cloneOffsetRangeFocus), std::end(state.cloneOffsetRangeFocus), false);
        ImGui::OpenPopup("##CloneOffsetPopup");
    }

    DrawUtilityValuePopup(
        "##TimeOverridePopup", "Time Override", panelRect, state.timeDraft, state.timeValue, 0, 23, "%d:00", state.timeEnabled);
    DrawUtilityValuePopup("##WeatherOverridePopup", "Weather Override", panelRect, state.weatherDraft, state.weatherValue, 0, 45,
        "%d", state.weatherEnabled);
    DrawCloneOffsetPopup(panelRect);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void Gui::DrawViewportStatus()
{
    if (!g_scene)
    {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImRect gizmoRect = g_gizmoPanelLayout.Rect(display);
    const ImRect utilityRect = g_utilityPanelLayout.Rect(display, display.x * GetSidebarVisualWidthRatio());
    const float left = gizmoRect.Max.x + Theme::g_outerPadding;
    const float right = utilityRect.Min.x - Theme::g_outerPadding;
    if (right <= left)
    {
        return;
    }

    int objectCount = 0;
    int streamedObjectCount = 0;
    int vehicleCount = 0;
    int spawnedVehicleCount = 0;
    int pickupCount = 0;
    int spawnedPickupCount = 0;
    int actorCount = 0, spawnedActorCount = 0;
    int removalCount = 0;
    int checkpointCount = 0;
    for (int i = 0; i < g_scene->count; ++i)
    {
        const ItemData& data = g_scene->data[i];
        if (data.model.HasValue() && data.model.Get().modelId > 0 && g_scene->items[i].type == SceneItemNormal)
        {
            ++objectCount;
            if (g_objectRuntime && g_objectRuntime->Runtime(i).state == ObjectStreamState::Spawned)
            {
                ++streamedObjectCount;
            }
        }
        if (data.vehicle.HasValue() && VehicleCatalog::IsValid(data.vehicle.Get().modelId))
        {
            ++vehicleCount;
            if (g_vehicleRuntime && g_vehicleRuntime->GetVehicle(i))
            {
                ++spawnedVehicleCount;
            }
        }
        if (g_scene->items[i].type == SceneItemRemoveBuilding && data.removeBuilding.HasValue())
        {
            ++removalCount;
        }
        if (data.pickup.HasValue())
        {
            ++pickupCount;
            if (g_pickupRuntime && g_pickupRuntime->GetObject(i))
            {
                ++spawnedPickupCount;
            }
        }
        if (data.actor.HasValue())
        {
            ++actorCount;
            if (g_actorRuntime && g_actorRuntime->GetPed(i))
            {
                ++spawnedActorCount;
            }
        }
        if (g_scene->items[i].type == SceneItemRaceCheckpoint && data.raceCheckpoint.HasValue())
        {
            ++checkpointCount;
        }
    }

    char firstLine[64] = {};
    char selectedSuffix[64] = {};
    constexpr int groupCount = 6;
    constexpr char separator[] = " \xC2\xB7 ";
    char groups[groupCount][64] = {};
    snprintf(firstLine, sizeof(firstLine), "Scene Items: %d/%d", g_scene->count, EditorScene::MaxItems);
    const int selectedCount = g_scene->SelectionCount();
    if (selectedCount > 0)
    {
        snprintf(selectedSuffix, sizeof(selectedSuffix), " (%d Selected)", selectedCount);
    }

    snprintf(groups[0], sizeof(groups[0]), "Objects: %d (%d/%d Streamed)", objectCount, streamedObjectCount,
        g_objectRuntime ? g_objectRuntime->PreviewCapacity() : 0);
    snprintf(groups[1], sizeof(groups[1]), "Vehicles: %d/%d (%d/%d Spawned)", vehicleCount, PawnExport::MaxVehicles,
        spawnedVehicleCount, g_vehicleRuntime ? g_vehicleRuntime->PreviewCapacity() : 0);
    snprintf(groups[2], sizeof(groups[2]), "Pickups: %d (%d/%d Streamed)", pickupCount, spawnedPickupCount,
        g_pickupRuntime ? g_pickupRuntime->PreviewCapacity() : 0);
    snprintf(groups[3], sizeof(groups[3]), "Actors: %d (%d/%d Spawned)", actorCount, spawnedActorCount,
        g_actorRuntime ? g_actorRuntime->PreviewCapacity() : 0);
    snprintf(groups[4], sizeof(groups[4]), "Removals: %d/%d", removalCount, PawnExport::MaxRemovalInstructions);
    snprintf(groups[5], sizeof(groups[5]), "CPs: %d/%d", checkpointCount, EditorScene::MaxItems);

    ImFont* regularFont = m_fontSmall ? m_fontSmall : ImGui::GetFont();
    ImFont* boldFont = m_fontBold ? m_fontBold : regularFont;
    const float fontSize = ScaledFontSize(regularFont);
    const float separatorWidth = regularFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, separator).x;
    float groupWidths[groupCount + 1] = {};
    for (int i = 0; i < groupCount; ++i)
    {
        groupWidths[i + 1] = groupWidths[i] + regularFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, groups[i]).x;
    }

    std::array<int, 3> lineEnds {};
    int lineCount = 1;
    for (; lineCount <= 3; ++lineCount)
    {
        float bestWidth = FLT_MAX;
        float bestBalance = FLT_MAX;
        for (int splits = 0; splits < (1 << (groupCount - 1)); ++splits)
        {
            std::array<int, 3> ends {};
            int count = 0;
            int begin = 0;
            float widest = 0.0f;
            float balance = 0.0f;
            for (int i = 0; i < groupCount; ++i)
            {
                if (i != groupCount - 1 && !(splits & (1 << i)))
                {
                    continue;
                }
                if (count == lineCount)
                {
                    count = 0;
                    break;
                }
                ends[count++] = i + 1;
                const float width = groupWidths[i + 1] - groupWidths[begin] + separatorWidth * (i - begin);
                widest = ImMax(widest, width);
                balance += width * width;
                begin = i + 1;
            }
            if (count != lineCount)
            {
                continue;
            }
            if (widest < bestWidth || (widest == bestWidth && balance < bestBalance))
            {
                bestWidth = widest;
                bestBalance = balance;
                lineEnds = ends;
            }
        }
        if (bestWidth <= right - left || lineCount == 3)
        {
            break;
        }
    }

    const float firstY = display.y - Theme::g_outerPadding - fontSize * (lineCount + 1);
    const float centerX = (left + right) * 0.5f;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const float shadowOffset = Ui::Hairline();
    const ImU32 textColor = IM_COL32_WHITE;
    auto drawText = [&](ImFont* font, ImVec2 position, ImU32 color, const char* text)
    {
        drawList->AddText(
            font, fontSize, ImVec2(position.x + shadowOffset, position.y + shadowOffset), IM_COL32(0, 0, 0, 160), text);
        drawList->AddText(font, fontSize, position, color, text);
    };

    const float firstWidth = boldFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, firstLine).x;
    const float selectedWidth = selectedSuffix[0] ? boldFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, selectedSuffix).x : 0.0f;
    float x = centerX - (firstWidth + selectedWidth) * 0.5f;
    drawText(boldFont, ImVec2(x, firstY), textColor, firstLine);
    x += firstWidth;
    if (selectedSuffix[0])
    {
        drawText(boldFont, ImVec2(x, firstY), Theme::Alpha(textColor, 230), selectedSuffix);
    }

    int begin = 0;
    for (int i = 0; i < lineCount; ++i)
    {
        char line[sizeof(groups) + groupCount * sizeof(separator)] = {};
        size_t length = 0;
        for (int group = begin; group < lineEnds[i]; ++group)
        {
            length += snprintf(line + length, sizeof(line) - length, "%s%s", group == begin ? "" : separator, groups[group]);
        }
        const float width = regularFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line).x;
        drawText(regularFont, ImVec2(centerX - width * 0.5f, firstY + fontSize * (i + 1)), textColor, line);
        begin = lineEnds[i];
    }
}
