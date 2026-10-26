// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

namespace EntityRuntime
{
    template <class Runtime, class Entity, class Scene>
    int LookupItemIndex(Runtime& runtime, Entity* entity, Scene* scene)
    {
        const auto owner = runtime.owners.find(entity);
        if (owner == runtime.owners.end() || !scene)
        {
            return -1;
        }
        const auto entry = runtime.entries.find(owner->second);
        if (entry == runtime.entries.end() || !runtime.Alive(entry->second))
        {
            return -1;
        }
        const int index = entry->second.index;
        return index >= 0 && index < scene->count && scene->items[index].id == owner->second ? index : -1;
    }

    template <class Entry, class Entity>
    void Freeze(Entry& entry, Entity* entity, bool freeze)
    {
        if (freeze != entry.frozen)
        {
            if (freeze)
            {
                entry.gravity = entity->bApplyGravity;
                entry.moveForce = entity->bDisableMoveForce;
                entry.turnForce = entity->bDisableTurnForce;
                entry.dontApplySpeed = entity->bDontApplySpeed;
            }
            entity->bApplyGravity = freeze ? false : entry.gravity;
            entity->bDisableMoveForce = freeze ? true : entry.moveForce;
            entity->bDisableTurnForce = freeze ? true : entry.turnForce;
            entity->bDontApplySpeed = freeze ? true : entry.dontApplySpeed;
            entry.frozen = freeze;
        }
        if (freeze)
        {
            entity->m_vecMoveSpeed = {};
            entity->m_vecTurnSpeed = {};
            entity->m_vecFrictionMoveSpeed = {};
            entity->m_vecFrictionTurnSpeed = {};
        }
    }
}
