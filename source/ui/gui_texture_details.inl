// Source Repository: https://github.com/waverso/waversomapstudio

static IDirect3DTexture9* RwTextureToD3D(RwTexture* tex)
{
    if (!tex || !tex->raster)
    {
        return nullptr;
    }
    return *reinterpret_cast<IDirect3DTexture9**>(tex->raster + 1);
}

static void DrawTextureDetailModal()
{
    TexBrowserUiState& ui = g_textureBrowserUi;
    if (ui.detailRow < 0)
    {
        ui.previewWasOpen = false;
        return;
    }
    const bool previewJustOpened = !ui.previewWasOpen;
    ui.previewWasOpen = true;

    const TextureBrowser::RowInfo info = TextureBrowser::GetRow((std::uint32_t)ui.detailRow);
    const ImGuiIO& io = ImGui::GetIO();

    DrawModalBackdrop("##TexDetailBackdrop");

    const bool isFav = TextureBrowser::IsFavorite((std::uint32_t)ui.detailRow);

    ImFont* font = ImGui::GetFont();
    ImFont* smallFont = g_gui->m_fontSmall;
    const float fontSize = ScaledFontSize(font);
    const float sm = ScaledFontSize(smallFont);

    const float winW = Ui::Px(450.0f);
    const float innerW = winW - Theme::g_outerPadding * 2.0f;
    const float previewW = innerW;

    float previewH = previewW;
    if (info.width > 0 && info.height > 0)
    {
        const float aspect = (float)info.width / (float)info.height;
        previewH = aspect >= 1.0f ? previewW / ImClamp(aspect, 1.0f, 4.0f) : previewW;
    }
    previewH = ImClamp(previewH, previewW * 0.25f, previewW);

    const bool isLowLod = info.isLowLod;
    const bool isVerticalSeamless = CatalogDatabase::TextureIsSeamless(info.texNameIdx, 0);
    const bool isHorizontalSeamless = CatalogDatabase::TextureIsSeamless(info.texNameIdx, 1);
    const bool isSeamless = isVerticalSeamless || isHorizontalSeamless;
    const char* description = info.description ? info.description : "";
    const bool hasDescription = description[0] != 0;
    const int chipCountForHeight = (isLowLod ? 1 : 0) + (isFav ? 1 : 0) + (isSeamless ? 1 : 0);

    const float cardPadY = Ui::Px(7.0f);
    const float nameH = fontSize;
    const float nameGap = Ui::Px(7.0f);
    const float chipPadX = Ui::Px(10.0f);
    const float chipH = sm + Ui::Px(8.0f);
    const float chipGap = Ui::Px(6.0f);
    const float rowGap = Ui::Px(7.0f);
    const float sectionGap = Ui::Px(9.0f);
    const float infoRowH = sm + Ui::Px(8.0f);
    const float sepGap = Ui::Px(7.0f);
    const float infoCardH = cardPadY + nameH + (chipCountForHeight > 0 ? nameGap + chipH : 0.0f) + sectionGap + infoRowH
        + (hasDescription ? rowGap + infoRowH : 0.0f) + sepGap + Ui::Hairline() + sepGap + infoRowH * 2.0f + cardPadY;

    const float winH = ModalHeight(previewH + Theme::g_itemSpacing + infoCardH);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(winW, ceilf(winH)), ImGuiCond_Always);
    PushModalStyle();

    bool applied = false;
    bool closed = false;

    if (ImGui::Begin("##TexDetailModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 winPos = ImGui::GetWindowPos();

        DrawModalTitleBar(dl, winPos, winW, "Texture Details");

        const ImVec2 pvMin(winPos.x + Theme::g_outerPadding, winPos.y + Theme::g_tabHeight + Theme::g_padding);
        const ImVec2 pvMax(pvMin.x + previewW, pvMin.y + previewH);
        const ImVec2 boxC((pvMin.x + pvMax.x) * 0.5f, (pvMin.y + pvMax.y) * 0.5f);

        float baseW, baseH;
        if (info.width > 0 && info.height > 0)
        {
            const float s = ImMin(previewW / (float)info.width, previewH / (float)info.height);
            baseW = (float)info.width * s;
            baseH = (float)info.height * s;
        }
        else
        {
            baseW = previewW;
            baseH = previewH;
        }

        if (ui.previewLastRow != ui.detailRow)
        {
            ui.previewZoom = ui.previewZoomTarget = 1.0f;
            ui.previewPan = ui.previewPanTarget = ImVec2(0.0f, 0.0f);
            ui.previewLastRow = ui.detailRow;
        }
        else if (previewJustOpened)
        {
            const float dw = baseW * ui.previewZoom, dh = baseH * ui.previewZoom;
            const ImVec2 ic(boxC.x + ui.previewPan.x, boxC.y + ui.previewPan.y);
            const bool visible = (ic.x + dw * 0.5f) > pvMin.x && (ic.x - dw * 0.5f) < pvMax.x && (ic.y + dh * 0.5f) > pvMin.y
                && (ic.y - dh * 0.5f) < pvMax.y;
            if (!visible)
            {
                ui.previewZoom = ui.previewZoomTarget = 1.0f;
                ui.previewPan = ui.previewPanTarget = ImVec2(0.0f, 0.0f);
            }
        }

        ImGui::SetCursorScreenPos(pvMin);
        ImGui::InvisibleButton("##texdetail_preview_input", ImVec2(previewW, previewH),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
        const bool previewHovered = ImGui::IsItemHovered();
        const bool previewActive = ImGui::IsItemActive();
        const bool previewDragging = previewActive
            && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)
                || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f));
        if (previewDragging)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Drag);
        }

        static double wheelAt = -1e9;
        if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_R, false))
        {
            ui.previewZoomTarget = 1.0f;
            ui.previewPanTarget = ImVec2(0.0f, 0.0f);
        }
        if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            TextureBrowser::ToggleFavorite((std::uint32_t)ui.detailRow);
        }
        if (ImGui::GetTime() - wheelAt >= 0.5 && !io.MouseDown[0] && !io.MouseDown[1] && !io.MouseDown[2]
            && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            SetStyledTooltip("Reset View (R)\nFavorite (F)");
        }

        if ((previewHovered || previewActive) && io.MouseWheel != 0.0f)
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
            const float oldZoom = ui.previewZoomTarget;
            const float newZoom = ImClamp(oldZoom * powf(step, io.MouseWheel), 0.1f, 20.0f);
            if (newZoom != oldZoom)
            {
                const ImVec2 mp = io.MousePos;
                const float k = newZoom / oldZoom;
                ui.previewPanTarget.x = (boxC.x + ui.previewPanTarget.x - mp.x) * k + mp.x - boxC.x;
                ui.previewPanTarget.y = (boxC.y + ui.previewPanTarget.y - mp.y) * k + mp.y - boxC.y;
                ui.previewZoomTarget = newZoom;
            }
        }

        if (previewDragging)
        {
            ui.previewPanTarget.x += io.MouseDelta.x;
            ui.previewPanTarget.y += io.MouseDelta.y;
        }

        {
            const float dt = ImClamp(io.DeltaTime, 0.0f, 0.05f);
            const float k = 1.0f - expf(-dt * 28.0f);
            ui.previewZoom += (ui.previewZoomTarget - ui.previewZoom) * k;
            ui.previewPan.x += (ui.previewPanTarget.x - ui.previewPan.x) * k;
            ui.previewPan.y += (ui.previewPanTarget.y - ui.previewPan.y) * k;
            if (fabsf(ui.previewZoomTarget - ui.previewZoom) < 0.0005f)
            {
                ui.previewZoom = ui.previewZoomTarget;
            }
            if (fabsf(ui.previewPanTarget.x - ui.previewPan.x) < 0.1f)
            {
                ui.previewPan.x = ui.previewPanTarget.x;
            }
            if (fabsf(ui.previewPanTarget.y - ui.previewPan.y) < 0.1f)
            {
                ui.previewPan.y = ui.previewPanTarget.y;
            }
        }

        dl->AddRectFilled(pvMin, pvMax, Theme::g_controlBackground);
        if (IDirect3DTexture9* d3d = RwTextureToD3D(TextureBrowser::GetThumb((std::uint32_t)ui.detailRow)))
        {
            const float drawW = baseW * ui.previewZoom;
            const float drawH = baseH * ui.previewZoom;
            const ImVec2 ic(boxC.x + ui.previewPan.x, boxC.y + ui.previewPan.y);
            dl->PushClipRect(pvMin, pvMax, true);
            dl->AddImage((ImTextureID)d3d, ImVec2(ic.x - drawW * 0.5f, ic.y - drawH * 0.5f),
                ImVec2(ic.x + drawW * 0.5f, ic.y + drawH * 0.5f));
            dl->PopClipRect();
        }
        dl->AddRect(pvMin, pvMax, Theme::g_border, 0.0f, 0, Ui::Hairline());

        char tagsBuf[128];
        TextureBrowser::GetTagsSummary(info.texNameIdx, tagsBuf, sizeof(tagsBuf));

        char valLib[40], valRes[32], valSize[24], valMip[16], valModel[64], valDim[24];
        snprintf(valLib, sizeof(valLib), "%s.txd", info.txdName);
        snprintf(valRes, sizeof(valRes), "%ux%u", info.width, info.height);
        snprintf(valSize, sizeof(valSize), "%u KB", info.sizeKb);
        snprintf(valMip, sizeof(valMip), "%u", info.mips);
        if (info.width > 0 && info.height > 0)
        {
            unsigned int a = info.width, b = info.height;
            while (b != 0)
            {
                const unsigned int t = a % b;
                a = b;
                b = t;
            }
            snprintf(valDim, sizeof(valDim), "%u:%u", info.width / a, info.height / a);
        }
        else
        {
            snprintf(valDim, sizeof(valDim), "Unknown");
        }
        if (info.modelId >= 0)
        {
            char mname[48];
            TextureBrowser::GetModelDisplay(info.modelId, mname, sizeof(mname));
            snprintf(valModel, sizeof(valModel), "%d - %s", info.modelId, mname);
        }
        else
        {
            snprintf(valModel, sizeof(valModel), "None");
        }

        const ImVec2 cardMin(winPos.x + Theme::g_outerPadding, pvMax.y + Theme::g_itemSpacing);
        const ImVec2 cardMax(cardMin.x + innerW, cardMin.y + infoCardH);
        dl->AddRectFilled(cardMin, cardMax, Theme::g_infoCardBackground);
        dl->AddRect(cardMin, cardMax, Theme::g_border, 0.0f, 0, Ui::Hairline());

        float cardY = cardMin.y + cardPadY;
        const float nameW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, info.textureName).x;
        const float namePad = ImMax(0.0f, (innerW - nameW) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardY));
        SelectableField("##texdetail_name", info.textureName, innerW, fontSize, font, Theme::g_text, namePad, false);
        cardY += nameH;

        struct Chip
        {
            const char* label;
            ImU32 col;
            float w;
        };
        const char* seamlessLabel = isVerticalSeamless && isHorizontalSeamless
            ? "Seamless (Both)"
            : (isHorizontalSeamless ? "Seamless (Horizontal)" : "Seamless (Vertical)");
        const ImU32 seamlessColor = isVerticalSeamless && isHorizontalSeamless
            ? Theme::g_chipSeamlessBoth
            : (isHorizontalSeamless ? Theme::g_chipSeamlessHor : Theme::g_chipSeamlessVer);
        Chip chipSrc[3] = { { "Low LOD", Theme::g_chipLowLod, 0.0f }, { "Favorited", Theme::g_chipFavorited, 0.0f },
            { seamlessLabel, seamlessColor, 0.0f } };
        bool chipOn[3] = { isLowLod, isFav, isSeamless };
        Chip chips[3];
        int chipCount = 0;
        for (int i = 0; i < 3; ++i)
        {
            if (!chipOn[i])
            {
                continue;
            }
            chips[chipCount] = chipSrc[i];
            chips[chipCount].w = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chips[chipCount].label).x + chipPadX * 2.0f;
            chipCount++;
        }

        if (chipCount > 0)
        {
            cardY += nameGap;
            float chipsW = 0.0f;
            for (int i = 0; i < chipCount; ++i)
            {
                chipsW += chips[i].w;
            }
            chipsW += (chipCount - 1) * chipGap;
            float chipX = cardMin.x + (innerW - chipsW) * 0.5f;
            for (int i = 0; i < chipCount; ++i)
            {
                dl->AddRectFilled(ImVec2(chipX, cardY), ImVec2(chipX + chips[i].w, cardY + chipH),
                    (chips[i].col & IM_COL32(255, 255, 255, 0)) | IM_COL32(0, 0, 0, 32));
                const float tw = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chips[i].label).x;
                dl->AddText(smallFont, sm, ImVec2(chipX + (chips[i].w - tw) * 0.5f, cardY + (chipH - sm) * 0.5f), chips[i].col,
                    chips[i].label);
                chipX += chips[i].w + chipGap;
            }
            cardY += chipH;
        }

        cardY += sectionGap;

        const float valPadX = Ui::Px(2.0f);
        const float leftX = cardMin.x + Theme::g_padding;
        const float availW = innerW - Theme::g_padding * 2.0f;

        auto drawTextCell = [&](float cellX, float cellY, float cellW, const char* label, const char* value, const char* id)
        {
            dl->AddText(smallFont, sm, ImVec2(cellX, cellY), Theme::g_textDim, label);
            const float valueW = ImMin(cellW, smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, value).x + valPadX);
            ImGui::SetCursorScreenPos(ImVec2(cellX + cellW - valueW, cellY));
            SelectableField(id, value, valueW, sm, smallFont, Theme::g_text, valPadX, false);
        };
        auto drawDescriptionCell = [&](float cellX, float cellY, float cellW, const char* value)
        {
            const char* label = "Description";
            dl->AddText(smallFont, sm, ImVec2(cellX, cellY), Theme::g_textDim, label);
            const float labelW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, label).x;
            const float maxValueW = ImMax(1.0f, cellW - labelW - Theme::g_padding);
            const float fullValueW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, value).x + valPadX;
            const float valueW = ImMin(maxValueW, fullValueW);
            const float valueX = cellX + cellW - valueW;
            char visible[256];
            snprintf(visible, sizeof(visible), "%s", value);
            bool truncated = fullValueW > maxValueW;
            if (truncated)
            {
                ImGui::PushFont(smallFont);
                EllipsizeText(visible, sizeof(visible), value, valueW - valPadX, sm);
                ImGui::PopFont();
            }
            ImGui::SetCursorScreenPos(ImVec2(valueX, cellY));
            SelectableField("##texdetail_desc", visible, valueW, sm, smallFont, Theme::g_text, valPadX, false);
            if (truncated && ImGui::IsItemHovered())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_popupPadding));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
                if (ImGui::BeginTooltip())
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + Ui::Px(360.0f));
                    ImGui::TextUnformatted(value);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                ImGui::PopStyleVar(2);
            }
        };

        const float topColGap = Theme::g_padding;
        const float topColW = (availW - topColGap) * 0.5f;
        drawTextCell(leftX, cardY, topColW, "Library", valLib, "##texdetail_lib");
        drawTextCell(leftX + topColW + topColGap, cardY, topColW, "Model", valModel, "##texdetail_model");
        cardY += infoRowH;

        if (hasDescription)
        {
            cardY += rowGap;
            drawDescriptionCell(leftX, cardY, availW, description);
            cardY += infoRowH;
        }

        cardY += sepGap;
        dl->AddLine(ImVec2(leftX, cardY), ImVec2(leftX + availW, cardY), Theme::g_border, Ui::Hairline());
        cardY += Ui::Hairline() + sepGap;

        const float bottomColGap = Theme::g_padding;
        const float bottomColW = (availW - bottomColGap * 2.0f) / 3.0f;
        drawTextCell(leftX, cardY, bottomColW, "Source", TextureBrowser::SourceImgName(info.sourceImg), "##texdetail_source");
        drawTextCell(leftX + (bottomColW + bottomColGap), cardY, bottomColW, "Category", tagsBuf, "##texdetail_category");
        drawTextCell(leftX + (bottomColW + bottomColGap) * 2.0f, cardY, bottomColW, "Resolution", valRes, "##texdetail_res");
        cardY += infoRowH;
        drawTextCell(leftX, cardY, bottomColW, "Aspect Ratio", valDim, "##texdetail_dim");
        drawTextCell(leftX + (bottomColW + bottomColGap), cardY, bottomColW, "Size", valSize, "##texdetail_size");
        drawTextCell(leftX + (bottomColW + bottomColGap) * 2.0f, cardY, bottomColW, "Mipmaps", valMip, "##texdetail_mip");

        ImGui::SetCursorScreenPos(cardMin);
        ImGui::Dummy(ImVec2(innerW, infoCardH));

        DrawModalFooter();
        const float btnW = (innerW - Theme::g_padding) * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
        if (ui.viewOnly)
        {
            if (ImGui::Button("Close##texdetail", ImVec2(innerW, Theme::g_frameHeight)))
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
            const bool canApply = info.modelId >= 0;
            if (canApply && !io.WantTextInput
                && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            {
                applied = true;
            }
            ImGui::BeginDisabled(!canApply);
            if (ImGui::Button("Apply##texdetail", ImVec2(btnW, Theme::g_frameHeight)))
            {
                applied = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0.0f, Theme::g_padding);
            if (ImGui::Button("Close##texdetail", ImVec2(btnW, Theme::g_frameHeight)))
            {
                closed = true;
            }
        }
        ImGui::PopStyleVar();
    }
    ImGui::End();
    PopModalStyle();

    if (applied)
    {
        if (CommitMaterialTexture(ui.itemIdx, ui.matSlot, info.modelId, info.txdName, info.textureName))
        {
            CloseTextureBrowser();
        }
    }
    else if (closed)
    {
        ui.detailRow = -1;
    }
}

