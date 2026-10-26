// Source Repository: https://github.com/waverso/waversomapstudio

template <typename ToggleFavorite>
static void HandleModelPreviewInput(float previewSize, double& wheelAt, ToggleFavorite toggleFavorite)
{
    const auto& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool rdrag = active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f);
    const bool mdrag = active
        && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f));
    if ((hovered || active) && io.MouseWheel != 0.0f)
    {
        wheelAt = ImGui::GetTime();
        float step = 1.1f;
        if (io.KeyShift)
        {
            step = 1.25f;
        }
        else if (io.KeyAlt)
        {
            step = 1.035f;
        }
        ModelPreview::Zoom(io.MouseWheel, step);
    }
    if (rdrag || mdrag)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Drag);
    }
    if (rdrag)
    {
        ModelPreview::Orbit(-io.MouseDelta.x * 0.35f, io.MouseDelta.y * 0.35f);
    }
    if (mdrag)
    {
        ModelPreview::Pan(io.MouseDelta.x, io.MouseDelta.y, previewSize);
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        ModelPreview::Reset();
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false))
    {
        toggleFavorite();
    }
    if (ImGui::GetTime() - wheelAt >= 0.5 && !io.MouseDown[0] && !io.MouseDown[1] && !io.MouseDown[2]
        && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip("Reset View (R)\nFavorite (F)");
    }
}

static float ModelInfoCardHeight(bool favorite, bool animation, int metricRows = 3)
{
    const float sm = ScaledFontSize(g_gui->m_fontSmall);
    const float infoRowH = sm + Ui::Px(8.0f);
    return Ui::Px(7.0f) * 2.0f + ImGui::GetFontSize() + ((favorite || animation) ? Ui::Px(7.0f) + infoRowH : 0.0f) + Ui::Px(9.0f)
        + infoRowH * (metricRows + 1) + Ui::Px(7.0f) * 2.0f + Ui::Hairline();
}

