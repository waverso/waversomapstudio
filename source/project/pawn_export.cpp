// Source Repository: https://github.com/waverso/waversomapstudio

#include "pawn_export.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "../shared/animation_catalog.h"
#include "../catalog/database.h"
#include "../objects/model_constraints.h"
#include "../pickups/pickup_position.h"

namespace PawnExport
{
    namespace
    {
        std::string Quote(const char* text)
        {
            std::string result = "\"";
            for (; *text; ++text)
            {
                switch (*text)
                {
                    case '\\':
                        result += "\\\\";
                        break;
                    case '"':
                        result += "\\\"";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    case '\t':
                        result += "\\t";
                        break;
                    default:
                        result += *text;
                        break;
                }
            }
            return result + '"';
        }

        std::string Color(unsigned int value)
        {
            std::ostringstream stream;
            stream << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << value;
            return stream.str();
        }

        std::string Float(float value)
        {
            char buffer[128];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value == 0 ? 0.0f : value,
                std::chars_format::general, std::numeric_limits<float>::max_digits10);
            if (result.ec != std::errc {})
            {
                throw std::runtime_error("Cannot format an exported float.");
            }
            std::string text(buffer, result.ptr);
            if (text.find('.') == std::string::npos)
            {
                const auto exponent = text.find('e');
                text.insert(exponent == std::string::npos ? text.size() : exponent, ".0");
            }
            return text;
        }

