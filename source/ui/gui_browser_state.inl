// Source Repository: https://github.com/waverso/waversomapstudio

struct TexBrowserUiState
{
    ComboSearch categorySearch;
    bool open = false;
    bool viewOnly = false;
    int itemIdx = -1;
    int matSlot = -1;
    int detailRow = -1;

    char search[64] = {};
    bool searchFocused = false;
    std::uint8_t sourceMask = 0;
    int tagFilter = -1;
    std::uint32_t categoryMask = 0;
    std::uint16_t dimensionMask = 0;
    std::uint8_t searchFields = TextureBrowser::SearchDefault;
    int seamlessFilter = -1;
    bool hideDuplicates = true;
    std::uint8_t lodFilter = BrowserFilter::Hide;

    int mipMin = -1, mipMax = -1;
    int sizeMinKb = -1, sizeMaxKb = -1;
    int wMin = -1, wMax = -1;
    int hMin = -1, hMax = -1;
    int aspectW = 0, aspectH = 0;
    bool favoritesOnly = false;

    int sortKey = 0;
    bool sortDesc = false;

    bool advOpen = false;
    int advMip[2] = { 1, 10 };
    int advSize[2] = { 1, 256 };
    int advW[2] = { 1, 512 };
    int advH[2] = { 1, 512 };
    bool advHideDups = true;
    std::uint8_t advLodFilter = BrowserFilter::Hide;
    int advTagFilter = -1;
    std::uint8_t advSourceMask = 0;
    int advSeamlessFilter = -1;
    std::uint32_t advCategoryMask = 0;
    std::uint16_t advDimensionMask = 0;
    std::uint8_t advSearchFields = TextureBrowser::SearchDefault;
    int advAspectIdx = 0;
    char advBuf[8][16] = {};
    bool advFocus[8] = {};

    int page = 0;
    bool dirty = true;
    bool focusSearch = false;
    bool pendingRelease = false;
    std::vector<std::uint32_t> filtered;

    float previewZoom = 1.0f;
    ImVec2 previewPan = ImVec2(0.0f, 0.0f);
    float previewZoomTarget = 1.0f;
    ImVec2 previewPanTarget = ImVec2(0.0f, 0.0f);
    int previewLastRow = -1;
    bool previewWasOpen = false;
};
static TexBrowserUiState g_textureBrowserUi;

struct ObjectBrowserUiState
{
    ComboSearch categorySearch;
    bool open = false;
    int detailModelId = -1;
    int changeItemId = -1;
    bool viewOnly = false;
    bool pickupMode = false;
    bool layoutMode = false;
    int page = 0;
    bool dirty = true;
    bool focusSearch = false;
    bool searchFocused = false;
    char search[64] = {};
    bool favoritesOnly = false;
    std::uint8_t lodFilter = BrowserFilter::Hide;
    std::uint8_t breakableFilter = BrowserFilter::Any;
    std::uint8_t collidableFilter = BrowserFilter::Any;
    std::uint8_t animationFilter = BrowserFilter::Any;
    std::uint8_t uvAnimFilter = BrowserFilter::Any;
    std::uint8_t backfaceFilter = BrowserFilter::Any;
    std::uint8_t timeFilter = BrowserFilter::Any;
    std::uint8_t physicsFilter = BrowserFilter::Any;
    std::uint8_t explosiveFilter = BrowserFilter::Any;
    std::uint8_t searchFields = ObjectBrowser::SearchDefault;
    std::uint8_t sourceMask = 0;
    std::uint32_t categoryMask = 0;
    int idMin = -1, idMax = -1;
    int dffSizeMinKb = -1, dffSizeMaxKb = -1;
    float radiusMin = -1.0f, radiusMax = -1.0f;
    float lengthMin = -1.0f, lengthMax = -1.0f;
    float widthMin = -1.0f, widthMax = -1.0f;
    float heightMin = -1.0f, heightMax = -1.0f;
    int drawDistanceMin = -1, drawDistanceMax = -1;
    int polyCountMin = -1, polyCountMax = -1;
    int sortKey = ObjectBrowser::SortId;
    bool sortDesc = false;
    bool filtersOpen = false;
    std::uint8_t advLodFilter = BrowserFilter::Hide;
    std::uint8_t advBreakableFilter = BrowserFilter::Any;
    std::uint8_t advCollidableFilter = BrowserFilter::Any;
    std::uint8_t advAnimationFilter = BrowserFilter::Any;
    std::uint8_t advUVAnimFilter = BrowserFilter::Any;
    std::uint8_t advBackfaceFilter = BrowserFilter::Any;
    std::uint8_t advTimeFilter = BrowserFilter::Any;
    std::uint8_t advPhysicsFilter = BrowserFilter::Any;
    std::uint8_t advExplosiveFilter = BrowserFilter::Any;
    std::uint8_t advSearchFields = ObjectBrowser::SearchDefault;
    std::uint8_t advSourceMask = 0;
    std::uint32_t advCategoryMask = 0;
    int advId[2] = { 321, 19999 };
    int advDffSizeKb[2] = {};
    float advRadius[2] = {};
    float advLength[2] = {};
    float advWidth[2] = {};
    float advHeight[2] = {};
    int advDrawDistance[2] = {};
    int advPolyCount[2] = {};
    char advIntBuf[10][16] = {};
    bool advIntFocus[10] = {};
    char advFloatBuf[10][16] = {};
    bool advFloatFocus[10] = {};
    std::vector<std::uint32_t> filtered;
};
static ObjectBrowserUiState g_objectBrowserUi;

