#pragma once
// Production dispatch, numeric kernels, cache construction and validation run
// unchanged. Native Maya curve queries, ramps and DG ownership are doubles.
#define DOMMY_AFFECT_SPIRAL_TESTS
#define MFnNurbsCurve UnusedForceCurve
#include "point_force_compute_doubles.h"
#undef MFnNurbsCurve
#include "math/DRemapMath.h"
#include "common/utility/DScaledProduct.h"
using DP_ATTRIBUTE_CLASS_TYPE=short;
const double kMFnNurbsEpsilon=1e-6;
static bool nanParameter=false,nanRamp=false,colorRamp=false,forbidWork=false;
static int rampSamples=0,tangentQueries=0;
struct RampPoint : Imath::V3d {
    double w=1.0;
    RampPoint(double x=0,double y=0,double z=0):Imath::V3d(x,y,z){}
    RampPoint(const Imath::V3d& v):Imath::V3d(v){}
    double distanceTo(const RampPoint& p)const{return (*this-p).length();}
};
#define MPoint RampPoint
inline MPoint DPToMP(const DPoint& p){return MPoint(p[0],p[1],p[2]);}
inline Imath::V3d operator^(const MVector& a,const MVector& b){return a.cross(b);}
struct MPlane {
    MVector normal;double offset=0;
    void setPlane(const MVector& n,double d){normal=n;offset=d;}
    double directedDistance(const MPoint& p)const{return normal.dot(p)+offset;}
};
struct SampleRamp {
    std::string attribute;
    SampleRamp()=default;
    SampleRamp(const MPlug& plug,MStatus* status):attribute(plug.attribute.name){
        *status=operation("ramp.bind "+attribute);
    }
    SampleRamp(MObject,const MObject& attr,MStatus* status):attribute(attr.name){
        *status=operation("ramp.bind "+attribute);
    }
    bool isCurveRamp(MStatus* status)const{
        *status=operation("ramp.type "+attribute);return !colorRamp;
    }
    void getValueAtPosition(float position,float& value,MStatus* status){
        assert(!forbidWork);++rampSamples;
        *status=operation("ramp.sample "+attribute);
        if(*status)value=nanRamp?NAN:position;
    }
};
#define MRampAttribute SampleRamp
struct MFnNurbsCurve {
    MFnNurbsCurve(MObject,MStatus* status){assert(!forbidWork);*status=operation("curve.bind");}
    double length(double,MStatus* status){*status=operation("curve.length");return 10;}
    MPoint closestPoint(const MPoint& p,double* parameter,double,MSpace::Space,MStatus* status){
        *status=operation("curve.closest");*parameter=p[1];return MPoint(0,p[1],0);
    }
    MStatus getParamAtPoint(const MPoint& p,double& parameter,double,MSpace::Space){
        const auto status=operation("curve.parameter");parameter=nanParameter?NAN:p[1];return status;
    }
    MVector tangent(double,MSpace::Space,MStatus* status){
        ++tangentQueries;*status=operation("curve.tangent");return MVector(0,1,0);
    }
    double findLengthFromParam(double parameter,MStatus* status){
        *status=operation("curve.partialLength");return parameter;
    }
};
struct NativeAffectOp {
    DScalarDataList& values;
    template<typename Params>NativeAffectOp(Params&,const DPointList&,DScalarDataList& v):values(v){}
    MStatus operator()(const tbb::blocked_range<std::size_t>& range){
        assert(!forbidWork);const auto status=operation("native.affect");if(!status)return status;
        ++nativeQueries;for(auto i=range.begin();i<range.end();++i)values[i]=0.25f;
        return MS::kSuccess;
    }
};
using AffectCurveOp=NativeAffectOp;
using AffectSurfaceOp=NativeAffectOp;
using AffectMeshOp=NativeAffectOp;
