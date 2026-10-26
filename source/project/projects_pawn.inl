// Source Repository: https://github.com/waverso/waversomapstudio

PawnImport::Result PreviewImport(const char* source)
{
    PawnImport::Environment environment;
    environment.modelExists = ValidObjectModel;
    environment.pickupModelExists = PickupRuntime::IsValidModel;
    auto result = PawnImport::Parse(source ? source : "", environment);
    for (auto& item : result.items)
    {
        if (item.data.removeBuilding.HasValue())
        {
            const int modelId = item.data.removeBuilding.Get().modelId;
            item.searchModelId = modelId;
            if (modelId < 0)
            {
                strcpy_s(item.name, "Remove All Models");
            }
            else
            {
                char modelName[48];
                TextureBrowser::GetModelDisplay(modelId, modelName, sizeof(modelName));
                const bool lowLod = (CatalogDatabase::View().models[modelId].flags & CatalogDatabase::ModelFlagLowLod) != 0;
                snprintf(item.name, sizeof(item.name), lowLod ? "Remove %s (LOD)" : "Remove %s", modelName);
            }
            continue;
        }
        if (item.data.vehicle.HasValue())
        {
            snprintf(item.name, sizeof(item.name), "%s", VehicleCatalog::Get(item.data.vehicle.Get().modelId)->name);
            item.searchModelId = item.data.vehicle.Get().modelId;
            continue;
        }
        if (item.data.pickup.HasValue())
        {
            const int modelId = item.data.pickup.Get().modelId;
            TextureBrowser::GetModelDisplay(modelId, item.name, sizeof(item.name));
            item.searchModelId = modelId;
            continue;
        }
        if (item.data.actor.HasValue())
        {
            const int skinId = item.data.actor.Get().skinId;
            snprintf(item.name, sizeof(item.name), "%s", SkinCatalog::Get(skinId)->name);
            item.searchModelId = skinId;
            continue;
        }
        TextureBrowser::GetModelDisplay(item.data.model.Get().modelId, item.name, sizeof(item.name));
        item.searchModelId = item.data.model.Get().modelId;
        for (int i = 0; i < item.data.MaterialCount(); ++i)
        {
            auto& material = item.data.materials[i];
            RwTexture* texture = material.textureModelId > 0 && g_objectRuntime
                ? g_objectRuntime->ResolveTexture(material.textureModelId, material.txdName, material.textureName)
                : nullptr;
            if (texture)
            {
                g_objectRuntime->ReleaseTexture(texture);
            }
            else
            {
                material.textureModelId = material.textureModelId == 0 ? 0 : -1;
                material.txdName[0] = '\0';
                material.textureName[0] = '\0';
            }
        }
    }
    return result;
}

