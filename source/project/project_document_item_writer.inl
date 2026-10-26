// Source Repository: https://github.com/waverso/waversomapstudio

void WriteItem(Writer& w, const ItemSnapshot& item)
{
    w.Int(item.id);
    w.Int(item.parentId);
    w.Int(item.order);
    w.Int(item.type);
    w.String(item.name);
    const auto& d = item.data;
    w.Vector(d.transform.pos);
    w.Vector(d.transform.rot);
    w.Bool(d.model.HasValue());
    if (d.model.HasValue())
    {
        w.Int(d.model.Get().modelId);
        w.Float(d.model.Get().drawDistance);
        w.Float(d.model.Get().streamDistance);
        w.Int(d.modelSeq);
    }
    w.Bool(d.slide.HasValue());
    if (d.slide.HasValue())
    {
        w.Vector(d.slide.Get().targetPos);
        w.Vector(d.slide.Get().targetRot);
        w.Float(d.slide.Get().speed);
        w.Int(d.slideSeq);
    }
    w.Bool(d.removeBuilding.HasValue());
    if (d.removeBuilding.HasValue())
    {
        w.Int(d.removeBuilding.Get().modelId);
        w.Float(d.removeBuilding.Get().radius);
        w.Bool(d.removeBuilding.Get().custom);
    }
    w.Int(d.MaterialCount());
    for (int i = 0; i < d.MaterialCount(); ++i)
    {
        const auto& m = d.materials[i];
        w.Int(m.materialIndex);
        w.U32(m.colorArgb);
        w.Int(m.textureModelId);
        w.String(m.txdName);
        w.String(m.textureName);
        w.Int(d.materials[i].sequence);
    }
    w.Int(d.MaterialTextCount());
    for (int i = 0; i < d.MaterialTextCount(); ++i)
    {
        const auto& m = d.materialTexts[i];
        w.Int(m.materialIndex);
        w.String(m.text);
        w.Int(m.materialSize);
        w.String(m.fontFace);
        w.Int(m.fontSize);
        w.Bool(m.bold);
        w.U32(m.fontColorArgb);
        w.U32(m.backgroundColorArgb);
        w.Int(m.alignment);
        w.Int(d.materialTexts[i].sequence);
    }
    w.Int(d.nextSeq);
    w.Bool(d.vehicle.HasValue());
    if (d.vehicle.HasValue())
    {
        w.Int(d.vehicle.Get().modelId);
        w.Int(d.vehicle.Get().colors[0]);
        w.Int(d.vehicle.Get().colors[1]);
        w.Int(d.vehicleSeq);
        w.Float(d.vehicle.Get().health);
    }
    w.Bool(d.boxArea.HasValue());
    if (d.boxArea.HasValue())
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            w.Float(d.boxArea.Get().GetSize(axis));
        }
        w.U32(d.boxArea.Get().colorRGBA);
        w.Int(d.boxAreaSeq);
    }
    w.Bool(d.pickup.HasValue());
    if (d.pickup.HasValue())
    {
        w.Int(d.pickup.Get().modelId);
        w.Float(d.pickup.Get().streamDistance);
        w.Int(d.pickupSeq);
    }
    w.Bool(d.actor.HasValue());
    if (d.actor.HasValue())
    {
        const auto& actor = d.actor.Get();
        w.Int(actor.skinId);
        w.Float(actor.health);
        w.Bool(actor.invulnerable);
        w.Float(actor.streamDistance);
        w.Int(d.actorSeq);
    }
    w.Bool(d.actorAnimation.HasValue());
    if (d.actorAnimation.HasValue())
    {
        const auto& animation = d.actorAnimation.Get();
        w.String(animation.library);
        w.String(animation.name);
        w.Float(animation.delta);
        w.Bool(animation.loop);
        w.Bool(animation.lockX);
        w.Bool(animation.lockY);
        w.Bool(animation.freeze);
        w.Int(animation.time);
        w.Int(d.actorAnimationSeq);
    }
    w.Bool(d.race.HasValue());
    if (d.race.HasValue())
    {
        const auto& race = d.race.Get();
        w.Int(race.mode);
        w.Int(race.laps);
    }
    w.Bool(d.raceCheckpoint.HasValue());
    if (d.raceCheckpoint.HasValue())
    {
        w.Float(d.raceCheckpoint.Get().radius);
        w.Int(d.raceCheckpoint.Get().type);
    }
    w.Bool(d.layout.HasValue());
    if (d.layout.HasValue())
    {
        const auto& layout = d.layout.Get();
        w.Int(static_cast<int>(layout.type));
        w.Int(layout.modelId);
        w.Int(layout.pieces);
        w.Int(layout.axis);
        w.Bool(layout.reverse);
        w.Float(layout.radius);
        w.Float(layout.loops);
        w.Float(layout.startAngle);
        w.Float(layout.offset);
        w.Vector(layout.rotation);
        w.Bool(layout.autoRadius);
        w.Bool(layout.autoStep);
        w.Float(layout.spacing);
        w.Int(layout.columns);
        w.Float(layout.gridSpacing[0]);
        w.Float(layout.gridSpacing[1]);
        w.Bool(layout.copyFromFirst);
        w.Bool(layout.modelPending);
        w.Bool(layout.piecesPending);
        w.Int(d.layoutSeq);
        w.Int(layout.curveId);
        w.Float(layout.lateralOffset);
        w.Float(layout.verticalOffset);
        for (const bool locked : layout.lockCurveRotation)
        {
            w.Bool(locked);
        }
        w.Float(layout.curveRange[0]);
        w.Float(layout.curveRange[1]);
        w.Int(static_cast<int>(layout.spacingMode));
        w.Float(layout.step);
    }
    w.Bool(d.curve.HasValue());
    if (d.curve.HasValue())
    {
        w.Bool(d.curve.Get().closed);
    }
    w.Bool(d.curveNode.HasValue());
    if (d.curveNode.HasValue())
    {
        const auto& node = d.curveNode.Get();
        w.Int(static_cast<int>(node.mode));
        w.Vector(node.incoming);
        w.Vector(node.outgoing);
        w.Float(node.tilt);
    }
}
