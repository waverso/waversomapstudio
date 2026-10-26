// Source Repository: https://github.com/waverso/waversomapstudio

namespace Layout
{
    namespace
    {
        struct AutoState
        {
            LayoutData settings;
            TransformData world;
            std::array<float, 2> sourceSize = {};
            std::uint64_t checkedAt = 0;
            bool sourceValid = false;
            std::uint64_t curve = 0;
            std::uint64_t appliedAt = 0;
            std::uint64_t sequence = 0;
            std::uint64_t triggerSequence = 0;
            bool initialized = false;
            bool changed = false;
        };
        std::unordered_map<int, AutoState> g_autoStates;
        std::vector<std::unique_ptr<IAction>> g_autoActions;
        std::unique_ptr<IAction> g_autoCommit;
        std::uint64_t g_autoSequence = 0;
        size_t g_autoBytes = 0;
        bool g_baselineAutoApply = false;
        std::uint64_t g_autoTime = 0;
        bool g_settlingAutoApply = false;
        EditorScene* g_autoScene = nullptr;

        bool SourceSize(const EditorScene& scene, int parent, const LayoutData& settings, std::array<float, 2>& size)
        {
            size = {};
            if (settings.pieces == 0 || settings.type == LayoutType::Curve || settings.UsesStep()
                || !(settings.type == LayoutType::Grid ? settings.autoStep : settings.autoRadius))
            {
                return false;
            }
            const auto children = Inspect(scene, parent);
            if ((!settings.copyFromFirst && children.CanGenerate()) || children.indices.empty())
            {
                return false;
            }
            CVector minimum, maximum;
            if (!Selection::ComputeItemBounds(scene, children.indices.front(), settings.rotation, minimum, maximum))
            {
                return false;
            }
            size = { maximum.x - minimum.x, maximum.y - minimum.y };
            return true;
        }

        bool CommitAutoApply()
        {
            if ((!g_autoCommit && g_autoActions.empty()) || !g_sceneActions)
            {
                return true;
            }
            if (!g_autoCommit)
            {
                auto group = std::make_unique<CompositeAction>();
                group->Reserve(g_autoActions.size());
                for (auto& action : g_autoActions)
                {
                    group->Add(std::move(action));
                }
                g_autoCommit = std::move(group);
                g_autoActions.clear();
            }
            const bool edited = g_sceneActions->UndoSequence() > g_autoSequence
                && (!g_userActions || g_userActions->UndoSequence() < g_sceneActions->UndoSequence());
            if (edited && !g_sceneActions->CanAppendApplied(g_autoCommit->MemoryUsage()))
            {
                GuiShowToast("Auto Apply exceeds the undo memory limit.", true);
                return false;
            }
            const bool recorded
                = edited ? g_sceneActions->AppendApplied(g_autoCommit) : g_sceneActions->RecordApplied(g_autoCommit);
            if (!g_autoCommit)
            {
                g_autoBytes = 0;
            }
            return recorded;
        }
    }

    void ResetAutoApply(bool dirty, bool keepTiming)
    {
        g_autoScene = g_scene;
        g_autoActions.clear();
        g_autoCommit.reset();
        for (auto& [id, state] : g_autoStates)
        {
            const auto appliedAt = state.appliedAt;
            state = {};
            state.appliedAt = appliedAt;
        }
        if (!keepTiming)
        {
            g_autoStates.clear();
        }
        g_autoBytes = 0;
        g_autoSequence = 0;
        g_baselineAutoApply = !dirty;
    }

