// Source Repository: https://github.com/waverso/waversomapstudio

void Gui::DrawProperties()
{
    ImGui::BeginChild("##Properties", ImGui::GetContentRegionAvail(), false);
    DrawInspectionProperties(ImGui::GetContentRegionAvail().x - Theme::g_padding * 2);
    ImGui::EndChild();
}

bool GuiIsAnyModalOpen()
{
    return IsAnyModalOpen();
}

void GuiRequestBookmarkFocus(std::uint64_t id)
{
    Selection::ClearAll();
    g_bookmarkViewRequest = true;
    g_viewUi.bmOpen = true;
    g_viewUi.bmSearch[0] = '\0';
    g_viewUi.bmFocused = false;
    g_viewUi.bookmarkFocusId = id;
    g_viewUi.bookmarkFocusPending = true;
    g_viewUi.bookmarkAreaFocusPending = true;
}

bool GuiIsTransformEditing(int itemIndex)
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count || !ImGui::GetCurrentContext() || !ImGui::IsAnyItemActive())
    {
        return false;
    }
    const int frame = ImGui::GetFrameCount();
    for (int index = itemIndex; index >= 0; index = g_scene->items[index].parent)
    {
        if (g_scene->items[index].selected && g_propertyInspector.interactionType == ComponentTypeTransform
            && g_propertyInspector.interactionFrame >= frame - 1)
        {
            return true;
        }
        const auto* cached = g_itemWidgets.Find(index);
        if (!cached)
        {
            continue;
        }
        const auto& widgets = *cached;
        const auto active = [frame](const Vec3State& state)
        {
            return state.interacting && state.interactionFrame >= frame - 1;
        };
        if (g_scene->items[index].selected && widgets.transformOpen && (active(widgets.posState) || active(widgets.rotState)))
        {
            return true;
        }
        if (!g_scene->ComposesWithParent(index))
        {
            break;
        }
    }
    return false;
}

int GuiGetBoxAreaEditingFace(int itemIndex)
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count || !g_scene->items[itemIndex].selected
        || !g_scene->data[itemIndex].boxArea.HasValue() || !ImGui::GetCurrentContext() || !ImGui::IsAnyItemActive())
    {
        return -1;
    }
    if (g_propertyInspector.boxActiveId == ImGui::GetActiveID()
        && g_propertyInspector.interactionFrame >= ImGui::GetFrameCount() - 1
        && std::find(g_propertyInspector.ids.begin(), g_propertyInspector.ids.end(), g_scene->items[itemIndex].id)
            != g_propertyInspector.ids.end())
    {
        const auto component = g_propertyInspector.open.find(ComponentTypeBoxArea * 32);
        if (component == g_propertyInspector.open.end() || component->second)
        {
            return g_propertyInspector.boxFace;
        }
    }
    const auto* widgets = g_itemWidgets.Find(itemIndex);
    if (!widgets || !widgets->boxAreaOpen || widgets->boxBoundsActiveId != ImGui::GetActiveID())
    {
        return -1;
    }
    return widgets->boxBoundsActiveFace;
}

bool GuiIsAreaEditing(int itemIndex)
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count || !g_scene->items[itemIndex].selected
        || !ImGui::GetCurrentContext() || !ImGui::IsAnyItemActive())
    {
        return false;
    }
    const auto& data = g_scene->data[itemIndex];
    const bool customRemove = data.removeBuilding.HasValue() && data.removeBuilding.Get().custom;
    if (!data.boxArea.HasValue() && !customRemove)
    {
        return false;
    }
    if (GuiIsTransformEditing(itemIndex))
    {
        return true;
    }
    if (g_propertyInspector.interactionFrame >= ImGui::GetFrameCount() - 1
        && (g_propertyInspector.interactionType == ComponentTypeBoxArea
            || g_propertyInspector.interactionType == ComponentTypeRemoveBuilding))
    {
        return true;
    }
    const auto* widgets = g_itemWidgets.Find(itemIndex);
    if (!widgets)
    {
        return false;
    }
    if (data.boxArea.HasValue() && widgets->boxAreaOpen
        && ((widgets->boxSizeState.interacting && widgets->boxSizeState.interactionFrame >= ImGui::GetFrameCount() - 1)
            || GuiGetBoxAreaEditingFace(itemIndex) >= 0))
    {
        return true;
    }
    return customRemove && widgets->rbOpen && widgets->rbRadiusActiveId == ImGui::GetActiveID();
}

void GuiResetProjectState()
{
    CloseObjectBrowser();
    CloseTextureBrowser();
    g_propertyInspector.session.Reset();
    g_propertyInspector.ids.clear();
    g_propertyInspector.nativeIds.clear();
    g_propertyInspector.nativeData.clear();
    g_propertyInspector.nativeNames.clear();
    g_propertyInspector.targets.clear();
    g_propertyInspector.components.clear();
    g_propertyInspector.structureSignature = 0;
    g_propertyInspector.layoutSignature = 0;
    g_propertyInspector.pending = {};
    g_propertyInspector.buffers.clear();
    g_propertyInspector.open.clear();
    g_propertyInspector.picker.kind = InspectionPicker::Kind::None;
    g_propertyInspector.interactionFrame = g_propertyInspector.drawFrame = -1;
    g_propertyInspector.nameDirty = false;
    g_propertyInspector.resetBuffers = false;
    CloseVehicleBrowser();
    CloseSkinBrowser();
    g_bookmarkViewRequest = false;
    g_viewUi.bookmarkFocusId = 0;
    g_viewUi.bookmarkFocusPending = false;
    g_viewUi.bookmarkAreaFocusPending = false;
    g_outlinerUi = OutlinerUiState {};
    g_itemWidgets.Clear();
    g_pendingCurveLocateId = -1;
    g_editTracker = ItemEditTracker {};
    g_editTracker.lastSerial = g_sceneActions->GetSerial();
    g_modal = PendingModalState {};
    g_propScroll = PropScrollRequest {};
    if (g_gizmos)
    {
        g_gizmos->SetGizmoVisible(false);
        g_gizmos->SetRadiusGizmo(false, CVector(0, 0, 0), 0);
    }
    ImGui::ClearActiveID();
}

void GuiOnSceneRestored(int oldCount) noexcept
{
    try
    {
        if (g_objectRuntime)
        {
            std::array<int, EditorScene::MaxItems> remap;
            remap.fill(-1);
            g_objectRuntime->OnSceneRemap(remap.data(), oldCount);
        }
        if (g_vehicleRuntime)
        {
            g_vehicleRuntime->Clear();
        }
        if (g_pickupRuntime)
        {
            g_pickupRuntime->Clear();
        }
        if (g_actorRuntime)
        {
            g_actorRuntime->Clear();
        }
        g_itemWidgets.Clear();
        g_propertyInspector.resetBuffers = true;
        for (size_t target = 0; target < g_propertyInspector.ids.size() && target < g_propertyInspector.targets.size(); ++target)
        {
            auto& selected = g_propertyInspector.targets[target];
            selected.index = g_scene->FindIndexById(g_propertyInspector.ids[target]);
            if (selected.index >= 0)
            {
                selected.data = &g_scene->data[selected.index];
            }
        }
        g_propertyInspector.interrupted = true;
    }
    catch (...)
    {
    }
}

bool GuiFinishPropertyEdit()
{
    try
    {
        return g_propertyInspector.Finish();
    }
    catch (const std::bad_alloc&)
    {
        g_toast.ShowError("Not enough memory to finish the edit.");
        return false;
    }
}
