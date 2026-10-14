#include <algorithm>
#include <array>
#include <cassert>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "DBounds.h"
#include "DIndexOrder.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DFileSize.h"
#include "common/file/DLegacyTextData.h"
#include "common/file/DMelString.h"
#include "common/file/DSequencePath.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DBatchRange.h"
#include "common/utility/DColorMode.h"
#include "common/utility/DCurveUVMapping.h"
#include "common/utility/DAttributeTransferPlan.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DAttributeMathInput.h"
#include "math/DMathArrayConversion.h"
#include "math/DUnaryMath.h"
#include "math/DVectorNodeMath.h"
#include "common/utility/DAttributeLookup.h"
#include "common/utility/DAttributeValueConversion.h"
#include "math/DProceduralMath.h"
#include "common/utility/DAhoGenerationPlan.h"
#include "common/utility/DVectorLookup.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DEyelashGeneration.h"
#include "common/utility/DFillSpacePlan.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DLeafDeformation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaPointBlend.h"
#include "maya_point_blend_cases.h"
#include "common/utility/DGoalMath.h"
#include "common/utility/DGroupSelection.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DMotionGenerationPlan.h"
#include "common/utility/DShapeFillPlan.h"
#include "common/utility/DSignalMath.h"
#include "common/utility/DSpatialSearchPlan.h"
#include "common/utility/DSubdivisionBudget.h"
#include "common/utility/DUvParameter.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DViewportPointBudget.h"
#include "common/utility/DWireTopologyPlan.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "common/utility/DMeshSelection.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DMeshUVTransfer.h"
#include "common/utility/DPointMean.h"
#include "common/utility/DSymmetryPointMatcher.h"
#include "common/utility/DParticleFieldMath.h"
#include "common/utility/DParallelWork.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DProceduralMeshPlans.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/object/DLine3DOp.h"
#include "common/object/DPlaneOp.h"
#include "PoissonGenerator.h"
#include "pdbIO.h"
#include "dommyMath.h"
#include "math/DRemapMath.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include "modules/volume/VDB/Common/DVdbViewportMath.h"
#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DAbcSamplingPlan.h"
#include "common/utility/DFrameSampling.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DPlugHandleOwner.h"
#include "common/utility/DStatusOperation.h"
#include "math/DMathEvaluation.h"
#include "modules/volume/VDB/Common/DVdbCombinePlan.h"
#include "modules/volume/VDB/Common/DVdbParticleRasterPlan.h"
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"
#include "modules/dynamics/DBullet/Collision/DMassProperties.h"
#include "modules/dynamics/DBullet/RigidBody/DBRigidBodyParameters.h"
#include "modules/dynamics/DBullet/Solver/DPassiveTransformMath.h"
#include "modules/dynamics/DBullet/shared_ptr.h"
#include "DPoint/math/DNumeric.h"
#include "DPoint/math/DVectorMath.h"
#include "Utility/3d/Math/DVec.h"
#include "Utility/3d/DataStruct/DHashGrid.h"
#include "DDataUtility/Data/DPAttributeData.h"
#include "DDataUtility/Generate/DPointPackingCommon.h"
#include "DCurve/IO/DCurveArnoldLayout.h"
#include "DCurve/Generate/DCurveClothSamplingPlan.h"
#include "DCurve/Generate/DTextGenerationPlan.h"
#include "DCurve/Utility/DCurveDividePlan.h"
#include "DCurve/Utility/DCurveCopyBudget.h"
#include "DCurve/Utility/DCurveDuplicatePlan.h"
#include "DCurve/Generate/DCurveFromCurvePlan.h"
#include "modules/curve/legacy/DWoolCurvePlan.h"
#include "DPoint/mesh/DMeshDuplicationPlan.h"
#include "noise/PerlinNoise.h"
#include "particle/field/DCurveFieldMath.h"
#include "particle/field/DKillFieldMath.h"
#include "registration/PluginRegistration.h"
#include "modules/generators/icosahedron/DIcosahedronTopology.h"
#include "DRandomGenerator.h"

const char* poissonVersionFromSecondTranslationUnit();
void runUtilityVectorTests();
void runAtomicOutputTests();
void runIoLocaleTests();
MStatus compileVdbComputeExceptionBoundary(bool fail);
MStatus compileComputeExceptionBoundary(bool fail);
MStatus compileDeformerExceptionBoundary(bool fail);
bool compileMayaStateGuards();
void runGeometryTests();
void runAttributeCastGroupTests();
void runExpressionSortTests();
void runPointGenerationTests();
void runRandomSamplingTests();
void runBlueNoiseTests();
void runAttributeAccessTests();
void runPointStorageTests();
void runCurveSamplingTests();
void runKillFieldNumericTests();

namespace {
struct TestCollisionVec3 {
    double values[3];

    double operator[](int axis) const { return values[axis]; }
};

struct TestCollisionQuat {
    double values[4];

    double operator[](int component) const { return values[component]; }
};

bool near(double lhs, double rhs, double tolerance = 1.0e-6) {
    return std::fabs(lhs - rhs) <= tolerance;
}

bool failNextArrayAllocation = false;

class TestStatusArray {
public:
    unsigned int length() const {
        return static_cast<unsigned int>(values_.size());
    }

    MStatus append(int value) {
        if (value < 0) return MS::kFailure;
        values_.push_back(value);
        return MS::kSuccess;
    }

    MStatus setLength(unsigned int length) {
        if (failNextArrayAllocation) {
            failNextArrayAllocation = false;
            return MS::kFailure;
        }
        values_.resize(length);
        return MS::kSuccess;
    }

    int& operator[](unsigned int index) { return values_[index]; }
    const int& operator[](unsigned int index) const {
        return values_[index];
    }

private:
    std::vector<int> values_;
};

void testCheckedMayaArray() {
    dommy::maya::CheckedArray<TestStatusArray> values;
    assert(values.append(3) == MS::kSuccess);
    assert(values.length() == 1U && values[0U] == 3);
    assert(values.setLength(3U) == MS::kSuccess);
    values[2U] = 7;
    const TestStatusArray& raw = values;
    assert(raw.length() == 3U && raw[2U] == 7);

    bool failed = false;
    try {
        values.append(-1);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);

    dommy::maya::CheckedArray<TestStatusArray> bounded(1U);
    assert(bounded.append(5) == MS::kSuccess);
    failed = false;
    try { bounded.append(6); }
    catch (const std::length_error&) { failed = true; }
    assert(failed && bounded.length() == 1U && bounded[0] == 5);
    failed = false;
    try { bounded.setLength(2U); }
    catch (const std::length_error&) { failed = true; }
    assert(failed && bounded.length() == 1U && bounded[0] == 5);

    // Planned trail/output allocation must propagate Maya's status failure.
    dommy::maya::CheckedArray<TestStatusArray> planned(6U);
    failNextArrayAllocation = true;
    failed = false;
    try { planned.setLength(6U); }
    catch (const std::runtime_error&) { failed = true; }
    assert(failed && planned.length() == 0U);
    planned.setLength(6U);
    planned[0U] = 11;
    failNextArrayAllocation = true;
    failed = false;
    try { planned.setLength(3U); }
    catch (const std::runtime_error&) { failed = true; }
    assert(failed && planned.length() == 6U && planned[0U] == 11);
}

template <typename T>
struct TestUVArray {
    std::vector<T> values;
    bool failCopy = false;
    bool shortCopy = false;
    unsigned int copyCalls = 0U;

    TestUVArray(std::initializer_list<T> initial = {}) : values(initial) {}
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    const T& operator[](unsigned int index) const { return values.at(index); }
    MStatus copy(const TestUVArray& source) {
        ++copyCalls;
        if (failCopy) return MS::kFailure;
        values = source.values;
        if (shortCopy && !values.empty()) values.pop_back();
        return MS::kSuccess;
    }
};

void testMeshUVTransferAndPointMean() {
    using dommy::mesh::copyMeshUVArrays;
    using dommy::mesh::planMeshUVTransfer;
    using dommy::mesh::validFullMeshUVArrays;
    const TestUVArray<float> sourceU{0.0f, 1.0f, 0.0f};
    const TestUVArray<float> sourceV{0.0f, 0.0f, 1.0f};
    const TestUVArray<int> counts{3};
    const TestUVArray<int> ids{0, 1, 2};
    for (unsigned int failure = 0U; failure <= 4U; ++failure) {
        TestUVArray<float> u{-1.0f}, v{-1.0f};
        TestUVArray<int> copiedCounts{-1}, copiedIds{-1};
        u.failCopy = failure == 0U;
        v.failCopy = failure == 1U;
        copiedCounts.failCopy = failure == 2U;
        copiedIds.failCopy = failure == 3U;
        const MStatus status = copyMeshUVArrays(
            sourceU, sourceV, counts, ids, u, v, copiedCounts, copiedIds);
        assert((status == MS::kSuccess) == (failure == 4U));
        assert(u.copyCalls == 1U);
        assert(v.copyCalls == (failure >= 1U ? 1U : 0U));
        assert(copiedCounts.copyCalls == (failure >= 2U ? 1U : 0U));
        assert(copiedIds.copyCalls == (failure >= 3U ? 1U : 0U));
        if (failure == 1U) assert(u.length() == 3U && v.length() == 1U);
        if (failure == 4U) assert(validFullMeshUVArrays(counts, u, v, copiedCounts, copiedIds));
    }
    assert(validFullMeshUVArrays(counts, sourceU, sourceV, counts, ids));
    assert(!validFullMeshUVArrays(TestUVArray<int>{4}, sourceU, sourceV, counts, ids));
    assert(!validFullMeshUVArrays(counts, sourceU, TestUVArray<float>{0}, counts, ids));
    assert(!validFullMeshUVArrays(counts, sourceU, sourceV, TestUVArray<int>{0}, ids));
    assert(!validFullMeshUVArrays(counts, sourceU, sourceV, TestUVArray<int>{-3}, ids));
    assert(!validFullMeshUVArrays(counts, sourceU, sourceV, counts, TestUVArray<int>{0, 1}));
    assert(!validFullMeshUVArrays(counts, sourceU, sourceV, counts, TestUVArray<int>{-1, 1, 2}));
    assert(!validFullMeshUVArrays(counts, sourceU, sourceV, counts, TestUVArray<int>{0, 1, 3}));
    assert(!validFullMeshUVArrays(counts,
        TestUVArray<float>{0, 1, std::numeric_limits<float>::infinity()}, sourceV, counts, ids));
    assert(validFullMeshUVArrays(counts, TestUVArray<float>{}, TestUVArray<float>{},
        TestUVArray<int>{}, TestUVArray<int>{}));
    // A collapsed UV shell remains legal data; displacement direction must
    // not index a hypothetical second UV in this one-element shell.
    assert(validFullMeshUVArrays(counts, TestUVArray<float>{0}, TestUVArray<float>{0},
        counts, TestUVArray<int>{0, 0, 0}));

    std::vector<dommy::mesh::UVTransferSlice> slices;
    const TestUVArray<int> twoFaces{3, 4};
    assert(planMeshUVTransfer({{3}, {4}}, twoFaces, 7U, 7U, slices));
    assert(slices.size() == 2U && slices[0].faceOffset == 0U &&
        slices[0].connectionCount == 3U && slices[1].faceOffset == 1U &&
        slices[1].connectionOffset == 3U && slices[1].connectionCount == 4U);
    assert(!planMeshUVTransfer({{3}, {3}}, twoFaces, 7U, 7U, slices));
    assert(slices.size() == 2U && slices[1].connectionCount == 4U);
    assert(!planMeshUVTransfer({{3}}, twoFaces, 7U, 7U, slices));
    assert(!planMeshUVTransfer({{3}, {4}, {3}}, twoFaces, 7U, 7U, slices));
    assert(!planMeshUVTransfer({{3}, {4}}, TestUVArray<int>{3, 0}, 3U, 3U, slices));
    assert(!planMeshUVTransfer({{3}, {4}}, TestUVArray<int>{3, -4}, 7U, 7U, slices));
    assert(!planMeshUVTransfer({{3}, {4}}, twoFaces, 6U, 7U, slices));
    assert(!planMeshUVTransfer({{3}, {4}}, twoFaces,
        dommy::generation::kMaxWorkItemCount, 7U, slices));
    assert(!planMeshUVTransfer({{3}, {4}}, twoFaces, 7U,
        std::numeric_limits<std::size_t>::max(), slices));
    assert(planMeshUVTransfer({{3, 4}}, twoFaces, 7U, 7U, slices));
    assert(slices.size() == 1U && slices[0].connectionCount == 7U);

    using Point = std::array<float, 3>;
    std::vector<Point> points(1000000U, Point{{10000.0f, -10000.0f, 3.0f}});
    std::array<double, 3> center;
    assert(dommy::numeric::checkedPointMean(points, center));
    assert(center[0] == 10000.0 && center[1] == -10000.0 && center[2] == 3.0);
    points.assign(3U, Point{{2.0e38f, 0, 0}});
    assert(dommy::numeric::checkedPointMean(points, center));
    assert(center[0] == static_cast<double>(2.0e38f));
    points = {Point{{1.0e20f, 0, 0}}, Point{{3, 0, 0}}, Point{{-1.0e20f, 0, 0}}};
    assert(dommy::numeric::checkedPointMean(points, center) && center[0] == 1.0);
    const std::array<double, 3> saved = center;
    points.clear();
    assert(!dommy::numeric::checkedPointMean(points, center) && center == saved);
    points = {Point{{std::numeric_limits<float>::quiet_NaN(), 0, 0}}};
    assert(!dommy::numeric::checkedPointMean(points, center) && center == saved);
    points[0][0] = std::numeric_limits<float>::infinity();
    assert(!dommy::numeric::checkedPointMean(points, center) && center == saved);
    points = {Point{{-2, 4, 0.5f}}};
    assert(dommy::numeric::checkedPointMean(points, center));
    assert(center[0] == -2 && center[1] == 4 && center[2] == 0.5);
}

struct TestEditableGeometry {
    std::vector<int> points;
    bool allowCommit;

    std::vector<int> getPositionConst() const {
        return points;
    }

    bool replacePositions(const std::vector<int>& replacement) {
        if (!allowCommit || replacement.size() != points.size()) {
            return false;
        }
        points = replacement;
        return true;
    }
};

void testStagedPositionEdits() {
    TestEditableGeometry first = {{1, 2}, true};
    TestEditableGeometry second = {{3, 4}, true};
    dommy::geometry::StagedPositionEdits<TestEditableGeometry> edits;
    assert(edits.reserve(2U));
    assert(edits.add(first));
    assert(edits.add(second));

    std::vector<std::vector<int>*> positions;
    assert(edits.mutablePositionPointers(positions));
    assert(positions.size() == 2U);
    (*positions[0])[0] = 10;
    (*positions[1])[1] = 40;
    assert(first.points[0] == 1);
    assert(second.points[1] == 4);
    assert(edits.commit());
    assert(first.points[0] == 10);
    assert(second.points[1] == 40);

    TestEditableGeometry rejected = {{5}, false};
    dommy::geometry::StagedPositionEdits<TestEditableGeometry> rejectedEdit;
    assert(rejectedEdit.add(rejected));
    rejectedEdit.positions(0U)[0] = 50;
    assert(!rejectedEdit.commit());
    assert(rejected.points[0] == 5);
}

void testCurveSamplingMath() {
    double x = 0.0;
    double y = 0.0;
    assert(!dommy::generation::orderedUnitCircleOffset(0U, 0U, x, y));
    assert(!dommy::generation::orderedUnitCircleOffset(4U, 4U, x, y));
    assert(!dommy::generation::unitCircleOffset(-0.1, x, y));
    assert(!dommy::generation::unitCircleOffset(1.0, x, y));
    assert(!dommy::generation::unitCircleOffset(
        std::numeric_limits<double>::quiet_NaN(), x, y));

    assert(dommy::generation::orderedUnitCircleOffset(0U, 4U, x, y));
    assert(near(x, 1.0));
    assert(near(y, 0.0));
    assert(dommy::generation::orderedUnitCircleOffset(1U, 4U, x, y));
    assert(near(x, 0.0));
    assert(near(y, 1.0));
    assert(dommy::generation::orderedUnitCircleOffset(2U, 4U, x, y));
    assert(near(x, -1.0));
    assert(near(y, 0.0));
    assert(dommy::generation::orderedUnitCircleOffset(3U, 4U, x, y));
    assert(near(x, 0.0));
    assert(near(y, -1.0));
}

void testBoundsReduction() {
    dommy::math::Bounds3d bounds;
    assert(bounds.empty());
    assert(!bounds.add(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    assert(!bounds.add(
        0.0, std::numeric_limits<double>::infinity(), 0.0));
    assert(bounds.empty());
    assert(bounds.add(4.0, 5.0, 6.0));
    assert(bounds.count() == 1);
    assert(bounds.minimum(0) == 4.0);
    assert(bounds.maximum(0) == 4.0);
    assert(bounds.add(-8.0, -3.0, -5.0));
    assert(bounds.add(-2.0, -7.0, -1.0));
    assert(bounds.count() == 3);
    assert(bounds.minimum(0) == -8.0);
    assert(bounds.minimum(1) == -7.0);
    assert(bounds.minimum(2) == -5.0);
    assert(bounds.maximum(0) == 4.0);
    assert(bounds.maximum(1) == 5.0);
    assert(bounds.maximum(2) == 6.0);
    const std::vector<std::array<double, 3>> points = {
        {{4, 5, 6}}, {{-8, -3, -5}}, {{-2, -7, -1}}};
    std::array<double, 3> mean;
    assert(dommy::numeric::checkedPointMean(points, mean));
    assert(near(mean[0], -2.0));
    assert(near(mean[1], -5.0 / 3.0));
    assert(near(mean[2], 0.0));

    dommy::math::Bounds3d huge;
    std::feclearexcept(FE_ALL_EXCEPT);
    for (double value : {1e308, 1e308, -1e308, -1e308})
        assert(huge.add(value, -value, value));
    assert(huge.count() == 4U && huge.minimum(0) == -1e308 && huge.maximum(0) == 1e308);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testPassiveTransformInterpolation() {
    float fraction = -1.0F;
    assert(dommy::bullet::normalizedSubstepFraction(0, 4, fraction));
    assert(near(fraction, 0.0));
    assert(dommy::bullet::normalizedSubstepFraction(1, 4, fraction));
    assert(near(fraction, 0.25));
    assert(dommy::bullet::normalizedSubstepFraction(4, 4, fraction));
    assert(near(fraction, 1.0));
    assert(!dommy::bullet::normalizedSubstepFraction(1, 0, fraction));
    assert(!dommy::bullet::normalizedSubstepFraction(5, 4, fraction));

    const vec3f startPosition(0.0F, 2.0F, 4.0F);
    const vec3f endPosition(10.0F, 6.0F, 0.0F);
    const quatf identity(1.0F, 0.0F, 0.0F, 0.0F);
    const quatf antipodalIdentity(-1.0F, 0.0F, 0.0F, 0.0F);
    vec3f outputPosition;
    quatf outputRotation;

    assert(dommy::bullet::interpolatePassiveTransform(
        startPosition, endPosition, identity, antipodalIdentity,
        0.5F, outputPosition, outputRotation));
    assert(near(outputPosition[0], 5.0));
    assert(near(outputPosition[1], 4.0));
    assert(near(outputPosition[2], 2.0));
    assert(near(outputRotation[0], 1.0));
    assert(near(outputRotation[1], 0.0));
    assert(near(outputRotation[2], 0.0));
    assert(near(outputRotation[3], 0.0));

    const float halfAngle = static_cast<float>(
        3.14159265358979323846 / 4.0);
    const quatf halfTurnAroundZ(
        std::cos(halfAngle), 0.0F, 0.0F, std::sin(halfAngle));
    assert(dommy::bullet::interpolatePassiveTransform(
        startPosition, endPosition, identity, halfTurnAroundZ,
        0.5F, outputPosition, outputRotation));
    const float expected = std::sqrt(0.5F + 0.5F * std::sqrt(0.5F));
    const float expectedZ = std::sqrt(0.5F - 0.5F * std::sqrt(0.5F));
    assert(near(outputRotation[0], expected, 1.0e-5));
    assert(near(outputRotation[3], expectedZ, 1.0e-5));

    const quatf zeroRotation(0.0F, 0.0F, 0.0F, 0.0F);
    assert(!dommy::bullet::interpolatePassiveTransform(
        startPosition, endPosition, zeroRotation, identity,
        0.5F, outputPosition, outputRotation));
    assert(!dommy::bullet::interpolatePassiveTransform(
        startPosition, endPosition, identity, identity,
        1.5F, outputPosition, outputRotation));
}

void testLineAndPlaneMath() {
    const DLine3D xAxis(
        Imath::V3f(0.0f, 0.0f, 0.0f),
        Imath::V3f(1.0f, 0.0f, 0.0f));
    const DLine3D zOffset(
        Imath::V3f(0.0f, 0.0f, 2.0f),
        Imath::V3f(0.0f, 1.0f, 0.0f));
    assert(near(DLine3D::Dist(xAxis, zOffset), 2.0));
    assert(near(xAxis.DistToPoint(Imath::V3f(0.0f, 3.0f, 4.0f)), 5.0));

    const DLine3D parallel(
        Imath::V3f(0.0f, 3.0f, 0.0f),
        Imath::V3f(2.0f, 0.0f, 0.0f));
    assert(near(DLine3D::Dist(xAxis, parallel), 3.0));

    const DLine3D pointLine(
        Imath::V3f(1.0f, 2.0f, 3.0f),
        Imath::V3f(0.0f, 0.0f, 0.0f));
    assert(near(
        pointLine.DistToPoint(Imath::V3f(1.0f, 2.0f, 7.0f)), 4.0));

    Imath::V3f firstClosest;
    Imath::V3f secondClosest;
    assert(DLine3D::ClosestPoint(
        xAxis,
        DLine3D(
            Imath::V3f(2.0f, -3.0f, 0.0f),
            Imath::V3f(0.0f, 1.0f, 0.0f)),
        firstClosest,
        secondClosest));
    assert(near(firstClosest.x, 2.0));
    assert(near(firstClosest.y, 0.0));
    assert(near(secondClosest.x, 2.0));
    assert(near(secondClosest.y, 0.0));
    assert(!DLine3D::ClosestPoint(
        xAxis, parallel, firstClosest, secondClosest));

    const DPlane xPlane = DPlane::ConstructFromPointNormal(
        Imath::V3f(1.0f, 0.0f, 0.0f),
        Imath::V3f(1.0f, 0.0f, 0.0f));
    const DPlane zPlane = DPlane::ConstructFromPointNormal(
        Imath::V3f(0.0f, 0.0f, 2.0f),
        Imath::V3f(0.0f, 0.0f, 1.0f));
    DLine3D intersection;
    assert(DPlane::PlanePlaneIntersection(xPlane, zPlane, intersection));
    assert(near(intersection.P0.x, 1.0));
    assert(near(intersection.P0.z, 2.0));

    bool hit = false;
    const Imath::V3f intersectionPoint = xPlane.IntersectLine(
        Imath::V3f(0.0f, 4.0f, 0.0f),
        Imath::V3f(2.0f, 4.0f, 0.0f),
        hit);
    assert(hit);
    assert(near(intersectionPoint.x, 1.0));

    const DPlane invalid = DPlane::ConstructFromPointNormal(
        Imath::V3f(0.0f, 0.0f, 0.0f),
        Imath::V3f(0.0f, 0.0f, 0.0f));
    assert(invalid.a == 0.0f && invalid.b == 0.0f &&
           invalid.c == 0.0f && invalid.d == 0.0f);
}

void testCheckedSizeArithmetic() {
    std::uint64_t wide = UINT64_C(5000000000);
    assert(dommy::memory::checkedUnsignedAdd(wide, UINT64_C(1), wide));
    assert(wide == UINT64_C(5000000001));
    const auto savedWide = wide;
    assert(!dommy::memory::checkedUnsignedAdd(wide, std::numeric_limits<std::uint64_t>::max(), wide));
    assert(wide == savedWide);
    std::size_t result = 0;
    assert(dommy::memory::checkedAdd(12, 4, result));
    assert(result == 16);
    assert(dommy::memory::checkedMultiply(12, 4, result));
    assert(result == 48);
    assert(dommy::memory::checkedProduct(3, 4, 5, result));
    assert(result == 60);

    result = 17;
    assert(!dommy::memory::checkedAdd(
        std::numeric_limits<std::size_t>::max(), 1, result));
    assert(result == 17);
    assert(!dommy::memory::checkedMultiply(
        std::numeric_limits<std::size_t>::max(), 2, result));
    assert(result == 17);
    assert(!dommy::memory::checkedProduct(
        std::numeric_limits<std::size_t>::max(), 2, 3, result));

    const std::vector<int> emptyValues;
    assert(dommy::memory::dataOrNull(emptyValues) == NULL);
    const std::vector<int> values(2, 7);
    assert(dommy::memory::dataOrNull(values) == &values[0]);
}

void testVectorLookup() {
    const std::vector<int> values{5, 8, 13};
    std::size_t index = 99U;
    assert(dommy::container::findIndex(values, 8, index));
    assert(index == 1U);
    assert(!dommy::container::findIndex(values, 21, index));
    assert(index == 1U);
}

void testSpatialSearchPlan() {
    dommy::spatial::SpatialSearchPlan plan;
    assert(dommy::spatial::buildSpatialSearchPlan(
        12U, 8U, 5, 3, 2.0, plan));
    assert(plan.searchPointCount == 12U);
    assert(plan.queryPointCount == 8U);
    assert(plan.maximumNeighbors == 5U);
    assert(plan.maximumConnections == 3U);
    assert(plan.workItemBudget == 40U);
    assert(plan.outputPairBudget == 24U);

    assert(!dommy::spatial::buildSpatialSearchPlan(
        12U, 8U, 0, 3, 2.0, plan));
    assert(!dommy::spatial::buildSpatialSearchPlan(
        12U, 8U, 5, 0, 2.0, plan));
    assert(!dommy::spatial::buildSpatialSearchPlan(
        12U, 8U, 5, 3, 0.0, plan));
    assert(!dommy::spatial::buildSpatialSearchPlan(
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedPointCount,
        100, 1, 2.0, plan));

    assert(dommy::spatial::buildSpatialSearchPlan(
        3U, 2U, 2, 1, 1.0, plan));
    std::vector<std::vector<std::size_t> > ids(2U);
    std::vector<std::vector<double> > distances(2U);
    ids[0].push_back(0U);
    ids[0].push_back(1U);
    distances[0].push_back(0.0);
    distances[0].push_back(0.25);
    ids[1].push_back(2U);
    distances[1].push_back(0.5);
    assert(dommy::spatial::validateSpatialNeighborLists(
        ids, distances, plan));

    ids[1][0] = 3U;
    assert(!dommy::spatial::validateSpatialNeighborLists(
        ids, distances, plan));
    ids[1][0] = 2U;
    distances[1][0] = std::numeric_limits<double>::quiet_NaN();
    assert(!dommy::spatial::validateSpatialNeighborLists(
        ids, distances, plan));
}

void testTextGenerationPlan() {
    dommy::curve::TextGenerationPlan plan;
    assert(dommy::curve::buildTextGenerationPlan(
        12U, 32U, 8U, 2.0, 4, 0.5, -0.25, plan));
    assert(plan.sampling == 4U);
    assert(plan.fontSize == 2.0);
    assert(!dommy::curve::buildTextGenerationPlan(
        dommy::curve::kMaxTextInputBytes + 1U,
        32U, 8U, 2.0, 4, 0.5, -0.25, plan));
    assert(!dommy::curve::buildTextGenerationPlan(
        12U, 32U, 8U, 0.0, 4, 0.5, -0.25, plan));
    assert(!dommy::curve::buildTextGenerationPlan(
        12U, 32U, 8U, 2.0, 0, 0.5, -0.25, plan));

    std::size_t curves = 0U;
    std::size_t points = 0U;
    assert(dommy::curve::reserveTextOutput(
        2U, 10U, 1U, 8U, curves, points));
    assert(curves == 3U && points == 18U);
    assert(!dommy::curve::reserveTextOutput(
        dommy::generation::kMaxGeneratedCurveCount,
        10U, 1U, 8U, curves, points));
}

void testExportPathValidation() {
    assert(dommy::file::isSafePathSyntax("/tmp/cache/output.any"));
    assert(!dommy::file::isSafePathSyntax("../cache/output.any"));
    assert(dommy::file::isSafeExportPath(
        "/tmp/cache/volume.0001.vdb", ".vdb"));
    assert(dommy::file::isSafeExportPath(
        "cache/volume.VDB", ".vdb"));
    assert(dommy::file::isSafeExportPath(
        "/tmp/cache/points.ass", ".ass"));
    assert(dommy::file::isSafeExportPath(
        "/tmp/cache/bodies.pdb", ".pdb"));
    assert(!dommy::file::isSafeExportPath(
        "cache/../volume.vdb", ".vdb"));
    assert(!dommy::file::isSafeExportPath(
        "cache/volume.exr", ".vdb"));
    assert(!dommy::file::isSafeExportPath(
        "cache/volume.vdb\n", ".vdb"));
    assert(!dommy::file::isSafeExportPath(
        "cache/", ".vdb"));
}

void testIndexTransformSafety() {
    const std::vector<std::size_t> validOrder{2U, 0U, 1U};
    assert(dommy::container::isValidPermutation(validOrder, 3U));
    assert(!dommy::container::isValidPermutation(
        std::vector<std::size_t>{0U, 0U, 2U}, 3U));
    assert(!dommy::container::isValidPermutation(
        std::vector<std::size_t>{0U, 1U, 3U}, 3U));

    std::vector<int> values{10, 20, 30};
    assert(dommy::container::reorderInPlace(values, validOrder));
    assert((values == std::vector<int>{30, 10, 20}));

    const std::vector<int> unchanged(values);
    assert(!dommy::container::reorderInPlace(
        values, std::vector<std::size_t>{0U, 0U, 1U}));
    assert(values == unchanged);

    assert(dommy::container::remapInPlace(
        values, std::vector<std::size_t>{2U, 2U, 0U}));
    assert((values == std::vector<int>{20, 20, 30}));
    const std::vector<int> remapped(values);
    assert(!dommy::container::remapInPlace(
        values, std::vector<std::size_t>{values.size()}));
    assert(values == remapped);

    std::vector<bool> group{true, false, true};
    assert(dommy::container::insertFill(group, 1U, 2U, false));
    assert(group.size() == 5U);
    assert(group[0] && !group[1] && !group[2] && !group[3] && group[4]);
    assert(!dommy::container::insertFill(
        group, group.size() + 1U, 1U, true));
    assert(group.size() == 5U);

    std::size_t offset = 99U;
    assert(dommy::container::normalizeCyclicOffset(0, 5U, offset));
    assert(offset == 0U);
    assert(dommy::container::normalizeCyclicOffset(-1, 5U, offset));
    assert(offset == 4U);
    assert(dommy::container::normalizeCyclicOffset(-6, 5U, offset));
    assert(offset == 4U);
    assert(dommy::container::normalizeCyclicOffset(
        std::numeric_limits<std::int64_t>::min(), 5U, offset));
    assert(offset == 2U);
    assert(dommy::container::normalizeCyclicOffset(
        std::numeric_limits<std::int64_t>::max(), 5U, offset));
    assert(offset == 2U);
    assert(!dommy::container::normalizeCyclicOffset(1, 0U, offset));
}

void testVdbViewportNormalization() {
    bool negative = true;
    float weight = -1.0f;

    assert(dommy::vdb::normalizedSignedWeight(
        0.0, 0.0, 0.0, 0.0, negative, weight));
    assert(!negative && weight == 0.0f);

    assert(dommy::vdb::normalizedSignedWeight(
        5.0, 5.0, 5.0, 0.0, negative, weight));
    assert(!negative && weight == 1.0f);

    assert(dommy::vdb::normalizedSignedWeight(
        -5.0, -5.0, -5.0, 0.0, negative, weight));
    assert(negative && weight == 1.0f);

    assert(dommy::vdb::normalizedSignedWeight(
        -2.5, -5.0, 10.0, 0.0, negative, weight));
    assert(negative && near(weight, 0.5));

    assert(dommy::vdb::normalizedSignedWeight(
        5.0, -5.0, 10.0, 0.0, negative, weight));
    assert(!negative && near(weight, 0.5));

    assert(dommy::vdb::normalizedSignedWeight(
        20.0, -5.0, 10.0, 0.0, negative, weight));
    assert(!negative && weight == 1.0f);

    assert(!dommy::vdb::normalizedSignedWeight(
        std::numeric_limits<double>::quiet_NaN(),
        -1.0, 1.0, 0.0, negative, weight));
    assert(!dommy::vdb::normalizedSignedWeight(
        0.0, 1.0, -1.0, 0.0, negative, weight));
}

void testParallelWorkAndPointDeformMath() {
    assert(dommy::parallel::grainSize(0U, 0U) == 20000U);
    assert(dommy::parallel::grainSize(1U, 0U, 0U) == 1U);
    assert(dommy::parallel::grainSize(100U, 4U, 1U) == 25U);
    assert(dommy::parallel::grainSize(101U, 4U, 1U) == 26U);

    double weight = -1.0;
    assert(dommy::point::radialFalloffWeight(0.0, 2.0, weight));
    assert(near(weight, 1.0));
    assert(dommy::point::radialFalloffWeight(1.0, 2.0, weight));
    assert(near(weight, 0.5625));
    assert(dommy::point::radialFalloffWeight(2.0, 2.0, weight));
    assert(near(weight, 0.0));
    assert(dommy::point::radialFalloffWeight(4.0, 2.0, weight));
    assert(near(weight, 0.0));
    assert(!dommy::point::radialFalloffWeight(-1.0, 2.0, weight));
    assert(!dommy::point::radialFalloffWeight(1.0, 0.0, weight));
    assert(!dommy::point::radialFalloffWeight(
        std::numeric_limits<double>::quiet_NaN(), 2.0, weight));
    assert(!dommy::point::canNormalizeWeight(0.0));
    assert(!dommy::point::canNormalizeWeight(
        std::numeric_limits<double>::infinity()));
    assert(dommy::point::canNormalizeWeight(0.5));

    const float limit = std::numeric_limits<float>::max();
    const Imath::V3f sentinel(7.0f, 8.0f, 9.0f);
    const auto samePoint = [](const Imath::V3f& a, const Imath::V3f& b) {
        return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
    };
    Imath::V3f result = sentinel;
    for (float start : {-limit, limit}) {
        for (float end : {-1.0f, 1.0f}) {
            const Imath::V3f source(start, -0.0f, 2.0f), target(end, 3.0f, -0.0f);
            assert(dommy::point::moveToward(source, target, 0.0, result));
            assert(samePoint(result, source) && std::signbit(result[1]));
            assert(dommy::point::moveToward(source, target, 1.0, result));
            assert(samePoint(result, target) && std::signbit(result[2]));
        }
    }
    const Imath::V3f source(-3.25f, 7.5f, 0.125f), target(4.5f, -2.0f, 0.5f);
    for (double t : {-0.5, 0.25, 0.5, 0.75, 1.5}) {
        assert(dommy::point::moveToward(source, target, t, result));
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            const double expected = double(source[axis]) +
                (double(target[axis]) - source[axis]) * t;
            assert(result[axis] == static_cast<float>(expected));
        }
    }
    for (double t : {0.0, 1.0}) {
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()}) {
            result = sentinel;
            assert(!dommy::point::moveToward(Imath::V3f(bad, 0, 0), target, t, result));
            assert(samePoint(result, sentinel));
            assert(!dommy::point::moveToward(source, Imath::V3f(0, 0, bad), t, result));
            assert(samePoint(result, sentinel));
        }
    }
    result = sentinel;
    assert(!dommy::point::moveToward(source, target,
        std::numeric_limits<double>::quiet_NaN(), result));
    assert(samePoint(result, sentinel));
    assert(!dommy::point::moveToward(Imath::V3f(1, limit, 3),
        Imath::V3f(2, -limit, 4), 2.0, result));
    assert(samePoint(result, sentinel));
}

void testCurveDividePlan() {
    dommy::curve::DividePlan plan;
    assert(dommy::curve::makeDividePlan(
        0, 4, 4, 0.1, 0.2, 0.0, 2U, plan));
    assert(plan.outputCurveCount == 8U);
    assert(plan.outputPointCount == 32U);
    assert(!dommy::curve::makeDividePlan(
        0, 0, 4, 0.1, 0.2, 0.0, 2U, plan));
    assert(!dommy::curve::makeDividePlan(
        0, 4, 1, 0.1, 0.2, 0.0, 2U, plan));
    assert(!dommy::curve::makeDividePlan(
        2, 4, 4, 0.1, 0.2, 0.0, 2U, plan));
    assert(!dommy::curve::makeDividePlan(
        1, 4, 4, 0.1, 0.0, 0.0, 2U, plan));
    assert(!dommy::curve::makeDividePlan(
        0, 1000000, 1000000, 0.0, 0.1, 0.0,
        1000000U, plan));

    assert(dommy::curve::makeDividePlan(
        0, 2, 3, 0.0, 0.1, 0.0, 1U, plan));
    double segment = 0.0;
    double gap = 0.0;
    double sampleStep = 0.0;
    assert(dommy::curve::divideSegmentLengths(
        plan, 10.0, false, segment, gap, sampleStep));
    assert(near(segment, 5.0));
    assert(near(gap, 0.0));
    assert(near(sampleStep, 2.5));
    double sampleLength = 0.0;
    assert(dommy::curve::divideSampleLength(
        plan, 1U, 2U, false, 10.0, segment, gap, sampleStep,
        sampleLength));
    assert(near(sampleLength, 10.0));
    assert(!dommy::curve::divideSampleLength(
        plan, 2U, 0U, false, 10.0, segment, gap, sampleStep,
        sampleLength));

    assert(dommy::curve::makeDividePlan(
        1, 3, 3, 0.1, 0.5, 0.0, 1U, plan));
    assert(dommy::curve::divideSegmentLengths(
        plan, 10.0, false, segment, gap, sampleStep));
    assert(!dommy::curve::divideSampleLength(
        plan, 2U, 2U, false, 10.0, segment, gap, sampleStep,
        sampleLength));
}

void testCurveFromCurvePlan() {
    dommy::curve::CurveFromCurvePlan plan;
    std::vector<std::size_t> cvCounts;
    cvCounts.push_back(4U);
    cvCounts.push_back(6U);
    assert(dommy::curve::makeCurveFromCurvePlan(
        0, 8, 1, 1, 1.0, 3U, cvCounts, plan));
    assert(plan.samplingPerSourceCurve.size() == 2U);
    assert(plan.samplingPerSourceCurve[0] == 8U);
    assert(plan.outputCurveCount == 6U);
    assert(plan.outputPointCount == 48U);
    assert(dommy::curve::curveFromCurveDegree(1, 4U) == 3);
    assert(dommy::curve::curveFromCurveDegree(1, 3U) == 1);

    assert(dommy::curve::makeCurveFromCurvePlan(
        1, 8, 2, 0, 0.5, 3U, cvCounts, plan));
    assert(plan.samplingPerSourceCurve[0] == 10U);
    assert(plan.samplingPerSourceCurve[1] == 16U);
    assert(plan.outputPointCount == 78U);

    assert(!dommy::curve::makeCurveFromCurvePlan(
        2, 8, 1, 1, 1.0, 3U, cvCounts, plan));
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, -1, 1, 1, 1.0, 3U, cvCounts, plan));
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, 8, -1, 1, 1.0, 3U, cvCounts, plan));
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, 8, 1, 2, 1.0, 3U, cvCounts, plan));
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, 8, 1, 1, 0.0, 3U, cvCounts, plan));
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, 8, 1, 1, 1.1, 3U, cvCounts, plan));
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, 8, 1, 1, 1.0, 0U, cvCounts, plan));
    const std::vector<std::size_t> invalidCvCounts(1U, 1U);
    assert(!dommy::curve::makeCurveFromCurvePlan(
        0, 8, 1, 1, 1.0, 3U, invalidCvCounts, plan));
}

