#if defined _waversomapstudio_races_included
    #endinput
#endif
#define _waversomapstudio_races_included

#if !defined _samp_included && !defined _INC_open_mp
    #error Include a_samp or open.mp before waversomapstudio_races.inc.
#endif

#define MAPRACE_VERSION (10000)
#define MAPRACE_MAX_NAME (48)
#define MAPRACE_INVALID_ID (-1)
#define MAPRACE_POINT_TO_POINT (0)
#define MAPRACE_CIRCUIT (1)
#define MAPRACE_AUTO_GROUND (9)
#define MAPRACE_AUTO_AIR (10)
#define MAPRACE_MIN_RADIUS (0.25)
#define MAPRACE_MAX_RADIUS (1000.0)

#define MAPRACE_STOP_CANCELLED (0)
#define MAPRACE_STOP_REPLACED (1)
#define MAPRACE_STOP_DEATH (2)
#define MAPRACE_STOP_DISCONNECTED (3)
#define MAPRACE_STOP_DIMENSION (4)
#define MAPRACE_STOP_RESOURCE (5)
#define MAPRACE_STOP_SHUTDOWN (6)

#if !defined MAPRACE_MAX_RACES
    #define MAPRACE_MAX_RACES (1024)
#endif
#if !defined MAPRACE_MAX_CHECKPOINTS
    #define MAPRACE_MAX_CHECKPOINTS (2048)
#endif
#if !defined MAPRACE_ICON_BASE
    #define MAPRACE_ICON_BASE (95)
#endif
#if !defined MAPRACE_STREAMER_PRIORITY
    #define MAPRACE_STREAMER_PRIORITY (1000)
#endif

#if MAPRACE_MAX_RACES < 1 || MAPRACE_MAX_CHECKPOINTS < 1
    #error MapRace capacities must be positive.
#endif
#if !defined _streamer_included
    #if MAPRACE_ICON_BASE < 0 || MAPRACE_ICON_BASE > 95
        #error MAPRACE_ICON_BASE must be between 0 and 95.
    #endif
#endif

forward OnMapRaceStart(playerid, raceid);
forward OnMapRaceCheckpoint(playerid, raceid, checkpoint, lap);
forward OnMapRaceLap(playerid, raceid, lap, milliseconds);
forward OnMapRaceFinish(playerid, raceid, milliseconds);
forward OnMapRaceStop(playerid, raceid, reason);

enum _MR_RACE {
    _MR_ID,
    _MR_NAME[MAPRACE_MAX_NAME + 1],
    _MR_MODE,
    _MR_LAPS,
    _MR_WORLD,
    _MR_INTERIOR,
    _MR_FIRST,
    _MR_LAST,
    _MR_COUNT,
    _MR_PLAYERS
};

enum _MR_CHECKPOINT {
    Float:_MR_X,
    Float:_MR_Y,
    Float:_MR_Z,
    Float:_MR_RADIUS,
    _MR_TYPE,
    _MR_NEXT,
    _MR_PREVIOUS,
    _MR_NUMBER
};

enum _MR_PLAYER {
    bool:_MR_ACTIVE,
    _MR_RACE_INDEX,
    _MR_TARGET,
    _MR_LAP,
    bool:_MR_STARTED,
    _MR_GENERATION,
    _MR_START_TICK,
    _MR_LAP_TICK,
    bool:_MR_ENTERING,
    bool:_MR_HAS_CP,
    _MR_CP_HANDLE,
    _MR_ICON_COUNT,
    _MR_ICONS[5]
};

static _mr_Races[MAPRACE_MAX_RACES][_MR_RACE];
static _mr_Checkpoints[MAPRACE_MAX_CHECKPOINTS][_MR_CHECKPOINT];
static _mr_Players[MAX_PLAYERS][_MR_PLAYER];
static _mr_RaceCount;
static _mr_CheckpointCount;
static const _mr_Alpha[] = {255, 40, 30, 20, 10};

