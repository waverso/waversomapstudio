// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    static bool PendingProjectEdit()
    {
        if (g_propertyInspector.session.Active() || g_propertyInspector.nameDirty)
        {
            return true;
        }
        const auto& tracker = g_editTracker;
        if (!g_scene || !g_sceneActions || tracker.itemId < 0 || tracker.lastSerial != g_sceneActions->GetSerial())
        {
            return false;
        }
        const int idx = g_scene->FindIndexById(tracker.itemId);
        const auto& before = tracker.wasActive ? tracker.preEdit : tracker.lastFrame;
        return idx >= 0 && before != g_scene->data[idx];
    }

    static bool FlushProjectEdit()
    {
        if (!GuiFinishPropertyEdit())
        {
            return false;
        }
        if (PendingProjectEdit())
        {
            const int idx = g_scene->FindIndexById(g_editTracker.itemId);
            const auto before = g_editTracker.wasActive ? g_editTracker.preEdit : g_editTracker.lastFrame;
            CommitItemEdit(idx, before);
        }
        g_editTracker = ItemEditTracker {};
        g_editTracker.lastSerial = g_sceneActions->GetSerial();
        ImGui::ClearActiveID();
        return true;
    }

    static void QueueProjectCommand(ProjectCommand command)
    {
        if (g_projectUi.pending == ProjectCommand::None)
        {
            g_projectUi.pending = command;
        }
    }

    static void SetProjectModal(ProjectModal modal)
    {
        g_projectUi.modal = modal;
        g_projectUi.error.clear();
    }

    static void ContinueProjectCommand(ProjectCommand command)
    {
        auto& ui = g_projectUi;
        ui.afterSave = ProjectCommand::None;
        if (command == ProjectCommand::New)
        {
            Projects::New();
            ui.modal = ProjectModal::None;
            GuiShowToast("New project created.", false);
        }
        else if (command == ProjectCommand::Open)
        {
            if (ui.projectToOpen)
            {
                std::string failure;
                const bool opened = Projects::Load(*ui.projectToOpen, failure);
                ui.projectToOpen.reset();
                ui.confirmBeforeOpen = !opened;
                SetProjectModal(opened ? ProjectModal::None : ProjectModal::Open);
                GuiShowToast(opened ? "Project opened." : failure.c_str(), !opened);
                return;
            }
            ui.selectedProjectIndex = -1;
            ui.confirmBeforeOpen = false;
            ui.search[0] = '\0';
            ui.searchFocused = false;
            ui.focusSearch = true;
            SetProjectModal(ProjectModal::Open);
            RefreshProjectList();
        }
        else
        {
            ui.modal = ProjectModal::None;
        }
    }

    static bool SaveCurrentProject()
    {
        auto& ui = g_projectUi;
        if (!Projects::IsNamed())
        {
            ui.saveAs = false;
            ui.modal = ProjectModal::SaveName;
            ui.focusName = true;
            ui.name[0] = '\0';
            ui.error.clear();
            return false;
        }
        if (!Projects::Save(nullptr, ui.error))
        {
            return false;
        }
        GuiShowToast("Project saved.", false);
        ContinueProjectCommand(ui.afterSave);
        return true;
    }

    static void ExecuteProjectCommand()
    {
        auto& ui = g_projectUi;
        const auto command = ui.pending;
        ui.pending = ProjectCommand::None;
        if (command == ProjectCommand::None)
        {
            return;
        }
        if (!FlushProjectEdit())
        {
            return;
        }
        ui.error.clear();
        if (command == ProjectCommand::New && !Projects::IsNamed() && !Projects::IsDirty(true))
        {
            return;
        }
        if (command == ProjectCommand::New || command == ProjectCommand::Open)
        {
            if (Projects::IsDirty(true))
            {
                ui.afterSave = command;
                SetProjectModal(ProjectModal::SaveChanges);
            }
            else
            {
                ContinueProjectCommand(command);
            }
        }
        else if (command == ProjectCommand::Save)
        {
            if (!Projects::IsDirty(true))
            {
                return;
            }
            ui.afterSave = ProjectCommand::None;
            if (!SaveCurrentProject())
            {
                if (ui.modal != ProjectModal::SaveName)
                {
                    GuiShowToast(ui.error.c_str(), true);
                }
            }
        }
        else if (command == ProjectCommand::SaveAs)
        {
            ui.afterSave = ProjectCommand::None;
            ui.saveAs = true;
            SetProjectModal(ProjectModal::SaveName);
            strncpy_s(ui.name, Projects::IsNamed() ? Projects::Name().c_str() : "", _TRUNCATE);
            ui.focusName = true;
        }
        else if (command == ProjectCommand::Import)
        {
            try
            {
                ui.imported = Projects::PreviewImport(ImGui::GetClipboardText());
            }
            catch (const std::exception& e)
            {
                ui.imported = {};
                ui.imported.failed = true;
                ui.imported.error = e.what();
            }
            if (ui.imported.failed)
            {
                GuiShowToast(ui.imported.error.c_str(), true);
            }
            else if (ui.imported.items.empty())
            {
                GuiShowToast("No valid items to import from the clipboard.", true);
            }
            else if (ui.imported.items.size() + 1 > static_cast<std::size_t>(EditorScene::MaxItems - g_scene->count))
            {
                GuiShowToast("Not enough space in the scene to import these items.", true);
            }
            else
            {
                SetProjectModal(ProjectModal::Import);
                return;
            }
            ui.imported = {};
        }
        else if (command == ProjectCommand::Export)
        {
            if (g_scene->count == 0)
            {
                return;
            }
            ui.exportOptions.categories = 0;
            SetProjectModal(ProjectModal::Export);
            strncpy_s(ui.name, Projects::IsNamed() ? Projects::Name().c_str() : "", _TRUNCATE);
            ui.focusName = true;
        }
    }
}