struct VehicleBrowserUiState
{
    ComboSearch categorySearch;
    bool open = false;
    bool viewOnly = false;
    bool dirty = true;
    bool focusSearch = false;
    bool searchFocused = false;
    bool favoritesOnly = false;
    bool filtersOpen = false;
    bool sortDesc = false;
    int sortKey = VehicleBrowser::SortId;
    int page = 0;
    int detailModelId = -1;
    int changeItemId = -1;
    char search[64] = {};
    VehicleBrowser::Filter filter;
    VehicleBrowser::Filter draft;
    char intBuf[6][16] = {};
    char floatBuf[8][16] = {};
    bool intFocus[6] = {};
    bool floatFocus[8] = {};
    std::vector<std::uint32_t> filtered;
};
static VehicleBrowserUiState g_vehicleBrowserUi;

struct SkinBrowserUiState
{
    bool open = false, viewOnly = false, dirty = true, focusSearch = false;
    bool searchFocused = false, favoritesOnly = false, filtersOpen = false, sortDesc = false;
    int sortKey = SkinBrowser::SortId, page = 0, detailModelId = -1, changeItemId = -1;
    char search[64] = {};
    char intBuf[2][16] = {};
    char floatBuf[4][16] = {};
    bool intFocus[2] = {};
    bool floatFocus[4] = {};
    SkinBrowser::Filter filter, draft;
    std::vector<std::uint32_t> filtered;
};
static SkinBrowserUiState g_skinBrowserUi;

static bool CommitMaterialTexture(int itemIdx, int matSlot, int modelId, const char* txdName, const char* texName)
{
    if (!texName || !texName[0])
    {
        g_toast.ShowError("Texture name cannot be empty.");
        return false;
    }
    RwTexture* tex = g_objectRuntime ? g_objectRuntime->ResolveTexture(modelId, txdName, texName) : nullptr;
    if (!tex)
    {
        g_toast.ShowError("Texture not found. Check model ID, TXD library and texture name.");
        return false;
    }
    g_objectRuntime->ReleaseTexture(tex);

    if (InspectionConfirmTexture(modelId, txdName, texName))
    {
        return true;
    }
    if (itemIdx >= 0 && itemIdx < g_scene->count && matSlot >= 0 && matSlot < g_scene->data[itemIdx].MaterialCount())
    {
        MaterialData& mat = g_scene->data[itemIdx].materials[matSlot];
        mat.textureModelId = modelId;
        strncpy_s(mat.txdName, sizeof(mat.txdName), txdName, _TRUNCATE);
        strncpy_s(mat.textureName, sizeof(mat.textureName), texName, _TRUNCATE);
    }
    return true;
}

static bool IsAnyModalOpen()
{
    return g_helpOpen || g_studioInfoOpen || g_updateAvailableOpen || g_themesOpen || g_projectUi.modal != ProjectModal::None
        || g_modal.activeType != PendingModalNone || g_textureBrowserUi.open || g_objectBrowserUi.open
        || g_objectBrowserUi.viewOnly || IsVehicleBrowserOpen() || IsSkinBrowserOpen();
}

static bool IsObjectBrowserOpen()
{
    return g_objectBrowserUi.open || g_objectBrowserUi.viewOnly;
}

