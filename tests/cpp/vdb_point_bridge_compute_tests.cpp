#include "vdb_point_bridge_doubles.h"
#include "vdb_point_bridge_compute.inc"

void setup() {
    resetCalls();
    injectedStatus = MS::kFailure;
    copies = publications = queries = invalidReads = 0;
    missingPoint = missingVdb = nullCreated = nullQuery = emptySurface = failedSearch = false;
    badResult = 0;
    indexScale = 1;
    sampledCoordinates.clear();
    rejectAttribute=false;
    inputData = DPData();
    outputData = DPData();
    inputData.points = {{-1.75f, 2.25f, -3.5f}, {4, 5, 6}};
    inputData.attributes["id"] =
        std::make_shared<TypedDPAttributeData<int32_t>>(std::vector<int32_t>{8, 9}, 3);
    auto group = std::make_shared<Group>();
    group->ids = {true, false};
    inputData.groups["g"] = group;
    outputData.points = {{999, 999, 999}};
    inputVdb.grids = {std::make_shared<openvdb::FloatGrid>()};
}
MDataBlock block() {
    MDataBlock data;
    data.inputs["aIsDeform"].number = 1;
    data.inputs["aAddAttribute"].number = 1;
    data.inputs["aSourceGridName"].text = "*";
    return data;
}
MStatus closest(MDataBlock& data) {
    return DPClosestPointVDB().compute(MPlug(DPClosestPointVDB::outDP), data);
}
MStatus sample(MDataBlock& data) {
    return DPAttributeVDB().compute(MPlug(DPAttributeVDB::outDP), data);
}
bool called(const std::string& value) {
    return std::find(operations.begin(), operations.end(), value) != operations.end();
}
template <typename T> const std::vector<T>& values(const std::string& name) {
    const auto attr =
        std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(outputData.attributes.at(name));
    assert(attr);
    return attr->value();
}
void unchanged() {
    assert(inputData.points[0][0] == -1.75f);
    assert(inputData.attributes.count("clp") == 0 && inputData.attributes.count("sample") == 0);
    assert(inputData.groups["g"]->ids == std::vector<bool>({true, false}));
}
void testClosest() {
    setup();auto replacement=block();
    inputData.attributes["clp"]=std::make_shared<DPFloatAttr>(std::vector<float>{4,5});
    assert(closest(replacement));assert(values<Imath::V3f>("clp")[0][0]==.25f);
    assert(inputData.attributes["clp"]->getAttrType()==DP_ATTR_FLOAT);
    for (int deform = 0; deform < 2; ++deform)
        for (int add = 0; add < 2; ++add) {
            setup();
            auto data = block();
            data.inputs["aIsDeform"].number = deform;
            data.inputs["aAddAttribute"].number = add;
            assert(closest(data) && publications == 1 && queries == 1);
            assert(outputData.points[0][0] == (deform ? .25f : -1.75f));
            assert(outputData.attributes.count("clp") == std::size_t(add));
            assert(called("point.replace") == bool(deform));
            if (add)
                assert(values<Imath::V3f>("clp")[1][0] == 6.0f);
            assert(values<int32_t>("id") == std::vector<int32_t>({8, 9}));
            unchanged();
        }
    for (int bad = 0; bad < 11; ++bad) {
        setup();
        auto data = block();
        if (bad == 0)
            nullQuery = true;
        else if (bad == 1)
            emptySurface = true;
        else if (bad == 2)
            failedSearch = true;
        else if (bad <= 8)
            badResult = bad - 2;
        else
            data.inputs["aIsoOffset"].number = bad == 9 ? NAN : INFINITY;
        assert(!closest(data) && publications == 0);
        assert(!called("attribute.add") && !called("point.replace"));
        assert(outputData.points[0][0] == 999);
        unchanged();
        if (bad < 2 || bad > 8)
            assert(queries == 0);
    }
}
template <typename Grid, typename T> void testSampleType(const T& value) {
    setup();
    auto data = block();
    auto grid = std::make_shared<Grid>();
    grid->sample = value;
    inputVdb.grids = {grid, std::make_shared<openvdb::FloatGrid>()};
    assert(sample(data) && publications == 1);
    assert(sampledCoordinates.size() == 2);
    assert((sampledCoordinates[0] == std::array<int, 3>{{-1, 2, -3}}));
    assert(outputData.attributes.count("sample") == 1);
    assert(outputData.points[0][0] == -1.75f);
    unchanged();
    const auto result = outputData.attributes["sample"];
    assert(result->attrSize() == 2);
    // The caller checks actual typed values below, not just successful publication.
}
void testSamples() {
    setup();auto replacement=block();
    inputData.attributes["sample"]=std::make_shared<DPStringAttr>(std::vector<std::string>{"a","b"});
    assert(sample(replacement));assert(values<float>("sample")[0]==0.0f);
    assert(inputData.attributes["sample"]->getAttrType()==DP_ATTR_STRING);
    testSampleType<openvdb::FloatGrid>(1.5f);
    assert(values<float>("sample")[0] == 1.5f);
    testSampleType<openvdb::DoubleGrid>(2.5);
    assert(values<double>("sample")[0] == 2.5);
    testSampleType<openvdb::Int32Grid>(int32_t(-8));
    assert(values<int32_t>("sample")[0] == -8);
    testSampleType<openvdb::Int64Grid>(int64_t(8000000000LL));
    assert(values<int64_t>("sample")[0] == 8000000000LL);
    testSampleType<openvdb::Vec3SGrid>(openvdb::Vec3s(1, 2, 3));
    assert(values<Imath::V3f>("sample")[0][2] == 3);
    testSampleType<openvdb::Vec3DGrid>(openvdb::Vec3d(4, 5, 6));
    assert(values<Imath::V3d>("sample")[0][2] == 6);
    for (double scale : {3e9, -3e9, double(INFINITY), double(NAN)}) {
        setup();
        auto data = block();
        indexScale = scale;
        assert(!sample(data) && !publications && sampledCoordinates.empty());
        unchanged();
    }
    setup();
    auto data = block();
    auto bad = std::make_shared<openvdb::DoubleGrid>();
    bad->sample = NAN;
    inputVdb.grids = {bad};
    assert(!sample(data) && !publications && !called("attribute.add"));
    setup();
    data = block();
    auto vector = std::make_shared<openvdb::Vec3SGrid>();
    vector->sample[1] = INFINITY;
    inputVdb.grids = {vector};
    assert(!sample(data) && !publications && !called("attribute.add"));
    for (double value : {-1.75, -.75, 0.0, .75, 1.75, 2147483647.0, -2147483648.0}) {
        const auto coordinate = checkedSampleCoord(openvdb::Vec3d(value, value, value));
        assert(coordinate[0] == static_cast<int>(value));
    }
    for (double value : {2147483648.0, -2147483649.0, double(NAN), double(INFINITY)}) {
        bool rejected = false;
        try {
            (void)checkedSampleCoord(openvdb::Vec3d(value, 0, 0));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
}
void testPassThroughAndBudget() {
    for (auto compute : {closest, sample}) {
        for (int mode = 0; mode < 4; ++mode) {
            setup();
            auto data = block();
            if (mode == 0)
                inputData = DPData();
            if (mode == 1)
                missingVdb = true;
            if (mode == 2)
                inputVdb.grids.clear();
            if (mode == 3) {
                auto unsupported = std::make_shared<openvdb::GridBase>();
                unsupported->kind = 6;
                inputVdb.grids = {unsupported};
            }
            assert(compute(data) && publications == 1 && !queries && !called("grid.sample"));
        }
        setup();
        auto data = block();
        missingPoint = true;
        assert(compute(data) == MS::kInvalidParameter && !publications);
        setup();
        data = block();
        nullCreated = true;
        assert(!compute(data) && !copies && !publications);
        setup();
        data = block();
        inputData = DPData();
        inputData.reportedCount = 2000000;
        assert(!compute(data) && !copies && !called("output.create"));
    }
    setup();
    auto data = block();
    data.inputs["aSourceGridName"].text = "absent";
    assert(sample(data) && !called("grid.sample"));
    setup();
    data = block();
    auto fog = std::make_shared<openvdb::FloatGrid>();
    fog->classification = openvdb::GRID_FOG_VOLUME;
    inputVdb.grids = {fog};
    assert(closest(data) && !queries);
    setup();
    assert(DPAttributeVDB().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter &&
           operations.empty());
    assert(DPClosestPointVDB().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter &&
           operations.empty());
    const std::size_t maximum = 64U * 1024U * 1024U;
    dommy::point::PointPayloadBudget exact;
    exact.add(maximum, 1);
    bool rejected = false;
    try {
        exact.add(1, 1);
    } catch (const std::length_error&) {
        rejected = true;
    }
    assert(rejected);
    // A dynamic default participates even when every current field value is empty.
    inputData.attributes["text"] = std::make_shared<TypedDPAttributeData<std::string>>(
        std::vector<std::string>{"", ""}, std::string(200, 'x'));
    dommy::point::PointPayloadBudget total;
    total.add(maximum - 200, 1);
    rejected = false;
    try {
        dommy::point::preflightPointPayload(inputData, 0, total);
    } catch (const std::length_error&) {
        rejected = true;
    }
    assert(rejected);
}
int main() {
    testClosest();
    testSamples();
    testPassThroughAndBudget();
    for(auto compute:{closest,sample}){
        setup();auto data=block();rejectAttribute=true;
        assert(!compute(data)&&publications==0);
        setup();data=block();assert(compute(data));
        setup();data=block();inputData.valid=false;assert(compute(data)==MS::kInvalidParameter&&publications==0);
    }
    setup();auto invalidName=block();invalidName.inputs["aAttributeName"].text="";
    assert(closest(invalidName)==MS::kInvalidParameter&&queries==0&&publications==0);
    setup();invalidName=block();invalidName.inputs["aAttributeName"].text="";
    invalidName.inputs["aAddAttribute"].number=0;assert(closest(invalidName));
    setup();invalidName=block();
    auto unnamed=std::make_shared<openvdb::FloatGrid>();unnamed->name="";
    inputVdb.grids={unnamed};
    assert(!sample(invalidName)&&publications==0&&sampledCoordinates.empty());
    int failures = 0;
    for (auto compute : {closest, sample}) {
        setup();
        auto data = block();
        assert(compute(data));
        const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i)
            for (int mode = 0; mode < 3; ++mode) {
                setup();
                data = block();
                if (mode == 2)
                    attributeThrowAt = static_cast<int>(i);
                else {
                    failAt = static_cast<int>(i);
                    injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
                }
                assert(!compute(data) && invalidReads == 0);
                assert(publications == (trace[i] == "output.clean" ? 1 : 0));
                assert(callCount == static_cast<int>(i) + 1);
                unchanged();
                ++failures;
                setup();
                data = block();
                assert(compute(data) && publications == 1);
            }
    }
    std::cout << "VDB point bridges: six sample types, four deformation/attribute modes, "
                 "empty/invalid queries, budgets and "
              << failures << " compute failures/exceptions passed\n";
}
