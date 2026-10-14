#pragma once

#include "DGenerationLimits.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace generation {

struct ShapeFillPlan {
    std::size_t sampleCounts[3];
    double sampleSteps[3];
    std::size_t candidateCount;

    ShapeFillPlan() : candidateCount(0U) {
        for (int axis = 0; axis < 3; ++axis) {
            sampleCounts[axis] = 0U;
            sampleSteps[axis] = 0.0;
        }
    }
};

inline bool buildShapeFillPlan(double extentX,
                               double extentY,
                               double extentZ,
                               int density,
                               ShapeFillPlan& output)
{
    if (density <= 0) {
        return false;
    }

    const double extents[3] = {extentX, extentY, extentZ};
    ShapeFillPlan plan;
    std::size_t candidateCount = 1U;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(extents[axis]) || extents[axis] < 0.0) {
            return false;
        }

        if (extents[axis] <= 1.0e-12) {
            plan.sampleCounts[axis] = 1U;
            plan.sampleSteps[axis] = 0.0;
        } else {
            std::size_t sampleCount = 0U;
            if (!memory::checkedAdd(
                    static_cast<std::size_t>(density), 1U, sampleCount)) {
                return false;
            }
            plan.sampleCounts[axis] = sampleCount;
            plan.sampleSteps[axis] = extents[axis] / density;
        }

        std::size_t nextCount = 0U;
        if (!checkedProduct(candidateCount, plan.sampleCounts[axis],
                            kMaxGeneratedPointCount, nextCount)) {
            return false;
        }
        candidateCount = nextCount;
    }

    plan.candidateCount = candidateCount;
    output = plan;
    return true;
}

} // namespace generation
} // namespace dommy