static void OpenObjectBrowser(int changeItemId)
{
    CloseSkinBrowser();
    ImGui::ClearActiveID();
    g_modal.focusFirst = false;
    ObjectBrowser::EnsureIndex();
    g_objectBrowserUi.open = true;
    g_objectBrowserUi.detailModelId = -1;
    g_objectBrowserUi.changeItemId = changeItemId;
    const int index = g_scene ? g_scene->FindIndexById(changeItemId) : -1;
    g_objectBrowserUi.pickupMode = index >= 0 && g_scene->data[index].pickup.HasValue();
    g_objectBrowserUi.layoutMode = index >= 0 && g_scene->data[index].layout.HasValue();
    g_objectBrowserUi.viewOnly = false;
    g_objectBrowserUi.dirty = true;
}

static void OpenObjectBrowserViewDetails(int modelId)
{
    CloseSkinBrowser();
    ImGui::ClearActiveID();
    ObjectBrowser::EnsureIndex();
    g_objectBrowserUi.open = false;
    g_objectBrowserUi.changeItemId = -1;
    g_objectBrowserUi.viewOnly = true;
    g_objectBrowserUi.detailModelId = modelId;
    g_objectBrowserUi.focusSearch = false;
    ModelPreview::Open(modelId);
}

static void CloseObjectBrowser()
{
    ModelPreview::Close();
    g_objectBrowserUi.open = false;
    g_objectBrowserUi.detailModelId = -1;
    g_objectBrowserUi.changeItemId = -1;
    g_objectBrowserUi.viewOnly = false;
    g_objectBrowserUi.filtersOpen = false;
}

static void OpenTextureBrowser(int itemIdx, int matSlot)
{
    ImGui::ClearActiveID();
    TextureBrowser::EnsureIndex();
    g_textureBrowserUi.advOpen = false;
    g_textureBrowserUi.open = true;
    g_textureBrowserUi.viewOnly = false;
    g_textureBrowserUi.itemIdx = itemIdx;
    g_textureBrowserUi.matSlot = matSlot;
    g_textureBrowserUi.detailRow = -1;
    g_textureBrowserUi.dirty = true;
}

static void CloseTextureBrowser()
{
    g_textureBrowserUi.open = false;
    g_textureBrowserUi.viewOnly = false;
    g_textureBrowserUi.detailRow = -1;
    g_textureBrowserUi.advOpen = false;
    g_textureBrowserUi.previewWasOpen = false;
    g_textureBrowserUi.pendingRelease = true;
}

static bool IsVehicleBrowserOpen()
{
    return g_vehicleBrowserUi.open || g_vehicleBrowserUi.viewOnly;
}

static void CloseVehicleBrowser()
{
    ModelPreview::Close();
    auto& ui = g_vehicleBrowserUi;
    ui.open = ui.viewOnly = ui.filtersOpen = false;
    ui.detailModelId = ui.changeItemId = -1;
}

static void OpenVehicleBrowser(int changeItemId)
{
    ImGui::ClearActiveID();
    CloseSkinBrowser();
    CloseObjectBrowser();
    g_modal.focusFirst = false;
    auto& ui = g_vehicleBrowserUi;
    ui.open = true;
    ui.viewOnly = false;
    ui.detailModelId = -1;
    ui.changeItemId = changeItemId;
    ui.dirty = true;
}

static void OpenVehicleDetails(int modelId)
{
    ImGui::ClearActiveID();
    CloseSkinBrowser();
    CloseObjectBrowser();
    auto& ui = g_vehicleBrowserUi;
    ui.open = false;
    ui.viewOnly = true;
    ui.changeItemId = -1;
    ui.detailModelId = modelId;
    ModelPreview::Open(modelId);
}

static bool IsSkinBrowserOpen()
{
    return g_skinBrowserUi.open || g_skinBrowserUi.viewOnly;
}

static void CloseSkinBrowser()
{
    if (IsSkinBrowserOpen())
    {
        ModelPreview::Close();
    }
    auto& ui = g_skinBrowserUi;
    ui.open = ui.viewOnly = ui.filtersOpen = false;
    ui.detailModelId = ui.changeItemId = -1;
}

static void OpenSkinBrowser(int changeItemId)
{
    ImGui::ClearActiveID();
    CloseObjectBrowser();
    CloseVehicleBrowser();
    g_modal.focusFirst = false;
    auto& ui = g_skinBrowserUi;
    ui.open = true;
    ui.viewOnly = false;
    ui.detailModelId = -1;
    ui.changeItemId = changeItemId;
    ui.dirty = true;
}

static void OpenSkinDetails(int modelId)
{
    OpenSkinBrowser();
    auto& ui = g_skinBrowserUi;
    ui.open = false;
    ui.viewOnly = true;
    ui.detailModelId = modelId;
    ModelPreview::Open(modelId, true);
}
