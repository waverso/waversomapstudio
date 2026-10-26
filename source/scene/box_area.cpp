// Source Repository: https://github.com/waverso/waversomapstudio

#include "box_area.h"

#include "CMenuManager.h"
#include "CRadar.h"
#include "free_camera.h"
#include "scene.h"

void BoxArea::DrawGangZones()
{
    if (!g_scene || !g_scene->count || (g_freeCamera && g_freeCamera->m_active))
    {
        return;
    }
    const bool inMenu = FrontEndMenuManager.m_bDrawRadarOrMap;
    RwUInt32 depthTest = FALSE;
    bool depthChanged = false;
    for (const int index : g_scene->DisplayOrder())
    {
        if (!g_scene->data[index].boxArea.HasValue())
        {
            continue;
        }
        const auto& area = g_scene->data[index].boxArea.Get();
        const auto& center = g_scene->world[index].pos;
        if (!inMenu && !depthChanged)
        {
            RwRenderStateGet(rwRENDERSTATEZTESTENABLE, &depthTest);
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(TRUE));
            depthChanged = true;
        }
        const CRect bounds(center.x - area.GetSize(0) * 0.5f, center.y - area.GetSize(1) * 0.5f,
            center.x + area.GetSize(0) * 0.5f, center.y + area.GetSize(1) * 0.5f);
        CRadar::DrawAreaOnRadar(bounds, CRGBA(area.colorRGBA), inMenu);
    }
    if (depthChanged)
    {
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(depthTest));
    }
}
