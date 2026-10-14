#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <maya/MMatrix.h>
#include <maya/MTime.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

bool operator!(const MStatus& status) {
    return status != MS::kSuccess;
}

namespace {
bool connected = true;
std::vector<MTime> sampledTimes;
} // namespace

struct MPlug {
    std::string name;
    bool operator!=(const MObject& object) const {
        return name != object.name;
    }
    bool isNull() const {
        return name.empty();
    }
    bool isConnected(MStatus* status) const {
        *status = operation("matrix.connected");
        return connected;
    }
};

struct MFnDependencyNode {
    MFnDependencyNode(const MObject&, MStatus* status) {
        *status = operation("node.bind");
    }
    MPlug findPlug(const MObject& attribute, bool networked, MStatus* status) const {
        assert(networked);
        *status = operation("node.findPlug");
        return {attribute.name};
    }
};

struct MDataHandle {
    bool valid;
    MTime time;
    double number;
    MMatrix* matrix;
    MTime asTime() const {
        assert(valid); // Fails if production reads a failed input handle.
        return time;
    }
    double asDouble() const {
        assert(valid);
        return number;
    }
    void setMMatrix(const MMatrix& value) {
        assert(valid && matrix);
        *matrix = value;
    }
};

struct MDataBlock;
struct MArrayDataBuilder {
    MMatrix staged;
    bool valid;
    MArrayDataBuilder(MDataBlock*, const MObject& attribute, unsigned int count, MStatus* status) {
        assert(attribute.name == "outputTransformMatrix" && count == 1U);
        *status = operation("output.builder");
        valid = *status == MS::kSuccess;
    }
    MDataHandle addElement(unsigned int index, MStatus* status) {
        assert(valid && index == 0U);
        *status = operation("output.addElement");
        return {*status == MS::kSuccess, MTime(), 0.0, &staged};
    }
};

struct MArrayDataHandle {
    MDataBlock* block;
    bool valid;
    MStatus set(const MArrayDataBuilder& builder);
    MStatus setAllClean();
};

struct MDataBlock {
    MTime evaluationTime = MTime(20.0);
    double delay = 5.0;
    MMatrix output;
    int publications = 0;
    bool arrayClean = false, plugClean = false;

    MDataBlock() {
        output[3][0] = -999.0;
    }
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        assert(attribute.name == "time" || attribute.name == "delay");
        *status = operation("input." + attribute.name);
        return {*status == MS::kSuccess, evaluationTime, delay, nullptr};
    }
    MArrayDataHandle outputArrayValue(const MObject& attribute, MStatus* status) {
        assert(attribute.name == "outputTransformMatrix");
        *status = operation("output.array");
        return {this, *status == MS::kSuccess};
    }
    MStatus setClean(const MPlug&) {
        const MStatus status = operation("output.plugClean");
        if (status == MS::kSuccess)
            plugClean = true;
        return status;
    }
};

MStatus MArrayDataHandle::set(const MArrayDataBuilder& builder) {
    assert(valid && builder.valid);
    const MStatus status = operation("output.set");
    if (status == MS::kSuccess) {
        block->output = builder.staged;
        ++block->publications;
    }
    return status;
}
MStatus MArrayDataHandle::setAllClean() {
    assert(valid);
    const MStatus status = operation("output.arrayClean");
    if (status == MS::kSuccess)
        block->arrayClean = true;
    return status;
}

namespace dommy {
namespace maya {
// Substitute only the host query; execute the production time/series helpers.
MStatus sampleMatrixPlugAtTime(const MPlug& plug, const MTime& time, MMatrix& matrix) {
    assert(plug.name == "transformMatrix" && std::isfinite(time.value()));
    const MStatus status = operation("matrix.sample");
    if (status == MS::kSuccess) {
        sampledTimes.push_back(time);
        matrix[3][0] = time.value();
    }
    return status;
}
} // namespace maya
} // namespace dommy

struct DTDelayTransformMatrix : AttributeNode {
    static MObject transformMatrix, delay, time, outputTransformMatrix;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
    MObject thisMObject() const {
        return {"node"};
    }
};

