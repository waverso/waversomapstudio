// Source Repository: https://github.com/waverso/waversomapstudio

static void DrawObjectBrowser()
{
    ObjectBrowserUiState& ui = g_objectBrowserUi;
    if (ui.viewOnly)
    {
        bool selected = false, closed = false;
        DrawObjectDetailModal(selected, closed);
        if (closed)
        {
            CloseObjectBrowser();
        }
        return;
    }
    if (!ui.open)
    {
        return;
    }

    if (ui.dirty)
    {
        ObjectBrowser::Filter filter;
        strncpy_s(filter.search, sizeof(filter.search), ui.search, _TRUNCATE);
        filter.searchFields = ui.searchFields ? ui.searchFields : ObjectBrowser::SearchName;
        filter.sourceMask = ui.sourceMask;
        filter.categoryMask = ui.categoryMask;
        filter.lodFilter = ui.lodFilter;
        filter.breakableFilter = ui.breakableFilter;
        filter.collidableFilter = ui.collidableFilter;
        filter.animationFilter = ui.animationFilter;
        filter.uvAnimFilter = ui.uvAnimFilter;
        filter.backfaceFilter = ui.backfaceFilter;
        filter.timeFilter = ui.timeFilter;
        filter.physicsFilter = ui.physicsFilter;
        filter.explosiveFilter = ui.explosiveFilter;
        filter.idMin = ui.idMin;
        filter.idMax = ui.idMax;
        filter.dffSizeMinKb = ui.dffSizeMinKb;
        filter.dffSizeMaxKb = ui.dffSizeMaxKb;
        filter.radiusMin = ui.radiusMin;
        filter.radiusMax = ui.radiusMax;
        filter.lengthMin = ui.lengthMin;
        filter.lengthMax = ui.lengthMax;
        filter.widthMin = ui.widthMin;
        filter.widthMax = ui.widthMax;
        filter.heightMin = ui.heightMin;
        filter.heightMax = ui.heightMax;
        filter.drawDistanceMin = ui.drawDistanceMin;
        filter.drawDistanceMax = ui.drawDistanceMax;
        filter.polyCountMin = ui.polyCountMin;
        filter.polyCountMax = ui.polyCountMax;
        filter.favoritesOnly = ui.favoritesOnly;
        filter.sortKey = ui.sortKey;
        filter.sortDesc = ui.sortDesc;
        ObjectBrowser::RunFilter(filter, ui.filtered);
        if (ui.pickupMode)
        {
            std::erase_if(ui.filtered,
                [](std::uint32_t id)
                {
                    return !PickupRuntime::IsValidModel(static_cast<int>(id));
                });
        }
        ui.dirty = false;
    }
    static const char* const sortNames[]
        = { "Model ID", "Name", "Library", "Radius", "Length", "Width", "Height", "Polygons", "DFF Size (KB)" };
    bool cancel = false;
    const int selectedId = DrawBrowserCatalog(
        "##ObjectBrowser", ui.pickupMode ? "Select Pickup Model" : "Select Object Model", "Search", "model", ui, sortNames,
        ObjectBrowserFiltersPanelActive(ui), ObjectBrowserHasActiveFilters(ui), ui.detailModelId >= 0 || ui.filtersOpen,
        OpenObjectFilters, ClearAllObjectBrowserFilters, ModelThumbnails::PreparePage,
        [](std::uint32_t key)
        {
            const int id = static_cast<int>(key);
            const auto model = ObjectBrowser::GetModel(id);
            BrowserCell cell;
            cell.name = model.name;
            cell.favorite = ObjectBrowser::IsFavorite(id);
            cell.image = GetBrowserModelImage(id);
            snprintf(cell.subtitle, sizeof(cell.subtitle), "ID %d%s", id, model.lowLod ? "  LOD" : "");
            snprintf(cell.tooltip, sizeof(cell.tooltip), "%s\n%s.txd\n%s\n%u materials", model.name, model.txdName,
                TextureBrowser::SourceImgName(model.sourceImg), static_cast<unsigned>(model.materialCount));
            return cell;
        },
        [](std::uint32_t id)
        {
            ObjectBrowser::ToggleFavorite(static_cast<int>(id));
        },
        cancel);
    if (selectedId >= 0)
    {
        ui.detailModelId = selectedId;
        ModelPreview::Open(selectedId);
    }
    DrawObjectFiltersModal();
    bool selected = false, closed = false;
    DrawObjectDetailModal(selected, closed);
    bool confirmed = false;
    if (selected)
    {
        if (ui.changeItemId >= 0 && InspectionConfirmModel(ui.detailModelId, ComponentTypeModel))
        {
            confirmed = true;
        }
        else if (ui.changeItemId >= 0)
        {
            const int ci = g_scene->FindIndexById(ui.changeItemId);
            if (ui.layoutMode && ci >= 0 && g_scene->data[ci].layout.HasValue() && !g_scene->data[ci].layout.Get().copyFromFirst
                && Layout::Inspect(*g_scene, ci).CanGenerate() && TextureBrowser::ModelExists(ui.detailModelId))
            {
                auto& layout = g_scene->data[ci].layout.Edit();
                layout.modelId = ui.detailModelId;
                layout.modelPending = true;
                auto& widgets = g_itemWidgets[ci];
                widgets.modelHasFocus = false;
                snprintf(widgets.modelBuf, sizeof(widgets.modelBuf), "%d", ui.detailModelId);
                confirmed = true;
            }
            else if (ui.pickupMode && ci >= 0 && g_scene->data[ci].pickup.HasValue()
                && PickupRuntime::IsValidModel(ui.detailModelId))
            {
                g_scene->data[ci].pickup.Edit().modelId = ui.detailModelId;
                ItemWidgetState& widgets = g_itemWidgets[ci];
                widgets.modelHasFocus = false;
                snprintf(widgets.modelBuf, sizeof(widgets.modelBuf), "%d", ui.detailModelId);
                confirmed = true;
            }
            else if (!ui.pickupMode && !ui.layoutMode && ci >= 0 && ci < g_scene->count
                && (g_scene->data[ci].model.HasValue()
                    || CanAddComponentType(g_scene->items[ci], g_scene->data[ci], ComponentTypeModel)))
            {
                if (!g_scene->data[ci].model.HasValue())
                {
                    GuiAddComponent(ci, ComponentTypeModel);
                }
                ModelData& model = g_scene->data[ci].model.Edit();
                model.modelId = ui.detailModelId;
                ApplyModelDistanceDefaults(model);
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
            confirmed = AddObjectToScene(ui.detailModelId);
        }
    }
    if (confirmed)
    {
        CloseObjectBrowser();
        g_modal.activeType = PendingModalNone;
    }
    else if (closed)
    {
        if (ui.viewOnly)
        {
            CloseObjectBrowser();
        }
        else
        {
            ModelPreview::Close();
            ui.detailModelId = -1;
        }
    }
    else if (cancel)
    {
        CloseObjectBrowser();
    }
}
