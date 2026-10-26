// Source Repository: https://github.com/waverso/waversomapstudio

#include "picker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <vector>

#include "../vendor/imgui/imgui.h"
#include "CDummyObject.h"
#include "CEntity.h"
#include "CGame.h"
#include "CMatrix.h"
#include "CObject.h"
#include "CPools.h"
#include "CTimer.h"
#include "box_area.h"
#include "../layout/curves.h"
#include "entity_raycast.h"
#include "entity_ref.h"
#include "free_camera.h"
#include "gizmos.h"
#include "../ui/gui.h"
#include "../rendering/model_preview.h"
#include "../objects/object_browser.h"
#include "../objects/object_runtime.h"
#include "../pickups/pickup_runtime.h"
#include "../races/races.h"
#include "../objects/removal_manager.h"
#include "../hooks/samp.h"
#include "scene.h"
#include "scene_input.h"
#include "scene_limits.h"
#include "selection.h"

namespace
{
    int PickSceneArea(const CVector& origin, const CVector& direction, float& maxDistance)
    {
        if (!g_scene)
        {
            return -1;
        }
        const float rayOrigin[3] = { origin.x, origin.y, origin.z };
        const double length = std::sqrt(static_cast<double>(direction.x) * direction.x
            + static_cast<double>(direction.y) * direction.y + static_cast<double>(direction.z) * direction.z);
        if (length < 1e-8f)
        {
            return -1;
        }
        const float rayDirection[3] = { static_cast<float>(direction.x / length), static_cast<float>(direction.y / length),
            static_cast<float>(direction.z / length) };
        int closest = -1;
        for (int index = 0; index < g_scene->count; ++index)
        {
            const auto& data = g_scene->data[index];
            const auto& position = g_scene->world[index].pos;
            const float center[3] = { position.x, position.y, position.z };
            float distance = -1.0f;
            if (data.boxArea.HasValue())
            {
                BoxArea::IntersectRay(data.boxArea.Get(), center, rayOrigin, rayDirection, maxDistance, distance);
            }
            else if (data.removeBuilding.HasValue() && data.removeBuilding.Get().custom && data.removeBuilding.Get().radius > 0)
            {
                const double x = static_cast<double>(position.x) - origin.x, y = static_cast<double>(position.y) - origin.y,
                             z = static_cast<double>(position.z) - origin.z;
                const double along = (x * direction.x + y * direction.y + z * direction.z) / length;
                const double radius = data.removeBuilding.Get().radius;
                const double discriminant = radius * radius - (x * x + y * y + z * z - along * along);
                if (discriminant >= 0)
                {
                    const double root = std::sqrt(discriminant);
                    distance = static_cast<float>(along >= root ? along - root : along + root);
                }
            }
            if (distance >= 0 && distance <= maxDistance)
            {
                maxDistance = distance;
                closest = index;
            }
        }
        return closest;
    }

    bool IsPickerExcludedEntity(CEntity* pEntity)
    {
        return pEntity && pEntity->m_nType == ENTITY_TYPE_OBJECT
            && (!g_pickupRuntime || g_pickupRuntime->LookupItemIndex(pEntity) < 0)
            && (SampIsServerObject((CObject*)pEntity) || PickupIsPickupObject((CObject*)pEntity));
    }

    bool IsBindableEntity(CEntity* pEntity)
    {
        if (!pEntity)
        {
            return false;
        }
        if (g_pickupRuntime && g_pickupRuntime->IsSpawning())
        {
            return false;
        }
        if (g_objectRuntime && (g_objectRuntime->IsSpawning() || LookupEditorItemIndex(pEntity) >= 0))
        {
            return false;
        }
        if (pEntity->m_nType == ENTITY_TYPE_OBJECT
            && (SampIsServerObject((CObject*)pEntity) || PickupIsPickupObject((CObject*)pEntity)))
        {
            return false;
        }
        return true;
    }
}

