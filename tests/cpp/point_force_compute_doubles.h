#pragma once
// Reuse failure-injectable ownership/DG boundaries. Force kernels, budgets and
// typed attribute conversions are production code; native query results are not.
#include "direction_search_compute_doubles.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DGoalMath.h"
#include "common/utility/DPointPayloadBudget.h"
using DScalarData=float;
using DScalarDataList=std::vector<float>;
using float3=MDataHandle::Float3;
using float2=MDataHandle::Float2;
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
inline void EchoWarning(const char*,const char*){}
inline std::size_t DGetGlobalProcessNum(std::size_t){return 1;}
inline bool isSingleRealAttrType(DP_ATTR_DATA_TYPE type){
    return type==DP_ATTR_INT||type==DP_ATTR_UINT32||type==DP_ATTR_INT64||type==DP_ATTR_FLOAT||type==DP_ATTR_DOUBLE;
}
inline bool getCheckedDScalarArrayFromAttribute(const DPData& data,const std::string& name,
                                               std::size_t count,DScalarDataList& values){
    return dommy::attribute::convertScalarAttribute(data.getAttr(name),count,values);
}
namespace dommy {namespace maya {
struct MeshClosestPointQuery {MStatus create(MObject){return operation("native.mesh.create");}};
}}
struct NativeForceOp {
    DPointList* points;DScalarDataList* m_radius=nullptr;bool noTargets=false;
    template<typename Params>NativeForceOp(const Params&,DPointList& p,const DScalarDataList&):points(&p){}
    template<typename Params>NativeForceOp(const Params&,DPointList& p,const DScalarDataList&,
                                           const std::vector<MObject>& objects):points(&p),noTargets(objects.empty()){}
    template<typename Params>NativeForceOp(const Params&,const dommy::maya::MeshClosestPointQuery&,
                                           DPointList& p,const DScalarDataList&):points(&p){}
    MStatus operator()(const tbb::blocked_range<size_t>& range){
        if(noTargets)return MS::kSuccess;
        const auto status=operation("native.force.query");if(!status)return status;
        if(rejectNative)return MS::kFailure;++nativeQueries;
        for(std::size_t i=range.begin();i<range.end();++i)(*points)[i][1]+=1;
        return MS::kSuccess;
    }
};
using AvoidCurveOp=NativeForceOp;using AvoidSurfaceOp=NativeForceOp;
using AvoidMeshOp=NativeForceOp;using AvoidDCOp=NativeForceOp;
using MagnetCurveOp=NativeForceOp;using MagnetSurfaceOp=NativeForceOp;
using MagnetMeshOp=NativeForceOp;using MagnetDCOp=NativeForceOp;
inline MStatus generateMCurveFromDcurve(const std::shared_ptr<Primitive>&,MObject& output){
    const auto status=operation("native.curve.generate");if(status)output={"curve"};return status;
}
struct MFnNurbsCurveData {
    MObject create(MStatus* status){*status=operation("native.curve.data");return *status?MObject{"curve"}:MObject::kNullObj;}
};
struct MFnNurbsCurve {
    enum Form{kOpen,kClosed};
    void create(const DPointList&,const std::vector<double>&,int,Form,bool,bool,MObject&,MStatus* status){
        *status=operation("native.curve.create");
    }
};