static void OpenComponentTextureDetails(int row, int itemIdx, int materialSlot)
{
    InspectionFreezeTexture(itemIdx, materialSlot);
    ImGui::ClearActiveID();
    OpenTextureBrowser(itemIdx, materialSlot);
    g_textureBrowserUi.detailRow = row;
    g_textureBrowserUi.previewWasOpen = false;
    g_textureBrowserUi.viewOnly = true;
}

static void DrawMaterialTextureInfoCard(const MaterialData& data, float innerW, int materialSlot, int itemIdx)
{
    const int row = TextureBrowser::FindRow(data.txdName, data.textureName);
    const bool indexed = row >= 0;
    const auto info = indexed ? TextureBrowser::GetRow(static_cast<std::uint32_t>(row)) : TextureBrowser::RowInfo {};
    auto getImage = [&]
    {
        RwTexture* texture = nullptr;
        if (g_objectRuntime && g_scene && itemIdx >= 0 && itemIdx < g_scene->count && materialSlot >= 0 && materialSlot < 16)
        {
            RwTexture* candidate = g_objectRuntime->Runtime(itemIdx).mats[materialSlot].tex;
            if (candidate && _stricmp(candidate->name, data.textureName) == 0)
            {
                texture = candidate;
            }
        }
        if (indexed)
        {
            if (RwTexture* thumbnail = TextureBrowser::GetThumb(static_cast<std::uint32_t>(row)))
            {
                texture = thumbnail;
            }
        }
        ComponentInfoImage image;
        image.texture = RwTextureToD3D(texture);
        if (indexed && info.height)
        {
            image.aspect = static_cast<float>(info.width) / info.height;
        }
        else if (texture && texture->raster && texture->raster->height > 0)
        {
            image.aspect = static_cast<float>(texture->raster->width) / texture->raster->height;
        }
        return image;
    };

    char library[40], resolution[32], mipmaps[16];
    snprintf(library, sizeof(library), "%s.txd", data.txdName);
    if (indexed)
    {
        snprintf(resolution, sizeof(resolution), "%ux%u", static_cast<unsigned>(info.width), static_cast<unsigned>(info.height));
        snprintf(mipmaps, sizeof(mipmaps), "%u", static_cast<unsigned>(info.mips));
    }
    else
    {
        snprintf(resolution, sizeof(resolution), "Unknown");
        snprintf(mipmaps, sizeof(mipmaps), "Unknown");
    }
    const ComponentInfoRow rows[3]
        = { { "Library", data.txdName[0] ? library : "No Library" }, { "Resolution", resolution }, { "Mipmaps", mipmaps } };
    ComponentInfoChip chips[2];
    int chipCount = 0;
    if (indexed)
    {
        if (info.isLowLod)
        {
            chips[chipCount++] = { "Low LOD", Theme::g_chipLowLod };
        }
        const bool vertical = CatalogDatabase::TextureIsSeamless(info.texNameIdx, 0);
        const bool horizontal = CatalogDatabase::TextureIsSeamless(info.texNameIdx, 1);
        if (vertical || horizontal)
        {
            chips[chipCount++] = { vertical && horizontal ? "Seamless (Both)"
                    : horizontal                          ? "Seamless (Horizontal)"
                                                          : "Seamless (Vertical)",
                vertical && horizontal ? Theme::g_chipSeamlessBoth
                    : horizontal       ? Theme::g_chipSeamlessHor
                                       : Theme::g_chipSeamlessVer };
        }
    }
    const bool favorite = indexed && TextureBrowser::IsFavorite(static_cast<std::uint32_t>(row));
    const auto result = DrawComponentInfoCard("##texture_info", innerW, data.textureName, chips, chipCount, rows, getImage,
        g_icons ? g_icons->heartTexture : nullptr, favorite ? Theme::g_chipFavorited : Theme::g_textDim, indexed, indexed);
    if (result.action)
    {
        TextureBrowser::ToggleFavorite(static_cast<std::uint32_t>(row));
        g_textureBrowserUi.dirty = true;
    }
    else if (result.details)
    {
        OpenComponentTextureDetails(row, itemIdx, materialSlot);
    }
}