EntityRay::RayResult ScenePicker::CastStatic(const CVector& origin, const CVector& dir, float maxDist)
{
    return EntityRay::Cast(
        origin, dir, maxDist, EntityRay::LodCastMode::IgnoreHidden, EntityRay::ScreenCulls, IsPickerExcludedEntity);
}

EntityRay::RayResult ScenePicker::CastDynamic(const CVector& origin, const CVector& dir, float maxDist)
{
    return EntityRay::RayResult {};
}

EntityRay::RayResult ScenePicker::Pick(float sx, float sy, PickMode mode, float maxDist)
{
    CVector origin, dir;
    if (!g_freeCamera || !g_freeCamera->ScreenToWorldRayForPicking(sx, sy, origin, dir))
    {
        return EntityRay::RayResult {};
    }
    return mode == PickMode::Static ? CastStatic(origin, dir, maxDist) : CastDynamic(origin, dir, maxDist);
}

bool ScenePicker::MatchesEntry(const SelectedEntry& e, CEntity* pEnt) const
{
    if (!pEnt)
    {
        return false;
    }
    if (pEnt->m_nModelIndex != e.nModelId)
    {
        return false;
    }
    if (pEnt->m_nType != e.nType && e.nType != ENTITY_TYPE_OBJECT && e.nType != ENTITY_TYPE_DUMMY)
    {
        return false;
    }
    if (pEnt->m_nAreaCode != e.nAreaCode)
    {
        return false;
    }

    const CVector pos = g_removalManager ? g_removalManager->OriginOf(pEnt) : pEnt->GetPosition();
    float dx = pos.x - e.posOrigin.x;
    float dy = pos.y - e.posOrigin.y;
    float dz = pos.z - e.posOrigin.z;
    return (dx * dx + dy * dy + dz * dz) <= (SceneLimits::PositionMatchEpsilon * SceneLimits::PositionMatchEpsilon);
}

int ScenePicker::FindInSelectionByPtr(CEntity* pHD) const
{
    for (int i = 0; i < (int)m_selection.size(); ++i)
    {
        const SelectedEntry& e = m_selection[i];
        if (e.lastEntity == pHD || e.lastEntityLOD == pHD)
        {
            return i;
        }
    }
    return -1;
}

void ScenePicker::InvalidateSelectionIndex()
{
    m_selectionIndexReady = false;
    m_selectionQueries = 0;
}

void ScenePicker::RemoveAt(int idx)
{
    InvalidateSelectionIndex();
    m_selection.erase(m_selection.begin() + idx);
    if (m_selection.empty())
    {
        m_activeMode = PickMode::Static;
    }
}

