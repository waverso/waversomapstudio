// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "CBaseModelInfo.h"

namespace ModelEffects
{
    constexpr float MeaningfulGeometryDiagonal = 0.50f;
    constexpr float ParticleFrameRadiusAlone = 1.10f;
    constexpr float ParticleFrameRadiusUnion = 0.55f;
    constexpr float UnionLightRadiusCap = 1.20f;

    struct LightInfo
    {
        float pos[3] {};
        float color[3] {};
        float alpha = 1.0f;
        float coronaSize = 0.0f;
        float farClip = 0.0f;
        float pointRange = 0.0f;
        float shadowSize = 0.0f;
        unsigned short flags = 0;
        unsigned char flashType = 0;
        unsigned char flareType = 0;
        unsigned effectIndex = 0;
        RwTexture* texture = nullptr;
        char textureName[24] {};
    };

    struct ParticleInfo
    {
        float pos[3] {};
        char name[24] {};
    };

    struct Summary
    {
        std::vector<LightInfo> lights;
        std::vector<ParticleInfo> particles;
        unsigned entriesSeen = 0;

        [[nodiscard]] bool Valid() const
        {
            return !lights.empty() || !particles.empty();
        }
        void Clear()
        {
            lights.clear();
            particles.clear();
            entriesSeen = 0;
        }
    };

    struct Bounds
    {
        float minimum[3] {};
        float maximum[3] {};
        bool valid = false;
    };

    enum class FramingMode : std::uint8_t
    {
        None,
        Union,
        EffectsOnly,
    };

    inline void FillEffect(C2dEffect* effect, unsigned entryIndex, Summary& out)
    {
        if (!effect)
        {
            return;
        }
        ++out.entriesSeen;
        if (effect->m_nType == EFFECT_LIGHT)
        {
            LightInfo light;
            light.pos[0] = effect->m_vecPosn.x;
            light.pos[1] = effect->m_vecPosn.y;
            light.pos[2] = effect->m_vecPosn.z;
            light.color[0] = static_cast<float>(effect->light.m_color.red) / 255.0f;
            light.color[1] = static_cast<float>(effect->light.m_color.green) / 255.0f;
            light.color[2] = static_cast<float>(effect->light.m_color.blue) / 255.0f;
            light.alpha = static_cast<float>(effect->light.m_color.alpha) / 255.0f;
            light.coronaSize = std::max(0.0f, effect->light.m_fCoronaSize);
            light.farClip = std::max(0.0f, effect->light.m_fCoronaFarClip);
            light.pointRange = std::max(0.0f, effect->light.m_fPointlightRange);
            light.shadowSize = std::max(0.0f, effect->light.m_fShadowSize);
            light.flags = effect->light.m_nFlags;
            light.flashType = effect->light.m_nCoronaFlashType;
            light.flareType = effect->light.m_nCoronaFlareType;
            light.effectIndex = entryIndex;
            light.texture = effect->light.m_pCoronaTex;
            out.lights.push_back(light);
        }
        else if (effect->m_nType == EFFECT_PARTICLE)
        {
            ParticleInfo particle {};
            particle.pos[0] = effect->m_vecPosn.x;
            particle.pos[1] = effect->m_vecPosn.y;
            particle.pos[2] = effect->m_vecPosn.z;
            std::memcpy(particle.name, effect->particle.m_szName, sizeof(particle.name) - 1);
            particle.name[sizeof(particle.name) - 1] = '\0';
            out.particles.push_back(particle);
        }
    }

    inline Summary Collect(const CBaseModelInfo* info)
    {
        Summary out;
        if (!info)
        {
            return out;
        }
        const int count = static_cast<int>(info->m_nNum2dEffects);
        for (int index = 0; index < count; ++index)
        {
            C2dEffect* effect = const_cast<CBaseModelInfo*>(info)->Get2dEffect(index);
            FillEffect(effect, static_cast<unsigned>(index), out);
        }
        return out;
    }

    inline std::uint32_t GeometryFxCount(RpGeometry* geometry)
    {
        using Fn = std::uint32_t(__cdecl*)(RpGeometry*);
        return reinterpret_cast<Fn>(0x4C4340)(geometry);
    }

    inline C2dEffect* GeometryFxAtIndex(RpGeometry* geometry, int index)
    {
        using Fn = C2dEffect*(__cdecl*)(RpGeometry*, int);
        return reinterpret_cast<Fn>(0x4C4A40)(geometry, index);
    }

