// Source Repository: https://github.com/waverso/waversomapstudio

inline void Parser::Process(const Call& call, const std::string& assignment)
{
    if (call.name == "RemoveBuildingForPlayer")
    {
        if (items.size() >= MaxCreatedItems)
        {
            out.failed = true;
            out.error = "Clipboard code exceeds the total item creation limit.";
            return;
        }
        ImportedItem item;
        item.snapshot.type = SceneItemRemoveBuilding;
        auto& d = item.snapshot.data;
        auto& removal = d.removeBuilding.Edit();
        bool valid = call.args.size() == 6 && !call.args[0].empty() && Integer(call, 1, removal.modelId, -1, 19999)
            && Float(call, 5, removal.radius) && removal.radius >= 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            valid = Float(call, axis + 2, d.transform.pos[axis]) && valid;
        }
        if (!valid)
        {
            return;
        }
        removal.custom = true;
        items.push_back(std::move(item));
        return;
    }
    const bool dynamic = call.name.find("Dynamic") != std::string::npos;
    if (call.name == "CreateActor" || call.name == "CreateDynamicActor" || call.name == "CreateDynamicActorEx")
    {
        if (items.size() >= MaxCreatedItems)
        {
            out.failed = true;
            out.error = "Clipboard code exceeds the total item creation limit.";
            return;
        }
        ImportedItem item;
        item.dynamic = dynamic;
        auto& d = item.snapshot.data;
        auto& actor = d.actor.Edit();
        bool valid = call.args.size() >= 5 && (dynamic || call.args.size() == 5)
            && Integer(call, 0, actor.skinId, SkinCatalog::FirstId, SkinCatalog::LastId) && SkinCatalog::IsValid(actor.skinId)
            && Float(call, 4, d.transform.rot[2]);
        for (int axis = 0; axis < 3; ++axis)
        {
            valid = Float(call, axis + 1, d.transform.pos[axis]) && valid;
        }
        if (dynamic)
        {
            int invulnerable = 1;
            if (!DefaultArgument(call, 5))
            {
                valid = Integer(call, 5, invulnerable, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()) && valid;
            }
            actor.invulnerable = invulnerable != 0;
            if (!DefaultArgument(call, 6))
            {
                valid = Float(call, 6, actor.health) && valid;
            }
            const std::size_t distance = call.name == "CreateDynamicActorEx" ? 7 : 10;
            if (DefaultArgument(call, distance))
            {
                const auto value = values.find("STREAMER_ACTOR_SD");
                if (value == values.end() || !std::isfinite(value->second)
                    || std::abs(value->second) > std::numeric_limits<float>::max())
                {
                    valid = false;
                }
                else
                {
                    actor.streamDistance = static_cast<float>(value->second);
                }
            }
            else
            {
                valid = Float(call, distance, actor.streamDistance) && valid;
            }
            valid = actor.streamDistance >= 0.0f && valid;
            actor.streamDistance = std::clamp(actor.streamDistance, ActorData::MinStreamDistance, ActorData::MaxStreamDistance);
        }
        if (!valid || actor.health < ActorData::MinHealth)
        {
            return;
        }
        actor.health = std::min(actor.health, ActorData::MaxHealth);
        d.actorSeq = d.nextSeq++;
        items.push_back(std::move(item));
        if (!assignment.empty())
        {
            references[assignment] = items.size() - 1;
        }
        return;
    }
    if (call.name == "CreatePickup" || call.name == "AddStaticPickup" || call.name == "CreateDynamicPickup"
        || call.name == "CreateDynamicPickupEx")
    {
        if (items.size() >= MaxCreatedItems)
        {
            out.failed = true;
            out.error = "Clipboard code exceeds the total item creation limit.";
            return;
        }
        ImportedItem item;
        item.dynamic = dynamic;
        auto& d = item.snapshot.data;
        auto& pickup = d.pickup.Edit();
        const auto& modelExists = env.pickupModelExists ? env.pickupModelExists : env.modelExists;
        bool valid = call.args.size() >= 5 && !call.args[1].empty() && Integer(call, 0, pickup.modelId, 321, 19999)
            && !(pickup.modelId >= 400 && pickup.modelId <= 614) && modelExists && modelExists(pickup.modelId);
        for (int axis = 0; axis < 3; ++axis)
        {
            valid = Float(call, axis + 2, d.transform.pos[axis]) && valid;
        }
        if (dynamic)
        {
            const std::size_t distance = call.name == "CreateDynamicPickupEx" ? 5 : 8;
            const bool defaultDistance
                = distance >= call.args.size() || (call.args[distance].size() == 1 && call.args[distance][0].text == "_");
            if (defaultDistance)
            {
                const auto value = values.find("STREAMER_PICKUP_SD");
                if (value == values.end() || std::abs(value->second) > std::numeric_limits<float>::max())
                {
                    valid = false;
                }
                else
                {
                    pickup.streamDistance = static_cast<float>(value->second);
                }
            }
            else
            {
                valid = Float(call, distance, pickup.streamDistance) && valid;
            }
            pickup.streamDistance
                = std::clamp(pickup.streamDistance, PickupData::MinStreamDistance, PickupData::MaxStreamDistance);
        }
        if (!valid || !PickupPosition::ValidInput(d.transform.pos))
        {
            return;
        }
        d.pickupSeq = d.nextSeq++;
        items.push_back(std::move(item));
        if (!assignment.empty() && call.name != "AddStaticPickup")
        {
            references[assignment] = items.size() - 1;
        }
        return;
    }
    if (call.name == "CreateVehicle" || call.name == "AddStaticVehicle" || call.name == "AddStaticVehicleEx")
    {
        if (items.size() >= MaxCreatedItems)
        {
            out.failed = true;
            out.error = "Clipboard code exceeds the total item creation limit.";
            return;
        }
        ImportedItem item;
        auto& d = item.snapshot.data;
        bool valid = call.args.size() >= 7 && Integer(call, 0, d.vehicle.Edit().modelId, 400, 611)
            && Float(call, 4, d.transform.rot[2]) && Integer(call, 5, d.vehicle.Edit().colors[0], -1, 255)
            && Integer(call, 6, d.vehicle.Edit().colors[1], -1, 255);
        for (int axis = 0; axis < 3; ++axis)
        {
            valid = Float(call, axis + 1, d.transform.pos[axis]) && valid;
        }
        if (!valid)
        {
            return;
        }
        d.vehicle.Edit();
        d.vehicleSeq = d.nextSeq++;
        items.push_back(std::move(item));
        if (!assignment.empty())
        {
            references[assignment] = items.size() - 1;
        }
        return;
    }
    if (call.name.starts_with("Create"))
    {
        if (items.size() >= MaxCreatedItems)
        {
            out.failed = true;
            out.error = "Clipboard code exceeds the total item creation limit.";
            return;
        }
        ImportedItem item;
        item.dynamic = dynamic;
        auto& d = item.snapshot.data;
        bool valid = call.args.size() >= 7 && Integer(call, 0, d.model.Edit().modelId, 321, 19999)
            && !(d.model.Edit().modelId >= 400 && d.model.Edit().modelId <= 611) && env.modelExists
            && env.modelExists(d.model.Edit().modelId);
        for (int axis = 0; axis < 3; ++axis)
        {
            valid = Float(call, axis + 1, d.transform.pos[axis]) && valid;
            valid = Float(call, axis + 4, d.transform.rot[axis]) && valid;
        }
        if (!valid)
        {
            return;
        }
        d.model.Edit();
        d.modelSeq = d.nextSeq++;
        if (dynamic)
        {
            const bool extended = call.name == "CreateDynamicObjectEx";
            Distance(call, extended ? 7 : 10, d.model.Edit().streamDistance);
            Distance(call, extended ? 8 : 11, d.model.Edit().drawDistance);
        }
        else
        {
            Distance(call, 7, d.model.Edit().drawDistance);
        }
        items.push_back(std::move(item));
        if (!assignment.empty())
        {
            references[assignment] = items.size() - 1;
        }
        return;
    }
    if (call.args.empty())
    {
        return;
    }
    std::size_t pos = 0;
    const std::string key = Reference(call.args[0], pos);
    const auto found = references.find(key);
    if (key.empty() || pos != call.args[0].size() || found == references.end() || items[found->second].destroyed)
    {
        return;
    }
    auto& item = items[found->second];
    if (item.dynamic != dynamic)
    {
        return;
    }
    if (call.name == "SetVehicleHealth")
    {
        auto& data = item.snapshot.data;
        float health;
        if (data.vehicle.HasValue() && call.args.size() == 2 && Float(call, 1, health) && health >= VehicleData::MinHealth)
        {
            data.vehicle.Edit().health = std::min(health, VehicleData::MaxHealth);
        }
        return;
    }
    const bool actorCall = call.name == "DestroyActor" || call.name == "DestroyDynamicActor" || call.name == "ApplyActorAnimation"
        || call.name == "ApplyDynamicActorAnimation" || call.name == "ClearActorAnimations"
        || call.name == "ClearDynamicActorAnimations" || call.name.starts_with("SetActor")
        || call.name.starts_with("SetDynamicActor");
    if (actorCall)
    {
        auto& d = item.snapshot.data;
        if (!d.actor.HasValue())
        {
            return;
        }
        if (call.name.starts_with("Apply"))
        {
            ActorAnimationData animation;
            int flags[4] = {};
            bool valid = call.args.size() == 9 && CopyString(call.args[1], animation.library)
                && CopyString(call.args[2], animation.name) && AnimationCatalog::Find(animation.library, animation.name)
                && Float(call, 3, animation.delta) && animation.delta >= 0.0f
                && Integer(call, 8, animation.time, 0, std::numeric_limits<int>::max());
            for (int i = 0; i < 4; ++i)
            {
                valid = Integer(call, i + 4, flags[i], std::numeric_limits<int>::min(), std::numeric_limits<int>::max()) && valid;
            }
            if (valid)
            {
                animation.loop = flags[0] != 0;
                animation.lockX = flags[1] != 0;
                animation.lockY = flags[2] != 0;
                animation.freeze = flags[3] != 0;
                if (!d.actorAnimation.HasValue())
                {
                    d.actorAnimationSeq = d.nextSeq++;
                }
                d.actorAnimation = animation;
            }
        }
        else if (call.name.starts_with("Clear") && call.args.size() == 1)
        {
            d.actorAnimation.Reset();
        }
        else if (call.name.starts_with("Destroy"))
        {
            item.destroyed = true;
            references.erase(found);
        }
        else if (call.name.ends_with("Pos") && call.args.size() == 4)
        {
            float position[3];
            bool valid = true;
            for (int axis = 0; axis < 3; ++axis)
            {
                valid = Float(call, axis + 1, position[axis]) && valid;
            }
            if (valid)
            {
                std::copy_n(position, 3, d.transform.pos);
            }
        }
        else if (call.name.ends_with("FacingAngle") && call.args.size() == 2)
        {
            float angle;
            if (Float(call, 1, angle))
            {
                d.transform.rot[2] = angle;
            }
        }
        else if (call.name.ends_with("Health") && call.args.size() == 2)
        {
            float health;
            if (Float(call, 1, health) && health >= ActorData::MinHealth)
            {
                d.actor.Edit().health = std::min(health, ActorData::MaxHealth);
            }
        }
        else if (call.name.ends_with("Invulnerable") && call.args.size() <= 2)
        {
            int invulnerable = 1;
            if (DefaultArgument(call, 1)
                || Integer(call, 1, invulnerable, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))
            {
                d.actor.Edit().invulnerable = invulnerable != 0;
            }
        }
        return;
    }
    if (item.snapshot.data.actor.HasValue())
    {
        return;
    }
    const bool pickupCall = call.name == "DestroyPickup" || call.name == "DestroyDynamicPickup";
    if (pickupCall != item.snapshot.data.pickup.HasValue() || item.snapshot.data.vehicle.HasValue())
    {
        return;
    }
    if (call.name.starts_with("Destroy"))
    {
        item.destroyed = true;
        references.erase(found);
        return;
    }
    if (call.name.ends_with("MaterialText"))
    {
        MaterialText(call, item.snapshot.data, dynamic);
    }
    else
    {
        Material(call, item.snapshot.data);
    }
}