void ScenePicker::AddEntry(CEntity* pEntity, PickMode mode)
{
    for (const auto& e : m_selection)
    {
        if (e.lastEntity == pEntity || MatchesEntry(e, pEntity))
        {
            return;
        }
    }
    if (mode == PickMode::Static && pEntity->GetPosition().IsZero())
    {
        return;
    }

    SelectedEntry e;
    e.nModelId = (short)pEntity->m_nModelIndex;
    e.nType = (eEntityType)pEntity->m_nType;
    e.nAreaCode = pEntity->m_nAreaCode;
    e.posPivot = mode == PickMode::Static || pEntity->m_placement.m_vPosn.IsZero() ? pEntity->GetPosition()
                                                                                   : pEntity->m_placement.m_vPosn;
    e.posOrigin = g_removalManager ? g_removalManager->OriginOf(pEntity) : pEntity->GetPosition();
    e.posCentre = pEntity->GetBoundCentre();
    e.radius = pEntity->GetBoundRadius();
    const ObjectBrowser::ModelInfo visualModel = ObjectBrowser::GetModel(e.nModelId);
    ModelPreview::Profile visualProfile;
    if (visualModel.manualCenter && ModelPreview::GetProfile(e.nModelId, visualProfile))
    {
        const CVector local(visualProfile.target[0], visualProfile.target[1], visualProfile.target[2]);
        if (pEntity->m_matrix)
        {
            const CMatrix& matrix = *pEntity->m_matrix;
            e.posCentre = matrix.pos + matrix.right * local.x + matrix.up * local.y + matrix.at * local.z;
        }
        else
        {
            const float heading = pEntity->m_placement.m_fHeading;
            const float c = cosf(heading), s = sinf(heading);
            e.posCentre = pEntity->GetPosition() + CVector(local.x * c - local.y * s, local.x * s + local.y * c, local.z);
        }
        e.radius = std::max(0.05f, visualModel.radius);
    }
    e.eMode = mode;

    if (pEntity->m_matrix)
    {
        e.rot.right = pEntity->m_matrix->right;
        e.rot.forward = pEntity->m_matrix->up;
        e.rot.up = pEntity->m_matrix->at;
    }
    else
    {
        const float h = pEntity->m_placement.m_fHeading;
        const float c = cosf(h), s = sinf(h);
        e.rot.right = CVector(c, s, 0.0f);
        e.rot.forward = CVector(-s, c, 0.0f);
        e.rot.up = CVector(0.0f, 0.0f, 1.0f);
    }

    if (pEntity->m_pLod)
    {
        e.nModelIdLOD = (short)pEntity->m_pLod->m_nModelIndex;
        e.posOriginLOD = pEntity->m_pLod->GetPosition();
    }

    BindEntity(e, pEntity);

    m_selection.push_back(e);
}

void ScenePicker::ToggleSelect(CEntity* pEntity, bool additive, PickMode mode)
{
    if (!pEntity)
    {
        return;
    }

    const int curArea = CGame::currArea;
    const bool isLod = EntityRay::IsLowLOD(pEntity);

    if (additive && !m_selection.empty() && curArea != m_lastArea)
    {
        ClearSelection();
        additive = false;
    }

    if (!additive)
    {
        m_selection.clear();
        InvalidateSelectionIndex();
        m_activeMode = mode;
        AddEntry(pEntity, mode);
        m_lastArea = curArea;
        return;
    }

    if (!m_selection.empty() && m_activeMode != mode)
    {
        return;
    }
    m_activeMode = mode;

    CEntity* lodPtr = isLod ? pEntity : pEntity->m_pLod;
    if (lodPtr)
    {
        for (int i = (int)m_selection.size() - 1; i >= 0; --i)
        {
            auto& se = m_selection[i];
            bool eIsLod = EntityRay::IsLowLOD(se.lastEntity);
            if (!isLod && eIsLod && se.lastEntity == lodPtr)
            {
                RemoveAt(i);
                continue;
            }
            if (isLod && !eIsLod && se.lastEntityLOD == lodPtr)
            {
                RemoveAt(i);
                continue;
            }
        }
    }

    int idx = -1;
    for (int i = 0; i < (int)m_selection.size(); ++i)
    {
        if (m_selection[i].lastEntity == pEntity || MatchesEntry(m_selection[i], pEntity))
        {
            idx = i;
            break;
        }
    }

    if (idx >= 0)
    {
        RemoveAt(idx);
    }
    else
    {
        AddEntry(pEntity, mode);
    }

    m_lastArea = curArea;
}

void ScenePicker::ClearSelection()
{
    m_selection.clear();
    InvalidateSelectionIndex();
    m_activeMode = PickMode::Static;
}

void ScenePicker::DropOutOfAreaSelection()
{
    const auto previousSize = m_selection.size();
    const unsigned char area = (unsigned char)CGame::currArea;
    m_selection.erase(std::remove_if(m_selection.begin(), m_selection.end(),
                          [area](const SelectedEntry& e)
                          {
                              return e.nAreaCode != area && e.nAreaCode != 13;
                          }),
        m_selection.end());
    if (m_selection.size() != previousSize)
    {
        InvalidateSelectionIndex();
    }
}

