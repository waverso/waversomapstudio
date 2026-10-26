// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    void DrawCheckpoint(
        Pass& pass, int slot, int id, const CVector& position, const CVector& next, float radius, int type, bool selected)
    {
        const unsigned char alpha = type <= 2 ? 32 : 96;
        const ImU32 accent = Theme::g_accent;
        CRGBA color = selected ? CRGBA((accent >> IM_COL32_R_SHIFT) & 255, (accent >> IM_COL32_G_SHIFT) & 255,
                                     (accent >> IM_COL32_B_SHIFT) & 255, alpha)
                               : CRGBA(255, 0, 0, alpha);
        CVector normal = Direction(position, next);
        CVector center = position;
        auto& visual = g_visuals[slot];
        auto* player = FindPlayerPed();
        if (player)
        {
            visual.Update(id, type, position, normal, radius, player->GetPosition());
        }
        if (type >= 5 && type <= 8)
        {
            if (player)
            {
                type = visual.motion.type;
                if (type < 0)
                {
                    return;
                }
                center.z = visual.motion.height;
                if (type == 5)
                {
                    normal = visual.direction;
                }
            }
        }
        if (type <= 2)
        {
            if (type == 1)
            {
                DrawFinish(pass, position, radius, color);
            }
            if (pass.picking
                || (g_tube.Ensure(MARKER3D_TUBE)
                    && GroundBoundsVisible(g_tube.minimum, g_tube.maximum, D3DXVECTOR3(position.x, position.y, position.z),
                        radius * 1.5f, pass.view * pass.projection)))
            {
                CVector ground = position;
                bool found = false;
                const float height = CWorld::FindGroundZFor3DCoord(position.x, position.y, position.z + 1.0f, &found, nullptr);
                if (found)
                {
                    ground.z = height - radius * 1.5f / 20.0f;
                }
                color.a = 128;
                DrawMarker(pass, g_tube, MARKER3D_TUBE, ground, CVector(0, 0, 1), radius * 1.5f, color, visual.rotation);
            }
            if (type == 0)
            {
                const CRGBA outline(0, 0, 0, 255);
                color = selected ? CRGBA(color.r, color.g, color.b, 255) : CRGBA(255, 64, 64, 255);
                const CVector arrowPosition = position + CVector(0, 0, 3);
                DrawMarker(pass, g_arrow, MARKER3D_CONE, arrowPosition, normal * -1.0f, radius * 0.625f, outline);
                DrawMarker(pass, g_arrow, MARKER3D_CONE, arrowPosition, normal * -1.0f, radius * 0.5f, color);
            }
        }
        else
        {
            if (type == 6)
            {
                const float spacing = visual.motion.amount;
                for (int layer = 3; layer > 0; --layer)
                {
                    CRGBA ghost = color;
                    ghost.a = static_cast<unsigned char>(std::max(0, color.a - layer * 16));
                    DrawMarker(pass, g_ring, MARKER3D_TORUS, center + normal * (spacing * layer), normal, radius, ghost);
                }
            }
            DrawMarker(pass, g_ring, MARKER3D_TORUS, center, normal, radius, color);
        }
    }

    void DrawScene(Pass& pass)
    {
        for (const int parent : g_scene->DisplayOrder())
        {
            if (!g_scene->data[parent].race.HasValue())
            {
                continue;
            }
            const auto children = g_scene->Children(parent);
            const auto& race = g_scene->data[parent].race.Get();
            const int count = static_cast<int>(children.size());
            for (int ordinal = 0; ordinal < count; ++ordinal)
            {
                const int index = children[ordinal];
                const auto& cp = g_scene->data[index].raceCheckpoint.Get();
                const bool finish = race.mode == 0 ? ordinal + 1 == count : ordinal == 0;
                int type = cp.type;
                if (type == RaceCheckpointData::AutoGround)
                {
                    type = finish ? 1 : 0;
                }
                else if (type == RaceCheckpointData::AutoAir)
                {
                    type = finish ? 4 : 3;
                }
                const CVector position = g_scene->world[index].pos;
                const int next = ordinal + 1 < count ? ordinal + 1 : race.mode == 1 ? 0 : -1;
                const CVector direction = next >= 0
                    ? g_scene->world[children[next]].pos
                    : position + (ordinal ? position - g_scene->world[children[ordinal - 1]].pos : CVector(0, 1, 0));
                pass.item = index;
                DrawCheckpoint(pass, index, g_scene->items[index].id, position, direction, cp.radius, type,
                    g_scene->items[index].selected || g_scene->HasSelectedAncestor(index));
            }
        }
    }
}

void Render()
{
    if (!g_scene || !g_gameHooks || !g_gameHooks->m_device)
    {
        return;
    }
    const bool editing = g_freeCamera && g_freeCamera->m_active;
    if (!editing && (!g_simulation.Active() || g_startDelay))
    {
        return;
    }
    if (editing
        && std::none_of(g_scene->DisplayOrder().begin(), g_scene->DisplayOrder().end(),
            [](int index)
            {
                return g_scene->data[index].raceCheckpoint.HasValue();
            }))
    {
        return;
    }
    Pass pass { g_gameHooks->m_device };
    if (!pass.Initialize())
    {
        return;
    }
    RenderState state(pass.device);
    if (!state.block)
    {
        return;
    }
    pass.Begin();
    if (!editing)
    {
        const int index = g_simulation.target;
        int next = index + 1;
        if (g_simulation.FinalTarget())
        {
            next = -1;
        }
        else if (next == static_cast<int>(g_checkpoints.size()))
        {
            next = g_simulation.mode == 1 ? 0 : -1;
        }
        const auto& target = g_checkpoints[index];
        const int previous = index > 0 ? index - 1 : g_simulation.mode == 1 ? static_cast<int>(g_checkpoints.size()) - 1 : -1;
        const CVector direction = next >= 0
            ? g_checkpoints[next].position
            : target.position + (previous >= 0 ? target.position - g_checkpoints[previous].position : CVector(0, 1, 0));
        DrawCheckpoint(
            pass, index, index, target.position, direction, target.data.radius, g_simulation.Type(target.data.type), false);
        return;
    }
    DrawScene(pass);
}

int Pick(const CVector& origin, const CVector& direction, float& maxDistance)
{
    const float lengthSquared = Utils::Dot3(direction, direction);
    if (!g_scene || !g_gameHooks || !std::isfinite(lengthSquared) || lengthSquared <= 0.0f)
    {
        return -1;
    }
    Pass pass { g_gameHooks->m_device };
    if (!pass.Initialize())
    {
        return -1;
    }
    pass.picking = true;
    pass.origin = D3DXVECTOR3(origin.x, origin.y, origin.z);
    pass.direction = D3DXVECTOR3(direction.x, direction.y, direction.z) * (1.0f / std::sqrt(lengthSquared));
    pass.distance = maxDistance;
    DrawScene(pass);
    maxDistance = pass.distance;
    return pass.hit;
}
