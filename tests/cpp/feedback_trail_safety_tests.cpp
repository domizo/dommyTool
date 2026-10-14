#include "maya_attribute_doubles.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMotionGenerationPlan.h"
#include <maya/MTime.h>
#include <maya/MTypeId.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>

struct MPlug {
    std::string name;
    bool operator==(const MObject& object) const {
        return name == object.name;
    }
    bool operator!=(const MObject& object) const {
        return name != object.name;
    }
};

namespace {
bool failRollback = false;
int timeConversions = 0;
} // namespace

struct MDataHandle {
    bool valid = false;
    MTime time;
    double number = 0.0;
    MObject* object = nullptr;
    bool* clean = nullptr;
    std::string attribute;

    MTime asTime() const {
        assert(valid);
        ++timeConversions;
        return time;
    }
    int asInt() const {
        assert(valid && std::isfinite(number));
        assert(number >= std::numeric_limits<int>::min() &&
               number <= std::numeric_limits<int>::max());
        return static_cast<int>(number);
    }
    double asDouble() const {
        assert(valid);
        return number;
    }
    MObject data() const {
        assert(valid && object);
        return *object;
    }
    MStatus set(const MObject& value) {
        assert(valid && object);
        const MStatus status = operation("output." + attribute + ".set");
        if (status != MS::kSuccess)
            return status;
        if (failRollback && value.name == "original.out")
            return MS::kInvalidParameter;
        *object = value;
        return MS::kSuccess;
    }
    void setClean() {
        assert(valid && clean);
        *clean = true;
    }
};

struct MDataBlock {
    MTime currentTime = MTime(2.0);
    std::map<std::string, double> inputs = {
        {"seed", 1},          {"randomCount", 10},        {"trailType", 0},
        {"trailLength", 5.0}, {"trailLengthRandom", 0.0}, {"timeSampling", 10}};
    std::map<std::string, MObject> outputs = {{"out", MObject{"original.out"}},
                                              {"previous", MObject{"original.previous"}}};
    std::map<std::string, bool> clean = {{"out", false}, {"previous", false}};
    bool plugClean = false;

    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input." + attribute.name);
        MDataHandle handle;
        handle.valid = *status == MS::kSuccess;
        handle.time = currentTime;
        if (attribute.name != "time" && attribute.name != "CurrentTime")
            handle.number = inputs.at(attribute.name);
        return handle;
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status) {
        *status = operation("output." + attribute.name + ".acquire");
        MDataHandle handle;
        handle.valid = *status == MS::kSuccess;
        handle.object = &outputs.at(attribute.name);
        handle.clean = &clean.at(attribute.name);
        handle.attribute = attribute.name;
        return handle;
    }
    MStatus setClean(const MPlug&) {
        const MStatus status = operation("block.clean");
        if (status == MS::kSuccess)
            plugClean = true;
        return status;
    }
};

struct FeedbackData {
    MObject value;
};
namespace dommy {
namespace maya {
// Host-owned creation is substituted; the production feedback template and
// void/status population adapter run unchanged. This is not an ownership test.
template <typename DataType, typename Populate>
MStatus createPluginDataObject(const MTypeId& type, Populate populate, MObject& result) {
    assert(type.id() == 7U);
    const MStatus status = operation("data.create");
    if (status != MS::kSuccess)
        return status;
    DataType candidate;
    const MStatus copied = invokeStatusOperation(populate, candidate);
    if (copied != MS::kSuccess)
        return copied;
    result = candidate.value;
    return MS::kSuccess;
}
} // namespace maya
} // namespace dommy

struct TrailInputs {
    MTime time;
    int seed, randomCount, type;
    double length, randomLength;
    int samples;
};
struct DMotionTrailCurveNode : AttributeNode {
    static MObject inputObject, transformMatrix, trailType, seed, randomCount;
    static MObject trailLength, timeSampling, trailLengthRandom, time, outputObjects;
    static MStatus initialize();
    static MStatus readInputs(MDataBlock&, TrailInputs&);
};

#include "feedback_trail_methods.inc"

