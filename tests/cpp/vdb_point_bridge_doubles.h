#pragma once
// Explicit Maya ownership/DG and OpenVDB query doubles, not native simulators.
// Typed attributes, numeric conversion and cumulative budgets are production.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include <map>
#include <iostream>
#include <array>

using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using DVec3Data = Imath::V3f;
using DVec3DataList = std::vector<DVec3Data>;
using DA_FLOAT = float;
using DA_DOUBLE = double;
using DA_INT = int32_t;
using DA_INT64 = int64_t;
using DA_VEC3F = Imath::V3f;
using DA_VEC3D = Imath::V3d;
static int copies = 0, publications = 0, queries = 0, invalidReads = 0;
static bool missingPoint = false, missingVdb = false, nullCreated = false;
static bool nullQuery = false, emptySurface = false, failedSearch = false;
static int badResult = 0;
static double indexScale = 1.0;
static bool badSample = false;
static bool rejectAttribute = false;
void AddAttributeList(DPAttrList&, const std::string&, const DPAttributeData&);
static std::vector<std::array<int, 3>> sampledCoordinates;
inline void voidApi(const char* name) {
    if (!operation(name))
        throw std::runtime_error("injected native failure");
}
struct Group {
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds() const {
        return ids;
    }
};
struct DPData {
    static const int id = 22;
    DPointList points;
    DPAttrList attributes;
    DPAttrIter beginAttr() { return attributes.begin(); }
    DPAttrIter endAttr() { return attributes.end(); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(attributes, name);
    }
    void addAttr(const std::string& name, const DPAttributeData& field) {
        voidApi("attribute.add");
        if (!rejectAttribute) AddAttributeList(attributes, name, field);
    }
    std::map<std::string, std::shared_ptr<Group>> groups;
    std::size_t reportedCount = 0U;
    bool valid = true;
    std::size_t numberOfPoints() const {
        return reportedCount ? reportedCount : points.size();
    }
    auto beginAttr() const -> ConstDPAttrIter {
        return attributes.begin();
    }
    auto endAttr() const -> ConstDPAttrIter {
        return attributes.end();
    }
    auto beginGrp() const -> decltype(groups.begin()) {
        return groups.begin();
    }
    auto endGrp() const -> decltype(groups.end()) {
        return groups.end();
    }
    bool hasValidPointTopology() const {
        dommy::attribute::ValidationBudget cost;
        if (!valid || !dommy::attribute::validateAttributeList(attributes, numberOfPoints(), cost))
            return false;
        for (const auto& point : points)
            if (!dommy::attribute::isValidValue(point))
                return false;
        for (const auto& group : groups)
            if (!group.second || group.second->ids.size() != numberOfPoints())
                return false;
        return true;
    }
    const DPointList* getPositionPtrConst() const {
        return &points;
    }
    void setDPData(const DPData& source) {
        voidApi("point.copy");
        ++copies;
        *this = source;
        for (auto& field : attributes)
            field.second = field.second->copy();
        for (auto& group : groups)
            group.second = std::make_shared<Group>(*group.second);
    }
    template <typename T>
    void addAttrWithValues(const std::string& name, const std::vector<T>& values) {
        addAttr(name, TypedDPAttributeData<T>(values));
    }
    bool replacePositions(const DPointList& values) {
        const auto status = operation("point.replace");
        if (status)
            points = values;
        return bool(status);
    }
};
static DPData inputData, outputData;
namespace openvdb {
using Vec3s = Imath::V3f;
using Vec3d = Imath::V3d;
using Vec3R = Vec3d;
enum GridClass { GRID_LEVEL_SET, GRID_FOG_VOLUME };
struct Coord {
    int xyz[3];
    Coord(int x, int y, int z) : xyz{x, y, z} {}
    int operator[](std::size_t axis) const {
        return xyz[axis];
    }
};
struct Transform {
    template <typename V> Vec3d worldToIndex(const V& value) const {
        voidApi("grid.transform");
        Vec3d result;
        for (unsigned i = 0; i < 3; ++i)
            result[i] = value[i] * indexScale;
        return result;
    }
};
struct GridBase {
    using ConstPtr = std::shared_ptr<const GridBase>;
    virtual ~GridBase() {}
    int kind = 0;
    GridClass classification = GRID_LEVEL_SET;
    std::string name = "sample";
    GridClass getGridClass() const {
        return classification;
    }
    const std::string& getName() const {
        return name;
    }
};
template <typename Value, int Kind> struct Grid : GridBase {
    Value sample{};
    Grid() {
        kind = Kind;
    }
    struct Accessor {
        const Grid* grid;
        Value getValue(const Coord& c) const {
            voidApi("grid.sample");
            sampledCoordinates.push_back({{c[0], c[1], c[2]}});
            return grid->sample;
        }
    };
    Accessor getConstAccessor() const {
        voidApi("grid.accessor");
        return {this};
    }
    const Transform& transform() const {
        static const Transform t;
        return t;
    }
};
using FloatGrid = Grid<float, 0>;
using DoubleGrid = Grid<double, 1>;
using Int32Grid = Grid<int32_t, 2>;
using Int64Grid = Grid<int64_t, 3>;
using Vec3SGrid = Grid<Vec3s, 4>;
using Vec3DGrid = Grid<Vec3d, 5>;
namespace tools {
template <typename Grid> struct ClosestSurfacePoint {
    using Ptr = std::unique_ptr<ClosestSurfacePoint>;
    struct IndexTree {
        std::size_t activeVoxelCount() const {
            return emptySurface ? 0U : 2U;
        }
    };
    static Ptr create(const Grid&, float) {
        voidApi("query.create");
        return nullQuery ? Ptr() : Ptr(new ClosestSurfacePoint);
    }
    const IndexTree& indexTree() const {
        static const IndexTree t;
        return t;
    }
    bool searchAndReplace(std::vector<Vec3R>& points, std::vector<float>& distances) {
        ++queries;
        assert(!emptySurface); // Native 5.2 would access an empty surface array.
        const bool success = bool(operation("query.search")) && !failedSearch;
        for (auto& point : points)
            point[0] += 2.0;
        distances.assign(points.size(), 2.0f);
        if (badResult == 1)
            points.pop_back();
        if (badResult == 2)
            distances.pop_back();
        if (badResult == 3)
            distances[0] = INFINITY;
        if (badResult == 4)
            distances[0] = -1;
        if (badResult == 5)
            points[0][1] = NAN;
        if (badResult == 6)
            points[0][2] = std::numeric_limits<double>::max();
        // Failure may leave valid-looking local buffers; the caller must still
        // honor the status instead of relying only on cardinality checks.
        return success;
    }
};
} // namespace tools
} // namespace openvdb
enum class DVdbGridType { kFloat = 1, kDouble = 2, kInt32 = 3, kInt64 = 4, kVec3f = 6, kVec3d = 7, kOther = 100 };
inline DVdbGridType getVdbGridType(const openvdb::GridBase& grid) {
    // Fixture tags are independent of the production enum's stable values.
    switch (grid.kind) {
    case 0: return DVdbGridType::kFloat;
    case 1: return DVdbGridType::kDouble;
    case 2: return DVdbGridType::kInt32;
    case 3: return DVdbGridType::kInt64;
    case 4: return DVdbGridType::kVec3f;
    case 5: return DVdbGridType::kVec3d;
    default: return DVdbGridType::kOther;
    }
}
using GridCPtrVec = std::vector<openvdb::GridBase::ConstPtr>;
struct DOpenVDBData {
    GridCPtrVec grids;
};
static DOpenVDBData inputVdb;
struct Input {
    double number = 0;
    MString text{"clp"};
};
struct MDataHandle {
    bool valid = false;
    const Input* value = nullptr;
    void check() const {
        if (!valid)
            ++invalidReads;
        assert(valid);
    }
    float asFloat() const {
        check();
        return static_cast<float>(value->number);
    }
    bool asBool() const {
        check();
        return value->number != 0;
    }
    MString asString() const {
        check();
        return value->text;
    }
};
struct MPlug {
    MObject object;
    explicit MPlug(const MObject& o) : object(o) {}
    bool operator!=(const MObject& o) const {
        return object.name != o.name;
    }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& object, MStatus* status) {
        *status = operation("input " + object.name);
        MDataHandle handle;
        handle.valid = bool(*status);
        handle.value = &inputs[object.name];
        return handle;
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = operation("output.handle");
        MDataHandle handle;
        handle.valid = bool(*status);
        return handle;
    }
    MStatus setClean(const MPlug&) {
        return operation("output.clean");
    }
};
struct MFnPluginData {
    DPData staged;
    const MFnPluginData& object() const {
        return *this;
    }
};
namespace dommy {
namespace maya {
template <typename T>
MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const T*& output) {
    handle.check();const auto status = operation("point.bind");
    output = status && !missingPoint ? &inputData : nullptr;
    return status;
}
template <typename T> MStatus createPluginData(MFnPluginData& fn, int, T*& output) {
    const auto status = operation("output.create");
    output = status && !nullCreated ? &fn.staged : nullptr;
    return status && !output ? MStatus(MS::kFailure) : status;
}
template <typename T>
MStatus publishTypedPluginDataObject(MDataHandle& handle, const MFnPluginData& fn, int) {
    handle.check();
    if (!fn.staged.hasValidPointTopology())
        return MS::kInvalidParameter;
    const auto status = operation("output.publish");
    if (status) {
        outputData = fn.staged;
        ++publications;
    }
    return status;
}
} // namespace maya
} // namespace dommy
inline const DPData* getInputDP(const MObject&, MDataBlock&) {
    if (!operation("point.input") || missingPoint || !inputData.hasValidPointTopology())
        return nullptr;
    return &inputData;
}
inline const DOpenVDBData* getInputVDBfromHandle(const MDataHandle& handle) {
    handle.check();
    voidApi("grid.bind");
    return missingVdb ? nullptr : &inputVdb;
}
inline void getGrids(GridCPtrVec& grids, const DOpenVDBData& data, const std::string& selection) {
    voidApi("grid.select");
    for (const auto& grid : data.grids)
        if (selection.empty() || selection == "*" || grid->getName() == selection)
            grids.push_back(grid);
}
inline void EchoWarning(const char*, const char*) {}