#include "delay_transform_methods.inc"

namespace {
void testInitialization(const char* fixture) {
    std::ifstream input(fixture);
    assert(input.good());
    std::ostringstream expected;
    expected << input.rdbuf();
    const std::string baseline =
        schemaTrace("DTDelayTransformMatrix", DTDelayTransformMatrix::initialize);
    if (baseline != expected.str()) {
        std::cerr << "Delay initializer declaration trace changed:\n" << baseline;
        std::abort();
    }
    assert(testFailures("DTDelayTransformMatrix", DTDelayTransformMatrix::initialize, 24) == 48);
}

MDataBlock defaults(int failure = -1) {
    resetCalls(failure);
    connected = true;
    sampledTimes.clear();
    return MDataBlock();
}

MStatus compute(MDataBlock& block) {
    DTDelayTransformMatrix node;
    return node.compute(MPlug{"outputTransformMatrix"}, block);
}

void testComputeFailures() {
    MDataBlock block = defaults();
    assert(compute(block) == MS::kSuccess);
    const std::vector<std::string> baseline = operations;
    const std::vector<std::string> expected = {
        "input.time",        "input.delay",   "node.bind",         "node.findPlug",
        "matrix.connected",  "matrix.sample", "output.array",      "output.builder",
        "output.addElement", "output.set",    "output.arrayClean", "output.plugClean"};
    assert(baseline == expected);
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < static_cast<int>(baseline.size()); ++failure) {
            block = defaults(failure);
            assert(compute(block) == status);
            assert(callCount == failure + 1);
            assert(!block.plugClean);
            assert(block.arrayClean == (failure > 10));
            // Publication is committed at set(builder). A later clean failure
            // must propagate, but cannot undo that already successful host call.
            const bool published = failure > 9;
            assert(block.publications == (published ? 1 : 0));
            assert(block.output[3][0] == (published ? 15.0 : -999.0));
            block = defaults();
            assert(compute(block) == MS::kSuccess);
            assert(operations == baseline);
        }
    }
}

void testTimeAndDelay() {
    for (MTime::Unit unit : {MTime::kFilm, MTime::kSeconds, MTime::kNTSCFrame}) {
        for (double delay : {-5.0, -0.0, 0.0, 5.0}) {
            MDataBlock block = defaults();
            block.evaluationTime = MTime(20.0, unit);
            block.delay = delay;
            assert(compute(block) == MS::kSuccess);
            assert(sampledTimes.size() == 1U);
            assert(sampledTimes[0].value() == 20.0 - delay);
            assert(sampledTimes[0].unit() == unit);
            assert(block.output[3][0] == 20.0 - delay);
            assert(block.publications == 1 && block.arrayClean && block.plugClean);
        }
    }
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (double invalid : {infinity, -infinity, nan}) {
        for (bool timeInput : {false, true}) {
            MDataBlock block = defaults();
            if (timeInput)
                block.evaluationTime = MTime(invalid);
            else
                block.delay = invalid;
            assert(compute(block) != MS::kSuccess);
            assert(callCount == (timeInput ? 1 : 2));
            assert(sampledTimes.empty() && block.publications == 0 && !block.plugClean);
        }
    }
    MDataBlock block = defaults();
    block.evaluationTime = MTime(std::numeric_limits<double>::max());
    block.delay = -std::numeric_limits<double>::max();
    assert(compute(block) != MS::kSuccess);
    assert(sampledTimes.empty() && block.publications == 0);

    block = defaults();
    connected = false;
    assert(compute(block) == MS::kFailure);
    assert(sampledTimes.empty() && block.publications == 0);

    block = defaults();
    DTDelayTransformMatrix node;
    assert(node.compute(MPlug{"unrelated"}, block) == MS::kUnknownParameter);
    assert(callCount == 0 && block.publications == 0);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testInitialization(argv[1]);
    testComputeFailures();
    testTimeAndDelay();
    std::cout << "Delay transform: 48 initialization + 24 compute failures, stable schema, "
                 "signed delay/time units and invalid-input rejection: OK\n";
}
