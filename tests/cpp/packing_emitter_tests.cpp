#include "mesh_emitter_doubles.h"
#include "noise/Poisson/PoissonGenerator.h"
#include "geometry/DBlueNoiseSampler.h"
#include "math/DRandomGenerator.h"

int randomConstructions = 0;
namespace PoissonGenerator {
struct CountingPRNG : DefaultPRNG {
    explicit CountingPRNG(uint32_t seed) : DefaultPRNG(seed) { ++randomConstructions; }
};
}
namespace dommy { namespace math {
struct CountingRandomGenerator : RandomGenerator {
    explicit CountingRandomGenerator(unsigned seed) : RandomGenerator(seed) { ++randomConstructions; }
};
} }

// Extra attribute overloads used by these historical initializers only.
struct PackingNumericAttribute : MFnNumericAttribute {
    using MFnNumericAttribute::setDefault;
    using MFnNumericAttribute::setMin;
    MStatus setDefault(double x, double y, double z, double w) {
        return operation(name + " default " + text(x) + " " + text(y) + " " + text(z) + " " +
                         text(w));
    }
    MStatus setMin(double x, double y) {
        return operation(name + " min " + text(x) + " " + text(y));
    }
    MObject createColor(const char* a, const char* b, MStatus* s = nullptr) {
        return bind(a, b, "color", s);
    }
};
struct MFnPointArrayData {
    MObject create(const MPointArray& points, MStatus* s) {
        *s = operation("pointArray default " + text(points.size()));
        return *s ? MObject{"pointArray"} : MObject::kNullObj;
    }
};
enum { D_X_AXIS = 0, D_Y_AXIS, D_Z_AXIS };
struct PackingHandle : MDataHandle {
    PackingHandle(MDataHandle h) : MDataHandle(h) {}
    long long asInt64() const {
        check();
        return static_cast<long long>(input->number);
    }
};
struct PackingBlock : EmitterBlock {
    PackingHandle inputValue(MObject attr, MStatus* s) {
        return EmitterBlock::inputValue(attr, s);
    }
};
#include "packing_emitter.inc"
#include "../fixtures/packing_emitter_sampling_baseline.inc"

