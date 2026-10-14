#pragma once
#include <string>
// OpenVDB 5.2 public enum values and menu boundary doubles, checked against
// math/FiniteDifference.h at a79df2f32b8ebb5d15fd73325c46cee845a81e49.
namespace openvdb {
namespace math {
enum BiasedGradientScheme {
    UNKNOWN_BIAS = -1,
    FIRST_BIAS = 0,
    SECOND_BIAS,
    THIRD_BIAS,
    WENO5_BIAS,
    HJWENO5_BIAS
};
enum TemporalIntegrationScheme { UNKNOWN_TIS = -1, TVD_RK1 = 0, TVD_RK2, TVD_RK3 };
enum { NUM_TEMPORAL_SCHEMES = TVD_RK3 + 1 };
inline std::string biasedGradientSchemeToMenuName(BiasedGradientScheme s) {
    return s == FIRST_BIAS ? "1st-order biased gradient" : "5th-order HJ-WENO biased gradient";
}
inline std::string temporalIntegrationSchemeToMenuName(TemporalIntegrationScheme s) {
    const char* names[] = {"Forward Euler", "2nd-order Runge-Kutta", "3rd-order Runge-Kutta"};
    return names[static_cast<int>(s)];
}
} // namespace math
} // namespace openvdb