stock bool:_mr_ValidPlayer(playerid) {
    return playerid >= 0 && playerid < MAX_PLAYERS;
}

stock bool:_mr_Finite(Float:value) {
    return (_:value & 0x7F800000) != 0x7F800000;
}

stock _mr_FindRace(raceid) {
    for (new i = 0; i < _mr_RaceCount; ++i) {
        if (_mr_Races[i][_MR_ID] == raceid)
            return i;
    }
    return -1;
}

stock bool:MapRace_Create(raceid, const name[], mode = MAPRACE_POINT_TO_POINT, laps = 1,
                         world = -1, interior = -1) {
    if (raceid < 0 || _mr_FindRace(raceid) != -1 || _mr_RaceCount >= MAPRACE_MAX_RACES ||
        !name[0] || strlen(name) > MAPRACE_MAX_NAME || laps < 1 || laps > 999 ||
        (mode != MAPRACE_POINT_TO_POINT && mode != MAPRACE_CIRCUIT) ||
        world < -1 || interior < -1 || interior > 255)
        return false;

    new index = _mr_RaceCount++;
    _mr_Races[index][_MR_ID] = raceid;
    format(_mr_Races[index][_MR_NAME], MAPRACE_MAX_NAME + 1, "%s", name);
    _mr_Races[index][_MR_MODE] = mode;
    _mr_Races[index][_MR_LAPS] = mode == MAPRACE_CIRCUIT ? laps : 1;
    _mr_Races[index][_MR_WORLD] = world;
    _mr_Races[index][_MR_INTERIOR] = interior;
    _mr_Races[index][_MR_FIRST] = -1;
    _mr_Races[index][_MR_LAST] = -1;
    return true;
}

stock bool:MapRace_AddCheckpoint(raceid, Float:x, Float:y, Float:z, Float:radius = 5.0,
                                type = MAPRACE_AUTO_GROUND) {
    new index = _mr_FindRace(raceid);
    if (index == -1 || _mr_Races[index][_MR_PLAYERS] ||
        _mr_CheckpointCount >= MAPRACE_MAX_CHECKPOINTS || type < 0 || type > MAPRACE_AUTO_AIR ||
        !_mr_Finite(x) || !_mr_Finite(y) || !_mr_Finite(z) || !_mr_Finite(radius) ||
        radius < MAPRACE_MIN_RADIUS || radius > MAPRACE_MAX_RADIUS)
        return false;

    new checkpoint = _mr_CheckpointCount++;
    _mr_Checkpoints[checkpoint][_MR_X] = x;
    _mr_Checkpoints[checkpoint][_MR_Y] = y;
    _mr_Checkpoints[checkpoint][_MR_Z] = z;
    _mr_Checkpoints[checkpoint][_MR_RADIUS] = radius;
    _mr_Checkpoints[checkpoint][_MR_TYPE] = type;
    _mr_Checkpoints[checkpoint][_MR_NEXT] = -1;
    _mr_Checkpoints[checkpoint][_MR_PREVIOUS] = _mr_Races[index][_MR_LAST];
    _mr_Checkpoints[checkpoint][_MR_NUMBER] = ++_mr_Races[index][_MR_COUNT];
    if (_mr_Races[index][_MR_LAST] != -1)
        _mr_Checkpoints[_mr_Races[index][_MR_LAST]][_MR_NEXT] = checkpoint;
    else
        _mr_Races[index][_MR_FIRST] = checkpoint;
    _mr_Races[index][_MR_LAST] = checkpoint;
    return true;
}

stock bool:MapRace_GetName(raceid, output[], size = sizeof output) {
    new index = _mr_FindRace(raceid);
    if (index == -1 || size < 1)
        return false;
    format(output, size, "%s", _mr_Races[index][_MR_NAME]);
    return true;
}

stock MapRace_GetCheckpointCount(raceid) {
    new index = _mr_FindRace(raceid);
    return index == -1 ? 0 : _mr_Races[index][_MR_COUNT];
}

