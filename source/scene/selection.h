// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

enum class GizmoPivot
{
    Pivot,
    Center
};
class CEntity;
class EditorScene;
struct CVector;
struct ImGuiIO;

namespace Selection
{
    int TotalCount();
    bool HasMovableRoots();
    void ClearAll();
    void HandleInput(ImGuiIO& io);
    void DeleteUnifiedSelection();
    void MoveToPoint(const CVector& point);
    void SnapToGround(bool alignToNormal);
    void UpdateGizmoBinding();
    void UpdateBoxAreaGizmoBinding();
    bool IsEntitySelected(CEntity* pEntity);
    void BeginRenderSelection();
    void EndRenderSelection();
    bool IsEntityHighlighted(CEntity* pEntity);
    bool GetDrawHighlight();
    void SetDrawHighlight(bool draw);
    bool ComputeBounds(CVector& outMinimum, CVector& outMaximum);
    bool ComputeChildrenBounds(const EditorScene& scene, int root, CVector& outMinimum, CVector& outMaximum);
    bool ComputeItemBounds(const EditorScene& scene, int root, const float rotation[3], CVector& outMinimum, CVector& outMaximum);
    bool ComputeFocusTarget(CVector& outCenter, float& outStandoff);

    GizmoPivot GetPivotMode();
    void TogglePivotMode();
}
