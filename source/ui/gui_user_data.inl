// Source Repository: https://github.com/waverso/waversomapstudio

UserData::Settings Gui::CaptureUserSettings() const
{
    UserData::Settings settings;
    settings.uiScale = Ui::UserMultiplier();
    settings.gizmoScale = g_gizmos->GetGizmoScale();
    settings.indicatorScale = g_gizmos->GetAxisIndicatorScale();
    settings.vehicleColorStyle = static_cast<std::uint32_t>(g_vehicleColorPickerStyle);
    settings.themeIndex = static_cast<std::uint32_t>(Theme::SourcePresetIndex(Theme::g_selectedIndex));
    settings.fov = g_freeCamera->m_targetFovDegrees > 0.0f ? g_freeCamera->m_targetFovDegrees : FreeCamera::DefaultFovDegrees;
    settings.mouseSensitivity = g_freeCamera->m_mouseSensitivity;
    settings.baseSpeed = g_freeCamera->m_baseSpeed;
    settings.scrollSpeed = g_freeCamera->m_scrollSpeed;
    settings.fastMultiplier = g_freeCamera->m_fastMultiplier;
    settings.slowMultiplier = g_freeCamera->m_slowMultiplier;
    settings.outline = Selection::GetDrawHighlight();
    settings.sidebarWidth = m_sidebarResizeProgress;
    settings.sidebarSplit = m_sidebarSplitRatio;
    settings.positionGrid = g_gizmos->GetTranslationGridSnap();
    settings.rotationGrid = g_gizmos->GetRotationGridSnap();
    settings.updateCheckDays = m_updateCheckDays;
    settings.lastUpdateCheck = StudioInfo::LastCheckTime() ? StudioInfo::LastCheckTime() : m_lastUpdateCheck;
    return settings;
}

void Gui::LoadUserData()
{
    try
    {
        m_userStorage = std::make_unique<UserData::Storage>(Projects::StorageRoot());
        if (auto document = m_userStorage->Load(UserData::Group::Settings))
        {
            const auto& settings = std::get<UserData::Settings>(*document);
            Ui::SetUserMultiplier(settings.uiScale);
            g_gizmos->SetGizmoScale(settings.gizmoScale);
            g_gizmos->SetAxisIndicatorScale(settings.indicatorScale);
            g_vehicleColorPickerStyle = static_cast<VehicleColorPickerStyle>(settings.vehicleColorStyle);
            Theme::g_selectedIndex = static_cast<Theme::Index>(settings.themeIndex);
            Theme::Apply(Theme::Presets[settings.themeIndex]);
            g_freeCamera->m_targetFovDegrees = settings.fov;
            g_freeCamera->m_mouseSensitivity = settings.mouseSensitivity;
            g_freeCamera->m_baseSpeed = settings.baseSpeed;
            g_freeCamera->m_scrollSpeed = settings.scrollSpeed;
            g_freeCamera->m_fastMultiplier = settings.fastMultiplier;
            g_freeCamera->m_slowMultiplier = settings.slowMultiplier;
            Selection::SetDrawHighlight(settings.outline);
            m_sidebarResizeProgress = settings.sidebarWidth;
            m_sidebarResizeInitialized = true;
            m_sidebarSplitRatio = settings.sidebarSplit;
            g_gizmos->SetTranslationGridSnap(settings.positionGrid);
            g_gizmos->SetRotationGridSnap(settings.rotationGrid);
            m_updateCheckDays = settings.updateCheckDays;
            m_lastUpdateCheck = settings.lastUpdateCheck;
        }
        if (auto document = m_userStorage->Load(UserData::Group::Themes))
        {
            Theme::Restore(std::get<UserData::Themes>(*document));
            Theme::Apply(Theme::Colors(Theme::g_selectedIndex));
        }
        if (auto document = m_userStorage->Load(UserData::Group::Bookmarks))
        {
            const auto& saved = std::get<UserData::Bookmarks>(*document);
            std::vector<ViewBookmark> bookmarks;
            bookmarks.reserve(saved.size());
            std::uint64_t next = 1;
            for (const auto& value : saved)
            {
                ViewBookmark bookmark {};
                bookmark.id = value.id;
                strncpy_s(bookmark.name, value.name.c_str(), _TRUNCATE);
                bookmark.pos = CVector(value.position[0], value.position[1], value.position[2]);
                bookmark.yaw = std::remainder(value.yaw, Utils::TwoPi);
                bookmark.pitch = value.pitch;
                bookmark.zone = bookmark.pos.z < -242.990005f || bookmark.pos.z >= 900.0f
                    ? Zones::InvalidZone
                    : Zones::GetZoneAtPoint2D(bookmark.pos.x, bookmark.pos.y);
                bookmarks.push_back(bookmark);
                next = std::max(next, bookmark.id + 1);
            }
            g_viewUi.bookmarks.swap(bookmarks);
            g_nextBookmarkId = next;
        }
        if (auto document = m_userStorage->Load(UserData::Group::Favorites))
        {
            UserData::g_favoriteState = std::move(std::get<UserData::Favorites>(*document));
            VehicleBrowser::RestoreFavorites();
            SkinBrowser::RestoreFavorites();
            AnimationBrowser::RestoreFavorites();
            TextureBrowser::RestoreFavorites();
        }
        m_lastUserSettings = CaptureUserSettings();
        m_favoritesRevision = UserData::g_favoritesRevision;
        m_bookmarksRevision = UserData::g_bookmarksRevision;
        m_themesRevision = Theme::g_revision;
        m_selectedThemeId = Theme::Colors(Theme::g_selectedIndex).themeId;
    }
    catch (const std::exception& error)
    {
        m_userStorage.reset();
        m_userDataError = "Cannot initialize user data: " + std::string(error.what());
    }
    g_updateCheckAutomatic = StudioInfo::CheckUpdateIfDue(m_updateCheckDays, m_lastUpdateCheck);
}

