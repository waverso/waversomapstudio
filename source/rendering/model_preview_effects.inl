// Source Repository: https://github.com/waverso/waversomapstudio

RpAtomic* PreviewScanAtomicBounds(RpAtomic* atomic, void* data)
{
    if (!atomic || ModelChecks::IsGunflashAtomic(atomic) || !(RpAtomicGetFlags(atomic) & rpATOMICRENDER))
    {
        return atomic;
    }
    RpGeometry* geometry = RpAtomicGetGeometry(atomic);
    if (!geometry || RpGeometryGetNumMorphTargets(geometry) <= 0)
    {
        return atomic;
    }
    RpMorphTarget* morph = RpGeometryGetMorphTarget(geometry, 0);
    const RwV3d* vertices = morph ? RpMorphTargetGetVertices(morph) : nullptr;
    const int count = vertices ? std::max(0, RpGeometryGetNumVertices(geometry)) : 0;
    RwMatrix transform {};
    RwMatrixSetIdentity(&transform);
    if (RwFrame* frame = RpAtomicGetFrame(atomic))
    {
        if (RwMatrix* ltm = RwFrameGetLTM(frame))
        {
            transform = *ltm;
        }
    }
    auto& bounds = *static_cast<ModelEffects::Bounds*>(data);
    for (int index = 0; index < count; ++index)
    {
        const RwV3d& point = vertices[index];
        const float world[3] { transform.pos.x + point.x * transform.right.x + point.y * transform.up.x
                + point.z * transform.at.x,
            transform.pos.y + point.x * transform.right.y + point.y * transform.up.y + point.z * transform.at.y,
            transform.pos.z + point.x * transform.right.z + point.y * transform.up.z + point.z * transform.at.z };
        ModelEffects::ExpandBounds(bounds, world, 0.0f);
    }
    return atomic;
}

void RecollectPreviewEffects()
{
    g_preview.effects.Clear();

    const bool weaponObject
        = (g_preview.modelId >= 321 && g_preview.modelId <= 373) || (g_preview.modelId >= 821 && g_preview.modelId <= 869);

    ModelEffects::Dff::ParseStats stats {};
    const bool dffAuthoritative
        = !weaponObject && ModelEffects::Dff::CollectFromArchiveDff(g_preview.modelId, g_preview.effects, stats);

    if (!weaponObject && !dffAuthoritative)
    {
        if (g_preview.instance)
        {
            g_preview.effects = ModelEffects::CollectFromInstance(g_preview.instance);
        }
        if (!g_preview.effects.Valid() && g_preview.referencedInfo && g_preview.referencedInfo->m_nNum2dEffects > 0)
        {
            g_preview.effects = ModelEffects::Collect(g_preview.referencedInfo);
        }
        if (!g_preview.effects.Valid() && g_preview.modelName[0])
        {
            if (const ModelEffects::SaMpPointLightParams* params = ModelEffects::FindSaMpPointLight(g_preview.modelName))
            {
                ModelEffects::LightInfo light;
                light.color[0] = params->color[0];
                light.color[1] = params->color[1];
                light.color[2] = params->color[2];
                light.alpha = params->alpha;
                light.coronaSize = params->coronaSize;
                light.pointRange = params->pointRange;
                light.flashType = params->flashType;
                light.flags = params->flags;
                strncpy_s(light.textureName, sizeof(light.textureName), params->textureName, _TRUNCATE);
                g_preview.effects.lights.push_back(light);
            }
            else if (g_fxMan.FindFxSystemBP(g_preview.modelName))
            {
                ModelEffects::ParticleInfo particle {};
                strncpy_s(particle.name, sizeof(particle.name), g_preview.modelName, _TRUNCATE);
                g_preview.effects.particles.push_back(particle);
            }
        }
    }
    else if (!weaponObject && g_preview.referencedInfo && !g_preview.effects.Valid()
        && g_preview.referencedInfo->m_nNum2dEffects > g_preview.effects.entriesSeen)
    {
        g_preview.effects = ModelEffects::Collect(g_preview.referencedInfo);
    }
    if (g_preview.lightFade.size() < g_preview.effects.lights.size())
    {
        g_preview.lightFade.resize(g_preview.effects.lights.size());
    }
}

