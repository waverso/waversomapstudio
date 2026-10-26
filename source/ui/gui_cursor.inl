// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    static bool g_dragCursorLockRequested = false;
    static bool g_dragCursorLockActive = false;
    static ImGuiID g_dragCursorOwnerId = 0;
    static POINT g_dragCursorRestoreScreenPosition = {};
    static POINT g_dragCursorLockScreenPosition = {};
    static ImVec2 g_dragCursorMovement = {};
    static float g_dragCursorMaxDistanceSquared = 0.0f;
    static int g_dragCursorHideFrames = 0;

    static void HideTooltipsForActiveDrag()
    {
        ImGuiContext& context = *ImGui::GetCurrentContext();
        for (ImGuiWindow* window : context.Windows)
        {
            if (window->Flags & ImGuiWindowFlags_Tooltip)
            {
                ImGui::SetWindowHiddendAndSkipItemsForCurrentFrame(window);
            }
        }
    }

    static bool IsDragCursorWindowFocused()
    {
        return RsGlobal.ps && RsGlobal.ps->window && GetForegroundWindow() == RsGlobal.ps->window;
    }

    static void HideDragCursor()
    {
        if (!ImGui::GetCurrentContext() || !IsDragCursorWindowFocused())
        {
            return;
        }
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        SetCursor(nullptr);
        HideTooltipsForActiveDrag();
    }

    static void ClampDragCursorScreenPosition(POINT& point)
    {
        RECT clip = {};
        if (GetClipCursor(&clip) && clip.right > clip.left && clip.bottom > clip.top)
        {
            point.x = std::clamp(point.x, clip.left, clip.right - 1);
            point.y = std::clamp(point.y, clip.top, clip.bottom - 1);
        }
    }

    static void RestoreDragCursor()
    {
        if (!g_dragCursorLockActive || !IsDragCursorWindowFocused())
        {
            return;
        }
        ClampDragCursorScreenPosition(g_dragCursorRestoreScreenPosition);
        POINT client = g_dragCursorRestoreScreenPosition;
        if (!ScreenToClient(RsGlobal.ps->window, &client))
        {
            return;
        }
        POINT current = {};
        if (!GetCursorPos(&current) || current.x != g_dragCursorRestoreScreenPosition.x
            || current.y != g_dragCursorRestoreScreenPosition.y)
        {
            if (!SetCursorPos(g_dragCursorRestoreScreenPosition.x, g_dragCursorRestoreScreenPosition.y))
            {
                return;
            }
            if (g_gameHooks)
            {
                g_gameHooks->NotifySyntheticCursorWarp(client);
            }
            if (!GetCursorPos(&current) || current.x != g_dragCursorRestoreScreenPosition.x
                || current.y != g_dragCursorRestoreScreenPosition.y)
            {
                return;
            }
        }
        GuiSyncMousePosition(g_dragCursorRestoreScreenPosition);
        g_dragCursorLockActive = false;
        g_dragCursorHideFrames = 3;
        HideDragCursor();
    }

    static void CancelDragCursorLock()
    {
        if (g_dragCursorOwnerId && ImGui::GetCurrentContext() && ImGui::GetActiveID() == g_dragCursorOwnerId
            && !ImGui::TempInputIsActive(g_dragCursorOwnerId))
        {
            ImGui::ClearActiveID();
        }
        g_dragCursorOwnerId = 0;
        g_dragCursorLockRequested = false;
        RestoreDragCursor();
    }

    static void PrepareDragCursorInput()
    {
        ImGuiContext& context = *ImGui::GetCurrentContext();
        if (context.IO.MouseDown[0])
        {
            context.IO.ConfigInputTrickleEventQueue = true;
        }
        if (!context.IO.ConfigInputTrickleEventQueue)
        {
            int leftButtonChanges = 0;
            for (const ImGuiInputEvent& event : context.InputEventsQueue)
            {
                if (event.Type == ImGuiInputEventType_MouseButton && event.MouseButton.Button == 0 && ++leftButtonChanges > 1)
                {
                    context.IO.ConfigInputTrickleEventQueue = true;
                    break;
                }
            }
        }
        if (g_dragCursorLockActive)
        {
            GuiSyncMousePosition(g_dragCursorRestoreScreenPosition);
        }
    }

    static void UpdateDragCursorInput(ImGuiIO& io)
    {
        if (!IsDragCursorWindowFocused())
        {
            CancelDragCursorLock();
            return;
        }
        if (g_dragCursorOwnerId
            && (ImGui::GetActiveID() != g_dragCursorOwnerId || !io.MouseDown[0] || !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                || io.MouseClicked[0] || ImGui::TempInputIsActive(g_dragCursorOwnerId)))
        {
            CancelDragCursorLock();
        }
        if (!g_dragCursorOwnerId)
        {
            RestoreDragCursor();
            return;
        }

        io.MouseDelta = ImVec2(0.0f, 0.0f);
        POINT currentScreen = {};
        if (!GetCursorPos(&currentScreen))
        {
            return;
        }
        const ImVec2 delta(static_cast<float>(currentScreen.x - g_dragCursorLockScreenPosition.x),
            static_cast<float>(currentScreen.y - g_dragCursorLockScreenPosition.y));
        const ImVec2 movement = g_dragCursorMovement + delta;
        const float distanceSqr = ImMax(g_dragCursorMaxDistanceSquared, ImLengthSqr(movement));
        const float threshold = io.MouseDragThreshold * 0.5f;
        if (g_dragCursorLockActive || distanceSqr >= threshold * threshold)
        {
            const float sidebarRatio = g_gui ? std::clamp(g_gui->m_sidebarOffsetRatio, 0.0f, 0.9f) : 0.0f;
            POINT centerClient = { static_cast<LONG>(std::lround(io.DisplaySize.x * (1.0f - sidebarRatio) * 0.5f)),
                static_cast<LONG>(std::lround(io.DisplaySize.y * 0.5f)) };
            POINT centerScreen = centerClient;
            if (!ClientToScreen(RsGlobal.ps->window, &centerScreen))
            {
                return;
            }
            ClampDragCursorScreenPosition(centerScreen);
            centerClient = centerScreen;
            if (!ScreenToClient(RsGlobal.ps->window, &centerClient))
            {
                return;
            }
            if (currentScreen.x != centerScreen.x || currentScreen.y != centerScreen.y)
            {
                if (!SetCursorPos(centerScreen.x, centerScreen.y))
                {
                    return;
                }
                if (g_gameHooks)
                {
                    g_gameHooks->NotifySyntheticCursorWarp(centerClient);
                }
            }
            g_dragCursorLockActive = true;
            g_dragCursorLockScreenPosition = centerScreen;
            GuiSyncMousePosition(g_dragCursorRestoreScreenPosition);
            HideDragCursor();
        }
        else
        {
            g_dragCursorLockScreenPosition = currentScreen;
        }
        g_dragCursorMovement = movement;
        g_dragCursorMaxDistanceSquared = distanceSqr;
        io.MouseDragMaxDistanceSqr[0] = distanceSqr;
        io.MouseDelta = delta;
    }

    static void LockMouseCursorForActiveDrag()
    {
        const ImGuiContext& context = *ImGui::GetCurrentContext();
        if (!ImGui::IsItemActive() || context.ActiveIdSource != ImGuiInputSource_Mouse || !context.IO.MouseDown[0]
            || ImGui::TempInputIsActive(context.LastItemData.ID) || !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)
            || !IsDragCursorWindowFocused())
        {
            return;
        }
        if (!g_dragCursorOwnerId)
        {
            if (g_dragCursorLockActive || !context.IO.MouseClicked[0] || !ImGui::IsMousePosValid(&context.IO.MouseClickedPos[0]))
            {
                return;
            }
            POINT start
                = { static_cast<LONG>(context.IO.MouseClickedPos[0].x), static_cast<LONG>(context.IO.MouseClickedPos[0].y) };
            if (!ClientToScreen(RsGlobal.ps->window, &start))
            {
                return;
            }
            g_dragCursorOwnerId = context.LastItemData.ID;
            g_dragCursorRestoreScreenPosition = g_dragCursorLockScreenPosition = start;
            g_dragCursorMovement = ImVec2(0.0f, 0.0f);
            g_dragCursorMaxDistanceSquared = 0.0f;
        }
        if (g_dragCursorOwnerId != context.LastItemData.ID)
        {
            return;
        }
        g_dragCursorLockRequested = true;
        if (g_dragCursorLockActive)
        {
            HideDragCursor();
        }
    }

    static void UpdateDragCursorLock()
    {
        if (!g_dragCursorOwnerId)
        {
            return;
        }
        if (!g_dragCursorLockRequested)
        {
            CancelDragCursorLock();
        }
        else
        {
            g_dragCursorLockRequested = false;
            if (g_dragCursorLockActive)
            {
                HideDragCursor();
            }
        }
    }

    static void ApplyDragCursorHide()
    {
        if (g_dragCursorLockActive || g_dragCursorHideFrames > 0)
        {
            HideDragCursor();
        }
    }

    static void AdvanceDragCursorHide()
    {
        if (g_dragCursorHideFrames <= 0)
        {
            return;
        }
        ApplyDragCursorHide();
        --g_dragCursorHideFrames;
    }
}