void ScenePicker::BindEntity(SelectedEntry& e, CEntity* pEntity)
{
    InvalidateSelectionIndex();
    e.lastEntity = pEntity;
    e.refKind = EntityRef::KindOf(pEntity);
    e.refEntity = EntityRef::Make(pEntity, e.refKind);

    if (pEntity->m_nType == ENTITY_TYPE_OBJECT)
    {
        CDummyObject* d = static_cast<CObject*>(pEntity)->m_pDummyObject;
        if (d)
        {
            e.anchorDummy = d;
            e.refAnchor = EntityRef::Make(reinterpret_cast<CEntity*>(d), EntityRef::Kind::Dummy);
        }
    }
    else if (pEntity->m_nType == ENTITY_TYPE_DUMMY)
    {
        e.anchorDummy = reinterpret_cast<CDummyObject*>(pEntity);
        e.refAnchor = EntityRef::Make(pEntity, EntityRef::Kind::Dummy);
    }

    if (pEntity->m_pLod && e.nModelIdLOD == (short)pEntity->m_pLod->m_nModelIndex)
    {
        e.lastEntityLOD = pEntity->m_pLod;
        e.refKindLOD = EntityRef::KindOf(pEntity->m_pLod);
        e.refLOD = EntityRef::Make(pEntity->m_pLod, e.refKindLOD);
    }
}

void ScenePicker::OnEntityRemove(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    for (auto& e : m_selection)
    {
        if (e.lastEntity == pEntity)
        {
            InvalidateSelectionIndex();
            e.lastEntity = nullptr;
            e.refEntity = -1;
            e.refKind = EntityRef::Kind::None;
        }
        if (e.lastEntityLOD == pEntity)
        {
            InvalidateSelectionIndex();
            e.lastEntityLOD = nullptr;
            e.refLOD = -1;
            e.refKindLOD = EntityRef::Kind::None;
        }
    }
}

void ScenePicker::OnEntityDestroy(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    for (auto& e : m_selection)
    {
        if (reinterpret_cast<CEntity*>(e.anchorDummy) == pEntity)
        {
            e.anchorDummy = nullptr;
            e.refAnchor = -1;
        }
    }
}

void ScenePicker::OnEntityAdd(CEntity* pEntity)
{
    if (!pEntity || m_selection.empty())
    {
        return;
    }
    if (!IsBindableEntity(pEntity))
    {
        return;
    }

    for (auto& e : m_selection)
    {
        const bool anchorHit = e.anchorDummy
            && (reinterpret_cast<CEntity*>(e.anchorDummy) == pEntity
                || (pEntity->m_nType == ENTITY_TYPE_OBJECT && static_cast<CObject*>(pEntity)->m_pDummyObject == e.anchorDummy));
        if (anchorHit)
        {
            BindEntity(e, pEntity);
            continue;
        }

        if (!e.lastEntity && !e.anchorDummy && MatchesEntry(e, pEntity))
        {
            BindEntity(e, pEntity);
            continue;
        }

        if (e.nModelIdLOD >= 0 && !e.lastEntityLOD && pEntity->m_nModelIndex == e.nModelIdLOD)
        {
            const CVector d = pEntity->GetPosition() - e.posOriginLOD;
            if ((d.x * d.x + d.y * d.y + d.z * d.z) <= (SceneLimits::PositionMatchEpsilon * SceneLimits::PositionMatchEpsilon))
            {
                InvalidateSelectionIndex();
                e.lastEntityLOD = pEntity;
                e.refKindLOD = EntityRef::KindOf(pEntity);
                e.refLOD = EntityRef::Make(pEntity, e.refKindLOD);
            }
        }
    }
}

