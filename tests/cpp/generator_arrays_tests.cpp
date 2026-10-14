#include "common/utility/DStatusOperation.h"
#include <cassert>
#include <iostream>
#include <vector>

static MStatus result = MS::kSuccess;
static int calls = 0, successes = 0;
struct Value {
    double x = 0, y = 0, z = 0, w = 1;
    Value() = default;
    Value(double, double, double) {}
};
using MPoint = Value;
using MVector = Value;
namespace MSpace { enum Space { kObject, kWorld }; }
struct Target {
    template <typename... T> MStatus append(T...) { ++calls; return result; }
    template <typename... T> MStatus setLength(T...) { ++calls; return result; }
    template <typename... T> MStatus copy(T...) { ++calls; return result; }
    template <typename... T> MStatus addRotationQuaternion(T...) { ++calls; return result; }
    template <typename... T> MStatus addTranslation(T...) { ++calls; return result; }
    template <typename... T> MStatus addRotation(T...) { ++calls; return result; }
    Value asMatrix() const { return {}; }
};
static Target basdeCvs, rootBendRatioArray, tipBendRatioArray, lengthRatioArray,
    sufaceOffsetRatioArray, globalBendRatioArray, rootBendPerWeightArray,
    tipBendPerWeightArray, lenRandValueArray, rndPosArray, randTransX,
    randTransY, randTransZ, randRotX, randRotY, randRotZ, transMatArray,
    baseRotMatArray, alignMat, rotMat, transMat, storage;
static Target *dupPointArray = &storage, *tangentArray = &storage,
    *normalArray = &storage, *cvs = &storage;
static int segment = 2, lashCount = 2, order = 0;
static double yVal = 1, baseRot[3] = {};
static Value xQuat, yQuat, rootPt;
namespace matrixScenario {
using MPoint = double;
static int at = 0, failAt = -1, indexed = 0;
struct Transform { double asMatrix() const { return 1; } };
struct MatrixList {
    std::vector<double> values;
    MStatus append(double value) {
        if (at++ == failAt) return MS::kFailure;
        values.push_back(value); return MS::kSuccess;
    }
    double operator[](unsigned index) const { ++indexed; return values.at(index); }
};
}
#include "generator_arrays.inc"

int main() {
    for (auto probe : arrayProbes)
        for (const auto status : {MS::kSuccess, MS::kFailure, MS::kInvalidParameter}) {
            calls = successes = 0; result = status;
            assert(probe() == status && calls == 1);
            assert(successes == (status == MS::kSuccess ? 1 : 0));
        }
    for (int index = -1; index < 4; ++index) {
        matrixScenario::at = matrixScenario::indexed = 0;
        matrixScenario::failAt = index;
        const MStatus status = matrixScenario::execute();
        assert(status == (index < 0 ? MS::kSuccess : MS::kFailure));
        assert(matrixScenario::indexed == (index < 0 ? 4 : 0));
    }
    std::cout << "Eyelash: " << sizeof(arrayProbes) / sizeof(arrayProbes[0])
              << " exact array/transform status boundaries passed\n";
}