template <bool Skin>
static void DrawCatalogInfoCard(int modelId, float width, bool favorite)
{
    const auto model = [modelId]
    {
        if constexpr (Skin)
        {
            return SkinBrowser::GetModel(modelId);
        }
        else
        {
            return VehicleBrowser::GetModel(modelId);
        }
    }();
    const bool animation = [&]
    {
        if constexpr (Skin)
        {
            return false;
        }
        else
        {
            return model.animation;
        }
    }();
    if (model.modelId < 0)
    {
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    ImFont* smallFont = g_gui->m_fontSmall;
    const float fontSize = ScaledFontSize(font);
    const float sm = ScaledFontSize(smallFont);
    const float infoRowH = sm + Ui::Px(8.0f);
    const ImVec2 cardMin = ImGui::GetCursorScreenPos();
    const float height = ModelInfoCardHeight(favorite, animation, Skin ? 2 : 3);
    const ImVec2 cardMax(cardMin.x + width, cardMin.y + height);
    dl->AddRectFilled(cardMin, cardMax, Theme::g_infoCardBackground);
    dl->AddRect(cardMin, cardMax, Theme::g_border, 0.0f, 0, Ui::Hairline());
    ImGui::PushClipRect(cardMin, cardMax, true);
    float y = cardMin.y + Ui::Px(7.0f);
    const float nameW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, model.name).x;
    ImGui::SetCursorScreenPos(ImVec2(cardMin.x, y));
    SelectableField(
        "##vehicledetail_name", model.name, width, fontSize, font, Theme::g_text, ImMax(0.0f, (width - nameW) * 0.5f), false);
    y += fontSize;
    struct InfoChip
    {
        const char* label;
        ImU32 color;
    };
    InfoChip chips[2];
    int chipCount = 0;
    if (animation)
    {
        chips[chipCount++] = { "Model Anim", Theme::g_chipAnimation };
    }
    if (favorite)
    {
        chips[chipCount++] = { "Favorited", Theme::g_chipFavorited };
    }
    if (chipCount)
    {
        y += Ui::Px(7.0f);
        float chipWidths[2];
        float chipsWidth = 0.0f;
        for (int index = 0; index < chipCount; ++index)
        {
            chipWidths[index] = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chips[index].label).x + Ui::Px(20.0f);
            chipsWidth += chipWidths[index];
        }
        chipsWidth += Theme::g_padding * 0.5f * (chipCount - 1);
        float x = cardMin.x + (width - chipsWidth) * 0.5f;
        for (int index = 0; index < chipCount; ++index)
        {
            const float textW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chips[index].label).x;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + chipWidths[index], y + infoRowH),
                (chips[index].color & IM_COL32(255, 255, 255, 0)) | IM_COL32(0, 0, 0, 32));
            dl->AddText(smallFont, sm, ImVec2(x + (chipWidths[index] - textW) * 0.5f, y + (infoRowH - sm) * 0.5f),
                chips[index].color, chips[index].label);
            x += chipWidths[index] + Theme::g_padding * 0.5f;
        }
        y += infoRowH;
    }
    y += Ui::Px(9.0f);
    const float leftX = cardMin.x + Theme::g_padding;
    const float availW = width - Theme::g_padding * 2.0f;
    const float valPadX = Ui::Px(2.0f);
    auto infoCell = [&](float x, float cellW, const char* label, const char* value)
    {
        const float labelW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, label).x;
        const float valueW
            = ImMin(ImMax(1.0f, cellW - labelW - valPadX), smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, value).x + valPadX);
        dl->PushClipRect(ImVec2(x, y), ImVec2(x + cellW, y + infoRowH), true);
        dl->AddText(smallFont, sm, ImVec2(x, y), Theme::g_textDim, label);
        ImGui::SetCursorScreenPos(ImVec2(x + cellW - valueW, y));
        ImGui::PushID(label);
        SelectableField("##value", value, valueW, sm, smallFont, Theme::g_text, valPadX, false);
        ImGui::PopID();
        dl->PopClipRect();
    };
    char id[24];
    snprintf(id, sizeof(id), "%d", model.modelId);
    const float topW = (availW - Theme::g_padding) * 0.5f;
    infoCell(leftX, topW, "Model ID", id);
    infoCell(leftX + topW + Theme::g_padding, topW, "Model Name", model.modelName);
    y += infoRowH + Ui::Px(7.0f);
    dl->AddLine(ImVec2(leftX, y), ImVec2(leftX + availW, y), Theme::g_border, Ui::Hairline());
    y += Ui::Hairline() + Ui::Px(7.0f);
    const float colW = (availW - Theme::g_padding * 2.0f) / 3.0f;
    auto row3 = [&](const char* label0, const char* value0, const char* label1, const char* value1, const char* label2,
                    const char* value2)
    {
        infoCell(leftX, colW, label0, value0);
        infoCell(leftX + colW + Theme::g_padding, colW, label1, value1);
        infoCell(leftX + (colW + Theme::g_padding) * 2.0f, colW, label2, value2);
        y += infoRowH;
    };
    char metrics[4][32];
    const float dimensions[] = { model.length, model.width, model.height, model.radius };
    for (int i = 0; i < 4; ++i)
    {
        char number[24];
        SmartFormat(number, sizeof(number), dimensions[i]);
        snprintf(metrics[i], sizeof(metrics[i]), "%s m", number);
    }
    if constexpr (Skin)
    {
        row3("Category", SkinCatalog::Categories[model.category], "Gender", SkinCatalog::Genders[model.gender], "Radius",
            metrics[3]);
        row3("Length", metrics[0], "Width", metrics[1], "Height", metrics[2]);
    }
    else
    {
        char seats[16], speed[24], colors[16];
        snprintf(seats, sizeof(seats), "%d", model.seats);
        snprintf(speed, sizeof(speed), "%d km/h", model.maxSpeed);
        snprintf(colors, sizeof(colors), "%d", model.colorSlots);
        row3("Category", VehicleBrowser::CategoryName(model.category), "Mod Shop",
            VehicleCatalog::ModificationShops[model.modificationShop], "Seats", seats);
        row3("Max Speed", speed, "Color Slots", colors, "Length", metrics[0]);
        row3("Width", metrics[1], "Height", metrics[2], "Radius", metrics[3]);
    }
    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(cardMin);
    ImGui::Dummy(ImVec2(width, height));
}

