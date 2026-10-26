// Source Repository: https://github.com/waverso/waversomapstudio

#include "projects.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "../../resources/resource.h"
#include "../shared/skin_catalog.h"
#include "../shared/vehicle_catalog.h"
#include "../utils/atomic_file.h"
#include "../scene/actions.h"
#include "../layout/curves.h"
#include "../catalog/database.h"
#include "../scene/free_camera.h"
#include "../ui/gui_api.h"
#include "../layout/layout.h"
#include "../objects/object_runtime.h"
#include "../scene/picker.h"
#include "../pickups/pickup_runtime.h"
#include "project_filename.h"
#include "../races/races.h"
#include "../scene/scene.h"
#include "../scene/selection.h"
#include "../textures/texture_browser.h"

namespace Projects
{
    static_assert(ProjectDocument::MaxItems == EditorScene::MaxItems);
    namespace
    {
        std::string g_projectName = "Untitled";
        std::filesystem::path g_projectPath;
        std::vector<std::uint8_t> g_savedBytes;
        int g_lastSerial = -1;
        bool g_dirty = false;
        bool g_needsSave = false;

        ProjectDocument::Items Capture(bool world = false)
        {
            ProjectDocument::Items items;
            if (!g_scene)
            {
                return items;
            }
            if (world)
            {
                g_scene->UpdateWorldTransforms();
            }
            int order[EditorScene::MaxItems];
            int count = 0;
            g_scene->CollectDisplayOrder(-1, order, count);
            items.resize(count);
            for (int i = 0; i < count; ++i)
            {
                g_scene->CaptureItemSnapshot(order[i], items[i]);
                if (world)
                {
                    const auto& w = g_scene->world[order[i]];
                    items[i].data.transform.pos[0] = w.pos.x;
                    items[i].data.transform.pos[1] = w.pos.y;
                    items[i].data.transform.pos[2] = w.pos.z;
                    std::memcpy(items[i].data.transform.rot, w.eulerDeg, sizeof(w.eulerDeg));
                }
            }
            return items;
        }

        void ReplaceScene(const ProjectDocument::Items& items)
        {
            EditorScene replacement;
            for (const auto& item : items)
            {
                const int parent = item.parentId < 0 ? -1 : replacement.FindIndexById(item.parentId);
                if ((item.parentId >= 0 && parent < 0) || replacement.AppendItemWithId(item, parent) < 0)
                {
                    throw std::runtime_error("Cannot restore the project scene.");
                }
                if (item.data.layout.HasValue())
                {
                    replacement.nextId = (std::max)(replacement.nextId, item.data.layout.Get().curveId + 1);
                }
            }
            replacement.UpdateWorldTransforms();

            Races::Stop();
            Curves::Reset();
            Layout::ResetAutoApply();
            Selection::ClearAll();
            for (int i = 0; i < g_scene->count; ++i)
            {
                g_scene->items[i].selected = true;
            }
            GuiDeleteSceneSelection();
            g_scene->Swap(replacement);
            ActionHistoryClearForNewProject();
            GuiResetProjectState();
            GuiOnItemsAppended(0, g_scene->count);
        }

        void MarkSaved(std::vector<std::uint8_t> bytes)
        {
            g_savedBytes = std::move(bytes);
            g_lastSerial = g_sceneActions ? g_sceneActions->GetSerial() : -1;
            g_dirty = false;
            g_needsSave = false;
        }

        bool ValidObjectModel(int model)
        {
            return model >= 321 && model < 20000 && !(model >= 400 && model <= 611)
                && CatalogDatabase::ModelExists(static_cast<std::uint16_t>(model));
        }
    }

#include "projects_storage.inl"

    const std::string& Name()
    {
        return g_projectName;
    }

    bool IsNamed()
    {
        return !g_projectPath.empty();
    }

    bool IsCurrent(const Entry& entry)
    {
        if (!IsNamed())
        {
            return false;
        }
        std::error_code error;
        return _wcsicmp(entry.path.c_str(), g_projectPath.c_str()) == 0
            || std::filesystem::equivalent(entry.path, g_projectPath, error);
    }

