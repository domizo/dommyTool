#include "common/utility/DNumericValidation.h"
#include <maya/MStatus.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
struct MFnNurbsCurve {
    double findParamFromLength(double value,MStatus* status)const{*status=MS::kSuccess;return value;}
};
#include "curve_periodic.inc"

int main(){
    double value=91,u=91,v=91;
    assert(sampleCurve(.25f,.5f,.125f,0,1,2,value)==MS::kSuccess && value==.875);
    for(float offset:{std::ldexp(1.0f,60),-std::ldexp(1.0f,60)}){
        assert(sampleCurve(.25,offset,0,0,1,2,value)==MS::kSuccess && value==.25);
        assert(sampleCurve(.75,offset,.125f,.5,3.5,2,value)==MS::kSuccess);
        assert(value==(offset>0?1.875:2.875));
        assert(sampleSurface(.75,-1.75,offset,-offset,.125f,.5,3.5,-2,1,2,u,v)==MS::kSuccess);
        assert(u==(offset>0?1.875:2.875));
        assert(v==(offset>0?.375:-.625));
    }
    assert(sampleCurve(.25f,0,.125f,0,1,0,value)==MS::kSuccess && value==.375);
    assert(sampleCurve(.25f,2,0,0,1,1,value)==MS::kSuccess && value==1);
    std::cout<<"Link curve/surface ordinary and original-offset periodic cases passed\n";
}