stock bool:MapRace_GetCheckpoint(raceid, number, &Float:x, &Float:y, &Float:z,
                                &Float:radius, &type) {
    new index = _mr_FindRace(raceid);
    if (index == -1 || number < 1 || number > _mr_Races[index][_MR_COUNT])
        return false;
    new checkpoint = _mr_Races[index][_MR_FIRST];
    for (new i = 1; i < number; ++i)
        checkpoint = _mr_Checkpoints[checkpoint][_MR_NEXT];
    x = _mr_Checkpoints[checkpoint][_MR_X];
    y = _mr_Checkpoints[checkpoint][_MR_Y];
    z = _mr_Checkpoints[checkpoint][_MR_Z];
    radius = _mr_Checkpoints[checkpoint][_MR_RADIUS];
    type = _mr_Checkpoints[checkpoint][_MR_TYPE];
    return true;
}

stock MapRace_GetPlayerRace(playerid) {
    if (!_mr_ValidPlayer(playerid) || !_mr_Players[playerid][_MR_ACTIVE])
        return MAPRACE_INVALID_ID;
    return _mr_Races[_mr_Players[playerid][_MR_RACE_INDEX]][_MR_ID];
}

stock MapRace_GetPlayerCheckpoint(playerid) {
    if (!_mr_ValidPlayer(playerid) || !_mr_Players[playerid][_MR_ACTIVE])
        return 0;
    return _mr_Checkpoints[_mr_Players[playerid][_MR_TARGET]][_MR_NUMBER];
}

stock MapRace_GetPlayerLap(playerid) {
    return _mr_ValidPlayer(playerid) && _mr_Players[playerid][_MR_ACTIVE]
        ? _mr_Players[playerid][_MR_LAP] : 0;
}

stock MapRace_GetPlayerElapsed(playerid) {
    return _mr_ValidPlayer(playerid) && _mr_Players[playerid][_MR_ACTIVE]
        ? GetTickCount() - _mr_Players[playerid][_MR_START_TICK] : 0;
}

stock bool:_mr_InDimension(playerid, index) {
    return (_mr_Races[index][_MR_WORLD] == -1 ||
            _mr_Races[index][_MR_WORLD] == GetPlayerVirtualWorld(playerid)) &&
           (_mr_Races[index][_MR_INTERIOR] == -1 ||
            _mr_Races[index][_MR_INTERIOR] == GetPlayerInterior(playerid));
}

stock bool:_mr_FinalTarget(playerid) {
    new index = _mr_Players[playerid][_MR_RACE_INDEX];
    new target = _mr_Players[playerid][_MR_TARGET];
    if (_mr_Races[index][_MR_MODE] == MAPRACE_POINT_TO_POINT)
        return target == _mr_Races[index][_MR_LAST];
    return target == _mr_Races[index][_MR_FIRST] && _mr_Players[playerid][_MR_STARTED] &&
           _mr_Players[playerid][_MR_LAP] == _mr_Races[index][_MR_LAPS];
}

stock _mr_TargetType(playerid) {
    new type = _mr_Checkpoints[_mr_Players[playerid][_MR_TARGET]][_MR_TYPE];
    if (type == MAPRACE_AUTO_GROUND)
        return _mr_FinalTarget(playerid) ? 1 : 0;
    if (type == MAPRACE_AUTO_AIR)
        return _mr_FinalTarget(playerid) ? 4 : 3;
    return type;
}

stock _mr_ClearVisuals(playerid) {
    if (_mr_Players[playerid][_MR_HAS_CP]) {
        #if defined _streamer_included
            DestroyDynamicRaceCP(STREAMER_TAG_RACE_CP:_mr_Players[playerid][_MR_CP_HANDLE]);
        #else
            DisablePlayerRaceCheckpoint(playerid);
        #endif
        _mr_Players[playerid][_MR_HAS_CP] = false;
        _mr_Players[playerid][_MR_CP_HANDLE] = 0;
    }
    for (new i = 0; i < _mr_Players[playerid][_MR_ICON_COUNT]; ++i) {
        #if defined _streamer_included
            DestroyDynamicMapIcon(STREAMER_TAG_MAP_ICON:_mr_Players[playerid][_MR_ICONS][i]);
        #else
            RemovePlayerMapIcon(playerid, _mr_Players[playerid][_MR_ICONS][i]);
        #endif
    }
    _mr_Players[playerid][_MR_ICON_COUNT] = 0;
}

