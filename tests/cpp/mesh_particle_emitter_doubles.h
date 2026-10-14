#pragma once

#include "particle_output_doubles.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DPointMeanAccumulator.h"
#ifndef DOMMY_TEST_REPLACE_MESH_SAMPLING
#include "modules/particles/particle/emitter/DMeshEmitterSampling.h"
#endif
#include "noise/PerlinNoise.h"

// Share particle/mesh status and publication boundaries. Curves and surfaces
// are simple host-query fixtures, not native NURBS evaluation.
const MObject mSpeed{"mSpeed"};
using MFnAttribute=Attribute;
const double kMFnNurbsEpsilon=1e-6;
enum { D_RADIAL_EMIT_SOURCE=0, D_RADIAL_EMIT_DIRECTION=1 };
MVector operator*(const MVector& v,const MMatrix& m) {
    return MVector(v.x*m[0][0]+v.y*m[1][0]+v.z*m[2][0],
                   v.x*m[0][1]+v.y*m[1][1]+v.z*m[2][1],
                   v.x*m[0][2]+v.y*m[1][2]+v.z*m[2][2]);
}
struct MFnNurbsCurve {
    bool valid;
    MFnNurbsCurve(const MObject&,MStatus* s) { *s=operation("curve.bind"); valid=bool(*s); }
    int numCVs(MStatus* s) { assert(valid); *s=operation("curve.count"); return fixture.meshPoints.length(); }
    MStatus getCVs(MPointArray& p,MSpace::Space) {
        assert(valid); const MStatus s=operation("curve.cvs"); if(s)p=fixture.meshPoints; return s;
    }
    double length(double,MStatus* s) { assert(valid); *s=operation("curve.length"); return 6; }
    double findParamFromLength(double length,MStatus* s) { assert(valid); *s=operation("curve.param"); return length; }
    MStatus getPointAtParam(double u,MPoint& p,MSpace::Space) {
        assert(valid); const MStatus s=operation("curve.point"); if(s)p=MPoint(u,0,0); return s;
    }
};
struct MFnNurbsSurface {
    bool valid;
    MFnNurbsSurface(const MObject&,MStatus* s) { *s=operation("surface.bind"); valid=bool(*s); }
    int numCVsInU(MStatus* s) { assert(valid); *s=operation("surface.countU"); return 2; }
    int numCVsInV(MStatus* s) { assert(valid); *s=operation("surface.countV"); return 2; }
    MStatus getCVs(MPointArray& p,MSpace::Space) {
        assert(valid); const MStatus s=operation("surface.cvs"); if(s)p=fixture.meshPoints; return s;
    }
    MStatus getKnotDomain(double& a,double& b,double& c,double& d) {
        assert(valid); a=c=0; b=d=1; return operation("surface.domain");
    }
    MStatus getPointAtParam(double u,double v,MPoint& p,MSpace::Space) {
        assert(valid); const MStatus s=operation("surface.point"); if(s)p=MPoint(u,v,0); return s;
    }
};
