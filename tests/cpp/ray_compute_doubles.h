#pragma once
// Native Maya query payloads and DG ownership are explicit doubles, not a
// geometry emulator. The complete ray kernels, dispatch and conversions run.
#define DOMMY_RAY_TESTS
#define MFnNurbsCurve UnusedForceCurve
#include "point_force_compute_doubles.h"
#undef MFnNurbsCurve
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMeshSelection.h"
using DPGroupData=Group;
struct RayPoint : Imath::V3d {
    double w=1.0;
    RayPoint(double x=0,double y=0,double z=0):Imath::V3d(x,y,z){}
    RayPoint(const Imath::V3d& p):Imath::V3d(p){}
    double distanceTo(const RayPoint& p)const{return (*this-p).length();}
};
#define MPoint RayPoint
inline MPoint operator+(const MPoint& a,const Imath::V3d& b){
    return MPoint(a[0]+b[0],a[1]+b[1],a[2]+b[2]);
}
using MFloatPoint=MPoint;
using MFloatVector=MVector;
using MPointArray=RayArray<MPoint>;
using MFloatPointArray=MPointArray;
using MDoubleArray=RayArray<double>;
using MFloatArray=RayArray<float>;
struct MMeshIsectAccelParams {};
const double kMFnMeshPointTolerance=1e-6;
static double queryTolerance=0;
static int rayQueries=0,closestQueries=0,acceleratorReleases=0;
static bool noHit=false,badNativePoint=false,badNativeArray=false;
static MPoint hit(const MPoint& p,double distance){
    return MPoint(p[0],badNativePoint?NAN:p[1]+distance,p[2]);
}
struct MFnMesh {
    MFnMesh(MObject object,MStatus* status){assert(!object.isNull());*status=operation("mesh.bind");}
    MMeshIsectAccelParams autoUniformGridParams(){voidApi("mesh.accelerator");return {};}
    void freeCachedIntersectionAccelerator(){++acceleratorReleases;}
    bool allIntersections(const MFloatPoint& p,const MFloatVector&,const MIntArray*,
        const MIntArray*,bool sortedIds,MSpace::Space space,float,bool both,
        MMeshIsectAccelParams*,bool sortedHits,MFloatPointArray& points,MFloatArray* params,
        MIntArray* faces,MIntArray*,MFloatArray*,MFloatArray*,float tolerance,MStatus* status){
        assert(sortedIds&&sortedHits&&!both&&space==MSpace::kWorld);
        queryTolerance=tolerance;++rayQueries;*status=operation("mesh.project");
        points={hit(p,2),hit(p,4)};*params={2,4};*faces={0,1};
        if(badNativeArray)params->clear();return !noHit;
    }
    bool intersect(const MPoint& p,const MVector&,MPointArray& points,double tolerance,
        MSpace::Space space,MIntArray* faces,MStatus* status){
        assert(space==MSpace::kWorld);queryTolerance=tolerance;
        ++rayQueries;*status=operation("mesh.direction");points={hit(p,2)};*faces={0};
        if(badNativeArray)faces->clear();return !noHit;
    }
    MStatus getPolygonNormal(int,MVector& normal,MSpace::Space space){
        assert(space==MSpace::kWorld);normal=MVector(0,1,0);return operation("mesh.normal");
    }
    MStatus getClosestPointAndNormal(const MPoint& p,MPoint& closest,MVector& normal,
        MSpace::Space space,int*,void*){
        assert(space==MSpace::kWorld);++closestQueries;
        closest=hit(p,1);normal=MVector(0,1,0);return operation("mesh.closest");
    }
};
struct MFnNurbsCurve {
    MFnNurbsCurve(MObject object,MStatus* status){assert(!object.isNull());*status=operation("curve.bind");}
    MPoint closestPoint(const MPoint& p,double* parameter,double tolerance,
        MSpace::Space space,MStatus* status){
        assert(space==MSpace::kWorld);queryTolerance=tolerance;++closestQueries;
        *status=operation("curve.closest");*parameter=0;return hit(p,1);
    }
    MVector normal(double,MSpace::Space space,MStatus* status){
        assert(space==MSpace::kWorld);*status=operation("curve.normal");return MVector(0,1,0);
    }
};
struct MFnNurbsSurface {
    MFnNurbsSurface(MObject object,MStatus* status){assert(!object.isNull());*status=operation("surface.bind");}
    MPoint closestPoint(const MPoint& p,double* u,double* v,bool ignore,double tolerance,
        MSpace::Space space,MStatus* status){
        assert(!ignore&&space==MSpace::kWorld);queryTolerance=tolerance;++closestQueries;
        *status=operation("surface.closest");*u=*v=0;return hit(p,1);
    }
    MVector normal(double,double,MSpace::Space space,MStatus* status){
        assert(space==MSpace::kWorld);*status=operation("surface.normal");return MVector(0,1,0);
    }
    bool intersect(const MPoint& p,const MVector&,MDoubleArray& u,MDoubleArray& v,
        MPointArray& points,double tolerance,MSpace::Space space,bool calculateDistance,
        MDoubleArray* distances,bool calculateExact,bool*,MStatus* status){
        assert(calculateDistance&&!calculateExact&&space==MSpace::kWorld);
        queryTolerance=tolerance;++rayQueries;*status=operation("surface.project");
        points={hit(p,4),hit(p,2)};u={0,1};v={0,1};*distances={4,2};
        if(badNativeArray)u.clear();return !noHit;
    }
};