void testCurveCopyBudget() {
    std::size_t countU = 0U;
    std::size_t countV = 0U;
    std::size_t gridCount = 0U;
    assert(dommy::curve::checkedCurveCopyCounts(
        4, 5, countU, countV, gridCount));
    assert(countU == 4U && countV == 5U && gridCount == 20U);
    assert(!dommy::curve::checkedCurveCopyCounts(
        -1, 5, countU, countV, gridCount));
    assert(!dommy::curve::checkedCurveCopyCounts(
        1000000, 1000000, countU, countV, gridCount));

    std::size_t outputCvCount = 0U;
    assert(dommy::curve::checkedCurveCopyBudget(
        20U, 8U, outputCvCount));
    assert(outputCvCount == 200U);
    assert(!dommy::curve::checkedCurveCopyBudget(
        1U, 0U, outputCvCount));
    assert(!dommy::curve::checkedCurveCopyBudget(
        dommy::generation::kMaxGeneratedCurveCount,
        dommy::generation::kMaxGeneratedPointCount, outputCvCount));
}

void testMeshDuplicationPlan() {
    assert(dommy::mesh::clampedDuplicationSourceIndex(-1, 3U) == 0);
    assert(dommy::mesh::clampedDuplicationSourceIndex(1, 3U) == 1);
    assert(dommy::mesh::clampedDuplicationSourceIndex(9, 3U) == 2);

    std::vector<int> indices{-4, 1, 99};
    assert(dommy::mesh::normalizeDuplicationSourceIndices(2U, indices));
    assert(indices[0] == 0 && indices[1] == 1 && indices[2] == 1);
    assert(!dommy::mesh::normalizeDuplicationSourceIndices(0U, indices));

    std::vector<dommy::mesh::DuplicationSourceSize> sources;
    sources.push_back(dommy::mesh::DuplicationSourceSize(4U, 2U, 6U, 4U));
    sources.push_back(dommy::mesh::DuplicationSourceSize(8U, 6U, 18U, 9U));
    const std::vector<int> selection{0, 1, 1};
    dommy::mesh::MeshDuplicationBudget budget;
    assert(dommy::mesh::buildMeshDuplicationBudget(
        sources, selection, true, budget));
    assert(budget.meshCount == 3U);
    assert(budget.pointCount == 20U);
    assert(budget.faceCount == 14U);
    assert(budget.connectionCount == 42U);
    assert(budget.uvCount == 22U);
    assert(budget.workItemCount == 98U);

    // Count limits alone admit this request, but collection publication uses
    // the existing 256 MiB geometry staging policy. Reject before generation.
    const std::vector<dommy::mesh::DuplicationSourceSize> tooMuchStorage{
        dommy::mesh::DuplicationSourceSize(1500U, 500U, 1500U, 1500U)};
    assert(!dommy::mesh::buildMeshDuplicationBudget(
        tooMuchStorage, std::vector<int>(1000U, 0), true, budget));
    assert(budget.meshCount == 3U && budget.workItemCount == 98U);

    const std::vector<int> badSelection{2};
    assert(!dommy::mesh::buildMeshDuplicationBudget(
        sources, badSelection, true, budget));
    const std::vector<dommy::mesh::DuplicationSourceSize> oversized{
        dommy::mesh::DuplicationSourceSize(
            dommy::generation::kMaxGeneratedPointCount + 1U,
            1U, 3U, 0U)};
    const std::vector<int> singleSelection{0};
    assert(!dommy::mesh::buildMeshDuplicationBudget(
        oversized, singleSelection, false, budget));
}

void testBoundedPrefixCopy() {
    const std::vector<int> shortSource{4, 5};
    std::vector<int> destination{10, 11, 12, 13};
    assert(dommy::memory::copyPrefix(
               shortSource, shortSource.size(), destination,
               destination.size()) == 2U);
    assert(destination[0] == 4 && destination[1] == 5);
    assert(destination[2] == 12 && destination[3] == 13);

    const std::vector<int> longSource{1, 2, 3, 4, 5};
    assert(dommy::memory::copyPrefix(
               longSource, longSource.size(), destination,
               destination.size()) == destination.size());
    assert(destination[0] == 1 && destination[3] == 4);

    const std::vector<int> emptySource;
    assert(dommy::memory::copyPrefix(
               emptySource, emptySource.size(), destination,
               destination.size()) == 0U);
    assert(destination[0] == 1 && destination[3] == 4);

    assert(dommy::memory::indexInRange(0, 1U));
    assert(dommy::memory::indexInRange(3U, 4U));
    assert(!dommy::memory::indexInRange(-1, 4U));
    assert(!dommy::memory::indexInRange(4U, 4U));
}

void testSymmetryPointMatcher() {
    typedef dommy::mesh::Point3d Point;
    const std::vector<Point> points{
        Point(-2.0, 0.0, 0.0), Point(2.0, 0.0, 0.0),
        Point(0.0, 1.0, 0.0), Point(5.0, 3.0, 0.0)};
    std::vector<std::size_t> matches;
    assert(dommy::mesh::findMirroredPointMatches(points, 0.01, 10U, matches));
    assert(matches.size() == points.size());
    assert(matches[0] == 1U);
    assert(matches[1] == 0U);
    assert(matches[2] == 2U);
    assert(matches[3] == dommy::mesh::kNoPointMatch);

    const std::vector<Point> approximate{
        Point(-1.0, 0.0, 0.0), Point(1.05, 0.0, 0.0)};
    assert(dommy::mesh::findMirroredPointMatches(
        approximate, 0.1, 10U, matches));
    assert(matches[0] == 1U && matches[1] == 0U);

    matches.assign(1U, 42U);
    assert(!dommy::mesh::findMirroredPointMatches(
        points, -1.0, 10U, matches));
    assert(matches.size() == 1U && matches[0] == 42U);
    assert(!dommy::mesh::findMirroredPointMatches(
        points, 1.0, 3U, matches));
    const std::vector<Point> invalid{
        Point(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0)};
    assert(!dommy::mesh::findMirroredPointMatches(
        invalid, 1.0, 10U, matches));
}

void testIndexPatternParsing() {
    bool reverse = false;
    std::vector<std::size_t> indices;
    assert(dommy::component::parseIndexPattern(
        "1 3-6 ! 9", 10U, 10U, reverse, indices));
    const std::vector<std::size_t> expected{1U, 3U, 4U, 5U, 9U};
    assert(indices == expected);
    assert(reverse);

    assert(dommy::component::parseIndexPattern(
        "8-3 12", 6U, 10U, reverse, indices));
    const std::vector<std::size_t> clamped{3U, 4U, 5U};
    assert(indices == clamped);
    assert(!reverse);

    assert(dommy::component::parseIndexPattern(
        "0;2-5;8", 10U, 10U, reverse, indices));
    const std::vector<std::size_t> semicolonSeparated{
        0U, 2U, 3U, 4U, 8U};
    assert(indices == semicolonSeparated);
    assert(!reverse);

    indices.assign(1U, 42U);
    reverse = true;
    assert(!dommy::component::parseIndexPattern(
        "1--3", 10U, 10U, reverse, indices));
    assert(indices.size() == 1U && indices[0] == 42U && reverse);
    assert(!dommy::component::parseIndexPattern(
        "-1", 10U, 10U, reverse, indices));
    assert(!dommy::component::parseIndexPattern(
        "0-100", 100U, 20U, reverse, indices));
    assert(!dommy::component::parseIndexPattern(
        "184467440737095516160", std::numeric_limits<std::size_t>::max(),
        10U, reverse, indices));

    std::vector<bool> selection(6U, false);
    const std::vector<std::size_t> direct{1U, 3U, 3U};
    assert(dommy::component::mergeIndexPatternSelection(
        direct, false, selection));
    assert(!selection[0] && selection[1] && !selection[2] &&
           selection[3] && !selection[4] && !selection[5]);

    selection.assign(6U, false);
    selection[1] = true;
    const std::vector<std::size_t> excluded{1U, 4U};
    assert(dommy::component::mergeIndexPatternSelection(
        excluded, true, selection));
    assert(selection[0] && selection[1] && selection[2] &&
           selection[3] && !selection[4] && selection[5]);

    const std::vector<bool> unchanged(selection);
    const std::vector<std::size_t> invalid{6U};
    assert(!dommy::component::mergeIndexPatternSelection(
        invalid, false, selection));
    assert(selection == unchanged);
}

void testGoalMathSafety() {
    float velocity = 0.0f;
    assert(dommy::point::computeGoalVelocityComponent(
        0.0, 10.0, 2.0, 0.5, 0.25, 2.0, 3.0, velocity));
    assert(near(velocity, 27.0));

    velocity = 7.0f;
    assert(!dommy::point::computeGoalVelocityComponent(
        0.0, std::numeric_limits<double>::quiet_NaN(),
        0.0, 1.0, 0.0, 1.0, 1.0, velocity));
    assert(velocity == 7.0f);

    const double maximum =
        static_cast<double>(std::numeric_limits<float>::max());
    assert(!dommy::point::computeGoalVelocityComponent(
        -maximum, maximum, maximum,
        maximum, maximum, maximum, maximum, velocity));
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(dommy::point::computeGoalVelocityComponent(
        0.0, 1.0, 1e308, 0.0, 2.0, 0.0, 1.0, velocity) && velocity == 0.0f);
    assert(!dommy::point::computeGoalVelocityComponent(
        0.0, 1.0, std::numeric_limits<double>::infinity(), 0.0, 2.0, 0.0, 1.0, velocity));
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testFillSpacePlan() {
    using dommy::generation::FillSpacePlan;

    std::size_t pairCount = 0U;
    assert(dommy::generation::checkedNeighborPairCount(
        4U, 1U, pairCount));
    assert(pairCount == 3U);
    assert(dommy::generation::checkedNeighborPairCount(
        4U, 2U, pairCount));
    assert(pairCount == 5U);
    assert(dommy::generation::checkedNeighborPairCount(
        4U, 20U, pairCount));
    assert(pairCount == 6U);

    FillSpacePlan plan;
    assert(dommy::generation::buildFillSpacePlan(
        4U, dommy::generation::kFillSpaceByCount,
        2, 1.0, true, 1, false, 1.0, plan));
    assert(plan.pairCount == 3U);
    assert(plan.outputPointCount == 4U);
    for (std::size_t index = 0U; index < 3U; ++index) {
        assert(dommy::generation::reserveFillSpacePairWork(plan));
        std::size_t sampleCount = 0U;
        assert(dommy::generation::reserveFillSpaceSamples(
            plan, 10.0, sampleCount));
        assert(sampleCount == 2U);
    }
    assert(plan.outputPointCount == 10U);
    assert(plan.workItemCount == 13U);

    assert(dommy::generation::buildFillSpacePlan(
        3U, dommy::generation::kFillSpaceByDistance,
        1, 1.0, false, 0, true, 2.0, plan));
    assert(plan.pairCount == 3U);
    std::size_t sampleCount = 99U;
    assert(dommy::generation::reserveFillSpaceSamples(
        plan, 3.9, sampleCount));
    assert(sampleCount == 1U);
    assert(dommy::generation::reserveFillSpaceSamples(
        plan, 2.0, sampleCount));
    assert(sampleCount == 0U);

    assert(!dommy::generation::buildFillSpacePlan(
        1U, dommy::generation::kFillSpaceByDistance,
        1, 0.0, true, 1, false, 1.0, plan));
    assert(!dommy::generation::buildFillSpacePlan(
        1U, dommy::generation::kFillSpaceByCount,
        -1, 1.0, true, 1, false, 1.0, plan));
    assert(!dommy::generation::buildFillSpacePlan(
        1U, dommy::generation::kFillSpaceByCount,
        1, 1.0, true, -1, false, 1.0, plan));
    assert(!dommy::generation::buildFillSpacePlan(
        1U, dommy::generation::kFillSpaceByCount,
        1, 1.0, true, 1, true,
        std::numeric_limits<double>::quiet_NaN(), plan));
    assert(!dommy::generation::buildFillSpacePlan(
        20000U, dommy::generation::kFillSpaceByCount,
        1, 1.0, false, 0, true, 1.0, plan));
    assert(!dommy::generation::buildFillSpacePlan(
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kFillSpaceByCount,
        1, 1.0, true, 1, false, 1.0, plan));
}

void testFrameSamplingLimits() {
    std::size_t count = 0U;
    assert(dommy::sampling::checkedFrameCount(1.0, 10.0, 1.0, 10U, count));
    assert(count == 10U);
    assert(dommy::sampling::checkedFrameCount(3.0, 3.0, 0.25, 1U, count));
    assert(count == 1U);
    assert(!dommy::sampling::checkedFrameCount(1.0, 10.0, 0.0, 10U, count));
    assert(!dommy::sampling::checkedFrameCount(10.0, 1.0, 1.0, 10U, count));
    assert(!dommy::sampling::checkedFrameCount(
        0.0, std::numeric_limits<double>::infinity(), 1.0, 10U, count));
    assert(!dommy::sampling::checkedFrameCount(
        0.0, 1000000.0, 1.0,
        dommy::sampling::kMaxExportFrameSamples, count));

    double frame = 0.0;
    assert(dommy::sampling::frameAt(-2.0, 0.25, 8U, frame));
    assert(near(frame, 0.0));
    assert(!dommy::sampling::frameAt(0.0, 0.0, 1U, frame));
    assert(!dommy::sampling::frameAt(
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 2U, frame));

    std::size_t events = 0U;
    assert(dommy::sampling::checkedSampleEventCount(100U, 4U, events));
    assert(events == 400U);
    assert(!dommy::sampling::checkedSampleEventCount(
        dommy::sampling::kMaxExportSampleEvents, 2U, events));
}

void assertAbcTimePlanMatchesFrames(
    const std::set<double>& frames,
    const dommy::sampling::AbcTimeSamplingPlan& timePlan) {
    assert(!frames.empty());
    assert(!timePlan.sampleTimes.empty());

    if (timePlan.mode == dommy::sampling::kAbcDefaultTimeSampling) {
        assert(frames.size() == 1U);
        assert(near(*frames.begin(), timePlan.sampleTimes.front()));
        return;
    }

    if (timePlan.mode == dommy::sampling::kAbcAcyclicTimeSampling) {
        assert(timePlan.sampleTimes.size() == frames.size());
        std::set<double>::const_iterator frame = frames.begin();
        for (std::size_t index = 0U;
             index < timePlan.sampleTimes.size(); ++index, ++frame) {
            assert(near(*frame, timePlan.sampleTimes[index]));
        }
        return;
    }

    assert(timePlan.period > 0.0);
    const std::size_t samplesPerCycle = timePlan.sampleTimes.size();
    std::size_t index = 0U;
    for (std::set<double>::const_iterator frame = frames.begin();
         frame != frames.end(); ++frame, ++index) {
        const double expected =
            timePlan.sampleTimes[index % samplesPerCycle] +
            timePlan.period *
                static_cast<double>(index / samplesPerCycle);
        assert(near(*frame, expected));
    }
}

void testAbcSamplingPlan() {
    typedef dommy::sampling::AbcFrameRange Range;
    typedef dommy::sampling::AbcSamplingPlan Plan;

    {
        Range range;
        range.startTime = 1.0;
        range.endTime = 2.0;
        range.shutterSamples.insert(0.25);

        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan(
            std::vector<Range>(1U, range), true, false, plan));
        assert(plan.hasExplicitTimeSampling);
        assert(near(plan.setupFrame, 1.0));
        assert(plan.transformFrames.size() == 3U);
        assert(plan.transformFrames.count(1.0) == 1U);
        assert(plan.transformFrames.count(1.25) == 1U);
        assert(plan.transformFrames.count(2.25) == 1U);
        assert(plan.shapeFrames.size() == 1U);
        assert(plan.shapeFrames.count(1.0) == 1U);
        assert(plan.transformTime.mode ==
               dommy::sampling::kAbcAcyclicTimeSampling);
        assert(plan.transformTime.sampleTimes.size() == 3U);
        assert(near(plan.transformTime.sampleTimes[0U], 1.0));
        assert(near(plan.transformTime.sampleTimes[1U], 1.25));
        assert(near(plan.transformTime.sampleTimes[2U], 2.25));
        assert(plan.shapeTime.mode ==
               dommy::sampling::kAbcUniformTimeSampling);
        assert(plan.shapeTime.sampleTimes.size() == 1U);
        assert(near(plan.shapeTime.sampleTimes[0U], 1.0));
        assertAbcTimePlanMatchesFrames(
            plan.transformFrames, plan.transformTime);
        assertAbcTimePlanMatchesFrames(
            plan.shapeFrames, plan.shapeTime);
        assertAbcTimePlanMatchesFrames(plan.transformFrames, plan.boundsTime);
    }

    {
        Range range;
        range.startTime = 1.0;
        range.endTime = 2.0;
        range.shutterSamples.insert(-0.25);

        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan(
            std::vector<Range>(1U, range), true, false, plan));
        assert(near(plan.setupFrame, 0.75));
        assert(plan.transformFrames.size() == 2U);
        assert(plan.transformFrames.count(0.75) == 1U);
        assert(plan.transformFrames.count(1.75) == 1U);
        assert(plan.shapeFrames.size() == 2U);
        assert(plan.shapeFrames.count(0.75) == 1U);
        assert(plan.shapeFrames.count(1.0) == 1U);
        assert(plan.transformTime.mode ==
               dommy::sampling::kAbcUniformTimeSampling);
        assert(plan.shapeTime.mode ==
               dommy::sampling::kAbcAcyclicTimeSampling);
        assert(plan.shapeTime.sampleTimes.size() == 2U);
        assert(near(plan.shapeTime.sampleTimes[0U], 0.75));
        assert(near(plan.shapeTime.sampleTimes[1U], 1.0));
        assertAbcTimePlanMatchesFrames(std::set<double>({0.75,1.0,1.75}), plan.boundsTime);
        assertAbcTimePlanMatchesFrames(
            plan.transformFrames, plan.transformTime);
        assertAbcTimePlanMatchesFrames(
            plan.shapeFrames, plan.shapeTime);
    }

    {
        Range range;
        range.startTime = 1.0;
        range.endTime = 2.0;
        range.shutterSamples.insert(-0.25);
        range.shutterSamples.insert(0.25);

        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan(
            std::vector<Range>(1U, range), true, true, plan));
        assert(plan.transformFrames == plan.shapeFrames);
        assert(plan.transformTime.mode ==
               dommy::sampling::kAbcCyclicTimeSampling);
        assert(plan.transformTime.sampleTimes.size() == 2U);
        assert(near(plan.transformTime.sampleTimes[0U], 0.75));
        assert(near(plan.transformTime.sampleTimes[1U], 1.25));
        assert(near(plan.transformTime.period, 1.0));
        assert(plan.shapeTime.mode ==
               dommy::sampling::kAbcCyclicTimeSampling);
        assertAbcTimePlanMatchesFrames(plan.transformFrames, plan.boundsTime);
        assertAbcTimePlanMatchesFrames(
            plan.transformFrames, plan.transformTime);
        assertAbcTimePlanMatchesFrames(
            plan.shapeFrames, plan.shapeTime);
    }

    {
        Range first;
        first.startTime = 1.0;
        first.endTime = 2.0;
        first.shutterSamples.insert(0.0);

        Range second;
        second.startTime = 4.0;
        second.endTime = 5.0;
        second.shutterSamples.insert(0.25);

        std::vector<Range> ranges;
        ranges.push_back(first);
        ranges.push_back(second);

        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan(
            ranges, true, true, plan));
        assert(plan.transformFrames.size() == 4U);
        assert(plan.transformTime.mode ==
               dommy::sampling::kAbcAcyclicTimeSampling);
        assert(plan.transformTime.sampleTimes.size() == 4U);
        assert(near(plan.transformTime.sampleTimes[0U], 1.0));
        assert(near(plan.transformTime.sampleTimes[1U], 2.0));
        assert(near(plan.transformTime.sampleTimes[2U], 4.25));
        assert(near(plan.transformTime.sampleTimes[3U], 5.25));
        assertAbcTimePlanMatchesFrames(
            plan.transformFrames, plan.transformTime);
        assertAbcTimePlanMatchesFrames(
            plan.shapeFrames, plan.shapeTime);
    }

    {
        Range preRoll;
        preRoll.startTime = -2.0;
        preRoll.endTime = 0.0;
        preRoll.preRoll = true;

        Range sampled;
        sampled.startTime = 1.0;
        sampled.endTime = 2.0;

        std::vector<Range> ranges;
        ranges.push_back(preRoll);
        ranges.push_back(sampled);

        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan(
            ranges, true, true, plan));
        assert(near(plan.setupFrame, 1.0));
        assert(plan.preRollFrames.size() == 3U);
        assert(plan.preRollFrames.count(-2.0) == 1U);
        assert(plan.preRollFrames.count(-1.0) == 1U);
        assert(plan.preRollFrames.count(0.0) == 1U);
        assert(plan.transformFrames.count(1.0) == 1U);
        assert(plan.transformFrames.count(2.0) == 1U);
        assert(plan.transformTime.mode ==
               dommy::sampling::kAbcUniformTimeSampling);
        assertAbcTimePlanMatchesFrames(
            plan.transformFrames, plan.transformTime);
        assertAbcTimePlanMatchesFrames(
            plan.shapeFrames, plan.shapeTime);
    }

    {
        Range currentFrame;
        currentFrame.startTime = 7.0;
        currentFrame.endTime = 7.0;

        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan(
            std::vector<Range>(1U, currentFrame), false, true, plan));
        assert(!plan.hasExplicitTimeSampling);
        assert(near(plan.setupFrame, 7.0));
        assert(plan.transformTime.mode ==
               dommy::sampling::kAbcDefaultTimeSampling);
        assert(plan.shapeTime.mode ==
               dommy::sampling::kAbcDefaultTimeSampling);
        assert(plan.boundsTime.mode == dommy::sampling::kAbcDefaultTimeSampling);
        assertAbcTimePlanMatchesFrames(
            plan.transformFrames, plan.transformTime);
        assertAbcTimePlanMatchesFrames(
            plan.shapeFrames, plan.shapeTime);
    }

    {
        Range range;
        range.startTime=1.0;range.endTime=2.0;
        range.shutterSamples={-0.25,0.25};
        Plan plan;
        assert(dommy::sampling::buildAbcSamplingPlan({range},true,false,plan));
        assert(plan.transformFrames.size()==4U&&plan.shapeFrames.size()==2U);
        assert(plan.boundsTime.mode==dommy::sampling::kAbcAcyclicTimeSampling);
        assertAbcTimePlanMatchesFrames(std::set<double>({0.75,1.0,1.25,1.75,2.25}),plan.boundsTime);
    }

    {
        Range invalid;
        invalid.startTime = std::numeric_limits<double>::quiet_NaN();
        Plan plan;
        assert(!dommy::sampling::buildAbcSamplingPlan(
            std::vector<Range>(1U, invalid), true, true, plan));
    }

    {
        Range first;
        first.startTime = 1.0;
        first.endTime = 1.0;
        Range second;
        second.startTime = 2.0;
        second.endTime = 2.0;
        std::vector<Range> ranges;
        ranges.push_back(first);
        ranges.push_back(second);

        Plan plan;
        assert(!dommy::sampling::buildAbcSamplingPlan(
            ranges, false, true, plan));
    }
}

std::string readTestFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void writeTestFile(const std::string& path, const std::string& value) {
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    assert(output.is_open());
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output.close();
    assert(!output.fail());
}

void testAtomicOutputFile() {
    const char* temporaryDirectory = std::getenv("TMPDIR");
#if defined(_WIN32)
    if (!temporaryDirectory) temporaryDirectory = std::getenv("TEMP");
#endif
    const std::string root = temporaryDirectory ? temporaryDirectory : ".";
    const std::string separator =
        !root.empty() && (root[root.size() - 1U] == '/' ||
                          root[root.size() - 1U] == '\\') ? "" : "/";
    const std::string destination = dommy::file::makeTemporarySiblingPath(
        root + separator + "dommytool-atomic-output-test");

    const std::string suffixed =
        dommy::file::createTemporarySiblingFileWithSuffix(
            root + separator + "dommytool-vdb-cache-test", ".vdb");
    assert(dommy::file::isRegularFile(suffixed));
    assert(suffixed.size() >= 4U &&
           suffixed.substr(suffixed.size() - 4U) == ".vdb");
    assert(std::remove(suffixed.c_str()) == 0);
    assert(dommy::file::sanitizeFileNameComponent("node:shape/one") ==
           "node_shape_one");

    writeTestFile(destination, "original");
    std::string abandonedTemporary;
    {
        dommy::file::PendingOutputFile pending(destination);
        abandonedTemporary = pending.diagnosticTemporaryPath();
        assert(dommy::file::isRegularFile(abandonedTemporary));
        pending.stream() << "partial";
    }
    assert(readTestFile(destination) == "original");
    std::ifstream abandoned(abandonedTemporary.c_str(), std::ios::binary);
    assert(!abandoned.is_open());

    {
        dommy::file::PendingOutputFile pending(destination);
        const std::string firstReservedPath = pending.diagnosticTemporaryPath();
        dommy::file::PendingOutputFile secondPending(destination);
        assert(firstReservedPath != secondPending.diagnosticTemporaryPath());
        pending.stream() << "complete";
        assert(pending.commit());
        assert(pending.commit());
    }
    assert(readTestFile(destination) == "complete");
    assert(dommy::file::fileSizeWithinLimit(destination, 8U));
    assert(!dommy::file::fileSizeWithinLimit(destination, 7U));
    assert(std::remove(destination.c_str()) == 0);
}

void testArnoldArrayBudget() {
    dommy::arnold::ArrayLayout layout;
    assert(dommy::arnold::checkedArrayLayout(100U, 2U, layout));
    assert(layout.elementCount == 100U);
    assert(layout.keyCount == 2U);
    assert(layout.valueCount == 200U);
    assert(!dommy::arnold::checkedArrayLayout(1U, 0U, layout));
    assert(!dommy::arnold::checkedArrayLayout(1U, 256U, layout));
    assert(!dommy::arnold::checkedArrayLayout(
        dommy::arnold::kMaxArnoldArrayValues, 2U, layout));
    assert(!dommy::arnold::checkedArrayLayout(
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) +
            1U,
        1U, layout));

    std::size_t total = 3U;
    assert(dommy::arnold::checkedAdjustedAccumulate(5U, -2, total));
    assert(total == 6U);
    assert(!dommy::arnold::checkedAdjustedAccumulate(1U, -2, total));
}

void testDeformerSafetyHelpers() {
    assert(compileMayaStateGuards());
    assert(near(dommy::deformer::linearFalloff(0.0, 2.0), 1.0));
    assert(near(dommy::deformer::linearFalloff(1.0, 2.0), 0.5));
    assert(near(dommy::deformer::linearFalloff(2.0, 2.0), 0.0));
    assert(near(dommy::deformer::linearFalloff(0.0, 0.0), 1.0));
    assert(near(dommy::deformer::linearFalloff(1.0, 0.0), 0.0));
    assert(near(dommy::deformer::linearFalloff(
                    std::numeric_limits<double>::quiet_NaN(), 1.0),
                0.0));
    assert(near(dommy::deformer::radialFalloff(1.0, 2.0, 0, 1.0), 1.0));
    assert(near(dommy::deformer::radialFalloff(2.0, 2.0, 0, 1.0), 0.0));
    assert(near(dommy::deformer::radialFalloff(1.0, 2.0, 1, 1.0), 0.5));
    assert(near(dommy::deformer::radialFalloff(1.0, 2.0, 2, 1.0), 0.25));
    assert(near(dommy::deformer::radialFalloff(1.0, 2.0, 1, 2.0), 0.25));
    assert(near(dommy::deformer::radialFalloff(1.0, 2.0, 1, 0.0), 1.0));
    assert(near(dommy::deformer::radialFalloff(1.0, 2.0, 3, 1.0), 0.0));
    assert(dommy::deformer::hasSampleForPoint(2, 3));
    assert(!dommy::deformer::hasSampleForPoint(3, 3));
    assert(dommy::deformer::validSamplingRequest(1, 0.0));
    assert(!dommy::deformer::validSamplingRequest(0, 1.0));
    assert(!dommy::deformer::validSamplingRequest(
        1, std::numeric_limits<double>::infinity()));
    assert(dommy::deformer::validSampleStorageRequest(4, 100, 64, 1000));
    assert(!dommy::deformer::validSampleStorageRequest(65, 1, 64, 1000));
    assert(!dommy::deformer::validSampleStorageRequest(4, 251, 64, 1000));
    assert(!dommy::deformer::validSampleStorageRequest(
        std::numeric_limits<long>::max(),
        std::numeric_limits<std::size_t>::max(), 64, 1000));
    assert(dommy::deformer::validMotionSampleRequest(4, 5.0, 100U));
    assert(!dommy::deformer::validMotionSampleRequest(
        static_cast<long>(dommy::deformer::kMaxMotionSampleCount + 1U),
        5.0, 1U));
    assert(!dommy::deformer::validMotionSampleRequest(
        2, 5.0, dommy::deformer::kMaxMotionPointSamples));
    assert(dommy::deformer::hasMatchingSampleLayout(
        std::vector<std::size_t>{3U, 5U},
        std::vector<std::size_t>{3U, 5U}));
    assert(!dommy::deformer::hasMatchingSampleLayout(
        std::vector<std::size_t>{3U, 5U},
        std::vector<std::size_t>{3U, 4U}));

    dommy::deformer::MotionSampleBlend blend;
    assert(dommy::deformer::motionSampleBlend(0.0, 5U, blend));
    assert(blend.lowerIndex == 0U && blend.upperIndex == 0U);
    assert(near(blend.fraction, 0.0));
    assert(dommy::deformer::motionSampleBlend(0.125, 5U, blend));
    assert(blend.lowerIndex == 0U && blend.upperIndex == 1U);
    assert(near(blend.fraction, 0.5));
    assert(dommy::deformer::motionSampleBlend(0.25, 5U, blend));
    assert(blend.lowerIndex == 1U && blend.upperIndex == 2U);
    assert(near(blend.fraction, 0.0));
    assert(dommy::deformer::motionSampleBlend(0.5, 5U, blend));
    assert(blend.lowerIndex == 2U && blend.upperIndex == 3U);
    assert(near(blend.fraction, 0.0));
    assert(dommy::deformer::motionSampleBlend(1.0, 5U, blend));
    assert(blend.lowerIndex == 4U && blend.upperIndex == 4U);
    assert(near(blend.fraction, 0.0));
    assert(dommy::deformer::motionSampleBlend(0.5, 1U, blend));
    assert(blend.lowerIndex == 0U && blend.upperIndex == 0U);
    assert(!dommy::deformer::motionSampleBlend(-0.1, 5U, blend));
    assert(!dommy::deformer::motionSampleBlend(1.1, 5U, blend));
    assert(!dommy::deformer::motionSampleBlend(
        std::numeric_limits<double>::quiet_NaN(), 5U, blend));
    assert(!dommy::deformer::motionSampleBlend(0.5, 0U, blend));

    dommy::generation::MotionTrailBudget trailBudget;
    assert(dommy::generation::buildMotionTrailBudget(
        12, 16, 24.0, 5.0, 1.0, trailBudget));
    assert(trailBudget.curveCount == 12U);
    assert(trailBudget.sampleCount == 16U);
    assert(trailBudget.pointCount == 192U);
    assert(!dommy::generation::buildMotionTrailBudget(
        12, 3, 24.0, 5.0, 1.0, trailBudget));
    assert(!dommy::generation::buildMotionTrailBudget(
        12, static_cast<int>(
                dommy::deformer::kMaxMotionSampleCount + 1U),
        24.0, 5.0, 1.0, trailBudget));
    assert(!dommy::generation::buildMotionTrailBudget(
        12, 16, 24.0, -1.0, 1.0, trailBudget));
    assert(!dommy::generation::buildMotionTrailBudget(
        12, 16, 24.0, 1.0, 2.1, trailBudget));
    assert(!dommy::generation::buildMotionTrailBudget(
        12, 16, 24.0, std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), trailBudget));
    assert(!dommy::generation::buildMotionTrailBudget(
        12, 16, std::numeric_limits<double>::infinity(),
        5.0, 1.0, trailBudget));
    assert(!dommy::generation::buildMotionTrailBudget(
        static_cast<int>(dommy::generation::kMaxGeneratedCurveCount),
        16, 24.0, 5.0, 1.0, trailBudget));
    double trailStep = 0.0;
    assert(dommy::generation::motionTrailStep(5.0, 6U, trailStep));
    assert(near(trailStep, -1.0));
    assert(near(trailStep * 5.0, -5.0));
    assert(dommy::generation::motionTrailStep(0.0, 4U, trailStep));
    assert(near(trailStep, 0.0));
    assert(!dommy::generation::motionTrailStep(-1.0, 4U, trailStep));
    assert(!dommy::generation::motionTrailStep(1.0, 1U, trailStep));

    dommy::generation::MotionDuplicationBudget duplicationBudget;
    assert(dommy::generation::buildMotionDuplicationBudget(
        4, 100U, 50U, 300U, duplicationBudget));
    assert(duplicationBudget.objectCount == 4U);
    assert(duplicationBudget.pointCount == 400U);
    assert(duplicationBudget.faceCount == 200U);
    assert(duplicationBudget.connectionCount == 1200U);
    assert(dommy::generation::buildMotionDuplicationBudget(
        0, 0U, 0U, 0U, duplicationBudget));
    assert(!dommy::generation::buildMotionDuplicationBudget(
        -1, 100U, 50U, 300U, duplicationBudget));
    assert(!dommy::generation::buildMotionDuplicationBudget(
        1, 0U, 0U, 0U, duplicationBudget));
    assert(!dommy::generation::buildMotionDuplicationBudget(
        2, dommy::generation::kMaxGeneratedPointCount,
        1U, 3U, duplicationBudget));

    assert(compileDeformerExceptionBoundary(false) == MS::kSuccess);
    assert(compileDeformerExceptionBoundary(true) == MS::kFailure);
    assert(dommy::maya::reportDeformerException(
               "test", std::exception_ptr()) == MS::kFailure);
}

