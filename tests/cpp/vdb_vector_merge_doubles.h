#pragma once

// Maya failure injection and sparse-grid boundary doubles. These do not model
// native VDB topology, memory allocation, transforms, threading or registration.
#include "maya_attribute_doubles.h"
#define MGlobal GuardMGlobal
#include "common/utility/DComputeGuard.h"
#undef MGlobal
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <map>
#include <regex>
#include <set>
#include <typeinfo>

struct MGlobal : GuardMGlobal {
    static void displayInfo(const MString&) {}
};
#define DOMMY_VDB_COMPUTE_CATCH(name) DOMMY_COMPUTE_CATCH(name)

static bool missingInput, nullCreated, nullMerged, failedDispatch;
static int publications, merges, invalidReads;
static bool lastCopyInactive;
static std::array<const void*, 3> mergedPointers;
inline void voidApi(const char* name) {
    if (!operation(name))
        throw std::runtime_error("injected native failure");
}

namespace boost {
using std::bind;
using std::regex;
using std::regex_replace;
struct bad_lexical_cast : std::exception {};
template <typename T> T lexical_cast(const std::string& text) {
    std::istringstream stream(text);
    T value;
    if (!(stream >> value) || stream.peek() != std::char_traits<char>::eof())
        throw bad_lexical_cast();
    return value;
}
inline std::string is_any_of(const char* delimiters) {
    return delimiters;
}
inline void split(std::vector<std::string>& out, const std::string& text,
                  const std::string& delimiters) {
    out.clear();
    std::size_t begin = 0;
    for (;;) {
        const auto end = text.find_first_of(delimiters, begin);
        out.push_back(text.substr(begin, end - begin));
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
}
} // namespace boost
using std::placeholders::_1;

enum class DVdbGridType { kFloat = 1, kDouble = 2, kInt32 = 3, kInt64 = 4, kBool = 5, kVec3f = 6, kInvalid = 0 };
namespace openvdb {
enum VecType { VEC_INVARIANT, V1, V2, V3, V4 };
const int NUM_VEC_TYPES = 5;
namespace util {
struct NullInterrupter {};
} // namespace util
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    virtual ~GridBase() = default;
    std::string name;
    DVdbGridType kind = DVdbGridType::kInvalid;
    VecType vectorType = VEC_INVARIANT;
    std::array<long double, 3> values{{0, 0, 0}};
    template <typename T> bool isType() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    const std::string& getName() const {
        return name;
    }
    void setName(const std::string& value) {
        voidApi("grid.name");
        name = value;
    }
    void setVectorType(VecType value) {
        voidApi("grid.vectorType");
        vectorType = value;
    }
};
template <typename T, DVdbGridType Kind> struct TypedGrid : GridBase {
    using TreeType = T;
    T sample{};
    TypedGrid() {
        kind = Kind;
    }
    const T& tree() const {
        return sample;
    }
};
using FloatGrid = TypedGrid<float, DVdbGridType::kFloat>;
using DoubleGrid = TypedGrid<double, DVdbGridType::kDouble>;
using Int32Grid = TypedGrid<int32_t, DVdbGridType::kInt32>;
using Int64Grid = TypedGrid<int64_t, DVdbGridType::kInt64>;
using BoolGrid = TypedGrid<bool, DVdbGridType::kBool>;
} // namespace openvdb
using Grid = openvdb::GridBase;
using GridCRef = const Grid&;
using GridPtr = Grid::Ptr;
using GridCPtr = Grid::ConstPtr;
using GridCPtrVec = std::vector<GridCPtr>;
inline DVdbGridType getVdbGridType(const Grid& grid) {
    return grid.kind;
}

struct DOpenVDBData {
    GridCPtrVec grids;
    bool hasValidData() const {
        return std::all_of(grids.begin(), grids.end(),
                           [](const GridCPtr& grid) { return bool(grid); });
    }
    std::size_t numberOfGrids() const {
        return grids.size();
    }
    const Grid& grid(std::size_t index) const {
        return *grids.at(index);
    }
    GridCPtr gridPtr(std::size_t index) const {
        return grids.at(index);
    }
    void insert(const GridCPtrVec& values) {
        voidApi("data.insert");
        grids = values;
    }
};
static DOpenVDBData inputData, stagedData, outputData;

struct InputValue {
    int number = 0;
    std::string text;
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
    bool asBool() const {
        check();
        return value.number != 0;
    }
    short asShort() const {
        check();
        return short(value.number);
    }
    MString asString() const {
        check();
        return MString(value.text.c_str());
    }
};
struct MPlug {
    MObject value;
    explicit MPlug(const MObject& object) : value(object) {}
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

template <typename ScalarGridT> void readMergeTrees(const Grid* mInGrid[3]);
// The native merger is the explicit boundary. Only its actual tree-binding
// prefix is executed by readMergeTrees; the voxel algorithm is NOT emulated.
struct ScalarGridMerger {
    const Grid* sources[3];
    GridPtr result;
    template <typename Warn>
    ScalarGridMerger(const Grid* x, const Grid* y, const Grid* z, const std::string&, bool inactive,
                     Warn, openvdb::util::NullInterrupter*)
        : sources{x, y, z} {
        lastCopyInactive = inactive;
    }
    template <typename T> void operator()(const T&) {
        voidApi("native.merge");
        ++merges;
        readMergeTrees<T>(sources);
        if (nullMerged)
            return;
        result = std::make_shared<Grid>();
        for (int axis = 0; axis < 3; ++axis) {
            mergedPointers[axis] = sources[axis];
            result->values[axis] = static_cast<const T*>(sources[axis])->tree();
        }
    }
    GridPtr getGrid() const {
        return result;
    }
};
template <typename T> bool dispatch(const Grid& grid, ScalarGridMerger& merger) {
    if (!grid.isType<T>())
        return false;
    merger.operator()<T>(static_cast<const T&>(grid));
    return true;
}
inline bool processVdbScalarGrid(DVdbGridType type, const Grid& grid, ScalarGridMerger& merger) {
    if (failedDispatch)
        return false;
    switch (type) {
    case DVdbGridType::kFloat:
        return dispatch<openvdb::FloatGrid>(grid, merger);
    case DVdbGridType::kDouble:
        return dispatch<openvdb::DoubleGrid>(grid, merger);
    case DVdbGridType::kInt32:
        return dispatch<openvdb::Int32Grid>(grid, merger);
    case DVdbGridType::kInt64:
        return dispatch<openvdb::Int64Grid>(grid, merger);
    default:
        return false;
    }
}