void RefreshPreviewEffectState()
{
    DestroyPreviewFxSystems();
    g_preview.framingMode = ModelEffects::FramingMode::None;
    g_preview.vertexBounds = {};
    g_preview.lightFade.clear();

    RecollectPreviewEffects();
    if (!g_preview.effects.Valid())
    {
        return;
    }

    ModelEffects::Bounds vertex {};
    const RwUInt8 type = g_preview.instance ? RwObjectGetType(g_preview.instance) : 0u;
    if (type == rpATOMIC)
    {
        PreviewScanAtomicBounds(reinterpret_cast<RpAtomic*>(g_preview.instance), &vertex);
    }
    else if (type == rpCLUMP)
    {
        RpClumpForAllAtomics(reinterpret_cast<RpClump*>(g_preview.instance), PreviewScanAtomicBounds, &vertex);
    }

    g_preview.vertexBounds = vertex;
    g_preview.fxSystems.resize(g_preview.effects.particles.size());
    g_preview.effectClock = 0.0;
}

void DestroyPreviewFxSystems()
{
    for (PreviewLive::PreviewFx& fx : g_preview.fxSystems)
    {
        if (fx.system)
        {
            g_fxMan.DestroyFxSystem(fx.system);
        }
    }
    g_preview.fxSystems.clear();
    if (g_preview.fallbackGlowRaster)
    {
        RwRasterDestroy(g_preview.fallbackGlowRaster);
        g_preview.fallbackGlowRaster = nullptr;
    }
}

void AdvancePreviewFx(double deltaSeconds)
{
    g_preview.effectClock += deltaSeconds;
    if (g_preview.effects.particles.empty())
    {
        return;
    }
    if (g_preview.fxSystems.size() < g_preview.effects.particles.size())
    {
        g_preview.fxSystems.resize(g_preview.effects.particles.size());
    }
    if (g_preview.target.camera)
    {
        g_fxMan.CalcFrustumInfo(g_preview.target.camera);
    }

    static RwMatrix identity;
    RwMatrixSetIdentity(&identity);

    for (std::size_t index = 0; index < g_preview.fxSystems.size(); ++index)
    {
        PreviewLive::PreviewFx& fx = g_preview.fxSystems[index];
        if (fx.system)
        {
            const bool finished = fx.system->m_nKillStatus == FX_KILLED || fx.system->m_nPlayStatus == FX_STOPPED;
            if (!finished
                && (!g_preview.target.camera || !fx.system->Update(g_preview.target.camera, static_cast<float>(deltaSeconds))))
            {
                continue;
            }
            g_fxMan.DestroyFxSystem(fx.system);
            fx.system = nullptr;
            fx.dead = true;
            fx.deadAt = g_preview.effectClock;
        }

        if (fx.dead && g_preview.effectClock - fx.deadAt < 1.0)
        {
            continue;
        }
        const ModelEffects::ParticleInfo& particle = g_preview.effects.particles[index];
        if (!particle.name[0])
        {
            continue;
        }
        RwV3d position { particle.pos[0], particle.pos[1], particle.pos[2] };
        FxSystem_c* system = g_fxMan.CreateFxSystem(const_cast<char*>(particle.name), &position, &identity, TRUE);
        if (!system)
        {
            continue;
        }
        system->Play();
        fx.system = system;
        fx.dead = false;
    }
}

RwRaster* MakeFallbackGlowRaster()
{
    RwRaster* raster = RwRasterCreate(64, 64, 32, rwRASTERTYPENORMAL | rwRASTERFORMAT8888);
    if (!raster)
    {
        return nullptr;
    }
    auto* pixels = static_cast<RwUInt8*>(RwRasterLock(raster, 0, rwRASTERLOCKWRITE));
    if (!pixels)
    {
        RwRasterDestroy(raster);
        return nullptr;
    }
    const RwInt32 stride = RwRasterGetStride(raster);
    for (int y = 0; y < 64; ++y)
    {
        auto* row = pixels + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < 64; ++x)
        {
            const float dx = (x + 0.5f) / 32.0f - 1.0f;
            const float dy = (y + 0.5f) / 32.0f - 1.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float falloff = std::max(0.0f, 1.0f - d);
            auto* pixel = row + static_cast<std::size_t>(x) * 4u;
            pixel[0] = 255;
            pixel[1] = 255;
            pixel[2] = 255;
            pixel[3] = static_cast<RwUInt8>(falloff * falloff * 255.0f);
        }
    }
    RwRasterUnlock(raster);
    return raster;
}