void Gui::UpdateUserData()
{
    try
    {
        if (m_userStorage)
        {
            const auto settings = CaptureUserSettings();
            if (settings != m_lastUserSettings)
            {
                m_userStorage->Submit(settings);
                m_lastUserSettings = settings;
            }
            if (m_favoritesRevision != UserData::g_favoritesRevision)
            {
                m_userStorage->Submit(UserData::g_favoriteState);
                m_favoritesRevision = UserData::g_favoritesRevision;
            }
            if (m_bookmarksRevision != UserData::g_bookmarksRevision)
            {
                UserData::Bookmarks bookmarks;
                bookmarks.reserve(g_viewUi.bookmarks.size());
                for (const auto& value : g_viewUi.bookmarks)
                {
                    UserData::Bookmark bookmark;
                    bookmark.id = value.id;
                    bookmark.name = value.name;
                    bookmark.position[0] = value.pos.x;
                    bookmark.position[1] = value.pos.y;
                    bookmark.position[2] = value.pos.z;
                    bookmark.yaw = value.yaw;
                    bookmark.pitch = value.pitch;
                    bookmarks.push_back(std::move(bookmark));
                }
                m_userStorage->Submit(std::move(bookmarks));
                m_bookmarksRevision = UserData::g_bookmarksRevision;
            }
            const auto selectedTheme = Theme::Colors(Theme::g_selectedIndex).themeId;
            if (m_themesRevision != Theme::g_revision || m_selectedThemeId != selectedTheme)
            {
                m_userStorage->Submit(Theme::Capture());
                m_themesRevision = Theme::g_revision;
                m_selectedThemeId = selectedTheme;
            }
            if (g_freeCamera->m_active && m_userDataError.empty() && m_userStorage->HasError() && !g_toast.IsVisible())
            {
                m_userDataError = m_userStorage->TakeError();
            }
        }
        if (g_freeCamera->m_active && !m_userDataError.empty() && !g_toast.IsVisible())
        {
            g_toast.ShowError(m_userDataError.c_str());
            m_userDataError.clear();
        }
    }
    catch (const std::exception& error)
    {
        if (m_userDataError.empty())
        {
            m_userDataError = "Cannot queue user data: " + std::string(error.what());
        }
    }
}

void Gui::ShutdownUserData()
{
    if (!m_userStorage)
    {
        return;
    }
    UpdateUserData();
    m_userStorage->Flush();
    const auto error = m_userStorage->TakeError();
    if (!error.empty())
    {
        OutputDebugStringA(error.c_str());
    }
    m_userStorage.reset();
}
