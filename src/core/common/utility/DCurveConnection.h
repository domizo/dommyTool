#pragma once

#include "DDirectionMath.h"
#include "DGenerationLimits.h"

#include <algorithm>
#include <array>
#include <deque>
#include <type_traits>
#include <vector>

namespace dommy {
namespace curve {

struct ConnectionEndpoints {
    std::array<double, 3> points[2];
    std::array<double, 3> tangents[2];
};

struct OrientedCurve {
    std::size_t index;
    bool reversed;
};
typedef std::vector<OrientedCurve> CurveChain;

// Stable greedy chains: extend the tail, then the head, choosing the nearest
// unused endpoint. Ties use input index, then first endpoint. Each curve is
// consumed once, including cycles; open/closed output remains a node setting.
inline bool buildCurveConnectionChains(const std::vector<ConnectionEndpoints>& curves,
                                      double distance, bool checkAngle, double angle,
                                      std::vector<CurveChain>& output)
{
    std::size_t work = 0U;
    if (!std::isfinite(distance) || distance < 0.0 ||
        !std::isfinite(angle) || angle < 0.0 ||
        curves.size() > generation::kMaxGeneratedCurveCount ||
        !generation::checkedProduct(curves.size(), curves.size(),
            generation::kMaxWorkItemCount / 4U, work)) return false;
    for (const ConnectionEndpoints& curve : curves) {
        for (unsigned int end = 0; end < 2; ++end) {
            for (unsigned int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(curve.points[end][axis]) ||
                    (checkAngle && !std::isfinite(curve.tangents[end][axis]))) return false;
            }
            if (checkAngle) {
                const double length = std::hypot(std::hypot(curve.tangents[end][0],
                    curve.tangents[end][1]), curve.tangents[end][2]);
                if (!std::isfinite(length) || length <= 1.0e-12) return false;
            }
        }
    }
    std::vector<bool> used(curves.size(), false);
    std::vector<CurveChain> candidate;
    for (std::size_t seed = 0; seed < curves.size(); ++seed) {
        if (used[seed]) continue;
        std::deque<OrientedCurve> chain(1U, OrientedCurve{seed, false});
        used[seed] = true;
        for (unsigned int side = 0; side < 2; ++side) {
            const bool front = side == 1U;
            while (true) {
                const OrientedCurve source = front ? chain.front() : chain.back();
                const unsigned int sourceEnd = front ? (source.reversed ? 1U : 0U)
                                                     : (source.reversed ? 0U : 1U);
                std::size_t next = curves.size();
                unsigned int nextEnd = 0U;
                double nearest = std::numeric_limits<double>::infinity();
                for (std::size_t i = 0; i < curves.size(); ++i) {
                    if (used[i]) continue;
                    for (unsigned int end = 0; end < 2; ++end) {
                        direction::Delta delta;
                        if (!direction::difference(curves[source.index].points[sourceEnd],
                                curves[i].points[end], delta)) return false;
                        if (delta.length > distance || delta.length >= nearest) continue;
                        if (checkAngle) {
                            const auto& a = curves[source.index].tangents[sourceEnd];
                            const auto& b = curves[i].tangents[end];
                            const double aLength = std::hypot(std::hypot(a[0], a[1]), a[2]);
                            const double bLength = std::hypot(std::hypot(b[0], b[1]), b[2]);
                            double dot = 0.0;
                            for (unsigned int axis = 0; axis < 3; ++axis)
                                dot += (a[axis] / aLength) * (b[axis] / bLength);
                            // Outgoing source direction versus incoming target direction.
                            if (sourceEnd == end) dot = -dot;
                            if (!(std::acos(std::max(-1.0, std::min(1.0, dot))) < angle)) continue;
                        }
                        nearest = delta.length;
                        next = i;
                        nextEnd = end;
                    }
                }
                if (next == curves.size()) break;
                const OrientedCurve entry{next, front ? nextEnd == 0U : nextEnd == 1U};
                if (front) chain.push_front(entry);
                else chain.push_back(entry);
                used[next] = true;
            }
        }
        candidate.emplace_back(chain.begin(), chain.end());
    }
    output.swap(candidate);
    return true;
}

// Only coincident joined endpoints are duplicates. Near-but-distinct endpoints
// both survive, so increasing the connection distance does not erase geometry.
template <typename Point, typename GetPoints, typename Transform, typename Emit>
bool emitCurveChain(const CurveChain& chain, GetPoints getPoints, Transform transform, Emit emit)
{
    bool havePrevious = false;
    std::array<double, 3> previous{{0.0, 0.0, 0.0}};
    for (const OrientedCurve& entry : chain) {
        const auto& points = getPoints(entry.index);
        if (points.size() < 2U) return false;
        for (std::size_t offset = 0; offset < points.size(); ++offset) {
            Point point;
            if (!transform(entry.index,
                    points[entry.reversed ? points.size() - 1U - offset : offset], point)) return false;
            const std::array<double, 3> value{{double(point[0]), double(point[1]), double(point[2])}};
            if (!(offset == 0U && havePrevious && value == previous) && !emit(point)) return false;
            previous = value;
            havePrevious = true;
        }
    }
    return true;
}

template <typename GetPoints, typename Emit>
bool emitCurveChain(const CurveChain& chain, GetPoints getPoints, Emit emit)
{
    typedef typename std::decay<decltype(getPoints(std::size_t(0))[0])>::type Point;
    return emitCurveChain<Point>(chain, getPoints,
        [](std::size_t, const Point& source, Point& output) { output = source; return true; }, emit);
}

} // namespace curve
} // namespace dommy
