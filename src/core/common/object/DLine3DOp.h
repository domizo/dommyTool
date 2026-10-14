//
//  DLine3DOp.hpp
//  dommyTool
//
//  Created by yuzo on 4/23/17.
//
//

#pragma once

#include <ImathVec.h>

struct DLine3D {
    DLine3D() : P0(0.0f, 0.0f, 0.0f), D(0.0f, 0.0f, 0.0f) {}

    DLine3D(const Imath::V3f& origin, const Imath::V3f& direction) : P0(origin), D(direction) {}

    float DistToPoint(const Imath::V3f& point) const;

    static bool ClosestPoint(const DLine3D& first, const DLine3D& second, Imath::V3f& firstResult,
                             Imath::V3f& secondResult);
    static float Dist(const DLine3D& first, const DLine3D& second);
    static float DistSq(const DLine3D& first, const DLine3D& second);

    Imath::V3f P0;
    Imath::V3f D;
};