stock _mr_Release(playerid) {
    _mr_ClearVisuals(playerid);
    --_mr_Races[_mr_Players[playerid][_MR_RACE_INDEX]][_MR_PLAYERS];
    _mr_Players[playerid][_MR_ACTIVE] = false;
    ++_mr_Players[playerid][_MR_GENERATION];
}

stock bool:StopMapRace(playerid, reason = MAPRACE_STOP_CANCELLED) {
    if (!_mr_ValidPlayer(playerid) || !_mr_Players[playerid][_MR_ACTIVE])
        return false;
    new raceid = MapRace_GetPlayerRace(playerid);
    _mr_Release(playerid);
    CallLocalFunction("OnMapRaceStop", "iii", playerid, raceid, reason);
    return true;
}

stock bool:_mr_CreateIcon(playerid, checkpoint, slot) {
    new color = 0xFF000000 | _mr_Alpha[slot];
    #if defined _streamer_included
        new index = _mr_Players[playerid][_MR_RACE_INDEX];
        new handle = _:CreateDynamicMapIcon(
            _mr_Checkpoints[checkpoint][_MR_X], _mr_Checkpoints[checkpoint][_MR_Y],
            _mr_Checkpoints[checkpoint][_MR_Z], 0, color, _mr_Races[index][_MR_WORLD],
            _mr_Races[index][_MR_INTERIOR], playerid, -1.0, _:MAPICON_GLOBAL,
            STREAMER_TAG_AREA:-1, MAPRACE_STREAMER_PRIORITY);
        if (handle == 0)
            return false;
    #else
        new handle = MAPRACE_ICON_BASE + slot;
        if (!SetPlayerMapIcon(playerid, handle, _mr_Checkpoints[checkpoint][_MR_X],
                             _mr_Checkpoints[checkpoint][_MR_Y],
                             _mr_Checkpoints[checkpoint][_MR_Z], 0, color, MAPICON_GLOBAL))
            return false;
    #endif
    _mr_Players[playerid][_MR_ICONS][_mr_Players[playerid][_MR_ICON_COUNT]++] = handle;
    return true;
}

stock bool:_mr_ShowIcons(playerid) {
    new index = _mr_Players[playerid][_MR_RACE_INDEX];
    new checkpoint = _mr_Players[playerid][_MR_TARGET];
    new lap = _mr_Players[playerid][_MR_LAP];
    new bool:started = _mr_Players[playerid][_MR_STARTED];
    new shown[5], count;
    for (new step = 0; step < 5; ++step) {
        new bool:duplicate;
        for (new i = 0; i < count; ++i) {
            if (shown[i] == checkpoint) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            if (!_mr_CreateIcon(playerid, checkpoint, step))
                return false;
            shown[count++] = checkpoint;
        }
        if (_mr_Races[index][_MR_MODE] == MAPRACE_POINT_TO_POINT) {
            checkpoint = _mr_Checkpoints[checkpoint][_MR_NEXT];
            if (checkpoint == -1)
                break;
        } else {
            if (checkpoint == _mr_Races[index][_MR_FIRST]) {
                if (started && lap++ >= _mr_Races[index][_MR_LAPS])
                    break;
                started = true;
            }
            checkpoint = _mr_Checkpoints[checkpoint][_MR_NEXT];
            if (checkpoint == -1)
                checkpoint = _mr_Races[index][_MR_FIRST];
        }
    }
    return true;
}

