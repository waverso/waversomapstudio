// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    constexpr int MaxTxdLoadsPerFrame = 2;
    constexpr size_t MaxPinnedTxds = 64;
    constexpr ULONGLONG ThumbnailRetryMs = 1000;

    std::vector<RwTexture*> g_thumbnailTexture;
    std::vector<ULONGLONG> g_thumbnailRetryAt;

    struct PinnedTxd
    {
        std::uint16_t txdIdx;
        int slot;
        std::uint32_t lastUse;
    };
    std::vector<PinnedTxd> g_pinned;
    std::uint32_t g_frame = 0;
    int g_loadsThisFrame = 0;

    RwTexture* ReadTextureFromCurrentTxd(const char* name, const char* mask)
    {
        return plugin::CallAndReturn<RwTexture*, 0x7F3AC0>(name, mask);
    }

    PinnedTxd* FindPinned(std::uint16_t txdIdx)
    {
        for (auto& p : g_pinned)
        {
            if (p.txdIdx == txdIdx)
            {
                return &p;
            }
        }
        return nullptr;
    }

    void InvalidateTxdThumbs(std::uint16_t txdIdx)
    {
        const auto& view = CatalogDatabase::View();
        const auto& info = view.txdInfos[txdIdx];
        for (std::uint32_t r = info.firstTexture; r < (std::uint32_t)info.firstTexture + info.textureCount; ++r)
        {
            g_thumbnailTexture[r] = nullptr;
            g_thumbnailRetryAt[r] = 0;
        }
    }

    void PinTxd(std::uint16_t txdIdx, int slot)
    {
        if (PinnedTxd* p = FindPinned(txdIdx))
        {
            p->lastUse = g_frame;
            return;
        }

        CTxdStore::AddRef(slot);
        g_pinned.push_back({ txdIdx, slot, g_frame });

        while (g_pinned.size() > MaxPinnedTxds)
        {
            size_t oldest = SIZE_MAX;
            for (size_t i = 0; i < g_pinned.size(); ++i)
            {
                if (g_pinned[i].lastUse == g_frame)
                {
                    continue;
                }
                if (oldest == SIZE_MAX || g_pinned[i].lastUse < g_pinned[oldest].lastUse)
                {
                    oldest = i;
                }
            }
            if (oldest == SIZE_MAX)
            {
                break;
            }

            InvalidateTxdThumbs(g_pinned[oldest].txdIdx);
            CTxdStore::RemoveRef(g_pinned[oldest].slot);
            g_pinned.erase(g_pinned.begin() + oldest);
        }
    }
}

void TextureBrowser::BeginFrame()
{
    g_frame++;
    g_loadsThisFrame = 0;
}

RwTexture* TextureBrowser::GetThumb(std::uint32_t row)
{
    EnsureIndex();
    const auto& view = CatalogDatabase::View();
    if (row >= g_thumbnailRetryAt.size())
    {
        return nullptr;
    }
    const std::uint16_t txdIdx = g_rowToTxd[row];

    if (g_thumbnailTexture[row])
    {
        if (PinnedTxd* p = FindPinned(txdIdx))
        {
            p->lastUse = g_frame;
        }
        return g_thumbnailTexture[row];
    }
    const ULONGLONG now = GetTickCount64();
    if (now < g_thumbnailRetryAt[row])
    {
        return nullptr;
    }

    int slot = -1;
    const int modelId = g_txdToModel[txdIdx];
    if (modelId >= 0)
    {
        CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelId);
        if (mi)
        {
            slot = mi->m_nTxdIndex;
        }
    }
    if (slot < 0)
    {
        slot = CTxdStore::FindTxdSlot(CatalogDatabase::TxdName(txdIdx));
    }
    if (slot < 0)
    {
        g_thumbnailRetryAt[row] = now + ThumbnailRetryMs;
        return nullptr;
    }

    const char* texName = CatalogDatabase::TexName(view.textureRows[row].textureNameIndex);

    RwTexture* tex = nullptr;
    TxdDef* txd = CTxdStore::ms_pTxdPool->GetAt(slot);
    if (txd && txd->m_pRwDictionary)
    {
        tex = RwTexDictionaryFindNamedTexture(txd->m_pRwDictionary, texName);
    }

    if (!tex)
    {
        if (g_loadsThisFrame >= MaxTxdLoadsPerFrame)
        {
            return nullptr;
        }
        g_loadsThisFrame++;
        CStreaming::RequestTxdModel(slot, (eStreamingFlags::PRIORITY_REQUEST | eStreamingFlags::KEEP_IN_MEMORY));
        CStreaming::LoadAllRequestedModels(true);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(slot);
        tex = ReadTextureFromCurrentTxd(texName, nullptr);
        CTxdStore::PopCurrentTxd();
    }

    if (!tex)
    {
        g_thumbnailRetryAt[row] = now + ThumbnailRetryMs;
        return nullptr;
    }

    PinTxd(txdIdx, slot);
    g_thumbnailTexture[row] = tex;
    g_thumbnailRetryAt[row] = 0;
    return tex;
}

void TextureBrowser::ReleaseAll()
{
    for (const auto& p : g_pinned)
    {
        InvalidateTxdThumbs(p.txdIdx);
        CTxdStore::RemoveRef(p.slot);
    }
    g_pinned.clear();
    std::fill(g_thumbnailRetryAt.begin(), g_thumbnailRetryAt.end(), 0);
}
