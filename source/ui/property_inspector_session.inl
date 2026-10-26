// Source Repository: https://github.com/waverso/waversomapstudio

bool PropertyInspector::Finish(bool settle)
{
    if (nameDirty)
    {
        auto group = std::make_unique<CompositeAction>();
        group->Reserve(ids.size());
        const auto& name = buffers[nameOwner].text;
        for (int id : ids)
        {
            const int index = g_scene->FindIndexById(id);
            if (index >= 0 && strcmp(g_scene->items[index].name, name.data()) != 0)
            {
                group->Add(std::make_unique<RenameItemAction>(id, g_scene->items[index].name, name.data()));
            }
        }
        if (!group->Empty() && !g_sceneActions->Execute(std::move(group), true))
        {
            return false;
        }
        nameDirty = false;
    }
    if (session.Active())
    {
        std::string error;
        if (!session.CaptureAfter(*g_scene, error) || !session.Finish(error))
        {
            session.Cancel(*g_scene);
            g_toast.ShowError(error.c_str());
            return false;
        }
    }
    const bool finished = !settle || Layout::SettleAutoApply();
    for (size_t selected = 0; selected < ids.size() && selected < targets.size(); ++selected)
    {
        auto& target = targets[selected];
        target.index = g_scene->FindIndexById(ids[selected]);
        if (target.index >= 0)
        {
            target.data = &g_scene->data[target.index];
        }
        else
        {
            interrupted = true;
        }
    }
    return finished;
}

bool PropertyInspector::Write(ImGuiID owner, const PropertyEdit::Session::Patch& patch, const PropertyWidgetResult& result)
{
    if (readOnly || ids.empty() || interrupted)
    {
        return false;
    }
    try
    {
        if (session.Active() && session.Owner() != owner && (!Finish() || interrupted))
        {
            return false;
        }
        std::string error;
        if (!session.Write(*g_scene, ids, owner, patch, error))
        {
            g_toast.ShowError(error.c_str());
            return false;
        }
        g_scene->UpdateWorldTransforms();
        interacting |= result.active;
        return true;
    }
    catch (const std::bad_alloc&)
    {
        session.Cancel(*g_scene);
        g_toast.ShowError("Not enough memory to edit the selection.");
        return false;
    }
}

void PropertyInspector::Bind()
{
    if (resetBuffers)
    {
        buffers.clear();
        widgets = ItemWidgetState {};
        layoutSignature = 0;
        resetBuffers = false;
    }
    std::vector<int> selected;
    for (int index : g_scene->DisplayOrder())
    {
        if (g_scene->items[index].selected)
        {
            selected.push_back(g_scene->items[index].id);
        }
    }
    const auto& natives = g_scenePicker->GetSelection();
    std::vector<std::uintptr_t> nativeSelection;
    for (const auto& native : natives)
    {
        nativeSelection.push_back(reinterpret_cast<std::uintptr_t>(native.lastEntity));
    }
    if (ids != selected || nativeIds != nativeSelection)
    {
        pending = {};
        if (!Finish())
        {
            targets.clear();
            interrupted = true;
            return;
        }
        selected.clear();
        for (int index : g_scene->DisplayOrder())
        {
            if (g_scene->items[index].selected)
            {
                selected.push_back(g_scene->items[index].id);
            }
        }
        ids.swap(selected);
        nativeIds.swap(nativeSelection);
        buffers.clear();
        widgets = ItemWidgetState {};
        ImGui::ClearActiveID();
    }
    readOnly = !natives.empty();
    targets.clear();
    nativeData.resize(natives.size());
    nativeNames.resize(natives.size());
    for (int id : ids)
    {
        const int index = g_scene->FindIndexById(id);
        if (index >= 0)
        {
            targets.push_back({ index, &g_scene->data[index] });
        }
    }
    for (size_t i = 0; i < natives.size(); ++i)
    {
        auto& data = nativeData[i];
        const auto& native = natives[i];
        data.transform.pos[0] = native.posPivot.x;
        data.transform.pos[1] = native.posPivot.y;
        data.transform.pos[2] = native.posPivot.z;
        Utils::BasisToEulerDegrees(native.rot, data.transform.rot);
        data.model.Edit().modelId = native.nModelId;
        ApplyModelDistanceDefaults(data.model.Edit());
        TextureBrowser::GetModelDisplay(native.nModelId, nativeNames[i].data(), nativeNames[i].size());
        targets.push_back({ -1, &data });
    }
    std::uint64_t signature = 1469598103934665603ull;
    for (int id : ids)
    {
        signature = (signature ^ static_cast<unsigned>(id)) * 1099511628211ull;
    }
    for (auto id : nativeIds)
    {
        signature = (signature ^ id) * 1099511628211ull;
    }
    for (const auto& target : targets)
    {
        signature = (signature ^ PropertyEdit::Structure(*target.data)) * 1099511628211ull;
    }
    if (signature != structureSignature)
    {
        std::vector<const ItemData*> data;
        data.reserve(targets.size());
        for (const auto& target : targets)
        {
            data.push_back(target.data);
        }
        components = PropertyEdit::CommonComponents(data, partial);
        structureSignature = signature;
    }
    interacting = false;
    interrupted = false;
}

static bool InspectionUsesTransactions()
{
    return g_propertyInspector.session.Active() || g_propertyInspector.drawFrame == ImGui::GetFrameCount();
}
