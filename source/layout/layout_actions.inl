// Source Repository: https://github.com/waverso/waversomapstudio

namespace Layout
{
    namespace
    {
        class ApplyAction final : public IAction
        {
            std::vector<ItemDataEditAction::Entry> entries;
            bool hasCreated;
            bool hasRemoved;
            CreateItemsAction created;
            DeleteItemsAction removed;

            void Set(bool after)
            {
                std::vector<int> selected(0);
                for (int index = 0; index < g_scene->count; ++index)
                {
                    if (g_scene->items[index].selected)
                    {
                        selected.push_back(g_scene->items[index].id);
                    }
                }
                std::vector<std::pair<int, ItemData>> values(0);
                values.reserve(entries.size());
                for (const auto& entry : entries)
                {
                    values.emplace_back(entry.itemId, after ? entry.after : entry.before);
                }
                if (after && hasCreated)
                {
                    created.Apply();
                }
                else if (!after && hasRemoved)
                {
                    removed.Undo();
                }
                if (after && hasRemoved)
                {
                    removed.Apply();
                }
                if (!after && hasCreated)
                {
                    created.Undo();
                }
                for (auto& [id, data] : values)
                {
                    const int index = g_scene->FindIndexById(id);
                    if (index >= 0)
                    {
                        if (g_objectRuntime && data.slide.HasValue())
                        {
                            g_objectRuntime->StopSlide(index);
                        }
                        g_scene->data[index] = std::move(data);
                    }
                }
                g_scene->ClearSelection();
                for (const int id : selected)
                {
                    const int index = g_scene->FindIndexById(id);
                    if (index >= 0)
                    {
                        g_scene->items[index].selected = true;
                    }
                }
                g_scene->UpdateWorldTransforms();
            }

            public:
            ApplyAction(std::vector<ItemDataEditAction::Entry>&& changes, std::vector<ItemSnapshot>&& additions,
                std::vector<ItemSnapshot>&& deletions)
                : entries(0), hasCreated(!additions.empty()), hasRemoved(!deletions.empty()),
                  created(std::move(additions), false, -1, false), removed(std::move(deletions))
            {
                entries.swap(changes);
            }
            void Apply() override
            {
                Set(true);
            }
            void Undo() override
            {
                Set(false);
            }
            size_t MemoryUsage() const noexcept override
            {
                size_t bytes
                    = sizeof(*this) + entries.capacity() * sizeof(entries[0]) + created.MemoryUsage() + removed.MemoryUsage();
                for (const auto& entry : entries)
                {
                    bytes += entry.before.DynamicMemoryUsage() + entry.after.DynamicMemoryUsage();
                }
                return bytes;
            }
        };
    }