RwRaster* ResolveParticleTxdTexture(const char* name)
{
    if (!name || !name[0] || !CTxdStore::ms_pTxdPool)
    {
        return nullptr;
    }
    const int slot = CTxdStore::FindTxdSlot(const_cast<char*>("particle"));
    if (slot < 0 || slot >= CTxdStore::ms_pTxdPool->m_nSize)
    {
        return nullptr;
    }
    TxdDef* def = CTxdStore::ms_pTxdPool->GetAt(slot);
    if (!def || !def->m_pRwDictionary)
    {
        return nullptr;
    }
    RwTexture* texture = RwTexDictionaryFindNamedTexture(def->m_pRwDictionary, const_cast<char*>(name));
    return texture ? texture->raster : nullptr;
}

RwRaster* GetShinyStarRaster(RwRaster* fallback)
{
    if (gpCoronaTexture)
    {
        if (RwTexture* star = gpCoronaTexture[CORONATYPE_SHINYSTAR])
        {
            if (star->raster)
            {
                return star->raster;
            }
        }
    }
    return fallback;
}

void DrawPreviewTrafficLens(const Vec3& center, bool smallLens, const RwRGBA& tint)
{
    RwIm3DVertex vertices[8];
    constexpr float Up[6] = { 0.162f, 0.162f, 0.0f, -0.162f, -0.162f, 0.0f };
    constexpr float Side[6] = { -0.09f, 0.09f, 0.162f, 0.09f, -0.09f, -0.162f };
    const int count = smallLens ? 8 : 6;
    for (int i = 0; i < count; ++i)
    {
        float offsetX, offsetZ;
        if (smallLens)
        {
            const float angle = (2.0f * std::numbers::pi_v<float>)*static_cast<float>(i) / 8.0f;
            offsetX = std::cos(angle) * 0.1f;
            offsetZ = std::sin(angle) * 0.1f;
        }
        else
        {
            offsetX = Side[i];
            offsetZ = Up[i];
        }
        const int vertex = smallLens ? i : (i + 1) % 6;
        RwIm3DVertexSetPos(&vertices[vertex], center.x + offsetX, center.y, center.z + offsetZ);
        RwIm3DVertexSetU(&vertices[vertex], 0.0f);
        RwIm3DVertexSetV(&vertices[vertex], 0.0f);
        RwIm3DVertexSetRGBA(&vertices[vertex], tint.red, tint.green, tint.blue, tint.alpha);
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLNONE));
    if (RwIm3DTransform(vertices, count, nullptr, rwIM3D_VERTEXUV | rwIM3D_VERTEXRGBA))
    {
        RwIm3DRenderPrimitive(rwPRIMTYPETRIFAN);
        RwIm3DEnd();
    }
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLBACK));
}

void DrawPreviewGlowQuad(const Vec3& center, const Vec3& right, const Vec3& up, float radiusX, float radiusY, const RwRGBA& tint,
    RwRaster* raster, bool zTest)
{
    if (radiusX <= 0.01f || radiusY <= 0.01f || !std::isfinite(radiusX) || !std::isfinite(radiusY))
    {
        return;
    }

    RwIm3DVertex vertices[4];
    const Vec3 corner[4] { center - right * radiusX - up * radiusY, center + right * radiusX - up * radiusY,
        center + right * radiusX + up * radiusY, center - right * radiusX + up * radiusY };
    const float uv[4][2] { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    for (int i = 0; i < 4; ++i)
    {
        RwIm3DVertexSetPos(&vertices[i], corner[i].x, corner[i].y, corner[i].z);
        RwIm3DVertexSetU(&vertices[i], uv[i][0]);
        RwIm3DVertexSetV(&vertices[i], uv[i][1]);
        RwIm3DVertexSetRGBA(&vertices[i], tint.red, tint.green, tint.blue, tint.alpha);
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, raster);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(zTest ? TRUE : FALSE));
    RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLNONE));

    if (RwIm3DTransform(vertices, 4, nullptr, rwIM3D_VERTEXUV | rwIM3D_VERTEXRGBA))
    {
        RwIm3DRenderPrimitive(rwPRIMTYPETRIFAN);
        RwIm3DEnd();
    }

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
}

