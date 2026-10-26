// Source Repository: https://github.com/waverso/waversomapstudio

struct ViewModshop
{
    const char* name;
    CVector position;
    CVector lookAt;
};

static const ViewModshop g_kViewModshops[] = {
    { "Pay 'n' Spray", { 1022.014f, -1054.041f, 40.231f }, { 1022.096f, -1053.086f, 39.947f } },
    { "Pay 'n' Spray", { 2091.976f, -1836.734f, 22.779f }, { 2091.069f, -1836.518f, 22.417f } },
    { "Pay 'n' Spray", { 486.595f, -1706.997f, 20.901f }, { 486.683f, -1707.945f, 20.594f } },
    { "Pay 'n' Spray", { 724.952f, -494.116f, 25.052f }, { 724.752f, -493.166f, 24.812f } },
    { "Pay 'n' Spray", { -1918.244f, 241.444f, 52.642f }, { -1917.91f, 242.359f, 52.416f } },
    { "Pay 'n' Spray", { -2410.656f, 1060.853f, 59.621f }, { -2411.019f, 1059.945f, 59.408f } },
    { "Pay 'n' Spray (Michelle's)", { -1797.033f, 1180.651f, 33.613f }, { -1796.752f, 1181.579f, 33.368f } },
    { "Pay 'n' Spray (Closed)", { 2397.352f, 1449.923f, 23.881f }, { 2397.224f, 1450.872f, 23.592f } },
    { "Pay 'n' Spray", { 1938.702f, 2151.146f, 24.002f }, { 1939.567f, 2151.52f, 23.664f } },
    { "Pay 'n' Spray", { -112.5f, 1083.248f, 36.973f }, { -112.146f, 1084.077f, 36.54f } },
    { "Pay 'n' Spray", { -1407.6f, 2624.959f, 70.307f }, { -1407.941f, 2624.075f, 69.987f } },
    { "TransFender", { 1051.381f, -1046.237f, 39.356f }, { 1050.922f, -1045.401f, 39.055f } },
    { "TransFender", { -1945.529f, 207.111f, 44.59f }, { -1945.288f, 208.048f, 44.338f } },
    { "TransFender", { 2395.551f, 1011.055f, 22.804f }, { 2395.242f, 1011.952f, 22.489f } },
    { "Wheel Arch Angels", { -2692.374f, 209.05f, 15.255f }, { -2693.26f, 209.329f, 14.884f } },
    { "Loco Low Co.", { 2646.667f, -2010.17f, 22.016f }, { 2646.631f, -2011.131f, 21.742f } },
    { "Bomb Shop", { 1815.999f, -1860.318f, 24.108f }, { 1816.919f, -1860.147f, 23.756f } },
    { "Bomb Shop", { 1996.841f, 2281.064f, 16.343f }, { 1997.178f, 2281.98f, 16.127f } },
};

static const char* ModshopZoneName(int id)
{
    const auto& position = g_kViewModshops[id].position;
    return Zones::GetZoneName(Zones::GetZoneAtPoint2D(position.x, position.y));
}

static void FlyToModshop(int id)
{
    const auto& shop = g_kViewModshops[id];
    const CVector direction = shop.lookAt - shop.position;
    const float yaw = atan2f(-direction.x, direction.y);
    const float pitch = asinf(std::clamp(direction.z / direction.Magnitude(), -1.0f, 1.0f));
    g_freeCamera->StartFlyTo(shop.position, yaw, pitch);
    g_renderInteriors = false;
    if (g_scenePicker)
    {
        g_scenePicker->DropOutOfAreaSelection();
    }
}

static void FlyToMapRegion(int id)
{
    const CVector& c = Zones::GetZoneCenter(id);
    const float minZ = Zones::GetZoneMinZ(id);
    const float maxZ = Zones::GetZoneMaxZ(id);
    const CVector target(c.x, c.y, minZ + (maxZ - minZ) * 0.4f);
    float yaw, pitch;
    g_freeCamera->GetYawPitch(yaw, pitch);
    const float flyPitch = -0.7853981f;
    const CVector dir(-sinf(yaw) * cosf(flyPitch), cosf(yaw) * cosf(flyPitch), sinf(flyPitch));
    const float standoff = std::clamp(Zones::GetZoneExtentXY(id) * 1.2f, 30.0f, 400.0f);
    g_freeCamera->StartFlyTo(target - dir * standoff, yaw, flyPitch);
    g_renderInteriors = false;
    if (g_scenePicker)
    {
        g_scenePicker->DropOutOfAreaSelection();
    }
}

static void FlyToInterior(int id)
{
    const Interiors::Interior& it = Interiors::Get(id);
    const CVector target(it.x, it.y, it.z);
    g_renderInteriors = true;

    if (it.lx != 0.0f || it.ly != 0.0f || it.lz != 0.0f)
    {
        const CVector d(it.lx - it.x, it.ly - it.y, it.lz - it.z);
        const float len = d.Magnitude();
        if (len > 1e-4f)
        {
            const float yaw = atan2f(-d.x, d.y);
            const float pitch = std::clamp(asinf(std::clamp(d.z / len, -1.0f, 1.0f)), -1.55f, 1.55f);
            g_freeCamera->StartFlyTo(target, yaw, pitch);
            return;
        }
    }

    float yaw, pitch;
    g_freeCamera->GetYawPitch(yaw, pitch);
    const float flyPitch = -0.7853981f;
    const CVector dir(-sinf(yaw) * cosf(flyPitch), cosf(yaw) * cosf(flyPitch), sinf(flyPitch));
    g_freeCamera->StartFlyTo(target - dir * 7.0f, yaw, flyPitch);
}