    inline RpAtomic* CollectGeometryEffects(RpAtomic* atomic, void* data)
    {
        Summary* out = static_cast<Summary*>(data);
        RpGeometry* geometry = RpAtomicGetGeometry(atomic);
        if (!geometry)
        {
            return atomic;
        }
        const std::uint32_t count = GeometryFxCount(geometry);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            FillEffect(GeometryFxAtIndex(geometry, static_cast<int>(index)), out->entriesSeen, *out);
        }
        return atomic;
    }

    inline Summary CollectFromInstance(RwObject* instance)
    {
        Summary out;
        if (!instance)
        {
            return out;
        }
        const RwUInt8 type = RwObjectGetType(instance);
        if (type == rpATOMIC)
        {
            CollectGeometryEffects(reinterpret_cast<RpAtomic*>(instance), &out);
        }
        else if (type == rpCLUMP)
        {
            RpClumpForAllAtomics(reinterpret_cast<RpClump*>(instance), CollectGeometryEffects, &out);
        }
        return out;
    }

    inline float BoundsDiagonal(const Bounds& bounds)
    {
        if (!bounds.valid)
        {
            return 0.0f;
        }
        const float x = bounds.maximum[0] - bounds.minimum[0];
        const float y = bounds.maximum[1] - bounds.minimum[1];
        const float z = bounds.maximum[2] - bounds.minimum[2];
        return std::sqrt(x * x + y * y + z * z);
    }

    inline void ExpandBounds(Bounds& bounds, const float pos[3], float radius)
    {
        radius = std::max(0.0f, radius);
        if (!bounds.valid)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                bounds.minimum[axis] = pos[axis] - radius;
                bounds.maximum[axis] = pos[axis] + radius;
            }
            bounds.valid = true;
            return;
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            bounds.minimum[axis] = std::min(bounds.minimum[axis], pos[axis] - radius);
            bounds.maximum[axis] = std::max(bounds.maximum[axis], pos[axis] + radius);
        }
    }

    inline float LightFrameRadiusAlone(const LightInfo& light)
    {
        const float estimate = light.coronaSize > 0.02f ? light.coronaSize * 1.2f : light.pointRange * 0.32f;
        return std::clamp(estimate, 0.45f, 12.0f);
    }

    inline float LightFrameRadiusUnion(const LightInfo& light)
    {
        return std::clamp(light.coronaSize, 0.15f, UnionLightRadiusCap);
    }

    inline Bounds EffectBounds(const Summary& effects, bool effectsOnly)
    {
        Bounds bounds {};
        for (const LightInfo& light : effects.lights)
        {
            ExpandBounds(bounds, light.pos, effectsOnly ? LightFrameRadiusAlone(light) : LightFrameRadiusUnion(light));
        }
        for (const ParticleInfo& particle : effects.particles)
        {
            ExpandBounds(bounds, particle.pos, effectsOnly ? ParticleFrameRadiusAlone : ParticleFrameRadiusUnion);
        }
        return bounds;
    }

    struct Merged
    {
        Bounds bounds {};
        FramingMode mode = FramingMode::None;
    };

    inline Merged CombineWithGeometry(const Bounds& vertexBounds, const Summary& effects)
    {
        Merged result;
        result.bounds = vertexBounds;
        if (!effects.Valid())
        {
            return result;
        }

        const bool meaningful = vertexBounds.valid && BoundsDiagonal(vertexBounds) >= MeaningfulGeometryDiagonal;
        if (!meaningful)
        {
            result.mode = FramingMode::EffectsOnly;
            result.bounds = EffectBounds(effects, true);
            return result;
        }

        result.mode = FramingMode::Union;
        Bounds particleGlow {};
        for (const ParticleInfo& particle : effects.particles)
        {
            ExpandBounds(particleGlow, particle.pos, ParticleFrameRadiusUnion);
        }
        if (particleGlow.valid && BoundsDiagonal(particleGlow) > BoundsDiagonal(vertexBounds))
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                result.bounds.minimum[axis] = std::min(result.bounds.minimum[axis], particleGlow.minimum[axis]);
                result.bounds.maximum[axis] = std::max(result.bounds.maximum[axis], particleGlow.maximum[axis]);
            }
        }
        return result;
    }

    enum LightFlags : unsigned short
    {
        kFlagCheckObstacles = 0x0001,
        kFlagFogMask = 0x0006,
        kFlagWithoutCorona = 0x0008,
        kFlagOnlyLongDist = 0x0010,
        kFlagAtDay = 0x0020,
        kFlagAtNight = 0x0040,
        kFlagBlinking1 = 0x0080,
        kFlagOnlyFromBelow = 0x0100,
        kFlagBlinking2 = 0x0200,
        kFlagUpdateHeightAG = 0x0400,
        kFlagCheckDirection = 0x0800,
        kFlagBlinking3 = 0x1000,
    };

    inline constexpr unsigned short RandomSeedRandomiser[8] = { 0, 27034, 43861, 52326, 64495, 38437, 21930, 39117 };

    struct GameClock
    {
        unsigned timeMs = 0;
        unsigned frameCounter = 0;
        float timeStep = 0.0f;
    };

    inline unsigned EntitySeedForModel(int modelId)
    {
        return (static_cast<unsigned>(modelId) * 2654435761u) & 0xFFFFu;
    }

    inline unsigned LightRandomSeed(int modelId, const LightInfo& light)
    {
        return EntitySeedForModel(modelId) ^ RandomSeedRandomiser[light.effectIndex % 8];
    }

    inline unsigned BlinkRandom(unsigned frameCounter, unsigned seed)
    {
        return (seed * 1103515245u + frameCounter * 12345u + 2654435761u) >> 16 & 31u;
    }

    inline bool EvaluateLightOn(const LightInfo& light, unsigned seed, const GameClock& clock, float& brightnessOut)
    {
        brightnessOut = 1.0f;
        const unsigned t = clock.timeMs;
        bool applyLight = false;
        switch (light.flashType)
        {
            case 0:
                applyLight = true;
                break;
            case 1:
            case 2:
            {
                bool on = (((t ^ seed) & 0x60u) != 0);
                if (((seed ^ (t / 4096u)) & 0x3u) != 0)
                {
                    on = true;
                }
                applyLight = true;
                brightnessOut = on ? 1.0f : 0.0f;
                break;
            }
            case 3:
                applyLight = ((t + light.effectIndex * 128u) & 0x200u) != 0;
                break;
            case 4:
                applyLight = ((t + light.effectIndex * 256u) & 0x400u) != 0;
                break;
            case 5:
                applyLight = ((t + light.effectIndex * 512u) & 0x800u) != 0;
                break;
            case 6:
                if ((seed & 0xFFu) > 0x10u)
                {
                    applyLight = true;
                    break;
                }
                {
                    bool on = (((t ^ (seed * 8u)) & 0x60u) != 0);
                    if (((seed ^ (t / 4096u)) & 0x3u) != 0)
                    {
                        on = true;
                    }
                    applyLight = true;
                    brightnessOut = on ? 1.0f : 0.0f;
                }
                break;
            case 8:
                applyLight = false;
                break;
            case 9:
                applyLight = false;
                break;
            case 10:
                applyLight = true;
                break;
            case 11:
            case 12:
            case 13:
            {
                unsigned offset = t + 3333u * (light.flashType - 11u) + seed * 777u;
                const unsigned m10 = offset % 10000u;
                const unsigned mode = 9u * m10 / 10000u;
                const float fade = static_cast<float>(m10 - 1111u * mode) * 0.0009f;
                applyLight = true;
                switch (mode)
                {
                    case 0:
                        brightnessOut = fade;
                        break;
                    case 1:
                    case 2:
                        brightnessOut = 1.0f;
                        break;
                    case 3:
                        brightnessOut = 1.0f - fade;
                        break;
                    default:
                        applyLight = false;
                        break;
                }
                break;
            }
            default:
                break;
        }
        if (!applyLight)
        {
            brightnessOut = 0.0f;
            return false;
        }

        if (light.flags & kFlagBlinking1)
        {
            brightnessOut = (1.0f - BlinkRandom(clock.frameCounter, seed) * 0.012f) * brightnessOut;
        }
        if ((light.flags & kFlagBlinking2) && ((clock.frameCounter + seed) & 3u) != 0)
        {
            brightnessOut = 0.0f;
        }
        if (light.flags & kFlagBlinking3)
        {
            const unsigned phase = (clock.frameCounter + seed) & 0x3Fu;
            if (phase == 1u)
            {
                brightnessOut *= 0.5f;
            }
            else if (phase != 0u)
            {
                brightnessOut = 0.0f;
            }
        }
        return applyLight;
    }

    struct LightFadeState
    {
        float fadedIntensity = 0.0f;
    };

    inline void UpdateLightFade(LightFadeState& state, bool registeredThisFrame, const GameClock& clock, float fadeSpeed = 15.0f)
    {
        const float fadeStep = clock.timeStep * fadeSpeed;
        const float target = registeredThisFrame ? 255.0f : 0.0f;
        float value = state.fadedIntensity;
        if (value < target)
        {
            value += fadeStep;
        }
        else if (value > target)
        {
            value -= fadeStep;
        }
        state.fadedIntensity = std::clamp(value, 0.0f, 255.0f);
    }

    enum class TrafficLightState : std::uint8_t
    {
        Green = 0,
        Yellow = 1,
        Red = 2,
        Off = 3,
    };

    constexpr float TrafficSpriteSize = 1.0f;

    inline void TrafficLensColour(TrafficLightState state, bool smallLens, unsigned char out[4])
    {
        constexpr unsigned char Jitter = 25;
        switch (state)
        {
            case TrafficLightState::Green:
                out[0] = Jitter;
                out[1] = 255;
                out[2] = Jitter;
                break;
            case TrafficLightState::Yellow:
                out[0] = 255;
                out[1] = 128;
                out[2] = Jitter;
                break;
            case TrafficLightState::Red:
                out[0] = 255;
                out[1] = Jitter;
                out[2] = Jitter;
                break;
            default:
                out[0] = out[1] = out[2] = 0;
                break;
        }
        out[3] = smallLens ? 255 : 127;
    }

    inline TrafficLightState TrafficLightCycle(unsigned timeMs)
    {
        const unsigned masked = (timeMs / 2u) & 16383u;
        if (masked < 6000u)
        {
            return TrafficLightState::Red;
        }
        if (masked < 11000u)
        {
            return TrafficLightState::Green;
        }
        if (masked < 12000u)
        {
            return TrafficLightState::Yellow;
        }
        return TrafficLightState::Red;
    }

    inline TrafficLightState TrafficColorStateOf(const LightInfo& light)
    {
        if (light.color[0] > 200.0f / 255.0f)
        {
            return light.color[1] > 100.0f / 255.0f ? TrafficLightState::Yellow : TrafficLightState::Red;
        }
        return TrafficLightState::Green;
    }

    inline void TrafficLightColour(TrafficLightState state, float out[3])
    {
        switch (state)
        {
            case TrafficLightState::Green:
                out[0] = 0.0f;
                out[1] = 1.0f;
                out[2] = 0.0f;
                break;
            case TrafficLightState::Yellow:
                out[0] = 1.0f;
                out[1] = 1.0f;
                out[2] = 0.0f;
                break;
            case TrafficLightState::Red:
                out[0] = 1.0f;
                out[1] = 0.0f;
                out[2] = 0.0f;
                break;
            default:
                out[0] = out[1] = out[2] = 0.0f;
                break;
        }
    }

    struct SaMpPointLightParams
    {
        float color[3];
        float alpha;
        float coronaSize;
        float pointRange;
        const char* textureName;
        unsigned char flashType;
        unsigned short flags;
    };

    inline const SaMpPointLightParams* FindSaMpPointLight(const char* modelName)
    {
        if (!modelName)
        {
            return nullptr;
        }
        auto suffixNumber = [modelName](const char* prefix) -> int
        {
            size_t i = 0;
            for (; prefix[i]; ++i)
            {
                if (tolower(static_cast<unsigned char>(modelName[i])) != prefix[i])
                {
                    return -1;
                }
            }
            if (!modelName[i])
            {
                return -1;
            }
            int value = 0;
            bool any = false;
            for (; modelName[i] >= '0' && modelName[i] <= '9'; ++i)
            {
                value = value * 10 + (modelName[i] - '0');
                any = true;
            }
            return any && modelName[i] == '\0' ? value : -1;
        };

        constexpr float Alpha = 200.0f / 255.0f;
        static constexpr SaMpPointLightParams White { { 1.0f, 1.0f, 1.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams Red { { 1.0f, 0.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams Green { { 0.0f, 1.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams Blue { { 0.0f, 0.0f, 1.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams BlinkWhite { { 1.0f, 1.0f, 1.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtNight | kFlagBlinking3 };
        static constexpr SaMpPointLightParams BlinkRed { { 1.0f, 0.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtNight | kFlagBlinking3 };
        static constexpr SaMpPointLightParams BlinkGreen { { 0.0f, 1.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtNight | kFlagBlinking3 };
        static constexpr SaMpPointLightParams BlinkBlue { { 0.0f, 0.0f, 1.0f }, Alpha, 0.50f, 25.0f, "coronastar", 0,
            kFlagAtNight | kFlagBlinking3 };
        static constexpr SaMpPointLightParams PulseWhite { { 1.0f, 1.0f, 1.0f }, Alpha, 0.50f, 25.0f, "coronastar", 3,
            kFlagAtDay | kFlagAtNight };
        static constexpr SaMpPointLightParams PulseRed { { 1.0f, 0.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 3,
            kFlagAtDay | kFlagAtNight };
        static constexpr SaMpPointLightParams PulseGreen { { 0.0f, 1.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 3,
            kFlagAtDay | kFlagAtNight };
        static constexpr SaMpPointLightParams PulseBlue { { 0.0f, 0.0f, 1.0f }, Alpha, 0.50f, 25.0f, "coronastar", 3,
            kFlagAtDay | kFlagAtNight };
        static constexpr SaMpPointLightParams PulseOrange { { 1.0f, 152.0f / 255.0f, 0.0f }, Alpha, 0.50f, 25.0f, "coronastar", 3,
            kFlagAtDay | kFlagAtNight };
        static constexpr SaMpPointLightParams BigWhite { { 1.0f, 1.0f, 1.0f }, Alpha, 3.00f, 30.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams BigRed { { 1.0f, 0.0f, 0.0f }, Alpha, 3.00f, 30.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams BigGreen { { 0.0f, 1.0f, 0.0f }, Alpha, 3.00f, 30.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams BigBlue { { 0.0f, 0.0f, 1.0f }, Alpha, 3.00f, 30.0f, "coronastar", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };
        static constexpr SaMpPointLightParams Moon { { 1.0f, 1.0f, 1.0f }, Alpha, 5.00f, 30.0f, "coronamoon", 0,
            kFlagAtDay | kFlagAtNight | kFlagUpdateHeightAG };

        const int number = suffixNumber("pointlight");
        switch (number)
        {
            case 1:
                return &White;
            case 2:
                return &Red;
            case 3:
                return &Green;
            case 4:
                return &Blue;
            case 5:
                return &BlinkWhite;
            case 6:
                return &BlinkRed;
            case 7:
                return &BlinkGreen;
            case 8:
                return &BlinkBlue;
            case 9:
                return &PulseWhite;
            case 10:
                return &PulseRed;
            case 11:
                return &PulseGreen;
            case 12:
                return &PulseBlue;
            case 13:
                return &PulseRed;
            case 14:
                return &PulseOrange;
            case 15:
                return &BigWhite;
            case 16:
                return &BigRed;
            case 17:
                return &BigGreen;
            case 18:
                return &BigBlue;
            default:
                break;
        }
        if (suffixNumber("pointlightmoon") > 0)
        {
            return &Moon;
        }
        return nullptr;
    }

    struct FlareElement
    {
        float position;
        float size;
        float colorMult[3];
    };

    inline constexpr FlareElement HeadLightsFlare[26] {
        { 4.00f, 5.0f, { 60, 60, 60 } },
        { 3.00f, 7.0f, { 40, 40, 40 } },
        { 2.00f, 5.0f, { 40, 40, 40 } },
        { 1.50f, 6.0f, { 90, 90, 90 } },
        { 1.25f, 5.0f, { 40, 40, 40 } },
        { 0.80f, 14.0f, { 60, 60, 60 } },
        { 0.60f, 4.0f, { 40, 40, 40 } },
        { 0.25f, 10.0f, { 60, 60, 60 } },
        { 0.10f, 6.0f, { 30, 30, 30 } },
        { 0.05f, 14.0f, { 50, 50, 50 } },
        { -0.03f, 3.0f, { 30, 30, 30 } },
        { -0.10f, 6.0f, { 60, 60, 60 } },
        { -0.30f, 5.0f, { 30, 30, 30 } },
        { -0.40f, 60.0f, { 30, 30, 30 } },
        { -0.55f, 4.0f, { 40, 40, 40 } },
        { -0.75f, 14.0f, { 50, 50, 50 } },
        { -0.90f, 5.2f, { 35, 35, 35 } },
        { -1.00f, 11.0f, { 55, 55, 55 } },
        { -1.20f, 3.5f, { 35, 35, 35 } },
        { -1.35f, 9.0f, { 50, 50, 50 } },
        { -1.70f, 54.0f, { 35, 35, 35 } },
        { -2.00f, 5.0f, { 50, 50, 50 } },
        { -2.50f, 4.5f, { 35, 35, 35 } },
        { -3.00f, 14.0f, { 50, 50, 50 } },
        { -6.00f, 24.0f, { 70, 70, 70 } },
        { -9.00f, 14.0f, { 70, 50, 70 } },
    };
}
