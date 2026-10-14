//
//  DLine3DOp.cpp
//  dommyTool
//
//  Created by yuzo on 4/23/17.
//

#include "DLine3DOp.h"

#include <cmath>
#include <limits>

namespace {

const float kDirectionTolerance = std::numeric_limits<float>::epsilon() * 64.0f;

bool isFinite(const Imath::V3f& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float dot(const Imath::V3f& lhs, const Imath::V3f& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float squaredLength(const Imath::V3f& value) {
    return dot(value, value);
}

float squaredPointDistance(const Imath::V3f& lhs, const Imath::V3f& rhs) {
    return squaredLength(Imath::V3f(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z));
}

float squaredPointLineDistance(const Imath::V3f& point, const Imath::V3f& origin,
                               const Imath::V3f& direction) {
    const float directionLengthSquared = squaredLength(direction);
    if (directionLengthSquared <= kDirectionTolerance) {
        return squaredPointDistance(point, origin);
    }

    const Imath::V3f offset(point.x - origin.x, point.y - origin.y, point.z - origin.z);
    const float parameter = dot(offset, direction) / directionLengthSquared;
    const Imath::V3f projected(origin.x + direction.x * parameter,
                               origin.y + direction.y * parameter,
                               origin.z + direction.z * parameter);
    return squaredPointDistance(point, projected);
}

} // namespace

bool DLine3D::ClosestPoint(const DLine3D& first, const DLine3D& second, Imath::V3f& firstResult,
                           Imath::V3f& secondResult) {
    if (!isFinite(first.P0) || !isFinite(first.D) || !isFinite(second.P0) || !isFinite(second.D)) {
        return false;
    }

    const float a = squaredLength(first.D);
    const float b = dot(first.D, second.D);
    const float c = squaredLength(second.D);
    if (a <= kDirectionTolerance || c <= kDirectionTolerance) {
        return false;
    }

    const Imath::V3f offset(first.P0.x - second.P0.x, first.P0.y - second.P0.y,
                            first.P0.z - second.P0.z);
    const float d = dot(first.D, offset);
    const float e = dot(second.D, offset);
    const float denominator = a * c - b * b;
    if (!std::isfinite(denominator) || std::fabs(denominator) <= kDirectionTolerance * a * c) {
        return false;
    }

    const float firstParameter = (b * e - c * d) / denominator;
    const float secondParameter = (a * e - b * d) / denominator;
    const Imath::V3f candidateFirst(first.P0.x + first.D.x * firstParameter,
                                    first.P0.y + first.D.y * firstParameter,
                                    first.P0.z + first.D.z * firstParameter);
    const Imath::V3f candidateSecond(second.P0.x + second.D.x * secondParameter,
                                     second.P0.y + second.D.y * secondParameter,
                                     second.P0.z + second.D.z * secondParameter);
    if (!isFinite(candidateFirst) || !isFinite(candidateSecond)) {
        return false;
    }

    firstResult = candidateFirst;
    secondResult = candidateSecond;
    return true;
}

float DLine3D::Dist(const DLine3D& first, const DLine3D& second) {
    const float distanceSquared = DistSq(first, second);
    return std::sqrt(distanceSquared);
}

float DLine3D::DistSq(const DLine3D& first, const DLine3D& second) {
    if (!isFinite(first.P0) || !isFinite(first.D) || !isFinite(second.P0) || !isFinite(second.D)) {
        return std::numeric_limits<float>::infinity();
    }

    const Imath::V3f cross = first.D.cross(second.D);
    const float crossLengthSquared = squaredLength(cross);
    const Imath::V3f offset(second.P0.x - first.P0.x, second.P0.y - first.P0.y,
                            second.P0.z - first.P0.z);
    if (crossLengthSquared > kDirectionTolerance) {
        const float separation = dot(offset, cross);
        return (separation * separation) / crossLengthSquared;
    }

    if (squaredLength(first.D) > kDirectionTolerance) {
        return squaredPointLineDistance(second.P0, first.P0, first.D);
    }
    if (squaredLength(second.D) > kDirectionTolerance) {
        return squaredPointLineDistance(first.P0, second.P0, second.D);
    }
    return squaredPointDistance(first.P0, second.P0);
}

float DLine3D::DistToPoint(const Imath::V3f& point) const {
    if (!isFinite(P0) || !isFinite(D) || !isFinite(point)) {
        return std::numeric_limits<float>::infinity();
    }
    return std::sqrt(squaredPointLineDistance(point, P0, D));
}
