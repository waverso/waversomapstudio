// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <d3d9.h>
#include <string_view>
#include <vector>

#include "../scene/scene_data.h"

bool RenderMaterialText(IDirect3DDevice9* device, const MaterialTextData& data, int width, int height, std::string_view text,
    std::vector<unsigned int>& pixels);
void ReleaseMaterialTextRenderer();