    bool IsDirty(bool refresh)
    {
        if (refresh)
        {
            g_lastSerial = -1;
        }
        UpdateDirty();
        return g_dirty;
    }

    void UpdateDirty()
    {
        if (!g_scene || !g_sceneActions || g_lastSerial == g_sceneActions->GetSerial())
        {
            return;
        }
        g_lastSerial = g_sceneActions->GetSerial();
        try
        {
            if (g_savedBytes.empty())
            {
                g_savedBytes = ProjectDocument::Encode({});
            }
            g_dirty = g_needsSave || ProjectDocument::Encode(Capture()) != g_savedBytes;
        }
        catch (const std::exception&)
        {
            g_dirty = true;
        }
    }

    bool Save(const char* name, std::string& error, bool overwrite, bool saveAs)
    {
        try
        {
            const bool existing = IsNamed() && !saveAs;
            auto path = existing ? g_projectPath : NamedPath(L"projects", name, L".bin");
            auto newName = Utf8(path.stem());
            const auto items = Capture();
            std::optional<ProjectDocument::Camera> camera;
            if (g_freeCamera)
            {
                camera.emplace();
                const auto position = g_freeCamera->m_position;
                camera->position[0] = position.x;
                camera->position[1] = position.y;
                camera->position[2] = position.z;
                g_freeCamera->GetYawPitch(camera->yaw, camera->pitch);
            }
            auto bytes = ProjectDocument::Encode(items, camera);
            auto baseline = ProjectDocument::Encode(items);
            AtomicWrite(path, bytes, existing || overwrite);
            g_projectPath = std::move(path);
            g_projectName = std::move(newName);
            MarkSaved(std::move(baseline));
            error.clear();
            return true;
        }
        catch (const std::exception& e)
        {
            error = e.what();
            return false;
        }
    }

    bool Delete(const Entry& entry, std::string& error)
    {
        try
        {
            const bool current = IsCurrent(entry);
            std::string untitled = "Untitled";
            const auto path = std::filesystem::absolute(entry.path).lexically_normal();
            const auto directory = std::filesystem::absolute(Directory(L"projects")).lexically_normal();
            if (_wcsicmp(path.parent_path().c_str(), directory.c_str()) != 0 || !HasProjectSignature(path))
            {
                error = "The file is not in the projects folder.";
                return false;
            }
            if (!DeleteFileW(path.c_str()))
            {
                error = "Cannot delete the project file.";
                return false;
            }
            if (current)
            {
                g_projectPath.clear();
                g_projectName = std::move(untitled);
                g_needsSave = true;
                g_dirty = true;
            }
            error.clear();
            return true;
        }
        catch (const std::exception& e)
        {
            error = e.what();
            return false;
        }
    }

    bool Load(const Entry& entry, std::string& error)
    {
        try
        {
            auto bytes = ReadFile(entry.path);
            auto newPath = entry.path;
            auto newName = entry.name;
            ProjectDocument::Items items;
            std::optional<ProjectDocument::Camera> camera;
            if (!ProjectDocument::Decode(bytes, items, error, &camera))
            {
                return false;
            }
            for (const auto& item : items)
            {
                if (item.data.model.HasValue() && item.data.model.Get().modelId != 0
                    && !ValidObjectModel(item.data.model.Get().modelId))
                {
                    error = "This project contains an unavailable object model.";
                    return false;
                }
            }
            auto baseline = ProjectDocument::Encode(items);
            ReplaceScene(items);
            g_projectPath = std::move(newPath);
            g_projectName = std::move(newName);
            MarkSaved(std::move(baseline));
            if (camera && g_freeCamera)
            {
                g_freeCamera->StartFlyTo(CVector(camera->position[0], camera->position[1], camera->position[2]),
                    std::remainder(camera->yaw, 2.0f * std::numbers::pi_v<float>), camera->pitch);
            }
            error.clear();
            return true;
        }
        catch (const std::exception& e)
        {
            error = e.what();
            return false;
        }
    }

    void New()
    {
        auto bytes = ProjectDocument::Encode({});
        ReplaceScene({});
        g_projectPath.clear();
        g_projectName = "Untitled";
        MarkSaved(std::move(bytes));
    }

#include "projects_pawn.inl"
}
