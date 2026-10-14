#include "motion_rotation_doubles.h"
#include "common/utility/DRandomValueGeneration.h"

using DScalarDataList = std::vector<float>;
using DVec3Data = Imath::V3f;
using DVec3DataList = std::vector<DVec3Data>;

bool getCheckedMVectorArrayFromAttribute(const DPData&, const std::string&, std::size_t,
                                         MVectorArray&) {
    return false;
}
bool getCheckedDScalarArrayFromAttribute(const DPData&, const std::string&, std::size_t,
                                         DScalarDataList&) {
    return false;
}
bool getCheckedMMatrixArrayFromAttribute(const DPData& data, const std::string& name,
                                         std::size_t count, MMatrixArray& output) {
    const auto found = data.matrices.find(name);
    if (found == data.matrices.end() || found->second.size() != count)
        return false;
    output.values = found->second;
    return true;
}
int matrixObjectReads = 0, matrixPlugReads = 0;
MStatus getMatrixArrayFromData(const MObject&, MMatrixArray& output) {
    ++matrixObjectReads;
    output.values.resize(2);
    output.values[0][3][0] = 12;
    output.values[1][3][1] = 34;
    return MS::kSuccess;
}
MStatus getMatrixArrayFromData(const MObject& object, const MObject&, MMatrixArray& output) {
    ++matrixPlugReads;
    const MStatus status = getMatrixArrayFromData(object, output);
    --matrixObjectReads;
    return status;
}
MVector DPToMV(const DPoint& p) {
    return {p.x, p.y, p.z};
}
#include "transform_methods.inc"

namespace {
std::string trace(const std::vector<MMatrix>& matrices) {
    std::ostringstream stream;
    stream << std::setprecision(17);
    for (const auto& m : matrices) {
        for (const auto& row : m.values)
            for (double v : row)
                stream << v << ' ';
        stream << '\n';
    }
    return stream.str();
}
DTransformParams parameters(short mode) {
    DTransformParams p;
    p.mAnimMatrixType = mode;
    p.mAnimationAttribute = "animation";
    p.mLocalTranslate = {1, 2, 3};
    p.mGlobalTranslate = {4, 5, 6};
    p.mLocalRot = {13, 27, -8};
    p.mGlobalRot = {-7, 4, 20};
    p.mGlobalScale = {2, 3, 4};
    p.mUniformScale = 1.25;
    p.mNormalVec = {0, 0, 1};
    p.mTangentVec = {0, 1, 0};
    p.mLocalTransRndMin = {-1, -2, -3};
    p.mLocalTransRndMax = {2, 4, 6};
    p.mGlobalRotRndMin = {-20, -10, -5};
    p.mGlobalRotRndMax = {10, 5, 30};
    return p;
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    if (std::string(argv[1]) == "--budget") {
        DPData data;
        DTransformParams p;
        std::vector<MMatrix> result(1);
        result[0][3][0] = 123;
        const auto before = trace(result);
        data.points.resize(12, DPoint(1, 2, 3));
        resetRun();
        assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kInvalidParameter);
        assert(trace(result) == before && callCount == 0 && matrixCopies == 0);
        data.points.resize(3);
        resetRun();
        assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kSuccess);
        result.reserve(40);
        const auto retained = trace(result);
        resetRun();
        assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kInvalidParameter);
        assert(trace(result) == retained && callCount == 0 && matrixCopies == 0);
        data.points.clear();
        resetRun();
        assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kSuccess && result.empty());
        std::cout << "Transform aggregate admission, retained result and empty recovery passed.\n";
        return 0;
    }
    const bool emitTrace = std::string(argv[1]) == "--trace";
    std::string normals;
    int tested = 0;
    for (short mode = MAT_ANIM_NONE; mode < MAT_ANIM_TYPE_COUNT; ++mode) {
        DPData data;
        data.matrices["animation"] = std::vector<MMatrix>(3);
        data.matrices["animation"][0][3][2] = 9;
        data.matrices["animation"][1][3][0] = 8;
        const DTransformParams p = parameters(mode);
        std::vector<MMatrix> result;
        resetRun();
        assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kSuccess);
        const auto baseline = operations;
        assert(baseline.size() == 21 && workerCopies == 0);
        const std::string expected = trace(result);
        normals += "mode " + std::to_string(mode) + "\n" + expected;
        if (emitTrace)
            continue;
        for (auto code : {MS::kFailure, MS::kInvalidParameter})
            for (int i = 0; i < 21; ++i) {
                resetRun(i);
                injectedStatus = code;
                result.resize(2);
                result[0][3][0] = 123;
                result[1][3][1] = 456;
                const auto before = trace(result);
                assert(DGenerateTransformMatrixFromDP(&data, p, result) == code);
                assert(trace(result) == before && outOfBounds == 0);
                // Remaining points may run, but this failed point must stop.
                const int operationsSkipped = 6 - i % 7;
                assert(callCount == 21 - operationsSkipped);
                resetRun();
                assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kSuccess);
                assert(trace(result) == expected);
                ++tested;
            }
    }
    if (emitTrace) {
        std::cout << normals;
        return 0;
    }
    std::ifstream fixture(argv[1]);
    assert(fixture);
    const std::string expected = {std::istreambuf_iterator<char>(fixture), {}};
    assert(normals == expected);
    {
        DPData input;
        DTransformParams snapshotParams = parameters(MAT_ANIM_INPUT_DATA);
        std::vector<MMatrix> plugResult, snapshotResult;
        resetRun(); matrixObjectReads = matrixPlugReads = 0;
        assert(DGenerateTransformMatrixFromDP(&input, snapshotParams, plugResult) == MS::kSuccess);
        assert(matrixPlugReads == 1 && matrixObjectReads == 0);
        snapshotParams.mAnimationInputIsData = true;
        resetRun(); matrixObjectReads = matrixPlugReads = 0;
        assert(DGenerateTransformMatrixFromDP(&input, snapshotParams, snapshotResult) == MS::kSuccess);
        assert(matrixPlugReads == 0 && matrixObjectReads == 1);
        assert(trace(plugResult) == trace(snapshotResult));
    }
    DPData data;
    DTransformParams p;
    std::vector<MMatrix> result(1);
    result[0][3][0] = 123;
    const auto before = trace(result);
    assert(DGenerateTransformMatrixFromDP(nullptr, p, result) == MS::kInvalidParameter);
    data.valid = false;
    assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kInvalidParameter);
    data.valid = true;
    data.fakeCount = dommy::generation::kMaxGeneratedTransformCount + 1;
    assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kInvalidParameter);
    data.fakeCount = 4;
    assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kFailure);
    data.fakeCount = 0;
    p.mGlobalScale.x = std::numeric_limits<double>::infinity();
    assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kInvalidParameter);
    assert(trace(result) == before);
    p = DTransformParams();
    data.points.clear();
    resetRun();
    assert(DGenerateTransformMatrixFromDP(&data, p, result) == MS::kSuccess);
    assert(result.empty() && callCount == 0);
    std::cout << "Transform generator: " << tested
              << " API failures, recovery, three legacy "
                 "normal traces and input/output boundaries passed.\n";
}
