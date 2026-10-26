// Source Repository: https://github.com/waverso/waversomapstudio

static ComponentInfoImage GetComponentModelImage(int modelId)
{
    ComponentInfoImage image;
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId)
    {
        return image;
    }

    const std::uint32_t id = static_cast<std::uint32_t>(modelId);
    ModelThumbnails::PreparePage(&id, 1);
    ModelThumbnails::View thumbnail;
    if (ModelThumbnails::Get(modelId, thumbnail))
    {
        image.texture = thumbnail.texture;
        image.uvMin = ImVec2(thumbnail.uvMin[0], thumbnail.uvMin[1]);
        image.uvMax = ImVec2(thumbnail.uvMax[0], thumbnail.uvMax[1]);
    }
    return image;
}

static void DrawObjectDetailModal(bool& selected, bool& closed)
{
    ObjectBrowserUiState& ui = g_objectBrowserUi;
    if (ui.detailModelId < 0)
    {
        return;
    }
    const ObjectBrowser::ModelInfo model = ObjectBrowser::GetModel(ui.detailModelId);
    if (model.modelId < 0)
    {
        closed = true;
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    DrawModalBackdrop("##ObjectDetailBackdrop");
    ImFont* font = ImGui::GetFont();
    ImFont* smallFont = g_gui->m_fontSmall;
    const float fontSize = ScaledFontSize(font), sm = ScaledFontSize(smallFont);
    const float winW = Ui::Px(450.0f);
    const float innerW = winW - Theme::g_outerPadding * 2.0f;
    const float previewSize = innerW;
    const bool isFav = ObjectBrowser::IsFavorite(model.modelId);
    const bool hasDescription = model.description && model.description[0];

    const float cardPadY = Ui::Px(7.0f);
    const float nameH = fontSize;
    const float nameGap = Ui::Px(7.0f);
    const float chipPadX = Ui::Px(10.0f);
    const float chipH = sm + Ui::Px(8.0f);
    const float chipGap = Ui::Px(6.0f);
    const float chipRowGap = Ui::Px(5.0f);
    const float rowGap = Ui::Px(7.0f);
    const float sectionGap = Ui::Px(9.0f);
    const float infoRowH = sm + Ui::Px(8.0f);
    const float sepGap = Ui::Px(7.0f);
    const float chipAvailW = innerW - Theme::g_padding * 2.0f;

    struct DetailChip
    {
        const char* label;
        ImU32 color;
        float width;
    };
    DetailChip chips[10];
    int chipCount = 0;
    auto addChip = [&](bool enabled, const char* label, ImU32 color)
    {
        if (!enabled || chipCount >= IM_ARRAYSIZE(chips))
        {
            return;
        }
        DetailChip& chip = chips[chipCount++];
        chip.label = label;
        chip.color = color;
        chip.width = ImMin(chipAvailW, smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, label).x + chipPadX * 2.0f);
    };
    addChip(model.lowLod, "Low LOD", Theme::g_chipLowLod);
    addChip((model.behaviour & ObjectBrowser::BehaviourBreakable) != 0, "Breakable", Theme::g_chipBreakable);
    addChip((model.behaviour & ObjectBrowser::BehaviourCollidable) != 0, "Collidable", Theme::g_chipCollidable);
    addChip((model.behaviour & ObjectBrowser::BehaviourAnimation) != 0, "Model Anim", Theme::g_chipAnimation);
    addChip((model.behaviour & ObjectBrowser::BehaviourUvAnim) != 0, "UV Anim", Theme::g_chipUvAnim);
    addChip((model.behaviour & ObjectBrowser::BehaviourBackface) != 0, "Two-Sided", Theme::g_chipBackface);
    addChip((model.behaviour & ObjectBrowser::BehaviourPhysics) != 0, "Physics", Theme::g_chipPhysics);
    addChip((model.behaviour & ObjectBrowser::BehaviourExplosive) != 0, "Explosive", Theme::g_chipExplosive);
    addChip(isFav, "Favorited", Theme::g_chipFavorited);
    char timeChip[24] = {};
    if (model.hasTime)
    {
        snprintf(timeChip, sizeof(timeChip), "Time %u-%u", model.timeOn, model.timeOff);
        addChip(true, timeChip, Theme::g_chipTime);
    }

    int chipLineStart[10] = {}, chipLineCount[10] = {}, chipLineTotal = 0;
    float chipLineWidth[10] = {};
    for (int i = 0; i < chipCount;)
    {
        const int start = i++;
        int count = 1;
        float width = chips[start].width;
        while (i < chipCount && width + chipGap + chips[i].width <= chipAvailW)
        {
            width += chipGap + chips[i].width;
            ++count;
            ++i;
        }
        chipLineStart[chipLineTotal] = start;
        chipLineCount[chipLineTotal] = count;
        chipLineWidth[chipLineTotal] = width;
        ++chipLineTotal;
    }
    const float chipsH = chipLineTotal > 0 ? chipLineTotal * chipH + (chipLineTotal - 1) * chipRowGap : 0.0f;

    const float infoCardH = cardPadY + nameH + (chipCount ? nameGap + chipsH : 0.0f) + sectionGap + infoRowH
        + (hasDescription ? rowGap + infoRowH : 0.0f) + sepGap + Ui::Hairline() + sepGap + infoRowH * 4.0f + cardPadY;
    const float winH = ModalHeight(previewSize + Theme::g_itemSpacing + infoCardH);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(winW, ceilf(winH)), ImGuiCond_Always);
    PushModalStyle();
    bool use = false, close = false;

    if (ImGui::Begin("##ObjectBrowserDetail", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetWindowPos();
        DrawModalTitleBar(dl, pos, winW, "Object Model Details");
        const float pad = Theme::g_outerPadding;
        const ImVec2 pmin(pos.x + pad, pos.y + Theme::g_tabHeight + Theme::g_padding);
        const ImVec2 pmax(pmin.x + previewSize, pmin.y + previewSize);
        ImGui::SetCursorScreenPos(pmin);
        ImGui::InvisibleButton("##object_preview_input", ImVec2(previewSize, previewSize),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
        static double wheelAt = -1e9;
        HandleModelPreviewInput(previewSize, wheelAt,
            [&]
            {
                ObjectBrowser::ToggleFavorite(ui.detailModelId);
            });

        dl->AddRectFilled(pmin, pmax, Theme::g_controlBackground);
        if (IDirect3DTexture9* texture = ModelPreview::Texture())
        {
            dl->AddImage(reinterpret_cast<ImTextureID>(texture), pmin, pmax);
        }
        dl->AddRect(pmin, pmax, Theme::g_border, 0, 0, Ui::Hairline());

        const ImVec2 cardMin(pos.x + pad, pmax.y + Theme::g_itemSpacing);
        const ImVec2 cardMax(cardMin.x + innerW, cardMin.y + infoCardH);
        dl->AddRectFilled(cardMin, cardMax, Theme::g_infoCardBackground);
        dl->AddRect(cardMin, cardMax, Theme::g_border, 0.0f, 0, Ui::Hairline());
        float cardY = cardMin.y + cardPadY;
        const char* name = model.name[0] ? model.name : "Unnamed Model";
        const float nameW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name).x;
        ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardY));
        SelectableField(
            "##objectdetail_name", name, innerW, fontSize, font, Theme::g_text, ImMax(0.0f, (innerW - nameW) * 0.5f), false);
        cardY += nameH;

        if (chipCount)
        {
            cardY += nameGap;
            for (int line = 0; line < chipLineTotal; ++line)
            {
                float chipX = cardMin.x + Theme::g_padding + (chipAvailW - chipLineWidth[line]) * 0.5f;
                for (int i = 0; i < chipLineCount[line]; ++i)
                {
                    DetailChip& chip = chips[chipLineStart[line] + i];
                    dl->AddRectFilled(ImVec2(chipX, cardY), ImVec2(chipX + chip.width, cardY + chipH),
                        (chip.color & IM_COL32(255, 255, 255, 0)) | IM_COL32(0, 0, 0, 32));
                    const float textW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chip.label).x;
                    dl->AddText(smallFont, sm, ImVec2(chipX + (chip.width - textW) * 0.5f, cardY + (chipH - sm) * 0.5f),
                        chip.color, chip.label);
                    chipX += chip.width + chipGap;
                }
                cardY += chipH + (line + 1 < chipLineTotal ? chipRowGap : 0.0f);
            }
        }
        cardY += sectionGap;

        const float leftX = cardMin.x + Theme::g_padding;
        const float availW = innerW - Theme::g_padding * 2.0f;
        const float valPadX = Ui::Px(2.0f);
        auto infoCell = [&](float cx, float cy, float cw, const char* label, const char* value, const char* id)
        {
            dl->AddText(smallFont, sm, ImVec2(cx, cy), Theme::g_textDim, label);
            const float valueW = ImMin(cw, smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, value).x + valPadX);
            ImGui::SetCursorScreenPos(ImVec2(cx + cw - valueW, cy));
            SelectableField(id, value, valueW, sm, smallFont, Theme::g_text, valPadX, false);
        };
        auto descriptionCell = [&](const char* description)
        {
            const char* label = "Description";
            dl->AddText(smallFont, sm, ImVec2(leftX, cardY), Theme::g_textDim, label);
            const float labelW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, label).x;
            const float maxValueW = ImMax(1.0f, availW - labelW - Theme::g_padding);
            const float fullValueW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, description).x + valPadX;
            const float valueW = ImMin(maxValueW, fullValueW);
            char visible[256];
            snprintf(visible, sizeof(visible), "%s", description);
            const bool truncated = fullValueW > maxValueW;
            if (truncated)
            {
                ImGui::PushFont(smallFont);
                EllipsizeText(visible, sizeof(visible), description, valueW - valPadX, sm);
                ImGui::PopFont();
            }
            ImGui::SetCursorScreenPos(ImVec2(leftX + availW - valueW, cardY));
            SelectableField("##objectdetail_desc", visible, valueW, sm, smallFont, Theme::g_text, valPadX, false);
            if (truncated && ImGui::IsItemHovered())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_popupPadding));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
                if (ImGui::BeginTooltip())
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + Ui::Px(360.0f));
                    ImGui::TextUnformatted(description);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                ImGui::PopStyleVar(2);
            }
        };
        auto metricText = [](char* out, size_t outSize, float value)
        {
            char number[32];
            SmartFormat(number, sizeof(number), value);
            snprintf(out, outSize, "%s m", number);
        };
        char idText[24], txdText[96];
        snprintf(idText, sizeof(idText), "%d", model.modelId);
        snprintf(txdText, sizeof(txdText), "%s.txd", model.txdName[0] ? model.txdName : "Unknown");
        const float topGap = Theme::g_padding;
        const float topW = (availW - topGap) * 0.5f;
        infoCell(leftX, cardY, topW, "Model ID", idText, "##objectdetail_id");
        infoCell(leftX + topW + topGap, cardY, topW, "Library", txdText, "##objectdetail_txd");
        cardY += infoRowH;

        if (hasDescription)
        {
            cardY += rowGap;
            descriptionCell(model.description);
            cardY += infoRowH;
        }

        cardY += sepGap;
        dl->AddLine(ImVec2(leftX, cardY), ImVec2(leftX + availW, cardY), Theme::g_border, Ui::Hairline());
        cardY += Ui::Hairline() + sepGap;

        char colText[96];
        if (!model.colName || !model.colName[0])
        {
            snprintf(colText, sizeof(colText), "None");
        }
        else
        {
            const size_t colLen = strlen(model.colName);
            const bool hasExtension = colLen >= 4 && _stricmp(model.colName + colLen - 4, ".col") == 0;
            snprintf(colText, sizeof(colText), "%s%s", model.colName, hasExtension ? "" : ".col");
        }
        const char* category = ObjectBrowser::CategoryName(model.category);
        if (!category || !category[0])
        {
            category = "None";
        }
        const float colGap = Theme::g_padding;
        const float colW = (availW - colGap * 2.0f) / 3.0f;
        auto row3 = [&](const char* label0, const char* value0, const char* id0, const char* label1, const char* value1,
                        const char* id1, const char* label2, const char* value2, const char* id2)
        {
            infoCell(leftX, cardY, colW, label0, value0, id0);
            infoCell(leftX + colW + colGap, cardY, colW, label1, value1, id1);
            infoCell(leftX + (colW + colGap) * 2.0f, cardY, colW, label2, value2, id2);
            cardY += infoRowH;
        };

        row3("Source", TextureBrowser::SourceImgName(model.sourceImg), "##objectdetail_source", "Category", category,
            "##objectdetail_category", "COL File", colText, "##objectdetail_col");

        char lengthText[32], widthText[32], heightText[32];
        metricText(lengthText, sizeof(lengthText), model.length);
        metricText(widthText, sizeof(widthText), model.width);
        metricText(heightText, sizeof(heightText), model.height);
        row3("Length", lengthText, "##objectdetail_length", "Width", widthText, "##objectdetail_width", "Height", heightText,
            "##objectdetail_height");

        char radiusText[32], drawText[32], materialText[24];
        metricText(radiusText, sizeof(radiusText), model.radius);
        metricText(drawText, sizeof(drawText), model.drawDistance);
        snprintf(materialText, sizeof(materialText), "%u", model.materialCount);
        row3("Radius", radiusText, "##objectdetail_radius", "Draw Dist", drawText, "##objectdetail_draw", "Materials",
            materialText, "##objectdetail_materials");

        char polyText[32], vertexText[32], dffText[32];
        snprintf(polyText, sizeof(polyText), "%u", model.polyCount);
        snprintf(vertexText, sizeof(vertexText), "%u", model.vertexCount);
        snprintf(dffText, sizeof(dffText), "%u KB", model.dffSizeKb);
        row3("Polygons", polyText, "##objectdetail_poly", "Vertices", vertexText, "##objectdetail_vertices", "DFF Size (KB)",
            dffText, "##objectdetail_dff");

        ImGui::SetCursorScreenPos(cardMin);
        ImGui::Dummy(ImVec2(innerW, infoCardH));
        const float by = DrawModalFooter();
        const float bw = (innerW - Theme::g_padding) * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        if (ui.viewOnly)
        {
            ImGui::SetCursorScreenPos(ImVec2(pmin.x, by));
            if (ImGui::Button("Close##object_detail", ImVec2(innerW, Theme::g_frameHeight)))
            {
                close = true;
            }
            if (!ImGui::IsWindowAppearing() && !io.WantTextInput
                && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            {
                close = true;
            }
        }
        else
        {
            ImGui::SetCursorScreenPos(ImVec2(pmin.x, by));
            if (ImGui::Button(
                    ui.changeItemId >= 0 ? "Change##object_detail" : "Add##object_detail", ImVec2(bw, Theme::g_frameHeight)))
            {
                use = true;
            }
            ImGui::SetCursorScreenPos(ImVec2(pmin.x + bw + Theme::g_padding, by));
            if (ImGui::Button("Back##object_detail", ImVec2(bw, Theme::g_frameHeight)))
            {
                close = true;
            }
        }
        ImGui::PopStyleVar();

        if (!ui.viewOnly && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        {
            use = true;
        }
    }
    ImGui::End();
    PopModalStyle();
    if (use)
    {
        selected = true;
    }
    if (close)
    {
        closed = true;
    }
}

