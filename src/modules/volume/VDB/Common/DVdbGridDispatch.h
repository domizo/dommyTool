#pragma once

// Project-owned dispatch built on the public OpenVDB grid API. This file does
// not contain the historical UT helper implementation or require the HDK.
#include <openvdb/openvdb.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/points/PointDataGrid.h>

#include <cstddef>
#include <typeinfo>
#include <utility>

// Preserve the values used by existing callers; these are not Maya attributes.
enum class DVdbGridType {
    kInvalid = 0, kFloat = 1, kDouble = 2, kInt32 = 3, kInt64 = 4,
    kBool = 5, kVec3f = 6, kVec3d = 7, kVec3i = 8,
    kPointIndex = 9, kPointData = 10
};

template<class Grid> const Grid* vdbGridCast(const openvdb::GridBase* value)
{
    return value && value->template isType<Grid>() ? dynamic_cast<const Grid*>(value) : nullptr;
}
template<class Grid> Grid* vdbGridCast(openvdb::GridBase* value)
{
    return value && value->template isType<Grid>() ? dynamic_cast<Grid*>(value) : nullptr;
}
template<class Grid> const Grid& vdbGridCast(const openvdb::GridBase& value)
{
    const Grid* result = vdbGridCast<Grid>(&value);
    if (!result) throw std::bad_cast();
    return *result;
}
template<class Grid> Grid& vdbGridCast(openvdb::GridBase& value)
{
    Grid* result = vdbGridCast<Grid>(&value);
    if (!result) throw std::bad_cast();
    return *result;
}
template<class Grid> typename Grid::ConstPtr vdbGridCast(openvdb::GridBase::ConstPtr value)
{
    return vdbGridCast<Grid>(value.get()) ? openvdb::gridConstPtrCast<Grid>(value) :
                                          typename Grid::ConstPtr();
}
template<class Grid> typename Grid::Ptr vdbGridCast(openvdb::GridBase::Ptr value)
{
    return vdbGridCast<Grid>(value.get()) ? openvdb::gridPtrCast<Grid>(value) :
                                          typename Grid::Ptr();
}