static void DrawViewMapRegions(ViewUiState& ui, float innerW)
{
    const float comboW = Ui::Px(110.0f);
    const float searchW = innerW - comboW - Theme::g_padding;

    SearchInput("##loc_search", "Search", ui.locSearch, sizeof(ui.locSearch), searchW, ui.locFocused);
    ImGui::SameLine(0, Theme::g_padding);
    {
        const char* modes[] = { "Map Regions", "Interiors", "Modshops" };
        if (ImCustom::BeginCombo("##places_mode", modes[ui.placesMode], comboW))
        {
            for (int m = 0; m < IM_ARRAYSIZE(modes); m++)
            {
                if (ImCustom::ComboItem(modes[m], ui.placesMode == m))
                {
                    ui.placesMode = m;
                }
            }
            ImCustom::EndCombo();
        }
    }

    const bool interiors = ui.placesMode == 1;
    const bool modshops = ui.placesMode == 2;
    const int total = modshops ? IM_ARRAYSIZE(g_kViewModshops) : interiors ? Interiors::InteriorCount : Zones::ZoneCount;

    int matches[256];
    int nMatches = 0;
    for (int i = 0; i < total && nMatches < 256; i++)
    {
        const char* name = modshops ? g_kViewModshops[i].name : interiors ? Interiors::GetName(i) : Zones::GetZoneName(i);
        if (TextSearch::ContainsAsciiNoCase(name, ui.locSearch)
            || (modshops && TextSearch::ContainsAsciiNoCase(ModshopZoneName(i), ui.locSearch)))
        {
            matches[nMatches++] = i;
        }
    }

    const float rowH = Ui::Px(21.0f);
    float below = Theme::g_padding + Theme::g_headerHeight + Theme::g_padding;
    if (ui.bmOpen)
    {
        below += Theme::g_frameHeight + Theme::g_padding + ViewBmListHeight() + Theme::g_padding;
    }
    const float fillAvail = ImGui::GetContentRegionAvail().y - ImGui::GetScrollY();
    const float listH = ImMax(Ui::Px(64.0f), fillAvail - below);
    const float fontSize = ImGui::GetFontSize();

    ImDrawList* pdl = ImGui::GetWindowDrawList();
    const ImVec2 clipMn = pdl->GetClipRectMin();
    const ImVec2 clipMx = pdl->GetClipRectMax();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::g_alternatePanelBackground);
    ImVec2 listMn(0, 0), listMx(0, 0);
    ImGui::BeginChild("##loc_list", ImVec2(innerW, listH), false);
    {
        listMn = ImGui::GetWindowPos();
        listMx = ImVec2(listMn.x + ImGui::GetWindowSize().x, listMn.y + ImGui::GetWindowSize().y);

        if (nMatches == 0)
        {
            DrawCenteredMessage(modshops ? "No modshops found." : interiors ? "No interiors found." : "No map regions found.");
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float w = ImGui::GetContentRegionAvail().x;

            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGuiListClipper clipper;
            clipper.Begin(nMatches, rowH);
            while (clipper.Step())
            {
                for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++)
                {
                    const int id = matches[n];
                    ImGui::PushID(id);

                    ImGui::SetCursorPos(ImVec2(0, n * rowH));
                    const ImVec2 rmn = ImGui::GetCursorScreenPos();
                    const ImVec2 rmx = ImVec2(rmn.x + w, rmn.y + rowH);
                    const bool pressed = ImGui::InvisibleButton("##loc_row", ImVec2(w, rowH));
                    const bool hovered = ImGui::IsItemHovered();

                    if (hovered)
                    {
                        dl->AddRectFilled(rmn, rmx, Theme::g_hoveredControlBackground);
                    }
                    else if (n & 1)
                    {
                        dl->AddRectFilled(rmn, rmx, Theme::g_alternateListRowBackground);
                    }

                    char tagBuf[24];
                    const char* tag;
                    const char* name;
                    if (modshops)
                    {
                        tag = ModshopZoneName(id);
                        name = g_kViewModshops[id].name;
                    }
                    else if (interiors)
                    {
                        const Interiors::Interior& it = Interiors::Get(id);
                        snprintf(tagBuf, sizeof(tagBuf), "Interior %d", it.interior);
                        tag = tagBuf;
                        name = it.name;
                    }
                    else
                    {
                        tag = Zones::GetZoneRegionName(id);
                        name = Zones::GetZoneName(id);
                    }

                    const ImVec2 rs = ImGui::CalcTextSize(tag);
                    const float textY = rmn.y + floorf((rowH - fontSize) * 0.5f);
                    dl->AddText(ImVec2(rmx.x - Theme::g_padding - rs.x, textY), Theme::g_textDim, tag);
                    const ImVec4 nameClip(rmn.x, rmn.y, rmx.x - Theme::g_padding * 2.0f - rs.x, rmx.y);
                    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(rmn.x + Theme::g_padding, textY),
                        hovered ? Theme::g_textHover : Theme::g_text, name, nullptr, 0.0f, &nameClip);

                    if (pressed)
                    {
                        if (modshops)
                        {
                            FlyToModshop(id);
                        }
                        else if (interiors)
                        {
                            FlyToInterior(id);
                        }
                        else
                        {
                            FlyToMapRegion(id);
                        }
                    }

                    ImGui::PopID();
                }
            }
            ImGui::PopStyleVar();
        }
        DrawClippedListBorder(listMn, listMx, clipMn, clipMx);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}