bool ScenePicker::TryResolve(SelectedEntry& e)
{
    auto scanPool = [&](auto* pool) -> CEntity*
    {
        if (!pool)
        {
            return nullptr;
        }
        for (int i = 0; i < pool->m_nSize; ++i)
        {
            if (pool->m_byteMap[i].bEmpty)
            {
                continue;
            }
            CEntity* ent = (CEntity*)pool->GetAt(i);
            if (!ent || ent->m_nModelIndex != e.nModelId)
            {
                continue;
            }
            if (!IsBindableEntity(ent))
            {
                continue;
            }
            if (!MatchesEntry(e, ent))
            {
                continue;
            }
            return ent;
        }
        return nullptr;
    };

    if (e.anchorDummy)
    {
        auto* pool = CPools::ms_pObjectPool;
        if (pool)
        {
            for (int i = 0; i < pool->m_nSize; ++i)
            {
                if (pool->m_byteMap[i].bEmpty)
                {
                    continue;
                }
                CObject* obj = pool->GetAt(i);
                if (!obj || obj->m_pDummyObject != e.anchorDummy)
                {
                    continue;
                }
                if (!IsBindableEntity(obj))
                {
                    continue;
                }
                BindEntity(e, obj);
                return true;
            }
        }
        if (MatchesEntry(e, reinterpret_cast<CEntity*>(e.anchorDummy)))
        {
            BindEntity(e, reinterpret_cast<CEntity*>(e.anchorDummy));
            return true;
        }
        return false;
    }

    if (e.nType == ENTITY_TYPE_BUILDING)
    {
        if (CEntity* ent = scanPool(CPools::ms_pBuildingPool))
        {
            BindEntity(e, ent);
            return true;
        }
        return false;
    }

    if (CEntity* ent = scanPool(CPools::ms_pObjectPool))
    {
        BindEntity(e, ent);
        return true;
    }
    if (CEntity* ent = scanPool(CPools::ms_pDummyPool))
    {
        BindEntity(e, ent);
        return true;
    }
    return false;
}

void ScenePicker::TryResolveLOD(SelectedEntry& e)
{
    auto* pool = CPools::ms_pBuildingPool;
    if (!pool)
    {
        return;
    }
    for (int i = 0; i < pool->m_nSize; ++i)
    {
        if (pool->m_byteMap[i].bEmpty)
        {
            continue;
        }
        CEntity* ent = (CEntity*)pool->GetAt(i);
        if (!ent || ent->m_nModelIndex != e.nModelIdLOD)
        {
            continue;
        }
        const CVector d = ent->GetPosition() - e.posOriginLOD;
        if ((d.x * d.x + d.y * d.y + d.z * d.z) > (SceneLimits::PositionMatchEpsilon * SceneLimits::PositionMatchEpsilon))
        {
            continue;
        }
        InvalidateSelectionIndex();
        e.lastEntityLOD = ent;
        e.refKindLOD = EntityRef::KindOf(ent);
        e.refLOD = EntityRef::Make(ent, e.refKindLOD);
        return;
    }
}

void ScenePicker::ValidateSelection()
{
    if (m_selection.empty())
    {
        return;
    }

    bool needResolve = false;
    for (auto& e : m_selection)
    {
        if (e.lastEntity && !EntityRef::Alive(e.lastEntity, e.refKind, e.refEntity))
        {
            InvalidateSelectionIndex();
            e.lastEntity = nullptr;
            e.refEntity = -1;
            e.refKind = EntityRef::Kind::None;
        }
        if (e.lastEntityLOD && !EntityRef::Alive(e.lastEntityLOD, e.refKindLOD, e.refLOD))
        {
            InvalidateSelectionIndex();
            e.lastEntityLOD = nullptr;
            e.refLOD = -1;
            e.refKindLOD = EntityRef::Kind::None;
        }
        if (e.anchorDummy && !EntityRef::Alive(reinterpret_cast<CEntity*>(e.anchorDummy), EntityRef::Kind::Dummy, e.refAnchor))
        {
            e.anchorDummy = nullptr;
            e.refAnchor = -1;
        }
        if (e.eMode != PickMode::Static)
        {
            continue;
        }
        if (!e.lastEntity || (e.nModelIdLOD >= 0 && !e.lastEntityLOD))
        {
            needResolve = true;
        }
    }

    if (!needResolve)
    {
        return;
    }
    if (CTimer::m_FrameCounter % 4 != 0)
    {
        return;
    }

    for (auto& e : m_selection)
    {
        if (e.eMode != PickMode::Static)
        {
            continue;
        }
        if (!e.lastEntity)
        {
            TryResolve(e);
        }
        if (e.nModelIdLOD >= 0 && !e.lastEntityLOD)
        {
            TryResolveLOD(e);
        }
    }
}