stock bool:_mr_ShowTarget(playerid) {
    _mr_ClearVisuals(playerid);
    new index = _mr_Players[playerid][_MR_RACE_INDEX];
    new target = _mr_Players[playerid][_MR_TARGET];
    new next = _mr_Checkpoints[target][_MR_NEXT];
    if (_mr_Races[index][_MR_MODE] == MAPRACE_CIRCUIT && next == -1)
        next = _mr_Races[index][_MR_FIRST];
    if (_mr_FinalTarget(playerid))
        next = -1;

    new Float:nx, Float:ny, Float:nz;
    if (next != -1) {
        nx = _mr_Checkpoints[next][_MR_X];
        ny = _mr_Checkpoints[next][_MR_Y];
        nz = _mr_Checkpoints[next][_MR_Z];
    } else {
        new previous = _mr_Checkpoints[target][_MR_PREVIOUS];
        if (_mr_Races[index][_MR_MODE] == MAPRACE_CIRCUIT &&
            target == _mr_Races[index][_MR_FIRST])
            previous = _mr_Races[index][_MR_LAST];
        if (previous != -1) {
            nx = _mr_Checkpoints[target][_MR_X] * 2.0 - _mr_Checkpoints[previous][_MR_X];
            ny = _mr_Checkpoints[target][_MR_Y] * 2.0 - _mr_Checkpoints[previous][_MR_Y];
            nz = _mr_Checkpoints[target][_MR_Z] * 2.0 - _mr_Checkpoints[previous][_MR_Z];
        } else {
            nx = _mr_Checkpoints[target][_MR_X];
            ny = _mr_Checkpoints[target][_MR_Y] + 1.0;
            nz = _mr_Checkpoints[target][_MR_Z];
        }
    }
    if (!_mr_Finite(nx) || !_mr_Finite(ny) || !_mr_Finite(nz) ||
        (nx == _mr_Checkpoints[target][_MR_X] && ny == _mr_Checkpoints[target][_MR_Y] &&
         nz == _mr_Checkpoints[target][_MR_Z])) {
        nx = _mr_Checkpoints[target][_MR_X];
        ny = _mr_Checkpoints[target][_MR_Y] + 1.0;
        nz = _mr_Checkpoints[target][_MR_Z];
        if (ny == _mr_Checkpoints[target][_MR_Y])
            return false;
    }

    #if defined _streamer_included
        new handle = _:CreateDynamicRaceCP(
            _mr_TargetType(playerid), _mr_Checkpoints[target][_MR_X],
            _mr_Checkpoints[target][_MR_Y], _mr_Checkpoints[target][_MR_Z], nx, ny, nz,
            _mr_Checkpoints[target][_MR_RADIUS], _mr_Races[index][_MR_WORLD],
            _mr_Races[index][_MR_INTERIOR], playerid, -1.0, STREAMER_TAG_AREA:-1,
            MAPRACE_STREAMER_PRIORITY);
        if (handle == 0)
            return false;
        _mr_Players[playerid][_MR_CP_HANDLE] = handle;
    #else
        DisablePlayerRaceCheckpoint(playerid);
        #if defined _INC_open_mp
            new CP_TYPE:type = CP_TYPE:_mr_TargetType(playerid);
        #else
            new type = _mr_TargetType(playerid);
        #endif
        if (!SetPlayerRaceCheckpoint(playerid, type, _mr_Checkpoints[target][_MR_X],
                                     _mr_Checkpoints[target][_MR_Y],
                                     _mr_Checkpoints[target][_MR_Z], nx, ny, nz,
                                     _mr_Checkpoints[target][_MR_RADIUS]))
            return false;
    #endif
    _mr_Players[playerid][_MR_HAS_CP] = true;
    if (!_mr_ShowIcons(playerid))
        return false;
    #if defined _streamer_included
        Streamer_Update(playerid, STREAMER_TYPE_RACE_CP);
        Streamer_Update(playerid, STREAMER_TYPE_MAP_ICON);
    #endif
    return true;
}