bool Gui::IsDragCursorInputBlocked() const
{
    return g_dragCursorOwnerId != 0 || g_dragCursorLockActive || g_dragCursorHideFrames > 0;
}

void GuiSyncMousePosition(const POINT& screen)
{
    if (!ImGui::GetCurrentContext() || !RsGlobal.ps || !RsGlobal.ps->window)
    {
        return;
    }
    POINT client = screen;
    if (!ScreenToClient(RsGlobal.ps->window, &client))
    {
        return;
    }
    ImGuiContext& context = *ImGui::GetCurrentContext();
    const ImVec2 position(static_cast<float>(client.x), static_cast<float>(client.y));
    for (ImGuiInputEvent& event : context.InputEventsQueue)
    {
        if (event.Type == ImGuiInputEventType_MousePos)
        {
            event.MousePos.PosX = position.x;
            event.MousePos.PosY = position.y;
        }
    }
    context.IO.MousePos = context.IO.MousePosPrev = context.MouseLastValidPos = position;
    context.IO.MouseDelta = ImVec2(0.0f, 0.0f);
}

void Gui::CancelDragCursor()
{
    CancelDragCursorLock();
    if (ImGui::GetCurrentContext()
        && (!g_freeCamera || !g_freeCamera->m_active || FrontEndMenuManager.m_bMenuActive || !IsDragCursorWindowFocused()))
    {
        g_dragCursorHideFrames = 0;
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    }
}
