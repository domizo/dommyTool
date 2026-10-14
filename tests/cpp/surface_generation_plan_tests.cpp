#include "common/utility/DGenerationLimits.h"
#include "common/utility/DAttributeLookup.h"
#include <maya/MStatus.h>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
static int admissions = 0;
#include "surface_generation_plan.inc"

using namespace dommy::generation;
using namespace dommy::curve;
int main() {
    SurfaceSamplingPlan surface;
    assert(checkedSurfaceSamplingPlan(4, 3, 8, surface));
    assert(surface.curveCount == 7 && surface.pointCount == 56);
    assert(surface.estimatedBytes == 7*2048 + 56*96 + 8*512);
    for (std::size_t samples : {2U, 10U, 1000U, 100000U}) {
        const std::size_t curves = (kMaxCurveGenerationBytes - samples*512U) / (2048U + samples*96U);
        assert(checkedSurfaceSamplingPlan(static_cast<int>(curves), 0, static_cast<int>(samples), surface));
        const auto before = surface;
        assert(!checkedSurfaceSamplingPlan(static_cast<int>(curves+1), 0, static_cast<int>(samples), surface));
        assert(surface.estimatedBytes == before.estimatedBytes && surface.curveCount == before.curveCount);
    }
    assert(!checkedSurfaceSamplingPlan(1000000, 0, 10, surface));
    assert(!checkedSurfaceSamplingPlan(INT_MAX, 1, 2, surface));
    assert(checkedSurfaceSamplingPlan(0, 0, 1000000, surface) && surface.estimatedBytes == 0);
    assert(!checkedSurfaceSamplingPlan(0, 0, 1000001, surface));

    ClothSamplingPlan cloth;
    const auto build = [&](int u, int v, bool bias) {
        return checkedClothSamplingPlan(u, v, bias, 0, 1, -2, 4,
                                        kMaxGeneratedPointCount, kMaxWorkItemCount, cloth);
    };
    assert(build(4, 5, false));
    assert(cloth.curveCountU == 6 && cloth.curveCountV == 7);
    assert(cloth.totalPointCount == 142 && cloth.totalSurfaceQueries == 200);
    assert(cloth.estimatedBytes == 13*2048 + 142*96 + 12*512);
    std::size_t withNormals = 0;
    assert(checkedClothNormalStorage(cloth, 2, withNormals));
    assert(withNormals == cloth.estimatedBytes + 142*48 + 13*512 + 13*2*4);
    auto atLimit = cloth;
    const std::size_t extra = withNormals - cloth.estimatedBytes;
    atLimit.estimatedBytes = kMaxCurveGenerationBytes - extra;
    assert(checkedClothNormalStorage(atLimit, 2, withNormals) && withNormals == kMaxCurveGenerationBytes);
    ++atLimit.estimatedBytes;
    withNormals = 23;
    assert(!checkedClothNormalStorage(atLimit, 2, withNormals) && withNormals == 23);
    assert(clothAdmission(atLimit, true, "CN") == MS::kInvalidParameter && admissions == 0);
    assert(clothAdmission(cloth, true, "") == MS::kInvalidParameter && admissions == 0);
    assert(clothAdmission(cloth, true, "CN") == MS::kSuccess && admissions == 1);
    assert(clothAdmission(cloth, false, "") == MS::kSuccess && admissions == 2);
    assert(!checkedClothNormalStorage(cloth, std::numeric_limits<std::size_t>::max(), withNormals));
    const auto before = cloth;
    assert(!build(1, 999998, false));
    assert(!build(1500, 1500, false));
    assert(!build(INT_MAX, 1, false));
    assert(cloth.estimatedBytes == before.estimatedBytes && cloth.totalPointCount == before.totalPointCount);
    assert(build(4, 5, true) && cloth.totalPointCount == 258);
    assert(!checkedClothSamplingPlan(4, 5, false, 0, 0, 0, 1, kMaxGeneratedPointCount, kMaxWorkItemCount, cloth));
    std::cout << "Surface/Cloth: count/byte/normal boundaries and transactional plans passed\n";
}