stock bool:StartMapRace(playerid, raceid) {
    new index = _mr_FindRace(raceid);
    if (!_mr_ValidPlayer(playerid) || !IsPlayerConnected(playerid) || index == -1 ||
        !_mr_Races[index][_MR_COUNT] || !_mr_InDimension(playerid, index) ||
        (_mr_Races[index][_MR_MODE] == MAPRACE_CIRCUIT && _mr_Races[index][_MR_COUNT] < 2))
        return false;

    if (_mr_Players[playerid][_MR_ACTIVE]) {
        new generation = _mr_Players[playerid][_MR_GENERATION];
        StopMapRace(playerid, MAPRACE_STOP_REPLACED);
        if (_mr_Players[playerid][_MR_GENERATION] != generation + 1)
            return false;
        index = _mr_FindRace(raceid);
        if (index == -1 || !_mr_Races[index][_MR_COUNT] || !_mr_InDimension(playerid, index) ||
            (_mr_Races[index][_MR_MODE] == MAPRACE_CIRCUIT && _mr_Races[index][_MR_COUNT] < 2))
            return false;
    }
    _mr_Players[playerid][_MR_ACTIVE] = true;
    _mr_Players[playerid][_MR_RACE_INDEX] = index;
    _mr_Players[playerid][_MR_TARGET] = _mr_Races[index][_MR_FIRST];
    _mr_Players[playerid][_MR_LAP] = 1;
    _mr_Players[playerid][_MR_STARTED] = false;
    _mr_Players[playerid][_MR_ENTERING] = false;
    _mr_Players[playerid][_MR_START_TICK] = GetTickCount();
    _mr_Players[playerid][_MR_LAP_TICK] = _mr_Players[playerid][_MR_START_TICK];
    ++_mr_Players[playerid][_MR_GENERATION];
    ++_mr_Races[index][_MR_PLAYERS];
    if (!_mr_ShowTarget(playerid)) {
        _mr_Release(playerid);
        return false;
    }
    CallLocalFunction("OnMapRaceStart", "ii", playerid, raceid);
    return true;
}

stock bool:_mr_Enter(playerid) {
    if (!_mr_ValidPlayer(playerid) || !_mr_Players[playerid][_MR_ACTIVE] ||
        _mr_Players[playerid][_MR_ENTERING])
        return false;
    new index = _mr_Players[playerid][_MR_RACE_INDEX];
    if (!_mr_InDimension(playerid, index)) {
        StopMapRace(playerid, MAPRACE_STOP_DIMENSION);
        return true;
    }
    new target = _mr_Players[playerid][_MR_TARGET];
    new Float:x, Float:y, Float:z;
    if (!GetPlayerPos(playerid, x, y, z))
        return false;
    x -= _mr_Checkpoints[target][_MR_X];
    y -= _mr_Checkpoints[target][_MR_Y];
    z -= _mr_Checkpoints[target][_MR_Z];
    new Float:radius = _mr_Checkpoints[target][_MR_RADIUS];
    if (!_mr_Finite(x) || !_mr_Finite(y) || !_mr_Finite(z) ||
        x * x + y * y + z * z > radius * radius)
        return false;

    new generation = _mr_Players[playerid][_MR_GENERATION];
    new raceid = _mr_Races[index][_MR_ID];
    new lap = _mr_Players[playerid][_MR_LAP];
    new now = GetTickCount();
    _mr_Players[playerid][_MR_ENTERING] = true;
    CallLocalFunction("OnMapRaceCheckpoint", "iiii", playerid, raceid,
                      _mr_Checkpoints[target][_MR_NUMBER], lap);
    if (_mr_Players[playerid][_MR_GENERATION] != generation)
        return true;

    new bool:finished = _mr_FinalTarget(playerid);
    if (_mr_Races[index][_MR_MODE] == MAPRACE_CIRCUIT &&
        target == _mr_Races[index][_MR_FIRST]) {
        if (_mr_Players[playerid][_MR_STARTED]) {
            CallLocalFunction("OnMapRaceLap", "iiii", playerid, raceid, lap,
                              now - _mr_Players[playerid][_MR_LAP_TICK]);
            if (_mr_Players[playerid][_MR_GENERATION] != generation)
                return true;
            ++_mr_Players[playerid][_MR_LAP];
        }
        _mr_Players[playerid][_MR_STARTED] = true;
        _mr_Players[playerid][_MR_LAP_TICK] = now;
    }
    if (finished) {
        new elapsed = now - _mr_Players[playerid][_MR_START_TICK];
        _mr_Release(playerid);
        CallLocalFunction("OnMapRaceFinish", "iii", playerid, raceid, elapsed);
        return true;
    }
    new next = _mr_Checkpoints[target][_MR_NEXT];
    _mr_Players[playerid][_MR_TARGET] = next == -1 ? _mr_Races[index][_MR_FIRST] : next;
    _mr_Players[playerid][_MR_ENTERING] = false;
    if (!_mr_ShowTarget(playerid))
        StopMapRace(playerid, MAPRACE_STOP_RESOURCE);
    return true;
}