bool DrawPreviewEffectSprites()
{
    if (!g_preview.effects.Valid() || !g_preview.target.frame)
    {
        return false;
    }

    const RwMatrix* cameraMatrix = RwFrameGetLTM(g_preview.target.frame);
    if (!cameraMatrix)
    {
        return false;
    }
    const Vec3 right { cameraMatrix->right.x, cameraMatrix->right.y, cameraMatrix->right.z };
    const Vec3 up { cameraMatrix->up.x, cameraMatrix->up.y, cameraMatrix->up.z };
    const Vec3 at { cameraMatrix->at.x, cameraMatrix->at.y, cameraMatrix->at.z };
    const Vec3 eye { cameraMatrix->pos.x, cameraMatrix->pos.y, cameraMatrix->pos.z };

    const ModelEffects::GameClock clock { CTimer::m_snTimeInMilliseconds, CTimer::m_FrameCounter, CTimer::ms_fTimeStep };
    const bool traffic = ModelChecks::IsTrafficLight(static_cast<unsigned short>(g_preview.modelId));
    const ModelEffects::TrafficLightState cycle
        = traffic ? ModelEffects::TrafficLightCycle(clock.timeMs) : ModelEffects::TrafficLightState::Off;
    const bool sameDir = at.y > 0.0f;
    const bool lensSmall = ModelChecks::TrafficLensIsSmall(static_cast<unsigned short>(g_preview.modelId));
    auto coronaDrawPos = [&eye](const Vec3& center, float pull)
    {
        const Vec3 toLight = center - eye;
        const float length = std::sqrt(Dot(toLight, toLight));
        if (length <= 0.001f)
        {
            return center;
        }
        const float scale = std::min(pull, length * 0.9f) / length;
        return Vec3 { center.x - toLight.x * scale, center.y - toLight.y * scale, center.z - toLight.z * scale };
    };

    if (g_preview.lightFade.size() < g_preview.effects.lights.size())
    {
        g_preview.lightFade.resize(g_preview.effects.lights.size());
    }

    bool drawn = false;
    for (std::size_t index = 0; index < g_preview.effects.lights.size(); ++index)
    {
        const ModelEffects::LightInfo& light = g_preview.effects.lights[index];
        ModelEffects::LightFadeState& fade = g_preview.lightFade[index];

        float rgb[3] {};
        float sizeParam = light.coronaSize;
        float pull = 0.8f;
        bool registered = false;
        if (traffic && light.flashType == 7)
        {
            const ModelEffects::TrafficLightState lamp = ModelEffects::TrafficColorStateOf(light);
            registered = lamp == cycle && (sameDir != (light.pos[1] > 0.0f));
            if (registered)
            {
                ModelEffects::TrafficLightColour(lamp, rgb);
                for (int channel = 0; channel < 3; ++channel)
                {
                    rgb[channel] *= 0.7f;
                }
                sizeParam = ModelEffects::TrafficSpriteSize * 0.175f;
                pull = 1.5f;
                unsigned char lens[4];
                ModelEffects::TrafficLensColour(lamp, lensSmall, lens);
                const RwRGBA lensTint { lens[0], lens[1], lens[2], lens[3] };
                DrawPreviewTrafficLens(Vec3 { light.pos[0], light.pos[1], light.pos[2] }, lensSmall, lensTint);
            }
        }
        else
        {
            float brightness = 0.0f;
            registered = ModelEffects::EvaluateLightOn(
                light, ModelEffects::LightRandomSeed(g_preview.modelId, light), clock, brightness);
            const float rgbScale = registered ? brightness : 1.0f;
            for (int channel = 0; channel < 3; ++channel)
            {
                rgb[channel] = light.color[channel] * rgbScale;
            }
        }

        ModelEffects::UpdateLightFade(fade, registered, clock);
        const float intensity = fade.fadedIntensity / 255.0f;
        if (intensity <= 0.01f)
        {
            continue;
        }

        const Vec3 center { light.pos[0], light.pos[1], light.pos[2] };
        const Vec3 drawPos = coronaDrawPos(center, pull);

        if (sizeParam <= 0.02f)
        {
            continue;
        }
        const float halfSize = std::clamp(sizeParam * 2.0f * PreviewViewWindow, 0.05f, 40.0f);
        const float depth = Dot(drawPos - eye, at);
        const float fogScale = std::min(40.0f, depth) * CWeather::Foggyness / 40.0f + 1.0f;
        float farFade = 1.0f;
        if (light.farClip > 1.0f)
        {
            if (depth >= light.farClip)
            {
                continue;
            }
            farFade = std::clamp((light.farClip - depth) / (light.farClip * 0.5f), 0.0f, 1.0f);
        }

        const float brightness = std::clamp(light.alpha * intensity * farFade, 0.0f, 1.0f);
        if (brightness <= 0.004f)
        {
            continue;
        }

        RwRGBA tint;
        tint.red = static_cast<RwUInt8>(std::min(255.0f, rgb[0] * 255.0f * brightness));
        tint.green = static_cast<RwUInt8>(std::min(255.0f, rgb[1] * 255.0f * brightness));
        tint.blue = static_cast<RwUInt8>(std::min(255.0f, rgb[2] * 255.0f * brightness));
        tint.alpha = static_cast<RwUInt8>(std::min(255.0f, brightness * 255.0f));

        RwRaster* raster = light.texture ? light.texture->raster : nullptr;
        if (!raster)
        {
            raster = ResolveParticleTxdTexture(light.textureName);
        }
        if (!raster)
        {
            if (!g_preview.fallbackGlowRaster)
            {
                g_preview.fallbackGlowRaster = MakeFallbackGlowRaster();
            }
            raster = g_preview.fallbackGlowRaster;
        }

        DrawPreviewGlowQuad(drawPos, right, up, halfSize, halfSize * fogScale, tint, raster,
            g_preview.framingMode != ModelEffects::FramingMode::EffectsOnly);

        if (light.flareType != 0)
        {
            Vec3 rel = drawPos - eye;
            const float depth = Dot(rel, at);
            if (depth > 0.1f)
            {
                const float ndcX = Dot(rel, right) / (depth * PreviewViewWindow);
                const float ndcY = Dot(rel, up) / (depth * PreviewViewWindow);
                const float sx = PreviewTargetSize * 0.5f + ndcX * PreviewTargetSize * 0.5f;
                const float sy = PreviewTargetSize * 0.5f - ndcY * PreviewTargetSize * 0.5f;
                const float pixelScale = PreviewTargetSize / 448.0f;

                if (!g_preview.fallbackGlowRaster)
                {
                    g_preview.fallbackGlowRaster = MakeFallbackGlowRaster();
                }
                RwRaster* starRaster = GetShinyStarRaster(g_preview.fallbackGlowRaster);
                for (const ModelEffects::FlareElement& element : ModelEffects::HeadLightsFlare)
                {
                    const float px = PreviewTargetSize * 0.5f + (sx - PreviewTargetSize * 0.5f) * element.position;
                    const float py = PreviewTargetSize * 0.5f + (sy - PreviewTargetSize * 0.5f) * element.position;
                    const float pixelSize = element.size * 4.0f * pixelScale;
                    const float ex = (px - PreviewTargetSize * 0.5f) / (PreviewTargetSize * 0.5f);
                    const float ey = (PreviewTargetSize * 0.5f - py) / (PreviewTargetSize * 0.5f);
                    const Vec3 dir = Normalize(at + right * (ex * PreviewViewWindow) + up * (ey * PreviewViewWindow));
                    const Vec3 worldPos = eye + dir * depth;
                    const float worldHalf = (pixelSize * 0.5f) * (2.0f * depth * PreviewViewWindow) / PreviewTargetSize;

                    RwRGBA flareTint;
                    flareTint.red = static_cast<RwUInt8>(std::min(255.0f, tint.red * element.colorMult[0] / 255.0f * 0.85f));
                    flareTint.green = static_cast<RwUInt8>(std::min(255.0f, tint.green * element.colorMult[1] / 255.0f * 0.85f));
                    flareTint.blue = static_cast<RwUInt8>(std::min(255.0f, tint.blue * element.colorMult[2] / 255.0f * 0.85f));
                    flareTint.alpha = 255;
                    DrawPreviewGlowQuad(worldPos, right, up, worldHalf, worldHalf, flareTint, starRaster, false);
                }
            }
        }
        drawn = true;
    }
    return drawn;
}