void ScenePicker::BuildSelectionIndex() const
{
    const short model = m_selection.front().nModelId;
    if (std::all_of(m_selection.begin(), m_selection.end(),
            [model](const SelectedEntry& entry)
            {
                return !entry.lastEntity && entry.nModelId == model;
            }))
    {
        return;
    }
    try
    {
        m_selectedEntities.clear();
        m_unresolvedSelection.clear();
        m_selectedEntities.reserve(m_selection.size() * 2);
        m_unresolvedSelection.reserve(m_selection.size());
        for (std::size_t index = 0; index < m_selection.size(); ++index)
        {
            const auto& entry = m_selection[index];
            if (entry.lastEntity)
            {
                m_selectedEntities.insert(entry.lastEntity);
            }
            else
            {
                m_unresolvedSelection.try_emplace(entry.nModelId, 0).first->second.push_back(index);
            }
            if (entry.lastEntityLOD)
            {
                m_selectedEntities.insert(entry.lastEntityLOD);
            }
        }
        m_selectionIndexReady = true;
    }
    catch (const std::bad_alloc&)
    {
        m_selectedEntities.clear();
        m_unresolvedSelection.clear();
    }
}

bool ScenePicker::IsSelectedLinear(CEntity* pEntity) const
{
    for (const auto& e : m_selection)
    {
        if (e.lastEntity == pEntity || e.lastEntityLOD == pEntity)
        {
            return true;
        }
        if (e.lastEntity == nullptr && MatchesEntry(e, pEntity))
        {
            return true;
        }
    }
    return false;
}

bool ScenePicker::IsSelectedIndexed(CEntity* pEntity) const
{
    if (!m_selectionIndexReady && m_selectionQueries < 257 && ++m_selectionQueries == 257)
    {
        BuildSelectionIndex();
    }
    if (!m_selectionIndexReady)
    {
        return IsSelectedLinear(pEntity);
    }
    if (m_selectedEntities.contains(pEntity))
    {
        return true;
    }
    const auto found = m_unresolvedSelection.find(static_cast<short>(pEntity->m_nModelIndex));
    if (found != m_unresolvedSelection.end())
    {
        for (const auto index : found->second)
        {
            if (MatchesEntry(m_selection[index], pEntity))
            {
                return true;
            }
        }
    }
    return false;
}