void testGenerationLimitsAndIcosahedronTopology() {
    dommy::batch::Range batch = dommy::batch::rangeAt(
        0U, dommy::batch::kMaximumShadingSampleCount, 0U);
    assert(batch.empty());
    batch = dommy::batch::rangeAt(10U, 0U, 0U);
    assert(batch.empty());
    batch = dommy::batch::rangeAt(
        10000U, dommy::batch::kMaximumShadingSampleCount, 0U);
    assert(batch.begin == 0U && batch.count == 4096U && batch.end() == 4096U);
    batch = dommy::batch::rangeAt(
        10000U, dommy::batch::kMaximumShadingSampleCount, batch.end());
    assert(batch.begin == 4096U && batch.count == 4096U && batch.end() == 8192U);
    batch = dommy::batch::rangeAt(
        10000U, dommy::batch::kMaximumShadingSampleCount, batch.end());
    assert(batch.begin == 8192U && batch.count == 1808U && batch.end() == 10000U);
    assert(dommy::batch::rangeAt(
        10000U, dommy::batch::kMaximumShadingSampleCount,
        batch.end()).empty());

    std::size_t result = 99;
    assert(!dommy::generation::checkedCount(
        -1, dommy::generation::kMaxGeneratedPointCount, result));
    assert(result == 99);
    assert(dommy::generation::checkedCount(
        12, dommy::generation::kMaxGeneratedPointCount, result));
    assert(result == 12);
    assert(dommy::generation::checkedCount(
        static_cast<int>(dommy::generation::kMaxCurveSampleCount),
        dommy::generation::kMaxCurveSampleCount, result));
    assert(result == dommy::generation::kMaxCurveSampleCount);
    const std::size_t preservedCurveSampleCount = result;
    assert(!dommy::generation::checkedCount(
        static_cast<int>(dommy::generation::kMaxCurveSampleCount + 1U),
        dommy::generation::kMaxCurveSampleCount, result));
    assert(result == preservedCurveSampleCount);
    assert(dommy::generation::checkedCount(
        static_cast<int>(dommy::generation::kMaxConnectedGeometryCount),
        dommy::generation::kMaxConnectedGeometryCount, result));
    assert(result == dommy::generation::kMaxConnectedGeometryCount);
    const std::size_t preservedGeometryCount = result;
    assert(!dommy::generation::checkedCount(
        static_cast<int>(
            dommy::generation::kMaxConnectedGeometryCount + 1U),
        dommy::generation::kMaxConnectedGeometryCount, result));
    assert(result == preservedGeometryCount);
    assert(!dommy::generation::checkedProduct(
        65536U, 65536U, dommy::generation::kMaxGeneratedPointCount, result));
    assert(dommy::generation::checkedProduct(
        20U, 30U, dommy::generation::kMaxGeneratedPointCount, result));
    assert(result == 600);
    assert(dommy::generation::checkedSum(
        4U, 1U, dommy::generation::kMaxGeneratedPointCount, result));
    assert(result == 5U);
    const std::size_t preservedSum = result;
    assert(!dommy::generation::checkedSum(
        dommy::generation::kMaxGeneratedPointCount, 1U,
        dommy::generation::kMaxGeneratedPointCount, result));
    assert(result == preservedSum);
    assert(dommy::generation::checkedGridBudget(
        10.0, 20.0, 30.0, 1.0, 2.0,
        dommy::generation::kMaxWorkItemCount, result));
    assert(result == 12U * 22U * 32U);
    const std::size_t preservedBudget = result;
    assert(!dommy::generation::checkedGridBudget(
        100000.0, 100000.0, 100000.0, 0.001, 6.0,
        dommy::generation::kMaxWorkItemCount, result));
    assert(result == preservedBudget);
    assert(!dommy::generation::checkedGridBudget(
        -1.0, 1.0, 1.0, 1.0, 0.0,
        dommy::generation::kMaxWorkItemCount, result));

    unsigned int subdivisions = 99U;
    assert(dommy::geometry::historicalSubdivisionCount(1, subdivisions));
    assert(subdivisions == 0U);
    assert(dommy::geometry::historicalSubdivisionCount(2, subdivisions));
    assert(subdivisions == 2U);
    assert(!dommy::geometry::historicalSubdivisionCount(0, subdivisions));
    assert(!dommy::geometry::historicalSubdivisionCount(11, subdivisions));

    std::size_t pointCount = 0;
    std::size_t faceCount = 0;
    assert(dommy::geometry::checkedIcosahedronCounts(0, pointCount, faceCount));
    assert(pointCount == 12 && faceCount == 20);
    assert(dommy::geometry::checkedIcosahedronCounts(8, pointCount, faceCount));
    assert(pointCount == 655362 && faceCount == 1310720);
    assert(!dommy::geometry::checkedIcosahedronCounts(9, pointCount, faceCount));

    dommy::geometry::IcosahedronTopology topology;
    assert(dommy::geometry::buildHistoricalIcosahedron(1, topology));
    assert(topology.points.size() == 12 && topology.triangles.size() == 20);
    assert(dommy::geometry::buildHistoricalIcosahedron(2, topology));
    assert(topology.points.size() == 162 && topology.triangles.size() == 320);
    for (std::size_t index = 0; index < topology.points.size(); ++index) {
        const dommy::geometry::IcosahedronPoint& point = topology.points[index];
        const double length = std::sqrt(
            static_cast<double>(point.x) * point.x +
            static_cast<double>(point.y) * point.y +
            static_cast<double>(point.z) * point.z);
        assert(near(length, 1.0, 1.0e-5));
    }
    for (std::size_t index = 0; index < topology.triangles.size(); ++index) {
        const dommy::geometry::IcosahedronTriangle& triangle = topology.triangles[index];
        assert(triangle.first >= 0 && triangle.second >= 0 && triangle.third >= 0);
        assert(static_cast<std::size_t>(triangle.first) < topology.points.size());
        assert(static_cast<std::size_t>(triangle.second) < topology.points.size());
        assert(static_cast<std::size_t>(triangle.third) < topology.points.size());
    }

    const std::size_t preservedPointCount = topology.points.size();
    assert(!dommy::geometry::buildHistoricalIcosahedron(9, topology));
    assert(topology.points.size() == preservedPointCount);

    dommy::generation::ArcMeshPlan arcPlan;
    assert(dommy::generation::buildArcMeshPlan(
        10, 5.0, 1.0, 0.0, 45.0, 0.0, arcPlan));
    assert(arcPlan.sampleCount == 12U);
    assert(arcPlan.vertexCount == 24U);
    assert(arcPlan.faceCount == 11U);
    assert(arcPlan.connectionCount == 44U);
    assert(near(arcPlan.stepDegrees, 45.0 / 11.0));
    assert(!dommy::generation::buildArcMeshPlan(
        10, 0.0, 0.0, 0.0, 45.0, 0.0, arcPlan));
    assert(!dommy::generation::buildArcMeshPlan(
        10, 1.0, 0.0, 45.0, 45.0, 0.0, arcPlan));
    assert(!dommy::generation::buildArcMeshPlan(
        dommy::generation::kMaxArcDivisionCount + 1,
        1.0, 0.0, 0.0, 45.0, 0.0, arcPlan));
    assert(!dommy::generation::buildArcMeshPlan(
        10, std::numeric_limits<double>::quiet_NaN(),
        0.0, 0.0, 45.0, 0.0, arcPlan));

    const double rigValues[] = {
        1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0, 0.75, 0.25,
    };
    dommy::generation::RigControllerPlan rigPlan;
    assert(dommy::generation::buildRigControllerPlan(
        0, 8, rigValues,
        sizeof(rigValues) / sizeof(rigValues[0]), rigPlan));
    assert(rigPlan.maximumPointCount == 192U);
    assert(!dommy::generation::buildRigControllerPlan(
        9, 8, rigValues,
        sizeof(rigValues) / sizeof(rigValues[0]), rigPlan));
    assert(!dommy::generation::buildRigControllerPlan(
        0, 2, rigValues,
        sizeof(rigValues) / sizeof(rigValues[0]), rigPlan));
    const double invalidRigValues[] = {
        std::numeric_limits<double>::infinity(),
    };
    assert(!dommy::generation::buildRigControllerPlan(
        0, 8, invalidRigValues, 1U, rigPlan));

    dommy::generation::LeafMeshPlan leafPlan;
    assert(dommy::generation::buildLeafMeshPlan(8, 3, leafPlan));
    assert(leafPlan.lengthSampleCount == 8U);
    assert(leafPlan.widthSampleCount == 3U);
    assert(leafPlan.pointsPerRow == 5U);
    assert(leafPlan.vertexCount == 40U);
    assert(leafPlan.faceCount == 28U);
    assert(leafPlan.connectionCount == 112U);
    assert(!dommy::generation::buildLeafMeshPlan(2, 3, leafPlan));
    assert(!dommy::generation::buildLeafMeshPlan(8, 1, leafPlan));
    assert(!dommy::generation::buildLeafMeshPlan(-1, 3, leafPlan));
    assert(!dommy::generation::buildLeafMeshPlan(
        static_cast<int>(dommy::generation::kMaxGeneratedPointCount),
        static_cast<int>(dommy::generation::kMaxGeneratedPointCount),
        leafPlan));

    dommy::generation::EyeMeshPlan eyePlan;
    assert(dommy::generation::buildEyeMeshPlan(9, 9, 4, false, eyePlan));
    assert(eyePlan.divideU == 36U);
    assert(eyePlan.divideV == 36U);
    assert(eyePlan.irisDivide == 4U);
    assert(dommy::generation::matchesEyePartPlan(eyePlan.lens, 1397U, 1377U, 5472U));
    assert(dommy::generation::matchesEyePartPlan(eyePlan.iris, 1513U, 1512U, 6012U));
    assert(!dommy::generation::matchesEyePartPlan(eyePlan.lens, 1396U, 1377U, 5472U));
    assert(dommy::generation::buildEyeMeshPlan(9, 9, 4, true, eyePlan));
    assert(dommy::generation::matchesEyePartPlan(eyePlan.iris, 1514U, 1548U, 6120U));

    // Minimum ring and a strongly asymmetric grid exercise both tip branches.
    assert(dommy::generation::buildEyeMeshPlan(1, 1, 0, false, eyePlan));
    assert(eyePlan.lensTipPointCount == 0U);
    assert(dommy::generation::matchesEyePartPlan(eyePlan.lens, 17U, 17U, 64U));
    assert(dommy::generation::buildEyeMeshPlan(1, 32, 0, false, eyePlan));
    assert(dommy::generation::matchesEyePartPlan(eyePlan.lens, 1602U, 1536U, 6016U));
    assert(!dommy::generation::buildEyeMeshPlan(1, 10000, 0, false, eyePlan));
    assert(eyePlan.lens.pointCount == 1602U); // Failed plans leave output intact.
    assert(!dommy::generation::buildEyeMeshPlan(0, 9, 4, false, eyePlan));
    assert(!dommy::generation::buildEyeMeshPlan(9, 0, 4, false, eyePlan));
    assert(!dommy::generation::buildEyeMeshPlan(9, 9, -1, false, eyePlan));
    assert(!dommy::generation::buildEyeMeshPlan(1000000, 1000000, 1, true, eyePlan));
}

void testLeafDeformationAndTileUV() {
    using dommy::generation::LeafBendTransform;
    dommy::generation::LeafMeshPlan plan;
    assert(dommy::generation::buildLeafMeshPlan(100000, 2, plan));
    assert(plan.bendPointTransformCount == 299997U);
    assert(plan.workItemCount <= dommy::generation::kMaxWorkItemCount);

    // Compare the optimized suffix composition with the old repeated bends,
    // using different pivots and positive/negative, nonconstant ramp values.
    for (std::size_t rows : {3U, 12U, 31U}) {
        for (double length : {0.1, 12.3, 100.0}) {
            std::vector<double> angles(rows - 1U), pivots(rows - 1U);
            std::vector<LeafBendTransform> suffixes(rows);
            LeafBendTransform suffix;
            for (std::size_t row = rows - 1U; row-- > 0U;) {
                angles[row] = 0.1 * std::sin(static_cast<double>(row) + 0.3);
                pivots[row] = length * (1.0 - static_cast<double>(row + 1U) / (rows - 1U));
                assert(dommy::generation::prependLeafBendStep(angles[row], pivots[row], suffix));
                suffixes[row] = suffix;
            }
            for (std::size_t row = 0U; row < rows; ++row) {
                double normalized = 0.0;
                assert(dommy::curve::pointUVPosition(row, rows, normalized));
                const double rampPosition = 1.0 - normalized;
                assert(rampPosition >= 0.0 && rampPosition <= 1.0);
                if (row == 0U) assert(rampPosition == 1.0);
                if (row + 1U == rows) assert(rampPosition == 0.0);
                for (double depth : {-0.4, 0.0, 0.7}) {
                    const double initialY = length * rampPosition;
                    double y = initialY, z = depth;
                    for (std::size_t step = row; step + 1U < rows; ++step) {
                        const double distanceY = y - pivots[step];
                        const double oldZ = z;
                        y = distanceY * std::cos(angles[step]) - oldZ * std::sin(angles[step]) + pivots[step];
                        z = distanceY * std::sin(angles[step]) + oldZ * std::cos(angles[step]);
                    }
                    double actualY = 0.0, actualZ = 0.0;
                    assert(dommy::generation::applyLeafBend(suffixes[row], initialY, depth, actualY, actualZ));
                    assert(std::abs(actualY - y) < 1.0e-10);
                    assert(std::abs(actualZ - z) < 1.0e-10);
                }
            }
        }
    }
    LeafBendTransform quarterTurn;
    assert(dommy::generation::prependLeafBendStep(std::acos(-1.0) * 0.5, 2.0, quarterTurn));
    double y = 0.0, z = 0.0;
    assert(dommy::generation::applyLeafBend(quarterTurn, 3.0, 0.0, y, z));
    assert(near(y, 2.0) && near(z, 1.0));
    assert(!dommy::generation::prependLeafBendStep(std::numeric_limits<double>::infinity(), 0.0, quarterTurn));

    int next = -1;
    assert(dommy::random::nextNonnegativeSeed(0, next) && next == 1);
    assert(dommy::random::nextNonnegativeSeed(INT_MAX - 1, next) && next == INT_MAX);
    assert(dommy::random::nextNonnegativeSeed(INT_MAX, next) && next == 0);
    assert(!dommy::random::nextNonnegativeSeed(-1, next) && next == 0);

    float u = -1.0f, v = -1.0f;
    // U-major surface placement order, with unequal U/V dimensions.
    for (std::size_t index = 0U; index < 8U; ++index) {
        assert(dommy::uv::mapTileUV(1.0, 1.0, index / 4U, index % 4U, 2U, 4U, u, v));
        assert(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f);
    }
    assert(near(u, 1.0) && near(v, 1.0));
    // Skipped placements do not move the surviving cell at original index 4.
    assert(dommy::uv::mapTileUV(0.0, 0.0, 4U / 4U, 4U % 4U, 2U, 4U, u, v));
    assert(near(u, 0.5) && near(v, 0.0));
    assert(!dommy::uv::mapTileUV(0.0, 0.0, 0U, 0U, 0U, 4U, u, v));
    assert(!dommy::uv::mapTileUV(0.0, 0.0, 2U, 0U, 2U, 4U, u, v));
    assert(!dommy::uv::mapTileUV(std::numeric_limits<double>::infinity(), 0.0, 0U, 0U, 2U, 4U, u, v));
}

void testWoolCurvePlan() {
    dommy::curve::WoolCurvePlan plan;
    assert(dommy::curve::buildWoolCurvePlan(
        1.0, 50.0, 50, 8, 0.25, plan));
    assert(plan.generatedPointCount == 400U);
    assert(plan.curveCvCount == 401U);
    assert(plan.knotCount == 403U);
    assert(plan.degree == 3U);
    assert(near(plan.radiusStep, 49.0 / 400.0));

    assert(!dommy::curve::buildWoolCurvePlan(
        1.0, 50.0, 0, 8, 0.0, plan));
    assert(!dommy::curve::buildWoolCurvePlan(
        1.0, 50.0, 1, 2, 0.0, plan));
    assert(!dommy::curve::buildWoolCurvePlan(
        1.0, 1.0, 1, 3, 0.0, plan));
    assert(!dommy::curve::buildWoolCurvePlan(
        -1.0, 1.0, 1, 3, 0.0, plan));
    assert(!dommy::curve::buildWoolCurvePlan(
        1.0, 2.0,
        static_cast<int>(dommy::generation::kMaxCurveSampleCount),
        3, 0.0, plan));
    assert(!dommy::curve::buildWoolCurvePlan(
        1.0, 2.0, 1, 3,
        std::numeric_limits<double>::quiet_NaN(), plan));
}

void testAttributeTransferPlan() {
    dommy::transfer::AttributeTransferPlan plan;
    assert(dommy::transfer::buildAttributeTransferPlan(
        3U, 2U, 2, 2.0, 0.0, 1.0, 1.0, plan));
    assert(plan.sourcePointCount == 3U);
    assert(plan.targetPointCount == 2U);
    assert(plan.maximumNeighbors == 2U);
    assert(plan.workItemBudget == 4U);
    assert(!dommy::transfer::buildAttributeTransferPlan(
        3U, 2U, 0, 2.0, 0.0, 1.0, 1.0, plan));
    assert(!dommy::transfer::buildAttributeTransferPlan(
        3U, 2U, 2, 0.0, 0.0, 1.0, 1.0, plan));
    assert(!dommy::transfer::buildAttributeTransferPlan(
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::transfer::kUnlimitedSampling,
        2.0, 0.0, 1.0, 1.0, plan));

    typedef std::vector<std::vector<uint32_t> > IdLists;
    typedef std::vector<std::vector<double> > ValueLists;
    const IdLists ids = {
        std::vector<uint32_t>{0U, 1U},
        std::vector<uint32_t>{2U},
    };
    const ValueLists distances = {
        std::vector<double>{0.0, 1.0},
        std::vector<double>{0.5},
    };
    assert(dommy::transfer::buildAttributeTransferPlan(
        3U, 2U, 2, 2.0, 0.0, 1.0, 1.0, plan));
    assert(dommy::transfer::validateNeighborLists(
        ids, distances, plan));
    IdLists invalidIds = ids;
    invalidIds[1][0] = 3U;
    assert(!dommy::transfer::validateNeighborLists(
        invalidIds, distances, plan));

    const ValueLists factors = {
        std::vector<double>{1.0, 0.5},
        std::vector<double>{0.25},
    };
    const std::vector<double> source = {2.0, 4.0, 8.0};
    std::vector<double> output;
    assert(dommy::transfer::transferWeightedValues(
        source, 3U, ids, factors, output));
    assert(output.size() == 2U);
    assert(near(output[0], 2.0));
    assert(near(output[1], 2.0));

    const std::vector<Imath::V3f> vectors = {
        Imath::V3f(2.0f, 4.0f, 6.0f),
        Imath::V3f(4.0f, 8.0f, 12.0f),
        Imath::V3f(8.0f, 16.0f, 24.0f),
    };
    std::vector<Imath::V3f> vectorOutput;
    assert(dommy::transfer::transferWeightedValues(
        vectors, 3U, ids, factors, vectorOutput));
    assert(vectorOutput.size() == 2U);
    assert(near(vectorOutput[0][0], 2.0));
    assert(near(vectorOutput[0][1], 4.0));
    assert(near(vectorOutput[0][2], 6.0));

    const std::vector<int32_t> integerSource = {
        std::numeric_limits<int32_t>::max(), 1, 1,
    };
    const ValueLists overflowingFactors = {
        std::vector<double>{2.0, 1.0},
        std::vector<double>{1.0},
    };
    std::vector<int32_t> integerOutput;
    assert(!dommy::transfer::transferWeightedValues(
        integerSource, 3U, ids, overflowingFactors, integerOutput));
}

void testEyelashAndDuplicateCurvePlans() {
    const double finiteInputs[] = {0.0, -2.0, 4.5};
    dommy::generation::EyelashGenerationPlan eyelashPlan;
    assert(dommy::generation::buildEyelashGenerationPlan(
        20, 8, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), eyelashPlan));
    assert(eyelashPlan.curveCount == 20U);
    assert(eyelashPlan.segmentCount == 8U);
    assert(eyelashPlan.pointsPerCurve == 10U);
    assert(eyelashPlan.totalPointCount == 200U);
    assert(eyelashPlan.bendWorkItems == 1800U);
    assert(!dommy::generation::buildEyelashGenerationPlan(
        1, 8, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), eyelashPlan));
    assert(!dommy::generation::buildEyelashGenerationPlan(
        20, 1, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), eyelashPlan));
    assert(!dommy::generation::buildEyelashGenerationPlan(
        20, 8, NULL, 1U, eyelashPlan));
    const double nonFiniteInputs[] = {
        0.0, std::numeric_limits<double>::infinity()};
    assert(!dommy::generation::buildEyelashGenerationPlan(
        20, 8, nonFiniteInputs,
        sizeof(nonFiniteInputs) / sizeof(nonFiniteInputs[0]), eyelashPlan));

    std::vector<bool> deleteFlags;
    assert(dommy::curve::buildDuplicateDeletionPlan(
        6U,
        [](std::size_t first, std::size_t second) {
            const int groups[] = {1, 1, 2, 2, 2, 3};
            return groups[first] == groups[second];
        },
        deleteFlags));
    assert(deleteFlags.size() == 6U);
    assert(deleteFlags[0] && !deleteFlags[1]);
    assert(deleteFlags[2] && deleteFlags[3] && !deleteFlags[4]);
    assert(!deleteFlags[5]);

    assert(dommy::curve::buildDuplicateDeletionPlan(
        0U,
        [](std::size_t, std::size_t) { return false; },
        deleteFlags));
    assert(deleteFlags.empty());
    deleteFlags.assign(1U, true);
    assert(!dommy::curve::buildDuplicateDeletionPlan(
        20000U,
        [](std::size_t, std::size_t) { return false; },
        deleteFlags));
    assert(deleteFlags.size() == 1U && deleteFlags[0]);
}

void testSignalMath() {
    dommy::signal::TimeSample sample;
    assert(dommy::signal::sampleTime(
        12.0, 1.0, 0.0, 24.0, false, sample));
    assert(near(sample.position, 0.5));
    assert(near(sample.completedCycles, 0.0));

    assert(dommy::signal::sampleTime(
        36.0, 1.0, 0.0, 24.0, true, sample));
    assert(near(sample.position, 0.5));
    assert(near(sample.completedCycles, 1.0));
    assert(dommy::signal::sampleTime(
        -6.0, 1.0, 0.0, 24.0, true, sample));
    assert(near(sample.position, 0.75));
    assert(near(sample.completedCycles, -1.0));
    assert(dommy::signal::sampleTime(
        12.0, 1.0, 24.0, 0.0, false, sample));
    assert(near(sample.position, 0.5));
    assert(dommy::signal::sampleTime(
        100.0, 1.0, 0.0, 24.0, false, sample));
    assert(near(sample.position, 1.0));

    assert(!dommy::signal::sampleTime(
        1.0, 1.0, 4.0, 4.0, false, sample));
    assert(!dommy::signal::sampleTime(
        std::numeric_limits<double>::infinity(),
        1.0, 0.0, 1.0, false, sample));

    double mapped = 0.0;
    assert(dommy::signal::mappedRampValue(2.0, 6.0, 0.25f, mapped));
    assert(near(mapped, 3.0));
    assert(dommy::signal::mappedRampValue(6.0, 2.0, 0.25f, mapped));
    assert(near(mapped, 3.0));
    assert(!dommy::signal::mappedRampValue(
        0.0, 1.0, std::numeric_limits<float>::quiet_NaN(), mapped));
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(dommy::signal::sampleTime(0.0, 1.0, -1e308, 1e308, true, sample));
    assert(sample.position == 0.5 && sample.completedCycles == 0.0);
    for (float ramp : {0.0f, 0.5f, 1.0f}) {
        assert(dommy::signal::mappedRampValue(-1e308, 1e308, ramp, mapped));
        assert(mapped == (ramp == 0.0f ? -1e308 : ramp == 1.0f ? 1e308 : 0.0));
    }
    assert(dommy::signal::sampleTime(std::ldexp(1.0, 60), 1.0, 0.0, 3.0, true, sample));
    assert(near(sample.position, 1.0 / 3.0));
    assert(dommy::signal::sampleTime(-std::ldexp(1.0, 60), 1.0, 0.0, 3.0, true, sample));
    assert(near(sample.position, 2.0 / 3.0));
    sample.position = 91.0; sample.completedCycles = 92.0;
    assert(!dommy::signal::sampleTime(1e308, 2.0, 0.0, 1.0, true, sample));
    assert(sample.position == 91.0 && sample.completedCycles == 92.0);
    assert(dommy::signal::outputValue(-1e308, 1e308, 0.0, 0.0, 2.0, 1.0, mapped));
    assert(mapped == 1e308);
    assert(dommy::signal::outputValue(3.0, 1e308, 1e308, 2.0, 2.0, 0.0, mapped));
    assert(mapped == 3.0);
    assert(dommy::signal::outputValue(0.0, 1e308, -1e308, 1.0, 2.0, 2.0, mapped));
    assert(mapped == 0.0);
    assert(dommy::signal::applyModulation(5.0, 2.0, 1e308, 2.0, 0.0, false, mapped));
    assert(mapped == 5.0);
    assert(dommy::signal::applyModulation(5.0, 2.0, 1e308, 2.0, 0.0, true, mapped));
    assert(mapped == 0.0);
    mapped = 91.0;
    assert(!dommy::signal::outputValue(0.0, NAN, 0.0, 0.0, 0.0, 0.0, mapped));
    assert(mapped == 91.0);
    assert(!dommy::signal::applyModulation(5.0, NAN, 1e308, 2.0, 0.0, false, mapped));
    assert(mapped == 91.0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testAhoGenerationPlan() {
    const double finiteInputs[] = {1.0, 0.25, 0.0, 1.0};
    dommy::generation::AhoGenerationPlan plan;
    assert(dommy::generation::buildAhoGenerationPlan(
        10U, 20, 8, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), plan));
    assert(plan.inputCurveCount == 10U);
    assert(plan.copiesPerCurve == 20U);
    assert(plan.pointsPerCurve == 8U);
    assert(plan.outputCurveCount == 200U);
    assert(plan.totalPointCount == 1600U);
    assert(dommy::generation::buildAhoGenerationPlan(
        10U, 0, 8, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), plan));
    assert(plan.outputCurveCount == 0U && plan.totalPointCount == 0U);
    assert(!dommy::generation::buildAhoGenerationPlan(
        10U, -1, 8, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), plan));
    assert(!dommy::generation::buildAhoGenerationPlan(
        10U, 20, 1, finiteInputs,
        sizeof(finiteInputs) / sizeof(finiteInputs[0]), plan));
    assert(!dommy::generation::buildAhoGenerationPlan(
        dommy::generation::kMaxGeneratedCurveCount, 2, 8,
        finiteInputs, sizeof(finiteInputs) / sizeof(finiteInputs[0]), plan));
    const double invalidInputs[] = {
        1.0, std::numeric_limits<double>::quiet_NaN()};
    assert(!dommy::generation::buildAhoGenerationPlan(
        1U, 1, 2, invalidInputs,
        sizeof(invalidInputs) / sizeof(invalidInputs[0]), plan));
}

void testClothSamplingPlan()
{
    dommy::curve::ClothSamplingPlan plan;
    assert(dommy::curve::checkedClothSamplingPlan(
        4, 5, false, 2.0, 6.0, -3.0, 7.0,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxWorkItemCount, plan));
    assert(plan.samplingU == 4U && plan.samplingV == 5U);
    assert(plan.curveCountU == 6U && plan.curveCountV == 7U);
    assert(plan.pointsPerUCurve == 12U && plan.pointsPerVCurve == 10U);
    assert(plan.totalCurveCount == 13U);
    assert(plan.totalPointCount == 142U);
    assert(plan.totalSurfaceQueries == 200U);
    assert(near(plan.stepU, 0.8));
    assert(near(plan.stepV, 10.0 / 6.0));
    assert(near(dommy::curve::clothParameterAt(
        plan.startU, plan.endU, plan.stepU, 0U, plan.curveCountU), 2.0));
    assert(near(dommy::curve::clothParameterAt(
        plan.startU, plan.endU, plan.stepU, 5U, plan.curveCountU), 6.0));

    assert(dommy::curve::checkedClothSamplingPlan(
        4, 5, true, 2.0, 6.0, -3.0, 7.0,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxWorkItemCount, plan));
    assert(plan.pointsPerUCurve == 22U && plan.pointsPerVCurve == 18U);
    assert(plan.totalPointCount == 258U);
    assert(plan.totalSurfaceQueries == 432U);

    const std::size_t preservedPointCount = plan.totalPointCount;
    assert(!dommy::curve::checkedClothSamplingPlan(
        0, 5, false, 0.0, 1.0, 0.0, 1.0, 100U, 100U, plan));
    assert(plan.totalPointCount == preservedPointCount);
    assert(!dommy::curve::checkedClothSamplingPlan(
        4, 5, false, 1.0, 1.0, 0.0, 1.0, 1000U, 1000U, plan));
    assert(!dommy::curve::checkedClothSamplingPlan(
        4, 5, false, 0.0,
        std::numeric_limits<double>::infinity(), 0.0, 1.0,
        1000U, 1000U, plan));
    assert(!dommy::curve::checkedClothSamplingPlan(
        50, 50, true, 0.0, 1.0, 0.0, 1.0, 1000U, 1000U, plan));
}

void testWireTopologyPlan()
{
    dommy::mesh::WireTopologyPlan plan;
    assert(dommy::mesh::checkedWireTopologyPlan(
        8U, 8U, 0.25, -0.5, 2.0, 3.0,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedFaceCount,
        dommy::generation::kMaxWorkItemCount, plan));
    assert(plan.vertexCount == 64U);
    assert(plan.faceCount == 56U);
    assert(plan.faceVertexCount == 224U);
    assert(plan.uvCoordinateCount == 72U);
    assert(dommy::mesh::hasValidWireTopology(plan));
    assert(plan.faceVertexIndices[220] == 55);
    assert(plan.faceVertexIndices[221] == 48);
    assert(plan.faceVertexIndices[222] == 56);
    assert(plan.faceVertexIndices[223] == 63);
    assert(plan.uvIndices[220] == 61);
    assert(plan.uvIndices[221] == 62);
    assert(plan.uvIndices[222] == 71);
    assert(plan.uvIndices[223] == 70);
    assert(near(plan.uCoordinates.front(), 0.25));
    assert(near(plan.uCoordinates[8], 2.25));
    assert(near(plan.vCoordinates.front(), -0.5));
    assert(near(plan.vCoordinates.back(), 2.5));

    const std::size_t preservedFaceCount = plan.faceCount;
    assert(!dommy::mesh::checkedWireTopologyPlan(
        1U, 8U, 0.0, 0.0, 1.0, 1.0, 100U, 100U, 1000U, plan));
    assert(plan.faceCount == preservedFaceCount);
    assert(!dommy::mesh::checkedWireTopologyPlan(
        8U, 2U, 0.0, 0.0, 1.0, 1.0, 100U, 100U, 1000U, plan));
    assert(!dommy::mesh::checkedWireTopologyPlan(
        8U, 8U, 0.0, 0.0,
        std::numeric_limits<double>::infinity(), 1.0,
        100U, 100U, 1000U, plan));
    assert(!dommy::mesh::checkedWireTopologyPlan(
        100U, 100U, 0.0, 0.0, 1.0, 1.0,
        1000U, 10000U, 100000U, plan));
}

