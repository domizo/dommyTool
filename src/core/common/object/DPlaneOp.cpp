//
//  DPlaneOp.cpp
//  dommyTool
//
//  Created by yuzo on 4/23/17.
//

#include "DPlaneOp.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

const float kPlaneTolerance = std::numeric_limits<float>::epsilon() * 64.0f;

bool isFinite(const Imath::V3f& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float dot(const Imath::V3f& lhs, const Imath::V3f& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float squaredLength(const Imath::V3f& value) {
    return dot(value, value);
}

Imath::V3f scaled(const Imath::V3f& value, float scale) {
    return Imath::V3f(value.x * scale, value.y * scale, value.z * scale);
}

Imath::V3f add(const Imath::V3f& lhs, const Imath::V3f& rhs) {
    return Imath::V3f(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

Imath::V3f subtract(const Imath::V3f& lhs, const Imath::V3f& rhs) {
    return Imath::V3f(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

} // namespace

DPlane::DPlane() : a(0.0f), b(0.0f), c(0.0f), d(0.0f) {}

DPlane::DPlane(float aValue, float bValue, float cValue, float dValue)
    : a(aValue), b(bValue), c(cValue), d(dValue) {}

DPlane::DPlane(const Imath::V3f& normalizedNormal, float dValue)
    : a(normalizedNormal.x), b(normalizedNormal.y), c(normalizedNormal.z), d(dValue) {}

DPlane DPlane::ConstructFromPointNormal(const Imath::V3f& point, const Imath::V3f& normal) {
    if (!isFinite(point) || !isFinite(normal)) {
        return DPlane();
    }
    const float lengthSquared = squaredLength(normal);
    if (!std::isfinite(lengthSquared) || lengthSquared <= kPlaneTolerance) {
        return DPlane();
    }

    const Imath::V3f unitNormal = scaled(normal, 1.0f / std::sqrt(lengthSquared));
    return DPlane(unitNormal, -dot(point, unitNormal));
}

DPlane DPlane::ConstructFromPointVectors(const Imath::V3f& point, const Imath::V3f& firstVector,
                                         const Imath::V3f& secondVector) {
    return ConstructFromPointNormal(point, firstVector.cross(secondVector));
}

DPlane DPlane::ConstructFromPoints(const Imath::V3f& first, const Imath::V3f& second,
                                   const Imath::V3f& third) {
    return ConstructFromPointVectors(first, subtract(second, first), subtract(third, first));
}

DPlane DPlane::Normalize() const {
    const Imath::V3f normal(a, b, c);
    const float lengthSquared = squaredLength(normal);
    if (!std::isfinite(lengthSquared) || lengthSquared <= kPlaneTolerance || !std::isfinite(d)) {
        return DPlane();
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return DPlane(a * inverseLength, b * inverseLength, c * inverseLength, d * inverseLength);
}

Imath::V3f DPlane::IntersectLine(const DLine3D& line) const {
    return IntersectLine(line.P0, add(line.P0, line.D));
}

Imath::V3f DPlane::IntersectLine(const Imath::V3f& first, const Imath::V3f& second) const {
    bool hit = false;
    return IntersectLine(first, second, hit);
}

Imath::V3f DPlane::IntersectLine(const Imath::V3f& first, const Imath::V3f& second,
                                 bool& hit) const {
    hit = false;
    const Imath::V3f normal(a, b, c);
    const Imath::V3f direction = subtract(second, first);
    if (!isFinite(first) || !isFinite(second) || !isFinite(normal) || !std::isfinite(d)) {
        return first;
    }

    const float denominator = dot(normal, direction);
    const float scale = std::max(1.0f, std::sqrt(squaredLength(normal) * squaredLength(direction)));
    if (!std::isfinite(denominator) || std::fabs(denominator) <= kPlaneTolerance * scale) {
        return first;
    }

    const float parameter = -(dot(normal, first) + d) / denominator;
    const Imath::V3f result = add(first, scaled(direction, parameter));
    if (!std::isfinite(parameter) || !isFinite(result)) {
        return first;
    }
    hit = true;
    return result;
}

float DPlane::IntersectLineRatio(const Imath::V3f& first, const Imath::V3f& second) const {
    const Imath::V3f normal(a, b, c);
    const Imath::V3f direction = subtract(second, first);
    const float denominator = dot(normal, direction);
    const float scale = std::max(1.0f, std::sqrt(squaredLength(normal) * squaredLength(direction)));
    if (!isFinite(first) || !isFinite(second) || !isFinite(normal) || !std::isfinite(d) ||
        !std::isfinite(denominator) || std::fabs(denominator) <= kPlaneTolerance * scale) {
        return 0.0f;
    }
    const float ratio = -(dot(normal, first) + d) / denominator;
    return std::isfinite(ratio) ? ratio : 0.0f;
}

float DPlane::Dot(const DPlane& plane, const Imath::V4f& value) {
    return plane.a * value.x + plane.b * value.y + plane.c * value.z + plane.d * value.w;
}

float DPlane::DotCoord(const DPlane& plane, const Imath::V3f& value) {
    return plane.a * value.x + plane.b * value.y + plane.c * value.z + plane.d;
}

float DPlane::DotNormal(const DPlane& plane, const Imath::V3f& value) {
    return plane.a * value.x + plane.b * value.y + plane.c * value.z;
}

bool DPlane::PlanePlaneIntersection(const DPlane& first, const DPlane& second, DLine3D& line) {
    const Imath::V3f firstNormal(first.a, first.b, first.c);
    const Imath::V3f secondNormal(second.a, second.b, second.c);
    if (!isFinite(firstNormal) || !isFinite(secondNormal) || !std::isfinite(first.d) ||
        !std::isfinite(second.d)) {
        return false;
    }

    const Imath::V3f direction = firstNormal.cross(secondNormal);
    const float denominator = squaredLength(direction);
    const float normalScale = squaredLength(firstNormal) * squaredLength(secondNormal);
    if (!std::isfinite(denominator) ||
        denominator <= kPlaneTolerance * std::max(1.0f, normalScale)) {
        return false;
    }

    const Imath::V3f weightedNormals(second.d * firstNormal.x - first.d * secondNormal.x,
                                     second.d * firstNormal.y - first.d * secondNormal.y,
                                     second.d * firstNormal.z - first.d * secondNormal.z);
    const Imath::V3f point = scaled(weightedNormals.cross(direction), 1.0f / denominator);
    const Imath::V3f unitDirection = scaled(direction, 1.0f / std::sqrt(denominator));
    if (!isFinite(point) || !isFinite(unitDirection)) {
        return false;
    }

    line = DLine3D(point, unitDirection);
    return true;
}

#ifdef USE_D3D
DPlane::operator D3DXPLANE() {
    return D3DXPLANE(a, b, c, d);
}
#endif
