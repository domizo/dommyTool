#pragma once

// Explicit host/native boundaries. Matrix arithmetic here is a test oracle,
// not a substitute for OpenVDB's maps, sparse trees or Maya evaluation.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DCheckedSize.h"
#include "modules/volume/VDB/Common/DVDBComputeGuard.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include <array>
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>

static int publications, splits, invalidReads, gridCopies;
static bool missingInput, nullCreated, nullGrid, nullMap, failedDispatch, badInverse;
static bool lastCopyInactive;
static int badMapCache, badMapAt, mapConstructions;
inline void voidApi(const char* name) {
    if (!operation(name))
        throw std::runtime_error("injected native failure");
}
namespace boost {
struct bad_lexical_cast : std::exception {};
template <typename T> T lexical_cast(const std::string& text) {
    std::istringstream stream(text);
    T value;
    if (!(stream >> value) || stream.peek() != std::char_traits<char>::eof())
        throw bad_lexical_cast();
    return value;
}
inline std::string is_any_of(const char* value) {
    return value;
}
inline void split(std::vector<std::string>& out, const std::string& value,
                  const std::string& delimiters) {
    out.clear();
    std::size_t begin = 0U;
    for (;;) {
        const auto end = value.find_first_of(delimiters, begin);
        out.push_back(value.substr(begin, end - begin));
        if (end == std::string::npos)
            break;
        begin = end + 1U;
    }
}
namespace math {
namespace constants {
template <typename T> T pi() {
    return std::acos(T(-1));
}
} // namespace constants
} // namespace math
} // namespace boost

enum class DVdbGridType { kFloat = 1, kDouble = 2, kInt32 = 3, kInt64 = 4, kBool = 5, kVec3f = 6, kVec3d = 7, kVec3i = 8, kInvalid = 0 };
namespace openvdb {
template <typename T> std::shared_ptr<T> ConstPtrCast(const std::shared_ptr<const T>& value) {
    return std::const_pointer_cast<T>(value);
}
struct Vec3R {
    double values[3];
    Vec3R(double x, double y, double z) : values{x, y, z} {}
    double operator[](int i) const {
        return values[i];
    }
};
struct Mat4R {
    double values[4][4]{};
    static Mat4R identity() {
        Mat4R result;
        for (int i = 0; i < 4; ++i)
            result[i][i] = 1.0;
        return result;
    }
    double* operator[](std::size_t i) {
        return values[i];
    }
    const double* operator[](std::size_t i) const {
        return values[i];
    }
    Mat4R operator*(const Mat4R& other) const {
        Mat4R result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    result[i][j] += values[i][k] * other[k][j];
        return result;
    }
    void preTranslate(const Vec3R& value) {
        auto t = identity();
        for (int i = 0; i < 3; ++i)
            t[3][i] = value[i];
        *this = t * *this;
    }
    void preScale(const Vec3R& value) {
        auto s = identity();
        for (int i = 0; i < 3; ++i)
            s[i][i] = value[i];
        *this = s * *this;
    }
    void preRotate(int axis, double angle) {
        auto r = identity();
        const int a = (axis + 1) % 3, b = (axis + 2) % 3;
        r[a][a] = r[b][b] = std::cos(angle);
        r[a][b] = std::sin(angle);
        r[b][a] = -std::sin(angle);
        *this = r * *this;
    }
    Mat4R inverse() const {
        voidApi("matrix.inverse");
        Mat4R a = *this, result = identity();
        if (badInverse) {
            result[0][0] = std::numeric_limits<double>::infinity();
            return result;
        }
        for (int i = 0; i < 4; ++i) {
            int pivot = i;
            for (int row = i + 1; row < 4; ++row)
                if (std::abs(a[row][i]) > std::abs(a[pivot][i]))
                    pivot = row;
            if (a[pivot][i] == 0.0)
                throw std::runtime_error("singular matrix");
            for (int col = 0; col < 4; ++col) {
                std::swap(a[i][col], a[pivot][col]);
                std::swap(result[i][col], result[pivot][col]);
            }
            const double factor = a[i][i];
            for (int col = 0; col < 4; ++col) {
                a[i][col] /= factor;
                result[i][col] /= factor;
            }
            for (int row = 0; row < 4; ++row) {
                if (row == i)
                    continue;
                const double multiplier = a[row][i];
                for (int col = 0; col < 4; ++col) {
                    a[row][col] -= multiplier * a[i][col];
                    result[row][col] -= multiplier * result[i][col];
                }
            }
        }
        return result;
    }
};
namespace math {
enum { X_AXIS, Y_AXIS, Z_AXIS };
struct AffineMap {
    using Ptr = std::shared_ptr<AffineMap>;
    Mat4R matrix;
    int invalidCache = 0;
    explicit AffineMap(const Mat4R& value) : matrix(value) {
        voidApi("map.create");
        invalidCache = mapConstructions++ == badMapAt ? badMapCache : 0;
    }
    double determinant() const {
        return invalidCache == 1 ? std::numeric_limits<double>::infinity() : 1.0;
    }
    Vec3R voxelSize() const {
        return Vec3R(invalidCache == 2 ? std::numeric_limits<double>::infinity() : 1.0,
                     invalidCache == 3 ? 0.0 : 1.0, 1.0);
    }
    Mat4R getConstJacobianInv() const {
        auto result = Mat4R::identity();
        if (invalidCache == 4)
            result[1][2] = std::numeric_limits<double>::quiet_NaN();
        return result;
    }
    Mat4R getMat4() const {
        return matrix;
    }
    Ptr getAffineMap() const {
        voidApi("map.affine");
        return nullMap ? nullptr : std::make_shared<AffineMap>(*this);
    }
};
inline AffineMap::Ptr simplify(AffineMap::Ptr value) {
    voidApi("map.simplify");
    return value;
}
struct Transform {
    using Ptr = std::shared_ptr<Transform>;
    AffineMap::Ptr map;
    explicit Transform(AffineMap::Ptr value) : map(value) {
        voidApi("transform.create");
    }
    AffineMap::Ptr baseMap() const {
        return map;
    }
};
} // namespace math
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    DVdbGridType kind = DVdbGridType::kVec3f;
    std::string name;
    std::shared_ptr<int> tree = std::make_shared<int>(1);
    math::Transform::Ptr xform;
    const std::string& getName() const {
        return name;
    }
    void setName(const std::string& value) {
        voidApi("grid.name");
        name = value;
    }
    const math::Transform& transform() const {
        return *xform;
    }
    ConstPtr copyGrid() const {
        voidApi("grid.copy");
        ++gridCopies;
        return nullGrid ? nullptr : std::make_shared<GridBase>(*this);
    }
    void setTransform(math::Transform::Ptr value) {
        voidApi("grid.transform");
        xform = value;
    }
};
} // namespace openvdb
using Grid = openvdb::GridBase;
using GridPtr = Grid::Ptr;
using GridCPtr = Grid::ConstPtr;
using GridCRef = const Grid&;
using GridCPtrVec = std::vector<GridCPtr>;
using GridCPtrVecCIter = GridCPtrVec::const_iterator;
inline DVdbGridType getVdbGridType(const Grid& grid) {
    return grid.kind;
}

