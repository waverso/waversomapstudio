// Source Repository: https://github.com/waverso/waversomapstudio

static bool MapGizmoHotkey(UINT vk, GizmoHotkeyEvent& event)
{
    switch (vk)
    {
        case '1':
            event = GizmoHotkeyEvent::None;
            return true;
        case '2':
            event = GizmoHotkeyEvent::Position;
            return true;
        case '3':
            event = GizmoHotkeyEvent::Rotation;
            return true;
        case '4':
            event = GizmoHotkeyEvent::Both;
            return true;
        case 'Z':
            event = GizmoHotkeyEvent::TogglePivot;
            return true;
        case 'X':
            event = GizmoHotkeyEvent::ToggleSpace;
            return true;
        default:
            return false;
    }
}

static bool GizmoHotkeyModifiersDown()
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0
        || (GetKeyState(VK_MENU) & 0x8000) != 0 || (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
}

static bool GizmoHotkeyContextAllowed()
{
    if (!g_freeCamera || !g_freeCamera->m_active || !g_gizmos || !ImGui::GetCurrentContext() || SampTextInputActive())
    {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    return !io.WantCaptureKeyboard && !io.WantTextInput && !g_gizmos->IsDragging() && !g_gizmos->IsRadiusDragging()
        && !g_gizmos->IsBoxAreaDragging();
}

static bool IsMouseButtonDownMessage(UINT msg)
{
    switch (msg)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONDBLCLK:
        case WM_NCMBUTTONDOWN:
        case WM_NCMBUTTONDBLCLK:
        case WM_NCXBUTTONDOWN:
        case WM_NCXBUTTONDBLCLK:
            return true;
        default:
            return false;
    }
}

void GameHooks::ResetGizmoHotkeyInput()
{
    m_gizmoHotkeySequence = {};
    m_gizmoHotkeyEvents.clear();
    m_syntheticCursorWarps.clear();
}

void GameHooks::NotifySyntheticCursorWarp(const POINT& clientPos)
{
    if (!m_syntheticCursorWarps.empty() && m_syntheticCursorWarps.back().x == clientPos.x
        && m_syntheticCursorWarps.back().y == clientPos.y)
    {
        return;
    }
    if (m_syntheticCursorWarps.size() == 16)
    {
        m_syntheticCursorWarps.erase(m_syntheticCursorWarps.begin());
    }
    m_syntheticCursorWarps.push_back(clientPos);
}

bool GameHooks::ConsumeSyntheticCursorWarp(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KILLFOCUS || msg == WM_CANCELMODE || (msg == WM_ACTIVATEAPP && wParam == FALSE))
    {
        m_syntheticCursorWarps.clear();
        return false;
    }
    if (msg != WM_MOUSEMOVE || m_syntheticCursorWarps.empty())
    {
        return false;
    }

    const POINT clientPos = { static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
    const auto warp = std::find_if(m_syntheticCursorWarps.begin(), m_syntheticCursorWarps.end(),
        [&](const POINT& point)
        {
            return point.x == clientPos.x && point.y == clientPos.y;
        });
    if (warp == m_syntheticCursorWarps.end())
    {
        m_syntheticCursorWarps.clear();
        return false;
    }
    m_syntheticCursorWarps.erase(m_syntheticCursorWarps.begin(), warp + 1);
    return true;
}

void GameHooks::TrackGizmoHotkeyMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KILLFOCUS || msg == WM_CANCELMODE || (msg == WM_ACTIVATEAPP && wParam == FALSE))
    {
        ResetGizmoHotkeyInput();
        return;
    }

    if (IsMouseButtonDownMessage(msg))
    {
        ++m_inputDownSerial;
        return;
    }

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        if ((lParam & (1LL << 30)) != 0)
        {
            return;
        }
        ++m_inputDownSerial;

        GizmoHotkeyEvent event;
        const UINT vk = (UINT)wParam;
        if (!MapGizmoHotkey(vk, event) || m_gizmoHotkeySequence.vk != 0)
        {
            return;
        }

        m_gizmoHotkeySequence.vk = vk;
        m_gizmoHotkeySequence.downSerial = m_inputDownSerial;
        m_gizmoHotkeySequence.contextAllowedAtDown = GizmoHotkeyContextAllowed() && !GizmoHotkeyModifiersDown();
        return;
    }

    if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
    {
        const UINT vk = (UINT)wParam;
        if (m_gizmoHotkeySequence.vk != vk)
        {
            return;
        }

        const bool valid = m_gizmoHotkeySequence.contextAllowedAtDown && m_gizmoHotkeySequence.downSerial == m_inputDownSerial
            && GizmoHotkeyContextAllowed() && !GizmoHotkeyModifiersDown();
        m_gizmoHotkeySequence = {};

        GizmoHotkeyEvent event;
        if (valid && MapGizmoHotkey(vk, event) && m_gizmoHotkeyEvents.size() < 16)
        {
            m_gizmoHotkeyEvents.push_back(event);
        }
    }
}

bool GameHooks::PopGizmoHotkeyEvent(GizmoHotkeyEvent& event)
{
    if (m_gizmoHotkeyEvents.empty())
    {
        return false;
    }
    event = m_gizmoHotkeyEvents.front();
    m_gizmoHotkeyEvents.erase(m_gizmoHotkeyEvents.begin());
    return true;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND wnd, UINT umsg, WPARAM wparam, LPARAM lparam);
LRESULT __stdcall GameHooks::HookWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_gameHooks && g_gameHooks->ConsumeSyntheticCursorWarp(msg, wParam, lParam))
    {
        return 0;
    }
    if (g_gameHooks)
    {
        g_gameHooks->TrackGizmoHotkeyMessage(msg, wParam, lParam);
    }
    if (g_gui
        && (msg == WM_LBUTTONUP || msg == WM_KILLFOCUS || msg == WM_CANCELMODE || msg == WM_CAPTURECHANGED
            || (msg == WM_ACTIVATEAPP && wParam == FALSE)))
    {
        g_gui->CancelDragCursor();
    }
    if (ImGui::GetCurrentContext())
    {
        if (msg == WM_KILLFOCUS || msg == WM_CANCELMODE || (msg == WM_ACTIVATEAPP && wParam == FALSE))
        {
            ImGui::GetIO().ClearEventsQueue();
            ImGui::GetIO().ClearInputKeys();
        }

        if (msg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDrawCursor || ImGui::GetMouseCursor() == ImGuiMouseCursor_None)
            {
                SetCursor(nullptr);
                return true;
            }
        }

        if (!SampTextInputActive())
        {
            if (g_gui && g_freeCamera->m_active && msg == WM_KEYDOWN && wParam == VK_ESCAPE)
            {
                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
                return true;
            }
            if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            {
                return true;
            }

            ImGuiIO& io = ImGui::GetIO();
            const bool nativeShortcut = (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
                && (wParam == VK_F8 || (!io.WantTextInput && (wParam == 'T' || wParam == VK_F6)));
            if (!nativeShortcut && (io.WantCaptureKeyboard || io.WantTextInput) && msg >= WM_KEYFIRST && msg <= WM_KEYLAST)
            {
                plugin::Call<0x53F1E0>();
                return true;
            }
            if (io.WantCaptureMouse && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)
            {
                return true;
            }
        }
        else
        {
            switch (msg)
            {
                case WM_KEYUP:
                case WM_SYSKEYUP:
                case WM_MOUSEMOVE:
                case WM_NCMOUSEMOVE:
                    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            }
        }
    }
    return g_gameHooks->m_originalWndProc(hWnd, msg, wParam, lParam);
}