    static std::unique_ptr<IAction> BuildAction(int parent, std::string& error)
    {
        error.clear();
        if (!g_scene || !g_sceneActions || parent < 0 || parent >= g_scene->count || !g_scene->data[parent].layout.HasValue())
        {
            error = "Select an item with a Layout component.";
            return nullptr;
        }
        auto& scene = *g_scene;
        scene.UpdateWorldTransforms();
        const auto children = Inspect(scene, parent);
        auto settings = EffectiveSettings(scene.data[parent].layout.Get(), children);
        if (settings.copyFromFirst && scene.data[parent].layout.Get().pieces > 0 && children.indices.empty())
        {
            error = "Copy From First Child requires a child.";
            return nullptr;
        }
        if (!settings.IsValid())
        {
            error = "Invalid layout settings.";
            return nullptr;
        }
        if (!ResolveGeometry(scene, children, settings, error))
        {
            return nullptr;
        }
        const int previousCount = static_cast<int>(children.indices.size());
        const int extra = (std::max)(0, settings.pieces - previousCount);
        const bool canGenerate = children.CanGenerate(settings.copyFromFirst);
        std::vector<int> prototypeItems(0);
        if (settings.copyFromFirst && extra > 0)
        {
            const int first = children.indices.front();
            for (const int index : scene.DisplayOrder())
            {
                if (index == first || scene.IsDescendantOf(index, first))
                {
                    prototypeItems.push_back(index);
                }
            }
        }
        const size_t needed = static_cast<size_t>(extra) * (settings.copyFromFirst ? prototypeItems.size() : 1);
        if (needed > static_cast<size_t>(EditorScene::MaxItems - scene.count)
            || scene.nextId >= INT_MAX - static_cast<int>(needed))
        {
            error = "Layout exceeds the item limit.";
            return nullptr;
        }
        if (!settings.copyFromFirst && canGenerate && settings.pieces > 0 && (extra > 0 || settings.modelPending)
            && !TextureBrowser::ModelExists(settings.modelId))
        {
            error = "Choose a valid model before generating pieces.";
            return nullptr;
        }
        for (const int index : children.indices)
        {
            const auto& data = scene.data[index];
            if (settings.pieces > 0 && data.removeBuilding.HasValue() && !data.removeBuilding.Get().custom)
            {
                error = "Layout cannot move a non-custom building removal.";
                return nullptr;
            }
        }
        const CurveGeometry::Path* path = nullptr;
        if (settings.type == LayoutType::Curve && settings.pieces > 0)
        {
            const int curve = scene.FindIndexById(settings.curveId);
            if (!Curves::CanUse(scene, parent, curve))
            {
                error = "Choose a curve outside this Layout's hierarchy.";
                return nullptr;
            }
            path = Curves::Resolve(scene, curve, error);
            if (!path)
            {
                return nullptr;
            }
        }
        std::vector<ItemDataEditAction::Entry> entries(0);
        std::vector<ItemSnapshot> created(0), removed(0);
        entries.reserve((std::min)(settings.pieces, previousCount) + 1);
        created.reserve(needed);
        removed.reserve((std::max)(0, previousCount - settings.pieces));
        std::array<bool, EditorScene::MaxItems> deleting {};
        int lastOrder = -1;
        for (int piece = 0; piece < previousCount; ++piece)
        {
            const int index = children.indices[piece];
            lastOrder = (std::max)(lastOrder, scene.items[index].order);
            if (piece >= settings.pieces)
            {
                deleting[index] = true;
                continue;
            }
            ItemDataEditAction::Entry entry;
            entry.itemId = scene.items[index].id;
            entry.before = scene.data[index];
            entry.after = entry.before;
            if (!settings.copyFromFirst && canGenerate && settings.modelPending
                && entry.after.model.Get().modelId != settings.modelId)
            {
                entry.after.model.Edit().modelId = settings.modelId;
                ApplyModelDistanceDefaults(entry.after.model.Edit());
            }
            SetPiecePose(scene, parent, settings, settings.radius, piece, scene.items[index].type, entry.after, path);
            if (entry.before != entry.after)
            {
                entries.push_back(std::move(entry));
            }
        }
        if (lastOrder >= INT_MAX - extra)
        {
            error = "Layout child order exceeds the supported range.";
            return nullptr;
        }
        for (const int index : scene.DisplayOrder())
        {
            const int ancestor = scene.items[index].parent;
            if (!deleting[index] && (ancestor < 0 || !deleting[ancestor]))
            {
                continue;
            }
            deleting[index] = true;
            removed.emplace_back();
            scene.CaptureItemSnapshot(index, removed.back());
        }
        std::vector<ItemSnapshot> prototype(0);
        if (extra > 0)
        {
            if (settings.copyFromFirst)
            {
                prototype.reserve(prototypeItems.size());
                for (const int index : prototypeItems)
                {
                    prototype.emplace_back();
                    scene.CaptureItemSnapshot(index, prototype.back());
                }
            }
            else
            {
                prototype.resize(1);
                auto& snapshot = prototype.front();
                snapshot.type = SceneItemNormal;
                AddComponentToItem(snapshot.data, ComponentTypeModel);
                auto& model = snapshot.data.model.Edit();
                model.modelId = settings.modelId;
                ApplyModelDistanceDefaults(model);
                snapshot.searchModelId = settings.modelId;
                TextureBrowser::GetModelDisplay(settings.modelId, snapshot.name, sizeof(snapshot.name));
            }
        }
        for (int piece = previousCount; piece < settings.pieces; ++piece)
        {
            std::array<int, EditorScene::MaxItems> cloned {};
            const size_t first = created.size();
            for (size_t part = 0; part < prototype.size(); ++part)
            {
                auto snapshot = prototype[part];
                snapshot.id = scene.nextId + static_cast<int>(created.size());
                if (part == 0)
                {
                    snapshot.parentId = scene.items[parent].id;
                    snapshot.order = ++lastOrder;
                    SetPiecePose(scene, parent, settings, settings.radius, piece, snapshot.type, snapshot.data, path);
                }
                else
                {
                    snapshot.parentId = cloned[scene.items[prototypeItems[part]].parent];
                }
                if (settings.copyFromFirst)
                {
                    cloned[prototypeItems[part]] = snapshot.id;
                }
                created.push_back(std::move(snapshot));
            }
            if (settings.copyFromFirst)
            {
                for (size_t part = first; part < created.size(); ++part)
                {
                    auto& value = created[part].data;
                    if (!value.layout.HasValue())
                    {
                        continue;
                    }
                    const int curve = scene.FindIndexById(value.layout.Get().curveId);
                    if (curve >= 0 && cloned[curve] > 0)
                    {
                        value.layout.Edit().curveId = cloned[curve];
                    }
                }
            }
        }
        ItemDataEditAction::Entry parentEntry;
        parentEntry.itemId = scene.items[parent].id;
        parentEntry.before = scene.data[parent];
        parentEntry.after = parentEntry.before;
        auto& applied = parentEntry.after.layout.Edit();
        applied.pieces = settings.pieces;
        applied.modelId = settings.modelId;
        applied.radius = settings.radius;
        std::copy_n(settings.gridSpacing, 2, applied.gridSpacing);
        applied.modelPending = applied.piecesPending = false;
        if (parentEntry.before != parentEntry.after)
        {
            entries.push_back(std::move(parentEntry));
        }
        if (entries.empty() && created.empty() && removed.empty())
        {
            return nullptr;
        }
        auto action = std::make_unique<ApplyAction>(std::move(entries), std::move(created), std::move(removed));
        if (action->MemoryUsage() > ActionManager::MaxHistoryBytes)
        {
            error = "Layout exceeds the undo memory limit.";
            return nullptr;
        }
        return action;
    }
}