struct DOpenVDBData {
    GridCPtrVec grids;
    bool payloadValid = true;
    bool hasValidData() const {
        return payloadValid && grids.size() <= dommy::vdb::kMaxVdbGridCount &&
               std::all_of(grids.begin(), grids.end(),
                           [](const GridCPtr& value) { return bool(value); });
    }
    std::size_t numberOfGrids() const {
        return grids.size();
    }
    GridCRef grid(std::size_t index) const {
        return *grids.at(index);
    }
    GridCPtr gridPtr(std::size_t index) const {
        return grids.at(index);
    }
    void insert(GridCPtr value) {
        voidApi("data.insert");
        if (!value || grids.size() >= dommy::vdb::kMaxVdbGridCount)
            throw std::length_error("invalid grid count");
        grids.push_back(value);
    }
    void insert(GridCRef value) {
        insert(value.copyGrid());
    }
    void insert(const GridCPtrVec& values) {
        voidApi("data.insert");
        if (values.size() > dommy::vdb::kMaxVdbGridCount)
            throw std::length_error("grid count limit");
        grids = values;
    }
};
static DOpenVDBData inputData, stagedData, outputData;
struct MFloatVector {
    float values[3];
    MFloatVector(float x = 0, float y = 0, float z = 0) : values{x, y, z} {}
    float& operator[](unsigned int i) {
        return values[i];
    }
    float operator[](unsigned int i) const {
        return values[i];
    }
};
struct InputValue {
    float number = 0;
    std::string text;
    MFloatVector vector;
};
struct MDataHandle {
    InputValue value;
    bool valid;
    void check() const {
        if (!valid) {
            ++invalidReads;
            throw std::runtime_error("invalid handle");
        }
    }
    float asFloat() const {
        check();
        return value.number;
    }
    bool asBool() const {
        check();
        return value.number != 0;
    }
    MFloatVector asFloatVector() const {
        check();
        return value.vector;
    }
    MString asString() const {
        check();
        return MString(value.text.c_str());
    }
};
struct MPlug {
    MObject value;
    explicit MPlug(MObject object) : value(object) {}
    bool operator!=(const MObject& object) const {
        return value.name != object.name;
    }
};
struct MDataBlock {
    std::map<std::string, InputValue> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        return {inputs[attribute.name], bool(*status)};
    }
};
struct MFnPluginData {
    MObject object() const {
        return {"staged"};
    }
};
inline const DOpenVDBData* getInputVDB(const MObject&, MDataBlock&) {
    return operation("input.vdb") && !missingInput ? &inputData : nullptr;
}
inline MStatus createOpenVDBOutputData(MFnPluginData&, DOpenVDBData*& output) {
    const MStatus status = operation("output.create");
    output = status && !nullCreated ? &stagedData : nullptr;
    return status;
}
inline MStatus publishOpenVDBOutput(MDataBlock&, const MObject&, const MPlug&, const MObject&) {
    DOMMY_RETURN_IF_FAILURE(operation("output.handle"));
    DOMMY_RETURN_IF_FAILURE(operation("output.set"));
    outputData = stagedData;
    ++publications;
    return operation("output.clean");
}
// Only the splitter boundary is substituted; no sparse topology is simulated.
struct VectorGridSplitter {
    GridPtr grids[3];
    explicit VectorGridSplitter(bool inactive) {
        lastCopyInactive = inactive;
    }
    GridPtr getXGrid() const {
        return grids[0];
    }
    GridPtr getYGrid() const {
        return grids[1];
    }
    GridPtr getZGrid() const {
        return grids[2];
    }
};
inline bool processVdbVectorGrid(DVdbGridType type, const Grid& source, VectorGridSplitter& op) {
    voidApi("native.split");
    ++splits;
    if (failedDispatch || (type != DVdbGridType::kVec3f && type != DVdbGridType::kVec3d &&
                           type != DVdbGridType::kVec3i))
        return false;
    for (auto& value : op.grids) {
        value = nullGrid ? nullptr : std::make_shared<Grid>(source);
    }
    return true;
}