void ScenePicker::HandleInput(ImGuiIO& io)
{
    Curves::ClearHover();
    if (!g_freeCamera->m_active)
    {
        return;
    }
    if (g_gui && g_gui->IsDragCursorInputBlocked())
    {
        return;
    }

    if (io.WantCaptureMouse || io.WantCaptureKeyboard || (io.MouseDown[0] && io.MouseDownOwned[0]) || ImGui::IsAnyItemActive()
        || SampTextInputActive() || g_freeCamera->m_moving || g_freeCamera->m_panHasAnchor
        || (g_gui
            && (g_gui->m_sidebarInteracting || g_gui->IsMouseOverGizmoPanel() || g_gui->IsMouseOverUtilityPanel()
                || g_gui->IsMouseOverToolbar())))
    {
        return;
    }
    if (g_gizmos && g_gizmos->IsHoveringOrDragging())
    {
        return;
    }
    CVector origin, direction;
    if (!g_freeCamera->ScreenToWorldRayForPicking(io.MousePos.x, io.MousePos.y, origin, direction))
    {
        return;
    }
    const bool clicked = SceneInput::IsMouseClicked(ImGuiMouseButton_Left);
    const bool curveHover = (!io.MouseDown[0] || clicked) && Curves::UpdateHover(origin, direction);
    if (!clicked)
    {
        return;
    }
    SceneInput::ConsumeMouseClick(ImGuiMouseButton_Left);
    const bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool additive = shiftHeld || ctrlHeld;
    EntityRay::RayResult hit;
    int itemIdx = -1;
    if (curveHover)
    {
        float distance = (std::numeric_limits<float>::max)();
        itemIdx = Curves::Pick(origin, direction, distance);
        if (itemIdx >= 0 && g_scene->SingleSelectedIndex() == itemIdx && g_scene->SelectionCount() == 1)
        {
            if (g_gui)
            {
                g_gui->RequestProperties();
            }
            Curves::UpdateHandleGizmoBinding();
            return;
        }
    }
    else
    {
        hit = CastStatic(origin, direction, 500.0f);
        itemIdx = hit.pEntity ? LookupEditorItemIndex(hit.pEntity) : -1;
        float checkpointDistance = std::min(500.0f, hit.fHitDistance);
        const int areaIdx = PickSceneArea(origin, direction, checkpointDistance);
        if (areaIdx >= 0)
        {
            itemIdx = areaIdx;
        }
        const int checkpoint = Races::Pick(origin, direction, checkpointDistance);
        if (checkpoint >= 0)
        {
            itemIdx = checkpoint;
        }
        const int curve = Curves::Pick(origin, direction, checkpointDistance);
        if (curve >= 0)
        {
            itemIdx = curve;
        }
    }
    if (itemIdx >= 0 && g_scene)
    {
        SceneItem& item = g_scene->items[itemIdx];
        if (additive)
        {
            item.selected = !item.selected;
        }
        else
        {
            ClearSelection();
            GuiClearSceneSelection();
            item.selected = true;
        }
        for (int parent = item.parent; parent >= 0; parent = g_scene->items[parent].parent)
        {
            g_scene->items[parent].open = true;
        }
    }
    else if (hit.pEntity)
    {
        if (!additive)
        {
            GuiClearSceneSelection();
        }
        ToggleSelect(hit.pEntity, additive, PickMode::Static);
    }
    else if (!additive)
    {
        Selection::ClearAll();
    }
    if (g_gui
        && ((itemIdx >= 0 && g_scene && g_scene->items[itemIdx].selected)
            || (itemIdx < 0 && hit.pEntity && IsSelected(hit.pEntity))))
    {
        g_gui->RequestProperties();
    }
    if (curveHover)
    {
        Curves::UpdateHandleGizmoBinding();
    }
}

bool ScenePicker::IsSelected(CEntity* pEntity) const
{
    const auto count = m_selection.size();
    if (!pEntity || count == 0)
    {
        return false;
    }
    if (count == 1)
    {
        const auto& entry = m_selection.front();
        return entry.lastEntity == pEntity || entry.lastEntityLOD == pEntity
            || (!entry.lastEntity && MatchesEntry(entry, pEntity));
    }
#ifdef _DEBUG
    constexpr std::size_t minimumIndexedSelection = 1024;
#else
    constexpr std::size_t minimumIndexedSelection = 16;
#endif
    return count < minimumIndexedSelection && !m_selectionIndexReady ? IsSelectedLinear(pEntity) : IsSelectedIndexed(pEntity);
}

const std::vector<ScenePicker::SelectedEntry>& ScenePicker::GetSelection() const
{
    return m_selection;
}
