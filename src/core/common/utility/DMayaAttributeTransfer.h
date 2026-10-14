#pragma once

#include "DAttributeTransferPlan.h"

#include <maya/MRampAttribute.h>
#include <maya/MStatus.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace dommy {
namespace maya {

typedef std::vector<std::vector<double> > AttributeTransferFactors;

template <typename DistanceLists>
inline MStatus sampleAttributeTransferFactors(
    const DistanceLists& distances,
    const transfer::AttributeTransferPlan& plan,
    MRampAttribute& ramp,
    AttributeTransferFactors& factors)
{
    AttributeTransferFactors candidate(distances.size());
    const double valueRange =
        plan.valueMaximum - plan.valueMinimum;
    for (std::size_t target = 0U;
         target < distances.size(); ++target) {
        candidate[target].reserve(distances[target].size());
        for (std::size_t neighbor = 0U;
             neighbor < distances[target].size(); ++neighbor) {
            const double normalizedDistance =
                static_cast<double>(distances[target][neighbor]) /
                plan.distance;
            if (!std::isfinite(normalizedDistance)) {
                return MS::kInvalidParameter;
            }

            float rampValue = 0.0f;
            MStatus rampStatus;
            ramp.getValueAtPosition(
                static_cast<float>(normalizedDistance),
                rampValue,
                &rampStatus);
            if (rampStatus != MS::kSuccess ||
                !std::isfinite(static_cast<double>(rampValue))) {
                return rampStatus == MS::kSuccess
                    ? MStatus(MS::kInvalidParameter)
                    : rampStatus;
            }

            const double factor =
                (plan.valueMinimum +
                 valueRange * static_cast<double>(rampValue)) *
                plan.scale;
            if (!std::isfinite(factor)) {
                return MS::kInvalidParameter;
            }
            candidate[target].push_back(factor);
        }
    }

    factors.swap(candidate);
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy
