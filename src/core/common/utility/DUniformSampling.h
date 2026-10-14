#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"

namespace dommy {
namespace generation {

// Ordered float endpoints, inclusive sampling, and transactional output.
// Compute the interval in double so even [-FLT_MAX, FLT_MAX] is safe.
// Zero samples clear output; a single sample is the lower endpoint.
inline bool generateUniformFloatSamples(float minimum, float maximum,
                                        std::size_t count, std::vector<float>& output)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum ||
        count > kMaxGeneratedPointCount) return false;
    std::vector<float> candidate(count, minimum);
    if (count > 1U) {
        const double width = static_cast<double>(maximum) - minimum;
        for (std::size_t i = 1U; i + 1U < count; ++i) {
            const double fraction = static_cast<double>(i) / static_cast<double>(count - 1U);
            const double value = static_cast<double>(minimum) + width * fraction;
            if (!numeric::checkedFloatCast(value, candidate[i])) return false;
        }
        candidate.back() = maximum;
    }
    output.swap(candidate);
    return true;
}

} // namespace generation
} // namespace dommy
