// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>

#include "CEntity.h"
#include "CPools.h"

namespace EntityRef
{
    enum class Kind : unsigned char
    {
        None,
        Building,
        Object,
        Dummy
    };

    inline Kind KindOf(CEntity* ent)
    {
        if (!ent)
        {
            return Kind::None;
        }
        switch (ent->m_nType)
        {
            case ENTITY_TYPE_BUILDING:
                return Kind::Building;
            case ENTITY_TYPE_OBJECT:
                return Kind::Object;
            case ENTITY_TYPE_DUMMY:
                return Kind::Dummy;
            default:
                return Kind::None;
        }
    }

    template <typename TPool>
    inline int MakeIn(TPool* pool, CEntity* ent)
    {
        if (!pool || !ent)
        {
            return -1;
        }
        const uintptr_t stride = pool->GetObjectSize();
        const uintptr_t off = (uintptr_t)ent - (uintptr_t)pool->m_pObjects;
        if (stride == 0 || off % stride != 0)
        {
            return -1;
        }
        const uintptr_t slot = off / stride;
        if (slot >= (uintptr_t)pool->m_nSize)
        {
            return -1;
        }
        return ((int)slot << 8) | pool->m_byteMap[slot].IntValue();
    }

    template <typename TPool>
    inline CEntity* ResolveIn(TPool* pool, int ref)
    {
        if (!pool || ref < 0)
        {
            return nullptr;
        }
        const int slot = ref >> 8;
        if (slot >= pool->m_nSize)
        {
            return nullptr;
        }
        if (pool->m_byteMap[slot].IntValue() != (unsigned char)(ref & 0xFF))
        {
            return nullptr;
        }
        return (CEntity*)pool->GetAt(slot);
    }

    inline int Make(CEntity* ent, Kind kind)
    {
        switch (kind)
        {
            case Kind::Building:
                return MakeIn(CPools::ms_pBuildingPool, ent);
            case Kind::Object:
                return MakeIn(CPools::ms_pObjectPool, ent);
            case Kind::Dummy:
                return MakeIn(CPools::ms_pDummyPool, ent);
            default:
                return -1;
        }
    }

    inline CEntity* Resolve(Kind kind, int ref)
    {
        switch (kind)
        {
            case Kind::Building:
                return ResolveIn(CPools::ms_pBuildingPool, ref);
            case Kind::Object:
                return ResolveIn(CPools::ms_pObjectPool, ref);
            case Kind::Dummy:
                return ResolveIn(CPools::ms_pDummyPool, ref);
            default:
                return nullptr;
        }
    }

    inline bool Alive(CEntity* ent, Kind kind, int ref)
    {
        return ent != nullptr && ref >= 0 && Resolve(kind, ref) == ent;
    }
}
