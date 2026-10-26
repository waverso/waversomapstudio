// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct RwCamera;
class FxSystem_c;

namespace ModelPreview
{
    struct Profile
    {
        float target[3] = {};
        float yaw = 135.0f;
        float pitch = 22.0f;
        float distance = 1.0f;
    };

    bool GetProfile(int modelId, Profile& out);
    void Open(int modelId, bool skin = false);
    void Close();
    void Reset();
    void Orbit(float deltaYaw, float deltaPitch);
    void Pan(float deltaX, float deltaY, float viewportSize);
    void Zoom(float wheelDelta, float step = 1.1f);
    void UpdateFrame(IDirect3DDevice9* device);
    IDirect3DTexture9* Texture();
    bool ShouldUpdateFx(FxSystem_c* system, RwCamera* camera);
    void OnDeviceLost();
}