void testSubdivisionBudget()
{
    dommy::mesh::SubdivisionBudget budget;
    assert(dommy::mesh::checkedSubdivisionBudget(
        0, 8U, 6U, 24U,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedFaceCount,
        dommy::generation::kMaxWorkItemCount, budget));
    assert(budget.estimatedFinalVertexCount == 8U);
    assert(budget.estimatedFinalFaceCount == 6U);
    assert(budget.estimatedFinalFaceVertexCount == 24U);
    assert(budget.estimatedTotalWorkItems == 38U);

    assert(dommy::mesh::checkedSubdivisionBudget(
        1, 8U, 6U, 24U,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedFaceCount,
        dommy::generation::kMaxWorkItemCount, budget));
    assert(budget.estimatedFinalVertexCount == 38U);
    assert(budget.estimatedFinalFaceCount == 24U);
    assert(budget.estimatedFinalFaceVertexCount == 96U);
    assert(budget.estimatedTotalWorkItems == 196U);

    assert(dommy::mesh::checkedSubdivisionBudget(
        dommy::mesh::kMaxSubdivisionLevel, 8U, 6U, 24U,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedFaceCount,
        dommy::generation::kMaxWorkItemCount, budget));
    assert(budget.estimatedFinalVertexCount == 655358U);
    assert(budget.estimatedFinalFaceCount == 393216U);
    assert(budget.estimatedFinalFaceVertexCount == 1572864U);

    const std::size_t preservedVertexCount =
        budget.estimatedFinalVertexCount;
    assert(!dommy::mesh::checkedSubdivisionBudget(
        dommy::mesh::kMaxSubdivisionLevel + 1, 8U, 6U, 24U,
        dommy::generation::kMaxGeneratedPointCount,
        dommy::generation::kMaxGeneratedFaceCount,
        dommy::generation::kMaxWorkItemCount, budget));
    assert(budget.estimatedFinalVertexCount == preservedVertexCount);
    assert(!dommy::mesh::checkedSubdivisionBudget(
        1, 8U, 6U, 24U, 20U, 100U, 1000U, budget));
    assert(!dommy::mesh::checkedSubdivisionBudget(
        1, 8U, 6U, 5U, 100U, 100U, 1000U, budget));
}

void testPerlinNoiseBoundaries()
{
    const noise::Perlin first(0U);
    const noise::Perlin second(0U);
    const noise::Perlin differentSeed(42U);
    const double sample = first.noise(0.25, -1.5, 3.75);
    assert(std::isfinite(sample));
    assert(sample == second.noise(0.25, -1.5, 3.75));
    assert(sample != differentSeed.noise(0.25, -1.5, 3.75));
    assert(near(sample, first.noise(256.25, -1.5, 3.75)));
    assert(first.noise(std::numeric_limits<double>::quiet_NaN()) == 0.0);
    assert(first.noise(std::numeric_limits<double>::infinity()) == 0.0);
    assert(std::isfinite(first.noise(std::numeric_limits<double>::max())));

    const noise::PerlinOctave octave(4, 7U, 0.5, 2.0);
    assert(std::isfinite(octave.noise(0.125, 0.25, 0.5)));
    assert(noise::PerlinOctave(-1).noise(0.25) == 0.0);
    assert(noise::PerlinOctave(noise::kMaxPerlinOctaves + 1).noise(0.25) ==
           0.0);
    assert(noise::PerlinOctave(
               2, 0U, std::numeric_limits<double>::infinity(), 2.0)
               .noise(0.25) == 0.0);
    assert(noise::PerlinOctave(
               2, 0U, 1.0, std::numeric_limits<double>::max())
               .noise(2.0) == 0.0);
}

void testMeshInputValidation() {
    std::size_t pointCount = 91U;
    assert(dommy::mesh::checkedMatchingPointCount(
        12U, 12U, 100U, pointCount));
    assert(pointCount == 12U);

    const std::size_t preserved = pointCount;
    assert(!dommy::mesh::checkedMatchingPointCount(
        12U, 11U, 100U, pointCount));
    assert(pointCount == preserved);
    assert(!dommy::mesh::checkedMatchingPointCount(
        101U, 101U, 100U, pointCount));
    assert(pointCount == preserved);

    assert(dommy::mesh::hasPerPointValues(12U, 12U));
    assert(!dommy::mesh::hasPerPointValues(11U, 12U));
}

void testStringReplacement() {
    std::string value("cache/${frame}/body_${frame}.pdb");
    assert(dommy::file::replaceAll(value, "${frame}", "12"));
    assert(value == "cache/12/body_12.pdb");

    value = "suffix-token";
    assert(dommy::file::replaceAll(value, "token", "x"));
    assert(value == "suffix-x");
    const std::string preserved = value;
    assert(!dommy::file::replaceAll(value, "", "ignored"));
    assert(value == preserved);
}

void testVdbParticleRasterPlan() {
    dommy::vdb::ParticleRasterParameters parameters;
    parameters.voxelSize = 1.0;
    parameters.halfBandVoxels = 3.0;
    parameters.radiusScale = 1.0;
    parameters.minimumRadiusVoxels = 0.0;
    parameters.maskWidth = 0.0;
    parameters.velocityMultiplier = 1.0;
    parameters.velocitySpacing = 1.0;

    std::size_t workItems = 0U;
    assert(dommy::vdb::checkedParticleRasterWork(
        10U, 1.0, 0.0, parameters,
        dommy::generation::kMaxWorkItemCount, workItems));
    assert(workItems == 7290U);

    parameters.rasterizeTrails = true;
    parameters.minimumRadiusVoxels = 1.0;
    parameters.rasterPassCount = 2U;
    assert(dommy::vdb::checkedParticleRasterWork(
        1U, 1.0, 2.0, parameters,
        dommy::generation::kMaxWorkItemCount, workItems));
    assert(workItems == 4394U * 18U);

    const std::size_t preservedWorkItems = workItems;
    parameters.voxelSize = 0.0;
    assert(!dommy::vdb::checkedParticleRasterWork(
        1U, 1.0, 0.0, parameters,
        dommy::generation::kMaxWorkItemCount, workItems));
    assert(workItems == preservedWorkItems);

    parameters.voxelSize = 0.001;
    parameters.rasterPassCount = 1U;
    assert(!dommy::vdb::checkedParticleRasterWork(
        100000U, 100.0, 0.0, parameters,
        dommy::generation::kMaxWorkItemCount, workItems));
    assert(!dommy::vdb::hasValidPointSamplingInput(
        dommy::generation::kMaxGeneratedPointCount + 1U, 1.0));
    assert(dommy::vdb::hasValidPointSamplingInput(1U, 0.1));

    const TestCollisionVec3 finiteVector = {{1.0, 2.0, 3.0}};
    const TestCollisionVec3 invalidVector = {{1.0,
        std::numeric_limits<double>::infinity(), 3.0}};
    double length = 0.0;
    assert(dommy::vdb::vectorLength(finiteVector, length));
    assert(near(length, std::sqrt(14.0)));
    assert(!dommy::vdb::vectorLength(invalidVector, length));
}

void testVdbCombinePlan() {
    dommy::vdb::CombinePlan plan;
    assert(dommy::vdb::buildCombinePlan(
               3U, 2U, true, true, false, true, plan) ==
           dommy::vdb::kCombinePlanSuccess);
    assert(plan.steps.size() == 2U);
    assert(plan.steps[0].aIndex == 0U && plan.steps[0].bIndex == 0U);
    assert(plan.steps[1].aIndex == 1U && plan.steps[1].bIndex == 1U);
    assert(plan.steps[0].emitOutput && plan.steps[1].emitOutput);
    assert(plan.unusedA && !plan.unusedB);

    assert(dommy::vdb::buildCombinePlan(
               3U, 2U, true, true, false, false, plan) ==
           dommy::vdb::kCombinePlanSuccess);
    assert(plan.steps.size() == 3U);
    for (std::size_t index = 0U; index < plan.steps.size(); ++index) {
        assert(plan.steps[index].aIndex == index);
        assert(plan.steps[index].bIndex == 0U);
        assert(plan.steps[index].emitOutput);
    }
    assert(!plan.unusedA && plan.unusedB);

    assert(dommy::vdb::buildCombinePlan(
               2U, 3U, true, true, true, true, plan) ==
           dommy::vdb::kCombinePlanSuccess);
    assert(plan.steps.size() == 3U);
    assert(plan.steps[0].aIndex == 0U);
    assert(!plan.steps[0].usePreviousA);
    assert(plan.steps[0].bIndex == 0U);
    assert(!plan.steps[0].emitOutput);
    assert(plan.steps[1].aIndex == dommy::vdb::kNoCombineGrid);
    assert(plan.steps[1].usePreviousA && plan.steps[1].bIndex == 1U);
    assert(!plan.steps[1].emitOutput);
    assert(plan.steps[2].usePreviousA && plan.steps[2].bIndex == 2U);
    assert(plan.steps[2].emitOutput);
    assert(plan.unusedA && !plan.unusedB);

    assert(dommy::vdb::buildCombinePlan(
               3U, 0U, true, false, false, true, plan) ==
           dommy::vdb::kCombinePlanSuccess);
    assert(plan.steps.size() == 3U);
    assert(plan.steps[2].aIndex == 2U);
    assert(plan.steps[2].bIndex == dommy::vdb::kNoCombineGrid);

    assert(dommy::vdb::buildCombinePlan(
               0U, 3U, false, true, false, true, plan) ==
           dommy::vdb::kCombinePlanSuccess);
    assert(plan.steps.size() == 3U);
    assert(plan.steps[2].aIndex == dommy::vdb::kNoCombineGrid);
    assert(plan.steps[2].bIndex == 2U);

    assert(dommy::vdb::buildCombinePlan(
               0U, 3U, false, true, false, false, plan) ==
           dommy::vdb::kCombinePlanSuccess);
    assert(plan.steps.size() == 1U && plan.steps[0].bIndex == 0U);
    assert(plan.unusedB);

    const std::size_t preservedStepCount = plan.steps.size();
    assert(dommy::vdb::buildCombinePlan(
               0U, 1U, true, true, false, true, plan) ==
           dommy::vdb::kCombinePlanMissingA);
    assert(plan.steps.size() == preservedStepCount);
    assert(dommy::vdb::buildCombinePlan(
               1U, 0U, true, true, false, true, plan) ==
           dommy::vdb::kCombinePlanMissingB);
    assert(dommy::vdb::buildCombinePlan(
               dommy::vdb::kMaxVdbGridCount + 1U,
               1U, true, true, false, true, plan) ==
           dommy::vdb::kCombinePlanGridLimitExceeded);
}

void testFluidGridPlan() {
    dommy::fluid::GridPlan plan;
    assert(dommy::fluid::buildGridPlan(
        4U, 3U, 2U, 8.0, 6.0, 4.0, -4.0, -3.0, -2.0,
        2U, true, dommy::generation::kMaxWorkItemCount, plan));

    assert(plan.scalar.voxelCount == 24U);
    assert(plan.scalar.dimensions[0] == 4U);
    assert(plan.scalar.dimensions[1] == 3U);
    assert(plan.scalar.dimensions[2] == 2U);
    assert(near(plan.cellSize[0], 2.0));
    assert(near(plan.cellSize[1], 2.0));
    assert(near(plan.cellSize[2], 2.0));
    assert(near(plan.scalar.origin[0], -3.0));
    assert(near(plan.scalar.origin[1], -2.0));
    assert(near(plan.scalar.origin[2], -1.0));
    assert(near(
        plan.scalar.origin[0] +
            static_cast<double>(plan.scalar.dimensions[0] - 1U) *
                plan.cellSize[0],
        3.0));

    assert(plan.velocity[0].voxelCount == 30U);
    assert(plan.velocity[1].voxelCount == 32U);
    assert(plan.velocity[2].voxelCount == 36U);
    assert(plan.velocity[0].dimensions[0] == 5U);
    assert(plan.velocity[1].dimensions[1] == 4U);
    assert(plan.velocity[2].dimensions[2] == 3U);
    assert(near(plan.velocity[0].origin[0], -4.0));
    assert(near(plan.velocity[0].origin[1], -2.0));
    assert(near(plan.velocity[1].origin[0], -3.0));
    assert(near(plan.velocity[1].origin[1], -3.0));
    assert(near(plan.velocity[2].origin[2], -2.0));
    assert(near(
        plan.velocity[0].origin[0] +
            static_cast<double>(plan.velocity[0].dimensions[0] - 1U) *
                plan.cellSize[0],
        4.0));
    assert(plan.totalWorkItems == 146U);

    dommy::fluid::GridPlan scalarOnly;
    assert(dommy::fluid::buildGridPlan(
        4U, 3U, 2U, 8.0, 6.0, 4.0, -4.0, -3.0, -2.0,
        1U, false, dommy::generation::kMaxWorkItemCount, scalarOnly));
    assert(scalarOnly.totalWorkItems == 24U);
    assert(scalarOnly.velocity[0].voxelCount == 0U);

    const std::size_t preservedWorkItems = plan.totalWorkItems;
    assert(!dommy::fluid::buildGridPlan(
        0U, 3U, 2U, 8.0, 6.0, 4.0, -4.0, -3.0, -2.0,
        1U, false, dommy::generation::kMaxWorkItemCount, plan));
    assert(plan.totalWorkItems == preservedWorkItems);
    assert(!dommy::fluid::buildGridPlan(
        4U, 3U, 2U, 0.0, 6.0, 4.0, -4.0, -3.0, -2.0,
        1U, false, dommy::generation::kMaxWorkItemCount, plan));
    assert(!dommy::fluid::buildGridPlan(
        4U, 3U, 2U, 8.0, 6.0, 4.0,
        std::numeric_limits<double>::infinity(), -3.0, -2.0,
        1U, false, dommy::generation::kMaxWorkItemCount, plan));
    assert(!dommy::fluid::buildGridPlan(
        1000U, 1000U, 1000U, 8.0, 6.0, 4.0, -4.0, -3.0, -2.0,
        1U, false, dommy::generation::kMaxWorkItemCount, plan));
    assert(!dommy::fluid::buildGridPlan(
        4U, 3U, 2U, 8.0, 6.0, 4.0, -4.0, -3.0, -2.0,
        3U, true, 150U, plan));
}

void testNumericValidation() {
    int convertedInteger = 7;
    assert(dommy::numeric::checkedNumericCast(42.0L, convertedInteger));
    assert(convertedInteger == 42);
    const int preservedInteger = convertedInteger;
    assert(!dommy::numeric::checkedNumericCast(
        static_cast<long double>(std::numeric_limits<int>::max()) + 1.0L,
        convertedInteger));
    assert(convertedInteger == preservedInteger);

    unsigned int convertedUnsigned = 9U;
    assert(!dommy::numeric::checkedNumericCast(
        -1.0L, convertedUnsigned));
    assert(convertedUnsigned == 9U);

    float converted = 7.0f;
    assert(!dommy::numeric::checkedNumericCast(
        std::numeric_limits<long double>::infinity(), converted));
    assert(converted == 7.0f);
    assert(dommy::numeric::checkedFloatCast(42.5, converted));
    assert(converted == 42.5f);
    assert(dommy::numeric::checkedFloatCast(
        static_cast<double>(std::numeric_limits<float>::max()), converted));

    const float preserved = converted;
    assert(!dommy::numeric::checkedFloatCast(
        std::numeric_limits<double>::infinity(), converted));
    assert(converted == preserved);
    assert(!dommy::numeric::checkedFloatCast(
        static_cast<double>(std::numeric_limits<float>::max()) * 2.0,
        converted));
    assert(converted == preserved);

    assert(dommy::numeric::clampUnitToFloat(-2.0, converted));
    assert(converted == 0.0f);
    assert(dommy::numeric::clampUnitToFloat(2.0, converted));
    assert(converted == 1.0f);
    assert(dommy::numeric::clampUnitToFloat(0.25, converted));
    assert(converted == 0.25f);
    assert(!dommy::numeric::clampUnitToFloat(
        std::numeric_limits<double>::quiet_NaN(), converted));

    assert(dommy::numeric::isWithinInclusiveBounds(5.0, 1.0, 10.0));
    assert(dommy::numeric::isWithinInclusiveBounds(5.0, 10.0, 1.0));
    assert(dommy::numeric::isWithinInclusiveBounds(1.0, 10.0, 1.0));
    assert(!dommy::numeric::isWithinInclusiveBounds(11.0, 10.0, 1.0));
    assert(!dommy::numeric::isWithinInclusiveBounds(
        std::numeric_limits<double>::quiet_NaN(), 1.0, 10.0));

    double rangeValue = -1.0;
    assert(dommy::numeric::normalizeRange(15.0, 10.0, 20.0, rangeValue));
    assert(rangeValue == 0.5);
    assert(!dommy::numeric::normalizeRange(15.0, 10.0, 10.0, rangeValue));
    assert(rangeValue == 0.5);

    assert(dommy::numeric::wrapPeriodic(23.0, 10.0, 20.0, rangeValue));
    assert(rangeValue == 13.0);
    assert(dommy::numeric::wrapPeriodic(7.0, 10.0, 20.0, rangeValue));
    assert(rangeValue == 17.0);
    assert(dommy::numeric::wrapPeriodic(20.0, 10.0, 20.0, rangeValue));
    assert(rangeValue == 10.0);
    assert(!dommy::numeric::wrapPeriodic(
        std::numeric_limits<double>::infinity(),
        10.0, 20.0, rangeValue));
    assert(rangeValue == 10.0);
}

void testAttributeBlendMath() {
    double doubleResult = 0.0;
    assert(dommy::attribute::checkedLinearBlend(
        10.0, 20.0, 0.25, doubleResult));
    assert(doubleResult == 12.5);

    float floatResult = 0.0F;
    assert(dommy::attribute::checkedLinearBlend(
        2.0F, 6.0F, 0.5, floatResult));
    assert(floatResult == 4.0F);

    int integerResult = 0;
    assert(dommy::attribute::checkedLinearBlend(
        1, 4, 0.5, integerResult));
    assert(integerResult == 2);
    assert(dommy::attribute::checkedLinearBlend(
        INT_MIN, INT_MAX, 0.5, integerResult));
    assert(integerResult == 0);

    const int preservedInteger = integerResult;
    assert(!dommy::attribute::checkedLinearBlend(
        INT_MIN, INT_MAX, 2.0, integerResult));
    assert(integerResult == preservedInteger);
    assert(!dommy::attribute::checkedLinearBlend(
        1, 2, std::numeric_limits<double>::quiet_NaN(),
        integerResult));
    assert(integerResult == preservedInteger);
}

void testRandomIntGeneration() {
    std::vector<int> first;
    std::vector<int> second;
    dommy::random::generateIntValues(42, 7, 3, 128, first);
    dommy::random::generateIntValues(42, 3, 7, 128, second);
    assert(first == second);
    assert(first.size() == 128);
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index] >= 3 && first[index] <= 7);
    }
}

void testPointPackingMath() {
    float position = -1.0f;
    assert(dommy::packing::normalizedPosition(5.0f, 0.0f, 10.0f, position));
    assert(near(position, 0.5));
    assert(dommy::packing::normalizedPosition(5.0f, 5.0f, 0.0f, position));
    assert(near(position, 0.5));
    assert(!dommy::packing::normalizedPosition(
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f, position));

    std::mt19937 firstEngine(17U);
    std::mt19937 secondEngine(17U);
    assert(dommy::packing::uniform(firstEngine, 2.0f, 1.0f) ==
           dommy::packing::uniform(secondEngine, 1.0f, 2.0f));
    assert(dommy::packing::isFinite(PackedCircle(MPoint(0.0, 0.0, 0.0), 1.0f)));
    assert(!dommy::packing::isFinite(PackedCircle(MPoint(0.0, 0.0, 0.0), -1.0f)));
}

void testGeometryLayoutArithmetic() {
    std::size_t scalarCount = 0;
    assert(dommy::geometry::checkedTupleScalarCount(4, 3, scalarCount));
    assert(scalarCount == 12);
    assert(!dommy::geometry::checkedTupleScalarCount(4, 0, scalarCount));

    std::vector<float> tupleBuffer;
    assert(dommy::geometry::checkedAssignTupleBuffer(tupleBuffer, 2, 3, 1.0f));
    assert(tupleBuffer.size() == 6 && tupleBuffer[5] == 1.0f);
    assert(!dommy::geometry::checkedAssignTupleBuffer(
        tupleBuffer, std::numeric_limits<std::size_t>::max(), 2, 0.0f));
    assert(tupleBuffer.size() == 6);
    const std::vector<float> appended(2, 2.0f);
    assert(dommy::geometry::checkedAppendBuffer(tupleBuffer, appended));
    assert(tupleBuffer.size() == 8 && tupleBuffer[7] == 2.0f);

    std::size_t indexCount = 0;
    assert(dommy::geometry::checkedAccumulatePolylineIndices(0, indexCount));
    assert(dommy::geometry::checkedAccumulatePolylineIndices(1, indexCount));
    assert(indexCount == 0);
    assert(dommy::geometry::checkedAccumulatePolylineIndices(3, indexCount));
    assert(indexCount == 4);
    assert(dommy::geometry::checkedAccumulatePolylineIndices(2, indexCount));
    assert(indexCount == 6);

    indexCount = std::numeric_limits<std::size_t>::max() - 1;
    assert(!dommy::geometry::checkedAccumulatePolylineIndices(2, indexCount));
}

void testViewportBufferElementCount() {
    MHWRender::MVertexBuffer buffer;
    const MHWRender::MVertexBufferDescriptor positionDescriptor(3);
    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, positionDescriptor, 12, 3) != NULL);
    assert(buffer.lastCount() == 4);
    assert(buffer.writeOnly());

    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, positionDescriptor, 11, 3) == NULL);
    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, MHWRender::MVertexBufferDescriptor(0), 12, 3) == NULL);
    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, MHWRender::MVertexBufferDescriptor(-1), 12, 3) == NULL);
    assert(dommy::viewport::acquireFloatBuffer(
        NULL, positionDescriptor, 12, 3) == NULL);
    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, positionDescriptor, 12, 2) == NULL);
    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, positionDescriptor, 12, 0) == NULL);
    assert(dommy::viewport::acquireFloatBuffer(
        &buffer, positionDescriptor, 12, 5) == NULL);
    for (const auto type : {MHWRender::MGeometry::kDouble, MHWRender::MGeometry::kChar,
         MHWRender::MGeometry::kUnsignedChar, MHWRender::MGeometry::kInt16,
         MHWRender::MGeometry::kUnsignedInt16, MHWRender::MGeometry::kInt32,
         MHWRender::MGeometry::kUnsignedInt32}) {
        assert(dommy::viewport::acquireFloatBuffer(
            &buffer, MHWRender::MVertexBufferDescriptor(3, type), 12, 3) == NULL);
    }
    for (const int dimension : {1, 2, 3, 4}) {
        assert(dommy::viewport::acquireFloatBuffer(
            &buffer, MHWRender::MVertexBufferDescriptor(dimension),
            static_cast<std::size_t>(dimension) * 2U, dimension) != NULL);
        assert(buffer.lastCount() == 2U);
    }

    MHWRender::MIndexBuffer indexBuffer;
    assert(dommy::viewport::acquireIndexBuffer(&indexBuffer, 8U) != NULL);
    assert(indexBuffer.lastCount() == 8U);
    assert(indexBuffer.writeOnly());
    assert(dommy::viewport::acquireIndexBuffer(&indexBuffer, 0U) == NULL);
    assert(dommy::viewport::acquireIndexBuffer(NULL, 8U) == NULL);
    assert(dommy::viewport::acquireIndexBuffer(
        &indexBuffer,
        static_cast<std::size_t>(
            std::numeric_limits<unsigned int>::max()) + 1U) == NULL);

    MHWRender::MGeometry geometry;
    unsigned int* indices = NULL;
    MHWRender::MIndexBuffer* createdBuffer =
        dommy::viewport::createIndexBuffer(geometry, 6U, indices);
    assert(createdBuffer != NULL);
    assert(indices != NULL);
    assert(createdBuffer->lastCount() == 6U);
    assert(dommy::viewport::createIndexBuffer(
        geometry, 0U, indices) == NULL);
    assert(indices == NULL);
}

void testViewportPointBudget() {
    dommy::viewport::PointSamplePlan plan;
    std::vector<std::size_t> available;
    available.push_back(4);
    available.push_back(0);
    available.push_back(2);

    assert(dommy::viewport::buildPointSamplePlan(available, 10, false, plan));
    assert(plan.totalSampleCount == 6);
    assert(plan.offsets[0] == 0 && plan.offsets[1] == 4 && plan.offsets[2] == 4);
    assert(plan.sampleCounts[0] == 4 && plan.sampleCounts[1] == 0 &&
           plan.sampleCounts[2] == 2);

    assert(dommy::viewport::buildPointSamplePlan(available, 4, false, plan));
    assert(plan.totalSampleCount == 4);
    assert(plan.sampleCounts[0] == 2 && plan.sampleCounts[1] == 0 &&
           plan.sampleCounts[2] == 2);

    assert(dommy::viewport::buildPointSamplePlan(available, 10, true, plan));
    assert(plan.totalSampleCount == 2);
    assert(plan.sampleCounts[0] == 1 && plan.sampleCounts[1] == 0 &&
           plan.sampleCounts[2] == 1);

    available.assign(5, 1);
    assert(dommy::viewport::buildPointSamplePlan(available, 2, false, plan));
    assert(plan.totalSampleCount == 2);
    std::size_t selectedItems = 0;
    for (std::size_t index = 0; index < plan.sampleCounts.size(); ++index) {
        selectedItems += plan.sampleCounts[index];
    }
    assert(selectedItems == 2);

    available.clear();
    available.push_back(8);
    available.push_back(1);
    available.push_back(0);
    available.push_back(5);
    for (std::size_t budget = 0; budget <= 16; ++budget) {
        assert(dommy::viewport::buildPointSamplePlan(
            available, budget, false, plan));
        assert(plan.totalSampleCount == std::min<std::size_t>(14, budget));
        std::size_t runningOffset = 0;
        for (std::size_t index = 0; index < available.size(); ++index) {
            assert(plan.offsets[index] == runningOffset);
            assert(plan.sampleCounts[index] <= available[index]);
            runningOffset += plan.sampleCounts[index];
        }
        assert(runningOffset == plan.totalSampleCount);

        assert(dommy::viewport::buildPointSamplePlan(
            available, budget, true, plan));
        assert(plan.totalSampleCount == std::min<std::size_t>(3, budget));
    }

    const dommy::viewport::PointSamplePlan preservedPlan = plan;
    available.clear();
    available.push_back(std::numeric_limits<std::size_t>::max());
    available.push_back(1);
    assert(!dommy::viewport::buildPointSamplePlan(available, 2, false, plan));
    assert(plan.totalSampleCount == preservedPlan.totalSampleCount);
    assert(plan.sampleCounts == preservedPlan.sampleCounts);
}

void testCollisionMeshInputValidation() {
    const TestCollisionVec3 vertices[] = {
        {{0.0, 0.0, 0.0}},
        {{1.0, 0.0, 0.0}},
        {{0.0, 1.0, 0.0}}
    };
    const TestCollisionVec3 normals[] = {
        {{0.0, 0.0, 1.0}},
        {{0.0, 0.0, 1.0}},
        {{0.0, 0.0, 1.0}}
    };
    const unsigned int indices[] = {0, 1, 2};
    assert(dommy::bullet::hasValidCollisionMeshInput(
        vertices, 3, normals, indices, 3));
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        static_cast<const TestCollisionVec3*>(NULL), 3, normals, indices, 3));
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        vertices, 3, normals, indices, 2));

    const unsigned int invalidIndices[] = {0, 1, 3};
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        vertices, 3, normals, invalidIndices, 3));
    const unsigned int degenerateIndices[] = {0, 1, 1};
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        vertices, 3, normals, degenerateIndices, 3));
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        vertices,
        dommy::generation::kMaxGeneratedPointCount + 1U,
        normals, indices, 3));
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        vertices, 3, normals, indices,
        (dommy::generation::kMaxGeneratedFaceCount + 1U) * 3U));

    TestCollisionVec3 invalidVertices[3] = {vertices[0], vertices[1], vertices[2]};
    invalidVertices[1].values[0] = std::numeric_limits<double>::infinity();
    assert(!dommy::bullet::hasValidCollisionMeshInput(
        invalidVertices, 3, normals, indices, 3));

    assert(dommy::bullet::hasValidSphereInput(1.0f));
    assert(!dommy::bullet::hasValidSphereInput(0.0f));
    assert(!dommy::bullet::hasValidSphereInput(
        std::numeric_limits<float>::infinity()));
    assert(dommy::bullet::hasValidPlaneInput(vertices[2], 0.0f));
    assert(!dommy::bullet::hasValidPlaneInput(vertices[0], 0.0f));
    const TestCollisionVec3 halfExtents = {{1.0, 2.0, 3.0}};
    const TestCollisionVec3 invalidHalfExtents = {{1.0, 0.0, 3.0}};
    assert(dommy::bullet::hasValidBoxInput(halfExtents));
    assert(!dommy::bullet::hasValidBoxInput(invalidHalfExtents));
    const TestCollisionQuat identity = {{1.0, 0.0, 0.0, 0.0}};
    const TestCollisionQuat zero = {{0.0, 0.0, 0.0, 0.0}};
    assert(dommy::bullet::hasValidQuaternion(identity));
    assert(!dommy::bullet::hasValidQuaternion(zero));

    const std::vector<float> softCoordinates = {
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    const std::vector<int> softIndices = {0, 1, 2};
    assert(dommy::bullet::hasValidSoftBodyMeshInput(
        softCoordinates, softIndices));
    std::vector<int> invalidSoftIndices = softIndices;
    invalidSoftIndices[2] = -1;
    assert(!dommy::bullet::hasValidSoftBodyMeshInput(
        softCoordinates, invalidSoftIndices));
    invalidSoftIndices[2] = 1;
    assert(!dommy::bullet::hasValidSoftBodyMeshInput(
        softCoordinates, invalidSoftIndices));
}

void testShapeFillAndViewportGeometryBudgets() {
    dommy::generation::ShapeFillPlan fillPlan;
    assert(dommy::generation::buildShapeFillPlan(
        1.0, 2.0, 3.0, 1, fillPlan));
    assert(fillPlan.candidateCount == 8U);
    assert(fillPlan.sampleCounts[0] == 2U &&
           fillPlan.sampleCounts[1] == 2U &&
           fillPlan.sampleCounts[2] == 2U);
    assert(near(fillPlan.sampleSteps[0], 1.0));

    assert(dommy::generation::buildShapeFillPlan(
        0.0, 2.0, 0.0, 4, fillPlan));
    assert(fillPlan.candidateCount == 5U);
    assert(fillPlan.sampleCounts[0] == 1U &&
           fillPlan.sampleCounts[1] == 5U &&
           fillPlan.sampleCounts[2] == 1U);
    assert(fillPlan.sampleSteps[0] == 0.0 && fillPlan.sampleSteps[2] == 0.0);
    const std::size_t preservedCandidateCount = fillPlan.candidateCount;
    assert(!dommy::generation::buildShapeFillPlan(
        1.0, 1.0, 1.0, 0, fillPlan));
    assert(fillPlan.candidateCount == preservedCandidateCount);
    assert(dommy::generation::buildShapeFillPlan(
        1.0, 1.0, 1.0, 214, fillPlan));
    assert(!dommy::generation::buildShapeFillPlan(
        1.0, 1.0, 1.0, 215, fillPlan));

    dommy::viewport::GeometryBudget geometryBudget(10U);
    assert(geometryBudget.take(4U) == 4U);
    assert(geometryBudget.remaining() == 6U);
    assert(geometryBudget.take(8U) == 6U);
    assert(geometryBudget.remaining() == 0U);

    std::size_t vertexCount = 0U;
    std::size_t componentCount = 0U;
    std::size_t indexCount = 0U;
    assert(dommy::viewport::checkedWireBoxLayout(
        2U, vertexCount, componentCount, indexCount));
    assert(vertexCount == 16U && componentCount == 48U && indexCount == 48U);
    assert(!dommy::viewport::checkedWireBoxLayout(
        dommy::viewport::kMaxVdbViewportWireBoxCount + 1U,
        vertexCount, componentCount, indexCount));
    assert(dommy::viewport::checkedSurfaceIndexCount(
        2U, 3U, 32U, indexCount));
    assert(indexCount == 21U);
    assert(!dommy::viewport::checkedSurfaceIndexCount(
        2U, 3U, 20U, indexCount));
}

void testBulletMassProperties() {
    const vec3f vertices[] = {
        vec3f(0.0f, 0.0f, 0.0f),
        vec3f(1.0f, 0.0f, 0.0f),
        vec3f(0.0f, 1.0f, 0.0f),
        vec3f(0.0f, 0.0f, 1.0f)
    };
    const unsigned int indices[] = {
        0U, 2U, 1U,
        0U, 1U, 3U,
        0U, 3U, 2U,
        1U, 2U, 3U
    };
    dommy::bullet::MassProperties properties;
    assert(dommy::bullet::computeMassProperties(
        vertices, 4U, indices, 12U, properties));
    assert(near(properties.volume, 1.0 / 6.0));
    assert(near(properties.center[0], 0.25));
    assert(near(properties.center[1], 0.25));
    assert(near(properties.center[2], 0.25));
    assert(properties.inertia(0, 0) > 0.0f);
    assert(near(properties.inertia(0, 1), properties.inertia(1, 0)));

    unsigned int reversedIndices[12];
    for (std::size_t index = 0; index < 12U; index += 3U) {
        reversedIndices[index] = indices[index];
        reversedIndices[index + 1U] = indices[index + 2U];
        reversedIndices[index + 2U] = indices[index + 1U];
    }
    dommy::bullet::MassProperties reversed;
    assert(dommy::bullet::computeMassProperties(
        vertices, 4U, reversedIndices, 12U, reversed));
    assert(near(reversed.volume, properties.volume));
    assert(near(reversed.center[0], properties.center[0]));
    assert(near(reversed.inertia(0, 0), properties.inertia(0, 0)));

    vec3f translated[4];
    for (int index = 0; index < 4; ++index) {
        translated[index] = vertices[index] + vec3f(1000000.0f, -1000000.0f, 500000.0f);
    }
    dommy::bullet::MassProperties translatedProperties;
    assert(dommy::bullet::computeMassProperties(
        translated, 4U, indices, 12U, translatedProperties));
    assert(near(translatedProperties.volume, properties.volume));
    assert(near(translatedProperties.center[0], 1000000.25, 0.1));
    assert(near(translatedProperties.inertia(0, 0), properties.inertia(0, 0), 1.0e-5));

    const vec3f coplanar[] = {
        vec3f(0.0f, 0.0f, 0.0f), vec3f(1.0f, 0.0f, 0.0f),
        vec3f(0.0f, 1.0f, 0.0f), vec3f(1.0f, 1.0f, 0.0f)
    };
    dommy::bullet::MassProperties rejected;
    rejected.volume = 7.0f;
    assert(!dommy::bullet::computeMassProperties(
        coplanar, 4U, indices, 12U, rejected));
    assert(rejected.volume == 7.0f);
    const unsigned int invalidIndices[] = {
        0U, 2U, 1U, 0U, 1U, 4U, 0U, 3U, 2U, 1U, 2U, 3U};
    assert(!dommy::bullet::computeMassProperties(
        vertices, 4U, invalidIndices, 12U, rejected));

    const std::vector<vec3f> vertexVector(vertices, vertices + 4U);
    const std::vector<unsigned int> indexVector(indices, indices + 12U);
    dommy::bullet::MassProperties scaled;
    assert(dommy::bullet::computeScaledMassProperties(
        vertexVector, indexVector, vec3f(2.0f, 2.0f, 2.0f), scaled));
    assert(near(scaled.volume, properties.volume * 8.0));
    assert(!dommy::bullet::computeScaledMassProperties(
        vertexVector, indexVector,
        vec3f(std::numeric_limits<float>::infinity(), 1.0f, 1.0f), scaled));
}

