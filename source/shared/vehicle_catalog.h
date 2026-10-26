// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <cstdint>

namespace VehicleCatalog
{
    inline constexpr int FirstId = 400;
    inline constexpr int LastId = 611;
    inline constexpr float PreviewYaw = 135.0f;
    inline constexpr float PreviewPitch = 22.0f;

    inline constexpr std::array<int, 13> AnimatedModels = { 430, 453, 454, 460, 511, 512, 513, 593, 476, 524, 539, 553, 582 };

    inline constexpr bool HasAnimation(int modelId)
    {
        for (int id : AnimatedModels)
        {
            if (id == modelId)
            {
                return true;
            }
        }
        return false;
    }

    inline constexpr bool HasPropellerAnimation(int modelId)
    {
        return modelId == 460 || modelId == 476 || modelId == 511 || modelId == 512 || modelId == 513 || modelId == 539
            || modelId == 553 || modelId == 593;
    }

    struct Entry
    {
        const char* name;
        const char* modelName;
        std::uint16_t maxSpeed;
        std::uint8_t category;
        std::uint8_t seats;
        std::uint8_t modificationShop;
        std::uint8_t colorSlots;
    };

    inline constexpr const char* Categories[] = {
        "Airplanes",
        "Bikes",
        "Boats",
        "Convertibles",
        "Helicopters",
        "Industrial",
        "Lowriders",
        "Off-Road",
        "Public Service",
        "RC Vehicles",
        "Saloons",
        "Sports",
        "Station Wagons",
        "Trailers",
        "Unique",
    };

    inline constexpr const char* ModificationShops[] = { "None", "TransFender", "Loco Low Co.", "Wheel Arch Angels" };

