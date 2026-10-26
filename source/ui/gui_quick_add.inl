// Source Repository: https://github.com/waverso/waversomapstudio

static void OpenQuickAddModal()
{
    g_modal.activeType = PendingModalAddItem;
    g_modal.focusFirst = true;
    g_modal.modelBuf[0] = '\0';
    g_modal.contextParent = -1;
    g_modal.useMouseRay = false;
    g_modal.rayOrigin = {};
    g_modal.rayDir = {};
}

static void DrawAppModal()
{
    if (IsObjectBrowserOpen() || IsVehicleBrowserOpen() || IsSkinBrowserOpen())
    {
        return;
    }
    if (g_modal.activeType == PendingModalNone)
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const char* title = "Add";
    const char* description = "Enter the model ID.";
    const char* labels[] = { "Empty Item", "Browse Objects", "Browse Vehicles", "Browse Actor Skins", "Pickup", "Custom Removal",
        "Box Area", "Race", "Layout", "Curve" };
    const float gap = Theme::g_padding;
    constexpr int rowCounts[] = { 4, 6 };
    float buttonW[std::size(labels)];
    float rowWidths[2] = { gap * (rowCounts[0] - 1), gap * (rowCounts[1] - 1) };
    for (int i = 0; i < std::size(labels); ++i)
    {
        buttonW[i] = ImGui::CalcTextSize(labels[i]).x + Theme::g_padding * 2.0f;
        rowWidths[i < rowCounts[0] ? 0 : 1] += buttonW[i];
    }
    const float innerW = ImMax(Ui::Px(500.0f), ImMax(rowWidths[0], rowWidths[1]));
    const float ModalW = innerW + Theme::g_outerPadding * 2.0f;
    for (int i = 0; i < std::size(labels); ++i)
    {
        const int row = i < rowCounts[0] ? 0 : 1;
        buttonW[i] += (innerW - rowWidths[row]) / rowCounts[row];
    }
    const float descH = ImGui::CalcTextSize(description, nullptr, false, innerW).y;
    const float bodyH = descH + Theme::g_itemSpacing * 2.0f + Theme::g_frameHeight * 3.0f + gap;
    const float ModalH = ModalHeight(bodyH);

    DrawModalBackdrop("##ModalBackdrop");
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(ModalW, ceilf(ModalH)), ImGuiCond_Always);
    PushModalStyle();

    bool confirmed = !g_popupClosedOnEscape
        && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)
        && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    bool cancelled = false;
    bool browserOpened = false;
    bool actionCompleted = false;

    if (ImGui::Begin("##AppModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 winPos = ImGui::GetWindowPos();
        const float winW = ImGui::GetWindowWidth();
        DrawModalTitleBar(dl, winPos, winW, title);

        const float bodyX = winPos.x + Theme::g_outerPadding;
        const float bodyY = winPos.y + Theme::g_tabHeight + Theme::g_padding;
        ImGui::SetCursorScreenPos(ImVec2(bodyX, bodyY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Theme::g_textDim));
        ImGui::PushTextWrapPos(Theme::g_outerPadding + innerW);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        const float fieldX = Theme::g_outerPadding + Theme::g_propertyLabelWidth;
        const float fieldW = innerW - Theme::g_propertyLabelWidth;
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, Theme::g_tabHeight + Theme::g_padding + descH + Theme::g_itemSpacing));
        DrawLabel(dl, "Model ID");
        ImGui::SetCursorPosX(fieldX);
        ImGui::SetNextItemWidth(fieldW);
        if (g_modal.focusFirst)
        {
            ImGui::SetKeyboardFocusHere();
            g_modal.focusFirst = false;
        }
        if (ImGui::InputTextEx("##modal_model", nullptr, g_modal.modelBuf, sizeof(g_modal.modelBuf),
                ImVec2(fieldW, Theme::g_frameHeight),
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            confirmed = true;
        }

        const float buttonsY = Theme::g_tabHeight + Theme::g_padding + descH + Theme::g_itemSpacing * 2.0f + Theme::g_frameHeight;
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, buttonsY));

        if (ImGui::Button(labels[0], ImVec2(buttonW[0], Theme::g_frameHeight)))
        {
            actionCompleted = AddEmptyToScene(-1, AddPopupContext {});
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[1], ImVec2(buttonW[1], Theme::g_frameHeight)))
        {
            OpenObjectBrowser();
            browserOpened = true;
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[2], ImVec2(buttonW[2], Theme::g_frameHeight)))
        {
            OpenVehicleBrowser();
            browserOpened = true;
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[3], ImVec2(buttonW[3], Theme::g_frameHeight)))
        {
            OpenSkinBrowser();
            browserOpened = true;
        }
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, buttonsY + Theme::g_frameHeight + gap));
        if (ImGui::Button(labels[4], ImVec2(buttonW[4], Theme::g_frameHeight)))
        {
            actionCompleted = AddPickupToScene(-1, AddPopupContext {});
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[5], ImVec2(buttonW[5], Theme::g_frameHeight)))
        {
            actionCompleted = AddCustomRemoveToScene(-1, AddPopupContext {});
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[6], ImVec2(buttonW[6], Theme::g_frameHeight)))
        {
            actionCompleted = AddBoxAreaToScene(-1, AddPopupContext {});
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[7], ImVec2(buttonW[7], Theme::g_frameHeight)))
        {
            actionCompleted = AddRaceItemToScene(-1, AddPopupContext {}, false);
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[8], ImVec2(buttonW[8], Theme::g_frameHeight)))
        {
            actionCompleted = AddLayoutToScene(-1, AddPopupContext {});
        }
        ImGui::SameLine(0, gap);
        if (ImGui::Button(labels[9], ImVec2(buttonW[9], Theme::g_frameHeight)))
        {
            actionCompleted = AddCurveItemToScene(-1, AddPopupContext {}, false);
        }

        DrawModalFooter();
        const float btnW = (innerW - Theme::g_padding) * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        if (ImGui::Button("Add##modal", ImVec2(btnW, Theme::g_frameHeight)))
        {
            confirmed = true;
        }
        ImGui::SameLine(0, Theme::g_padding);
        if (ImGui::Button("Cancel##modal", ImVec2(btnW, Theme::g_frameHeight)))
        {
            cancelled = true;
        }
        ImGui::PopStyleVar();
    }
    ImGui::End();
    PopModalStyle();

    if (browserOpened)
    {
        return;
    }
    if (actionCompleted)
    {
        g_modal.activeType = PendingModalNone;
        return;
    }
    if (confirmed)
    {
        const char* p = g_modal.modelBuf;
        while (*p == ' ')
        {
            ++p;
        }
        const int modelId = (*p != '\0') ? atoi(p) : -1;
        bool keepOpen = false;

        if (modelId >= 0 && modelId <= 311)
        {
            keepOpen = !AddActorToScene(modelId);
        }
        else if (modelId >= 400 && modelId <= 611)
        {
            if (!AddVehicleToScene(modelId))
            {
                keepOpen = true;
            }
        }
        else if (modelId >= 321 && modelId <= 19999)
        {
            if (!AddObjectToScene(modelId))
            {
                keepOpen = true;
            }
        }
        else
        {
            g_toast.ShowError("Model ID is out of range.");
            keepOpen = true;
        }

        if (keepOpen)
        {
            g_modal.focusFirst = true;
        }
        else
        {
            g_modal.activeType = PendingModalNone;
        }
    }

    if (cancelled)
    {
        g_modal.activeType = PendingModalNone;
    }
}
