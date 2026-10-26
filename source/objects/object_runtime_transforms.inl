// Source Repository: https://github.com/waverso/waversomapstudio

void ObjectRuntime::ToggleSlide(int idx)
{
    ObjectRuntimeState& rt = m_itemStates[idx];
    if (rt.slide.playing)
    {
        StopSlide(idx);
        return;
    }
    if (rt.state != ObjectStreamState::Spawned || rt.scriptHandle < 0)
    {
        return;
    }
    if (!g_scene->data[idx].slide.HasValue())
    {
        return;
    }

    const EditorScene::WorldTransform& w = g_scene->world[idx];
    rt.slide.playing = true;
    rt.slide.curPos = w.pos;
    memcpy(rt.slide.curRot, w.eulerDeg, sizeof(rt.slide.curRot));
}

void ObjectRuntime::StopSlide(int idx)
{
    ObjectRuntimeState& rt = m_itemStates[idx];
    if (!rt.slide.playing)
    {
        return;
    }
    rt.slide.playing = false;

    if (rt.state == ObjectStreamState::Spawned && rt.scriptHandle >= 0)
    {
        const EditorScene::WorldTransform& w = g_scene->world[idx];
        plugin::Command<plugin::Commands::SET_OBJECT_COORDINATES>(rt.scriptHandle, w.pos.x, w.pos.y, w.pos.z);
        SetObjectRotation(rt.scriptHandle, rt.spawnedModelId, w.eulerDeg);
        rt.lastPos[0] = w.pos.x;
        rt.lastPos[1] = w.pos.y;
        rt.lastPos[2] = w.pos.z;
        memcpy(rt.lastRot, w.eulerDeg, sizeof(rt.lastRot));
    }
}

void ObjectRuntime::UpdateSlides(float dtSeconds)
{
    if (dtSeconds <= 0.0f)
    {
        return;
    }

    for (int i = 0; i < g_scene->count; ++i)
    {
        ObjectRuntimeState& rt = m_itemStates[i];
        if (!rt.slide.playing)
        {
            continue;
        }

        if (rt.state != ObjectStreamState::Spawned || rt.scriptHandle < 0 || !g_scene->data[i].slide.HasValue())
        {
            rt.slide.playing = false;
            continue;
        }

        const SlideData& sd = g_scene->data[i].slide.Get();
        const CVector target(sd.targetPos[0], sd.targetPos[1], sd.targetPos[2]);

        const float step = sd.speed * dtSeconds;
        const CVector diff = target - rt.slide.curPos;
        const float remaining = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (step >= remaining || remaining < 1e-4f)
        {
            StopSlide(i);
            continue;
        }

        const float ratio = remaining / step;
        rt.slide.curPos.x += diff.x / ratio;
        rt.slide.curPos.y += diff.y / ratio;
        rt.slide.curPos.z += diff.z / ratio;
        for (int k = 0; k < 3; ++k)
        {
            rt.slide.curRot[k] += (sd.targetRot[k] - rt.slide.curRot[k]) / ratio;
        }

        plugin::Command<plugin::Commands::SET_OBJECT_COORDINATES>(
            rt.scriptHandle, rt.slide.curPos.x, rt.slide.curPos.y, rt.slide.curPos.z);
        SetObjectRotation(rt.scriptHandle, rt.spawnedModelId, rt.slide.curRot);
    }
}

void ObjectRuntime::PushTransformIfChanged(int idx)
{
    const EditorScene::WorldTransform& w = g_scene->world[idx];
    ObjectRuntimeState& rt = m_itemStates[idx];

    const float wp[3] = { w.pos.x, w.pos.y, w.pos.z };
    const bool posChanged = memcmp(rt.lastPos, wp, sizeof(rt.lastPos)) != 0;
    const bool rotChanged = memcmp(rt.lastRot, w.eulerDeg, sizeof(rt.lastRot)) != 0;

    if (rt.slide.playing)
    {
        if (!posChanged && !rotChanged)
        {
            return;
        }
        StopSlide(idx);
    }
    else
    {
        if (posChanged)
        {
            plugin::Command<plugin::Commands::SET_OBJECT_COORDINATES>(rt.scriptHandle, wp[0], wp[1], wp[2]);
            memcpy(rt.lastPos, wp, sizeof(rt.lastPos));
        }
        if (rotChanged || posChanged)
        {
            SetObjectRotation(rt.scriptHandle, rt.spawnedModelId, w.eulerDeg);
            memcpy(rt.lastRot, w.eulerDeg, sizeof(rt.lastRot));
        }
    }

    if (rt.obj && ModelChecks::IsEscalatorBaseModel(g_scene->data[idx].model.Get().modelId)
        && !(g_gizmos && g_gizmos->IsDragging()))
    {
        const bool editing = ImGui::GetCurrentContext() && ImGui::IsAnyItemActive();
        if ((posChanged || rotChanged) && !rt.escalatorDirty)
        {
            rt.escalatorDirty = true;
            if (editing)
            {
                rt.obj->RemoveEscalatorsForEntity();
            }
        }
        if (rt.escalatorDirty && !editing)
        {
            rt.obj->RemoveEscalatorsForEntity();
            rt.obj->CreateEffects();
            rt.escalatorDirty = false;
        }
    }
}