bool ApplyImport(const PawnImport::Result& result, std::string& error)
{
    if (result.failed || result.items.empty())
    {
        error = result.error.empty() ? "There are no valid items to import." : result.error;
        return false;
    }
    const std::size_t createdCount = result.items.size() + 1;
    if (createdCount > static_cast<std::size_t>(EditorScene::MaxItems - g_scene->count))
    {
        error = "The import exceeds the remaining scene capacity.";
        return false;
    }
    if (g_scene->nextId <= 0 || createdCount > static_cast<std::size_t>(std::numeric_limits<int>::max() - g_scene->nextId))
    {
        error = "No more scene item IDs are available.";
        return false;
    }
    auto items = result.items;
    int order = 0;
    for (int i = 0; i < g_scene->count; ++i)
    {
        if (g_scene->items[i].parent < 0)
        {
            order = (std::max)(order, g_scene->items[i].order + 1);
        }
    }
    int id = g_scene->nextId;
    const int parentId = id++;
    CVector minPos(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    CVector maxPos(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    for (const auto& item : items)
    {
        const CVector position(item.data.transform.pos[0], item.data.transform.pos[1], item.data.transform.pos[2]);
        minPos.x = (std::min)(minPos.x, position.x);
        minPos.y = (std::min)(minPos.y, position.y);
        minPos.z = (std::min)(minPos.z, position.z);
        maxPos.x = (std::max)(maxPos.x, position.x);
        maxPos.y = (std::max)(maxPos.y, position.y);
        maxPos.z = (std::max)(maxPos.z, position.z);
    }
    const CVector center((minPos.x + maxPos.x) * 0.5f, (minPos.y + maxPos.y) * 0.5f, (minPos.z + maxPos.z) * 0.5f);

    ItemSnapshot parent;
    parent.id = parentId;
    parent.parentId = -1;
    parent.order = order++;
    parent.type = SceneItemNormal;
    parent.open = true;
    parent.searchModelId = -2;
    strcpy_s(parent.name, "Imported Map");
    parent.data.transform.pos[0] = center.x;
    parent.data.transform.pos[1] = center.y;
    parent.data.transform.pos[2] = center.z;

    ProjectDocument::Items grouped;
    grouped.reserve(createdCount);
    grouped.push_back(std::move(parent));
    int childOrder = 10;
    for (auto& item : items)
    {
        item.id = id++;
        item.parentId = parentId;
        item.order = childOrder;
        childOrder += 10;
        if (item.type != SceneItemRemoveBuilding)
        {
            item.data.transform.pos[0] -= center.x;
            item.data.transform.pos[1] -= center.y;
            item.data.transform.pos[2] -= center.z;
        }
        grouped.push_back(std::move(item));
    }
    auto action = std::make_unique<CreateItemsAction>(std::move(grouped));
    if (!g_sceneActions->Execute(std::move(action)))
    {
        error = "The import could not be applied.";
        return false;
    }
    if (g_scenePicker)
    {
        g_scenePicker->ClearSelection();
    }
    g_scene->UpdateWorldTransforms();
    if (g_freeCamera)
    {
        g_freeCamera->FocusSelection();
    }
    GuiRequestOutlinerScrollTo(parentId);
    error.clear();
    return true;
}

bool ExportExists(const char* name, bool& exists, std::string& error)
{
    try
    {
        exists = std::filesystem::exists(NamedPath(L"exports", name, L".pwn"));
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

bool Export(const char* name, const PawnExport::Options& options, bool overwrite, std::string& error)
{
    try
    {
        const auto path = NamedPath(L"exports", name, L".pwn");
        const auto items = Capture(true);
        for (const auto& item : items)
        {
            if (options.Includes(PawnExport::Objects) && item.data.model.HasValue()
                && !ValidObjectModel(item.data.model.Get().modelId))
            {
                throw std::runtime_error("Choose a valid model for every object before exporting.");
            }
        }
        const auto code = PawnExport::Build(items, options);
        if (options.Includes(PawnExport::Races)
            && std::any_of(items.begin(), items.end(),
                [](const auto& item)
                {
                    return item.data.race.HasValue();
                }))
        {
            HMODULE module = nullptr;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(&Export), &module))
            {
                throw std::runtime_error("Cannot locate the race include resource.");
            }
            const auto resource = FindResourceA(module, MAKEINTRESOURCEA(IDR_RACE_INCLUDE), RT_RCDATA);
            const auto loaded = resource ? LoadResource(module, resource) : nullptr;
            const auto size = resource ? SizeofResource(module, resource) : 0;
            const auto* bytes = loaded ? static_cast<const std::uint8_t*>(LockResource(loaded)) : nullptr;
            if (!bytes || !size)
            {
                throw std::runtime_error("Cannot load the race include resource.");
            }
            const auto includePath = path.parent_path() / L"waversomapstudio_races.inc";
            const bool exists = std::filesystem::exists(includePath);
            bool matches = false;
            if (exists)
            {
                const auto previous = ReadFile(includePath);
                matches = previous.size() == size && std::memcmp(previous.data(), bytes, size) == 0;
            }
            if (!matches)
            {
                AtomicWrite(includePath, { bytes, size }, exists);
            }
        }
        AtomicWrite(path, { reinterpret_cast<const std::uint8_t*>(code.data()), code.size() }, overwrite);
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
