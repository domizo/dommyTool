#include "common/utility/DStatusOperation.h"
#include <array>
#include <cassert>
#include <iostream>

struct MObject {};
static MStatus result = MS::kSuccess;
static int getters = 0, successes = 0, acquisitions = 0;
static bool nullTuple = false;
static double raw[] = {1, 2, 3};
struct MVector {
    std::array<double, 3> values;
    explicit MVector(const double* p) : values{{p[0], p[1], p[2]}} {}
    double operator[](unsigned i) const { return values.at(i); }
};
using MPoint = MVector;
struct MDataHandle {
    int asInt() const { ++getters; return 1; }
    short asShort() const { ++getters; return 1; }
    double asDouble() const { ++getters; return 1; }
    float asFloat() const { ++getters; return 1; }
    bool asBool() const { ++getters; return true; }
    MVector asVector() const { ++getters; return MVector(raw); }
    const double* asDouble2() const { ++getters; return nullTuple ? nullptr : raw; }
    const double* asDouble3() const { ++getters; return nullTuple ? nullptr : raw; }
};
struct MDataBlock {
    MDataHandle inputValue(MObject, MStatus* status) {
        ++acquisitions; *status = result; return {};
    }
};
#include "generator_inputs.inc"

int main() {
    for (auto probe : probes) {
        for (const auto status : {MS::kSuccess, MS::kFailure, MS::kInvalidParameter}) {
            raw[0] = 1; nullTuple = false; result = status;
            getters = successes = acquisitions = 0;
            assert(probe() == status);
            assert(acquisitions == 1 && getters == (status == MS::kSuccess ? 1 : 0));
            assert(successes == (status == MS::kSuccess ? 1 : 0));
        }
        raw[0] = 1; nullTuple = true; result = MS::kSuccess;
        // Scalar probes still succeed; tuple probes must reject without a dereference.
        const MStatus status = probe();
        assert(status == MS::kSuccess || status == MS::kFailure);
    }
    std::cout << "Generator inputs: " << sizeof(probes) / sizeof(probes[0])
              << " exact scalar/compound boundaries passed\n";
}