void testRangeRemapping() {
    float normalized = 0.0f;
    assert(dommy::math::normalizeRange(2.0f, 2.0f, 4.0f, normalized));
    assert(near(normalized, 0.0));
    assert(dommy::math::normalizeRange(3.0f, 2.0f, 4.0f, normalized));
    assert(near(normalized, 0.5));
    assert(near(dommy::math::denormalizeRange(normalized, 2.0f, 4.0f), 3.0));

    assert(dommy::math::normalizeRange(-3.0f, -4.0f, -2.0f, normalized));
    assert(near(normalized, 0.5));
    assert(dommy::math::normalizeRange(0.0f, -2.0f, 2.0f, normalized));
    assert(near(normalized, 0.5));
    assert(dommy::math::normalizeRange(3.0f, 4.0f, 2.0f, normalized));
    assert(near(normalized, 0.5));

    normalized = 99.0f;
    assert(!dommy::math::normalizeRange(1.0f, 1.0f, 1.0f, normalized));
    assert(normalized == 0.0f);
    assert(!dommy::math::normalizeRange(
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f, normalized));
    assert(normalized == 0.0f);
    assert(dommy::math::normalizeRange(
        0.0f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        normalized));
    assert(normalized == 0.5f);
    std::feclearexcept(FE_ALL_EXCEPT);
    double wideNormalized = 91.0;
    assert(dommy::math::normalizeRange(0.0, -1e308, 1e308, wideNormalized));
    assert(wideNormalized == 0.5);
    assert(dommy::numeric::normalizeRange(0.0, -1e308, 1e308, wideNormalized));
    assert(wideNormalized == 0.5);
    assert(!dommy::numeric::normalizeRange(0.0, 1e308, -1e308, wideNormalized));
    assert(wideNormalized == 0.5);
    const long double longMaximum = std::numeric_limits<long double>::max();
    long double longNormalized = 91.0L;
    assert(dommy::math::normalizeRange(0.0L, -longMaximum, longMaximum, longNormalized));
    assert(longNormalized == 0.5L);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    const double narrowMinimum = 1.0e20;
    const double narrowMaximum = std::nextafter(
        narrowMinimum, std::numeric_limits<double>::infinity());
    double narrowNormalized = 0.0;
    assert(dommy::math::normalizeRange(
        narrowMaximum, narrowMinimum, narrowMaximum, narrowNormalized));
    assert(near(narrowNormalized, 1.0));

    const double extreme = std::numeric_limits<double>::max();
    assert(dommy::math::denormalizeRange(0.5, -extreme, extreme) == 0.0);
    assert(dommy::math::denormalizeRange(1e16, 1.0, 1.0) == 1.0);
    double fitted = 91.0;
    assert(dommy::math::fitFiniteValue(1e16, 0.0, 1.0, 1.0, 1.0, fitted));
    assert(fitted == 1.0);
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(dommy::math::checkedLinearInterpolation(-extreme, extreme, 0.5, fitted));
    assert(fitted == 0.0);
    assert(dommy::math::checkedLinearInterpolation(-extreme, extreme, 0.0, fitted));
    assert(fitted == -extreme);
    assert(dommy::math::checkedLinearInterpolation(-extreme, extreme, 1.0, fitted));
    assert(fitted == extreme);
    fitted = 91.0;
    assert(!dommy::math::checkedLinearInterpolation(0.0, extreme, 2.0, fitted));
    assert(fitted == 91.0);
    assert(!dommy::math::checkedLinearInterpolation(1.0, 1.0,
        std::numeric_limits<double>::infinity(), fitted));
    assert(fitted == 91.0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
    assert(dommy::math::denormalizeRange(0.5L, -longMaximum, longMaximum) == 0.0L);
    dommy::numeric::detail::BinaryAccumulator<136U, -2148> floatRounding;
    assert(floatRounding.add(1.0) && floatRounding.add(std::ldexp(1.0, -24)) &&
           floatRounding.add(std::ldexp(1.0, -80)));
    float roundedFloat = 0.0f;
    assert(floatRounding.mean(1U, roundedFloat));
    assert(roundedFloat == std::nextafter(1.0f, 2.0f));
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(dommy::numeric::wrapPeriodic(0.0, -1e308, 1e308, fitted));
    assert(fitted == 0.0);
    assert(dommy::numeric::wrapPeriodic(1e308, -1e308, 0.0, fitted));
    assert(fitted == -1e308);
    assert(dommy::numeric::wrapPeriodic(std::ldexp(1.0, 60), 0.0, 3.0, fitted));
    assert(fitted == 1.0);
    assert(dommy::numeric::wrapPeriodic(-std::ldexp(1.0, 60), 0.0, 3.0, fitted));
    assert(fitted == 2.0);
    assert(dommy::numeric::wrapPeriodic(std::ldexp(1.0, 60), 0.5, 3.5, fitted));
    assert(fitted == 1.0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testGroupSelectionSafety() {
    std::vector<bool> selection(10U, false);
    dommy::group::selectPeriodicRange(0, 10, 1, 2, 10U, selection);
    for (std::size_t index = 0U; index < selection.size(); ++index) {
        assert(selection[index] == (index % 2U == 0U));
    }

    std::fill(selection.begin(), selection.end(), false);
    dommy::group::selectPeriodicRange(8, 2, 2, 3, 10U, selection);
    for (std::size_t index = 0U; index < selection.size(); ++index) {
        const bool expected =
            index >= 2U && index < 8U && ((index - 2U) % 3U) < 2U;
        assert(selection[index] == expected);
    }

    std::fill(selection.begin(), selection.end(), false);
    dommy::group::selectPeriodicRange(-50, 50, 0, 0, 10U, selection);
    assert(std::find(selection.begin(), selection.end(), true) ==
           selection.end());

    assert(dommy::group::isInsideOpenBox(
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 4.0, 6.0));
    assert(!dommy::group::isInsideOpenBox(
        1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 4.0, 6.0));
    assert(!dommy::group::isInsideOpenBox(
        std::numeric_limits<double>::max(), 0.0, 0.0,
        std::numeric_limits<double>::max(), 0.0, 0.0,
        std::numeric_limits<double>::max(), 4.0, 6.0));

    assert(dommy::group::isInsideEllipsoid(
        2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 4.0, 6.0));
    assert(!dommy::group::isInsideEllipsoid(
        2.1, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 4.0, 6.0));
    assert(!dommy::group::isInsideEllipsoid(
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0, 6.0));
    assert(!dommy::group::isInsideEllipsoid(
        std::numeric_limits<double>::quiet_NaN(),
        0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 4.0, 6.0));
}

void testDirectionMathSafety() {
    const Imath::V3f from(1.0f, 2.0f, 3.0f);
    const Imath::V3f to(4.0f, 6.0f, 3.0f);
    dommy::direction::Delta delta;
    assert(dommy::direction::difference(from, to, delta));
    assert(near(delta.length, 5.0));

    Imath::V3f direction;
    assert(dommy::direction::assignDelta(delta, direction));
    assert(near(direction[0], 3.0));
    assert(near(direction[1], 4.0));
    assert(near(direction[2], 0.0));

    assert(dommy::direction::assignFinite(to, direction));
    assert(near(direction[0], 4.0));
    assert(near(direction[1], 6.0));
    assert(near(direction[2], 3.0));
    assert(dommy::direction::assignDelta(delta, direction));

    Imath::V3f adjusted;
    assert(dommy::direction::adjust(
        direction, true, true, 2.0, adjusted));
    assert(near(adjusted[0], -1.2));
    assert(near(adjusted[1], -1.6));

    const Imath::V3f zero(0.0f, 0.0f, 0.0f);
    assert(dommy::direction::adjust(zero, false, true, 10.0, adjusted));
    assert(adjusted[0] == 0.0f &&
           adjusted[1] == 0.0f &&
           adjusted[2] == 0.0f);

    assert(!dommy::direction::adjust(
        direction, false, false,
        std::numeric_limits<double>::infinity(), adjusted));
    const Imath::V3f invalid(
        std::numeric_limits<float>::infinity(), 0.0f, 0.0f);
    assert(!dommy::direction::difference(invalid, to, delta));
    assert(!dommy::direction::assignFinite(invalid, direction));
}

void testMelStringEscaping() {
    assert(dommy::script::escapeMelStringLiteral("cache/alembic") == "cache/alembic");
    assert(dommy::script::escapeMelStringLiteral("a\\b\"c\nd") == "a\\\\b\\\"c\\nd");
}

void testVdbComputeExceptionBoundary() {
    assert(compileVdbComputeExceptionBoundary(false) == MS::kSuccess);
    assert(compileVdbComputeExceptionBoundary(true) == MS::kFailure);
    assert(dommy::vdb::reportComputeException("test", std::exception_ptr()) == MS::kFailure);
}

void testComputeExceptionBoundary() {
    assert(compileComputeExceptionBoundary(false) == MS::kSuccess);
    assert(compileComputeExceptionBoundary(true) == MS::kFailure);
    assert(dommy::maya::reportComputeException(
        "test", std::exception_ptr()) == MS::kFailure);
}

void testLegacyTextParsing() {
    // A line per record must not force a relocation of every prior record.
    std::vector<dommy::file::Triple> grown;
    std::size_t reallocations = 0, relocated = 0;
    for (std::size_t i = 0; i < 4000; ++i) {
        const std::size_t capacity = grown.capacity();
        std::istringstream values("1 2 3");
        assert(dommy::file::detail::appendTriples(values, grown));
        if (grown.capacity() != capacity) { ++reallocations; relocated += i; }
        assert(grown.capacity() <= dommy::file::kMaxLegacyTextRecords);
    }
    assert(reallocations <= 14 && relocated < 8192);
    std::vector<dommy::file::Triple> bounded;
    const std::string maximumLine(dommy::file::kMaxLegacyTextLineBytes, '#');
    std::istringstream exactLine(maximumLine + "\nP 1 2 3");
    assert(dommy::file::readPointData(exactLine, bounded) && bounded.size() == 1);
    std::istringstream tooLong(maximumLine + "#\nP 1 2 3\n");
    assert(!dommy::file::readPointData(tooLong, bounded) && bounded.empty());
    std::istringstream tooLongColor(maximumLine + "#\nVC 1 2 3\n");
    assert(!dommy::file::readVertexColors(tooLongColor, bounded));
    // Do not first materialize the full hostile line: expose an unbounded
    // stream without allocating it and count consumed bytes.
    class EndlessLine : public std::streambuf {
    public:
        std::size_t consumed = 0;
        int_type underflow() override { return '#'; }
        int_type uflow() override { ++consumed; return '#'; }
    } endless;
    std::istream hostile(&endless);
    assert(!dommy::file::readPointData(hostile, bounded));
    assert(endless.consumed == dommy::file::kMaxLegacyTextLineBytes + 1);
    std::size_t bytes = dommy::file::kMaxLegacyTextBytes - 2;
    std::string line;
    std::istringstream exactBytes("#");
    assert(dommy::file::detail::readLegacyLine(exactBytes, line, bytes));
    assert(bytes == dommy::file::kMaxLegacyTextBytes);
    bytes = dommy::file::kMaxLegacyTextBytes - 1;
    std::istringstream excessBytes("#");
    assert(!dommy::file::detail::readLegacyLine(excessBytes, line, bytes));
    // Ass and other physical-byte formats must not charge a virtual newline.
    using dommy::file::readBoundedLine;
    using dommy::file::UnterminatedLinePolicy;
    bytes = 0U;
    std::istringstream physicalExact("1234");
    assert(readBoundedLine(physicalExact, line, bytes, 4U, 4U,
                           UnterminatedLinePolicy::PhysicalBytes));
    assert(line == "1234" && bytes == 4U);
    assert(!readBoundedLine(physicalExact, line, bytes, 4U, 4U,
                            UnterminatedLinePolicy::PhysicalBytes));
    bytes = 0U;
    std::istringstream physicalExcess("12345\n");
    assert(!readBoundedLine(physicalExcess, line, bytes, 4U, 4U,
                            UnterminatedLinePolicy::PhysicalBytes));
    assert(line == "1234" && bytes == 4U && physicalExcess.fail());
    bytes = 0U;
    std::istringstream blank("\n");
    assert(readBoundedLine(blank, line, bytes, 1U, 0U,
                           UnterminatedLinePolicy::PhysicalBytes));
    assert(line.empty() && bytes == 1U);
    std::istringstream failedStream("P 1 2 3\n");
    failedStream.setstate(std::ios::failbit);
    assert(!dommy::file::readPointData(failedStream, bounded));

    std::istringstream colorInput("VTXCOLOR 2\nVC 0.1 0.2 0.3 0.4 0.5 0.6\n");
    std::vector<dommy::file::Triple> colors;
    assert(dommy::file::readVertexColors(colorInput, colors));
    assert(colors.size() == 2 && near(colors[1][2], 0.6));

    std::istringstream partialColor("VTXCOLOR 1\nVC 0.1 0.2\n");
    assert(!dommy::file::readVertexColors(partialColor, colors));
    std::istringstream nonFiniteColor("VTXCOLOR 1\nVC nan 0.2 0.3\n");
    assert(!dommy::file::readVertexColors(nonFiniteColor, colors));
    std::istringstream countMismatch("VTXCOLOR 2\nVC 0.1 0.2 0.3\n");
    assert(!dommy::file::readVertexColors(countMismatch, colors));

    std::istringstream pointInput("POINT_DATA_FROM_MESH mesh 1\nP 1 2 3\n");
    std::vector<dommy::file::Triple> points;
    assert(dommy::file::readPointData(pointInput, points));
    assert(points.size() == 1 && points[0][1] == 2.0);

    std::istringstream multipleSections(
        "#D POINT DATA 1.0\n"
        "POINT_DATA_FROM_MESH mesh 2\n"
        "P 1 2 3\n"
        "P 4 5 6\n"
        "POINT_DATA_FROM_CURVE curve 1\n"
        "P -1 -2 -3\n");
    assert(dommy::file::readPointData(multipleSections, points));
    assert(points.size() == 3);
    assert(points[0][0] == 1.0 && points[1][2] == 6.0);
    assert(points[2][0] == -1.0 && points[2][2] == -3.0);

    std::istringstream headerlessPoints("P 1 2 3\nP 4 5 6\n");
    assert(dommy::file::readPointData(headerlessPoints, points));
    assert(points.size() == 2);

    std::istringstream incompleteFirstSection(
        "POINT_DATA_FROM_MESH mesh 2\n"
        "P 1 2 3\n"
        "POINT_DATA_FROM_CURVE curve 1\n"
        "P 4 5 6\n");
    assert(!dommy::file::readPointData(incompleteFirstSection, points));
    assert(points.empty());

    std::istringstream oversizedSection(
        "POINT_DATA_FROM_MESH mesh 1\nP 1 2 3 4 5 6\n");
    assert(!dommy::file::readPointData(oversizedSection, points));
    assert(points.empty());

    std::istringstream incompleteFinalSection(
        "POINT_DATA_FROM_MESH mesh 1\nP 1 2 3\n"
        "POINT_DATA_FROM_DPOINT dpoint 2\nP 4 5 6\n");
    assert(!dommy::file::readPointData(incompleteFinalSection, points));
    assert(points.empty());

    std::istringstream headerAfterHeaderless(
        "P 1 2 3\nPOINT_DATA_FROM_MESH mesh 1\nP 4 5 6\n");
    assert(!dommy::file::readPointData(headerAfterHeaderless, points));
    assert(points.empty());

    std::istringstream malformedSecondHeader(
        "POINT_DATA_FROM_MESH mesh 1\nP 1 2 3\n"
        "POINT_DATA_FROM_CURVE curve nope\n");
    assert(!dommy::file::readPointData(malformedSecondHeader, points));
    assert(points.empty());

    std::string path;
    assert(dommy::file::replaceFrameToken("cache.$F.txt", 12, 4, path));
    assert(path == "cache.0012.txt");
    assert(dommy::file::replaceFrameToken("cache.$F.txt", -3, 4, path));
    assert(path == "cache.-0003.txt");

    assert(dommy::file::replaceLegacyFrameToken(
        "cache.$F4.vdb", 12.75, path));
    assert(path == "cache.0012.vdb");
    assert(dommy::file::replaceLegacyFrameToken(
        "cache.$F4.vdb", -3.75, path));
    assert(path == "cache.-0003.vdb");
    assert(dommy::file::replaceLegacyFrameToken(
        "cache.$FF.vdb", 1.25, path));
    assert(path == "cache.1.25.vdb");
    assert(dommy::file::replaceLegacyFrameToken(
        "$F/$F2/cache.$F.vdb", 7.0, path));
    assert(path == "7/07/cache.7.vdb");
    assert(!dommy::file::replaceLegacyFrameToken(
        "cache.$F33.vdb", 1.0, path));
    assert(!dommy::file::replaceLegacyFrameToken(
        "cache.$F.vdb", std::numeric_limits<double>::quiet_NaN(), path));

    assert(dommy::file::replaceHashFrameToken(
        "cache.###.vdb", 1.0, 24.0, 1, path));
    assert(path == "cache.001.vdb");
    assert(dommy::file::replaceHashFrameToken(
        "cache.###.vdb", 1.25, 24.0, 1, path));
    assert(path == "cache.1.25.vdb");
    assert(dommy::file::replaceHashFrameToken(
        "cache.###.vdb", 1.25, 24.0, 0, path));
    assert(path == "cache.1.063.vdb");
    assert(dommy::file::replaceHashFrameToken(
        "cache.###.vdb", -1.25, 24.0, 0, path));
    assert(path == "cache.-2.188.vdb");
    assert(dommy::file::replaceHashFrameToken(
        "cache.####.vdb", 1.25, 24.0, 2, path));
    assert(path == "cache.0313.vdb");
    assert(dommy::file::replaceHashFrameToken(
        "cache.##/take.####.vdb", 5.0, 24.0, 1, path));
    assert(path == "cache.05/take.0005.vdb");
    assert(!dommy::file::replaceHashFrameToken(
        "cache.#.vdb", 1.0, 0.0, 0, path));
    assert(!dommy::file::replaceHashFrameToken(
        "cache.#.vdb", 1.0, 24.0, 3, path));
    assert(!dommy::file::replaceHashFrameToken(
        "cache.#.vdb", std::numeric_limits<double>::infinity(), 24.0, 0,
        path));
}

void testLocalRandomGenerator() {
    dommy::math::RandomGenerator first(42U);
    dommy::math::RandomGenerator second(42U);
    for (int index = 0; index < 16; ++index) {
        const double lhs = first.uniform(-2.0, 7.0);
        const double rhs = second.uniform(-2.0, 7.0);
        assert(lhs == rhs);
        assert(lhs >= -2.0 && lhs <= 7.0);
    }
    assert(first.index(0) == 0);
    assert(first.index(5) < 5);

    // Compare to the original standard-library call on this runtime, including
    // equal bounds and mixed integer/real draws. Do not pin a cross-library RNG.
    dommy::math::RandomGenerator compatible(19U);
    std::mt19937 baseline(19U);
    const double ranges[][2] = {{-2, 7}, {7, -2}, {4, 4}, {0, 1}, {-9, -3}};
    for (const auto& range : ranges) {
        const double lo = std::min(range[0], range[1]);
        const double hi = std::max(range[0], range[1]);
        std::uniform_real_distribution<double> real(lo, hi);
        assert(compatible.uniform(range[0], range[1]) == real(baseline));
        std::uniform_int_distribution<std::size_t> integer(0, 6);
        assert(compatible.index(7) == integer(baseline));
    }

    const double largest = std::numeric_limits<double>::max();
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double invalid[][2] = {{nan, 1}, {1, nan}, {-infinity, 1}, {1, infinity},
                                 {-largest, largest}, {largest, -largest},
                                 {largest * .5, infinity}};
    for (const auto& range : invalid) {
        dommy::math::RandomGenerator actual(42U), untouched(42U);
        bool rejected = false;
        try {
            (void)actual.uniform(range[0], range[1]);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        assert(actual.uniform(0, 1) == untouched.uniform(0, 1));
    }
    for (const auto& range : {std::make_pair(-largest, 0.0),
                              std::make_pair(0.0, largest),
                              std::make_pair(largest, largest)}) {
        assert(std::isfinite(first.uniform(range.first, range.second)));
    }
}

void testInterleavedOrder() {
    const std::vector<std::size_t> lengths = {2, 0, 3};
    std::vector<std::size_t> order;
    assert(dommy::math::buildInterleavedOrder(lengths, order));
    const std::vector<std::size_t> expected = {0, 2, 1, 3, 4};
    assert(order == expected);

    assert(dommy::math::buildInterleavedOrder(std::vector<std::size_t>(), order));
    assert(order.empty());

    // Independent historical item-major/group-major reference, including all
    // 19,531 configurations of up to six groups with lengths zero through four.
    std::size_t configurations = 1U;
    for (std::size_t groups = 0U; groups <= 6U; ++groups) {
        for (std::size_t configuration = 0U; configuration < configurations; ++configuration) {
            std::vector<std::size_t> counts(groups), offsets(groups);
            std::size_t value = configuration, total = 0U, longest = 0U;
            for (std::size_t group = 0U; group < groups; ++group) {
                counts[group] = value % 5U;
                value /= 5U;
                offsets[group] = total;
                total += counts[group];
                longest = std::max(longest, counts[group]);
            }
            std::vector<std::size_t> reference;
            for (std::size_t item = 0U; item < longest; ++item)
                for (std::size_t group = 0U; group < groups; ++group)
                    if (item < counts[group]) reference.push_back(offsets[group] + item);
            order = {99U};
            assert(dommy::math::buildInterleavedOrder(counts, order));
            assert(order == reference && order.size() == total);
            std::vector<std::size_t> visited;
            assert(dommy::math::visitInterleavedIndices(counts,
                [&](std::size_t group, std::size_t item, std::size_t flatIndex) {
                    assert(group < groups && item < counts[group]);
                    assert(flatIndex == offsets[group] + item);
                    visited.push_back(flatIndex);
                }));
            assert(visited == reference);
        }
        configurations *= 5U;
    }
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    for (const auto& overflow : {std::vector<std::size_t>{maximum, 1U},
                                 std::vector<std::size_t>{0U, 1U, maximum, 0U}}) {
        order = {99U};
        assert(!dommy::math::buildInterleavedOrder(overflow, order));
        assert(order.empty());
        unsigned calls = 0U;
        assert(!dommy::math::visitInterleavedIndices(overflow,
            [&](std::size_t, std::size_t, std::size_t) { ++calls; }));
        assert(calls == 0U);
    }
    for (bool allocation : {false, true}) {
        unsigned calls = 0U;
        bool rejected = false;
        try {
            (void)dommy::math::visitInterleavedIndices(lengths,
                [&](std::size_t, std::size_t, std::size_t) {
                    if (++calls == 3U) {
                        if (allocation) throw std::bad_alloc();
                        throw std::invalid_argument("Visitor failure");
                    }
                });
        } catch (const std::bad_alloc&) { rejected = allocation; }
          catch (const std::invalid_argument&) { rejected = !allocation; }
        assert(rejected && calls == 3U);
    }
    // A representable size_t sum is valid for the primitive, without reserving
    // the total output itself. Stop at the second group before huge iteration.
    unsigned boundaryCalls = 0U;
    bool stopped = false;
    try {
        (void)dommy::math::visitInterleavedIndices({maximum - 1U, 1U},
            [&](std::size_t group, std::size_t item, std::size_t flatIndex) {
                ++boundaryCalls;
                assert(item == 0U && flatIndex == (group == 0U ? 0U : maximum - 1U));
                if (group == 1U) throw std::invalid_argument("Boundary reached");
            });
    } catch (const std::invalid_argument&) { stopped = true; }
    assert(stopped && boundaryCalls == 2U);
}

void testArnoldCurveLayout() {
    using dommy::arnold::CurveLayout;

    const CurveLayout linear = CurveLayout::create(4, false, false);
    assert(linear.valid());
    assert(linear.controlVertexCount() == 4);
    assert(linear.varyingCount() == 4);
    for (std::size_t index = 0; index < 4; ++index) {
        assert(linear.controlVertexSourceIndex(index) == index);
        assert(linear.varyingSourceIndex(index) == index);
    }

    const CurveLayout paddedCubic = CurveLayout::create(4, true, true);
    assert(paddedCubic.valid());
    assert(paddedCubic.controlVertexCount() == 8);
    assert(paddedCubic.varyingCount() == 6);
    const std::size_t expectedControlIndices[] = {0, 0, 0, 1, 2, 3, 3, 3};
    const std::size_t expectedVaryingIndices[] = {0, 0, 1, 2, 3, 3};
    for (std::size_t index = 0; index < paddedCubic.controlVertexCount(); ++index)
        assert(paddedCubic.controlVertexSourceIndex(index) == expectedControlIndices[index]);
    for (std::size_t index = 0; index < paddedCubic.varyingCount(); ++index)
        assert(paddedCubic.varyingSourceIndex(index) == expectedVaryingIndices[index]);

    const CurveLayout unpaddedCubic = CurveLayout::create(5, true, false);
    assert(unpaddedCubic.valid());
    assert(unpaddedCubic.controlVertexCount() == 5);
    assert(unpaddedCubic.varyingCount() == 3);
    for (std::size_t index = 0; index < unpaddedCubic.varyingCount(); ++index)
        assert(unpaddedCubic.varyingSourceIndex(index) == index + 1);

    assert(!CurveLayout::create(0, false, false).valid());
    assert(!CurveLayout::create(3, true, false).valid());

    std::size_t total = std::numeric_limits<std::size_t>::max() - 1;
    assert(!dommy::arnold::checkedAdd(2, total));
    assert(total == std::numeric_limits<std::size_t>::max() - 1);
}

void testMeshFaceToPointSelection() {
    std::vector<int> normalizedFaces;
    const std::vector<int> unorderedFaces = {3, 1, 3, 0};
    assert(dommy::mesh::normalizeFaceSelection(
        unorderedFaces, 4, 10, normalizedFaces));
    assert((normalizedFaces == std::vector<int>{0, 1, 3}));

    const std::vector<int> preservedFaces = normalizedFaces;
    assert(!dommy::mesh::normalizeFaceSelection(
        std::vector<int>{0, -1}, 4, 10, normalizedFaces));
    assert(normalizedFaces == preservedFaces);
    assert(!dommy::mesh::normalizeFaceSelection(
        std::vector<int>{4}, 4, 10, normalizedFaces));
    assert(normalizedFaces == preservedFaces);
    assert(!dommy::mesh::normalizeFaceSelection(
        std::vector<int>{0, 1, 2}, 4, 2, normalizedFaces));
    assert(normalizedFaces == preservedFaces);

    std::vector<int> percentageFaces;
    assert(dommy::mesh::buildPercentageFaceSelection(
        5, 50.0, 10, percentageFaces));
    assert((percentageFaces == std::vector<int>{0, 1}));
    assert(dommy::mesh::buildPercentageFaceSelection(
        5, 100.0, 10, percentageFaces));
    assert((percentageFaces == std::vector<int>{0, 1, 2, 3, 4}));
    const std::vector<int> preservedPercentageFaces = percentageFaces;
    assert(!dommy::mesh::buildPercentageFaceSelection(
        5, -1.0, 10, percentageFaces));
    assert(percentageFaces == preservedPercentageFaces);
    assert(!dommy::mesh::buildPercentageFaceSelection(
        5, std::numeric_limits<double>::infinity(), 10, percentageFaces));
    assert(percentageFaces == preservedPercentageFaces);

    const std::vector<std::size_t> faceCounts = {3, 3};
    const std::vector<std::size_t> connections = {0, 1, 2, 2, 3, 0};
    const std::vector<bool> selectedFaces = {false, true};
    std::vector<bool> selectedPoints;
    assert(dommy::mesh::buildPointSelectionFromFaces(
        4, faceCounts, connections, selectedFaces, selectedPoints));
    assert(selectedPoints.size() == 4);
    assert(selectedPoints[0]);
    assert(!selectedPoints[1]);
    assert(selectedPoints[2]);
    assert(selectedPoints[3]);

    const std::vector<std::size_t> invalidConnections = {0, 1, 9};
    assert(!dommy::mesh::buildPointSelectionFromFaces(
        4, std::vector<std::size_t>(1, 3), invalidConnections,
        std::vector<bool>(1, true), selectedPoints));
}

void testCurveFieldMath() {
    float normalized = -1.0f;
    assert(dommy::field::normalizeRange(4.0, 2.0, 6.0, normalized));
    assert(near(normalized, 0.5));
    assert(dommy::field::normalizeRange(99.0, 2.0, 6.0, normalized));
    assert(normalized == 1.0f);
    assert(!dommy::field::normalizeRange(2.0, 2.0, 2.0, normalized));
    assert(normalized == 0.0f);
    const double largest = std::numeric_limits<double>::max();
    assert(dommy::field::normalizeRange(largest, -largest, largest, normalized));
    assert(normalized == 1.0f);
    assert(dommy::field::normalizeRange(0.0, -largest, largest, normalized));
    assert(normalized == 0.5f);
    assert(dommy::field::normalizeRange(largest, -largest, -largest / 2.0, normalized));
    assert(normalized == 1.0f); // Field boundary retains its [0,1] clamp.
    assert(!dommy::field::normalizeRange(
        std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0, normalized));
    assert(normalized == 0.0f);
    assert(!dommy::field::normalizeRange(0.0, 1.0, 0.0, normalized));
    assert(!dommy::field::normalizeRange(0.0, 0.0, 1.0e-13, normalized));
    assert(dommy::field::validMassCount(4, 1));
    assert(dommy::field::validMassCount(4, 4));
    assert(!dommy::field::validMassCount(4, 0));
    assert(!dommy::field::validMassCount(4, 2));
}

void testParticleFieldMath() {
    assert(dommy::field::hasMatchingParticleFieldCounts(4, 4, 4, 4));
    assert(!dommy::field::hasMatchingParticleFieldCounts(4, 4, 3, 4));
    assert(dommy::field::isValidOptionalFieldArrayCount(4, 0));
    assert(dommy::field::isValidOptionalFieldArrayCount(4, 4));
    assert(!dommy::field::isValidOptionalFieldArrayCount(4, 1));
    assert(dommy::field::isValidOptionalFieldMassCount(4, 0));
    assert(dommy::field::isValidOptionalFieldMassCount(4, 1));
    assert(dommy::field::isValidOptionalFieldMassCount(4, 4));
    assert(!dommy::field::isValidOptionalFieldMassCount(4, 2));
    assert(dommy::field::isValidFieldMass(0.0));
    assert(dommy::field::isValidFieldMass(2.0));
    assert(!dommy::field::isValidFieldMass(-1.0));
    assert(!dommy::field::isValidFieldMass(
        std::numeric_limits<double>::infinity()));

    double mass = -1.0;
    assert(dommy::field::fieldMassFromInverseMass(0.25, mass));
    assert(near(mass, 4.0));
    assert(dommy::field::fieldMassFromInverseMass(0.0, mass));
    assert(mass == 1.0);
    const double preservedMass = mass;
    assert(!dommy::field::fieldMassFromInverseMass(-1.0, mass));
    assert(mass == preservedMass);
    assert(!dommy::field::fieldMassFromInverseMass(
        std::numeric_limits<double>::quiet_NaN(), mass));

    assert(dommy::field::isValidFieldTimeStep(1.0 / 24.0));
    assert(!dommy::field::isValidFieldTimeStep(0.0));
    assert(!dommy::field::isValidFieldTimeStep(
        std::numeric_limits<double>::infinity()));

    double force = 0.0;
    assert(dommy::field::checkedAccumulateFieldForce(2.0, 3.0, force));
    assert(force == 5.0);
    assert(!dommy::field::checkedAccumulateFieldForce(
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), force));

    double product = 7.0;
    assert(dommy::field::checkedMultiplyFieldValue(3.0, 4.0, product));
    assert(product == 12.0);
    const double preservedProduct = product;
    assert(!dommy::field::checkedMultiplyFieldValue(
        std::numeric_limits<double>::max(), 2.0, product));
    assert(product == preservedProduct);

    double component = 0.0;
    assert(dommy::field::computeVectorFieldForceComponent(
        2.0, 3.0, 4.0, component));
    assert(component == 14.0);
    const double preservedComponent = component;
    assert(!dommy::field::computeVectorFieldForceComponent(
        2.0, std::numeric_limits<double>::max(), 2.0, component));
    assert(component == preservedComponent);

    assert(dommy::field::computeNoiseFieldForceComponent(
        2.0, 0.5, 3.0, 4.0, component));
    assert(component == 12.0);
    assert(!dommy::field::computeNoiseFieldForceComponent(
        2.0, 0.5, 3.0, -1.0, component));
    assert(component == 12.0);
    assert(!dommy::field::computeNoiseFieldForceComponent(
        std::numeric_limits<double>::max(), 2.0, 1.0, 1.0,
        component));

    assert(dommy::field::computeGoalFieldForceComponent(
        2.0, 10.0, 3.0, 0.5, 0.25, 2.0, component));
    assert(near(component, 6.5));
    const double preservedGoalComponent = component;
    assert(!dommy::field::computeGoalFieldForceComponent(
        -std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 0.0,
        1.0, 0.0, 1.0, component));
    assert(component == preservedGoalComponent);

    double goalScale = 0.0;
    assert(dommy::field::computeGoalDistanceScale(
        2.0, 10.0, 2.0, 3.0, goalScale));
    assert(near(goalScale, 1.92));
    assert(dommy::field::computeGoalDistanceScale(
        11.0, 10.0, 2.0, 3.0, goalScale));
    assert(goalScale == 0.0);
    assert(dommy::field::computeGoalDistanceScale(
        2.0, 10.0, -1.0, 3.0, goalScale));
    assert(goalScale == 3.0);
    const double preservedGoalScale = goalScale;
    assert(!dommy::field::computeGoalDistanceScale(
        2.0, 0.0, 2.0, 3.0, goalScale));
    assert(goalScale == preservedGoalScale);

    double velocity = 0.0;
    assert(dommy::field::integrateFieldVelocity(
        2.0, 8.0, 0.25, 0.5, velocity));
    assert(near(velocity, 3.0));
    assert(dommy::field::integrateFieldVelocity(
        2.0, 8.0, 0.0, 0.5, velocity));
    assert(near(velocity, 2.0));
    assert(!dommy::field::integrateFieldVelocity(
        2.0, 8.0, -1.0, 0.5, velocity));
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(dommy::field::computeNoiseFieldForceComponent(1e308, 2.0, 0.25, 1.0, component));
    assert(component == 5e307);
    assert(dommy::field::computeGoalFieldForceComponent(0.0, 1.0, 1e308, 0.0, 2.0, 1e-308, component));
    assert(near(component, -2.0));
    assert(dommy::field::computeGoalFieldForceComponent(-1e308, 1e308, 0.0, 1.0, 0.0, 0.25, component));
    assert(component == 5e307);
    assert(dommy::field::computeVectorFieldForceComponent(-1e308, 1e308, 2.0, component));
    assert(component == 1e308);
    assert(dommy::field::integrateFieldVelocity(1.0, 1e308, 2.0, 1e-308, velocity));
    assert(near(velocity, 3.0));
    assert(dommy::field::integrateFieldVelocity(-1e308, 1e308, 2.0, 1.0, velocity));
    assert(velocity == 1e308);
    assert(!dommy::field::computeNoiseFieldForceComponent(
        std::numeric_limits<double>::infinity(), 2.0, 0.0, 1.0, component));
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testKillFieldMath() {
    using dommy::kill_field::Point3d;

    assert(dommy::kill_field::compareScalar(
        dommy::kill_field::kGreaterThan, 2.0, 1.0));
    assert(!dommy::kill_field::compareScalar(
        dommy::kill_field::kEqual,
        std::numeric_limits<double>::quiet_NaN(), 0.0));
    assert(dommy::kill_field::compareComponents(
        dommy::kill_field::kGreaterOrEqual,
        Point3d(2.0, 3.0, 4.0), Point3d(2.0, 2.0, 3.0)));
    assert(dommy::kill_field::compareMagnitude(
        dommy::kill_field::kEqual,
        Point3d(3.0, 4.0, 0.0), Point3d(0.0, 0.0, 5.0)));
    assert(dommy::kill_field::compareAngle(
        dommy::kill_field::kGreaterThan,
        Point3d(1.0, 0.0, 0.0), Point3d(0.0, 1.0, 0.0), 1.0));
    assert(!dommy::kill_field::compareAngle(
        dommy::kill_field::kEqual,
        Point3d(), Point3d(1.0, 0.0, 0.0), 0.0));

    std::vector<dommy::kill_field::IdInterval> intervals;
    assert(dommy::kill_field::parseIdExpression(
        "5; 1-4;3-7", 64U, 8U, intervals));
    assert(intervals.size() == 1U);
    assert(dommy::kill_field::containsId(intervals, 1));
    assert(dommy::kill_field::containsId(intervals, 6));
    assert(!dommy::kill_field::containsId(intervals, 7));
    assert(!dommy::kill_field::containsId(intervals, -1));
    assert(dommy::kill_field::parseIdExpression(
        "", 64U, 8U, intervals));
    assert(intervals.empty());
    assert(!dommy::kill_field::parseIdExpression(
        "1-", 64U, 8U, intervals));
    assert(!dommy::kill_field::parseIdExpression(
        "1;;2", 64U, 8U, intervals));
    assert(!dommy::kill_field::parseIdExpression(
        "2147483648", 64U, 8U, intervals));

    std::vector<Point3d> points;
    points.push_back(Point3d(0.0, 0.0, 0.0));
    points.push_back(Point3d(0.5, 0.0, 0.0));
    points.push_back(Point3d(10.0, 0.0, 0.0));
    std::vector<unsigned char> hasNeighbor;
    assert(dommy::kill_field::findNeighborPresence(
        points, 1.0, 1000U, hasNeighbor));
    assert(hasNeighbor.size() == 3U);
    assert(hasNeighbor[0] == 1U && hasNeighbor[1] == 1U);
    assert(hasNeighbor[2] == 0U);

    points[1] = Point3d(-0.0, 0.0, 0.0);
    assert(dommy::kill_field::findNeighborPresence(
        points, 0.0, 1000U, hasNeighbor));
    assert(hasNeighbor[0] == 1U && hasNeighbor[1] == 1U);
    assert(hasNeighbor[2] == 0U);
    assert(!dommy::kill_field::findNeighborPresence(
        points, -1.0, 1000U, hasNeighbor));
    assert(!dommy::kill_field::findNeighborPresence(
        points, 1.0, 1U, hasNeighbor));
}

void testLegacySharedPointerAlias() {
    shared_ptr<int> first(new int(7));
    shared_ptr<int> second = first;
    assert(first.use_count() == 2);
    assert(*second == 7);
    first.reset();
    assert(second.use_count() == 1);
    assert(*second == 7);
}

void testPoissonBoundaryBehavior() {
    PoissonGenerator::DefaultPRNG generator(42);
    assert(PoissonGenerator::GeneratePoissonPoints(0, generator).empty());
    assert(PoissonGenerator::GeneratePoissonPoints(8, generator, 30, false, 0.0f).empty());

    const std::vector<PoissonGenerator::sPoint> onePoint =
        PoissonGenerator::GeneratePoissonPoints(1, generator, 30, false);
    assert(onePoint.size() == 1);
    assert(std::string(PoissonGenerator::Version) ==
           std::string(poissonVersionFromSecondTranslationUnit()));
}

void testPdbCodec() {
    pdb_io_t source;
    source.m_time = 12.5f;
    source.m_num_particles = 2;
    source.m_attributes.resize(3);

    source.m_attributes[0].m_name = "position";
    source.m_attributes[0].m_type = pdb_io_t::kVector;
    source.m_attributes[0].m_num_elements = 1;
    source.m_attributes[0].m_vector_data.resize(2);
    source.m_attributes[0].m_vector_data[0] = {1.0f, 2.0f, 3.0f};
    source.m_attributes[0].m_vector_data[1] = {-1.0f, -2.0f, -3.0f};

    source.m_attributes[1].m_name = "weights";
    source.m_attributes[1].m_type = pdb_io_t::kReal;
    source.m_attributes[1].m_num_elements = 2;
    source.m_attributes[1].m_real_data = {0.25f, 0.75f, 1.0f, 2.0f};

    source.m_attributes[2].m_name = "id";
    source.m_attributes[2].m_type = pdb_io_t::kLong;
    source.m_attributes[2].m_num_elements = 1;
    source.m_attributes[2].m_long_data = {4, 9};

    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    assert(source.write(stream));
    stream.seekg(0);

    pdb_io_t decoded;
    assert(decoded.read(stream));
    assert(decoded.m_num_particles == 2);
    assert(decoded.m_time == source.m_time);
    assert(decoded.m_attributes.size() == 3);
    assert(decoded.m_attributes[0].m_vector_data[1].z == -3.0f);
    assert(decoded.m_attributes[1].m_num_elements == 2);
    assert(decoded.m_attributes[1].m_real_data[3] == 2.0f);
    assert(decoded.m_attributes[2].m_long_data[1] == 9);

    const std::string encoded = stream.str();
    std::string invalidMagic = encoded;
    invalidMagic[0] = 0;
    std::istringstream invalidMagicStream(invalidMagic, std::ios::binary);
    assert(!decoded.read(invalidMagicStream));

    std::istringstream truncatedStream(encoded.substr(0, encoded.size() - 1), std::ios::binary);
    assert(!decoded.read(truncatedStream));

    pdb_io_t invalidShape = source;
    invalidShape.m_attributes[1].m_real_data.pop_back();
    std::ostringstream invalidShapeStream(std::ios::binary);
    assert(!invalidShape.write(invalidShapeStream));

    pdb_io_t nonFiniteTime = source;
    nonFiniteTime.m_time = std::numeric_limits<float>::infinity();
    std::ostringstream nonFiniteTimeStream(std::ios::binary);
    assert(!nonFiniteTime.write(nonFiniteTimeStream));

    pdb_io_t nonFiniteVector = source;
    nonFiniteVector.m_attributes[0].m_vector_data[0].x =
        std::numeric_limits<float>::quiet_NaN();
    std::ostringstream nonFiniteVectorStream(std::ios::binary);
    assert(!nonFiniteVector.write(nonFiniteVectorStream));

    pdb_io_t tooManyParticles = source;
    tooManyParticles.m_num_particles =
        static_cast<int>(pdb_io_t::kMaxParticleCount + 1U);
    std::ostringstream tooManyParticlesStream(std::ios::binary);
    assert(!tooManyParticles.write(tooManyParticlesStream));

    pdb_io_t empty;
    std::stringstream emptyStream(std::ios::in | std::ios::out | std::ios::binary);
    assert(empty.write(emptyStream));
    emptyStream.seekg(0);
    assert(decoded.read(emptyStream));
    assert(decoded.m_num_particles == 0);
    assert(decoded.m_attributes.empty());
}

void testPowerOfTwoAndContainerHelpers() {
    assert(dommy::math::round_up_to_power_of_two(0) == 0);
    assert(dommy::math::round_up_to_power_of_two(1) == 1);
    assert(dommy::math::round_up_to_power_of_two(3) == 4);
    assert(dommy::math::round_up_to_power_of_two(UINT_MAX) == 0);
    assert(dommy::math::round_down_to_power_of_two(0) == 0);
    assert(dommy::math::round_down_to_power_of_two(7) == 4);
    assert(dommy::math::intlog2(-1) == -1);
    assert(dommy::math::intlog2(0) == -1);
    assert(dommy::math::intlog2(8) == 3);

    std::vector<int> values;
    dommy::math::insert(values, 0, 4);
    dommy::math::insert(values, 1, 8);
    dommy::math::insert(values, 1, 6);
    assert(values.size() == 3 && values[0] == 4 && values[1] == 6 && values[2] == 8);
    dommy::math::erase(values, 99);
    dommy::math::erase(values, 1);
    assert(values.size() == 2 && values[1] == 8);
    dommy::math::erase_unordered(values, 99);
}

void testCurveGeneratorBoundaries() {
    const double pi = std::acos(-1.0);
    const double degrees[] = {0.0, 90.0, 180.0, 270.0, 360.0};
    for (std::size_t index = 0U; index < 5U; ++index) {
        const double angle = dommy::math::degreesToRadians(degrees[index]);
        assert(std::fabs(angle - index * pi / 2.0) < 1.0e-12);
    }
    // Keep the degree-valued random stream, converting only at the Maya API
    // boundary. Fixed seeds must reproduce the same angles on a given runtime.
    for (unsigned int seed : {0U, 1U, 12345U}) {
        dommy::math::RandomGenerator random(seed), replay(seed);
        for (std::size_t index = 0U; index < 128U; ++index) {
            const double angleDegrees = random.uniform(0.0, 360.0);
            const double angle = dommy::math::degreesToRadians(angleDegrees);
            assert(angle >= 0.0 && angle <= 2.0 * pi);
            assert(angle == dommy::math::degreesToRadians(replay.uniform(0.0, 360.0)));
            assert(std::fabs(dommy::math::radiansToDegrees(angle) - angleDegrees) < 1.0e-12);
        }
    }

    // Node source guards verify signed reads and minimum/mode checks. Exercise
    // their shared conversion without imposing a new cap on positive UI ints.
    const std::size_t maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    for (int raw : {std::numeric_limits<int>::min(), -1, 0, 1, 2,
                    std::numeric_limits<int>::max()}) {
        std::size_t count = 17U;
        const bool valid = dommy::generation::checkedCount(raw, maximum, count);
        assert(valid == (raw >= 0));
        if (valid) assert(count == static_cast<std::size_t>(raw));
        else assert(count == 17U);
    }

    // The knot replacement path uses this status/actual-length copy helper on
    // a detached candidate. These injected failures do not model Maya itself.
    const TestUVArray<double> replacement{0.0, 0.0, 1.0, 1.0};
    TestUVArray<double> destination{0.0, 1.0, 2.0, 3.0};
    const auto previous = destination.values;
    destination.failCopy = true;
    assert(dommy::maya::copyCheckedArray(replacement, destination) == MS::kFailure);
    assert(destination.values == previous);
    destination.failCopy = false;
    destination.shortCopy = true;
    assert(dommy::maya::copyCheckedArray(replacement, destination) == MS::kFailure);
    destination.shortCopy = false;
    assert(dommy::maya::copyCheckedArray(replacement, destination) == MS::kSuccess);
    assert(destination.values == replacement.values);
}

void testSharedGeometryMath() {
    const double pi = 3.14159265358979323846;
    assert(std::fabs(dommy::math::degreesToRadians(180.0) - pi) < 1.0e-14);
    assert(std::fabs(dommy::math::radiansToDegrees(pi) - 180.0) < 1.0e-12);
    assert(std::fabs(
        dommy::math::distance(MVector(0.0, 0.0, 0.0),
                              MVector(3.0, 4.0, 0.0)) - 5.0) < 1.0e-12);
    assert(std::fabs(
        dommy::math::distance(MPoint(0.0, 0.0, 0.0),
                              MPoint(0.0, 0.0, 2.0)) - 2.0) < 1.0e-12);

    const MPoint minimum(0.0, 0.0, 0.0);
    const MPoint maximum(1.0, 1.0, 1.0);
    MVector hit(9.0, 9.0, 9.0);
    assert(dommy::math::intersectSegmentAabb(
        minimum, maximum, MVector(-1.0, 0.5, 0.5),
        MVector(0.5, 0.5, 0.5), hit));
    assert(std::fabs(hit.x) < 1.0e-12 &&
           std::fabs(hit.y - 0.5) < 1.0e-12 &&
           std::fabs(hit.z - 0.5) < 1.0e-12);

    assert(dommy::math::intersectSegmentAabb(
        maximum, minimum, MVector(-1.0, -1.0, -1.0),
        MVector(0.0, 0.0, 0.0), hit));
    assert(std::fabs(hit.x) < 1.0e-12 &&
           std::fabs(hit.y) < 1.0e-12 &&
           std::fabs(hit.z) < 1.0e-12);

    hit = MVector(7.0, 8.0, 9.0);
    assert(!dommy::math::intersectSegmentAabb(
        minimum, maximum, MVector(-1.0, 2.0, 0.5),
        MVector(2.0, 2.0, 0.5), hit));
    assert(hit.x == 7.0 && hit.y == 8.0 && hit.z == 9.0);

    assert(!dommy::math::intersectSegmentAabb(
        minimum, maximum,
        MVector(std::numeric_limits<double>::quiet_NaN(), 0.5, 0.5),
        MVector(0.5, 0.5, 0.5), hit));
}

void testMvlTemplateMath() {
    mvl::mat<float, 2, 2> lhs(1.0f, 2.0f, 3.0f, 4.0f);
    const mvl::mat<float, 2, 2> rhs(4.0f, 3.0f, 2.0f, 1.0f);
    lhs += rhs;
    lhs *= 2.0f;
    assert(near(lhs(0, 0), 10.0));
    assert(near(lhs(1, 1), 10.0));

    mvl::mat<float, 2, 3> rectangular;
    rectangular(0, 0) = 1.0f;
    rectangular(0, 1) = 2.0f;
    rectangular(0, 2) = 3.0f;
    rectangular(1, 0) = 4.0f;
    rectangular(1, 1) = 5.0f;
    rectangular(1, 2) = 6.0f;
    const mvl::mat<float, 3, 2> transposed = mvl::trans(rectangular);
    assert(near(transposed(0, 0), 1.0));
    assert(near(transposed(2, 1), 6.0));

    const quatf quarterTurn = mvl::q_from_axis_angle(
        vec3f(0.0f, 0.0f, 1.0f), static_cast<float>(std::acos(-1.0) * 0.5));
    const vec3f rotated = mvl::qprod(quarterTurn, vec3f(1.0f, 0.0f, 0.0f));
    assert(near(rotated[0], 0.0, 1.0e-5));
    assert(near(rotated[1], 1.0, 1.0e-5));
    assert(near(rotated[2], 0.0, 1.0e-5));

    const quatf identity = mvl::qprod(quarterTurn, mvl::qconj(quarterTurn));
    assert(near(identity[0], 1.0, 1.0e-5));
    assert(near(identity[1], 0.0, 1.0e-5));
    assert(near(identity[2], 0.0, 1.0e-5));
    assert(near(identity[3], 0.0, 1.0e-5));

    vec3f axis;
    float angle = -1.0f;
    mvl::q_to_axis_angle(quatf(1.0001f, 0.0f, 0.0f, 0.0f), axis, angle);
    assert(std::isfinite(angle) && angle == 0.0f);
}

void testUtilityVectors() {
    assert(near(D_V_PI, 4.0 * D_PI / 3.0, 1.0e-12));
    assert(D_MAX_FLOAT == std::numeric_limits<float>::max());
    assert(near(D_LERP<int>(0, 10, 0.25f), 2.5));

    const DTools::Vec4 quotient =
        DTools::Vec4(8.0, 9.0, 10.0, 12.0) / DTools::Vec4(2.0, 3.0, 5.0, 4.0);
    assert(near(quotient[0], 4.0));
    assert(near(quotient[1], 3.0));
    assert(near(quotient[2], 2.0));
    assert(near(quotient[3], 3.0));

    const DTools::Vec3 source3(3.0, 4.0, 5.0);
    const DTools::Vec3 axis3(1.0, 0.0, 0.0);
    const DTools::Vec3 perpendicular3 = DTools::remCompB(source3, axis3);
    assert(near(perpendicular3[0], 0.0));
    assert(near(perpendicular3[1], 4.0));
    assert(near(perpendicular3[2], 5.0));

    DTools::Vec3 inPlace3(source3);
    DTools::remCompB2(inPlace3, axis3);
    assert(near(inPlace3[0], 0.0));
    assert(near(inPlace3[1], 4.0));
    assert(near(inPlace3[2], 5.0));

    DTools::Vec2 default2;
    assert(default2[0] == 0.0 && default2[1] == 0.0);
    assert(near(DTools::cross(DTools::Vec2(1.0, 0.0), DTools::Vec2(0.0, 1.0)), 1.0));

}

void testNumericBoundaryBehavior() {
    const double infinity = std::numeric_limits<double>::infinity();
    const double notANumber = std::numeric_limits<double>::quiet_NaN();

    assert(dommy::math::divideOrZero(8.0, 2.0) == 4.0);
    assert(dommy::math::divideOrZero(8.0, 0.0) == 0.0);
    assert(dommy::math::divideOrZero(infinity, 2.0) == 0.0);
    assert(dommy::math::divideOrZero(1.0, notANumber) == 0.0);
    assert(dommy::math::moduloOrZero(7.0, 4.0) == 3.0);
    assert(dommy::math::moduloOrZero(7.0, 0.0) == 0.0);
    assert(dommy::math::moduloOrZero(infinity, 4.0) == 0.0);
    assert(dommy::math::sqrtOrZero(-1.0) == 0.0);
    assert(dommy::math::sqrtOrZero(0.0) == 0.0);
    assert(dommy::math::sqrtOrZero(9.0) == 3.0);
    assert(dommy::math::sqrtOrZero(notANumber) == 0.0);
    assert(dommy::math::powerOrZero(2.0, 3.0) == 8.0);
    assert(dommy::math::powerOrZero(-1.0, 0.5) == 0.0);
    assert(dommy::math::powerOrZero(infinity, 2.0) == 0.0);
    assert(near(dommy::math::exponentialOrZero(1.0), std::exp(1.0)));
    assert(dommy::math::exponentialOrZero(infinity) == 0.0);

    assert(near(dommy::math::acosClamped(2.0), 0.0));
    assert(near(dommy::math::acosClamped(-2.0), std::acos(-1.0)));
    assert(dommy::math::acosClamped(notANumber) == 0.0);
    assert(dommy::math::angleRadians(1.0, 0.0, 1.0) == 0.0);
    assert(near(dommy::math::angleRadians(0.0, 1.0, 1.0), std::acos(0.0)));
    assert(dommy::math::angleRadians(notANumber, 1.0, 1.0) == 0.0);

    assert(near(dommy::math::fitRange(0.0, 10.0, 0.0, 100.0, 5.0), 50.0));
    assert(dommy::math::fitRange(1.0, 1.0, 20.0, 40.0, 99.0) == 20.0);
    assert(dommy::math::fitRange(0.0, 1.0, 0.0, 1.0, notANumber) == 0.0);
    assert(dommy::math::fitRangeClamped(0.0, 1.0, 10.0, 0.0, -1.0) == 10.0);
    assert(dommy::math::fitRangeClamped(0.0, 1.0, 10.0, 0.0, 2.0) == 0.0);

    double xRotation = 1.0;
    double zRotation = 1.0;
    dommy::math::aimY(
        MVector(0.0, 0.0, 0.0), xRotation, zRotation);
    assert(xRotation == 0.0 && zRotation == 0.0);

    dommy::math::aimY(
        MVector(0.0, 0.0, 1.0), xRotation, zRotation);
    assert(near(xRotation, std::acos(0.0)));
    assert(zRotation == 0.0);
}

void testSafeVectorNormalization() {
    float x = 1.0F;
    float y = 1.0F;
    float z = 1.0F;
    assert(!dommy::math::normalized3OrZero(
        0.0F, 0.0F, 0.0F, x, y, z));
    assert(x == 0.0F && y == 0.0F && z == 0.0F);

    assert(dommy::math::normalized3OrZero(
        3.0F, 4.0F, 0.0F, x, y, z));
    assert(near(x, 0.6) && near(y, 0.8) && z == 0.0F);

    const float maximum = std::numeric_limits<float>::max();
    assert(dommy::math::normalized3OrZero(
        maximum, maximum, maximum, x, y, z));
    assert(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
    assert(near(x, 1.0 / std::sqrt(3.0), 1.0e-5));

    assert(!dommy::math::normalized3OrZero(
        std::numeric_limits<float>::quiet_NaN(), 1.0F, 2.0F,
        x, y, z));
    assert(x == 0.0F && y == 0.0F && z == 0.0F);

    double outputX = 1.0;
    double outputY = 1.0;
    assert(!dommy::math::normalized2OrZero(
        std::numeric_limits<double>::infinity(), 1.0,
        outputX, outputY));
    assert(outputX == 0.0 && outputY == 0.0);
}

void testAttributeTemplateSafety() {
    static_assert(DPAttributeTypeSupported<float>::value, "float attributes must be supported");
    static_assert(!DPAttributeTypeSupported<long double>::value,
                  "unsupported types must be rejected explicitly");
    static_assert(DP_ATTR_UINT32 == DP_ATTR_INT32,
                  "the legacy uint32 attribute id must remain scene-compatible");

    std::vector<float> source;
    source.push_back(1.0f);
    source.push_back(2.0f);
    source.push_back(3.0f);
    DPFloatAttr attribute(source, -1.0f);
    const DPFloatAttr& constAttribute = attribute;
    assert(constAttribute.getValue(1U) == 2.0f);
    assert(&constAttribute.getValueArray() == &constAttribute.value());
    assert(constAttribute.str() == "float[3]");
    bool rejectedOutOfRangeRead = false;
    try {
        constAttribute.getValue(3U);
    } catch (const std::out_of_range&) {
        rejectedOutOfRangeRead = true;
    }
    assert(rejectedOutOfRangeRead);

    DPUInt32Attr unsignedAttribute(std::vector<uint32_t>(1U, 7U), 0U);
    assert(unsignedAttribute.getAttrType() == DP_ATTR_UINT32);
    assert(unsignedAttribute.typeName() == "uint32");
    assert(unsignedAttribute.str() == "uint32[1]");

    attribute.insertDefaultValue(2, 1);
    assert(attribute.attrSize() == 5);
    assert(attribute.value()[0] == 1.0f);
    assert(attribute.value()[1] == -1.0f);
    assert(attribute.value()[2] == -1.0f);
    assert(attribute.value()[3] == 2.0f);
    assert(attribute.value()[4] == 3.0f);

    const std::vector<float> beforeInvalidInsert = attribute.value();
    attribute.insertDefaultValue(1, 99);
    assert(attribute.value() == beforeInvalidInsert);

    DPIndexList invalidOrder;
    invalidOrder.push_back(0);
    invalidOrder.push_back(1);
    invalidOrder.push_back(2);
    invalidOrder.push_back(3);
    invalidOrder.push_back(99);
    attribute.sortArrayFromIds(invalidOrder);
    assert(attribute.value() == beforeInvalidInsert);

    DPIdList wrongMask(1, true);
    attribute.cropArrayFromIds(wrongMask);
    assert(attribute.value() == beforeInvalidInsert);

    std::vector<int32_t> integers(1, 7);
    DPIntAttr incompatible(integers, 0);
    std::ostringstream capturedError;
    std::streambuf* previousErrorBuffer = std::cerr.rdbuf(capturedError.rdbuf());
    attribute.copy(incompatible);
    std::cerr.rdbuf(previousErrorBuffer);
    assert(attribute.value() == beforeInvalidInsert);
    assert(capturedError.str().empty());

    const std::vector<float> beforeInvalidReplacement =
        attribute.value();
    const std::vector<float> wrongSizedReplacement(1U, 42.0f);
    assert(!attribute.replaceValues(
        wrongSizedReplacement, beforeInvalidReplacement.size()));
    assert(attribute.value() == beforeInvalidReplacement);
    assert(!attribute.setValueAt(attribute.attrSize(), 42.0f));

    std::vector<float> replacement(
        beforeInvalidReplacement.size(), 4.0f);
    assert(attribute.replaceValues(
        replacement, beforeInvalidReplacement.size()));
    assert(attribute.value() == replacement);
    assert(attribute.setValueAt(2U, 8.0f));
    assert(attribute.value()[2U] == 8.0f);

    DPFloatAttr defaultConstructed;
    assert(defaultConstructed.attrSize() == 0);
    assert(defaultConstructed.getDefaultValue() == 0.0f);
    assert(typeDefaultVal<std::string>().empty());

    std::vector<DIntArray> nestedValues;
    nestedValues.push_back(DIntArray(1, 10));
    nestedValues.push_back(DIntArray(2, 20));
    nestedValues.push_back(DIntArray(3, 30));
    DPAttributeData::Ptr polymorphic =
        DPIntArrayAttr(nestedValues).copy();
    assert(polymorphic);

    DPIndexList nestedOrder;
    nestedOrder.push_back(2);
    nestedOrder.push_back(0);
    nestedOrder.push_back(1);
    assert(polymorphic->sortArrayFromIds(nestedOrder));
    DPIntArrayAttr::Ptr nestedAttribute =
        std::dynamic_pointer_cast<DPIntArrayAttr>(polymorphic);
    assert(nestedAttribute);
    assert(nestedAttribute->value()[0].size() == 3);
    assert(nestedAttribute->value()[0][0] == 30);
    assert(nestedAttribute->value()[1][0] == 10);
    assert(nestedAttribute->value()[2][0] == 20);

    const std::vector<DIntArray> beforeInvalidRemap =
        nestedAttribute->value();
    DPIndexList invalidRemap(1, 99);
    assert(!polymorphic->remapFromIds(invalidRemap));
    assert(nestedAttribute->value() == beforeInvalidRemap);

    DPIdList deleteMask;
    deleteMask.push_back(false);
    deleteMask.push_back(true);
    deleteMask.push_back(false);
    assert(polymorphic->deleteArrayFromIds(deleteMask));
    assert(nestedAttribute->value().size() == 2);
    assert(nestedAttribute->value()[0][0] == 30);
    assert(nestedAttribute->value()[1][0] == 20);
}

void testHashGridReuse() {
    static_assert(!std::is_copy_constructible<DHashGrid>::value,
                  "DHashGrid must not be shallow-copied");
    DHashGrid grid(3);
    DReal points[] = {0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 1.5f, 1.5f, 1.5f};
    std::vector<DNeigh> neighbors;
    for (int iteration = 0; iteration < 100; ++iteration) {
        assert(grid.Setup(DTools::Vec3(0.0), DTools::Vec3(2.0), 1.0f, 3));
        assert(grid.SetObjectToCell(points, 3));
        assert(grid.GetNN(DTools::Vec3(0.1f), points, 3, neighbors, 0.3f));
        assert(neighbors.size() == 2); // Replaces rather than appends.
    }

    bool admitted = false;
    assert(!grid.Setup(DTools::Vec3(0.0), DTools::Vec3(2.0), 1.0f, 3,
        [&](std::size_t count, std::size_t cells) {
            assert(count == 3U && cells == 8U);
            admitted = true;
            return false;
        }));
    assert(admitted && !grid.SetObjectToCell(points, 3));
    assert(neighbors.size() == 2);
    assert(grid.Setup(DTools::Vec3(0.0), DTools::Vec3(2.0), 1.0f, 3));
    try {
        grid.Setup(DTools::Vec3(0.0), DTools::Vec3(2.0), 1.0f, 3,
            [](std::size_t, std::size_t) -> bool { throw std::length_error("storage"); });
        assert(false);
    } catch (const std::length_error&) {}
    assert(!grid.SetObjectToCell(points, 3));
    assert(grid.Setup(DTools::Vec3(0.0), DTools::Vec3(2.0), 1.0f, 3));

    // An over-budget grid must fail explicitly and invalidate an older index.
    assert(!grid.Setup(DTools::Vec3(0.0), DTools::Vec3(256.0), 1.0f, 3));
    assert(!grid.SetObjectToCell(points, 3));
    assert(!grid.GetNN(DTools::Vec3(0.1f), points, 3, neighbors, 0.3f));
    assert(neighbors.size() == 2);
    assert(!grid.Setup(DTools::Vec3(2.0), DTools::Vec3(1.0), 1.0f, 3));
    assert(!grid.Setup(DTools::Vec3(0.0), DTools::Vec3(1.0), 0.0f, 3));
    assert(!grid.Setup(DTools::Vec3(0.0), DTools::Vec3(1.0), 1.0f, -1));
    assert(!grid.Setup(DTools::Vec3(0.0), DTools::Vec3(1.0), 1.0f, 10000001));
    assert(!grid.Setup(DTools::Vec3(0.0),
        DTools::Vec3(std::numeric_limits<DReal>::infinity()), 1.0f, 3));

    // A degenerate finite box is a valid one-cell grid, not a setup error.
    DReal singlePoint[] = {0.5f, 0.5f, 0.5f};
    assert(grid.Setup(DTools::Vec3(0.5f), DTools::Vec3(0.5f), 1.0f, 1));
    assert(grid.SetObjectToCell(singlePoint, 1));
    assert(grid.GetNN(DTools::Vec3(0.5f), singlePoint, 1, neighbors, 0.0f));
    assert(neighbors.size() == 1 && neighbors[0].Idx == 0);
    assert(!grid.GetNN(DTools::Vec3(0.5f), singlePoint, 0, neighbors, 1.0f));
    assert(!grid.GetNN(DTools::Vec3(std::numeric_limits<DReal>::quiet_NaN()),
                      singlePoint, 1, neighbors, 1.0f));
    assert(!grid.GetNN(DTools::Vec3(0.5f), singlePoint, 1, neighbors,
                      std::numeric_limits<DReal>::infinity()));
    assert(!grid.GetNN(DTools::Vec3(0.5f), singlePoint, 1, neighbors,
                      -std::numeric_limits<DReal>::infinity()));
    assert(neighbors.size() == 1 && neighbors[0].Idx == 0);

    DReal invalidPoint[] = {
        std::numeric_limits<DReal>::quiet_NaN(), 0.5f, 0.5f};
    assert(!grid.SetObjectToCell(invalidPoint, 1));
    assert(!grid.GetNN(DTools::Vec3(0.5f), singlePoint, 1, neighbors, 1.0f));
    assert(neighbors.size() == 1); // Failure preserves the caller's output.
    assert(grid.SetObjectToCell(singlePoint, 1));
    assert(grid.GetNN(DTools::Vec3(5.0f), singlePoint, 1, neighbors, 0.1f));
    assert(neighbors.empty()); // Successful empty result is distinguishable.
    assert(grid.Setup(DTools::Vec3(0.0), DTools::Vec3(0.0), 1.0f, 0));
    assert(grid.SetObjectToCell(NULL, 0));
    assert(grid.GetNN(DTools::Vec3(0.0), NULL, 0, neighbors, 1.0f));
    assert(neighbors.empty());

    DReal pair[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    assert(grid.Setup(DTools::Vec3(0.0), DTools::Vec3(1.0), 1.0f, 2));
    assert(grid.SetObjectToCell(pair, 2));
    assert(grid.GetNN(DTools::Vec3(0.0), pair, 2, neighbors, 1.0f));
    assert(neighbors.size() == 2); // Inclusive radius boundary and self.
    assert(grid.GetNN_Direct(DTools::Vec3(0.0), pair, 2, neighbors, 1.0f));
    assert(neighbors.size() == 1 && neighbors[0].Idx == 1);

    // The budget expires after one candidate was collected. No partial result
    // escapes, and rebuilding cannot reset the instance-wide work counter.
    DHashGrid limited(3, 33U);
    assert(limited.Setup(DTools::Vec3(0.0), DTools::Vec3(1.0), 1.0f, 2));
    assert(limited.SetObjectToCell(pair, 2));
    bool threw = false;
    try {
        limited.GetNN(DTools::Vec3(0.0), pair, 2, neighbors, 1.0f);
    } catch (const std::length_error&) { threw = true; }
    assert(threw && neighbors.size() == 1 && neighbors[0].Idx == 1);
    threw = false;
    try { limited.SetObjectToCell(pair, 2); }
    catch (const std::length_error&) { threw = true; }
    assert(threw);
    assert(!limited.GetNN(DTools::Vec3(0.0), pair, 2, neighbors, 1.0f));

    // Sparse queries also charge empty-cell traversal, not only hits.
    DHashGrid sparse(3, 2100U);
    DReal corner[] = {7.5f, 7.5f, 7.5f};
    assert(sparse.Setup(DTools::Vec3(0.0), DTools::Vec3(8.0), 1.0f, 1));
    assert(sparse.SetObjectToCell(corner, 1));
    threw = false;
    try { sparse.GetNN(DTools::Vec3(0.0), corner, 1, neighbors, 8.0f); }
    catch (const std::length_error&) { threw = true; }
    assert(threw && neighbors.size() == 1 && neighbors[0].Idx == 1);

    // Compare ordinary indexed queries against a direct distance oracle.
    std::vector<DReal> cloud(90U);
    for (std::size_t i = 0; i < cloud.size(); ++i) {
        cloud[i] = static_cast<DReal>((i * 37U) % 101U) / 10.0f - 2.0f;
    }
    for (int variant = 0; variant < 4; ++variant) {
        // Include points outside the supplied bounds (clamped edge cells).
        const DReal maximum = variant == 0 ? 8.0f : 3.0f;
        const DReal width = 0.5f * static_cast<DReal>(1U << variant);
        assert(grid.Setup(DTools::Vec3(-2.0), DTools::Vec3(maximum), width, 30));
        assert(grid.SetObjectToCell(cloud.data(), 30));
        for (uint query = 0U; query < 30U; ++query) {
            const DTools::Vec3 point(cloud[query * 3U], cloud[query * 3U + 1U],
                                    cloud[query * 3U + 2U]);
            assert(grid.GetNN(point, cloud.data(), 30, neighbors, 2.0f));
            std::vector<int> actual, expected;
            for (const DNeigh& neighbor : neighbors) actual.push_back(neighbor.Idx);
            for (uint i = 0U; i < 30U; ++i) {
                const DTools::Vec3 other(cloud[i * 3U], cloud[i * 3U + 1U],
                                        cloud[i * 3U + 2U]);
                if (norm2(point - other) <= 4.0f) expected.push_back(static_cast<int>(i));
            }
            std::sort(actual.begin(), actual.end());
            assert(actual == expected);
        }
    }
}

void testUvParameterNormalization() {
    float normalized = -1.0f;
    assert(dommy::uv::normalizeParameter(10.0, 10.0, 20.0, normalized));
    assert(near(normalized, 0.0));
    assert(dommy::uv::normalizeParameter(15.0, 10.0, 20.0, normalized));
    assert(near(normalized, 0.5));
    assert(dommy::uv::normalizeParameter(20.0, 10.0, 20.0, normalized));
    assert(near(normalized, 1.0));
    assert(dommy::uv::normalizeParameter(15.0, 20.0, 10.0, normalized));
    assert(near(normalized, 0.5));

    assert(!dommy::uv::normalizeParameter(
        10.0, 10.0, 10.0, normalized));
    assert(!dommy::uv::normalizeParameter(
        std::numeric_limits<double>::quiet_NaN(),
        10.0,
        20.0,
        normalized));
    assert(!dommy::uv::normalizeParameter(
        std::numeric_limits<double>::infinity(),
        10.0,
        20.0,
        normalized));
}

void testCurveUVMapping() {
    dommy::curve::UVMappingPlan plan;
    assert(dommy::curve::buildUVMappingPlan(
        0, 0, 0.0, 2.0, 3.0, -1.0, plan));
    assert(!plan.useRamp && !plan.useArcLength);

    double value = 0.0;
    assert(dommy::curve::pointUVPosition(0U, 1U, value));
    assert(near(value, 0.0));
    assert(dommy::curve::pointUVPosition(2U, 5U, value));
    assert(near(value, 0.5));
    assert(!dommy::curve::pointUVPosition(0U, 0U, value));
    assert(!dommy::curve::pointUVPosition(5U, 5U, value));

    assert(dommy::curve::arcLengthUVPosition(5.0, 10.0, value));
    assert(near(value, 0.5));
    assert(dommy::curve::arcLengthUVPosition(0.0, 0.0, value));
    assert(near(value, 0.0));
    assert(dommy::curve::arcLengthUVPosition(11.0, 10.0, value));
    assert(near(value, 1.0));
    assert(!dommy::curve::arcLengthUVPosition(-1.0, 10.0, value));

    assert(dommy::curve::mapUVValue(plan, 0.5, value));
    assert(near(value, 2.0));
    assert(!dommy::curve::mapUVValue(
        plan, std::numeric_limits<double>::quiet_NaN(), value));

    assert(dommy::curve::buildUVMappingPlan(
        1, 1, 2.0, -2.0, 0.5, 1.0, plan));
    assert(plan.useRamp && plan.useArcLength);
    assert(!dommy::curve::buildUVMappingPlan(
        2, 0, 0.0, 1.0, 1.0, 0.0, plan));
    assert(!dommy::curve::buildUVMappingPlan(
        0, 2, 0.0, 1.0, 1.0, 0.0, plan));
    assert(!dommy::curve::buildUVMappingPlan(
        0, 0, 0.0, 1.0,
        std::numeric_limits<double>::infinity(), 0.0, plan));

    const double limit = std::numeric_limits<double>::max();
    for (double start : {-limit, limit}) {
        for (double end : {-1.0, 1.0}) {
            assert(dommy::curve::buildUVMappingPlan(0, 0, start, end, 0.5, 0.25, plan));
            assert(dommy::curve::mapUVValue(plan, 0.0, value));
            assert(value == start * 0.5 + 0.25);
            assert(dommy::curve::mapUVValue(plan, 1.0, value));
            assert(value == end * 0.5 + 0.25);
        }
    }
    assert(dommy::curve::buildUVMappingPlan(1, 1, 2.25, -3.5, 1.25, -0.75, plan));
    for (double t : {-0.5, 0.25, 0.5, 0.75, 1.5}) {
        assert(dommy::curve::mapUVValue(plan, t, value));
        assert(value == (2.25 + t * (-3.5 - 2.25)) * 1.25 - 0.75);
    }
    const auto previous = plan;
    assert(!dommy::curve::buildUVMappingPlan(0, 0, -limit, limit, 1.0, 0.0, plan));
    assert(plan.minimum == previous.minimum && plan.maximum == previous.maximum &&
        plan.span == previous.span && plan.scale == previous.scale && plan.offset == previous.offset);
    assert(dommy::curve::buildUVMappingPlan(0, 0, -0.0, -0.0, 1.0, -0.0, plan));
    for (double t : {0.0, 1.0}) {
        assert(dommy::curve::mapUVValue(plan, t, value));
        assert(value == 0.0 && std::signbit(value));
    }
}

void testRigidBodyParameterValidation() {
    dommy::bullet::RigidBodyParameters parameters;
    assert(dommy::bullet::validateRigidBodyParameters(
        1.0, 0.25, 0.5, 0.1, 0.2, parameters));
    assert(near(parameters.mass, 1.0));
    assert(near(parameters.restitution, 0.25));
    assert(near(parameters.friction, 0.5));
    assert(near(parameters.linearDamping, 0.1));
    assert(near(parameters.angularDamping, 0.2));

    assert(!dommy::bullet::validateRigidBodyParameters(
        -1.0, 0.25, 0.5, 0.1, 0.2, parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(
        1.0, 1.01, 0.5, 0.1, 0.2, parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(
        1.0, 0.25, -0.5, 0.1, 0.2, parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(
        1.0, 0.25, 0.5, 1.01, 0.2, parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(
        1.0, 0.25, 0.5, 0.1, 1.01, parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(
        std::numeric_limits<double>::quiet_NaN(),
        0.25,
        0.5,
        0.1,
        0.2,
        parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(
        std::numeric_limits<double>::max(),
        0.25,
        0.5,
        0.1,
        0.2,
        parameters));
}

void* stubCreator() {
    return 0;
}

template <typename Cleanup>
MStatus registerTestCleanup(dommy::PluginRegistration& registration, Cleanup cleanup) {
    return registration.registerResource([]() { return MStatus(MS::kSuccess); }, cleanup);
}

void testPluginRegistrationLifetime() {
    MFnPlugin plugin;
    dommy::PluginLifetime lifetime;

    std::vector<int> rollbackOrder;
    {
        dommy::PluginRegistration registration(plugin, lifetime);
        registerTestCleanup(registration, [&rollbackOrder](MFnPlugin&) {
            rollbackOrder.push_back(1);
            return MStatus(MS::kSuccess);
        });
        registerTestCleanup(registration, [&rollbackOrder](MFnPlugin&) {
            rollbackOrder.push_back(2);
            return MStatus(MS::kSuccess);
        });
    }
    assert(rollbackOrder.size() == 2);
    assert(rollbackOrder[0] == 2 && rollbackOrder[1] == 1);

    int retryAttempts = 0;
    std::vector<int> cleanupOrder;
    {
        dommy::PluginRegistration registration(plugin, lifetime);
        registerTestCleanup(registration, [&cleanupOrder](MFnPlugin&) {
            cleanupOrder.push_back(1);
            return MStatus(MS::kSuccess);
        });
        registerTestCleanup(registration, [&cleanupOrder, &retryAttempts](MFnPlugin&) {
            cleanupOrder.push_back(2);
            ++retryAttempts;
            return MStatus(retryAttempts == 1 ? MS::kFailure : MS::kSuccess);
        });
        registerTestCleanup(registration, [&cleanupOrder](MFnPlugin&) {
            cleanupOrder.push_back(3);
            return MStatus(MS::kSuccess);
        });
        assert(registration.commit() == MS::kSuccess);
    }

    assert(lifetime.cleanup(plugin) == MS::kFailure);
    assert(cleanupOrder.size() == 3);
    assert(cleanupOrder[0] == 3 && cleanupOrder[1] == 2 && cleanupOrder[2] == 1);
    assert(!lifetime.empty());
    assert(lifetime.cleanup(plugin) == MS::kSuccess);
    assert(cleanupOrder.size() == 4 && cleanupOrder[3] == 2);
    assert(lifetime.empty());

    bool throwOnce = true;
    {
        dommy::PluginRegistration registration(plugin, lifetime);
        registerTestCleanup(registration, [&throwOnce](MFnPlugin&) {
            if (throwOnce) {
                throwOnce = false;
                throw 1;
            }
            return MStatus(MS::kSuccess);
        });
        assert(registration.commit() == MS::kSuccess);
    }
    assert(lifetime.cleanup(plugin) == MS::kFailure);
    assert(!lifetime.empty());
    assert(lifetime.cleanup(plugin) == MS::kSuccess);
    assert(lifetime.empty());

    maya_stub::pluginEvents().clear();
    {
        dommy::PluginRegistration registration(plugin, lifetime);
        assert(registration.registerNode("testNode", MTypeId(1), stubCreator) == MS::kSuccess);
        assert(registration.registerCommand("testCommand", stubCreator) == MS::kSuccess);
        assert(registration.registerData("testData", MTypeId(2), stubCreator) == MS::kSuccess);
        assert(registration.registerGeometryOverrideCreator(
                   "drawdb/geometry/test", "testRegistrant", stubCreator) == MS::kSuccess);
        assert(registration.registerDrawOverrideCreator(
                   "drawdb/drawOverride/test", "testRegistrant", stubCreator) == MS::kSuccess);
    }

    const char* expectedEvents[] = {"registerNode",
                                    "registerCommand",
                                    "registerData",
                                    "registerGeometryOverride",
                                    "registerDrawOverride",
                                    "deregisterDrawOverride",
                                    "deregisterGeometryOverride",
                                    "deregisterData",
                                    "deregisterCommand",
                                    "deregisterNode"};
    assert(maya_stub::pluginEvents().size() == 10);
    for (std::size_t i = 0; i < 10; ++i) {
        assert(maya_stub::pluginEvents()[i] == expectedEvents[i]);
    }

    maya_stub::pluginEvents().clear();
    bool failedRegistrationRolledBack = false;
    {
        dommy::PluginRegistration registration(plugin, lifetime);
        registerTestCleanup(registration, [&failedRegistrationRolledBack](MFnPlugin&) {
            failedRegistrationRolledBack = true;
            return MStatus(MS::kSuccess);
        });
        assert(registration.registerCallback([](MStatus& status) {
            status = MS::kSuccess; return MCallbackId(0);
        }) == MS::kFailure);
        assert(registration.commit() == MS::kFailure);
    }
    assert(failedRegistrationRolledBack);
    assert(lifetime.empty());

    {
        dommy::PluginRegistration registration(plugin, lifetime);
        assert(registration.registerCallback([](MStatus& status) {
            status = MS::kSuccess; return MCallbackId(42);
        }) == MS::kSuccess);
    }
    assert(maya_stub::pluginEvents().size() == 1);
    assert(maya_stub::pluginEvents()[0] == "removeCallback");
}

void testColorModeContract() {
    using namespace dommy::color;

    assert(isValidMode(kSolid));
    assert(isValidMode(kPalette));
    assert(!isValidMode(-1));
    assert(!isValidMode(kPalette + 1));
    assert(isValidAssignment(kPrimitive));
    assert(isValidAssignment(kPoint));
    assert(!isValidAssignment(2));
    assert(!requiresPointAssignment(kSolid));
    assert(!requiresPointAssignment(kRandom));
    assert(requiresPointAssignment(kRamp));
    assert(requiresPointAssignment(kTexture));
    assert(requiresPointAssignment(kBoundingBox));
    assert(requiresPointAssignment(kExpression));
    assert(!requiresPointAssignment(kPalette));
    assert(isFiniteTriple(0.0, -1.0, 1.0));
    assert(!isFiniteTriple(
        std::numeric_limits<double>::infinity(), 0.0, 0.0));
    assert(!isFiniteTriple(
        0.0, std::numeric_limits<double>::quiet_NaN(), 0.0));
    assert(isOrderedFiniteRange(-1.0, 0.0, 1.0, 0.0, 1.0, 2.0));
    assert(!isOrderedFiniteRange(1.0, 0.0, 0.0, 0.0, 1.0, 1.0));
    assert(!isOrderedFiniteRange(
        0.0, 0.0, 0.0,
        std::numeric_limits<double>::infinity(), 1.0, 1.0));
}
} // namespace

void runMeshBoundaryTests();

namespace {
struct TestPlugHandle { int token = 0; };
struct TestPlugState {
    int acquisitions = 0;
    int releases = 0;
    int live = 0;
    int context = 0;
    int releasedContext = -1;
    bool fail = false;
};
struct TestPlug {
    TestPlugState* state = nullptr;
    bool isNull() const { return !state; }
    MStatus getValue(TestPlugHandle& handle) const {
        if (state->fail) return MS::kFailure;
        handle.token = ++state->acquisitions;
        ++state->live;
        return MS::kSuccess;
    }
    void destructHandle(TestPlugHandle& handle) const {
        assert(handle.token != 0);
        handle.token = 0;
        ++state->releases;
        --state->live;
        state->releasedContext = state->context;
    }
};
struct TestEvaluationContext {
    TestPlugState& state;
    explicit TestEvaluationContext(TestPlugState& s) : state(s) { state.context = 7; }
    ~TestEvaluationContext() { state.context = 0; }
};

void testPlugHandleOwnership() {
    int failedCalls = 0;
    const auto failOnce = [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE((++failedCalls, MStatus(MS::kFailure)));
        return MS::kSuccess;
    };
    assert(failOnce() == MS::kFailure && failedCalls == 1);
    typedef dommy::maya::PlugHandleOwner<TestPlug, TestPlugHandle> Owner;
    static_assert(!std::is_copy_constructible<Owner>::value, "Plug ownership must be unique");
    static_assert(!std::is_copy_assignable<Owner>::value, "Plug ownership must be unique");
    TestPlugState state;
    TestPlug plug;
    plug.state = &state;
    {
        Owner owner;
        assert(owner.read(TestPlug()) == MS::kFailure);
        assert(state.releases == 0);
        assert(owner.read(plug) == MS::kSuccess);
        assert(owner.handle().token == 1 && state.live == 1);
        assert(owner.read(plug) == MS::kSuccess);
        assert(state.live == 1 && state.releases == 1);
        state.fail = true;
        assert(owner.read(plug) == MS::kFailure);
        assert(state.live == 0 && state.releases == 2);
    }
    assert(state.acquisitions == state.releases);
    state.fail = false;
    try {
        TestEvaluationContext context(state);
        Owner owner;
        assert(owner.read(plug) == MS::kSuccess);
        throw std::runtime_error("simulated consumer failure");
    } catch (const std::runtime_error&) {}
    assert(state.live == 0 && state.acquisitions == state.releases);
    assert(state.releasedContext == 7 && state.context == 0);
    // Ordinary early returns must release in exactly the same way.
    const auto failAfterRead = [&]() {
        Owner owner;
        assert(owner.read(plug) == MS::kSuccess);
        return false;
    };
    assert(!failAfterRead());
    assert(state.live == 0 && state.acquisitions == state.releases);
}

struct PublicationAttribute {
    std::string type;
    std::vector<float> values;
    std::string typeName() const { return type; }
    std::size_t attrSize() const { return values.size(); }
};

struct PublicationOwner {
    std::map<std::string, std::shared_ptr<PublicationAttribute> > attributes;
    bool ignoreWrite = false;
    bool throwOnWrite = false;
    unsigned int writes = 0U;
    unsigned int removals = 0U;
    std::shared_ptr<const PublicationAttribute> getAttr(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? nullptr : found->second;
    }
    void addAttr(const std::string& name, const PublicationAttribute& attribute) {
        ++writes;
        if (throwOnWrite) throw std::bad_alloc();
        if (ignoreWrite) return;
        attributes[name] = std::make_shared<PublicationAttribute>(attribute);
    }
    void removeAttr(const std::string& name) {
        ++removals;
        attributes.erase(name);
    }
};

void testMathAttributePublication() {
    using dommy::attribute::publishAttributeReplacement;
    PublicationOwner owner;
    const PublicationAttribute source = {"float", {1.0f, 2.0f}};
    const PublicationAttribute result = {"float", {3.0f, 4.0f}};
    owner.addAttr("source", source);
    const auto original = owner.getAttr("source");
    assert(publishAttributeReplacement(owner, "source", "result", result, false));
    assert(owner.getAttr("source") == original && owner.removals == 0U);
    assert(owner.getAttr("result")->values == result.values);
    // A same-name replacement must survive DeleteOriginal=true.
    assert(publishAttributeReplacement(owner, "source", "source", result, true));
    assert(owner.getAttr("source")->values == result.values && owner.removals == 0U);
    const auto sameNameResult = owner.getAttr("source");
    const PublicationAttribute incompatible = {"double", {5.0f, 6.0f}};
    owner.addAttr("conflict", incompatible);
    const unsigned int beforeConflict = owner.writes;
    assert(!publishAttributeReplacement(owner, "source", "conflict", result, true));
    assert(!publishAttributeReplacement(owner, "source", "source", incompatible, true));
    assert(owner.writes == beforeConflict && owner.removals == 0U);
    assert(owner.getAttr("source") == sameNameResult);
    assert(owner.getAttr("conflict")->values == incompatible.values);
    assert(!publishAttributeReplacement(owner, "missing", "result", result, true));
    assert(!publishAttributeReplacement(owner, "source", "", result, true));
    assert(!publishAttributeReplacement(owner, "source", std::string("a\0b", 3), result, true));
    const PublicationAttribute shortResult = {"float", {3.0f}};
    assert(!publishAttributeReplacement(owner, "source", "result", shortResult, true));
    owner.ignoreWrite = true;
    assert(!publishAttributeReplacement(owner, "source", "result", result, true));
    assert(!publishAttributeReplacement(owner, "source", "new", result, true));
    assert(owner.getAttr("source") == sameNameResult && owner.removals == 0U);
    owner.ignoreWrite = false;
    owner.throwOnWrite = true;
    bool threw = false;
    try { publishAttributeReplacement(owner, "source", "result", result, true); }
    catch (const std::bad_alloc&) { threw = true; }
    assert(threw && owner.getAttr("source") == sameNameResult && owner.removals == 0U);
    owner.throwOnWrite = false;
    assert(publishAttributeReplacement(owner, "source", "result", result, true));
    assert(!owner.getAttr("source") && owner.removals == 1U);
    assert(owner.getAttr("result")->values == result.values);
}

template <typename T> struct ConversionTestAttribute {
    std::vector<T> values;
    unsigned int writes = 0U;
    const std::vector<T>& value() const { return values; }
    bool replaceValues(const std::vector<T>& replacement, std::size_t count) {
        if (count != values.size()) return false;
        values = replacement;
        ++writes;
        return true;
    }
};

template <typename T> struct ConversionTestOwner {
    std::shared_ptr<ConversionTestAttribute<T> > attribute;
    template <typename Destination>
    std::shared_ptr<ConversionTestAttribute<Destination> >
    getValidTypedAttributedata(const std::string& name) {
        return name == "my weight" ? attribute : nullptr;
    }
};

struct StoredConversionTestOwner {
    DPAttrList attributes;
    bool ignoreWrite = false;
    bool throwOnWrite = false;
    unsigned int writes = 0U;
    unsigned int removals = 0U;
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(attributes, name);
    }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        ++writes;
        if (throwOnWrite) throw std::bad_alloc();
        if (ignoreWrite) return;
        const auto previous = getAttr(name);
        if (previous && previous->typeName() != value.typeName()) return;
        attributes[name] = value.copy();
    }
    void removeAttr(const std::string& name) {
        ++removals;
        attributes.erase(name);
    }
    template <typename T>
    typename TypedDPAttributeData<T>::Ptr
    getValidTypedAttributedata(const std::string& name) {
        return std::dynamic_pointer_cast<TypedDPAttributeData<T> >(
            dommy::attribute::findAttribute(attributes, name));
    }
};

template <typename T> struct UnaryOutputArray {
    std::vector<T> values;
    unsigned int allocations = 0U;
    bool fail = false;
    bool shorten = false;
    bool throwOnAllocate = false;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    T& operator[](unsigned int i) { return values.at(i); }
    MStatus setLength(unsigned int count) {
        ++allocations;
        if (throwOnAllocate) throw std::bad_alloc();
        if (fail) return MS::kFailure;
        values.resize(shorten && count ? count - 1U : count);
        return MS::kSuccess;
    }
};

void testUnaryMathPipelines() {
    using namespace dommy::math;
    using dommy::attribute::readNumericAttributeValues;
    const FitOperation identity = {{0,0,0}, {1,1,1}, {0,0,0}, {1,1,1}};
    const FitOperation hugeOutput = {{0,0,0}, {1,1,1}, {0,0,0}, {3e9,3e9,3e9}};
    const NormalizeOperation normalize;
    StoredConversionTestOwner owner;
    const double precise = 0.5 + 1.0e-12;
    owner.attributes["weight"] = std::make_shared<DPDoubleAttr>(std::vector<double>{precise});
    DMathScalarDataList scalar;
    assert(readNumericAttributeValues(owner, "weight", 1U, scalar));
    assert(identity(scalar) && scalar[0] == precise);
    assert(DPublishAttributeValues(owner, false, false, "", "weight", scalar) == MS::kSuccess);
    assert(owner.getValidTypedAttributedata<double>("weight")->value()[0] == precise);
    assert(DPublishAttributeValues(owner, true, false, "float output", "weight", scalar) == MS::kSuccess);
    assert(owner.getValidTypedAttributedata<float>("float output")->value()[0] == float(precise));
    const auto source = owner.getAttr("weight");
    assert(DPublishAttributeValues(owner, true, true, "weight", "weight", scalar) != MS::kSuccess);
    assert(owner.getAttr("weight") == source);

    owner.attributes["ints"] = std::make_shared<DPIntAttr>(std::vector<int32_t>{INT_MIN, INT_MAX, 16777217});
    assert(readNumericAttributeValues(owner, "ints", 3U, scalar) && identity(scalar));
    UnaryOutputArray<int> integerOutput;
    auto readScalar = [&](std::size_t i) { return scalar[i]; };
    assert(writeScalarMathArray<int>(scalar.size(), readScalar, integerOutput) == MS::kSuccess);
    assert(integerOutput.values == std::vector<int>({INT_MIN, INT_MAX, 16777217}));
    assert(!readNumericAttributeValues(owner, "ints", 2U, scalar));
    assert(!readNumericAttributeValues(owner, "missing", 3U, scalar));
    owner.attributes["wide"] = std::make_shared<DPInt64Attr>(std::vector<int64_t>{(int64_t(1) << 53) + 1});
    assert(!readNumericAttributeValues(owner, "wide", 1U, scalar));
    owner.attributes["wide"] = std::make_shared<DPInt64Attr>(std::vector<int64_t>{int64_t(1) << 53});
    assert(readNumericAttributeValues(owner, "wide", 1U, scalar));
    owner.attributes["uint"] = std::make_shared<DPUInt32Attr>(std::vector<uint32_t>{UINT_MAX});
    assert(readNumericAttributeValues(owner, "uint", 1U, scalar) && scalar[0] == double(UINT_MAX));
    owner.attributes["bad"] = std::make_shared<DPDoubleAttr>(std::vector<double>{1.0, NAN});
    assert(!readNumericAttributeValues(owner, "bad", 2U, scalar) && scalar[0] == double(UINT_MAX));

    // The former integer-array UB now fails before Maya allocation/publication.
    scalar = {1.0};
    assert(hugeOutput(scalar) && scalar[0] == 3e9);
    integerOutput.values = {7}; integerOutput.allocations = 0U;
    assert(writeScalarMathArray<int>(scalar.size(), readScalar, integerOutput) != MS::kSuccess);
    assert(integerOutput.values[0] == 7 && integerOutput.allocations == 0U);
    struct Settings { double minimum[3], maximum[3]; bool keepRange; };
    const Settings largeRange = {{0,0,0}, {2147483648.0,1,1}, true};
    const auto identityRamp = [](unsigned int, float position, float& value) {
        value = position; return true;
    };
    scalar = {double(INT_MAX)};
    assert(remapValues(scalar, largeRange, identityRamp));
    // Maya's float ramp position still rounds here; reject the final int overflow.
    assert(scalar[0] == 2147483648.0);
    assert(writeScalarMathArray<int>(1U, readScalar, integerOutput) != MS::kSuccess);
    assert(integerOutput.values[0] == 7 && integerOutput.allocations == 0U);
    scalar = {0.25, 0.5};
    const auto before = scalar;
    const Settings unitRange = {{0,0,0}, {1,1,1}, false};
    unsigned int samples = 0U;
    assert(!remapValues(scalar, unitRange, [&](unsigned int, float, float& value) {
        value = 0.75f; return ++samples < 2U;
    }));
    assert(scalar == before);
    assert(!remapValues(scalar, unitRange, [](unsigned int, float, float& value) {
        value = std::numeric_limits<float>::quiet_NaN(); return true;
    }) && scalar == before);
    scalar = {1e300}; samples = 0U;
    assert(!remapValues(scalar, unitRange, [&](unsigned int, float, float&) { ++samples; return true; }));
    assert(samples == 0U && scalar[0] == 1e300);
    assert(DPublishAttributeValues(owner, true, true, "too large", "weight", scalar) != MS::kSuccess);
    assert(owner.getAttr("weight") == source && !owner.getAttr("too large"));

    const FitOperation perAxis = {{0,10,100}, {10,20,200}, {0,100,-1}, {1,200,1}};
    DMathVec3DataList fitted3 = {Imath::V3d(5,15,150)};
    DMathVec2DataList fitted2 = {Imath::V2d(5,15)};
    assert(perAxis(fitted3) && fitted3[0][0] == 0.5 && fitted3[0][1] == 150 && fitted3[0][2] == 0);
    assert(perAxis(fitted2) && fitted2[0][0] == 0.5 && fitted2[0][1] == 150);
    const Settings vectorRanges = {{0,10,100}, {10,20,200}, true};
    fitted3 = {Imath::V3d(5,15,150)}; fitted2 = {Imath::V2d(5,15)};
    const auto perAxisRamp = [](unsigned int axis, float position, float& value) {
        assert(position == 0.5f); value = (axis + 1U) * 0.25f; return true;
    };
    assert(remapValues(fitted3, vectorRanges, perAxisRamp));
    assert(fitted3[0][0] == 2.5 && fitted3[0][1] == 15 && fitted3[0][2] == 175);
    assert(remapValues(fitted2, vectorRanges, perAxisRamp) && fitted2[0][0] == 2.5 && fitted2[0][1] == 15);

    const FitOperation collapsed = {{1,1,1}, {1,1,1}, {4,5,6}, {9,9,9}};
    scalar = {2}; assert(collapsed(scalar) && scalar[0] == 4);
    scalar = {0.25, NAN};
    assert(!identity(scalar) && scalar[0] == 0.25 && std::isnan(scalar[1]));
    scalar = {1e300};
    UnaryOutputArray<float> floatOutput;
    assert(writeScalarMathArray<float>(1U, readScalar, floatOutput) != MS::kSuccess);
    assert(floatOutput.allocations == 0U);

    owner.attributes["direction"] = std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{
        Imath::V3d(1e300,0,0), Imath::V3d(1e-300,0,0), Imath::V3d(0,0,0)});
    DMathVec3DataList vectors;
    assert(readNumericAttributeValues(owner, "direction", 3U, vectors) && normalize(vectors));
    assert(vectors[0][0] == 1.0 && vectors[1][0] == 1.0 && vectors[2][0] == 0.0);
    assert(DPublishAttributeValues(owner, false, false, "", "direction", vectors) == MS::kSuccess);
    assert(owner.getValidTypedAttributedata<Imath::V3d>("direction")->value()[0][0] == 1.0);
    assert(DPublishAttributeValues(owner, true, false, "normal", "direction", vectors) == MS::kSuccess);
    assert(owner.getValidTypedAttributedata<Imath::V3f>("normal")->value()[1][0] == 1.0f);
    owner.attributes["vec2"] = std::make_shared<DPVec2dAttr>(std::vector<Imath::V2d>{Imath::V2d(1e300,0)});
    DMathVec2DataList vec2;
    assert(readNumericAttributeValues(owner, "vec2", 1U, vec2) && normalize(vec2) && vec2[0][0] == 1.0);
    assert(!readNumericAttributeValues(owner, "vec2", 1U, vectors));
    vectors = {Imath::V3d(3,0,0), Imath::V3d(0,0,NAN)};
    assert(!normalize(vectors) && vectors[0][0] == 3.0);

    // Allocation errors, misleading short success, invalid last components,
    // empty results, and over-budget requests must never produce a partial success.
    scalar = {1,2,3};
    integerOutput.fail = true;
    assert(writeScalarMathArray<int>(3U, readScalar, integerOutput) == MS::kFailure);
    assert(integerOutput.values == std::vector<int>({7}));
    integerOutput.fail = false; integerOutput.shorten = true;
    assert(writeScalarMathArray<int>(3U, readScalar, integerOutput) == MS::kFailure);
    integerOutput.shorten = false; integerOutput.throwOnAllocate = true;
    bool threw = false;
    try { writeScalarMathArray<int>(3U, readScalar, integerOutput); }
    catch (const std::bad_alloc&) { threw = true; }
    assert(threw); integerOutput.throwOnAllocate = false;
    integerOutput.allocations = 0U;
    scalar = {1, double(INT_MAX) + 1};
    assert(writeScalarMathArray<int>(2U, readScalar, integerOutput) != MS::kSuccess);
    assert(integerOutput.allocations == 0U);
    unsigned int reads = 0U;
    assert(writeScalarMathArray<int>(std::numeric_limits<std::size_t>::max(),
        [&](std::size_t) { ++reads; return 0; }, integerOutput) != MS::kSuccess);
    assert(reads == 0U && integerOutput.allocations == 0U);
    const std::size_t limit = (256U * 1024U * 1024U) / 96U - 1U;
    assert(validMathBudget(limit, 0U) && !validMathBudget(limit + 1U, 0U));
    scalar.clear(); assert(identity(scalar));
    assert(writeScalarMathArray<int>(0U, readScalar, integerOutput) == MS::kSuccess && integerOutput.length() == 0U);
    UnaryOutputArray<Imath::V3d> vectorOutput;
    auto readVector = [&](std::size_t i) -> const Imath::V3d& { return vectors[i]; };
    assert(writeVectorMathArray<Imath::V3d>(vectors.size(), readVector, vectorOutput) != MS::kSuccess);
    assert(vectorOutput.allocations == 0U);
    vectors = {Imath::V3d(precise,1e300,0)};
    assert(writeVectorMathArray<Imath::V3d>(1U, readVector, vectorOutput) == MS::kSuccess);
    assert(vectorOutput.values[0][0] == precise && vectorOutput.values[0][1] == 1e300);
    vectorOutput.fail = true;
    assert(writeVectorMathArray<Imath::V3d>(1U, readVector, vectorOutput) == MS::kFailure);
    vectorOutput.fail = false; vectors.clear(); assert(normalize(vectors));
    assert(writeVectorMathArray<Imath::V3d>(0U, readVector, vectorOutput) == MS::kSuccess && vectorOutput.length() == 0U);
}

struct VectorNodeTestOwner : StoredConversionTestOwner {
    std::size_t pointCount = 1U;
    std::size_t numberOfPoints() const { return pointCount; }
};

void testVectorNodePipelines() {
    using namespace dommy::math;
    const VectorNodeOperation<VectorNodeKind::Length> length;
    const VectorNodeOperation<VectorNodeKind::Minimum> minimum;
    const VectorNodeOperation<VectorNodeKind::Maximum> maximum;
    const VectorNodeOperation<VectorNodeKind::Average> average;
    const VectorNodeOperation<VectorNodeKind::Dot> dot;
    const VectorNodeOperation<VectorNodeKind::Angle> angle;
    const VectorNodeOperation<VectorNodeKind::Cross> cross;
    VectorNodeTestOwner owner;
    VectorNodeInput input;
    DMathScalarDataList result;
    DMathVec3DataList vectors;
    const DMathVec3DataList none;
    auto setSource = [&](const DMathVec3DataList& values) {
        owner.attributes["source"] = std::make_shared<DPVec3dAttr>(values);
        assert(readVectorNodeAttribute(owner, "source", values.size(), input));
    };
    setSource({{1e100, 0, 0}, {1e-100, 0, 0}, {3, 4, 0}});
    assert(evaluateVectorNode(input, none, false, 0U, length, result));
    assert(result[0] == 1e100 && result[1] == 1e-100 && result[2] == 5.0);
    UnaryOutputArray<double> array;
    assert(writeScalarMathArray<double>(result.size(), [&](std::size_t i) { return result[i]; }, array) == MS::kSuccess);
    assert(array.values == result);
    array.fail = true;
    assert(writeScalarMathArray<double>(result.size(), [&](std::size_t i) { return result[i]; }, array) != MS::kSuccess);
    array.fail = false; array.shorten = true;
    assert(writeScalarMathArray<double>(result.size(), [&](std::size_t i) { return result[i]; }, array) != MS::kSuccess);

    setSource({{16777217.0, 16777219.0, 16777221.0}});
    assert(evaluateVectorNode(input, none, false, 0U, minimum, result) && result[0] == 16777217.0);
    assert(evaluateVectorNode(input, none, false, 0U, maximum, result) && result[0] == 16777221.0);
    owner.attributes["source"] = std::make_shared<DPVec3iAttr>(std::vector<Imath::V3i>{{INT_MAX, INT_MAX - 1, INT_MAX - 2}});
    assert(readVectorNodeAttribute(owner, "source", 1U, input));
    assert(evaluateVectorNode(input, none, false, 0U, maximum, result) && result[0] == double(INT_MAX));
    owner.attributes["source"] = std::make_shared<DPVec2iAttr>(std::vector<Imath::V2i>{{2, 4}, {-4, -2}});
    assert(readVectorNodeAttribute(owner, "source", 2U, input) && input.components == 2U);
    assert(evaluateVectorNode(input, none, false, 0U, minimum, result) && result[0] == 2.0);
    assert(evaluateVectorNode(input, none, false, 0U, maximum, result) && result[1] == -2.0);
    assert(evaluateVectorNode(input, none, false, 0U, average, result) && result[0] == 3.0 && result[1] == -3.0);
    const double max = std::numeric_limits<double>::max();
    setSource({{3e38, 3e38, 3e38}, {max, max, max}, {max, -max, 0}, {1e300, 1, -1e300}});
    assert(evaluateVectorNode(input, none, false, 0U, average, result));
    assert(result[0] == 3e38 && result[1] == max && result[2] == 0.0 && near(result[3], 1.0/3.0));
    for (const DMathVec3Data& value : DMathVec3DataList{
             {1e308, 1e-300, -1e308}, {1e-300, 1e308, -1e308},
             {-1e308, 1e308, -1e-300}}) {
        double exact = 91.0, actual = 91.0;
        assert(dommy::numeric::checkedScalarMean(std::array<double, 3>{{value[0], value[1], value[2]}}, exact));
        assert(vectorNodeValue(VectorNodeKind::Average, value, 3U, DMathVec3Data(0,0,0), actual));
        assert(actual == exact && actual != 0.0);
    }
    const auto unchanged = result;
    assert(!evaluateVectorNode(input, none, false, 0U, length, result) && result == unchanged);
    input.values.back()[0] = std::numeric_limits<double>::quiet_NaN();
    assert(!evaluateVectorNode(input, none, false, 0U, maximum, result) && result == unchanged);

    setSource({{1e-300, 0, 0}, {1e300, 0, 0}, {0, 0, 0}});
    assert(evaluateVectorNode(input, input.values, false, 0U, dot, result));
    assert(result == DMathScalarDataList({1, 1, 0}));
    const DMathVec3DataList up{{0, 1e300, 0}};
    assert(evaluateVectorNode(input, up, true, 0U, cross, vectors));
    assert(vectors[0][2] == 1.0 && vectors[1][2] == 1.0 && vectors[2][2] == 0.0);
    UnaryOutputArray<DMathVec3Data> vectorArray;
    const auto readVector = [&](std::size_t i) -> const DMathVec3Data& { return vectors.at(i); };
    assert(writeVectorMathArray<DMathVec3Data>(vectors.size(), readVector, vectorArray) == MS::kSuccess);
    assert(vectorArray.values[0][2] == 1.0);
    vectorArray.fail = true;
    assert(writeVectorMathArray<DMathVec3Data>(vectors.size(), readVector, vectorArray) != MS::kSuccess);
    vectorArray.fail = false; vectorArray.shorten = true;
    assert(writeVectorMathArray<DMathVec3Data>(vectors.size(), readVector, vectorArray) != MS::kSuccess);
    assert(evaluateVectorNode(input, up, true, 0U, angle, result));
    assert(near(result[0], std::acos(0.0)) && result[2] == 0.0);
    assert(!evaluateVectorNode(input, up, false, 0U, dot, result));
    assert(!evaluateVectorNode(input, up, false, std::numeric_limits<std::size_t>::max(), dot, result));
    assert(!evaluateVectorNode(input, none, true, 0U, cross, vectors));
    input.components = 1U;
    assert(!evaluateVectorNode(input, up, true, 0U, dot, result));
    input.components = 3U;
    assert(!readVectorNodeAttribute(owner, "missing", 3U, input));
    assert(!readVectorNodeAttribute(owner, "source", 2U, input));
    assert(!readVectorNodeAttribute(owner, "source", std::numeric_limits<std::size_t>::max(), input));
    owner.attributes["scalar"] = std::make_shared<DPDoubleAttr>(std::vector<double>{1});
    assert(!readVectorNodeAttribute(owner, "scalar", 1U, input));

    std::vector<std::shared_ptr<VectorNodeTestOwner> > parts;
    for (int i = 0; i < 2; ++i) {
        auto part = std::make_shared<VectorNodeTestOwner>();
        part->attributes["source"] = std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{{3, 4, 0}});
        part->attributes["target"] = std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{{0, 1, 0}});
        parts.push_back(part);
    }
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source", "length", 0, "", none, length));
    for (const auto& part : parts) {
        assert(part->getValidTypedAttributedata<float>("length")->value()[0] == 5.0f);
        assert(part->getAttr("source") && part->removals == 0U);
    }
    const auto first = parts[0]->getAttr("source"), second = parts[1]->getAttr("source");
    assert(!processVectorNodePartitions(parts.begin(), parts.end(), "source", "source", 0, "", none, length));
    assert(parts[0]->getAttr("source") == first && parts[1]->getAttr("source") == second);
    const DMathVec3DataList targets{{1, 0, 0}, {0, 1, 0}};
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source", "dot", 2, "", targets, dot));
    assert(near(parts[0]->getValidTypedAttributedata<float>("dot")->value()[0], 0.6));
    assert(near(parts[1]->getValidTypedAttributedata<float>("dot")->value()[0], 0.8));
    assert(!processVectorNodePartitions(parts.begin(), parts.end(), "source", "dot", 2, "", up, dot));
    assert(!processVectorNodePartitions(parts.begin(), parts.end(), "source", "dot", 9, "", targets, dot));
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source", "target", 1, "target", none, cross));
    for (const auto& part : parts) {
        assert(near(part->getValidTypedAttributedata<Imath::V3f>("target")->value()[0][2], 0.6));
        assert(part->removals == 0U);
    }
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source", "source", 0, "", up, cross));
    for (const auto& part : parts)
        assert(near(part->getValidTypedAttributedata<Imath::V3f>("source")->value()[0][2], 0.6));
    parts[1]->attributes.erase("source");
    assert(!processVectorNodePartitions(parts.begin(), parts.end(), "source", "new", 0, "", none, length));
    parts[1]->pointCount = std::numeric_limits<std::size_t>::max();
    std::size_t count = 17U;
    assert(!vectorNodePartitionCount(parts.begin(), parts.end(), count) && count == 17U);
    parts[1].reset();
    assert(!vectorNodePartitionCount(parts.begin(), parts.end(), count));
    parts.clear();
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source", "new", 0, "", none, length));
    for (const std::size_t size : {2U, 0U, 1U}) {
        auto part = std::make_shared<VectorNodeTestOwner>();
        part->pointCount = size;
        part->attributes["source with space"] = std::make_shared<DPVec3fAttr>(
            std::vector<Imath::V3f>(size, Imath::V3f(1, 0, 0)));
        parts.push_back(part);
    }
    const DMathVec3DataList unevenTargets{{0, 1, 0}, {0, -1, 0}, {1, 0, 0}};
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source with space", "cross", 2, "", unevenTargets, cross));
    assert(parts[0]->getValidTypedAttributedata<Imath::V3f>("cross")->value()[0][2] == 1.0f);
    assert(parts[0]->getValidTypedAttributedata<Imath::V3f>("cross")->value()[1][2] == -1.0f);
    assert(parts[1]->getValidTypedAttributedata<Imath::V3f>("cross")->value().empty());
    assert(parts[2]->getValidTypedAttributedata<Imath::V3f>("cross")->value()[0][2] == 0.0f);
    assert(processVectorNodePartitions(parts.begin(), parts.end(), "source with space", "dot", 2, "", unevenTargets, dot));
    assert(parts[2]->getValidTypedAttributedata<float>("dot")->value()[0] == 1.0f);
    assert(!processVectorNodePartitions(parts.begin(), parts.end(), "source", "dot", 2, "", unevenTargets, dot));

    setSource({{1e100, 0, 0}});
    const auto saved = owner.getAttr("source");
    assert(!processVectorNodeAttribute(owner, 1U, "source", "overflow", 0, "", none, 0U, length));
    assert(owner.getAttr("source") == saved && !owner.getAttr("overflow"));
    setSource({{3e38, 3e38, 3e38}});
    assert(processVectorNodeAttribute(owner, 1U, "source", "mean", 0, "", none, 0U, average));
    assert(std::isfinite(owner.getValidTypedAttributedata<float>("mean")->value()[0]));
    owner.ignoreWrite = true;
    assert(!processVectorNodeAttribute(owner, 1U, "source", "ignored", 0, "", none, 0U, average));
    owner.ignoreWrite = false; owner.throwOnWrite = true;
    assert(!processVectorNodeAttribute(owner, 1U, "source", "throws", 0, "", none, 0U, average));

    setSource({});
    assert(evaluateVectorNode(input, none, false, 0U, length, result) && result.empty());
    assert(evaluateVectorNode(input, none, false, 0U, cross, vectors) && vectors.empty());
    array.shorten = false;
    assert(writeScalarMathArray<double>(0U, [&](std::size_t i) { return result.at(i); }, array) == MS::kSuccess);
    assert(array.values.empty());
    vectorArray.shorten = false;
    assert(writeVectorMathArray<DMathVec3Data>(0U, readVector, vectorArray) == MS::kSuccess);
    assert(vectorArray.values.empty());
}

void testLegacyAttributeNodePaths() {
    testVectorNodePipelines();
    testUnaryMathPipelines();
    using namespace dommy::attribute;
    ConvertedArray result;
    const auto integer = std::make_shared<DPIntAttr>(std::vector<int32_t>{INT_MIN, INT_MAX, 16777217});
    assert(convertAttributeToArray(integer, 3U, result));
    assert(result.kind == ArrayKind::Int && result.integers == std::vector<int>({INT_MIN, INT_MAX, 16777217}));
    assert(!convertAttributeToArray(integer, 2U, result));
    assert(!convertAttributeToArray(nullptr, 0U, result));
    assert(result.integers[1] == INT_MAX);
    const auto unsignedValues = std::make_shared<DPUInt32Attr>(std::vector<uint32_t>{0U, uint32_t(INT_MAX)});
    assert(convertAttributeToArray(unsignedValues, 2U, result));
    assert(result.integers == std::vector<int>({0, INT_MAX}));
    assert(!convertAttributeToArray(std::make_shared<DPUInt32Attr>(
        std::vector<uint32_t>{1U, std::numeric_limits<uint32_t>::max()}), 2U, result));
    assert(result.integers == std::vector<int>({0, INT_MAX}));
    assert(convertAttributeToArray(std::make_shared<TypedDPAttributeData<int64_t> >(
        std::vector<int64_t>{INT_MIN, INT_MAX}), 2U, result));
    assert(!convertAttributeToArray(std::make_shared<TypedDPAttributeData<int64_t> >(
        std::vector<int64_t>{std::numeric_limits<int64_t>::max()}), 1U, result));
    const double precise = 1.0 + 1.0e-12;
    assert(convertAttributeToArray(std::make_shared<DPDoubleAttr>(
        std::vector<double>{precise, 1.0e300}), 2U, result));
    assert(result.kind == ArrayKind::Double && result.doubles[0] == precise && result.doubles[1] == 1.0e300);
    assert(!convertAttributeToArray(std::make_shared<DPDoubleAttr>(
        std::vector<double>{1.0, std::numeric_limits<double>::infinity()}), 2U, result));
    assert(result.doubles[0] == precise);
    assert(convertAttributeToArray(std::make_shared<DPFloatAttr>(std::vector<float>{1.25f}), 1U, result));
    assert(result.kind == ArrayKind::Float && result.floats[0] == 1.25f && result.doubles.empty());
    assert(!convertAttributeToArray(std::make_shared<DPFloatAttr>(
        std::vector<float>{std::numeric_limits<float>::quiet_NaN()}), 1U, result));
    assert(result.floats[0] == 1.25f);
    const DPAttributeData::ConstPtr vectors[] = {
        std::make_shared<DPVec2iAttr>(std::vector<Imath::V2i>{Imath::V2i(INT_MAX, 2)}),
        std::make_shared<DPVec2fAttr>(std::vector<Imath::V2f>{Imath::V2f(1.25f, 2)}),
        std::make_shared<DPVec2dAttr>(std::vector<Imath::V2d>{Imath::V2d(precise, 1.0e300)}),
        std::make_shared<DPVec3iAttr>(std::vector<Imath::V3i>{Imath::V3i(INT_MAX, 2, 3)}),
        std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{Imath::V3f(1.25f, 2, 3)}),
        std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{Imath::V3d(precise, 1.0e300, 3)})
    };
    for (unsigned int i = 0U; i < 6U; ++i) {
        assert(convertAttributeToArray(vectors[i], 1U, result));
        assert(result.kind == ArrayKind::Vector && result.vectors.size() == 1U);
        assert(result.vectors[0][0] == (i % 3U == 0U ? double(INT_MAX) : i % 3U == 1U ? 1.25 : precise));
        assert(result.vectors[0][2] == (i < 3U ? 0.0 : 3.0));
    }
    assert(!convertAttributeToArray(std::make_shared<DPVec3dAttr>(
        std::vector<Imath::V3d>{Imath::V3d(1, 2, std::numeric_limits<double>::quiet_NaN())}), 1U, result));
    assert(result.vectors[0][0] == precise);
    assert(convertAttributeToArray(std::make_shared<DPDoubleAttr>(std::vector<double>()), 0U, result));
    assert(result.kind == ArrayKind::Double && result.doubles.empty() && result.vectors.empty());
    assert(!convertAttributeToArray(std::make_shared<DPBoolAttr>(std::vector<bool>{true}), 1U, result));
    assert(validArrayConversionBudget(0U, sizeof(Imath::V3d)));
    assert(!validArrayConversionBudget(std::numeric_limits<std::size_t>::max(), sizeof(double)));
    const std::size_t vectorLimit = (256U * 1024U * 1024U) / (4U * sizeof(Imath::V3d));
    assert(validArrayConversionBudget(vectorLimit, sizeof(Imath::V3d)));
    assert(!validArrayConversionBudget(vectorLimit + 1U, sizeof(Imath::V3d)));

    StoredConversionTestOwner owner;
    owner.attributes["my weight"] = std::make_shared<DPFloatAttr>(std::vector<float>{1.0f});
    const auto original = owner.getAttr("my weight");
    assert(DPublishAttributeValues(owner, true, true, "my weight", "my weight", std::vector<float>{2.0f}) == MS::kSuccess);
    assert(owner.getAttr("my weight") != original && owner.removals == 0U);
    assert(owner.getValidTypedAttributedata<float>("my weight")->value()[0] == 2.0f);
    const auto unchanged = owner.getAttr("my weight");
    owner.attributes["collision"] = std::make_shared<DPDoubleAttr>(std::vector<double>{99.0});
    assert(DPublishAttributeValues(owner, true, true, "collision", "my weight", std::vector<float>{3.0f}) != MS::kSuccess);
    assert(owner.getAttr("my weight") == unchanged && owner.removals == 0U);
    assert(owner.getValidTypedAttributedata<double>("collision")->value()[0] == 99.0);
    assert(DPublishAttributeValues(owner, true, true, "", "my weight", std::vector<float>{3.0f}) != MS::kSuccess);
    assert(DPublishAttributeValues(owner, true, true, "new", "missing", std::vector<float>{3.0f}) != MS::kSuccess);
    assert(DPublishAttributeValues(owner, true, true, "new", "my weight", std::vector<float>{}) != MS::kSuccess);
    assert(DPublishAttributeValues(owner, true, true, "new", "my weight", std::vector<float>{NAN}) != MS::kSuccess);
    owner.ignoreWrite = true;
    assert(DPublishAttributeValues(owner, true, true, "new", "my weight", std::vector<float>{3.0f}) != MS::kSuccess);
    owner.ignoreWrite = false;
    owner.throwOnWrite = true;
    assert(DPublishAttributeValues(owner, true, true, "new", "my weight", std::vector<float>{3.0f}) != MS::kSuccess);
    owner.throwOnWrite = false;
    assert(owner.getAttr("my weight") == unchanged && owner.removals == 0U);
    assert(DPublishAttributeValues(owner, true, true, "new", "my weight", std::vector<float>{3.0f}) == MS::kSuccess);
    assert(!owner.getAttr("my weight") && owner.removals == 1U);
    assert(DPublishAttributeValues(owner, false, true, "ignored", "collision", std::vector<float>{4.0f}) == MS::kSuccess);
    assert(owner.getValidTypedAttributedata<double>("collision")->value()[0] == 4.0);
    owner.attributes["vec2"] = std::make_shared<DPVec2fAttr>(std::vector<Imath::V2f>{Imath::V2f(1, 2)});
    assert(DPublishAttributeValues(owner, true, true, "vec2", "vec2", std::vector<Imath::V2f>{Imath::V2f(3, 4)}) == MS::kSuccess);
    assert(owner.getValidTypedAttributedata<Imath::V2f>("vec2")->value()[0][1] == 4.0f);
    owner.attributes["vec3"] = std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{Imath::V3f(1, 2, 3)});
    assert(DPublishAttributeValues(owner, true, false, "vec3out", "vec3", std::vector<Imath::V3f>{Imath::V3f(4, 5, 6)}) == MS::kSuccess);
    assert(owner.getAttr("vec3") && owner.getValidTypedAttributedata<Imath::V3f>("vec3out")->value()[0][2] == 6.0f);
}

void testAttributeLookupAndConversion() {
    using dommy::attribute::findAttribute;
    using dommy::numeric::checkedValueCast;
    std::map<std::string, std::shared_ptr<int> > attributes;
    attributes["weight"] = std::make_shared<int>(1);
    attributes["my weight"] = std::make_shared<int>(2);
    attributes["null"] = nullptr;
    attributes[""] = std::make_shared<int>(3);
    const std::string nulName("a\0b", 3);
    attributes[nulName] = std::make_shared<int>(4);
    assert(findAttribute(attributes, "weight") == attributes["weight"]);
    assert(findAttribute(attributes, "my weight") == attributes["my weight"]);
    for (const std::string name : {"weight ", " weight", "we ight", "myweight", "null", ""})
        assert(!findAttribute(attributes, name));
    assert(!findAttribute(attributes, nulName));
    const auto& constAttributes = attributes;
    assert(findAttribute(constAttributes, "my weight") == attributes["my weight"]);

    // Exercise the production typed storage and clone, not only test doubles.
    StoredConversionTestOwner storedOwner;
    const DPVec3dAttr vectorSource(std::vector<Imath::V3d>{Imath::V3d(1, 2, 3)},
                                  Imath::V3d(7, 8, 9));
    storedOwner.attributes["my weight"] = vectorSource.copy();
    const auto vectorCopy = storedOwner.getValidTypedAttributedata<Imath::V3d>("my weight");
    assert(vectorCopy && vectorCopy->getAttrType() == DP_ATTR_VEC3D);
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        assert(vectorCopy->getDefaultValue()[axis] == 7.0 + axis);
        assert(vectorCopy->value()[0][axis] == 1.0 + axis);
    }
    assert(replaceConvertedAttributeValues<Imath::V3d>(storedOwner, "my weight",
        std::vector<Imath::V3f>{Imath::V3f(4, 5, 6)}));
    assert(vectorCopy->value()[0][0] == 4.0 && vectorSource.value()[0][0] == 1.0);
    assert(vectorCopy->getDefaultValue()[0] == 7.0);
    const DPVec2dAttr vector2Source(std::vector<Imath::V2d>{Imath::V2d(1, 2)}, Imath::V2d(8, 9));
    const auto vector2Copy = std::dynamic_pointer_cast<DPVec2dAttr>(vector2Source.copy());
    assert(vector2Copy && vector2Copy->getDefaultValue()[0] == 8.0 &&
           vector2Copy->getDefaultValue()[1] == 9.0 && vector2Copy->value()[0][1] == 2.0);
    storedOwner.attributes["integer"] =
        std::make_shared<TypedDPAttributeData<std::int64_t> >(std::vector<std::int64_t>{123});
    assert(!replaceConvertedAttributeValues<std::int64_t>(storedOwner, "integer",
        std::vector<double>{std::ldexp(1.0, 63)}));
    assert(storedOwner.getValidTypedAttributedata<std::int64_t>("integer")->value()[0] == 123);
    assert(replaceConvertedAttributeValues<std::int64_t>(storedOwner, "integer",
        std::vector<std::int64_t>{std::numeric_limits<std::int64_t>::max()}));
    assert(storedOwner.getValidTypedAttributedata<std::int64_t>("integer")->value()[0] ==
           std::numeric_limits<std::int64_t>::max());

    std::int64_t signed64 = 0;
    std::uint64_t unsigned64 = 0;
    const auto signedMax = std::numeric_limits<std::int64_t>::max();
    const auto unsignedMax = std::numeric_limits<std::uint64_t>::max();
    assert(checkedValueCast(signedMax, signed64) && signed64 == signedMax);
    assert(checkedValueCast(unsignedMax, unsigned64) && unsigned64 == unsignedMax);
    assert(!checkedValueCast(unsignedMax, signed64) && signed64 == signedMax);
    assert(!checkedValueCast(-1, unsigned64) && unsigned64 == unsignedMax);
    assert(!checkedValueCast(std::ldexp(1.0, 63), signed64));
    assert(!checkedValueCast(std::ldexp(1.0, 64), unsigned64));
    double wide = 0.0;
    assert(checkedValueCast(16777217, wide, true) && wide == 16777217.0);
    assert(checkedValueCast(INT_MAX, wide, true) && wide == INT_MAX);
    assert(!checkedValueCast(signedMax, wide, true));
    assert(!checkedValueCast(std::int64_t(9007199254740993LL), wide, true));
    assert(checkedValueCast(1.0 + 1.0e-12, wide, true) && wide == 1.0 + 1.0e-12);
    float narrow = 9.0f;
    assert(!checkedValueCast(std::numeric_limits<double>::max(), narrow) && narrow == 9.0f);
    assert(!checkedValueCast(std::numeric_limits<double>::infinity(), narrow));
    assert(!checkedValueCast(std::numeric_limits<double>::quiet_NaN(), narrow));

    ConversionTestOwner<int> owner;
    owner.attribute = std::make_shared<ConversionTestAttribute<int> >();
    owner.attribute->values = {10, 20};
    assert(!replaceConvertedAttributeValues<int>(owner, "my weight", std::vector<double>{1.0, 2147483648.0}));
    assert(owner.attribute->values == std::vector<int>({10, 20}) && owner.attribute->writes == 0U);
    assert(!replaceConvertedAttributeValues<int>(owner, "my weight", std::vector<double>{1.0}));
    assert(!replaceConvertedAttributeValues<int>(owner, "missing", std::vector<double>{1.0, 2.0}));
    assert(replaceConvertedAttributeValues<int>(owner, "my weight", std::vector<double>{INT_MAX, -2.75}));
    assert(owner.attribute->values == std::vector<int>({INT_MAX, -2}) && owner.attribute->writes == 1U);

    ConversionTestOwner<Imath::V3i> vectorOwner;
    vectorOwner.attribute = std::make_shared<ConversionTestAttribute<Imath::V3i> >();
    vectorOwner.attribute->values = {Imath::V3i(4, 5, 6)};
    assert(!replaceConvertedAttributeValues<Imath::V3i>(vectorOwner, "my weight",
        std::vector<Imath::V3d>{Imath::V3d(1, 2, 2147483648.0)}));
    assert(vectorOwner.attribute->values[0][0] == 4 && vectorOwner.attribute->values[0][1] == 5 &&
        vectorOwner.attribute->values[0][2] == 6 && vectorOwner.attribute->writes == 0U);
    assert(replaceConvertedAttributeValues<Imath::V3i>(vectorOwner, "my weight",
        std::vector<Imath::V3d>{Imath::V3d(1, -2.5, INT_MAX)}));
    assert(vectorOwner.attribute->values[0][0] == 1 && vectorOwner.attribute->values[0][1] == -2 &&
        vectorOwner.attribute->values[0][2] == INT_MAX);
    Imath::V2i vec2(8, 9);
    assert(!checkedValueCast(Imath::V2d(1, std::numeric_limits<double>::quiet_NaN()), vec2));
    assert(vec2[0] == 8 && vec2[1] == 9);
    assert(!checkedValueCast(Imath::V3d(1, 2, 3), vec2));

    DMathScalarData result = INT_MAX;
    assert(dommy::math::applyProceduralBinary(D_CAL_ADD, result, 0.0));
    int stored = 0;
    assert(checkedValueCast(result, stored) && stored == INT_MAX);
    result = 16777217.0;
    assert(dommy::math::applyProceduralBinary(D_CAL_ADD, result, 2.0) && result == 16777219.0);
    result = 1.0 + 1.0e-12;
    assert(dommy::math::applyProceduralUnary(D_SRC_ABS, result) && result == 1.0 + 1.0e-12);
    Imath::V3d vectorResult(10, 20, 30);
    assert(dommy::math::applyProceduralBinary(D_CAL_ADD, vectorResult, Imath::V2d(2, 3)));
    assert(vectorResult[0] == 12 && vectorResult[1] == 23 && vectorResult[2] == 30);
    assert(dommy::math::applyProceduralBinary(D_CAL_MULT, vectorResult, 2.0));
    assert(vectorResult[0] == 24 && vectorResult[1] == 46 && vectorResult[2] == 60);
    result = 1.0;
    assert(dommy::math::applyProceduralBinary(D_CAL_ADD, result, Imath::V3d(2, 3, 4)) && result == 3.0);
}

void testMathInputAndEvaluation() {
    testLegacyAttributeNodePaths();
    testAttributeLookupAndConversion();
    testMathAttributePublication();
    unsigned int attempts = 0U;
    bool reachedPublication = false;
    const auto transientFailure = [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(++attempts == 1U ? MStatus(MS::kFailure) : MStatus(MS::kSuccess));
        reachedPublication = true;
        return MS::kSuccess;
    };
    assert(transientFailure() == MS::kFailure);
    assert(attempts == 1U && !reachedPublication);
    using namespace dommy::math;
    using namespace dommy::numeric;
    std::int64_t signed64 = 123;
    std::uint64_t unsigned64 = 456U;
    const long double signedUpper = std::ldexp(1.0L, 63);
    const long double unsignedUpper = std::ldexp(1.0L, 64);
    assert(!checkedNumericCast(signedUpper, signed64) && signed64 == 123);
    assert(!checkedNumericCast(unsignedUpper, unsigned64) && unsigned64 == 456U);
    assert(checkedNumericCast(-signedUpper, signed64));
    assert(signed64 == std::numeric_limits<std::int64_t>::min());
    assert(!checkedNumericCast(std::nextafter(-signedUpper,
        -std::numeric_limits<long double>::infinity()), signed64));
    const long double largestSigned = std::floor(std::nextafter(signedUpper, 0.0L));
    const long double largestUnsigned = std::floor(std::nextafter(unsignedUpper, 0.0L));
    assert(checkedNumericCast(largestSigned, signed64));
    assert(static_cast<long double>(signed64) == largestSigned);
    assert(checkedNumericCast(largestUnsigned, unsigned64));
    assert(static_cast<long double>(unsigned64) == largestUnsigned);
    assert(!checkedNumericCast(-1.0L, unsigned64));
    assert(!checkedNumericCast(std::numeric_limits<long double>::infinity(), unsigned64));
    assert(!checkedNumericCast(std::numeric_limits<long double>::quiet_NaN(), signed64));
    // Exercise an actual consumer's Int64 attribute-conversion adapter too.
    assert(!dommy::transfer::checkedComponentCast(signedUpper, signed64));
    double result = 99.0;
    const double precise = 16777216.25;
    static_assert(std::is_same<decltype(DAdd(1.0f, 1.0f)), float>::value, "Float layout stays unchanged");
    static_assert(std::is_same<decltype(DAdd(1.0, 1.0)), double>::value, "Double must not narrow");
    static_assert(std::is_same<decltype(DAdd(1, 1)), double>::value, "Integer arithmetic promotes first");
    assert(DAdd(precise, 0.5) == 16777216.75);
    assert(DAdd(INT_MAX, 1) == double(INT_MAX) + 1.0);
    assert(DNegative(INT_MIN) == -double(INT_MIN));
    assert(DDiv(INT_MIN, -1) == -double(INT_MIN));
    assert(DMin(precise, precise + 1.0) == precise);
    assert(DCeil(precise) == 16777217.0);
    assert(checkedMathBinary(D_CAL_ADD, precise, 0.5, result));
    assert(result == 16777216.75);
    assert(checkedMathBinary(D_CAL_MIN, precise, precise + 1.0, result));
    assert(result == precise);
    assert(checkedMathUnary(D_SRC_FLOOR, precise, result));
    assert(result == 16777216.0);
    assert(checkedMathUnary(D_SRC_CEIL, precise, result));
    assert(result == 16777217.0);
    assert(checkedMathUnary(D_SRC_ROUND, precise + 0.5, result));
    assert(result == 16777217.0);
    int integer = 0;
    assert(checkedMathBinary(D_CAL_ADD, INT_MAX, 0.0, result));
    assert(checkedNumericCast(result, integer) && integer == INT_MAX);
    assert(checkedMathBinary(D_CAL_ADD, INT_MIN, 0.0, result));
    assert(checkedNumericCast(result, integer) && integer == INT_MIN);
    assert(checkedMathUnary(D_SRC_ABS, INT_MIN, result));
    assert(!checkedNumericCast(result, integer) && integer == INT_MIN);
    assert(checkedMathBinary(D_CAL_ADD, INT_MAX, 1.0, result));
    assert(!checkedNumericCast(result, integer) && integer == INT_MIN);
    assert(checkedNumericCast(-1.75, integer) && integer == -1);
    assert(!checkedMathBinary(D_CAL_ADD, std::numeric_limits<double>::max(),
                              std::numeric_limits<double>::max(), result));
    assert(!checkedMathUnary(D_SRC_ABS, std::numeric_limits<double>::infinity(), result));
    assert(!checkedMathBinary(D_CAL_MIN, std::numeric_limits<double>::quiet_NaN(), 1.0, result));
    assert(!checkedMathBinary(D_CAL_COUNT, 1.0, 1.0, result));
    assert(!checkedMathUnary(D_SRC_COUNT, 1.0, result));
    assert(checkedMathBinary(D_CAL_DIV, 10.0, 0.0, result) && result == 0.0);
    assert(checkedMathBinary(D_CAL_MOD, 10.0, 0.0, result) && result == 0.0);
    assert(checkedMathUnary(D_SRC_SQRT, -1.0, result) && result == 0.0);

    const std::vector<int> source = {INT_MIN, INT_MAX};
    std::vector<double> converted(1U, precise);
    assert(!checkedScalarArrayCopy(source, source.size(), 3U, converted));
    assert(converted == std::vector<double>(1U, precise));
    assert(!checkedScalarArrayCopy(source, source.size(), 1U, converted));
    assert(checkedScalarArrayCopy(source, source.size(), 2U, converted));
    assert(converted[0] == INT_MIN && converted[1] == INT_MAX);
    std::vector<float> narrowed(1U, 3.0f);
    const std::vector<double> overflowing = {std::numeric_limits<double>::max()};
    assert(!checkedScalarArrayCopy(overflowing, 1U, 1U, narrowed));
    assert(narrowed == std::vector<float>(1U, 3.0f));
    assert(checkedScalarArrayCopy(std::vector<double>(), 0U, 0U, converted));
    assert(converted.empty());
    std::vector<std::array<double, 3>> vectors(1U, std::array<double, 3>{{1.0, 2.0, 3.0}});
    std::vector<std::array<float, 3>> floatVectors;
    assert(checkedVectorArrayCopy(vectors, 1U, 1U, floatVectors));
    assert(floatVectors[0][2] == 3.0f);
    vectors[0][2] = std::numeric_limits<double>::infinity();
    assert(!checkedVectorArrayCopy(vectors, 1U, 1U, floatVectors));
    assert(floatVectors[0][2] == 3.0f);

    std::vector<double> values = {1.0, 2.0, 3.0};
    const auto add = [](double& lhs, double rhs) { return checkedMathBinary(D_CAL_ADD, lhs, rhs, lhs); };
    assert(applyMathOperand(values, std::vector<double>(1U, 4.0), add));
    assert((values == std::vector<double>{5.0, 6.0, 7.0}));
    assert(!applyMathOperand(values, std::vector<double>(2U, 4.0), add));
    assert((values == std::vector<double>{5.0, 6.0, 7.0}));
    assert(applyMathOperand(values, std::vector<double>{1.0, 2.0, 3.0}, add));
    assert((values == std::vector<double>{6.0, 8.0, 10.0}));
    values.clear();
    assert(applyMathOperand(values, std::vector<double>(), add));
    assert(applyMathOperand(values, std::vector<double>(1U, 4.0), add));
    assert(validMathBudget(0U, 0U));
    assert(validMathBudget(1000U, 10U, 2U));
    assert(!validMathBudget(1000000U, 1000000U));
    assert(!validMathBudget(std::numeric_limits<std::size_t>::max(), 1U));
    assert(!validMathBudget(1U, std::numeric_limits<std::size_t>::max()));
    assert(!validMathBudget(0U, 1000000U, 1000000U));
    const std::size_t sourceBoundary = (256U * 1024U * 1024U - 96U) / 96U;
    assert(validMathBudget(sourceBoundary, 0U));
    assert(!validMathBudget(sourceBoundary + 1U, 0U));
    const long double wide = std::numeric_limits<long double>::max() / 4.0L;
    assert(dommy::math::isFinite(wide));
    assert(DDiv(wide, wide) == 1.0L);
}
} // namespace

int main() {
    checkMayaPointBlendCases();
    runAttributeCastGroupTests();
    runExpressionSortTests();
    runPointGenerationTests();
    runRandomSamplingTests();
    runAttributeAccessTests();
    runPointStorageTests();
    runCurveSamplingTests();
    testPlugHandleOwnership();
    testMathInputAndEvaluation();
    runMeshBoundaryTests();
    runGeometryTests();
    testCheckedMayaArray();
    testMeshUVTransferAndPointMean();
    testStagedPositionEdits();
    testCurveSamplingMath();
    testBoundsReduction();
    testPassiveTransformInterpolation();
    testLineAndPlaneMath();
    testCheckedSizeArithmetic();
	testVectorLookup();
    testSpatialSearchPlan();
    testTextGenerationPlan();
    testExportPathValidation();
    testIndexTransformSafety();
    testVdbViewportNormalization();
	testParallelWorkAndPointDeformMath();
	testCurveDividePlan();
	testCurveFromCurvePlan();
	testCurveCopyBudget();
	testMeshDuplicationPlan();
	testBoundedPrefixCopy();
	testSymmetryPointMatcher();
	testIndexPatternParsing();
    testGoalMathSafety();
	testFillSpacePlan();
	testFrameSamplingLimits();
    testAbcSamplingPlan();
	testAtomicOutputFile();
    testArnoldArrayBudget();
    testDeformerSafetyHelpers();
    testGenerationLimitsAndIcosahedronTopology();
    testWoolCurvePlan();
    testAttributeTransferPlan();
    testEyelashAndDuplicateCurvePlans();
    testSignalMath();
    testAhoGenerationPlan();
    testClothSamplingPlan();
    testWireTopologyPlan();
    testSubdivisionBudget();
    testPerlinNoiseBoundaries();
    testMeshInputValidation();
    testStringReplacement();
    testVdbCombinePlan();
    testVdbParticleRasterPlan();
    testFluidGridPlan();
    testNumericValidation();
    testAttributeBlendMath();
    testRandomIntGeneration();
    testPointPackingMath();
    testGeometryLayoutArithmetic();
    testViewportBufferElementCount();
    testViewportPointBudget();
    testCollisionMeshInputValidation();
    testShapeFillAndViewportGeometryBudgets();
    testBulletMassProperties();
    testRangeRemapping();
    testGroupSelectionSafety();
    testDirectionMathSafety();
    testMelStringEscaping();
    testVdbComputeExceptionBoundary();
    testComputeExceptionBoundary();
    testLegacyTextParsing();
    testLocalRandomGenerator();
    testInterleavedOrder();
    testArnoldCurveLayout();
    testMeshFaceToPointSelection();
    testCurveFieldMath();
    testParticleFieldMath();
    testKillFieldMath();
    runKillFieldNumericTests();
    runBlueNoiseTests();
    testLegacySharedPointerAlias();
    testPoissonBoundaryBehavior();
    testPdbCodec();
    testPowerOfTwoAndContainerHelpers();
    testCurveGeneratorBoundaries();
    testSharedGeometryMath();
    testMvlTemplateMath();
    testUtilityVectors();
    runUtilityVectorTests();
    runAtomicOutputTests();
    runIoLocaleTests();
    testNumericBoundaryBehavior();
    testSafeVectorNormalization();
    testAttributeTemplateSafety();
    testHashGridReuse();
    testUvParameterNormalization();
    testCurveUVMapping();
    testLeafDeformationAndTileUV();
    testRigidBodyParameterValidation();
    testPluginRegistrationLifetime();
    testColorModeContract();
    return 0;
}