namespace {
MDataBlock defaults(int failure = -1) {
    resetCalls(failure);
    timeConversions = 0;
    failRollback = false;
    return MDataBlock();
}

void testTrailInitialization(const char* path) {
    std::ifstream input(path);
    assert(input.good());
    std::ostringstream expected;
    expected << input.rdbuf();
    const std::string actual =
        schemaTrace("DMotionTrailCurveNode", DMotionTrailCurveNode::initialize);
    if (actual != expected.str()) {
        std::cerr << "Motion Trail declaration trace changed:\n" << actual;
        std::abort();
    }
    assert(testFailures("DMotionTrailCurveNode", DMotionTrailCurveNode::initialize, 62) == 124);
    for (int i = 0; i < 62; ++i) {
        resetCalls(); attributeThrowAt = i;
        assert(DMotionTrailCurveNode::initialize() == MS::kFailure && callCount == i + 1);
        assert(schemaTrace("DMotionTrailCurveNode", DMotionTrailCurveNode::initialize) == actual);
    }
}

void testTrailReads() {
    MDataBlock block = defaults();
    const TrailInputs sentinel = {MTime(-99.0), -99, -99, -99, -99.0, -99.0, -99};
    TrailInputs values = sentinel;
    assert(DMotionTrailCurveNode::readInputs(block, values) == MS::kSuccess);
    const auto trace = operations;
    assert(trace == std::vector<std::string>({"input.time", "input.seed", "input.randomCount",
                                              "input.trailType", "input.trailLength",
                                              "input.trailLengthRandom", "input.timeSampling"}));
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < 7; ++failure) {
            block = defaults(failure);
            values = sentinel;
            assert(DMotionTrailCurveNode::readInputs(block, values) == status);
            assert(callCount == failure + 1);
            assert(timeConversions == (failure == 0 ? 0 : 1));
            assert(values.seed == -99 && values.time.value() == -99.0);
            block = defaults();
            assert(DMotionTrailCurveNode::readInputs(block, values) == MS::kSuccess);
            assert(operations == trace);
        }
    }
    for (MTime::Unit unit : {MTime::kFilm, MTime::kSeconds, MTime::kNTSCFrame}) {
        for (int type : {DT_TRAIL_OBJECT, DT_TRAIL_POINT, DT_TRAIL_POINT_RANDOM}) {
            block = defaults();
            block.currentTime = MTime(-2.5, unit);
            block.inputs["trailType"] = type;
            block.inputs["seed"] = 17;
            block.inputs["randomCount"] = 0;
            block.inputs["trailLength"] = 7.5;
            block.inputs["trailLengthRandom"] = 2.5;
            block.inputs["timeSampling"] = 23;
            assert(DMotionTrailCurveNode::readInputs(block, values) == MS::kSuccess);
            assert(values.time.value() == -2.5 && values.time.unit() == unit);
            assert(values.type == type && values.seed == 17 && values.randomCount == 0);
            assert(values.length == 7.5 && values.randomLength == 2.5 && values.samples == 23);
        }
    }
    for (const std::string input : {"time", "trailLength", "trailLengthRandom"}) {
        for (double value :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()}) {
            block = defaults();
            values = sentinel;
            if (input == "time")
                block.currentTime = MTime(value);
            else
                block.inputs[input] = value;
            assert(DMotionTrailCurveNode::readInputs(block, values) != MS::kSuccess);
            assert(callCount == (input == "time" ? 1 : input == "trailLength" ? 5 : 6));
            assert(values.seed == -99 && values.length == -99.0);
        }
    }
    for (int type : {-1, static_cast<int>(DT_TRAIL_TYPE_COUNT)}) {
        block = defaults();
        block.inputs["trailType"] = type;
        assert(DMotionTrailCurveNode::readInputs(block, values) == MS::kFailure);
    }
    block = defaults();
    block.inputs["randomCount"] = -1;
    assert(DMotionTrailCurveNode::readInputs(block, values) == MS::kFailure);
}

MStatus feedback(MDataBlock& block, const std::string& requested, bool voidCopy = false) {
    const FeedbackData initial = {MObject{"initial.data"}}, next = {MObject{"next.data"}};
    const auto reader = [&](const MObject& attribute, MDataBlock&) -> const FeedbackData* {
        const MStatus status = operation("data.input." + attribute.name);
        if (status != MS::kSuccess)
            return nullptr;
        assert(attribute.name == "input" || attribute.name == "next");
        return attribute.name == "input" ? &initial : &next;
    };
    if (voidCopy) {
        return dommy::maya::computeFeedbackSolver<FeedbackData>(
            MPlug{requested}, block, MObject{"input"}, MObject{"CurrentTime"}, MObject{"previous"},
            MObject{"next"}, MObject{"out"}, MTypeId(7), reader,
            [](FeedbackData& destination, const FeedbackData& source) { destination = source; });
    }
    return dommy::maya::computeFeedbackSolver<FeedbackData>(
        MPlug{requested}, block, MObject{"input"}, MObject{"CurrentTime"}, MObject{"previous"},
        MObject{"next"}, MObject{"out"}, MTypeId(7), reader,
        [](FeedbackData& destination, const FeedbackData& source) {
            const MStatus status = operation("data.copy");
            if (status == MS::kSuccess)
                destination = source;
            return status;
        });
}

