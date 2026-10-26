// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "entity_raycast.h"
#include "entity_ref.h"

enum class PickMode
{
    Static,
    Dynamic
};

class CDummyObject;
struct ImGuiIO;

class ScenePicker
{
    public:
    struct SelectedEntry
    {
        short nModelId = -1;
        eEntityType nType = ENTITY_TYPE_NOTHING;
        unsigned char nAreaCode = (unsigned char)-1;
        CVector posPivot = {};
        CVector posOrigin = {};
        Utils::RotationBasis rot = {};

        CVector posCentre = {};
        float radius = 0.0f;
        PickMode eMode = PickMode::Static;

        CEntity* lastEntity = nullptr;
        CEntity* lastEntityLOD = nullptr;
        short nModelIdLOD = -1;

        CVector posOriginLOD = {};
        CDummyObject* anchorDummy = nullptr;
        EntityRef::Kind refKind = EntityRef::Kind::None;
        EntityRef::Kind refKindLOD = EntityRef::Kind::None;
        int refEntity = -1;
        int refLOD = -1;
        int refAnchor = -1;
    };

    EntityRay::RayResult Pick(float sx, float sy, PickMode mode, float maxDist = 500.0f);
    void ToggleSelect(CEntity* pEntity, bool additive, PickMode mode);
    void ClearSelection();
    void DropOutOfAreaSelection();
    void OnEntityRemove(CEntity* pEntity);
    void OnEntityAdd(CEntity* pEntity);
    void OnEntityDestroy(CEntity* pEntity);
    void ValidateSelection();
    void HandleInput(ImGuiIO& io);
    bool IsSelected(CEntity* pEntity) const;
    const std::vector<SelectedEntry>& GetSelection() const;

    private:
    std::vector<SelectedEntry> m_selection;
    mutable std::unordered_set<CEntity*> m_selectedEntities;
    mutable std::unordered_map<short, std::vector<std::size_t>> m_unresolvedSelection;
    mutable unsigned m_selectionQueries = 0;
    mutable bool m_selectionIndexReady = false;
    PickMode m_activeMode = PickMode::Static;
    int m_lastArea = -1;

    bool MatchesEntry(const SelectedEntry& e, CEntity* pEnt) const;
    void InvalidateSelectionIndex();
    void BuildSelectionIndex() const;
    __forceinline bool IsSelectedLinear(CEntity* pEntity) const;
    bool IsSelectedIndexed(CEntity* pEntity) const;
    int FindInSelectionByPtr(CEntity* pHD) const;
    void AddEntry(CEntity* pEntity, PickMode mode);
    void RemoveAt(int idx);
    void BindEntity(SelectedEntry& e, CEntity* pEntity);
    bool TryResolve(SelectedEntry& e);
    void TryResolveLOD(SelectedEntry& e);

    static EntityRay::RayResult CastStatic(const CVector& origin, const CVector& dir, float maxDist);
    static EntityRay::RayResult CastDynamic(const CVector& origin, const CVector& dir, float maxDist);
};

extern ScenePicker* g_scenePicker;