#if defined _streamer_included
    stock bool:MapRace_HandleDynamicEnter(playerid, STREAMER_TAG_RACE_CP:checkpointid) {
        if (!_mr_ValidPlayer(playerid) || !_mr_Players[playerid][_MR_ACTIVE] ||
            _:checkpointid != _mr_Players[playerid][_MR_CP_HANDLE])
            return false;
        return _mr_Enter(playerid);
    }
#else
    stock bool:MapRace_HandleNativeEnter(playerid) {
        return _mr_Enter(playerid);
    }
#endif

stock MapRace_HandleConnect(playerid) {
    if (!_mr_ValidPlayer(playerid))
        return 0;
    if (_mr_Players[playerid][_MR_ACTIVE])
        _mr_Release(playerid);
    return 1;
}

stock MapRace_HandleDisconnect(playerid) {
    StopMapRace(playerid, MAPRACE_STOP_DISCONNECTED);
    return 1;
}

stock MapRace_HandleDeath(playerid) {
    StopMapRace(playerid, MAPRACE_STOP_DEATH);
    return 1;
}

stock MapRace_HandleUpdate(playerid) {
    if (_mr_ValidPlayer(playerid) && _mr_Players[playerid][_MR_ACTIVE] &&
        !_mr_InDimension(playerid, _mr_Players[playerid][_MR_RACE_INDEX]))
        StopMapRace(playerid, MAPRACE_STOP_DIMENSION);
    return 1;
}

stock MapRace_Clear() {
    for (new playerid = 0; playerid < MAX_PLAYERS; ++playerid) {
        if (_mr_Players[playerid][_MR_ACTIVE])
            _mr_Release(playerid);
    }
    for (new i = 0; i < _mr_RaceCount; ++i) {
        _mr_Races[i][_MR_COUNT] = 0;
        _mr_Races[i][_MR_PLAYERS] = 0;
    }
    _mr_RaceCount = 0;
    _mr_CheckpointCount = 0;
    return 1;
}

public OnGameModeExit() {
    MapRace_Clear();
    #if defined _mr_OnGameModeExit
        return _mr_OnGameModeExit();
    #else
        return 1;
    #endif
}
#if defined _ALS_OnGameModeExit
    #undef OnGameModeExit
#else
    #define _ALS_OnGameModeExit
#endif
#define OnGameModeExit _mr_OnGameModeExit
#if defined _mr_OnGameModeExit
    forward _mr_OnGameModeExit();
#endif

public OnFilterScriptExit() {
    MapRace_Clear();
    #if defined _mr_OnFilterScriptExit
        return _mr_OnFilterScriptExit();
    #else
        return 1;
    #endif
}
#if defined _ALS_OnFilterScriptExit
    #undef OnFilterScriptExit
#else
    #define _ALS_OnFilterScriptExit
#endif
#define OnFilterScriptExit _mr_OnFilterScriptExit
#if defined _mr_OnFilterScriptExit
    forward _mr_OnFilterScriptExit();
#endif

