// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "curve_geometry.h"

class EditorScene;
struct CVector;
struct ItemData;
enum class CurveHandle;

namespace Curves
{
    struct Marker
    {
        std::array<float, 3> position;
        float radius;
        std::uint32_t color;
    };

    bool CanUse(const EditorScene& scene, int layout, int curve);
    const CurveGeometry::Path* Resolve(const EditorScene& scene, int curve, std::string& error);
    bool ComputeBounds(const EditorScene& scene, int curve, CVector& minimum, CVector& maximum);
    std::uint64_t Signature(const EditorScene& scene, int curve);
    bool Handles(const EditorScene& scene, int node, CVector& incoming, CVector& outgoing);
    void SetNodeMode(const EditorScene& scene, int node, ItemData& data, CurveHandle mode);
    void Synchronize(EditorScene& scene) noexcept;
    bool CanSubdivide(const EditorScene& scene);
    bool SubdivideSelection();
    void SetHandle(int nodeId, int handle);
    bool GetHandlePosition(CVector& position);
    void MoveHandleTo(const CVector& position);
    void AlignHandleTo(const CVector& normal);
    bool UpdateHandleGizmoBinding();
    void ClearHover();
    bool UpdateHover(const CVector& origin, const CVector& direction, bool selectedOnly = false);
    int Pick(const CVector& origin, const CVector& direction, float& distance);
    void BuildDisplay(const EditorScene& scene, std::vector<std::array<float, 7>>& lines, std::vector<Marker>& markers);
    void Reset();
}