namespace dommy { namespace vdb_dispatch {

inline const openvdb::GridBase* address(const openvdb::GridBase& value) { return &value; }
inline const openvdb::GridBase* address(const openvdb::GridBase* value) { return value; }
inline const openvdb::GridBase* address(const openvdb::GridBase::Ptr& value) { return value.get(); }
inline const openvdb::GridBase* address(const openvdb::GridBase::ConstPtr& value) { return value.get(); }

template<class Grid> struct Kind;
#define DOMMY_GRID_KIND(Grid, Value) \
    template<> struct Kind<Grid> { static constexpr DVdbGridType value = DVdbGridType::Value; }
DOMMY_GRID_KIND(openvdb::FloatGrid, kFloat);
DOMMY_GRID_KIND(openvdb::DoubleGrid, kDouble);
DOMMY_GRID_KIND(openvdb::Int32Grid, kInt32);
DOMMY_GRID_KIND(openvdb::Int64Grid, kInt64);
DOMMY_GRID_KIND(openvdb::BoolGrid, kBool);
DOMMY_GRID_KIND(openvdb::Vec3SGrid, kVec3f);
DOMMY_GRID_KIND(openvdb::Vec3DGrid, kVec3d);
DOMMY_GRID_KIND(openvdb::Vec3IGrid, kVec3i);
DOMMY_GRID_KIND(openvdb::tools::PointIndexGrid, kPointIndex);
DOMMY_GRID_KIND(openvdb::points::PointDataGrid, kPointData);
#undef DOMMY_GRID_KIND

// Each family is a compile-time list: a scalar-only operation is never
// instantiated for vector, boolean or point-data types it cannot handle.
template<class... Grids> struct Family;
template<> struct Family<> {
    static DVdbGridType identify(const openvdb::GridBase&) { return DVdbGridType::kInvalid; }
    template<class View, class Operation>
    static bool apply(DVdbGridType, View&&, Operation&) { return false; }
};
template<class First, class... Rest> struct Family<First, Rest...> {
    static DVdbGridType identify(const openvdb::GridBase& value)
    {
        return vdbGridCast<First>(&value) ? Kind<First>::value : Family<Rest...>::identify(value);
    }
    template<class View, class Operation>
    static bool apply(DVdbGridType kind, View&& value, Operation& operation)
    {
        if (kind != Kind<First>::value)
            return Family<Rest...>::apply(kind, std::forward<View>(value), operation);
        const openvdb::GridBase* base = address(value);
        if (!vdbGridCast<First>(base)) return false;
        operation.template operator()<First>(vdbGridCast<First>(std::forward<View>(value)));
        return true;
    }
};

using Scalar = Family<openvdb::FloatGrid, openvdb::DoubleGrid, openvdb::Int32Grid, openvdb::Int64Grid>;
using Real = Family<openvdb::FloatGrid, openvdb::DoubleGrid>;
using Vector = Family<openvdb::Vec3SGrid, openvdb::Vec3DGrid, openvdb::Vec3IGrid>;
using RealVector = Family<openvdb::Vec3SGrid, openvdb::Vec3DGrid>;
using Point = Family<openvdb::tools::PointIndexGrid, openvdb::points::PointDataGrid>;
using Numeric = Family<openvdb::FloatGrid, openvdb::DoubleGrid, openvdb::Int32Grid,
    openvdb::Int64Grid, openvdb::Vec3SGrid, openvdb::Vec3DGrid, openvdb::Vec3IGrid>;
using Topology = Family<openvdb::FloatGrid, openvdb::DoubleGrid, openvdb::Int32Grid,
    openvdb::Int64Grid, openvdb::Vec3SGrid, openvdb::Vec3DGrid, openvdb::Vec3IGrid, openvdb::BoolGrid>;
using All = Family<openvdb::FloatGrid, openvdb::DoubleGrid, openvdb::Int32Grid,
    openvdb::Int64Grid, openvdb::BoolGrid, openvdb::Vec3SGrid, openvdb::Vec3DGrid,
    openvdb::Vec3IGrid, openvdb::tools::PointIndexGrid, openvdb::points::PointDataGrid>;

} } // namespace dommy::vdb_dispatch

inline DVdbGridType getVdbGridType(const openvdb::GridBase& grid)
{
    return dommy::vdb_dispatch::All::identify(grid);
}
inline const char* getVdbGridTypeName(const openvdb::GridBase& grid)
{
    const char* const names[] = {"invalid type", "float", "double", "int32", "int64",
        "bool", "Vec3f", "Vec3d", "Vec3i", "PointIndex", "PointData"};
    return names[static_cast<std::size_t>(getVdbGridType(grid))];
}
inline int getVdbGridTupleSize(DVdbGridType type)
{
    const int index = static_cast<int>(type);
    return index >= 1 && index <= 5 ? 1 : (index >= 6 && index <= 8 ? 3 : 0);
}
inline int getVdbGridTupleSize(const openvdb::GridBase& grid)
{
    return getVdbGridTupleSize(getVdbGridType(grid));
}

#define DOMMY_GRID_VISITOR(Function, FamilyName) \
    template<class View, class Operation> \
    bool Function(DVdbGridType kind, View&& value, Operation& operation) \
    { return dommy::vdb_dispatch::FamilyName::apply(kind, std::forward<View>(value), operation); }
DOMMY_GRID_VISITOR(processVdbGrid, Numeric)
DOMMY_GRID_VISITOR(processVdbTopologyGrid, Topology)
DOMMY_GRID_VISITOR(processVdbScalarGrid, Scalar)
DOMMY_GRID_VISITOR(processVdbRealGrid, Real)
DOMMY_GRID_VISITOR(processVdbVectorGrid, Vector)
DOMMY_GRID_VISITOR(processVdbRealVectorGrid, RealVector)
DOMMY_GRID_VISITOR(processVdbPointGrid, Point)
#undef DOMMY_GRID_VISITOR