static void DrawModelInfoCard(int modelId, float innerW, bool showChips = true)
{
    const auto model = ObjectBrowser::GetModel(modelId);
    const bool valid = model.modelId >= 0;
    ComponentInfoChip chips[9];
    int chipCount = 0;
    auto addChip = [&](bool enabled, const char* label, ImU32 color)
    {
        if (showChips && enabled)
        {
            chips[chipCount++] = { label, color };
        }
    };
    addChip(model.lowLod, "Low LOD", Theme::g_chipLowLod);
    addChip((model.behaviour & ObjectBrowser::BehaviourBreakable) != 0, "Breakable", Theme::g_chipBreakable);
    addChip(
        model.collidable || (model.behaviour & ObjectBrowser::BehaviourCollidable) != 0, "Collidable", Theme::g_chipCollidable);
    addChip((model.behaviour & ObjectBrowser::BehaviourAnimation) != 0, "Model Anim", Theme::g_chipAnimation);
    addChip((model.behaviour & ObjectBrowser::BehaviourUvAnim) != 0, "UV Anim", Theme::g_chipUvAnim);
    addChip((model.behaviour & ObjectBrowser::BehaviourBackface) != 0, "Two-Sided", Theme::g_chipBackface);
    addChip((model.behaviour & ObjectBrowser::BehaviourPhysics) != 0, "Physics", Theme::g_chipPhysics);
    addChip((model.behaviour & ObjectBrowser::BehaviourExplosive) != 0, "Explosive", Theme::g_chipExplosive);
    char time[24];
    snprintf(time, sizeof(time), "Time %u-%u", static_cast<unsigned>(model.timeOn), static_cast<unsigned>(model.timeOff));
    addChip(model.hasTime, time, Theme::g_chipTime);

    const float dimensions[3] = { model.length, model.width, model.height };
    char values[3][32];
    for (int index = 0; index < 3; ++index)
    {
        char number[24];
        SmartFormat(number, sizeof(number), dimensions[index]);
        snprintf(values[index], sizeof(values[index]), "%s m", number);
        if (!valid)
        {
            snprintf(values[index], sizeof(values[index]), "Unknown");
        }
    }
    const ComponentInfoRow rows[3] = { { "Length", values[0] }, { "Width", values[1] }, { "Height", values[2] } };
    const bool favorite = ObjectBrowser::IsFavorite(modelId);
    const auto result = DrawComponentInfoCard(
        "##model_info", innerW, valid && model.name && model.name[0] ? model.name : "Unknown", chips, chipCount, rows,
        [modelId]
        {
            return GetComponentModelImage(modelId);
        },
        g_icons ? g_icons->heartTexture : nullptr, favorite ? Theme::g_chipFavorited : Theme::g_textDim, valid, valid);
    if (result.action)
    {
        ObjectBrowser::ToggleFavorite(modelId);
        g_objectBrowserUi.dirty = true;
    }
    if (result.details)
    {
        OpenObjectBrowserViewDetails(modelId);
    }
}