    static bool ApplyAutoLayouts(std::uint64_t time, bool editing, bool force)
    {
        if (!g_scene || !g_sceneActions || editing)
        {
            return true;
        }
        if (g_autoScene != g_scene)
        {
            ResetAutoApply();
            g_autoScene = g_scene;
        }
        if (g_autoCommit)
        {
            return CommitAutoApply();
        }
        std::unique_ptr<SceneEditGuard> guard;
        try
        {
            bool failed = false;
            bool applied = false;
            int passes = 0;
            do
            {
                applied = false;
                g_scene->UpdateWorldTransforms();
                std::array<int, EditorScene::MaxItems> layouts;
                int layoutCount = 0;
                for (int index : g_scene->DisplayOrder())
                {
                    if (g_scene->data[index].layout.HasValue())
                    {
                        layouts[layoutCount++] = g_scene->items[index].id;
                    }
                }
                for (int entry = 0; entry < layoutCount; ++entry)
                {
                    const int id = layouts[entry];
                    const int index = g_scene->FindIndexById(id);
                    if (index < 0 || !g_scene->data[index].layout.HasValue())
                    {
                        continue;
                    }
                    auto& state = g_autoStates[id];
                    const auto settings = g_scene->data[index].layout.Get();
                    TransformData world;
                    const auto& pose = g_scene->world[index];
                    world.pos[0] = pose.pos.x;
                    world.pos[1] = pose.pos.y;
                    world.pos[2] = pose.pos.z;
                    std::copy_n(pose.eulerDeg, 3, world.rot);
                    const bool settingsChanged = state.settings != settings;
                    bool sourceChanged = false;
                    if (force || !state.initialized || settingsChanged || time - state.checkedAt >= 100)
                    {
                        std::array<float, 2> size;
                        const bool valid = SourceSize(*g_scene, index, settings, size);
                        sourceChanged = state.sourceValid != valid || state.sourceSize != size;
                        state.sourceValid = valid;
                        state.sourceSize = size;
                        state.checkedAt = time;
                    }
                    const auto signature = settings.type == LayoutType::Curve
                        ? Curves::Signature(*g_scene, g_scene->FindIndexById(settings.curveId))
                        : 0;
                    const bool changed = state.initialized ? settingsChanged || state.curve != signature || sourceChanged
                            || (settings.type == LayoutType::Curve && state.world != world)
                                                           : !g_baselineAutoApply;
                    state.settings = settings;
                    state.world = world;
                    state.curve = signature;
                    state.initialized = true;
                    if (changed && !state.changed)
                    {
                        state.triggerSequence = state.sequence;
                    }
                    state.sequence = g_sceneActions->UndoSequence();
                    state.changed |= changed;
                    if (!state.changed || (!force && time - state.appliedAt < 100))
                    {
                        continue;
                    }
                    try
                    {
                        g_autoActions.reserve(g_autoActions.size() + 1);
                        std::string error;
                        auto action = BuildAction(index, error);
                        const size_t overhead
                            = sizeof(CompositeAction) + g_autoActions.capacity() * sizeof(std::unique_ptr<IAction>);
                        if (action && g_autoBytes + action->MemoryUsage() + overhead > ActionManager::MaxHistoryBytes)
                        {
                            action.reset();
                            error = "Auto Apply exceeds the undo memory limit.";
                        }
                        if (action)
                        {
                            if (!guard)
                            {
                                guard = std::make_unique<SceneEditGuard>();
                            }
                            if (g_autoActions.empty())
                            {
                                g_autoSequence = state.triggerSequence;
                            }
                            action->Apply();
                            applied = true;
                            g_autoBytes += action->MemoryUsage();
                            g_autoActions.push_back(std::move(action));
                            state.settings = g_scene->data[g_scene->FindIndexById(id)].layout.Get();
                            state.sourceValid
                                = SourceSize(*g_scene, g_scene->FindIndexById(id), state.settings, state.sourceSize);
                        }
                        if (!error.empty())
                        {
                            GuiShowToast(error.c_str(), true);
                            failed = true;
                        }
                    }
                    catch (const std::bad_alloc&)
                    {
                        GuiShowToast("Not enough memory to apply the layout automatically.", true);
                        failed = true;
                    }
                    state.appliedAt = time;
                    state.changed = false;
                    if (failed)
                    {
                        break;
                    }
                }
                g_baselineAutoApply = false;
                force = true;
            } while (applied && !failed && ++passes < 64);
            if (applied && passes >= 64)
            {
                GuiShowToast("Layout dependencies exceeded the update limit.", true);
                failed = true;
            }
            if (failed)
            {
                guard.reset();
                ResetAutoApply();
                return false;
            }
            for (auto state = g_autoStates.begin(); state != g_autoStates.end();)
            {
                const int index = g_scene->FindIndexById(state->first);
                if (index < 0 || !g_scene->data[index].layout.HasValue())
                {
                    state = g_autoStates.erase(state);
                }
                else
                {
                    ++state;
                }
            }
            try
            {
                if (!CommitAutoApply())
                {
                    guard.reset();
                    ResetAutoApply();
                    return false;
                }
            }
            catch (const std::bad_alloc&)
            {
                guard.reset();
                ResetAutoApply();
                GuiShowToast("Not enough memory to record Auto Apply.", true);
                return false;
            }
            if (guard)
            {
                guard->Commit();
            }
            return !g_autoCommit;
        }
        catch (const std::bad_alloc&)
        {
            guard.reset();
            ResetAutoApply();
            GuiShowToast("Not enough memory to apply the layout automatically.", true);
            return false;
        }
    }

    void UpdateAutoApply(std::uint64_t time, bool editing)
    {
        g_autoTime = time;
        ApplyAutoLayouts(time, editing, false);
    }

    bool SettleAutoApply()
    {
        if (g_settlingAutoApply)
        {
            return true;
        }
        bool hasLayout = false;
        if (g_scene)
        {
            for (int index = 0; index < g_scene->count && !hasLayout; ++index)
            {
                hasLayout = g_scene->data[index].layout.HasValue();
            }
        }
        if (!hasLayout && g_autoActions.empty() && !g_autoCommit)
        {
            return true;
        }
        g_settlingAutoApply = true;
        try
        {
            const bool result = ApplyAutoLayouts(g_autoTime, false, true);
            g_settlingAutoApply = false;
            return result;
        }
        catch (...)
        {
            g_settlingAutoApply = false;
            throw;
        }
    }
}
