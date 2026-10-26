// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    int Gcd(int a, int b)
    {
        while (b)
        {
            int t = a % b;
            a = b;
            b = t;
        }
        return a ? a : 1;
    }

    bool HasCategoryMask(std::uint16_t texNameIdx, std::uint32_t categoryMask)
    {
        if (!categoryMask)
        {
            return true;
        }
        const auto& view = CatalogDatabase::View();
        if (texNameIdx >= view.textureMetas.size())
        {
            return false;
        }
        const std::uint16_t category = view.textureMetas[texNameIdx].category;
        return category < 32 && (categoryMask & (1u << category));
    }

    bool MatchesDimensionMask(const CatalogDatabase::TextureRow& row, std::uint16_t dimensionMask)
    {
        if (!dimensionMask)
        {
            return true;
        }
        const auto& aspects = CatalogDatabase::View().aspectRatios;
        for (int i = 0; i < (int)aspects.size() && i < 16; ++i)
        {
            if (!(dimensionMask & (std::uint16_t)(1u << i)))
            {
                continue;
            }
            if (row.aspectIndex == i)
            {
                return true;
            }
        }
        return false;
    }
}

void TextureBrowser::RunFilter(const Filter& f, std::vector<std::uint32_t>& out)
{
    EnsureIndex();
    const auto& view = CatalogDatabase::View();
    out.clear();

    int faw = 0, fah = 0;
    if (f.aspectW > 0 && f.aspectH > 0)
    {
        const int g = Gcd(f.aspectW, f.aspectH);
        faw = f.aspectW / g;
        fah = f.aspectH / g;
    }

    auto matches = [&](std::uint32_t r) -> bool
    {
        const auto& row = view.textureRows[r];
        const std::uint16_t txdIdx = g_rowToTxd[r];
        const auto& meta = view.textureMetas[row.textureNameIndex];
        const std::uint8_t source = view.txdInfos[txdIdx].sourceArchive;
        if (f.sourceMask && (source >= CatalogDatabase::ArchiveCount || !(f.sourceMask & (1u << source))))
        {
            return false;
        }
        if (f.tag >= 0 && meta.category != (std::uint16_t)f.tag)
        {
            return false;
        }
        if (!HasCategoryMask(row.textureNameIndex, f.tagMask))
        {
            return false;
        }
        if (!MatchesDimensionMask(row, f.dimensionMask))
        {
            return false;
        }
        if (f.mipMin >= 0 && row.mips < f.mipMin)
        {
            return false;
        }
        if (f.mipMax >= 0 && row.mips > f.mipMax)
        {
            return false;
        }
        if (f.sizeMinKb >= 0 && row.sizeKiB < f.sizeMinKb)
        {
            return false;
        }
        if (f.sizeMaxKb >= 0 && row.sizeKiB > f.sizeMaxKb)
        {
            return false;
        }
        if (f.wMin >= 0 && row.width < f.wMin)
        {
            return false;
        }
        if (f.wMax >= 0 && row.width > f.wMax)
        {
            return false;
        }
        if (f.hMin >= 0 && row.height < f.hMin)
        {
            return false;
        }
        if (f.hMax >= 0 && row.height > f.hMax)
        {
            return false;
        }
        if (faw > 0 && row.width > 0 && row.height > 0)
        {
            const int rg = Gcd(row.width, row.height);
            if (row.width / rg != faw || row.height / rg != fah)
            {
                return false;
            }
        }
        const bool lowLod = CatalogDatabase::TextureIsLowLod(row.textureNameIndex);
        if (f.lodFilter == BrowserFilter::Only && !lowLod)
        {
            return false;
        }
        if (f.lodFilter == BrowserFilter::Hide && lowLod)
        {
            return false;
        }
        if (f.seamless >= 0 && !CatalogDatabase::TextureIsSeamless(row.textureNameIndex, f.seamless))
        {
            return false;
        }
        if (f.search[0])
        {
            const std::uint8_t fields = f.searchFields ? f.searchFields : SearchTexture;
            bool found = false;
            if (fields & SearchTexture)
            {
                found = found || TextSearch::ContainsAsciiNoCase(CatalogDatabase::TexName(row.textureNameIndex), f.search);
            }
            if (fields & SearchLibrary)
            {
                found = found || TextSearch::ContainsAsciiNoCase(CatalogDatabase::TxdName(txdIdx), f.search);
            }
            if (fields & SearchDescription)
            {
                found = found
                    || TextSearch::ContainsAsciiNoCase(CatalogDatabase::TextureDescription(row.textureNameIndex), f.search);
            }
            if (!found)
            {
                return false;
            }
        }
        return true;
    };

    if (f.favoritesOnly)
    {
        for (std::uint32_t r : g_favorites)
        {
            if (r < view.textureRows.size() && matches(r))
            {
                out.push_back(r);
            }
        }
    }
    else if (f.hideDuplicates)
    {
        for (std::uint32_t r : g_dedupRows)
        {
            if (matches(r))
            {
                out.push_back(r);
            }
        }
    }
    else
    {
        for (std::uint32_t r = 0; r < (std::uint32_t)view.textureRows.size(); ++r)
        {
            if (matches(r))
            {
                out.push_back(r);
            }
        }
    }

    auto cmp = [&](std::uint32_t a, std::uint32_t b) -> bool
    {
        const auto& ra = view.textureRows[a];
        const auto& rb = view.textureRows[b];
        int c = 0;
        switch (f.sortKey)
        {
            case SortLibName:
                c = std::strcmp(CatalogDatabase::TxdName(g_rowToTxd[a]), CatalogDatabase::TxdName(g_rowToTxd[b]));
                break;
            case SortSize:
                c = (int)ra.sizeKiB - (int)rb.sizeKiB;
                break;
            case SortResolution:
            {
                const unsigned aa = (unsigned)ra.width * ra.height;
                const unsigned bb = (unsigned)rb.width * rb.height;
                c = (aa < bb) ? -1 : (aa > bb ? 1 : 0);
                break;
            }
            case SortMipmaps:
                c = (int)ra.mips - (int)rb.mips;
                break;
            case SortTextureName:
            default:
                c = std::strcmp(CatalogDatabase::TexName(ra.textureNameIndex), CatalogDatabase::TexName(rb.textureNameIndex));
                break;
        }
        if (c == 0)
        {
            c = std::strcmp(CatalogDatabase::TexName(ra.textureNameIndex), CatalogDatabase::TexName(rb.textureNameIndex));
            if (c == 0)
            {
                c = (a < b) ? -1 : (a > b ? 1 : 0);
            }
        }
        return f.sortDesc ? (c > 0) : (c < 0);
    };
    std::sort(out.begin(), out.end(), cmp);
}
