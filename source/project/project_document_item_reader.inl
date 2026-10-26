// Source Repository: https://github.com/waverso/waversomapstudio

void ReadItem(Reader& r, ItemSnapshot& item)
{
    item.id = r.Int();
    item.parentId = r.Int();
    item.order = r.Int();
    const int type = r.Int();
    if (type < SceneItemNormal || type > SceneItemCurveNode)
    {
        throw std::runtime_error("Invalid item type.");
    }
    item.type = static_cast<SceneItemType>(type);
    r.String(item.name);
    auto& d = item.data;
    r.Vector(d.transform.pos);
    r.Vector(d.transform.rot);
    if (r.Bool())
    {
        d.model.Edit();
    }
    if (d.model.HasValue())
    {
        d.model.Edit().modelId = r.Int();
        d.model.Edit().drawDistance = r.Float();
        d.model.Edit().streamDistance = r.Float();
        d.modelSeq = r.Int();
    }
    if (r.Bool())
    {
        d.slide.Edit();
    }
    if (d.slide.HasValue())
    {
        r.Vector(d.slide.Edit().targetPos);
        r.Vector(d.slide.Edit().targetRot);
        d.slide.Edit().speed = r.Float();
        d.slideSeq = r.Int();
    }
    if (r.Bool())
    {
        d.removeBuilding.Edit();
    }
    if (d.removeBuilding.HasValue())
    {
        d.removeBuilding.Edit().modelId = r.Int();
        d.removeBuilding.Edit().radius = r.Float();
        d.removeBuilding.Edit().custom = r.Bool();
    }
    const int materialsCount = r.Int();
    if (materialsCount < 0 || materialsCount > ItemData::MaxMaterials)
    {
        throw std::runtime_error("Invalid material count.");
    }
    d.materials.resize(materialsCount);
    for (int i = 0; i < d.MaterialCount(); ++i)
    {
        auto& m = d.materials[i];
        m.materialIndex = r.Int();
        m.colorArgb = r.U32();
        m.textureModelId = r.Int();
        r.String(m.txdName);
        r.String(m.textureName);
        d.materials[i].sequence = r.Int();
    }
    const int materialTextsCount = r.Int();
    if (materialTextsCount < 0 || materialTextsCount > ItemData::MaxMaterials)
    {
        throw std::runtime_error("Invalid material text count.");
    }
    d.materialTexts.resize(materialTextsCount);
    for (int i = 0; i < d.MaterialTextCount(); ++i)
    {
        auto& m = d.materialTexts[i];
        m.materialIndex = r.Int();
        r.String(m.text);
        m.materialSize = r.Int();
        r.String(m.fontFace);
        m.fontSize = r.Int();
        m.bold = r.Bool();
        m.fontColorArgb = r.U32();
        m.backgroundColorArgb = r.U32();
        m.alignment = r.Int();
        d.materialTexts[i].sequence = r.Int();
    }
    d.nextSeq = r.Int();
    if (r.Bool())
    {
        d.vehicle.Edit();
        d.vehicle.Edit().modelId = r.Int();
        d.vehicle.Edit().colors[0] = r.Int();
        d.vehicle.Edit().colors[1] = r.Int();
        d.vehicleSeq = r.Int();
        d.vehicle.Edit().health = std::min(r.Float(), VehicleData::MaxHealth);
    }
    if (r.Bool())
    {
        auto& area = d.boxArea.Edit();
        for (int axis = 0; axis < 3; ++axis)
        {
            const float size = r.Float();
            if (size < BoxAreaData::MinSize || size > BoxAreaData::MaxSize)
            {
                throw std::runtime_error("Box area size must be between 0.25 m and 20000 m.");
            }
            area.SetSize(axis, size);
        }
        area.colorRGBA = r.U32();
        d.boxAreaSeq = r.Int();
    }
    if (r.Bool())
    {
        auto& pickup = d.pickup.Edit();
        pickup.modelId = r.Int();
        pickup.streamDistance = r.Float();
        d.pickupSeq = r.Int();
    }
    if (r.Bool())
    {
        auto& actor = d.actor.Edit();
        actor.skinId = r.Int();
        actor.health = std::min(r.Float(), ActorData::MaxHealth);
        actor.invulnerable = r.Bool();
        actor.streamDistance = r.Float();
        d.actorSeq = r.Int();
    }
    if (r.Bool())
    {
        auto& animation = d.actorAnimation.Edit();
        r.String(animation.library);
        r.String(animation.name);
        animation.delta = r.Float();
        animation.loop = r.Bool();
        animation.lockX = r.Bool();
        animation.lockY = r.Bool();
        animation.freeze = r.Bool();
        animation.time = r.Int();
        d.actorAnimationSeq = r.Int();
    }
    if (r.Bool())
    {
        auto& race = d.race.Edit();
        race.mode = r.Int();
        race.laps = r.Int();
    }
    if (r.Bool())
    {
        auto& checkpoint = d.raceCheckpoint.Edit();
        checkpoint.radius = r.Float();
        checkpoint.type = r.Int();
    }
    if (r.Bool())
    {
        auto& layout = d.layout.Edit();
        const int layoutType = r.Int();
        if (layoutType < 0 || layoutType > 2)
        {
            throw std::runtime_error("Invalid layout type.");
        }
        layout.type = static_cast<LayoutType>(layoutType);
        layout.modelId = r.Int();
        layout.pieces = r.Int();
        layout.axis = r.Int();
        layout.reverse = r.Bool();
        layout.radius = r.Float();
        layout.loops = r.Float();
        layout.startAngle = r.Float();
        layout.offset = r.Float();
        r.Vector(layout.rotation);
        layout.autoRadius = r.Bool();
        layout.autoStep = r.Bool();
        layout.spacing = r.Float();
        layout.columns = r.Int();
        layout.gridSpacing[0] = r.Float();
        layout.gridSpacing[1] = r.Float();
        layout.copyFromFirst = r.Bool();
        layout.modelPending = r.Bool();
        layout.piecesPending = r.Bool();
        d.layoutSeq = r.Int();
        layout.curveId = r.Int();
        layout.lateralOffset = r.Float();
        layout.verticalOffset = r.Float();
        for (bool& locked : layout.lockCurveRotation)
        {
            locked = r.Bool();
        }
        layout.curveRange[0] = r.Float();
        layout.curveRange[1] = r.Float();
        layout.spacingMode = static_cast<LayoutSpacing>(r.Int());
        layout.step = r.Float();
    }
    if (r.Bool())
    {
        d.curve.Edit().closed = r.Bool();
    }
    if (r.Bool())
    {
        auto& node = d.curveNode.Edit();
        node.mode = static_cast<CurveHandle>(r.Int());
        r.Vector(node.incoming);
        r.Vector(node.outgoing);
        node.tilt = r.Float();
    }
}
