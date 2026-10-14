#include "vdb_smoothing_doubles.h"
#include "math/DBounds.h"
#include "vdb_smoothing_compute.inc"
#include <random>
#include <cfloat>

void reset() {
    resetCalls();
    injectedStatus = MS::kFailure;
    copies = publications = queries = 0;
    nullCreated = badNeighbor = nullTransform = nullGrid = false;
    published = std::make_shared<DPData>();
}
bool called(const char* name) {
    return std::find(operations.begin(), operations.end(), name) != operations.end();
}
MDataBlock block(const Payload& source) {
    MDataBlock data;
    data.inputs["inDD"].payload = &source;
    data.inputs["inDP"].payload = &source;
    data.inputs["aSmoothAttribute"].text = "weight";
    data.inputs["aPointRadius"].number = .001;
    data.inputs["aSearchRadius"].number = 1;
    data.inputs["aSearchGridSize"].number = .1;
    data.inputs["aIterate"].number = 1;
    data.inputs["aMaxSampleCount"].number = 10;
    data.inputs["aVoxelSize"].number = 1;
    return data;
}
MStatus smooth(MDataBlock& data) {
    return DPAttrSmooth().compute(MPlug(DPAttrSmooth::outDD), data);
}
MStatus sample(MDataBlock& data) {
    return DOpenVDBSamplingDP().compute(MPlug(DOpenVDBSamplingDP::aVdbOutput), data);
}
void samplingInputCases() {
    DPData source; source.points = {{0, 0, 0}};
    for (int index : {0, 1})
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset(); auto data = block(source); failAt = index; injectedStatus = code;
            assert(sample(data) == code && callCount == index + 1 && !publications);
            assert(!called("transform.create"));
        }
    source.points[0].x = NAN;
    reset(); auto data = block(source);
    assert(sample(data) == MS::kInvalidParameter && !publications && !called("transform.create"));
    source.points[0].x = 0; reset(); data = block(source);
    assert(sample(data) == MS::kSuccess && publications == 1);
}
template <typename T> T value(int scalar) {
    using C = dommy::attribute::SmoothingComponents<T>;
    T result = C::zero();
    for (std::size_t axis = 0U; axis < C::count; ++axis)
        C::at(result, axis) = scalar;
    return result;
}
template <typename T, typename Owner> void setup(Owner& data) {
    data.points = {{-1.75f, 0, 0}, {0, 1, 0}, {1, 0, 0}};
    data.attributes["weight"] = std::make_shared<TypedDPAttributeData<T>>(
        std::vector<T>{value<T>(0), value<T>(10), value<T>(20)}, value<T>(7));
    auto group = std::make_shared<Group>();
    group->ids = {true, false, true};
    data.groups["keep"] = group;
}
template <typename T, typename Owner> void checkValues(const Owner& result, int iterations = 1) {
    using C = dommy::attribute::SmoothingComponents<T>;
    const auto field =
        std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(result.getAttr("weight"));
    assert(field && field->value().size() == 3);
    for (std::size_t axis = 0U; axis < C::count; ++axis) {
        assert(C::at(field->getDefaultValue(), axis) == (iterations == 0 ? 7 : 0));
        assert(C::at(field->value()[0], axis) == (iterations == 0 ? 0 : iterations == 1 ? 15 : 7));
        assert(C::at(field->value()[1], axis) == 10);
    }
    assert(result.groups.at("keep")->ids == std::vector<bool>({true, false, true}));
    assert(result.points[0][0] == -1.75f);
}
template <typename T> void testType() {
    reset();
    DPData points;
    setup<T>(points);
    auto data = block(points);
    assert(smooth(data) && publications == 1 && queries == 1);
    checkValues<T>(*std::dynamic_pointer_cast<DPData>(published));
    checkValues<T>(points, 0);
    reset();
    DCurveData curves;
    for (int i = 0; i < 2; ++i) {
        auto p = std::make_shared<DCurveInternalData>();
        setup<T>(*p);
        curves.primitives.push_back(p);
    }
    data = block(curves);
    assert(smooth(data) && queries == 2);
    auto curveResult = std::dynamic_pointer_cast<DCurveData>(published);
    for (const auto& p : curveResult->primitives)
        checkValues<T>(*p);
    for (const auto& p : curves.primitives)
        checkValues<T>(*p, 0);
    reset();
    DMeshData meshes;
    auto p = std::make_shared<DMeshInternalData>();
    setup<T>(*p);
    meshes.primitives.push_back(p);
    data = block(meshes);
    assert(smooth(data));
    checkValues<T>(*std::dynamic_pointer_cast<DMeshData>(published)->primitives[0]);
    checkValues<T>(*p, 0);
}
template <typename T> void checkMean(std::initializer_list<T> values, T expected) {
    dommy::attribute::SmoothingMean<T> mean(static_cast<uint32_t>(values.size()));
    for (T v : values)
        assert(mean.add(v));
    T result = 0;
    assert(mean.result(result) && result == expected);
}
void mathTests() {
    const int64_t high = INT64_MAX, low = INT64_MIN;
    checkMean<int64_t>({high, high}, high);
    checkMean<int64_t>({low, low}, low);
    checkMean<int64_t>({low, high}, 0);
    checkMean<int64_t>({high, high, low}, 3074457345618258602LL);
    checkMean<int64_t>({low, low, high}, -3074457345618258603LL);
    checkMean<int64_t>({9007199254740993LL, 9007199254740993LL}, 9007199254740993LL);
    checkMean<int32_t>({INT32_MAX, INT32_MAX}, INT32_MAX);
    checkMean<int32_t>({INT32_MIN, INT32_MIN}, INT32_MIN);
    checkMean<uint32_t>({UINT32_MAX, UINT32_MAX}, UINT32_MAX);
    checkMean<int32_t>({-2, -1}, -1);
    checkMean<int32_t>({-2, 1}, 0);
    checkMean<float>({FLT_MAX, FLT_MAX}, FLT_MAX);
    checkMean<double>({DBL_MAX, DBL_MAX}, DBL_MAX);
    checkMean<double>({DBL_MAX, 3, -DBL_MAX}, 1);
    std::mt19937 random(817);
    for (int pass = 0; pass < 5000; ++pass) {
        dommy::attribute::SmoothingMean<int64_t> mean(7);
        int64_t sum = 0;
        for (int i = 0; i < 7; ++i) {
            const int64_t v = int64_t(random()) - 2147483648LL;
            sum += v;
            assert(mean.add(v));
        }
        int64_t result;
        assert(mean.result(result) && result == sum / 7);
    }
    for (double invalid : {double(NAN), double(INFINITY)}) {
        dommy::attribute::SmoothingMean<double> mean(1);
        assert(!mean.add(invalid));
    }
    std::vector<double> previous{999};
    assert(!dommy::attribute::smoothNeighborValues(std::vector<double>{1, 2},
                                                   IDsListArray{{1}, {99}}, 1, previous));
    assert(previous == std::vector<double>{999});
    assert(dommy::attribute::smoothNeighborValues(std::vector<double>{1, 2}, IDsListArray{{}, {}},
                                                  1, previous));
    assert(previous == std::vector<double>({1, 2}));
    std::size_t total = 0U, retained = 0U;
    assert(dommy::attribute::addSmoothingWork(6000, 1, 1, 1, total, retained));
    assert(dommy::attribute::addSmoothingWork(6000, 1, 1, 1, total, retained));
    const auto old = total;
    assert(!dommy::attribute::addSmoothingWork(6000, 1, 1, 1, total, retained) && total == old);
    total = 0;
    assert(!dommy::attribute::addSmoothingWork(3, 2, INT32_MAX, 1, total, retained));
    dommy::point::PointPayloadBudget memory;
    memory.add(64U * 1024U * 1024U, 1U);
    bool rejected = false;
    try {
        memory.add(1U, 1U);
    } catch (const std::length_error&) {
        rejected = true;
    }
    assert(rejected);
    DPData source;
    setup<int32_t>(source);
    source.attributes["text"] = std::make_shared<TypedDPAttributeData<std::string>>(
        std::vector<std::string>(3), std::string(200, 'x'));
    dommy::point::PointPayloadBudget metadata;
    metadata.add(64U * 1024U * 1024U - 200U, 1U);
    SmoothingRequest request{"missing", 0, 0, 1, 0, 1};
    rejected = false;
    try {
        preflightSmoothingPartition(source, request, metadata, total);
    } catch (const std::length_error&) {
        rejected = true;
    }
    assert(rejected);
}
void cornerBudgetTest() {
    DMeshInternalData mesh;
    mesh.points.resize(3, DPoint(0, 0, 0));
    mesh.corners.attributes["corner"] = std::make_shared<TypedDPAttributeData<std::string>>(
        std::vector<std::string>(3), std::string(200, 'x'));
    dommy::point::PointPayloadBudget budget;
    budget.add(64U * 1024U * 1024U - 256U, 1U);
    bool rejected = false;
    try {
        preflightTopologyStorage(mesh, budget);
    } catch (const std::length_error&) {
        rejected = true;
    }
    assert(rejected);
}
void boundaryTests() {
    reset();
    DPData source;
    setup<int32_t>(source);
    auto data = block(source);
    data.inputs["aIterate"].number = 2;
    assert(smooth(data));
    checkValues<int32_t>(*std::dynamic_pointer_cast<DPData>(published), 2);
    reset();
    data = block(source);
    data.inputs["aIterate"].number = 0;
    assert(smooth(data) && queries == 0 && !called("attribute.add"));
    for (auto bad : {"aIterate", "aMaxSampleCount", "aPointRadius", "aSearchGridSize"}) {
        reset();
        data = block(source);
        data.inputs[bad].number = -1;
        assert(!smooth(data) && !publications && !copies);
    }
    reset();
    data = block(source);
    data.inputs["aIterate"].number = INT32_MAX;
    assert(!smooth(data) && !copies && !queries);
    reset();
    data = block(source);
    badNeighbor = true;
    assert(!smooth(data) && !publications && !called("attribute.add"));
    checkValues<int32_t>(source, 0);
    reset();
    data = block(source);
    data.inputs["aSmoothAttribute"].text = "absent";
    assert(smooth(data) && !queries);
    reset();
    DPData empty;
    data = block(empty);
    assert(smooth(data) && !queries);
    assert(!sample(data));
    reset();
    data = block(source);
    assert(sample(data));
    const auto grid = std::dynamic_pointer_cast<DOpenVDBData>(published)->grid;
    assert(grid->coordinates.size() == 3 && grid->coordinates[0].xyz[0] == -1);
    reset();
    data = block(source);
    source.points[0][0] = 2147483648.0f;
    data.inputs["aVoxelSize"].number = 1.0000000005;
    assert(!sample(data) && !publications && !called("grid.create"));
    int result = 999;
    assert(!dommy::vdb::checkedSamplingCoordinate(2147483647.0, result) && result == 999);
    assert(dommy::vdb::checkedSamplingCoordinate(-1.75, result) && result == -1);
    assert(dommy::vdb::checkedSamplingCoordinate(2147483520.0, result) && result == 2147483520);
    assert(dommy::vdb::checkedSamplingCoordinate(-2147483648.0, result) && result == INT32_MIN);
    reset();
    data = block(source);
    data.inputs["inDD"].payload = nullptr;
    assert(!smooth(data));
    reset();
    data = block(source);
    nullCreated = true;
    assert(!smooth(data) && !publications);
    setup<int32_t>(source);
    for (int mode = 0; mode < 2; ++mode) {
        reset();
        data = block(source);
        nullTransform = mode == 0;
        nullGrid = mode == 1;
        assert(!sample(data) && !publications);
    }
    for (double invalid : {0.0, -1.0, double(NAN), double(INFINITY)}) {
        reset();
        data = block(source);
        data.inputs["aVoxelSize"].number = invalid;
        assert(!sample(data) && !called("transform.create"));
    }
    reset();
    data = block(source);
    assert(DPAttrSmooth().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter &&
           operations.empty());
    assert(DOpenVDBSamplingDP().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter &&
           operations.empty());
}
int main() {
    samplingInputCases();
    mathTests();
    cornerBudgetTest();
    testType<int32_t>();
    testType<uint32_t>();
    testType<int64_t>();
    testType<float>();
    testType<double>();
    testType<Imath::V2i>();
    testType<Imath::V2f>();
    testType<Imath::V2d>();
    testType<Imath::V3i>();
    testType<Imath::V3f>();
    testType<Imath::V3d>();
    boundaryTests();
    int failures = 0;
    DPData source;
    setup<int64_t>(source);
    DCurveData curves;
    auto c = std::make_shared<DCurveInternalData>();
    setup<int64_t>(*c);
    curves.primitives.push_back(c);
    DMeshData meshes;
    auto m = std::make_shared<DMeshInternalData>();
    setup<int64_t>(*m);
    meshes.primitives.push_back(m);
    for (int path = 0; path < 4; ++path) {
        auto compute = path == 3 ? sample : smooth;
        const Payload& input = path == 1   ? static_cast<const Payload&>(curves)
                               : path == 2 ? static_cast<const Payload&>(meshes)
                                           : static_cast<const Payload&>(source);
        reset();
        auto data = block(input);
        assert(compute(data));
        const auto trace = operations;
        for (std::size_t i = 0U; i < trace.size(); ++i)
            for (int mode = 0; mode < 3; ++mode) {
                reset();
                const auto before = published;
                data = block(input);
                if (mode == 2)
                    attributeThrowAt = static_cast<int>(i);
                else {
                    failAt = static_cast<int>(i);
                    injectedStatus = mode ? MS::kInvalidParameter : MS::kFailure;
                }
                assert(!compute(data) && callCount == static_cast<int>(i) + 1);
                if (trace[i] != "output.clean")
                    assert(!publications && published == before);
                checkValues<int64_t>(source, 0);
                ++failures;
                checkValues<int64_t>(*c, 0);
                checkValues<int64_t>(*m, 0);
                reset();
                data = block(input);
                assert(compute(data));
            }
    }
    std::cout << "VDB smoothing/sampling: 11 types x 3 geometries, numeric boundaries, budgets and "
              << failures << " compute failure/exception cases passed\n";
}