std::string schema() {
    std::string result;
    for (const auto& node : initializers)
        result += schemaTrace(node.first, node.second);
    return result;
}
std::string outputContract(const std::vector<MStatus (*)(size_t, DPointList&)>& samples =
                              {poisson, blueNoise}) {
    std::ostringstream result;
    result << std::fixed << std::setprecision(9);
    unsigned mode = 0;
    for (auto sample : samples) {
        for (size_t count : {size_t(1), size_t(20)}) {
            inputs.clear();
            inputs["distance"].number = .2f;
            reset();
            DPointList points;
            assert(sample(count, points) == MS::kSuccess);
            result << mode << ' ' << count << ' ' << points.size() << '\n';
            for (const auto& p : points)
                result << p.x << ' ' << p.y << ' ' << p.z << '\n';
        }
        ++mode;
    }
    return result.str();
}
template <class Node> void distance() {
    Node node;
    const PackedCircle candidate(MPoint(0, 0, 0), 1);
    node.placedPoints = {PackedCircle(MPoint(2, 0, 0), 1), PackedCircle(MPoint(10, 0, 0), 1)};
    assert(node.getDistanceToNearestSample(candidate) == 0);
    std::reverse(node.placedPoints.begin(), node.placedPoints.end());
    assert(node.getDistanceToNearestSample(candidate) == 0);
    node.placedPoints[1] = PackedCircle(MPoint(1, 0, 0), 1);
    assert(node.getDistanceToNearestSample(candidate) == -1);
    std::reverse(node.placedPoints.begin(), node.placedPoints.end());
    assert(node.getDistanceToNearestSample(candidate) == -1);
    node.placedPoints[0] = PackedCircle(MPoint(4, 0, 0), 1);
    assert(node.getDistanceToNearestSample(candidate) == 2);
    node.placedPoints.push_back(PackedCircle(MPoint(NAN, 0, 0), 1));
    assert(!std::isfinite(node.getDistanceToNearestSample(candidate)));
}
void sampling() {
    for (auto sample : {&poisson, &blueNoise}) {
        for (float d : {-1.0f, -0.5f, 0.2f}) {
            for (size_t count : {size_t(0), size_t(1), size_t(20)}) {
                inputs.clear();
                inputs["distance"].number = d;
                reset();
                DPointList points;
                randomConstructions = 0;
                assert(sample(count, points) == MS::kSuccess);
                assert(randomConstructions == (count ? 1 : 0));
                assert(count ? !points.empty() : points.empty());
                for (const auto& p : points)
                    assert(std::isfinite(p.x) && std::isfinite(p.z));
                DPointList repeat;
                assert(sample(count, repeat) == MS::kSuccess && repeat.size() == points.size());
                for (size_t i = 0; i < points.size(); ++i)
                    assert(points[i].x == repeat[i].x && points[i].z == repeat[i].z);
            }
        }
        for (float d : {-2.0f, std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity()}) {
            inputs["distance"].number = d;
            reset();
            DPointList points;
            assert(sample(20, points) != MS::kSuccess && points.empty());
        }
    }
    for (float d : {0.0f, std::numeric_limits<float>::denorm_min(), 1e-12f}) {
        inputs["distance"].number = d;
        reset();
        DPointList points;
        assert(poisson(20, points) != MS::kSuccess && points.empty());
    }
    inputs["distance"].number = -1;
    reset();
    DPointList points;
    assert(poisson(10000000, points) != MS::kSuccess && points.empty());
    inputs["distance"].number = 0;
    reset();
    assert(blueNoise(20, points) == MS::kSuccess && points.size() == 20);
    assert(dommy::packing::isBoundedPoissonRequest(20, -1));
    assert(!dommy::packing::isBoundedPoissonRequest(10000001, -1));
}
int inputFailures() {
    int count = 0;
    for (auto read : readers) {
        inputs.clear();
        inputs["input"].number = 1;
        reset();
        assert(read() == MS::kSuccess && callCount == 1);
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(0);
            injectedStatus = status;
            assert(read() == status && callCount == 1);
            ++count;
        }
        reset();
        throwAt = 0;
        assert(read() != MS::kSuccess && callCount == 1);
        ++count;
    }
    return count;
}
int outputFailures() {
    int count = 0;
    const std::vector<std::array<double, 3>> previous{{{11, 22, 33}}};
    for (auto write : writers) {
        reset();
        shortPointStorage = false;
        assert(write() == MS::kSuccess && published == 1 && pointOutput.points.size() == 2);
        const auto trace = operations;
        for (size_t i = 0; i < trace.size(); ++i) {
            for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                reset(i);
                injectedStatus = status;
                pointOutput.points = previous;
                const auto result = write();
                assert(result != MS::kSuccess && callCount == int(i) + 1);
                if (trace[i] != "point.set" && trace[i] != "point.bounds")
                    assert(result == status);
                assert(published == (trace[i] == "clean" ? 1 : 0));
                if (!published)
                    assert(pointOutput.points == previous);
                ++count;
            }
            reset();
            throwAt = i;
            pointOutput.points = previous;
            assert(write() != MS::kSuccess && published == (trace[i] == "clean" ? 1 : 0));
            if (!published)
                assert(pointOutput.points == previous);
            ++count;
        }
        reset();
        shortPointStorage = true;
        pointOutput.points = previous;
        assert(write() != MS::kSuccess && !published && pointOutput.points == previous);
        shortPointStorage = false;
        ++count;
    }
    return count;
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--dump-schema") {
        std::cout << schema();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--dump-output") {
        std::cout << outputContract();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--distance") {
        distance<DPointPlaneEmit>();
        distance<DPScatter>();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--sampling") {
        sampling();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--inputs") {
        inputFailures();
        return 0;
    }
    assert(argc == 2);
    std::ifstream file(argv[1]);
    assert(file);
    assert(schema() == std::string(std::istreambuf_iterator<char>(file), {}));
    // Poisson is unchanged. Blue Noise has its own independent quality/count
    // tests rather than a coordinate/order contract with removed vendor code.
    assert(outputContract({poisson}) == outputContract({referencePoisson}));
    int failures = 0;
    for (const auto& node : initializers) {
        schemaTrace(node.first, node.second);
        failures += testFailures(node.first, node.second, callCount);
    }
    distance<DPointPlaneEmit>();
    distance<DPScatter>();
    sampling();
    std::cout << "Packing emitters: " << failures << " initializer failures, " << inputFailures()
              << " input failures, " << outputFailures()
              << " output failures, distance and sampling checks OK\n";
}
