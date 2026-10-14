#pragma once
// Compile-only declarations for non-Maya dependencies. Mesh, array, attribute,
// DG handle, matrix and iterator declarations come from the installed SDK.
#include <maya/MPxNode.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPlug.h>
#include <maya/MFloatPointArray.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MMatrixArray.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MIntArray.h>
#include <maya/MFnMesh.h>
#include <maya/MItMeshVertex.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DGenerationLimits.h"
#include <ImathVec.h>
#include <map>
#include <memory>
#include <string>
#include <cstdint>
#include <functional>
using DPoint=Imath::V3f;
using DPointList=std::vector<DPoint>;
using DDoubleArray=std::vector<double>;
using IDs=std::vector<uint32_t>;
using IDsListArray=std::vector<IDs>;
using DScalarList=std::vector<double>;
using DScalarListArray=std::vector<DScalarList>;
struct Group {const std::vector<bool>& getGroupIds()const;};
struct DPData {
    static const MTypeId id;
    std::size_t metadataBytes;
    using Groups=std::map<std::string,std::shared_ptr<Group>>;
    Groups::const_iterator beginGrp()const;
    Groups::const_iterator endGrp()const;
    std::size_t numberOfPoints()const;
    bool hasValidPointTopology()const;
    const DPointList& getPositionConst()const;
    void setDPData(const DPData&);
    using PointCopyOperation=std::function<MStatus(DPoint&,std::size_t,std::size_t)>;
    MStatus copyRepeatedFrom(const DPData&,std::size_t,const PointCopyOperation&);
    bool replacePositions(const DPointList&);
    void updateBBox();
};
const MTypeId DPData::id(1);
const DPData* getInputDP(const MObject&,MDataBlock&);
MStatus getValidInputDP(const MObject&,MDataBlock&,const DPData*&);
MVector DPToMV(const DPoint&);
namespace dommy {namespace maya {
template<class T>bool validatePluginData(const T& p){return p.hasValidPointTopology();}
template<class T>MStatus createPluginData(MFnPluginData&,const MTypeId&,T*&);
template<class T>MStatus getPluginData(MFnPluginData&,T*&);
template<class T>MStatus getPluginDataFromObject(const MObject&,const MTypeId&,const T*&);
template<class T>MStatus getPluginDataFromHandle(const MDataHandle&,const MTypeId&,const T*&);
}}
namespace tbb {
template<class T>struct blocked_range {blocked_range(T,T,T=1);};
template<class Range,class Op>void parallel_for(const Range&,const Op&);
}
namespace nanoflann {struct KDTreeSingleIndexAdaptorParams{explicit KDTreeSingleIndexAdaptorParams(int);};}
template<class T>struct PointCloud {struct Point {T x,y,z;};std::vector<Point> pts;};
void CreateSearchDataMFP(const MFloatPointArray&,PointCloud<double>&);
struct DPkdtree {
    DPkdtree(int,const PointCloud<double>&,nanoflann::KDTreeSingleIndexAdaptorParams);
    void buildIndex();
};
struct PointSearchKDTreeRadiusWithDistanceOp {
    PointSearchKDTreeRadiusWithDistanceOp(const DPkdtree&,const DPointList&,int,double,IDsListArray&,DScalarListArray&);
    void operator()(const tbb::blocked_range<std::size_t>&)const;
};
