#pragma once
// Declarations only. The SDK check validates real Maya calls, not the OpenVDB
// declarations below or a full native plug-in translation unit/link.
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>
namespace openvdb {
using Index32=std::uint32_t;
using Int32=std::int32_t;
template<typename T>struct Vector3 {
    Vector3();Vector3(T,T,T);T x()const;T y()const;T z()const;
    T operator[](int)const;T& operator[](int);
};
using Vec3d=Vector3<double>;using Vec3s=Vector3<float>;using Vec3I=Vector3<Index32>;
struct Vec4I {Index32 operator[](int)const;};
struct Coord {int operator[](int)const;int& operator[](int);};
namespace math {
template<typename T>bool isApproxEqual(T,T);
struct Transform {
    std::shared_ptr<Transform> copy()const;bool isLinear()const;
    bool operator!=(const Transform&)const;Vec3d worldToIndex(const Vec3d&)const;Vec3d indexToWorld(const Vec3d&)const;
};
}
enum GridClass {GRID_LEVEL_SET};
struct GridBase {
    using ConstPtr=std::shared_ptr<const GridBase>;
    const math::Transform& transform()const;const std::string& getName()const;
    std::size_t activeVoxelCount()const;GridClass getGridClass()const;
};
template<typename T>struct Tree {
    using Ptr=std::shared_ptr<Tree>;
    template<typename U>struct ValueConverter {using Type=Tree<U>;};
    explicit Tree(T);template<typename Other>void topologyUnion(const Other&);
};
using BoolTree=Tree<bool>;
template<typename T>struct Grid:GridBase {
    using ValueType=T;using TreeType=Tree<T>;
    using Ptr=std::shared_ptr<Grid>;using ConstPtr=std::shared_ptr<const Grid>;
    template<typename U>struct ValueConverter {using Type=Grid<U>;};
    T background()const;const TreeType& tree()const;
};
using FloatGrid=Grid<float>;using DoubleGrid=Grid<double>;using BoolGrid=Grid<bool>;
template<typename G>typename G::ConstPtr gridConstPtrCast(const GridBase::ConstPtr&);
namespace tree {
template<typename T>struct LeafManager {explicit LeafManager(T&);};
template<typename T>struct ValueAccessor {explicit ValueAccessor(T&);bool isValueOn(const Coord&)const;};
}
namespace tools {
enum {POLYFLAG_EXTERIOR=1,POLYFLAG_FRACTURE_SEAM=2};
struct PolygonPool {
    std::size_t numQuads()const;std::size_t numTriangles()const;
    const Vec4I& quad(std::size_t)const;const Vec3I& triangle(std::size_t)const;
    int quadFlags(std::size_t)const;int triangleFlags(std::size_t)const;
};
struct VolumeToMesh {
    VolumeToMesh(float,float);template<typename G>void operator()(const G&);
    std::size_t pointListSize()const;std::size_t polygonPoolListSize()const;
    std::unique_ptr<Vec3s[]>& pointList();std::unique_ptr<PolygonPool[]>& polygonPoolList();
    const std::vector<char>& pointFlags()const;
    template<typename T>void setRefGrid(T,double);template<typename T>void setAdaptivityMask(T);
};
struct MeshToVoxelEdgeData {
    struct Accessor {};Accessor getAccessor();
    void convert(const std::vector<Vec3s>&,const std::vector<Vec4I>&);
    void getEdgeData(Accessor&,const Coord&,std::vector<Vec3d>&,std::vector<Index32>&);
};
Vec3d findFeaturePoint(const std::vector<Vec3d>&,const std::vector<Vec3d>&);
template<typename T>void pruneInactive(T&);template<typename T>void dilateVoxels(T&,int);
}
}
namespace boost {
template<typename T>using scoped_ptr=std::unique_ptr<T>;
namespace algorithm {std::string join(const std::vector<std::string>&,const char*);}
}
using GridCPtrVec=std::vector<openvdb::GridBase::ConstPtr>;
template<typename G,typename Op>bool processTypedScalarGrid(std::shared_ptr<const G> grid,Op& op) {
    op.template operator()<G>(grid);return true;
}
struct DOpenVDBData {static const MTypeId id;};
struct DMeshInternalData {
    static MStatus createFromMayaObject(const MObject&,std::shared_ptr<DMeshInternalData>&);
    std::size_t numberOfPoints()const;
    int numFace()const;
    std::size_t numberOfConnections()const;
    std::size_t numberOfUVs()const;
};
using DMItem=std::shared_ptr<DMeshInternalData>;
struct DMeshData:MPxData {
    static const MTypeId id;
    using DMeshList=std::vector<DMItem>;
    bool insert(const DMeshList&);
    bool hasValidMeshTopology()const;
    void updateBBox();
};
// meshPrimData's declaration is extracted from the production header by the
// runner. Do not replace its signatures with a separate SDK-only surrogate.
MStatus getPrimDataFromMesh(MFnMesh&,meshPrimData&);
MStatus DOpenVDBMakePointList(const MObject&,const openvdb::math::Transform&,std::vector<openvdb::Vec3s>&);
MStatus DOpenVDBMakePrimList(const MObject&,std::vector<openvdb::Vec4I>&,std::vector<int>* = nullptr);
void getGrids(GridCPtrVec&,const DOpenVDBData&,const std::string&);
MStatus getValidInputVDB(const MObject&,MDataBlock&,const DOpenVDBData*&);
template<typename I,typename B>struct GenAdaptivityMaskOp {
    GenAdaptivityMaskOp(const MFloatVectorArray&,const I&,openvdb::tree::LeafManager<B>&,float);void run(bool);
};
namespace dommy {namespace vdb {
struct TreeEditBudget {bool addBytes(std::size_t,std::size_t);bool addWork(std::size_t,std::size_t);};
struct MeshingBudget {TreeEditBudget resources;std::size_t points,faces,connects;bool addOutput(std::size_t,std::size_t,std::size_t);};
template<typename G>bool buildReferenceVolume(const std::vector<openvdb::Vec3s>&,
    const std::vector<openvdb::Vec4I>&,const openvdb::math::Transform&,bool,TreeEditBudget&,
    typename G::ConstPtr&,typename G::template ValueConverter<openvdb::Int32>::Type::Ptr&);
}}
MStatus preflightVdbMeshInput(const MObject&,dommy::vdb::TreeEditBudget&,std::size_t&,std::size_t&);
