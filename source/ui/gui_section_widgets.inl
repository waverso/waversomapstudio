// Source Repository: https://github.com/waverso/waversomapstudio

static void DrawHdrMoreMenuItems(const ComponentMenuCtx* context)
{
    InspectionHeaderMenu(context);
}

template <typename DrawMenu>
static bool DrawSectionHeader(
    const char* uid, const char* title, bool* p_open, float innerW, bool headerDisabled, DrawMenu drawMenu)
{
    ImGui::PushID(uid);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImRect bb(cur, ImVec2(cur.x + innerW, cur.y + Theme::g_headerHeight));

    ImGui::ItemSize(bb, Theme::g_groupSpacing);
    bool visible = ImGui::ItemAdd(bb, ImGui::GetID("##component_header"));

    const float moreSz = Theme::g_headerHeight;
    ImRect moreRect(ImVec2(bb.Max.x - moreSz, bb.Min.y), bb.Max);

    bool hovered = ImGui::IsItemHovered();
    bool overMore = ImGui::IsMouseHoveringRect(moreRect.Min, moreRect.Max);

    const ImU32 headerFill
        = headerDisabled ? Theme::g_controlBackground : (hovered ? Theme::g_hoveredHeaderBackground : Theme::g_headerBackground);
    const ImU32 headerBorder = headerDisabled ? Theme::g_border : (hovered ? Theme::g_borderStrong : Theme::g_border);
    const ImU32 headerText = headerDisabled ? Theme::g_textDim : (hovered ? Theme::g_textHover : Theme::g_text);
    const ImU32 accentBar = headerDisabled ? Theme::g_border : (*p_open ? Theme::g_accent : Theme::g_borderStrong);

    if (visible)
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !overMore)
        {
            *p_open = !*p_open;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("hdr_more");
        }

        dl->AddRectFilled(bb.Min, bb.Max, headerFill);
        dl->AddRect(bb.Min, bb.Max, headerBorder, 0.0f, 0, Ui::Hairline());
        dl->AddRectFilled(bb.Min, ImVec2(bb.Min.x + Theme::g_accentSize, bb.Max.y), accentBar);

        DrawRotatedArrow(dl,
            ImVec2(bb.Min.x + Theme::g_padding + Theme::g_smallIconSize * 0.5f, bb.Min.y + (Theme::g_headerHeight * 0.5f)),
            *p_open ? 1.0f : 0.0f, headerText);
        float ax = bb.Min.x + Theme::g_padding;
        dl->AddText(ImVec2(ax + Theme::g_smallIconSize + Theme::g_padding,
                        bb.Min.y + floorf((Theme::g_headerHeight - ImGui::GetFontSize()) * 0.5f)),
            headerText, title);

        ImGui::SetCursorScreenPos(moreRect.Min);
        ImGui::InvisibleButton("##more", ImVec2(moreSz, moreSz));
        if (ImGui::IsItemClicked())
        {
            ImGui::OpenPopup("hdr_more");
        }
        DrawIconCentered(dl, g_icons->moreTexture, moreRect.Min, moreRect.Max, Theme::g_smallIconSize,
            ImGui::IsItemHovered() ? Theme::g_textHover : Theme::g_textDim);
    }

    if (ImCustom::BeginPopup("hdr_more"))
    {
        drawMenu();
        ImCustom::EndPopup();
    }

    ImGui::PopID();
    return *p_open;
}

static bool DrawComponentHeader(
    const char* uid, const char* title, bool* p_open, float innerW, const ComponentMenuCtx* menuCtx, bool headerDisabled)
{
    return DrawSectionHeader(uid, title, p_open, innerW, headerDisabled,
        [&]
        {
            DrawHdrMoreMenuItems(menuCtx);
        });
}

static bool DrawEditorSectionHeader(const char* uid, const char* title, bool* p_open, float innerW, bool* resetRequested)
{
    return DrawSectionHeader(uid, title, p_open, innerW, false,
        [&]
        {
            if (resetRequested && ImGui::MenuItem("Restore Defaults"))
            {
                *resetRequested = true;
            }
        });
}

static bool DrawViewSectionHeader(const char* uid, const char* title, bool* p_open, float innerW)
{
    ImGui::PushID(uid);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImRect bb(cur, ImVec2(cur.x + innerW, cur.y + Theme::g_headerHeight));

    ImGui::ItemSize(bb, Theme::g_groupSpacing);
    bool visible = ImGui::ItemAdd(bb, ImGui::GetID("##view_header"));

    bool hovered = ImGui::IsItemHovered();
    const ImU32 headerFill = hovered ? Theme::g_hoveredHeaderBackground : Theme::g_headerBackground;
    const ImU32 headerBorder = hovered ? Theme::g_borderStrong : Theme::g_border;
    const ImU32 headerText = hovered ? Theme::g_textHover : Theme::g_text;
    const ImU32 accentBar = *p_open ? Theme::g_accent : Theme::g_borderStrong;

    if (visible)
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            *p_open = !*p_open;
        }

        dl->AddRectFilled(bb.Min, bb.Max, headerFill);
        dl->AddRect(bb.Min, bb.Max, headerBorder, 0.0f, 0, Ui::Hairline());
        dl->AddRectFilled(bb.Min, ImVec2(bb.Min.x + Theme::g_accentSize, bb.Max.y), accentBar);

        DrawRotatedArrow(dl,
            ImVec2(bb.Min.x + Theme::g_padding + Theme::g_smallIconSize * 0.5f, bb.Min.y + (Theme::g_headerHeight * 0.5f)),
            *p_open ? 1.0f : 0.0f, headerText);
        dl->AddText(ImVec2(bb.Min.x + Theme::g_padding + Theme::g_smallIconSize + Theme::g_padding,
                        bb.Min.y + floorf((Theme::g_headerHeight - ImGui::GetFontSize()) * 0.5f)),
            headerText, title);
    }

    ImGui::PopID();
    return *p_open;
}
