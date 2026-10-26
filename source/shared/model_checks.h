// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#include "NodeName.h"

namespace ModelChecks
{
    inline bool IsEscalatorStepModel(int id)
    {
        return id == 1656 || id == 1698;
    }

    inline bool IsEscalatorBaseModel(int id)
    {
        return id == 3586 || id == 3743 || id == 8872 || id == 8979;
    }

    inline bool IsGunflashAtomic(const RpAtomic* atomic)
    {
        if (!atomic)
        {
            return false;
        }
        RwFrame* frame = RpAtomicGetFrame(const_cast<RpAtomic*>(atomic));
        const char* name = frame ? GetFrameNodeName(frame) : nullptr;
        return name && _stricmp(name, "gunflash") == 0;
    }

    inline RpAtomic* HideGunflashAtomic(RpAtomic* atomic, void*)
    {
        if (IsGunflashAtomic(atomic))
        {
            RpAtomicSetFlags(atomic, 0);
        }
        return atomic;
    }

    inline void HideGunflash(RwObject* object)
    {
        if (!object)
        {
            return;
        }
        const RwUInt8 type = RwObjectGetType(object);
        if (type == rpATOMIC)
        {
            HideGunflashAtomic(reinterpret_cast<RpAtomic*>(object), nullptr);
        }
        else if (type == rpCLUMP)
        {
            RpClumpForAllAtomics(reinterpret_cast<RpClump*>(object), HideGunflashAtomic, nullptr);
        }
    }

    inline bool IsVehicleLodAtomic(const RpAtomic* atomic)
    {
        if (!atomic)
        {
            return false;
        }
        RwFrame* frame = RpAtomicGetFrame(const_cast<RpAtomic*>(atomic));
        const char* name = frame ? GetFrameNodeName(frame) : nullptr;
        return name && strstr(name, "_vlo") != nullptr;
    }

    inline RpAtomic* HideVehicleLodAtomic(RpAtomic* atomic, void*)
    {
        if (IsVehicleLodAtomic(atomic))
        {
            RpAtomicSetFlags(atomic, 0);
        }
        return atomic;
    }

    inline void HideVehicleLod(RwObject* object)
    {
        if (!object)
        {
            return;
        }
        const RwUInt8 type = RwObjectGetType(object);
        if (type == rpATOMIC)
        {
            HideVehicleLodAtomic(reinterpret_cast<RpAtomic*>(object), nullptr);
        }
        else if (type == rpCLUMP)
        {
            RpClumpForAllAtomics(reinterpret_cast<RpClump*>(object), HideVehicleLodAtomic, nullptr);
        }
    }