void assertUnchanged(const MDataBlock& block) {
    assert(block.outputs.at("out").name == "original.out");
    assert(block.outputs.at("previous").name == "original.previous");
    assert(!block.clean.at("out") && !block.clean.at("previous") && !block.plugClean);
}

void testFeedback() {
    for (bool voidCopy : {false, true}) {
        for (MTime::Unit unit : {MTime::kFilm, MTime::kSeconds, MTime::kNTSCFrame}) {
            for (double time : {-1.0, 0.0, 1.0, 1.001, 2.0}) {
                for (const std::string requested : {"out", "previous"}) {
                    MDataBlock block = defaults();
                    block.currentTime = MTime(time, unit);
                    assert(feedback(block, requested, voidCopy) == MS::kSuccess);
                    assert(block.outputs.at(requested).name ==
                           (requested == "previous" && time <= 1.0 ? "initial.data" : "next.data"));
                    const bool updatePrevious = requested == "out" && time > 1.0;
                    if (updatePrevious)
                        assert(block.outputs.at("previous").name == "next.data");
                    assert(block.clean.at(requested) && block.plugClean && timeConversions == 1);
                    if (requested == "previous")
                        assert(block.outputs.at("out").name == "original.out");
                    else if (!updatePrevious)
                        assert(block.outputs.at("previous").name == "original.previous");
                }
            }
        }
    }
    MDataBlock block = defaults();
    assert(feedback(block, "out") == MS::kSuccess);
    const auto baseline = operations;
    assert(baseline ==
           std::vector<std::string>({"input.CurrentTime", "data.input.next", "data.create",
                                     "data.copy", "data.create", "data.copy", "output.out.acquire",
                                     "output.previous.acquire", "output.out.set",
                                     "output.previous.set", "block.clean"}));
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < 11; ++failure) {
            block = defaults(failure);
            assert(feedback(block, "out") == (failure == 1 ? MS::kFailure : status));
            assert(callCount == failure + 1 + (failure == 9 ? 1 : 0));
            if (failure < 10)
                assertUnchanged(block);
            else {
                assert(!block.plugClean && block.clean.at("out") && block.clean.at("previous"));
                assert(block.outputs.at("out").name == "next.data");
            }
            assert(timeConversions == (failure == 0 ? 0 : 1));
            block = defaults();
            assert(feedback(block, "out") == MS::kSuccess);
            assert(operations == baseline);
        }
        for (bool voidCopy : {false, true}) {
            block = defaults(0);
            assert(feedback(block, "previous", voidCopy) == status);
            assert(timeConversions == 0 && callCount == 1);
            assertUnchanged(block);
        }
    }
    injectedStatus = MS::kFailure;
    block = defaults(9);
    failRollback = true;
    assert(feedback(block, "out") == MS::kInvalidParameter);
    assert(block.outputs.at("out").name == "next.data");
    assert(block.outputs.at("previous").name == "original.previous");
    assert(!block.clean.at("out") && !block.clean.at("previous") && !block.plugClean);

    for (const std::string requested : {"out", "previous"}) {
        for (double invalid :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()}) {
            block = defaults();
            block.currentTime = MTime(invalid);
            assert(feedback(block, requested) == MS::kInvalidParameter);
            assert(callCount == 1);
            assertUnchanged(block);
        }
    }
    block = defaults();
    assert(feedback(block, "unrelated") == MS::kUnknownParameter);
    assert(callCount == 0);
    assertUnchanged(block);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testTrailInitialization(argv[1]);
    testTrailReads();
    testFeedback();
    std::cout << "Feedback/Trail: 124 initializer failures + 62 exceptions + 14 Trail input + 26 feedback failures, "
                 "stable schema, feedback routing/recovery/rollback: OK\n";
}
