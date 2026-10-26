// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    RwTexture* ReadTextureFromCurrentTxd(const char* name, const char* mask)
    {
        return plugin::CallAndReturn<RwTexture*, 0x7F3AC0>(name, mask);
    }

    struct TextureCacheEntry
    {
        int txdSlot = -1;
        char textureName[128] = {};
        RwTexture* tex = nullptr;
        mutable int refs = 0;
    };

    class TextureCache
    {
        struct IndexedEntry : TextureCacheEntry
        {
            std::uint64_t ordinal;
        };
        struct Key
        {
            int txdSlot;
            const char* textureName;
            std::uint64_t ordinal = 0;
        };
        struct ByKey
        {
            using is_transparent = void;
            template <class Left, class Right>
            bool operator()(const Left& left, const Right& right) const
            {
                if (left.txdSlot != right.txdSlot)
                {
                    return left.txdSlot < right.txdSlot;
                }
                const int names = _stricmp(left.textureName, right.textureName);
                return names != 0 ? names < 0 : left.ordinal < right.ordinal;
            }
        };
        struct ByPointer
        {
            using is_transparent = void;
            bool operator()(const IndexedEntry* left, const IndexedEntry* right) const
            {
                return left->tex != right->tex ? std::less<RwTexture*> {}(left->tex, right->tex) : left->ordinal < right->ordinal;
            }
            bool operator()(const IndexedEntry* left, RwTexture* right) const
            {
                return std::less<RwTexture*> {}(left->tex, right);
            }
            bool operator()(RwTexture* left, const IndexedEntry* right) const
            {
                return std::less<RwTexture*> {}(left, right->tex);
            }
        };
        struct Index
        {
            std::set<IndexedEntry, ByKey> entries;
            std::set<const IndexedEntry*, ByPointer> pointers;
        };
        static constexpr size_t IndexThreshold = 4096;
        std::vector<TextureCacheEntry> m_entries;
        std::unique_ptr<Index> m_index;
        std::uint64_t m_nextOrdinal = 1;
        bool m_indexFailed = false;

        public:
        const TextureCacheEntry* Find(int txdSlot, const char* textureName) const
        {
            if (m_index)
            {
                const auto found = m_index->entries.lower_bound(Key { txdSlot, textureName });
                return found != m_index->entries.end() && found->txdSlot == txdSlot
                        && _stricmp(found->textureName, textureName) == 0
                    ? &*found
                    : nullptr;
            }
            for (const auto& entry : m_entries)
            {
                if (entry.txdSlot == txdSlot && _stricmp(entry.textureName, textureName) == 0)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        void Add(const TextureCacheEntry& entry)
        {
            if (!m_index && !m_indexFailed && m_entries.size() >= IndexThreshold)
            {
                try
                {
                    auto index = std::make_unique<Index>();
                    std::vector<IndexedEntry> entries(m_entries.size());
                    for (size_t position = 0; position < entries.size(); ++position)
                    {
                        entries[position] = IndexedEntry { m_entries[position], position + 1 };
                    }
                    std::sort(entries.data(), entries.data() + entries.size(), ByKey {});
                    index->entries.insert(entries.data(), entries.data() + entries.size());
                    std::vector<const IndexedEntry*> pointers(entries.size());
                    size_t position = 0;
                    for (const auto& saved : index->entries)
                    {
                        pointers[position++] = &saved;
                    }
                    std::sort(pointers.data(), pointers.data() + pointers.size(), ByPointer {});
                    index->pointers.insert(pointers.data(), pointers.data() + pointers.size());
                    m_index = std::move(index);
                    m_nextOrdinal = m_entries.size() + 1;
                    m_entries.clear();
                }
                catch (const std::bad_alloc&)
                {
                    m_indexFailed = true;
                }
            }
            if (!m_index)
            {
                m_entries.push_back(entry);
                return;
            }
            const auto added = m_index->entries.insert(IndexedEntry { entry, m_nextOrdinal }).first;
            try
            {
                m_index->pointers.insert(&*added);
            }
            catch (...)
            {
                m_index->entries.erase(added);
                throw;
            }
            ++m_nextOrdinal;
        }

        void Release(RwTexture* texture)
        {
            if (!texture)
            {
                return;
            }
            if (m_index)
            {
                const auto found = m_index->pointers.lower_bound(texture);
                if (found == m_index->pointers.end() || (*found)->tex != texture)
                {
                    return;
                }
                const auto* entry = *found;
                if (--entry->refs <= 0)
                {
                    const auto stored = m_index->entries.find(*entry);
                    CTxdStore::RemoveRef(entry->txdSlot);
                    m_index->pointers.erase(found);
                    m_index->entries.erase(stored);
                    if (m_index->entries.empty())
                    {
                        m_index.reset();
                    }
                }
                return;
            }
            for (size_t index = 0; index < m_entries.size(); ++index)
            {
                if (m_entries[index].tex != texture)
                {
                    continue;
                }
                if (--m_entries[index].refs <= 0)
                {
                    CTxdStore::RemoveRef(m_entries[index].txdSlot);
                    m_entries.erase(m_entries.begin() + index);
                    if (m_entries.empty())
                    {
                        m_indexFailed = false;
                    }
                }
                return;
            }
        }
    };
    TextureCache g_textureCache;
}

RwTexture* ObjectRuntime::ResolveTexture(int texModelId, const char* txdLib, const char* texName)
{
    if (texModelId == 0)
    {
        if (m_blankTxdSlot < 0)
        {
            m_blankTxdSlot = CTxdStore::AddTxdSlot("mapstudio_blank");
        }
        if (m_blankTxdSlot < 0)
        {
            return nullptr;
        }
        TxdDef* blank = CTxdStore::ms_pTxdPool->GetAt(m_blankTxdSlot);
        if (!blank)
        {
            return nullptr;
        }
        if (!blank->m_pRwDictionary)
        {
            char path[MAX_PATH];
            const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
            if (!length || length >= MAX_PATH)
            {
                return nullptr;
            }
            char* slash = strrchr(path, '\\');
            if (!slash)
            {
                return nullptr;
            }
            *(slash + 1) = '\0';
            if (strcat_s(path, "SAMP\\blanktex.txd") != 0 || !CTxdStore::LoadTxd(m_blankTxdSlot, path))
            {
                return nullptr;
            }
        }
        txdLib = "mapstudio_blank";
        texName = "blanktex";
        texModelId = -1;
    }
    if (!texName || !texName[0])
    {
        return nullptr;
    }

    int txdSlot = -1;
    if (texModelId >= 0 && texModelId < 20000)
    {
        CBaseModelInfo* mi = CModelInfo::GetModelInfo(texModelId);
        if (mi)
        {
            txdSlot = mi->m_nTxdIndex;
        }
    }
    if (txdSlot < 0 && txdLib && txdLib[0])
    {
        txdSlot = CTxdStore::FindTxdSlot(txdLib);
    }
    if (txdSlot < 0)
    {
        return nullptr;
    }

    if (const auto* cached = g_textureCache.Find(txdSlot, texName))
    {
        ++cached->refs;
        return cached->tex;
    }

    RwTexture* tex = nullptr;
    TxdDef* txd = CTxdStore::ms_pTxdPool->GetAt(txdSlot);
    if (txd && txd->m_pRwDictionary)
    {
        tex = RwTexDictionaryFindNamedTexture(txd->m_pRwDictionary, texName);
    }

    if (!tex)
    {
        CStreaming::RequestTxdModel(txdSlot, (eStreamingFlags::PRIORITY_REQUEST | eStreamingFlags::KEEP_IN_MEMORY));
        CStreaming::LoadAllRequestedModels(true);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(txdSlot);
        tex = ReadTextureFromCurrentTxd(texName, nullptr);
        CTxdStore::PopCurrentTxd();
    }

    if (!tex)
    {
        return nullptr;
    }

    CTxdStore::AddRef(txdSlot);

    TextureCacheEntry e;
    e.txdSlot = txdSlot;
    strncpy_s(e.textureName, sizeof(e.textureName), texName, _TRUNCATE);
    e.tex = tex;
    e.refs = 1;
    try
    {
        g_textureCache.Add(e);
    }
    catch (const std::bad_alloc&)
    {
        CTxdStore::RemoveRef(txdSlot);
        return nullptr;
    }
    return tex;
}

void ObjectRuntime::ReleaseTexture(RwTexture* tex)
{
    g_textureCache.Release(tex);
}

void ObjectRuntime::ReleaseItemTextures(int idx)
{
    ObjectRuntimeState& rt = m_itemStates[idx];
    for (int m = 0; m < 16; ++m)
    {
        if (rt.mats[m].tex)
        {
            ReleaseTexture(rt.mats[m].tex);
        }
        rt.mats[m] = MaterialTextureState {};
    }
}

void ObjectRuntime::SyncItemTextures(int idx)
{
    const ItemData& d = g_scene->data[idx];
    ObjectRuntimeState& rt = m_itemStates[idx];
    const ULONGLONG now = GetTickCount64();

    const int materialCount = d.MaterialCount();
    for (int m = 0; m < 16; ++m)
    {
        MaterialTextureState& mr = rt.mats[m];
        const bool inUse = m < materialCount;
        const MaterialData* md = inUse ? &d.materials[m] : nullptr;
        const bool wants = md && (md->textureModelId == 0 || (md->textureModelId > 0 && md->textureName[0]));

        if (!wants)
        {
            if (mr.tex)
            {
                ReleaseTexture(mr.tex);
            }
            if (mr.tex || mr.textureModelId != -1)
            {
                mr = MaterialTextureState {};
            }
            continue;
        }

        if (mr.textureModelId == md->textureModelId && strcmp(mr.txdName, md->txdName) == 0
            && strcmp(mr.textureName, md->textureName) == 0 && (mr.tex || now < mr.retryAtMs))
        {
            continue;
        }

        if (mr.tex)
        {
            ReleaseTexture(mr.tex);
        }
        mr.tex = ResolveTexture(md->textureModelId, md->txdName, md->textureName);
        mr.retryAtMs = mr.tex ? 0 : now + MaterialRetryMs;
        mr.textureModelId = md->textureModelId;
        strncpy_s(mr.txdName, sizeof(mr.txdName), md->txdName, _TRUNCATE);
        strncpy_s(mr.textureName, sizeof(mr.textureName), md->textureName, _TRUNCATE);
    }
}
