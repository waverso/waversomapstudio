// Source Repository: https://github.com/waverso/waversomapstudio

enum class ProjectCommand
{
    None,
    New,
    Save,
    Open,
    Import,
    Export,
    SaveAs
};
enum class ProjectModal
{
    None,
    SaveChanges,
    SaveName,
    Open,
    Import,
    Export,
    OverwriteExport,
    OverwriteProject,
    DeleteProject
};

struct ProjectUiState
{
    ProjectCommand pending = ProjectCommand::None;
    ProjectCommand afterSave = ProjectCommand::None;
    ProjectModal modal = ProjectModal::None;
    bool focusName = false;
    bool saveAs = false;
    char name[1024] = {};
    std::string error;
    std::vector<Projects::Entry> entries;
    std::vector<int> visibleProjects;
    bool projectRowsReady = false;
    int selectedProjectIndex = -1;
    char search[256] = {};
    bool searchFocused = false;
    bool focusSearch = false;
    int sortKey = 1;
    bool sortDesc = true;
    bool resetProjectScroll = false;
    Projects::Entry projectToDelete;
    std::optional<Projects::Entry> projectToOpen;
    bool confirmBeforeOpen = false;
    PawnImport::Result imported;
    PawnExport::Options exportOptions;
};

static ProjectUiState g_projectUi;
