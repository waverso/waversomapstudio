// Source Repository: https://github.com/waverso/waversomapstudio

enum SearchMode
{
    SearchModeName = 0,
    SearchModeModel = 1
};

struct OutlinerUiState
{
    ItemListNavigation navigation;
    int visibleRow = 0;
    bool hoveredRow = false;

    int renameIndex = -1;
    char renameBuf[64] = {};
    bool focusRename = false;
    bool showRequested = false;

    int popupIndex = -1;
    bool openContextPopup = false;
    int dragIndex = -1;
    bool dragActive = false;
    int dropTarget = -1;
    int dropBoundary = -1;
    SceneDropMode dropMode = SceneDropNone;

    char searchBuf[64] = {};
    SearchMode searchMode = SearchModeName;
    bool searchFocused = false;

    int scrollToId = -1;
    int rangeAnchorId = -1;
    int pendingSingleId = -1;

    int newIdWatermark = 0;
};

static OutlinerUiState g_outlinerUi;
static OutlinerRows g_outlinerRows;

static bool MatchesSearchMode(const SceneItem& it, const char* search, SearchMode mode)
{
    if (!search || !search[0])
    {
        return true;
    }
    if (mode == SearchModeName)
    {
        return TextSearch::ContainsAsciiNoCase(it.name, search);
    }
    return it.searchModelId == atoi(search);
}

struct OutlinerNameSearch
{
    char query[64];
    int prefix[64];
    int length = 0;

    static char Lower(char value)
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
    }

    explicit OutlinerNameSearch(const char* search)
    {
        if (search)
        {
            while (length < 63 && search[length])
            {
                query[length] = Lower(search[length]);
                ++length;
            }
        }
        prefix[0] = 0;
        for (int index = 1, matched = 0; index < length; ++index)
        {
            while (matched && query[index] != query[matched])
            {
                matched = prefix[matched - 1];
            }
            if (query[index] == query[matched])
            {
                ++matched;
            }
            prefix[index] = matched;
        }
    }

    bool Contains(const char* text) const
    {
        if (!length)
        {
            return true;
        }
        int matched = 0;
        for (; text && *text; ++text)
        {
            const char value = Lower(*text);
            while (matched && value != query[matched])
            {
                matched = prefix[matched - 1];
            }
            if (value == query[matched] && ++matched == length)
            {
                return true;
            }
        }
        return false;
    }
};

static int BuildOutlinerRows(const char* search, SearchMode mode)
{
    const bool filtering = search && search[0];
    if (!filtering)
    {
        g_outlinerRows.Build(*g_scene, false,
            [](const SceneItem&)
            {
                return true;
            });
        return 0;
    }
    const OutlinerNameSearch name(mode == SearchModeName ? search : nullptr);
    const int model = mode == SearchModeName ? 0 : atoi(search);
    int found = 0;
    g_outlinerRows.Build(*g_scene, filtering,
        [&](const SceneItem& item)
        {
            const bool match = mode == SearchModeName ? name.Contains(item.name) : item.searchModelId == model;
            found += match;
            return match;
        });
    return found;
}

static void BeginOutlinerRename(int idx, bool reveal = false)
{
    if (!g_scene || idx < 0 || idx >= g_scene->count)
    {
        return;
    }
    if (g_scene->items[idx].type == SceneItemRaceCheckpoint)
    {
        return;
    }
    g_outlinerUi.renameIndex = idx;
    strcpy_s(g_outlinerUi.renameBuf, g_scene->items[idx].name);
    g_outlinerUi.focusRename = true;
    if (reveal)
    {
        g_outlinerUi.scrollToId = g_scene->items[idx].id;
        if (!MatchesSearchMode(g_scene->items[idx], g_outlinerUi.searchBuf, g_outlinerUi.searchMode))
        {
            g_outlinerUi.searchBuf[0] = '\0';
        }
        for (int parent = g_scene->items[idx].parent; parent >= 0; parent = g_scene->items[parent].parent)
        {
            g_scene->items[parent].open = true;
        }
    }
}

static bool OutlinerHasExpandedParents()
{
    EditorScene& sc = *g_scene;
    for (int i = 0; i < sc.count; ++i)
    {
        if (sc.items[i].open && sc.HasChildren(i))
        {
            return true;
        }
    }
    return false;
}

static bool OutlinerHasCollapsedParents()
{
    EditorScene& sc = *g_scene;
    for (int i = 0; i < sc.count; ++i)
    {
        if (!sc.items[i].open && sc.HasChildren(i))
        {
            return true;
        }
    }
    return false;
}

static void OutlinerCollapseAll()
{
    EditorScene& sc = *g_scene;
    for (int i = 0; i < sc.count; ++i)
    {
        if (sc.HasChildren(i))
        {
            sc.items[i].open = false;
        }
    }
}

static void OutlinerExpandAll()
{
    EditorScene& sc = *g_scene;
    for (int i = 0; i < sc.count; ++i)
    {
        sc.items[i].open = true;
    }
}
