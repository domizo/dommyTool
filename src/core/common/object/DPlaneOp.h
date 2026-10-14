//
//  DPlaneOp.hpp
//  dommyTool
//
//  Created by yuzo on 4/23/17.
//
//

#pragma once

#include <ImathVec.h>

#include "DLine3DOp.h"

struct DPlane {
    //
    // Initalization
    //
    DPlane();
    DPlane(const DPlane& plane) = default;
    DPlane(float aValue, float bValue, float cValue, float dValue);
    DPlane(const Imath::V3f& normalizedNormal, float dValue);

    //
    // Static constructors
    //
    static DPlane ConstructFromPointNormal(const Imath::V3f& point, const Imath::V3f& normal);
    static DPlane ConstructFromPointVectors(const Imath::V3f& point, const Imath::V3f& firstVector,
                                            const Imath::V3f& secondVector);
    static DPlane ConstructFromPoints(const Imath::V3f& first, const Imath::V3f& second,
                                      const Imath::V3f& third);

    //
    // Line intersection
    //
    Imath::V3f IntersectLine(const Imath::V3f& first, const Imath::V3f& second) const;
    Imath::V3f IntersectLine(const Imath::V3f& first, const Imath::V3f& second, bool& hit) const;
    float IntersectLineRatio(const Imath::V3f& first, const Imath::V3f& second) const;
    Imath::V3f IntersectLine(const DLine3D& line) const;

    static float Dot(const DPlane& P, const Imath::V4f& V); // dot product of a plane and a 4D
                                                            // vector
    static float DotCoord(const DPlane& P,
                          const Imath::V3f& V); // dot product of a plane and a 3D coordinate
    static float DotNormal(const DPlane& P,
                           const Imath::V3f& V); // dot product of a plane and a 3D normal

    //
    // Normalization
    //
    DPlane Normalize() const;

    //
    // Accessors
    //
    inline Imath::V3f Normal() const {
        return Imath::V3f(a, b, c);
    }

    inline DPlane Flip() const {
        DPlane Result;
        Result.a = -a;
        Result.b = -b;
        Result.c = -c;
        Result.d = -d;
        return Result;
    }

    static bool PlanePlaneIntersection(const DPlane& P1, const DPlane& P2, DLine3D& L);

#ifdef USE_D3D
    operator D3DXPLANE();
#endif

    float a, b, c, d; // the (a, b, c, d) in a*x + b*y + c*z + d = 0.
};
