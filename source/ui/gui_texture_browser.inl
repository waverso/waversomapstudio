// Source Repository: https://github.com/waverso/waversomapstudio

static void DrawTextureBrowser()
{
    TexBrowserUiState& ui = g_textureBrowserUi;
    if (g_textureBrowserUi.viewOnly)
    {
        if (ui.open && ui.detailRow >= 0)
        {
            DrawTextureDetailModal();
        }
        if (!ui.open || ui.detailRow < 0)
        {
            g_textureBrowserUi.viewOnly = false;
            CloseTextureBrowser();
        }
        return;
    }
    if (!ui.open)
    {
        return;
    }

    if (ui.dirty)
    {
        TextureBrowser::Filter f;
        strncpy_s(f.search, sizeof(f.search), ui.search, _TRUNCATE);
        f.sourceMask = ui.sourceMask;
        f.tag = ui.tagFilter;
        f.tagMask = ui.categoryMask;
        f.dimensionMask = ui.dimensionMask;
        f.searchFields = ui.searchFields ? ui.searchFields : TextureBrowser::SearchDefault;
        f.seamless = ui.seamlessFilter;
        f.hideDuplicates = ui.hideDuplicates;
        f.lodFilter = ui.lodFilter;
        f.mipMin = ui.mipMin;
        f.mipMax = ui.mipMax;
        f.sizeMinKb = ui.sizeMinKb;
        f.sizeMaxKb = ui.sizeMaxKb;
        f.wMin = ui.wMin;
        f.wMax = ui.wMax;
        f.hMin = ui.hMin;
        f.hMax = ui.hMax;
        f.aspectW = ui.aspectW;
        f.aspectH = ui.aspectH;
        f.favoritesOnly = ui.favoritesOnly;
        f.sortKey = ui.sortKey;
        f.sortDesc = ui.sortDesc;
        TextureBrowser::RunFilter(f, ui.filtered);
        ui.dirty = false;
    }

    static const char* const sortNames[] = { "Name", "Library", "Size", "Resolution", "Mipmaps" };
    bool cancelled = false;
    const int selectedRow = DrawBrowserCatalog(
        "##TexBrowser", "Select Texture", "Search", "texture", ui, sortNames, TexBrowserFiltersPanelActive(ui),
        TexBrowserHasActiveFilters(ui), ui.detailRow >= 0 || ui.advOpen, OpenAdvFilters, ClearAllTexBrowserFilters,
        [](const std::uint32_t*, size_t)
        {
        },
        [](std::uint32_t row)
        {
            const auto info = TextureBrowser::GetRow(row);
            BrowserCell cell;
            cell.name = info.textureName;
            cell.favorite = TextureBrowser::IsFavorite(row);
            cell.image.texture = RwTextureToD3D(TextureBrowser::GetThumb(row));
            if (info.height)
            {
                cell.image.aspect = static_cast<float>(info.width) / info.height;
            }
            snprintf(cell.subtitle, sizeof(cell.subtitle), "%ux%u", static_cast<unsigned>(info.width),
                static_cast<unsigned>(info.height));
            snprintf(cell.tooltip, sizeof(cell.tooltip), "%s\n%s.txd\n%s\n%u mipmaps", info.textureName, info.txdName,
                TextureBrowser::SourceImgName(info.sourceImg), static_cast<unsigned>(info.mips));
            return cell;
        },
        TextureBrowser::ToggleFavorite, cancelled);
    if (selectedRow >= 0)
    {
        ui.detailRow = selectedRow;
    }

    DrawTextureDetailModal();
    DrawAdvFiltersModal();

    if (cancelled)
    {
        CloseTextureBrowser();
    }
}
