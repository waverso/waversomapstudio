// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace CurveGeometry
{
    struct Vec
    {
        double x = 0, y = 0, z = 0;
        bool operator==(const Vec&) const = default;
        Vec operator+(Vec value) const
        {
            return { x + value.x, y + value.y, z + value.z };
        }
        Vec operator-(Vec value) const
        {
            return { x - value.x, y - value.y, z - value.z };
        }
        Vec operator*(double value) const
        {
            return { x * value, y * value, z * value };
        }
        double Dot(Vec value) const
        {
            return x * value.x + y * value.y + z * value.z;
        }
        Vec Cross(Vec value) const
        {
            return { y * value.z - z * value.y, z * value.x - x * value.z, x * value.y - y * value.x };
        }
        double Length() const
        {
            return std::sqrt(Dot(*this));
        }
        Vec Unit(Vec fallback = { 1, 0, 0 }) const
        {
            const double length = Length();
            return length > 1e-10 ? *this * (1.0 / length) : fallback;
        }
    };

    struct Knot
    {
        Vec position, incoming, outgoing;
        double tilt = 0;
        bool operator==(const Knot&) const = default;
    };

    struct Sample
    {
        Vec position, tangent, normal;
        double distance = 0, tilt = 0;
        size_t segment = 0;
        double parameter = 0;
    };

    inline Vec Rotate(Vec value, Vec axis, double angle)
    {
        return value * std::cos(angle) + axis.Cross(value) * std::sin(angle) + axis * (axis.Dot(value) * (1.0 - std::cos(angle)));
    }

    inline Vec Normal(Vec tangent, Vec up)
    {
        Vec normal = up - tangent * up.Dot(tangent);
        if (normal.Length() < 1e-8)
        {
            up = std::abs(tangent.z) < 0.9 ? Vec { 0, 0, 1 } : Vec { 0, 1, 0 };
            normal = up - tangent * up.Dot(tangent);
        }
        return normal.Unit({ 0, 0, 1 });
    }

    inline Vec Transport(Vec normal, Vec before, Vec after)
    {
        const Vec cross = before.Cross(after);
        const double sine = cross.Length();
        if (sine > 1e-8)
        {
            normal = Rotate(normal, cross * (1.0 / sine), std::atan2(sine, before.Dot(after)));
        }
        return Normal(after, normal);
    }

    class Path
    {
        static constexpr size_t MaxSamples = 65536;
        std::vector<Sample> samples;
        std::vector<Knot> controls;
        bool closed = false;

        bool Subdivide(Vec a, Vec b, Vec c, Vec d, double tiltA, double tiltB, double low, double high, int depth, size_t segment)
        {
            const double polygon = (b - a).Length() + (c - b).Length() + (d - c).Length();
            const Vec chord = d - a;
            const double length = chord.Length();
            const double deviation
                = length > 1e-10 ? (std::max)((b - a).Cross(chord).Length(), (c - a).Cross(chord).Length()) / length : polygon;
            if (depth == 12 || (deviation <= 0.01 && polygon - length <= 0.002))
            {
                if (samples.size() == MaxSamples)
                {
                    return false;
                }
                Sample sample;
                sample.position = d;
                sample.tangent = (d - c).Unit(chord.Unit(samples.back().tangent));
                sample.distance = samples.back().distance + (d - samples.back().position).Length();
                sample.tilt = tiltA + (tiltB - tiltA) * high;
                sample.segment = segment;
                sample.parameter = high;
                samples.push_back(sample);
                return true;
            }
            const Vec ab = (a + b) * 0.5, bc = (b + c) * 0.5, cd = (c + d) * 0.5;
            const Vec abc = (ab + bc) * 0.5, bcd = (bc + cd) * 0.5;
            const Vec middle = (abc + bcd) * 0.5;
            const double half = (low + high) * 0.5;
            return Subdivide(a, ab, abc, middle, tiltA, tiltB, low, half, depth + 1, segment)
                && Subdivide(middle, bcd, cd, d, tiltA, tiltB, half, high, depth + 1, segment);
        }

        public:
        bool Build(const std::vector<Knot>& knots, bool isClosed, Vec up)
        {
            samples.clear();
            controls = knots;
            closed = isClosed;
            if (knots.size() < 2)
            {
                return false;
            }
            const size_t segments = closed ? knots.size() : knots.size() - 1;
            samples.reserve((std::min)(MaxSamples, knots.size() * 16));
            const auto& first = knots.front();
            for (size_t segment = 0; segment < segments; ++segment)
            {
                const auto& a = knots[segment];
                const auto& b = knots[(segment + 1) % knots.size()];
                if (samples.size() == MaxSamples)
                {
                    samples.clear();
                    return false;
                }
                const double distance = samples.empty() ? 0 : samples.back().distance;
                samples.push_back({ a.position, (a.outgoing - a.position).Unit((b.position - a.position).Unit()), {}, distance,
                    a.tilt, segment, 0 });
                const double tilt = a.tilt + std::remainder(b.tilt - a.tilt, 2 * std::numbers::pi);
                if (!Subdivide(a.position, a.outgoing, b.incoming, b.position, a.tilt, tilt, 0, 1, 0, segment))
                {
                    samples.clear();
                    return false;
                }
            }
            if (Length() < 1e-8)
            {
                samples.clear();
                return false;
            }
            if ((first.outgoing - first.position).Length() < 1e-10)
            {
                for (const auto& sample : samples)
                {
                    if (sample.distance > 1e-8)
                    {
                        samples.front().tangent = (sample.position - first.position).Unit();
                        break;
                    }
                }
            }
            samples.front().normal = Normal(samples.front().tangent, up);
            for (size_t i = 1; i < samples.size(); ++i)
            {
                samples[i].normal = Transport(samples[i - 1].normal, samples[i - 1].tangent, samples[i].tangent);
            }
            if (closed)
            {
                const auto& a = samples.back();
                const auto& b = samples.front();
                const Vec normal = Transport(a.normal, a.tangent, b.tangent);
                const double correction = std::atan2(b.tangent.Dot(normal.Cross(b.normal)), normal.Dot(b.normal));
                for (auto& sample : samples)
                {
                    sample.normal = Rotate(sample.normal, sample.tangent, correction * sample.distance / Length());
                }
            }
            return true;
        }

        double Length() const
        {
            return samples.empty() ? 0 : samples.back().distance;
        }
        bool Closed() const
        {
            return closed;
        }
        const std::vector<Sample>& Samples() const
        {
            return samples;
        }
        Sample At(double progress, bool reverse = false) const
        {
            if (samples.empty())
            {
                return {};
            }
            progress = std::clamp(progress, 0.0, 1.0);
            if (reverse)
            {
                progress = 1 - progress;
            }
            const double distance = progress * Length();
            auto next = reverse ? std::lower_bound(samples.begin(), samples.end(), distance,
                                      [](const Sample& sample, double value)
                                      {
                                          return sample.distance < value;
                                      })
                                : std::upper_bound(samples.begin(), samples.end(), distance,
                                      [](double value, const Sample& sample)
                                      {
                                          return value < sample.distance;
                                      });
            Sample result;
            if (closed && reverse && progress == 0)
            {
                result = samples.back();
            }
            else if ((closed && !reverse && progress == 1) || next == samples.begin())
            {
                result = samples.front();
            }
            else if (next == samples.end())
            {
                result = samples.back();
            }
            else
            {
                const auto& before = *(next - 1);
                const double span = next->distance - before.distance;
                const double t = span > 1e-10 ? (distance - before.distance) / span : 0;
                result.segment = before.segment;
                result.parameter = before.parameter * (1 - t) + next->parameter * t;
                const auto& a = controls[result.segment];
                const auto& b = controls[(result.segment + 1) % controls.size()];
                const double v = result.parameter, u = 1 - v;
                result.position = before.position * (1 - t) + next->position * t;
                result.tangent = ((a.outgoing - a.position) * (u * u) + (b.incoming - a.outgoing) * (2 * u * v)
                    + (b.position - b.incoming) * (v * v))
                                     .Unit((b.position - a.position).Unit(before.tangent));
                const Vec normal = Transport(before.normal, before.tangent, result.tangent);
                const Vec target = Transport(next->normal, next->tangent, result.tangent);
                const double angle = std::atan2(result.tangent.Dot(normal.Cross(target)), normal.Dot(target));
                result.normal = Rotate(normal, result.tangent, angle * t);
                result.tilt = before.tilt + std::remainder(next->tilt - before.tilt, 2 * std::numbers::pi) * t;
            }
            result.distance = distance;
            result.normal = Rotate(result.normal, result.tangent, result.tilt);
            if (reverse)
            {
                result.tangent = result.tangent * -1;
            }
            return result;
        }
    };
}