    inline bool IsBreakable(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 625, 626, 627, 628, 629, 630, 631, 632, 633, 642, 643, 644, 646, 650, 716,
            717, 737, 738, 792, 858, 881, 882, 883, 884, 885, 886, 887, 888, 889, 890, 891, 892, 893, 894, 895, 904, 905, 941,
            955, 956, 959, 961, 990, 993, 996, 1209, 1211, 1213, 1219, 1220, 1221, 1223, 1224, 1225, 1226, 1227, 1228, 1229, 1230,
            1231, 1232, 1235, 1238, 1244, 1251, 1255, 1257, 1262, 1264, 1265, 1270, 1280, 1281, 1282, 1283, 1284, 1285, 1286,
            1287, 1288, 1289, 1290, 1291, 1293, 1294, 1297, 1300, 1302, 1315, 1328, 1329, 1330, 1338, 1350, 1351, 1352, 1370,
            1373, 1374, 1375, 1407, 1408, 1409, 1410, 1411, 1412, 1413, 1414, 1415, 1417, 1418, 1419, 1420, 1421, 1422, 1423,
            1424, 1425, 1426, 1428, 1429, 1431, 1432, 1433, 1436, 1437, 1438, 1440, 1441, 1443, 1444, 1445, 1446, 1447, 1448,
            1449, 1450, 1451, 1452, 1456, 1457, 1458, 1459, 1460, 1461, 1462, 1463, 1464, 1465, 1466, 1467, 1468, 1469, 1470,
            1471, 1472, 1473, 1474, 1475, 1476, 1477, 1478, 1479, 1480, 1481, 1482, 1483, 1514, 1517, 1520, 1534, 1543, 1544,
            1545, 1551, 1553, 1554, 1558, 1564, 1568, 1582, 1583, 1584, 1588, 1589, 1590, 1591, 1592, 1645, 1646, 1647, 1649,
            1654, 1664, 1666, 1667, 1668, 1669, 1670, 1672, 1676, 1684, 1686, 1775, 1776, 1949, 1950, 1951, 1960, 1961, 1962,
            1975, 1976, 1977, 2647, 2663, 2682, 2683, 2885, 2886, 2887, 2900, 2918, 2920, 2925, 2932, 2933, 2942, 2943, 2945,
            2947, 2958, 2959, 2966, 2968, 2971, 2977, 2987, 2988, 2989, 2991, 2994, 3006, 3018, 3019, 3020, 3021, 3022, 3023,
            3024, 3029, 3032, 3036, 3058, 3059, 3067, 3083, 3091, 3221, 3260, 3261, 3262, 3263, 3264, 3265, 3267, 3275, 3276,
            3278, 3280, 3281, 3282, 3302, 3374, 3409, 3460, 3516, 3794, 3795, 3797, 3851, 3853, 3855, 3857, 3858, 3859, 3864,
            3872, 3884, 11103, 12840, 16627, 16628, 16629, 16630, 16631, 16632, 16633, 16634, 16635, 16636, 16732, 17968 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "IsBreakable: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool IsPhysics(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 945, 1214, 1215, 1217, 1218, 1219, 1220, 1221, 1222, 1224, 1225, 1227, 1228,
            1230, 1233, 1234, 1235, 1238, 1243, 1252, 1258, 1264, 1265, 1269, 1270, 1271, 1280, 1282, 1291, 1293, 1299, 1300,
            1303, 1304, 1305, 1319, 1320, 1321, 1322, 1323, 1324, 1328, 1329, 1330, 1331, 1332, 1333, 1334, 1340, 1341, 1342,
            1343, 1344, 1370, 1372, 1375, 1430, 1434, 1435, 1491, 1492, 1494, 1499, 1502, 1523, 1549, 1558, 1563, 1598, 1610,
            1636, 1651, 1950, 1951, 1985, 2589, 2601, 2891, 2901, 2903, 2912, 2915, 2916, 2919, 2936, 2937, 2984, 2995, 2996,
            2997, 2998, 2999, 3000, 3001, 3002, 3003, 3007, 3008, 3009, 3010, 3011, 3012, 3013, 3014, 3031, 3034, 3046, 3052,
            3053, 3054, 3056, 3057, 3065, 3092, 3100, 3101, 3102, 3103, 3104, 3105, 3106, 3120, 3131, 3379, 3380, 3407, 3408,
            3593, 3594, 3794, 3795, 3797, 16368 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "IsPhysics: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool IsExplosive(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 1225, 1244, 1370, 1676, 1686, 3267, 3794, 3795, 3797, 3884 };
        static_assert(sizeof(models) / sizeof(models[0]) == 10, "The verified Explosive model set changed unexpectedly");
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "IsExplosive: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool IsUVAnim(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 5990, 5992, 7220, 7264, 7268, 7290, 7313, 7916, 8371, 9831, 14561, 14563,
            14564, 14570, 14571, 14605, 14606, 14607, 14614, 14615, 14626, 14627, 14631, 14640, 18642, 18752, 18779, 18827, 18844,
            18863, 18864, 18982, 18983, 18984, 18985, 19005, 19070, 19071, 19072, 19073, 19113, 19128, 19129, 19130, 19135, 19159,
            19197, 19527, 19604, 19620, 19797, 19840, 19841, 19842, 19884, 19885, 19886, 19887, 19888, 19889 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "IsExplosive: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool HasLightsFx(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 981, 1213, 1215, 1223, 1226, 1231, 1232, 1243, 1252, 1259, 1260, 1262, 1267,
            1278, 1283, 1284, 1290, 1294, 1295, 1297, 1315, 1350, 1351, 1352, 1373, 1378, 1384, 1388, 1394, 1568, 1662, 1731,
            1734, 1893, 2026, 2069, 2070, 2071, 2072, 2073, 2074, 2076, 2077, 2105, 2106, 2107, 2108, 2239, 3091, 3096, 3108,
            3110, 3112, 3256, 3257, 3258, 3383, 3384, 3385, 3398, 3448, 3460, 3463, 3472, 3516, 3518, 3519, 3526, 3528, 3534,
            3666, 3673, 3754, 3785, 3818, 3853, 3855, 3856, 3876, 3877, 3879, 3971, 3979, 3981, 4030, 4600, 4602, 4720, 4721,
            4722, 4723, 4725, 4726, 4865, 4866, 4867, 4874, 4990, 5002, 5003, 5040, 5457, 5467, 5475, 5732, 5767, 6391, 6932,
            6933, 6934, 6973, 6988, 7093, 7220, 7264, 7291, 7315, 7344, 7347, 7419, 7531, 7539, 7540, 7585, 7596, 7617, 7755,
            8486, 8497, 8844, 9101, 9104, 9277, 9278, 9279, 9280, 9281, 9282, 9283, 9933, 10027, 10029, 10056, 10229, 10250,
            10308, 10397, 10430, 10763, 10764, 10767, 10768, 10769, 10821, 10822, 10871, 10948, 11150, 11237, 11252, 11256, 11258,
            11260, 11388, 11390, 11395, 11701, 11726, 11727, 13612, 13624, 13664, 14383, 14388, 14389, 14390, 14393, 14395, 14404,
            14406, 14408, 14412, 14415, 14417, 14425, 14444, 14470, 14471, 14476, 14479, 14485, 14495, 14502, 14503, 14504, 14505,
            14506, 14507, 14508, 14509, 14510, 14531, 14533, 14534, 14536, 14539, 14547, 14559, 14563, 14581, 14587, 14590, 14591,
            14606, 14608, 14614, 14622, 14624, 14625, 14628, 14629, 14664, 14665, 14667, 14668, 14669, 14672, 14682, 14689, 14700,
            14701, 14702, 14703, 14706, 14707, 14708, 14709, 14710, 14711, 14712, 14713, 14714, 14717, 14718, 14735, 14746, 14748,
            14750, 14754, 14755, 14756, 14758, 14759, 14760, 14777, 14785, 14808, 14814, 14821, 14838, 14849, 14881, 14896, 14897,
            15025, 15026, 15041, 15042, 15046, 15048, 15049, 15053, 15054, 15055, 15058, 16150, 16335, 17578, 17616, 18007, 18018,
            18019, 18020, 18021, 18022, 18023, 18025, 18027, 18028, 18031, 18033, 18034, 18038, 18045, 18049, 18056, 18065, 18646,
            18647, 18648, 18649, 18650, 18651, 18652, 18653, 18654, 18655, 18656, 18657, 18658, 18761, 18778, 18850, 18876, 18881,
            19004, 19076, 19121, 19122, 19123, 19124, 19125, 19126, 19127, 19143, 19144, 19145, 19146, 19147, 19148, 19149, 19150,
            19151, 19152, 19153, 19154, 19155, 19156, 19159, 19271, 19278, 19279, 19280, 19281, 19282, 19283, 19284, 19285, 19286,
            19287, 19288, 19289, 19290, 19291, 19292, 19293, 19294, 19295, 19296, 19297, 19298, 19299, 19419, 19608, 19672, 19718,
            19759, 19803, 19806, 19895 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "IsUVAnim: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool HasParticlesFx(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 1227, 1236, 1264, 1265, 1438, 1439, 1440, 1441, 1442, 1485, 1617, 1618, 1635,
            1665, 1689, 1690, 1691, 2415, 2718, 2780, 3044, 3256, 3258, 3461, 3515, 3524, 3525, 3528, 3673, 5408, 6932, 6933,
            6934, 6962, 6965, 7291, 7916, 9831, 9833, 14573, 14889, 16135, 17017, 18668, 18669, 18670, 18671, 18672, 18673, 18674,
            18675, 18676, 18677, 18678, 18679, 18680, 18681, 18682, 18683, 18684, 18685, 18686, 18687, 18688, 18689, 18690, 18691,
            18692, 18693, 18694, 18695, 18696, 18697, 18698, 18699, 18700, 18701, 18702, 18703, 18704, 18705, 18706, 18707, 18708,
            18709, 18710, 18711, 18712, 18713, 18714, 18715, 18716, 18717, 18718, 18719, 18720, 18721, 18722, 18723, 18724, 18725,
            18726, 18727, 18728, 18729, 18730, 18731, 18732, 18733, 18734, 18735, 18736, 18737, 18738, 18739, 18740, 18741, 18742,
            18743, 18744, 18745, 18746, 18747, 18748, 18760, 18858, 18862, 19002, 19270, 19625, 19632, 19840, 19841 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "HasLightsFx: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool IsTrafficLight(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 1262, 1283, 1284, 1315, 1350, 1351, 1352, 3516, 3855 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "HasParticlesFx: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline bool TrafficLensIsSmall(unsigned short modelId)
    {
        static constexpr unsigned short models[] = { 1283, 1350, 1351, 1352, 3855 };
        static_assert(std::is_sorted(std::begin(models), std::end(models)),
            "IsTrafficLight: the model id table must stay sorted - binary search fails silently otherwise");
        return std::binary_search(std::begin(models), std::end(models), modelId);
    }

    inline constexpr unsigned short UnsafeObjectModelIds[] = { 1373, 3118, 3552, 3553 };
    static_assert(std::is_sorted(std::begin(UnsafeObjectModelIds), std::end(UnsafeObjectModelIds)),
        "kUnsafeObjectModelIds must stay sorted - binary search fails silently otherwise");

    inline bool IsUnsafeObjectModel(unsigned short modelId)
    {
        return std::binary_search(std::begin(UnsafeObjectModelIds), std::end(UnsafeObjectModelIds), modelId);
    }
}