template <bool Skin, class State>
static void DrawCatalogDetailModal(State& ui, bool& selected, bool& closed)
{
    if (ui.detailModelId < 0)
    {
        return;
    }
    DrawModalBackdrop("##VehicleDetailBackdrop");
    const auto& io = ImGui::GetIO();
    const float winW = Ui::Px(450);
    const float inner = winW - Theme::g_outerPadding * 2;
    const bool favorite = Skin ? SkinBrowser::IsFavorite(ui.detailModelId) : VehicleBrowser::IsFavorite(ui.detailModelId);
    const float cardH
        = ModelInfoCardHeight(favorite, !Skin && VehicleBrowser::GetModel(ui.detailModelId).animation, Skin ? 2 : 3);
    const float previewSize
        = ImMin(inner, ImMax(Ui::Px(100), io.DisplaySize.y - Ui::Px(40) - ModalHeight(Theme::g_itemSpacing + cardH)));
    const float winH = ModalHeight(previewSize + Theme::g_itemSpacing + cardH);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, io.DisplaySize.y * .5f), ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    PushModalStyle();
    if (ImGui::Begin(Skin ? "##SkinDetails" : "##VehicleDetails", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        const ImVec2 pos = ImGui::GetWindowPos();
        auto* dl = ImGui::GetWindowDrawList();
        DrawModalTitleBar(dl, pos, winW, Skin ? "Skin Model Details" : "Vehicle Model Details");
        const ImVec2 min(pos.x + (winW - previewSize) * .5f, pos.y + Theme::g_tabHeight + Theme::g_padding);
        const ImVec2 max(min.x + previewSize, min.y + previewSize);
        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton("##vehicle_preview", ImVec2(previewSize, previewSize),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
        static double wheelAt = -1e9;
        HandleModelPreviewInput(previewSize, wheelAt,
            [&]
            {
                if constexpr (Skin)
                {
                    SkinBrowser::ToggleFavorite(ui.detailModelId);
                }
                else
                {
                    VehicleBrowser::ToggleFavorite(ui.detailModelId);
                }
                ui.dirty = true;
            });
        dl->AddRectFilled(min, max, Theme::g_controlBackground);
        if (auto* texture = ModelPreview::Texture())
        {
            dl->AddImage(reinterpret_cast<ImTextureID>(texture), min, max);
        }
        dl->AddRect(min, max, Theme::g_border);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, max.y + Theme::g_itemSpacing));
        DrawCatalogInfoCard<Skin>(ui.detailModelId, inner, favorite);
        DrawModalFooter();
        if (ui.viewOnly)
        {
            if (ImGui::Button("Close##vehicle_details", ImVec2(inner, Theme::g_frameHeight)))
            {
                closed = true;
            }
            if (!ImGui::IsWindowAppearing() && !io.WantTextInput
                && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            {
                closed = true;
            }
        }
        else
        {
            const float bw = (inner - Theme::g_padding) * .5f;
            if (ImGui::Button(
                    ui.changeItemId >= 0 ? "Change##vehicle_details" : "Add##vehicle_details", ImVec2(bw, Theme::g_frameHeight)))
            {
                selected = true;
            }
            ImGui::SameLine(0, Theme::g_padding);
            if (ImGui::Button("Back##vehicle_details", ImVec2(bw, Theme::g_frameHeight)))
            {
                closed = true;
            }
            if (!io.WantTextInput
                && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            {
                selected = true;
            }
        }
    }
    ImGui::End();
    PopModalStyle();
}