        void Vector(std::ostream& stream, const float value[3])
        {
            stream << Float(value[0]) << ", " << Float(value[1]) << ", " << Float(value[2]);
        }
    }

    std::string Build(const ProjectDocument::Items& items, const Options& options)
    {
        std::string error;
        if (!ProjectDocument::Validate(items, error))
        {
            throw std::runtime_error(error);
        }
        int slideCount = 0;
        int objectCount = 0;
        int vehicleCount = 0;
        int removalCount = 0;
        int boxAreaCount = 0;
        int pickupCount = 0;
        int actorCount = 0;
        std::vector<const ItemSnapshot*> races;
        std::unordered_map<int, const ItemSnapshot*> roots;
        std::unordered_set<int> parents;
        bool actorReferenceNeeded = false;
        bool vehicleReferenceNeeded = false;
        bool temporaryNeeded = false;
        std::array<bool, AnimationCatalog::LibraryCount> actorAnimationLibraries {};
        for (const auto& item : items)
        {
            roots.emplace(item.id, item.parentId < 0 ? &item : roots.at(item.parentId));
            parents.insert(item.parentId);
            const auto& d = item.data;
            if (options.Includes(Races) && d.race.HasValue())
            {
                races.push_back(&item);
            }
            vehicleCount += options.Includes(Vehicles) && d.vehicle.HasValue();
            vehicleReferenceNeeded |= options.Includes(Vehicles) && d.vehicle.HasValue() && d.vehicle.Get().health != 1000.0f;
            removalCount += options.Includes(Removals) && d.removeBuilding.HasValue();
            boxAreaCount += options.Includes(BoxAreas) && d.boxArea.HasValue();
            if (options.Includes(Actors) && d.actor.HasValue())
            {
                ++actorCount;
                const auto& actor = d.actor.Get();
                actorReferenceNeeded
                    |= d.actorAnimation.HasValue() || (!options.streamer && (actor.health != 100.0f || !actor.invulnerable));
                if (!options.openMp && d.actorAnimation.HasValue())
                {
                    const auto& animation = d.actorAnimation.Get();
                    const auto* catalog = AnimationCatalog::Find(animation.library, animation.name);
                    if (_stricmp(animation.library, "ped") != 0)
                    {
                        actorAnimationLibraries[catalog->library] = true;
                    }
                }
            }
            if (options.Includes(Pickups) && d.pickup.HasValue())
            {
                ++pickupCount;
                if (!PickupPosition::ValidInput(d.transform.pos))
                {
                    throw std::runtime_error("Pickup coordinates exceed the GTA coordinate range.");
                }
            }
            if (!options.Includes(Objects) || !d.model.HasValue())
            {
                continue;
            }
            ++objectCount;
            if (d.model.Get().modelId < 321 || (d.model.Get().modelId >= 400 && d.model.Get().modelId <= 611))
            {
                throw std::runtime_error("Choose a valid model for every object before exporting.");
            }
            if (d.slide.HasValue())
            {
                ++slideCount;
            }
            else if (d.MaterialCount() || d.MaterialTextCount())
            {
                temporaryNeeded = true;
            }
        }
        if (!options.streamer && objectCount > MaxNativeObjects)
        {
            throw std::runtime_error(
                "Native exports support up to " + std::to_string(MaxNativeObjects) + " objects. Select Streamer Plugin.");
        }
        if (vehicleCount > MaxVehicles)
        {
            throw std::runtime_error("Vehicle exports support up to " + std::to_string(MaxVehicles) + " vehicles.");
        }
        if (removalCount > MaxRemovalInstructions)
        {
            throw std::runtime_error(
                "Exports support up to " + std::to_string(MaxRemovalInstructions) + " RemoveBuildingForPlayer instructions.");
        }
        if (boxAreaCount > MaxGangZones)
        {
            throw std::runtime_error("Box Area exports support up to " + std::to_string(MaxGangZones) + " gang zones.");
        }
        for (const auto* race : races)
        {
            const auto count = std::count_if(items.begin(), items.end(),
                [&](const auto& item)
                {
                    return item.parentId == race->id;
                });
            if (count < (race->data.race.Get().mode == 1 ? 2 : 1))
            {
                throw std::runtime_error("Race '" + std::string(race->name) + "' needs more checkpoints before exporting.");
            }
        }
        const char* create = options.streamer ? "CreateDynamicObject" : "CreateObject";
        if (!options.streamer && pickupCount > MaxNativePickups)
        {
            throw std::runtime_error("Native exports support up to 4096 pickups. Select Streamer Plugin.");
        }
        if (!options.streamer && actorCount > MaxNativeActors)
        {
            throw std::runtime_error("Native exports support up to 1000 actors. Select Streamer Plugin.");
        }
        const char* material = options.streamer ? "SetDynamicObjectMaterial" : "SetObjectMaterial";
        const char* text = options.streamer ? "SetDynamicObjectMaterialText" : "SetObjectMaterialText";
        const char* move = options.streamer ? "MoveDynamicObject" : "MoveObject";
        const bool iterateAreas = options.loopStyle != LoopStyle::For;
        const char* areaLoop = iterateAreas ? "foreach (new areaid : BoxAreas)\n"
                                            : "for (new areaid = 0; areaid < sizeof(g_BoxAreas); ++areaid)\n";
        const bool preloadActorAnimations = std::any_of(actorAnimationLibraries.begin(), actorAnimationLibraries.end(),
            [](bool used)
            {
                return used;
            });
        std::ostringstream out;
        out.imbue(std::locale::classic());
        const ItemSnapshot* previousGroup = nullptr;
        const auto group = [&](const ItemSnapshot& item)
        {
            const auto* root = roots.at(item.id);
            if (root == previousGroup)
            {
                return;
            }
            const bool parent = parents.contains(root->id);
            if (previousGroup && (parent || parents.contains(previousGroup->id)))
            {
                out << '\n';
            }
            if (parent && options.comments)
            {
                std::string name = root->name;
                for (char& c : name)
                {
                    if (static_cast<unsigned char>(c) < 32 || c == 127 || c == '\\')
                    {
                        c = ' ';
                    }
                }
                out << "    // " << name << '\n';
            }
            previousGroup = root;
        };
        out << (options.openMp ? "#include <open.mp>\n" : "#include <a_samp>\n");
        if (options.streamer)
        {
            out << "#include <streamer>\n";
        }
        if (boxAreaCount)
        {
            if (options.yTimers)
            {
                out << "#include <YSI_Coding\\y_timers>\n";
            }
            else if (options.loopStyle != LoopStyle::For)
            {
                out << (options.loopStyle == LoopStyle::Foreach ? "#include <foreach>\n" : "#include <YSI_Data/y_iterate>\n");
            }
        }
        if (!races.empty())
        {
            out << "\n#include \"waversomapstudio_races.inc\"\n\nenum\n{\n";
            for (std::size_t index = 0; index < races.size(); ++index)
            {
                out << "    MAP_RACE_" << index << (index + 1 < races.size() ? ",\n" : "\n");
            }
            out << "};\n";
        }
        if (slideCount)
        {
            out << "\nnew " << "g_MapSlideObjects[" << slideCount << "];\n";
        }
        if (boxAreaCount)
        {
            out << "\nstock bool:IsPointInBoxArea(Float:x, Float:y, Float:z,\n"
                   "    Float:minX, Float:minY, Float:minZ, Float:maxX, Float:maxY, Float:maxZ)\n{\n"
                   "    return x >= minX && x < maxX && y >= minY && y < maxY && z >= minZ && z < maxZ;\n}\n\n"
                   "enum E_BOX_AREA\n{\n"
                   "    BoxAreaName[64],\n    Float:BoxAreaBounds[6],\n    BoxAreaColor\n};\n\n"
                   "new const g_BoxAreas[][E_BOX_AREA] =\n{\n";
            int index = 0;
            for (const auto& item : items)
            {
                if (!item.data.boxArea.HasValue())
                {
                    continue;
                }
                const auto& area = item.data.boxArea.Get();
                float bounds[6];
                for (int axis = 0; axis < 3; ++axis)
                {
                    bounds[axis] = item.data.transform.pos[axis] - area.GetSize(axis) * 0.5f;
                    bounds[axis + 3] = item.data.transform.pos[axis] + area.GetSize(axis) * 0.5f;
                    if (!std::isfinite(bounds[axis]) || !std::isfinite(bounds[axis + 3]) || bounds[axis] >= bounds[axis + 3])
                    {
                        throw std::runtime_error("Box area bounds cannot be represented in world coordinates.");
                    }
                }
                out << "    {" << Quote(item.name) << ", {";
                for (int bound = 0; bound < 6; ++bound)
                {
                    if (bound)
                    {
                        out << ", ";
                    }
                    out << Float(bounds[bound]);
                }
                out << "}, " << Color(area.colorRGBA) << (++index < boxAreaCount ? "},\n" : "}\n");
            }
            out << "};\n\nnew bool:g_PlayerInsideBoxArea[MAX_PLAYERS][sizeof(g_BoxAreas)];\n"
                   "new g_BoxAreaGangZones[sizeof(g_BoxAreas)];\n";
            if (iterateAreas)
            {
                out << "new Iterator:BoxAreas<sizeof(g_BoxAreas)>;\n";
            }
            if (!options.yTimers)
            {
                out << "new g_BoxAreaTimer;\n";
            }
        }
        out << "\npublic OnGameModeInit()\n{\n";
        if (temporaryNeeded)
        {
            out << "    new " << "mapObject;\n\n";
        }
        if (actorReferenceNeeded)
        {
            out << (options.streamer ? "    new STREAMER_TAG_ACTOR:mapActor;\n\n" : "    new mapActor;\n\n");
        }
        if (vehicleReferenceNeeded)
        {
            out << "    new mapVehicle;\n\n";
        }
        int slideIndex = 0;
        std::size_t raceIndex = 0;
        for (const auto& item : items)
        {
            const auto& d = item.data;
            if (!(options.Includes(Races) && d.race.HasValue()) && !(options.Includes(Actors) && d.actor.HasValue())
                && !(options.Includes(Pickups) && d.pickup.HasValue()) && !(options.Includes(Vehicles) && d.vehicle.HasValue())
                && !(options.Includes(Objects) && d.model.HasValue()))
            {
                continue;
            }
            group(item);
            if (d.race.HasValue())
            {
                const auto index = raceIndex++;
                const auto& race = d.race.Get();
                out << "    MapRace_Create(MAP_RACE_" << index << ", " << Quote(item.name) << ", "
                    << (race.mode == 1 ? "MAPRACE_CIRCUIT" : "MAPRACE_POINT_TO_POINT") << ", " << race.laps << ");\n";
                std::vector<const ItemSnapshot*> checkpoints;
                for (const auto& child : items)
                {
                    if (child.parentId == item.id)
                    {
                        checkpoints.push_back(&child);
                    }
                }
                std::sort(checkpoints.begin(), checkpoints.end(),
                    [](const auto* a, const auto* b)
                    {
                        return a->order < b->order;
                    });
                for (const auto* child : checkpoints)
                {
                    const auto& cp = child->data.raceCheckpoint.Get();
                    out << "    MapRace_AddCheckpoint(MAP_RACE_" << index << ", ";
                    Vector(out, child->data.transform.pos);
                    out << ", " << Float(cp.radius) << ", ";
                    if (cp.type == RaceCheckpointData::AutoGround)
                    {
                        out << "MAPRACE_AUTO_GROUND";
                    }
                    else if (cp.type == RaceCheckpointData::AutoAir)
                    {
                        out << "MAPRACE_AUTO_AIR";
                    }
                    else
                    {
                        out << cp.type;
                    }
                    out << ");\n";
                }
                continue;
            }
            if (d.actor.HasValue())
            {
                const auto& actor = d.actor.Get();
                const bool reference
                    = d.actorAnimation.HasValue() || (!options.streamer && (actor.health != 100.0f || !actor.invulnerable));
                out << "    ";
                if (reference)
                {
                    out << "mapActor = ";
                }
                out << (options.streamer ? "CreateDynamicActor(" : "CreateActor(") << actor.skinId << ", ";
                Vector(out, d.transform.pos);
                out << ", " << Float(d.transform.rot[2]);
                if (options.streamer)
                {
                    out << ", " << (actor.invulnerable ? "true" : "false") << ", " << Float(actor.health) << ", 0, -1, -1, "
                        << Float(actor.streamDistance);
                }
                out << ");\n";
                if (!options.streamer && reference && !actor.invulnerable)
                {
                    out << "    SetActorInvulnerable(mapActor, false);\n";
                }
                if (!options.streamer && reference && actor.health != 100.0f)
                {
                    out << "    SetActorHealth(mapActor, " << Float(actor.health) << ");\n";
                }
                if (d.actorAnimation.HasValue())
                {
                    const auto& animation = d.actorAnimation.Get();
                    out << "    " << (options.streamer ? "ApplyDynamicActorAnimation(" : "ApplyActorAnimation(") << "mapActor, "
                        << Quote(animation.library) << ", " << Quote(animation.name) << ", " << Float(animation.delta) << ", "
                        << (animation.loop ? "true" : "false") << ", " << (animation.lockX ? "true" : "false") << ", "
                        << (animation.lockY ? "true" : "false") << ", " << (animation.freeze ? "true" : "false") << ", "
                        << animation.time << ");\n";
                }
                continue;
            }
            if (d.pickup.HasValue())
            {
                const auto& pickup = d.pickup.Get();
                out << "    " << (options.streamer ? "CreateDynamicPickup(" : "CreatePickup(") << pickup.modelId << ", 1, ";
                Vector(out, d.transform.pos);
                out << (options.streamer ? ", -1, -1, -1, " : ", -1");
                if (options.streamer)
                {
                    out << Float(pickup.streamDistance);
                }
                out << ");\n";
                continue;
            }
            if (d.vehicle.HasValue())
            {
                const auto& vehicle = d.vehicle.Get();
                const bool reference = vehicle.health != 1000.0f;
                out << "    ";
                if (reference)
                {
                    out << "mapVehicle = ";
                }
                out << "CreateVehicle(" << vehicle.modelId << ", ";
                Vector(out, d.transform.pos);
                out << ", " << Float(d.transform.rot[2]) << ", " << vehicle.colors[0] << ", " << vehicle.colors[1] << ", -1);\n";
                if (reference)
                {
                    out << "    SetVehicleHealth(mapVehicle, " << Float(vehicle.health) << ");\n";
                }
                continue;
            }
            if (!d.model.HasValue())
            {
                continue;
            }
            const bool needsReference = d.slide.HasValue() || d.MaterialCount() || d.MaterialTextCount();
            const bool materialColorSupported = d.model.Get().modelId >= 0 && d.model.Get().modelId < CatalogDatabase::MaxModelId
                && CatalogDatabase::ModelSupportsMaterialColor(static_cast<std::uint16_t>(d.model.Get().modelId));
            const std::string object
                = d.slide.HasValue() ? "g_MapSlideObjects[" + std::to_string(slideIndex++) + "]" : "mapObject";
            out << "    ";
            if (needsReference)
            {
                out << object << " = ";
            }
            out << create << '(' << d.model.Get().modelId << ", ";
            Vector(out, d.transform.pos);
            out << ", ";
            Vector(out, ModelConstraints::AppliedRotation(d.model.Get().modelId, d.transform.rot).data());
            if (options.streamer)
            {
                out << ", -1, -1, -1, " << Float(d.model.Get().streamDistance);
            }
            out << ", " << Float(d.model.Get().drawDistance) << ");\n";

            struct Entry
            {
                int sequence;
                int slot;
                bool text;
            };
            std::vector<Entry> entries;
            for (int i = 0; i < d.MaterialCount(); ++i)
            {
                entries.push_back({ d.materials[i].sequence, i, false });
            }
            for (int i = 0; i < d.MaterialTextCount(); ++i)
            {
                entries.push_back({ d.materialTexts[i].sequence, i, true });
            }
            std::stable_sort(entries.begin(), entries.end(),
                [](const Entry& a, const Entry& b)
                {
                    return a.sequence < b.sequence;
                });
            for (const auto& entry : entries)
            {
                if (!entry.text)
                {
                    const auto& m = d.materials[entry.slot];
                    const bool texture = m.textureModelId > 0 && m.textureName[0];
                    out << "    " << material << '(' << object << ", " << m.materialIndex << ", "
                        << (texture                        ? m.textureModelId
                                   : m.textureModelId == 0 ? 0
                                                           : -1)
                        << ", " << Quote(texture ? m.txdName : "none") << ", " << Quote(texture ? m.textureName : "none") << ", "
                        << Color(materialColorSupported ? m.colorArgb : 0) << ");\n";
                }
                else
                {
                    const auto& m = d.materialTexts[entry.slot];
                    out << "    " << text << '(' << object << ", ";
                    if (options.streamer)
                    {
                        out << m.materialIndex << ", " << Quote(m.text);
                    }
                    else
                    {
                        out << Quote(m.text) << ", " << m.materialIndex;
                    }
                    static constexpr const char* sizes[] = { "32x32", "64x32", "64x64", "128x32", "128x64", "128x128", "256x32",
                        "256x64", "256x128", "256x256", "512x64", "512x128", "512x256", "512x512" };
                    static constexpr const char* alignments[] = { "OBJECT_MATERIAL_TEXT_ALIGN_LEFT",
                        "OBJECT_MATERIAL_TEXT_ALIGN_CENTER", "OBJECT_MATERIAL_TEXT_ALIGN_RIGHT" };
                    const char* alignment = options.openMp && m.alignment == 1 ? "OBJECT_MATERIAL_TEXT_ALIGN_CENT"
                        : options.openMp && m.alignment == 2                   ? "OBJECT_MATERIAL_TEXT_ALIGN_RIGT"
                                                                               : alignments[m.alignment];
                    out << ", OBJECT_MATERIAL_SIZE_" << sizes[m.materialSize / 10 - 1] << ", " << Quote(m.fontFace) << ", "
                        << m.fontSize << ", " << (m.bold ? "true" : "false") << ", " << Color(m.fontColorArgb) << ", "
                        << Color(m.backgroundColorArgb) << ", " << alignment << ");\n";
                }
            }
        }
        if (boxAreaCount)
        {
            out << "    for (new areaid = 0; areaid < sizeof(g_BoxAreas); ++areaid)\n    {\n";
            if (iterateAreas)
            {
                out << "        Iter_Add(BoxAreas, areaid);\n";
            }
            out << "        g_BoxAreaGangZones[areaid] = GangZoneCreate(\n"
                   "            g_BoxAreas[areaid][BoxAreaBounds][0], "
                   "g_BoxAreas[areaid][BoxAreaBounds][1],\n"
                   "            g_BoxAreas[areaid][BoxAreaBounds][3], "
                   "g_BoxAreas[areaid][BoxAreaBounds][4]);\n    }\n";
            if (!options.yTimers)
            {
                out << "    g_BoxAreaTimer = SetTimer(\"CheckBoxAreas\", 500, true);\n";
            }
        }
        out << "    return 1;\n}\n";
        if (preloadActorAnimations || boxAreaCount || removalCount)
        {
            previousGroup = nullptr;
            out << "\npublic OnPlayerConnect(playerid)\n{\n";
            if (preloadActorAnimations)
            {
                out << "    PreloadMapActorAnimations(playerid);\n";
            }
            if (boxAreaCount)
            {
                out << "    " << areaLoop
                    << "    {\n        g_PlayerInsideBoxArea[playerid][areaid] = false;\n"
                       "        if (g_BoxAreaGangZones[areaid] != INVALID_GANG_ZONE)\n"
                       "            GangZoneShowForPlayer(playerid, g_BoxAreaGangZones[areaid], "
                       "g_BoxAreas[areaid][BoxAreaColor]);\n    }\n";
            }
            for (const auto& item : items)
            {
                const auto& d = item.data;
                if (!options.Includes(Removals) || !d.removeBuilding.HasValue())
                {
                    continue;
                }
                group(item);
                out << "    RemoveBuildingForPlayer(playerid, " << d.removeBuilding.Get().modelId << ", ";
                Vector(out, d.transform.pos);
                out << ", " << Float(d.removeBuilding.Get().radius) << ");\n";
            }
            out << "    return 1;\n}\n";
        }
        if (preloadActorAnimations)
        {
            out << "\nstock PreloadMapActorAnimations(playerid)\n{\n";
            for (int library = 0; library < AnimationCatalog::LibraryCount; ++library)
            {
                if (actorAnimationLibraries[library])
                {
                    out << "    ApplyAnimation(playerid, " << Quote(AnimationCatalog::Libraries[library])
                        << ", \"null\", 4.1, false, false, false, false, 0, 0);\n";
                }
            }
            out << "    return 1;\n}\n";
        }
        if (slideCount)
        {
            out << "\nstock StartMapSlides()\n{\n";
            slideIndex = 0;
            previousGroup = nullptr;
            for (const auto& item : items)
            {
                const auto& d = item.data;
                if (!d.model.HasValue() || !d.slide.HasValue())
                {
                    continue;
                }
                group(item);
                out << "    " << move << "(g_MapSlideObjects[" << slideIndex++ << "], ";
                Vector(out, d.slide.Get().targetPos);
                out << ", " << Float(d.slide.Get().speed) << ", ";
                Vector(out, ModelConstraints::AppliedRotation(d.model.Get().modelId, d.slide.Get().targetRot).data());
                out << ");\n";
            }
            out << "    return 1;\n}\n";
        }
        if (boxAreaCount)
        {
            out << "\npublic OnGameModeExit()\n{\n";
            if (!options.yTimers)
            {
                out << "    KillTimer(g_BoxAreaTimer);\n";
            }
            out << "    " << areaLoop
                << "        if (g_BoxAreaGangZones[areaid] != INVALID_GANG_ZONE)\n"
                   "            GangZoneDestroy(g_BoxAreaGangZones[areaid]);\n"
                   "    return 1;\n}\n";
        }
        if (!races.empty())
        {
            out << "\npublic OnMapRaceStart(playerid, raceid)\n{\n"
                   "    new name[MAPRACE_MAX_NAME + 1], message[96];\n"
                   "    MapRace_GetName(raceid, name);\n"
                   "    format(message, sizeof(message), \"Started race '%s'.\", name);\n"
                   "    SendClientMessage(playerid, 0x3AAEFFFF, message);\n    return 1;\n}\n\n"
                   "public OnMapRaceCheckpoint(playerid, raceid, checkpoint, lap)\n{\n"
                   "    new name[MAPRACE_MAX_NAME + 1], message[128];\n"
                   "    MapRace_GetName(raceid, name);\n"
                   "    format(message, sizeof(message), \"%s (CP %d): lap %d.\", "
                   "name, checkpoint, lap);\n"
                   "    SendClientMessage(playerid, 0xFFFFFFFF, message);\n    return 1;\n}\n\n"
                   "public OnMapRaceLap(playerid, raceid, lap, milliseconds)\n{\n"
                   "    new message[80];\n"
                   "    format(message, sizeof(message), \"Lap %d completed in %.2f seconds.\", "
                   "lap, milliseconds / 1000.0);\n"
                   "    SendClientMessage(playerid, 0x3AAEFFFF, message);\n    return 1;\n}\n\n"
                   "public OnMapRaceFinish(playerid, raceid, milliseconds)\n{\n"
                   "    new name[MAPRACE_MAX_NAME + 1], message[128];\n"
                   "    MapRace_GetName(raceid, name);\n"
                   "    format(message, sizeof(message), \"Finished race '%s' in %.2f seconds.\", "
                   "name, milliseconds / 1000.0);\n"
                   "    SendClientMessage(playerid, 0x3AAEFFFF, message);\n    return 1;\n}\n\n"
                   "public OnMapRaceStop(playerid, raceid, reason)\n{\n"
                   "    new message[64];\n"
                   "    format(message, sizeof(message), \"Race stopped (reason %d).\", reason);\n"
                   "    SendClientMessage(playerid, 0xFFFFFFFF, message);\n    return 1;\n}\n";
        }
        if (boxAreaCount)
        {
            out << (options.yTimers ? "\nptask CheckBoxAreas[500](playerid)\n{\n"
                                    : "\nforward CheckBoxAreas();\npublic CheckBoxAreas()\n{\n");
            out << "    new Float:x, Float:y, Float:z, message[128];\n";
            if (!options.yTimers)
            {
                out << (options.loopStyle == LoopStyle::For
                        ? "    for (new playerid = 0; playerid < MAX_PLAYERS; ++playerid)\n    {\n"
                        : "    foreach (new playerid : Player)\n    {\n");
                if (options.loopStyle == LoopStyle::For)
                {
                    out << "        if (!IsPlayerConnected(playerid)) continue;\n";
                }
            }
            const char* indent = options.yTimers ? "    " : "        ";
            out << indent << "if (!GetPlayerPos(playerid, x, y, z)) " << (options.yTimers ? "return 1;\n" : "continue;\n")
                << indent << areaLoop << indent << "{\n"
                << indent << "    new bool:inside = IsPointInBoxArea(x, y, z,\n"
                << indent << "        g_BoxAreas[areaid][BoxAreaBounds][0], g_BoxAreas[areaid][BoxAreaBounds][1],\n"
                << indent << "        g_BoxAreas[areaid][BoxAreaBounds][2], g_BoxAreas[areaid][BoxAreaBounds][3],\n"
                << indent << "        g_BoxAreas[areaid][BoxAreaBounds][4], g_BoxAreas[areaid][BoxAreaBounds][5]);\n"
                << indent << "    if (inside && !g_PlayerInsideBoxArea[playerid][areaid])\n"
                << indent << "    {\n"
                << indent
                << "        format(message, sizeof(message), \"You entered area '%s' (index %i).\", "
                   "g_BoxAreas[areaid][BoxAreaName], areaid);\n"
                << indent << "        SendClientMessage(playerid, 0x3AAEFFFF, message);\n"
                << indent << "    }\n"
                << indent << "    g_PlayerInsideBoxArea[playerid][areaid] = inside;\n"
                << indent << "}\n";
            if (!options.yTimers)
            {
                out << "    }\n";
            }
            out << "    return 1;\n}\n";
        }
        return out.str();
    }
}
