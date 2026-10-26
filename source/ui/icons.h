// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <d3d9.h>
#include <vector>

struct ImDrawCmd;
struct ImDrawList;

class Icons
{
    public:
    Icons(IDirect3DDevice9* pDevice);
    ~Icons();
    bool UsesAccent(IDirect3DTexture9* texture) const;
    static void BeginAccent(const ImDrawList*, const ImDrawCmd* command);
    static void EndAccent(const ImDrawList*, const ImDrawCmd* command);

    IDirect3DTexture9* addTexture = nullptr;
    IDirect3DTexture9* arrowTexture = nullptr;
    IDirect3DTexture9* boundsTexture = nullptr;
    IDirect3DTexture9* centerTexture = nullptr;
    IDirect3DTexture9* clockTexture = nullptr;
    IDirect3DTexture9* closeTexture = nullptr;
    IDirect3DTexture9* cloudTexture = nullptr;
    IDirect3DTexture9* copyTexture = nullptr;
    IDirect3DTexture9* cubeTexture = nullptr;
    IDirect3DTexture9* dayTexture = nullptr;
    IDirect3DTexture9* deleteTexture = nullptr;
    IDirect3DTexture9* editTexture = nullptr;
    IDirect3DTexture9* exportTexture = nullptr;
    IDirect3DTexture9* gitHubTexture = nullptr;
    IDirect3DTexture9* globalTexture = nullptr;
    IDirect3DTexture9* gridPositionTexture = nullptr;
    IDirect3DTexture9* gridRotationTexture = nullptr;
    IDirect3DTexture9* handTexture = nullptr;
    IDirect3DTexture9* heartTexture = nullptr;
    IDirect3DTexture9* helpTexture = nullptr;
    IDirect3DTexture9* hideTexture = nullptr;
    IDirect3DTexture9* importTexture = nullptr;
    IDirect3DTexture9* lampTexture = nullptr;
    IDirect3DTexture9* localTexture = nullptr;
    IDirect3DTexture9* logoTexture = nullptr;
    IDirect3DTexture9* moreTexture = nullptr;
    IDirect3DTexture9* newTexture = nullptr;
    IDirect3DTexture9* nightTexture = nullptr;
    IDirect3DTexture9* openTexture = nullptr;
    IDirect3DTexture9* pivotTexture = nullptr;
    IDirect3DTexture9* playTexture = nullptr;
    IDirect3DTexture9* positionTexture = nullptr;
    IDirect3DTexture9* randomTexture = nullptr;
    IDirect3DTexture9* rotationTexture = nullptr;
    IDirect3DTexture9* saveTexture = nullptr;
    IDirect3DTexture9* searchTexture = nullptr;
    IDirect3DTexture9* selectTexture = nullptr;
    IDirect3DTexture9* showTexture = nullptr;
    IDirect3DTexture9* subtractTexture = nullptr;
    IDirect3DTexture9* transformTexture = nullptr;
    IDirect3DTexture9* undoTexture = nullptr;
    IDirect3DTexture9* youTubeTexture = nullptr;

    float logoAspectRatio = 0.0f;

    private:
    IDirect3DDevice9* m_device = nullptr;
    IDirect3DPixelShader9* m_accentShader = nullptr;
    std::vector<IDirect3DTexture9*> m_accentTextures;

    struct IconSlot
    {
        const char* name;
        IDirect3DTexture9* Icons::* texture;
    };

    inline static constexpr IconSlot IconSlots[] = {
        { "Add", &Icons::addTexture },
        { "Arrow", &Icons::arrowTexture },
        { "Bounds", &Icons::boundsTexture },
        { "Center", &Icons::centerTexture },
        { "Clock", &Icons::clockTexture },
        { "Close", &Icons::closeTexture },
        { "Cloud", &Icons::cloudTexture },
        { "Copy", &Icons::copyTexture },
        { "Cube", &Icons::cubeTexture },
        { "Day", &Icons::dayTexture },
        { "Delete", &Icons::deleteTexture },
        { "Edit", &Icons::editTexture },
        { "Export", &Icons::exportTexture },
        { "GitHub", &Icons::gitHubTexture },
        { "Global", &Icons::globalTexture },
        { "GridPosition", &Icons::gridPositionTexture },
        { "GridRotation", &Icons::gridRotationTexture },
        { "Hand", &Icons::handTexture },
        { "Heart", &Icons::heartTexture },
        { "Help", &Icons::helpTexture },
        { "Hide", &Icons::hideTexture },
        { "Import", &Icons::importTexture },
        { "Lamp", &Icons::lampTexture },
        { "Local", &Icons::localTexture },
        { "Logo", &Icons::logoTexture },
        { "More", &Icons::moreTexture },
        { "New", &Icons::newTexture },
        { "Night", &Icons::nightTexture },
        { "Open", &Icons::openTexture },
        { "Pivot", &Icons::pivotTexture },
        { "Play", &Icons::playTexture },
        { "Position", &Icons::positionTexture },
        { "Random", &Icons::randomTexture },
        { "Rotation", &Icons::rotationTexture },
        { "Save", &Icons::saveTexture },
        { "Search", &Icons::searchTexture },
        { "Select", &Icons::selectTexture },
        { "Show", &Icons::showTexture },
        { "Subtract", &Icons::subtractTexture },
        { "Transform", &Icons::transformTexture },
        { "Undo", &Icons::undoTexture },
        { "YouTube", &Icons::youTubeTexture },
    };
};

extern Icons* g_icons;
