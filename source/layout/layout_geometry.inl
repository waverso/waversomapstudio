// Source Repository: https://github.com/waverso/waversomapstudio

namespace Layout
{
    namespace
    {
        constexpr double Turn = 2.0 * std::numbers::pi;

        Utils::RotationBasis Plane(int axis)
        {
            if (axis == 0)
            {
                return { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } };
            }
            if (axis == 1)
            {
                return { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } };
            }
            return {};
        }

        bool Closed(const LayoutData& data)
        {
            return data.offset == 0.0f && std::abs(data.loops - std::round(data.loops)) < 0.00001f;
        }

        bool CurveClosed(const CurveGeometry::Path& path, const LayoutData& data)
        {
            return path.Closed() && data.curveRange[0] == 0.0f && data.curveRange[1] == 1.0f;
        }

        double CurveLength(const CurveGeometry::Path& path, const LayoutData& data)
        {
            return path.Length() * (static_cast<double>(data.curveRange[1]) - data.curveRange[0]);
        }

        double LoopLength(const LayoutData& data)
        {
            return std::hypot(Turn * data.loops * data.radius, data.offset);
        }

        CVector PathPoint(const LayoutData& data, float radius, double progress)
        {
            const double angle
                = data.startAngle * std::numbers::pi / 180.0 + (data.reverse ? -1.0 : 1.0) * Turn * data.loops * progress;
            return Utils::TransformDirection(Plane(data.axis),
                { radius * static_cast<float>(std::cos(angle)), radius * static_cast<float>(std::sin(angle)),
                    data.offset * static_cast<float>(progress) });
        }

        TransformData PiecePose(const LayoutData& data, float radius, int piece)
        {
            TransformData result;
            const auto plane = Plane(data.axis);
            CVector position;
            Utils::RotationBasis frame = plane;
            if (data.type == LayoutType::Grid)
            {
                const int columns = (std::min)(data.columns, (std::max)(1, data.pieces));
                const int rows = (data.pieces + columns - 1) / columns;
                position = Utils::TransformDirection(plane,
                    { (piece % columns - (columns - 1) * 0.5f) * data.gridSpacing[0],
                        (piece / columns - (rows - 1) * 0.5f) * data.gridSpacing[1], 0 });
            }
            else
            {
                const int steps = Closed(data) ? data.pieces : data.pieces - 1;
                const double length = LoopLength(data);
                const double progress
                    = std::clamp(data.UsesStep() ? (length > 0 ? piece * static_cast<double>(data.step) / length : 0)
                                                 : (steps > 0 ? static_cast<double>(piece) / steps : 0),
                        0.0, 1.0);
                position = PathPoint(data, radius, progress);
                const double angle
                    = data.startAngle * std::numbers::pi / 180.0 + (data.reverse ? -1.0 : 1.0) * Turn * data.loops * progress;
                const float cosine = static_cast<float>(std::cos(angle));
                const float sine = static_cast<float>(std::sin(angle));
                const float direction = data.reverse ? -1.0f : 1.0f;
                const float advance = static_cast<float>(data.offset / (Turn * data.loops));
                const float tangentLength = std::hypot(radius, advance);
                const float radialWeight = tangentLength > 0.0f ? radius / tangentLength : 1.0f;
                const float axialWeight = tangentLength > 0.0f ? advance / tangentLength : 0.0f;
                frame.right = Utils::TransformDirection(
                    plane, { -sine * radialWeight * direction, cosine * radialWeight * direction, axialWeight });
                frame.up = Utils::TransformDirection(plane, { -cosine, -sine, 0 });
                frame.forward = Utils::CrossProduct(frame.up, frame.right);
            }
            result.pos[0] = position.x;
            result.pos[1] = position.y;
            result.pos[2] = position.z;
            Utils::BasisToEulerDegrees(Utils::ComposeRotationBases(frame, Utils::EulerDegreesToBasis(data.rotation)), result.rot);
            return result;
        }

        CurveGeometry::Sample CurveSample(const CurveGeometry::Path& path, const LayoutData& settings, double progress)
        {
            const double span = settings.curveRange[1] - settings.curveRange[0];
            const double coordinate
                = settings.reverse ? settings.curveRange[1] - span * progress : settings.curveRange[0] + span * progress;
            return path.At(settings.reverse ? 1.0 - coordinate : coordinate, settings.reverse);
        }

        template <class Callback>
        void CurvePoints(const CurveGeometry::Path& path, const LayoutData& settings, Callback&& add)
        {
            const auto point = [&](double progress)
            {
                const auto frame = CurveSample(path, settings, progress);
                const auto value = frame.position + frame.normal.Cross(frame.tangent) * settings.lateralOffset
                    + frame.normal * settings.verticalOffset;
                add(CVector(static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)));
            };
            point(0);
            const double span = settings.curveRange[1] - settings.curveRange[0];
            if (span <= 0)
            {
                return;
            }
            const auto sample = [&](const CurveGeometry::Sample& value)
            {
                const double coordinate = value.distance / path.Length();
                if (coordinate > settings.curveRange[0] && coordinate < settings.curveRange[1])
                {
                    point(settings.reverse ? (settings.curveRange[1] - coordinate) / span
                                           : (coordinate - settings.curveRange[0]) / span);
                }
            };
            if (settings.reverse)
            {
                for (auto it = path.Samples().rbegin(); it != path.Samples().rend(); ++it)
                {
                    sample(*it);
                }
            }
            else
            {
                for (const auto& value : path.Samples())
                {
                    sample(value);
                }
            }
            point(1);
        }

        TransformData CurvePose(
            const EditorScene& scene, int parent, const LayoutData& settings, int piece, const CurveGeometry::Path& path)
        {
            const int steps = CurveClosed(path, settings) ? settings.pieces : settings.pieces - 1;
            const double length = CurveLength(path, settings);
            const double progress = settings.spacingMode == LayoutSpacing::Step
                ? (length > 0 ? piece * static_cast<double>(settings.step) / length : 0)
                : (steps > 0 ? static_cast<double>(piece) / steps : 0);
            const auto sample = CurveSample(path, settings, std::clamp(progress, 0.0, 1.0));
            const auto convert = [](CurveGeometry::Vec value)
            {
                return CVector(static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z));
            };
            Utils::RotationBasis frame;
            frame.right = convert(sample.tangent);
            frame.up = convert(sample.normal);
            frame.forward = Utils::CrossProduct(frame.up, frame.right);
            const auto& world = scene.world[parent];
            const auto inverse = Utils::TransposeRotationBasis(world.rot);
            const CVector position
                = convert(sample.position) + frame.forward * settings.lateralOffset + frame.up * settings.verticalOffset;
            const CVector local = Utils::TransformDirection(inverse, position - world.pos);
            TransformData result;
            result.pos[0] = local.x;
            result.pos[1] = local.y;
            result.pos[2] = local.z;
            if (settings.lockCurveRotation[0] || settings.lockCurveRotation[1] || settings.lockCurveRotation[2])
            {
                float rotation[3];
                Utils::BasisToEulerDegrees(frame, rotation);
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (settings.lockCurveRotation[axis])
                    {
                        rotation[axis] = 0;
                    }
                }
                frame = Utils::EulerDegreesToBasis(rotation);
            }
            Utils::BasisToEulerDegrees(Utils::ComposeRotationBases(inverse,
                                           Utils::ComposeRotationBases(frame, Utils::EulerDegreesToBasis(settings.rotation))),
                result.rot);
            return result;
        }

        void SetPiecePose(const EditorScene& scene, int parent, const LayoutData& settings, float radius, int piece,
            SceneItemType type, ItemData& data, const CurveGeometry::Path* path = nullptr)
        {
            const auto pose = path ? CurvePose(scene, parent, settings, piece, *path) : PiecePose(settings, radius, piece);
            std::copy_n(pose.pos, 3, data.transform.pos);
            if (type == SceneItemRemoveBuilding)
            {
                const auto& world = scene.world[parent];
                const CVector position
                    = world.pos + Utils::TransformDirection(world.rot, { pose.pos[0], pose.pos[1], pose.pos[2] });
                data.transform.pos[0] = position.x;
                data.transform.pos[1] = position.y;
                data.transform.pos[2] = position.z;
            }
            if (data.pickup.HasValue() || data.raceCheckpoint.HasValue())
            {
                std::fill_n(data.transform.rot, 3, 0.0f);
                return;
            }
            if (data.vehicle.HasValue() || data.actor.HasValue()
                || (data.model.HasValue() && !ModelConstraints::SupportsRotationXY(data.model.Get().modelId)))
            {
                const auto& parentWorld = scene.world[parent];
                float rotation[3];
                Utils::BasisToEulerDegrees(
                    Utils::ComposeRotationBases(parentWorld.rot, Utils::EulerDegreesToBasis(pose.rot)), rotation);
                rotation[0] = rotation[1] = 0.0f;
                if (data.vehicle.HasValue() || data.actor.HasValue())
                {
                    data.transform.rot[0] = data.transform.rot[1] = 0.0f;
                    data.transform.rot[2] = rotation[2] - parentWorld.eulerDeg[2];
                }
                else
                {
                    Utils::BasisToEulerDegrees(Utils::ComposeRotationBases(Utils::TransposeRotationBasis(parentWorld.rot),
                                                   Utils::EulerDegreesToBasis(rotation)),
                        data.transform.rot);
                }
            }
            else
            {
                std::copy_n(pose.rot, 3, data.transform.rot);
            }
        }
    }

    float PathStep(const EditorScene& scene, const LayoutData& data)
    {
        if (data.UsesStep())
        {
            return data.step;
        }
        if (data.pieces == 0)
        {
            return 0.0f;
        }
        if (data.type == LayoutType::Loop)
        {
            const int steps = Closed(data) ? data.pieces : data.pieces - 1;
            return steps > 0 ? static_cast<float>(LoopLength(data) / steps) : 0.0f;
        }
        std::string error;
        const auto* path = Curves::Resolve(scene, scene.FindIndexById(data.curveId), error);
        if (!path)
        {
            return 0.0f;
        }
        const int steps = CurveClosed(*path, data) ? data.pieces : data.pieces - 1;
        return steps > 0 ? static_cast<float>(CurveLength(*path, data) / steps) : 0.0f;
    }

    bool ResolveGeometry(const EditorScene& scene, const Children& children, LayoutData& data, std::string& error)
    {
        error.clear();
        if (!data.IsValid())
        {
            error = "Invalid layout settings.";
            return false;
        }
        const bool step = data.UsesStep();
        if (data.pieces == 0 && !step)
        {
            return true;
        }
        if (data.type == LayoutType::Curve || step)
        {
            const auto* path
                = data.type == LayoutType::Curve ? Curves::Resolve(scene, scene.FindIndexById(data.curveId), error) : nullptr;
            if (data.type == LayoutType::Curve && !path)
            {
                return false;
            }
            if (!step)
            {
                return true;
            }
            double intervals = (path ? CurveLength(*path, data) : LoopLength(data)) / data.step;
            const double nearest = std::round(intervals);
            const double tolerance = 4 * std::numeric_limits<float>::epsilon() * (std::max)(1.0, intervals);
            if (std::abs(intervals - nearest) <= tolerance)
            {
                intervals = nearest;
            }
            const bool closed = path ? CurveClosed(*path, data) : Closed(data);
            const double count = (std::max)(1.0, closed ? std::ceil(intervals) : std::floor(intervals) + 1);
            if (!std::isfinite(count) || count > EditorScene::MaxItems)
            {
                error = "Step spacing exceeds the item limit. Increase Step.";
                return false;
            }
            if (!children.CanGenerate(data.copyFromFirst) && count != static_cast<double>(children.indices.size()))
            {
                error = "Step spacing requires a uniform model or Copy From First Child.";
                return false;
            }
            data.pieces = static_cast<int>(count);
            return true;
        }
        const bool grid = data.type == LayoutType::Grid;
        if (!(grid ? data.autoStep : data.autoRadius))
        {
            return true;
        }
        double width = 0.0, height = 0.0;
        if (data.copyFromFirst || !children.CanGenerate())
        {
            CVector minimum, maximum;
            if (children.indices.empty()
                || !Selection::ComputeItemBounds(scene, children.indices.front(), data.rotation, minimum, maximum))
            {
                error = "Auto spacing requires source bounds.";
                return false;
            }
            width = maximum.x - minimum.x;
            height = maximum.y - minimum.y;
        }
        else
        {
            if (!TextureBrowser::ModelExists(data.modelId))
            {
                error = "Auto spacing requires a valid model.";
                return false;
            }
            const auto model = ObjectBrowser::GetModel(data.modelId);
            const auto rotation = Utils::EulerDegreesToBasis(data.rotation);
            const float size[3] = { model.boundsMax[0] - model.boundsMin[0], model.boundsMax[1] - model.boundsMin[1],
                model.boundsMax[2] - model.boundsMin[2] };
            if (!model.statsIndexed
                || !std::all_of(std::begin(size), std::end(size),
                    [](float value)
                    {
                        return std::isfinite(value) && value >= 0.0f;
                    }))
            {
                error = "Auto spacing requires model bounds.";
                return false;
            }
            const auto extent = [&](float right, float forward, float up)
            {
                return std::abs(right) * size[0] + std::abs(forward) * size[1] + std::abs(up) * size[2];
            };
            width = extent(rotation.right.x, rotation.forward.x, rotation.up.x);
            height = extent(rotation.right.y, rotation.forward.y, rotation.up.y);
        }
        const double length = width * data.spacing;
        if (grid)
        {
            const double depth = height * data.spacing;
            if (!std::isfinite(length) || !std::isfinite(depth) || length <= 0.0 || depth <= 0.0 || length > LayoutData::MaxOffset
                || depth > LayoutData::MaxOffset)
            {
                error = "Auto Step is out of range. Adjust Rotation or Spacing.";
                return false;
            }
            data.gridSpacing[0] = static_cast<float>(length);
            data.gridSpacing[1] = static_cast<float>(depth);
            return true;
        }
        if (data.pieces < 2)
        {
            error = "Auto radius requires more copies.";
            return false;
        }
        const int steps = Closed(data) ? data.pieces : data.pieces - 1;
        const double angle = Turn * data.loops / steps;
        const double advance = data.offset / steps;
        if (!std::isfinite(length) || length < std::abs(advance) || angle >= std::numbers::pi)
        {
            error = "Adjust copies or offset to fit the source bounds.";
            return false;
        }
        const double calculated = std::sqrt(length * length - advance * advance) / (2.0 * std::sin(angle * 0.5));
        if (!std::isfinite(calculated) || calculated < LayoutData::MinRadius || calculated > LayoutData::MaxRadius)
        {
            error = "Auto Radius is out of range. Adjust Copies or Spacing.";
            return false;
        }
        data.radius = static_cast<float>(calculated);
        return true;
    }

    bool ComputeBounds(const EditorScene& scene, int parent, CVector& minimum, CVector& maximum)
    {
        if (parent < 0 || parent >= scene.count || !scene.data[parent].layout.HasValue())
        {
            return false;
        }
        const auto children = Inspect(scene, parent);
        auto settings = EffectiveSettings(scene.data[parent].layout.Get(), children);
        std::string error;
        if (!ResolveGeometry(scene, children, settings, error) && (!settings.IsValid() || settings.UsesStep()))
        {
            return false;
        }
        if (settings.pieces == 0)
        {
            return false;
        }
        const auto include = [&](const CVector& point)
        {
            minimum.x = (std::min)(minimum.x, point.x);
            minimum.y = (std::min)(minimum.y, point.y);
            minimum.z = (std::min)(minimum.z, point.z);
            maximum.x = (std::max)(maximum.x, point.x);
            maximum.y = (std::max)(maximum.y, point.y);
            maximum.z = (std::max)(maximum.z, point.z);
        };
        if (settings.type == LayoutType::Curve)
        {
            const int curve = scene.FindIndexById(settings.curveId);
            if (!Curves::CanUse(scene, parent, curve))
            {
                return false;
            }
            const auto* path = Curves::Resolve(scene, curve, error);
            if (!path)
            {
                return false;
            }
            const auto& world = scene.world[parent];
            bool first = true;
            CurvePoints(*path, settings,
                [&](const CVector& point)
                {
                    const CVector local = Utils::TransformDirection(Utils::TransposeRotationBasis(world.rot), point - world.pos);
                    if (first)
                    {
                        minimum = local;
                        maximum = local;
                        first = false;
                    }
                    else
                    {
                        include(local);
                    }
                });
        }
        else if (settings.type == LayoutType::Grid)
        {
            const int columns = (std::min)(settings.columns, settings.pieces);
            const int rows = (settings.pieces + columns - 1) / columns;
            const CVector extent((columns - 1) * settings.gridSpacing[0] * 0.5f, (rows - 1) * settings.gridSpacing[1] * 0.5f, 0);
            const auto plane = Plane(settings.axis);
            minimum = Utils::TransformDirection(plane, { -extent.x, -extent.y, 0 });
            maximum = minimum;
            include(Utils::TransformDirection(plane, extent));
        }
        else
        {
            minimum = PathPoint(settings, settings.radius, 0);
            maximum = minimum;
            include(PathPoint(settings, settings.radius, 1));
            const double start = settings.startAngle * std::numbers::pi / 180.0;
            const double sweep = (settings.reverse ? -1.0 : 1.0) * Turn * settings.loops;
            const double low = (std::min)(start, start + sweep);
            const double high = (std::max)(start, start + sweep);
            for (int quarter = 0; quarter < 4; ++quarter)
            {
                const double angle = quarter * Turn * 0.25;
                const double extremum = angle + std::ceil((low - angle) / Turn) * Turn;
                if (extremum <= high)
                {
                    include(PathPoint(settings, settings.radius, (extremum - start) / sweep));
                }
            }
        }
        return true;
    }

    void BuildPreview(const EditorScene& scene, int parent, std::vector<std::array<float, 6>>& lines)
    {
        if (parent < 0 || parent >= scene.count || !scene.data[parent].layout.HasValue())
        {
            return;
        }
        const auto children = Inspect(scene, parent);
        auto settings = EffectiveSettings(scene.data[parent].layout.Get(), children);
        std::string error;
        if (!ResolveGeometry(scene, children, settings, error))
        {
            return;
        }
        if (settings.pieces == 0)
        {
            return;
        }
        const auto& world = scene.world[parent];
        const auto add = [&](const CVector& first, const CVector& second)
        {
            const CVector a = world.pos + Utils::TransformDirection(world.rot, first);
            const CVector b = world.pos + Utils::TransformDirection(world.rot, second);
            lines.push_back({ a.x, a.y, a.z, b.x, b.y, b.z });
        };
        if (settings.type == LayoutType::Curve)
        {
            const int curve = scene.FindIndexById(settings.curveId);
            if (!Curves::CanUse(scene, parent, curve))
            {
                return;
            }
            const auto* path = Curves::Resolve(scene, curve, error);
            if (!path)
            {
                return;
            }
            CVector previous;
            bool first = true;
            CurvePoints(*path, settings,
                [&](const CVector& point)
                {
                    const CVector current
                        = Utils::TransformDirection(Utils::TransposeRotationBasis(world.rot), point - world.pos);
                    if (!first)
                    {
                        add(previous, current);
                    }
                    first = false;
                    previous = current;
                });
        }
        else if (settings.type == LayoutType::Grid)
        {
            const int columns = (std::min)(settings.columns, settings.pieces);
            for (int piece = 1; piece < settings.pieces; ++piece)
            {
                const auto current = PiecePose(settings, settings.radius, piece);
                const auto point = CVector(current.pos[0], current.pos[1], current.pos[2]);
                if (piece % columns != 0)
                {
                    const auto left = PiecePose(settings, settings.radius, piece - 1);
                    add({ left.pos[0], left.pos[1], left.pos[2] }, point);
                }
                if (piece >= columns)
                {
                    const auto above = PiecePose(settings, settings.radius, piece - columns);
                    add({ above.pos[0], above.pos[1], above.pos[2] }, point);
                }
            }
        }
        else
        {
            const int segments = std::clamp(static_cast<int>(std::ceil(settings.loops * 96.0f)), 16, 2048);
            CVector previous = PathPoint(settings, settings.radius, 0);
            for (int segment = 1; segment <= segments; ++segment)
            {
                const auto point = PathPoint(settings, settings.radius, static_cast<double>(segment) / segments);
                add(previous, point);
                previous = point;
            }
        }
    }
}