public OnPlayerConnect(playerid) {
    MapRace_HandleConnect(playerid);
    #if defined _mr_OnPlayerConnect
        return _mr_OnPlayerConnect(playerid);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerConnect
    #undef OnPlayerConnect
#else
    #define _ALS_OnPlayerConnect
#endif
#define OnPlayerConnect _mr_OnPlayerConnect
#if defined _mr_OnPlayerConnect
    forward _mr_OnPlayerConnect(playerid);
#endif

public OnPlayerDisconnect(playerid, reason) {
    MapRace_HandleDisconnect(playerid);
    #if defined _mr_OnPlayerDisconnect
        return _mr_OnPlayerDisconnect(playerid, reason);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerDisconnect
    #undef OnPlayerDisconnect
#else
    #define _ALS_OnPlayerDisconnect
#endif
#define OnPlayerDisconnect _mr_OnPlayerDisconnect
#if defined _mr_OnPlayerDisconnect
    forward _mr_OnPlayerDisconnect(playerid, reason);
#endif

#if defined _INC_open_mp
    #define _MR_WEAPON WEAPON
#else
    #define _MR_WEAPON _
#endif
public OnPlayerDeath(playerid, killerid, _MR_WEAPON:reason) {
    MapRace_HandleDeath(playerid);
    #if defined _mr_OnPlayerDeath
        return _mr_OnPlayerDeath(playerid, killerid, reason);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerDeath
    #undef OnPlayerDeath
#else
    #define _ALS_OnPlayerDeath
#endif
#define OnPlayerDeath _mr_OnPlayerDeath
#if defined _mr_OnPlayerDeath
    forward _mr_OnPlayerDeath(playerid, killerid, _MR_WEAPON:reason);
#endif
#undef _MR_WEAPON

public OnPlayerUpdate(playerid) {
    MapRace_HandleUpdate(playerid);
    #if defined _mr_OnPlayerUpdate
        return _mr_OnPlayerUpdate(playerid);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerUpdate
    #undef OnPlayerUpdate
#else
    #define _ALS_OnPlayerUpdate
#endif
#define OnPlayerUpdate _mr_OnPlayerUpdate
#if defined _mr_OnPlayerUpdate
    forward _mr_OnPlayerUpdate(playerid);
#endif

#if defined _streamer_included
    public OnPlayerEnterDynamicRaceCP(playerid, STREAMER_TAG_RACE_CP:checkpointid) {
        MapRace_HandleDynamicEnter(playerid, checkpointid);
        #if defined _mr_OnPlayerEnterDynamicRaceCP
            return _mr_OnPlayerEnterDynamicRaceCP(playerid, checkpointid);
        #else
            return 1;
        #endif
    }
    #if defined _ALS_OnPlayerEnterDynamicRaceCP
        #undef OnPlayerEnterDynamicRaceCP
    #else
        #define _ALS_OnPlayerEnterDynamicRaceCP
    #endif
    #define OnPlayerEnterDynamicRaceCP _mr_OnPlayerEnterDynamicRaceCP
    #if defined _mr_OnPlayerEnterDynamicRaceCP
        forward _mr_OnPlayerEnterDynamicRaceCP(playerid, STREAMER_TAG_RACE_CP:checkpointid);
    #endif
#else
    public OnPlayerEnterRaceCheckpoint(playerid) {
        MapRace_HandleNativeEnter(playerid);
        #if defined _mr_OnPlayerEnterRaceCP
            return _mr_OnPlayerEnterRaceCP(playerid);
        #else
            return 1;
        #endif
    }
    #if defined _ALS_OnPlayerEnterRaceCP
        #undef OnPlayerEnterRaceCheckpoint
    #else
        #define _ALS_OnPlayerEnterRaceCP
    #endif
    #define OnPlayerEnterRaceCheckpoint _mr_OnPlayerEnterRaceCP
    #if defined _mr_OnPlayerEnterRaceCP
        forward _mr_OnPlayerEnterRaceCP(playerid);
    #endif
#endif