    inline constexpr std::array<Entry, LastId - FirstId + 1> Entries = { {
        { "Landstalker", "landstal", 157, 7, 4, 1, 1 },
        { "Bravura", "bravura", 147, 10, 2, 1, 1 },
        { "Buffalo", "buffalo", 185, 11, 2, 1, 1 },
        { "Linerunner", "linerun", 111, 5, 2, 0, 1 },
        { "Perennial", "peren", 133, 12, 4, 0, 2 },
        { "Sentinel", "sentinel", 163, 10, 4, 1, 1 },
        { "Dumper", "dumper", 110, 14, 1, 0, 0 },
        { "Fire Truck", "firetruk", 148, 8, 2, 0, 1 },
        { "Trashmaster", "trash", 99, 5, 2, 0, 1 },
        { "Stretch", "stretch", 157, 14, 4, 1, 2 },
        { "Manana", "manana", 129, 10, 2, 1, 1 },
        { "Infernus", "infernus", 221, 11, 2, 1, 1 },
        { "Voodoo", "voodoo", 168, 6, 2, 2, 1 },
        { "Pony", "pony", 111, 5, 4, 0, 1 },
        { "Mule", "mule", 105, 5, 2, 0, 1 },
        { "Cheetah", "cheetah", 192, 11, 2, 1, 1 },
        { "Ambulance", "ambulan", 153, 8, 4, 0, 2 },
        { "Leviathan", "leviathn", 116, 4, 2, 0, 0 },
        { "Moonbeam", "moonbeam", 115, 12, 4, 1, 1 },
        { "Esperanto", "esperant", 148, 10, 2, 1, 1 },
        { "Taxi", "taxi", 145, 8, 4, 1, 1 },
        { "Washington", "washing", 153, 10, 4, 1, 1 },
        { "Bobcat", "bobcat", 140, 5, 2, 1, 2 },
        { "Mr. Whoopee", "mrwhoop", 99, 14, 2, 0, 2 },
        { "BF Injection", "bfinject", 136, 7, 2, 0, 2 },
        { "Hunter", "hunter", 204, 4, 1, 0, 0 },
        { "Premier", "premier", 173, 10, 4, 1, 2 },
        { "Enforcer", "enforcer", 165, 8, 4, 0, 2 },
        { "Securicar", "securica", 157, 14, 4, 0, 2 },
        { "Banshee", "banshee", 201, 11, 2, 1, 2 },
        { "Predator", "predator", 182, 2, 1, 0, 2 },
        { "Bus", "bus", 131, 8, 9, 0, 2 },
        { "Rhino", "rhino", 140, 8, 1, 0, 0 },
        { "Barracks", "barracks", 111, 8, 2, 0, 0 },
        { "Hotknife", "hotknife", 166, 14, 2, 0, 0 },
        { "Articulated Trailer 1", "artict1", 148, 13, 0, 0, 1 },
        { "Previon", "previon", 148, 10, 2, 1, 1 },
        { "Coach", "coach", 157, 8, 9, 0, 2 },
        { "Cabbie", "cabbie", 142, 8, 4, 1, 1 },
        { "Stallion", "stallion", 168, 3, 2, 1, 1 },
        { "Rumpo", "rumpo", 136, 5, 4, 0, 1 },
        { "RC Bandit", "rcbandit", 75, 9, 1, 0, 2 },
        { "Romero", "romero", 140, 14, 2, 1, 2 },
        { "Packer", "packer", 126, 5, 2, 0, 1 },
        { "Monster", "monster", 110, 7, 2, 0, 2 },
        { "Admiral", "admiral", 164, 10, 4, 1, 1 },
        { "Squalo", "squalo", 227, 2, 1, 0, 2 },
        { "Seasparrow", "seaspar", 186, 4, 2, 0, 0 },
        { "Pizzaboy", "pizzaboy", 111, 1, 1, 0, 2 },
        { "Tram", "tram", 180, 14, 2, 0, 1 },
        { "Articulated Trailer 2", "artict2", 148, 13, 0, 0, 1 },
        { "Turismo", "turismo", 192, 11, 2, 1, 1 },
        { "Speeder", "speeder", 211, 2, 1, 0, 2 },
        { "Reefer", "reefer", 129, 2, 1, 0, 2 },
        { "Tropic", "tropic", 169, 2, 1, 0, 2 },
        { "Flatbed", "flatbed", 157, 5, 2, 0, 1 },
        { "Yankee", "yankee", 107, 5, 2, 0, 2 },
        { "Caddy", "caddy", 96, 14, 2, 0, 1 },
        { "Solair", "solair", 157, 12, 4, 1, 1 },
        { "Berkley's RC Van", "topfun", 136, 5, 4, 0, 1 },
        { "Skimmer", "skimmer", 136, 0, 2, 0, 2 },
        { "PCJ-600", "pcj600", 161, 1, 2, 0, 1 },
        { "Faggio", "faggio", 111, 1, 2, 0, 2 },
        { "Freeway", "freeway", 144, 1, 2, 0, 2 },
        { "RC Baron", "rcbaron", 43, 9, 2, 0, 0 },
        { "RC Raider", "rcraider", 98, 9, 2, 0, 0 },
        { "Glendale", "glendale", 147, 10, 4, 1, 2 },
        { "Oceanic", "oceanic", 140, 10, 4, 1, 2 },
        { "Sanchez", "sanchez", 144, 1, 2, 0, 1 },
        { "Sparrow", "sparrow", 151, 4, 2, 0, 0 },
        { "Patriot", "patriot", 157, 7, 4, 0, 0 },
        { "Quad", "quad", 111, 1, 2, 0, 2 },
        { "Coastguard", "coastg", 178, 2, 1, 0, 2 },
        { "Dinghy", "dinghy", 159, 2, 1, 0, 2 },
        { "Hermes", "hermes", 148, 10, 2, 1, 2 },
        { "Sabre", "sabre", 171, 11, 2, 1, 1 },
        { "Rustler", "rustler", 270, 0, 1, 0, 2 },
        { "ZR-350", "zr350", 185, 11, 2, 1, 1 },
        { "Walton", "walton", 117, 5, 2, 1, 1 },
        { "Regina", "regina", 140, 12, 4, 1, 2 },
        { "Comet", "comet", 183, 3, 2, 1, 2 },
        { "BMX", "bmx", 73, 1, 1, 0, 1 },
        { "Burrito", "burrito", 157, 5, 4, 0, 1 },
        { "Camper", "camper", 123, 14, 3, 0, 2 },
        { "Marquis", "marquis", 113, 2, 1, 0, 2 },
        { "Baggage", "baggage", 99, 14, 1, 0, 0 },
        { "Dozer", "dozer", 65, 14, 1, 0, 0 },
        { "Maverick", "maverick", 222, 4, 4, 0, 2 },
        { "News Chopper", "vcnmav", 222, 4, 2, 0, 2 },
        { "Rancher", "rancher", 140, 7, 2, 1, 2 },
        { "FBI Rancher", "fbiranch", 157, 8, 4, 0, 2 },
        { "Virgo", "virgo", 148, 10, 2, 1, 2 },
        { "Greenwood", "greenwoo", 140, 10, 4, 1, 2 },
        { "Jetmax", "jetmax", 227, 2, 1, 0, 2 },
        { "Hotring Racer", "hotring", 213, 11, 2, 0, 2 },
        { "Sandking", "sandking", 177, 7, 2, 0, 2 },
        { "Blista Compact", "blistac", 163, 11, 2, 1, 2 },
        { "Police Maverick", "polmav", 222, 4, 4, 0, 2 },
        { "Boxville", "boxville", 107, 5, 4, 0, 2 },
        { "Benson", "benson", 122, 5, 2, 0, 2 },
        { "Mesa", "mesa", 140, 7, 2, 1, 1 },
        { "RC Goblin", "rcgoblin", 81, 9, 2, 0, 0 },
        { "Hotring Racer A", "hotrina", 213, 11, 2, 0, 2 },
        { "Hotring Racer B", "hotrinb", 213, 11, 2, 0, 2 },
        { "Bloodring Banger", "bloodra", 171, 10, 2, 0, 2 },
        { "Rancher (Lure)", "rnchlure", 140, 7, 2, 0, 2 },
        { "Super GT", "supergt", 178, 11, 2, 0, 1 },
        { "Elegant", "elegant", 165, 10, 4, 1, 1 },
        { "Journey", "journey", 107, 14, 2, 0, 1 },
        { "Bike", "bike", 79, 1, 1, 0, 1 },
        { "Mountain Bike", "mtbike", 102, 1, 1, 0, 1 },
        { "Beagle", "beagle", 143, 0, 2, 0, 2 },
        { "Cropduster", "cropdust", 133, 0, 1, 0, 2 },
        { "Stuntplane", "stunt", 159, 0, 1, 0, 2 },
        { "Tanker", "petro", 120, 5, 2, 0, 1 },
        { "Roadtrain", "rdtrain", 175, 5, 2, 0, 2 },
        { "Nebula", "nebula", 157, 10, 4, 1, 1 },
        { "Majestic", "majestic", 157, 10, 2, 1, 2 },
        { "Buccaneer", "buccanee", 163, 10, 2, 1, 1 },
        { "Shamal", "shamal", 270, 0, 1, 0, 0 },
        { "Hydra", "hydra", 270, 0, 1, 0, 0 },
        { "FCR-900", "fcr900", 161, 1, 2, 0, 2 },
        { "NRG-500", "nrg500", 176, 1, 2, 0, 2 },
        { "HPV1000", "copbike", 152, 8, 2, 0, 0 },
        { "Cement Truck", "cement", 157, 5, 2, 0, 2 },
        { "Towtruck", "towtruck", 157, 14, 2, 0, 2 },
        { "Fortune", "fortune", 157, 10, 2, 1, 1 },
        { "Cadrona", "cadrona", 149, 10, 2, 1, 1 },
        { "FBI Truck", "fbitruck", 175, 8, 2, 0, 0 },
        { "Willard", "willard", 148, 10, 4, 1, 1 },
        { "Forklift", "forklift", 60, 14, 1, 0, 1 },
        { "Tractor", "tractor", 70, 5, 1, 0, 1 },
        { "Combine Harvester", "combine", 111, 14, 1, 0, 0 },
        { "Feltzer", "feltzer", 166, 3, 2, 1, 2 },
        { "Remington", "remingtn", 168, 6, 2, 2, 2 },
        { "Slamvan", "slamvan", 157, 6, 2, 2, 1 },
        { "Blade", "blade", 171, 6, 2, 2, 2 },
        { "Freight", "freight", 180, 14, 2, 0, 0 },
        { "Brown Streak", "streak", 180, 14, 2, 0, 0 },
        { "Vortex", "vortex", 99, 14, 1, 0, 2 },
        { "Vincent", "vincent", 148, 10, 4, 1, 1 },
        { "Bullet", "bullet", 202, 11, 2, 1, 2 },
        { "Clover", "clover", 163, 10, 2, 1, 2 },
        { "Sadler", "sadler", 151, 5, 2, 0, 1 },
        { "Fire Truck (Ladder)", "firela", 148, 8, 2, 0, 1 },
        { "Hustler", "hustler", 147, 14, 2, 1, 1 },
        { "Intruder", "intruder", 148, 10, 4, 1, 1 },
        { "Primo", "primo", 142, 10, 4, 1, 1 },
        { "Cargobob", "cargobob", 151, 4, 2, 0, 0 },
        { "Tampa", "tampa", 153, 10, 2, 1, 2 },
        { "Sunrise", "sunrise", 144, 10, 4, 1, 1 },
        { "Merit", "merit", 157, 10, 4, 1, 1 },
        { "Utility Van", "utility", 121, 5, 2, 0, 2 },
        { "Nevada", "nevada", 243, 0, 1, 0, 2 },
        { "Yosemite", "yosemite", 143, 5, 2, 0, 2 },
        { "Windsor", "windsor", 157, 3, 2, 1, 1 },
        { "Monster A", "monstera", 110, 7, 2, 0, 0 },
        { "Monster B", "monsterb", 110, 7, 2, 0, 2 },
        { "Uranus", "uranus", 157, 11, 2, 3, 1 },
        { "Jester", "jester", 177, 11, 2, 3, 1 },
        { "Sultan", "sultan", 169, 10, 4, 3, 1 },
        { "Stratum", "stratum", 153, 12, 4, 3, 1 },
        { "Elegy", "elegy", 177, 10, 2, 3, 1 },
        { "Raindance", "raindanc", 151, 4, 2, 0, 2 },
        { "RC Tiger", "rctiger", 131, 9, 1, 0, 0 },
        { "Flash", "flash", 163, 11, 2, 3, 1 },
        { "Tahoma", "tahoma", 160, 6, 4, 2, 1 },
        { "Savanna", "savanna", 171, 6, 4, 2, 2 },
        { "Bandito", "bandito", 146, 7, 1, 0, 0 },
        { "Freight Flat", "freiflat", 180, 13, 2, 0, 0 },
        { "Streak Carriage", "streakc", 180, 13, 6, 0, 0 },
        { "Kart", "kart", 94, 14, 1, 0, 1 },
        { "Mower", "mower", 60, 14, 1, 0, 1 },
        { "Dune", "duneride", 110, 7, 2, 0, 2 },
        { "Sweeper", "sweeper", 60, 14, 1, 0, 1 },
        { "Broadway", "broadway", 157, 6, 2, 2, 2 },
        { "Tornado", "tornado", 157, 6, 2, 2, 2 },
        { "AT-400", "at400", 270, 0, 2, 0, 2 },
        { "DFT-30", "dft30", 157, 5, 2, 0, 1 },
        { "Huntley", "huntley", 157, 7, 4, 1, 1 },
        { "Stafford", "stafford", 153, 10, 4, 1, 1 },
        { "BF-400", "bf400", 152, 1, 2, 0, 1 },
        { "Newsvan", "newsvan", 136, 5, 4, 0, 2 },
        { "Tug", "tug", 86, 14, 1, 0, 2 },
        { "Tanker Trailer", "petrotr", 148, 13, 0, 0, 0 },
        { "Emperor", "emperor", 153, 10, 4, 1, 1 },
        { "Wayfarer", "wayfarer", 144, 1, 2, 0, 1 },
        { "Euros", "euros", 163, 11, 2, 1, 1 },
        { "Hotdog", "hotdog", 107, 14, 2, 0, 1 },
        { "Club", "club", 162, 11, 2, 1, 1 },
        { "Freight Box", "freibox", 180, 13, 0, 0, 0 },
        { "Articulated Trailer 3", "artict3", 148, 13, 0, 0, 1 },
        { "Andromada", "androm", 270, 0, 2, 0, 2 },
        { "Dodo", "dodo", 136, 0, 2, 0, 0 },
        { "RC Cam", "rccam", 60, 9, 2, 0, 0 },
        { "Launch", "launch", 173, 2, 1, 0, 2 },
        { "Police Premier (LSPD)", "copcarla", 175, 8, 4, 0, 2 },
        { "Police Premier (SFPD)", "copcarsf", 175, 8, 4, 0, 2 },
        { "Police Premier (LVPD)", "copcarvg", 175, 8, 4, 0, 2 },
        { "Police Ranger", "copcarru", 157, 8, 2, 0, 2 },
        { "Picador", "picador", 151, 5, 2, 1, 2 },
        { "S.W.A.T.", "swatvan", 110, 8, 2, 0, 0 },
        { "Alpha", "alpha", 168, 11, 2, 1, 1 },
        { "Phoenix", "phoenix", 170, 11, 2, 1, 2 },
        { "Glendale (Damaged)", "glenshit", 147, 10, 4, 0, 2 },
        { "Sadler (Damaged)", "sadlshit", 151, 5, 2, 0, 1 },
        { "Baggage Trailer (Covered)", "bagboxa", 99, 13, 0, 0, 0 },
        { "Baggage Trailer (Uncovered)", "bagboxb", 99, 13, 0, 0, 0 },
        { "Tug Stairs", "tugstair", 99, 13, 0, 0, 1 },
        { "Boxville (Mission)", "boxburg", 107, 5, 4, 0, 2 },
        { "Farm Trailer", "farmtr1", 99, 13, 0, 0, 0 },
        { "Street Cleaning Trailer", "utiltr1", 99, 13, 0, 0, 2 },
    } };

    constexpr bool IsValid(int modelId)
    {
        return modelId >= FirstId && modelId <= LastId;
    }

    constexpr const Entry* Get(int modelId)
    {
        return IsValid(modelId) ? &Entries[modelId - FirstId] : nullptr;
    }
}
