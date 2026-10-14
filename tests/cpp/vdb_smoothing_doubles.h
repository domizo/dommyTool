#pragma once
#define DOMMY_POISON_IMATH_DEFAULT
// Explicit DG/geometry/OpenVDB boundaries, not native tree/query emulation.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include <iostream>
#include <map>
#include <array>
#include <atomic>

using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using MTypeId = int;
using IDs = std::vector<uint32_t>;
using IDsListArray = std::vector<IDs>;
static int copies = 0, publications = 0, queries = 0;
static bool nullCreated = false, badNeighbor = false, nullTransform = false, nullGrid = false;
static void voidApi(const char* name) {
    if (!operation(name))
        throw std::runtime_error("injected native/geometry failure");
}
struct Group {
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds() const {
        return ids;
    }
};
struct Metadata {
    DPAttrList attributes;
    std::map<std::string, std::shared_ptr<Group>> groups;
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
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        auto found = attributes.find(name);
        return found == attributes.end() ? nullptr : found->second;
    }
    void addAttr(const std::string& name, const DPAttributeData& field) {
        voidApi("attribute.add");
        attributes[name] = field.copy();
    }
    void copyMetadata(const Metadata& other) {
        attributes.clear();
        groups.clear();
        for (const auto& field : other.attributes)
            attributes[field.first] = field.second->copy();
        for (const auto& group : other.groups)
            groups[group.first] = std::make_shared<Group>(*group.second);
    }
    bool validMetadata(std::size_t count) const {
        dommy::attribute::ValidationBudget cost;
        if (!dommy::attribute::validateAttributeList(attributes, count, cost))
            return false;
        for (const auto& group : groups)
            if (!group.second || group.second->ids.size() != count)
                return false;
        return true;
    }
};
struct Geometry : Metadata {
    DPointList points;
    std::size_t numberOfPoints() const {
        return points.size();
    }
    const DPointList* getPositionPtrConst() const {
        return &points;
    }
    bool validGeometry() const {
        if (!validMetadata(points.size()))
            return false;
        for (const auto& point : points)
            if (!dommy::attribute::isValidValue(point))
                return false;
        return true;
    }
    void copyGeometry(const Geometry& other) {
        points = other.points;
        copyMetadata(other);
    }
};
struct Payload {
    virtual ~Payload() {}
    virtual int type() const = 0;
    virtual bool valid() const = 0;
};
struct DPData : Payload, Geometry {
    static const int id = 22;
    int type() const override {
        return id;
    }
    bool valid() const override {
        return validGeometry();
    }
    bool hasValidPointTopology() const {
        return valid();
    }
    void setDPData(const DPData& other) {
        voidApi("point.copy");
        ++copies;
        copyGeometry(other);
    }
};
struct DCurveInternalData : Geometry {
    int getDegree() const {
        return 1;
    }
};
struct DMeshInternalData : Geometry {
    Metadata faces, corners, edges;
    int numFace() const {
        return 1;
    }
    std::size_t numberOfConnections() const {
        return points.size();
    }
    std::size_t numberOfUVs() const {
        return points.size();
    }
    auto beginFaceAttr() const -> ConstDPAttrIter {
        return faces.beginAttr();
    }
    auto endFaceAttr() const -> ConstDPAttrIter {
        return faces.endAttr();
    }
    auto beginPVtxAttr() const -> ConstDPAttrIter {
        return corners.beginAttr();
    }
    auto endPVtxAttr() const -> ConstDPAttrIter {
        return corners.endAttr();
    }
    auto beginFaceGrp() const -> decltype(faces.beginGrp()) {
        return faces.beginGrp();
    }
    auto endFaceGrp() const -> decltype(faces.endGrp()) {
        return faces.endGrp();
    }
    auto beginEdgeGrp() const -> decltype(edges.beginGrp()) {
        return edges.beginGrp();
    }
    auto endEdgeGrp() const -> decltype(edges.endGrp()) {
        return edges.endGrp();
    }
};
template <typename Primitive, int Id> struct Container : Payload, Metadata {
    static const int id = Id;
    std::vector<std::shared_ptr<Primitive>> primitives;
    int type() const override {
        return id;
    }
    bool valid() const override {
        if (!validMetadata(primitives.size()))
            return false;
        for (const auto& primitive : primitives)
            if (!primitive || !primitive->validGeometry())
                return false;
        return true;
    }
    bool hasValidCurveTopology() const {
        return valid();
    }
    bool hasValidMeshTopology() const {
        return valid();
    }
    std::size_t numberOfCurves() const {
        return primitives.size();
    }
    std::size_t numberOfMesh() const {
        return primitives.size();
    }
    auto beginPrims() const -> decltype(primitives.begin()) {
        return primitives.begin();
    }
    auto endPrims() const -> decltype(primitives.end()) {
        return primitives.end();
    }
    auto beginCurves() const -> decltype(primitives.begin()) {
        return primitives.begin();
    }
    auto endCurves() const -> decltype(primitives.end()) {
        return primitives.end();
    }
    auto beginMeshes() const -> decltype(primitives.begin()) {
        return primitives.begin();
    }
    auto endMeshes() const -> decltype(primitives.end()) {
        return primitives.end();
    }
    MStatus copyFrom(const Container& other) {
        const auto status = operation("container.copy");
        if (!status)
            return status;
        ++copies;
        copyMetadata(other);
        primitives.clear();
        for (const auto& p : other.primitives) {
            auto copy = std::make_shared<Primitive>(*p);
            copy->copyMetadata(*p);
            primitives.push_back(copy);
        }
        return MS::kSuccess;
    }
};
using DCurveData = Container<DCurveInternalData, 23>;
using DMeshData = Container<DMeshInternalData, 24>;
static std::shared_ptr<Payload> published;
struct Input {
    double number = 0.0;
    std::string text;
    const Payload* payload = nullptr;
};
struct MDataHandle {
    bool valid = false;
    const Input* input = nullptr;
    void check() const {
        assert(valid && input);
    }
    int typeId() const {
        check();
        return input->payload ? input->payload->type() : 0;
    }
    int asInt() const {
        check();
        return static_cast<int>(input->number);
    }
    double asDouble() const {
        check();
        return input->number;
    }
    MString asString() const {
        check();
        return MString(input->text.c_str());
    }
};
struct MPlug {
    MObject attribute;
    explicit MPlug(const MObject& a) : attribute(a) {}
    bool operator!=(const MObject& other) const {
        return attribute.name != other.name;
    }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        MDataHandle h;
        h.valid = bool(*status);
        h.input = &inputs[attribute.name];
        return h;
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = operation("output.handle");
        MDataHandle h;
        h.valid = bool(*status);
        return h;
    }
    MStatus setClean(const MPlug&) {
        return operation("output.clean");
    }
};
namespace dommy {
namespace maya {
template <typename T>
MStatus getPluginDataFromHandle(const MDataHandle& handle, int id, const T*& result) {
    handle.check();
    const auto status = operation("input.bind");
    if (!status)
        return status;
    result = handle.typeId() == id ? dynamic_cast<const T*>(handle.input->payload) : nullptr;
    return result ? MS::kSuccess : MS::kFailure;
}
template <typename T, typename Populate>
MStatus setPluginDataOutput(MDataBlock&, const MObject&, int, Populate populate) {
    auto status = operation("output.create");
    if (!status)
        return status;
    if (nullCreated)
        return MS::kFailure;
    auto candidate = std::make_shared<T>();
    status = populate(*candidate);
    if (!status)
        return status;
    if (!candidate->valid())
        return MS::kFailure;
    status = operation("output.publish");
    if (!status)
        return status;
    published = candidate;
    ++publications;
    return MS::kSuccess;
}
} // namespace maya
} // namespace dommy
inline const DPData* getInputDP(const MObject& attr, MDataBlock& data) {
    MStatus status;
    const auto handle = data.inputValue(attr, &status);
    if (!status)
        return nullptr;
    const DPData* input = nullptr;
    if (!dommy::maya::getPluginDataFromHandle(handle, DPData::id, input) || !input->valid())
        return nullptr;
    return input;
}
namespace tbb {
template <typename T> struct blocked_range {
    T first, last;
    blocked_range(T a, T b, std::size_t = 1) : first(a), last(b) {}
    T begin() const {
        return first;
    }
    T end() const {
        return last;
    }
};
template <typename Range, typename Operation> void parallel_for(const Range& range, Operation op) {
    op(range);
}
} // namespace tbb
namespace openvdb {
using Vec3R = Imath::V3d;
using Vec3d = Imath::V3d;
struct Coord {
    std::array<int, 3> xyz;
    Coord() : xyz{{0, 0, 0}} {}
    Coord(int x, int y, int z) : xyz{{x, y, z}} {}
};
namespace math {
struct Transform {
    double voxel;
    static std::shared_ptr<Transform> createLinearTransform(double voxel) {
        voidApi("transform.create");
        if (nullTransform)
            return nullptr;
        auto result = std::make_shared<Transform>();
        result->voxel = voxel;
        return result;
    }
    Vec3d worldToIndex(const Vec3d& point) const {
        voidApi("transform.apply");
        return point / voxel;
    }
};
} // namespace math
enum GridClass { GRID_FOG_VOLUME };
struct FloatGrid {
    std::vector<Coord> coordinates;
    static std::shared_ptr<FloatGrid> create(float) {
        voidApi("grid.create");
        return nullGrid ? nullptr : std::make_shared<FloatGrid>();
    }
    void setTransform(const std::shared_ptr<math::Transform>&) {
        voidApi("grid.transform");
    }
    void setName(const char* value) {
        assert(std::string(value) == "density");
        voidApi("grid.name");
    }
    void setGridClass(GridClass) {
        voidApi("grid.class");
    }
    struct Accessor {
        FloatGrid* grid;
        void setValue(const Coord& coord, float value) {
            assert(value == 1.0f);
            voidApi("grid.write");
            grid->coordinates.push_back(coord);
        }
    };
    Accessor getAccessor() {
        voidApi("grid.accessor");
        return Accessor{this};
    }
};
} // namespace openvdb
struct DPointVdbParticleList {
    DPointVdbParticleList(const std::vector<openvdb::Vec3R>&, const std::vector<double>&) {}
};
struct ParticleAtlas {
    void construct(const DPointVdbParticleList&, double) {
        ++queries;
        voidApi("atlas.create");
    }
};
struct PointSearchOp {
    std::size_t maximum;
    IDsListArray& neighbors;
    PointSearchOp(const ParticleAtlas&, const DPointVdbParticleList&, std::size_t k, double,
                  IDsListArray& ids, const std::vector<openvdb::Vec3R>&)
        : maximum(k), neighbors(ids) {}
    void operator()(const tbb::blocked_range<std::size_t>& range) const {
        voidApi("atlas.query");
        for (std::size_t i = range.begin(); i < range.end(); ++i) {
            for (std::size_t j = 0U; j < neighbors.size() && neighbors[i].size() < maximum; ++j)
                if (i != j)
                    neighbors[i].push_back(badNeighbor ? UINT32_MAX : static_cast<uint32_t>(j));
        }
    }
};
struct DOpenVDBData : Payload {
    static const int id = 25;
    std::shared_ptr<openvdb::FloatGrid> grid;
    int type() const override {
        return id;
    }
    bool valid() const override {
        return bool(grid);
    }
    void insert(const std::shared_ptr<openvdb::FloatGrid>& value) {
        voidApi("grid.insert");
        grid = value;
    }
};
struct MFnPluginData {
    std::shared_ptr<DOpenVDBData> candidate;
    MFnPluginData& object() {
        return *this;
    }
};
namespace dommy {
namespace maya {
inline MStatus createPluginData(MFnPluginData& fn, int, DOpenVDBData*& output) {
    const auto status = operation("output.create");
    if (!status)
        return status;
    if (nullCreated)
        return MS::kFailure;
    fn.candidate = std::make_shared<DOpenVDBData>();
    output = fn.candidate.get();
    return MS::kSuccess;
}
template <typename T>
MStatus publishTypedPluginDataObject(MDataHandle& handle, MFnPluginData& fn, int) {
    assert(handle.valid);
    const auto status = operation("output.publish");
    if (!status)
        return status;
    assert(fn.candidate->valid());
    published = fn.candidate;
    ++publications;
    return MS::kSuccess;
}
} // namespace maya
} // namespace dommy
