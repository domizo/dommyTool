#include "common/utility/DGenerationLimits.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "curve_conversion_plans.inc"

int main(){
    using namespace dommy::curve;
    using namespace dommy::generation;
    CurveConversionPlan plan;
    assert(initializeCurveConversionPlan(2,1,4,plan));
    assert(admitCurveConversionCounts(4,6,plan));
    assert(admitCurveConversionCounts(2,2,plan));
    assert(plan.points==6 && plan.knots==8 && plan.countedCurves==2 && plan.largestCurve==4);
    assert(plan.estimatedBytes==2*2048+6*96+4*512+4*128+8*32+2*4+576);
    const auto bytes=plan.estimatedBytes;
    assert(!admitCurveConversionCounts(2,2,plan) && plan.estimatedBytes==bytes);
    for(const auto count:{std::size_t(100000),std::numeric_limits<std::size_t>::max()})
        assert(!initializeCurveConversionPlan(count,count,count,plan) && plan.estimatedBytes==bytes);
    assert(!initializeCurveConversionPlan(2,3,2,plan));
    assert(!initializeCurveConversionPlan(2,0,1,plan));
    assert(!initializeCurveConversionPlan(0,0,kMaxConnectedGeometryCount+1,plan));
    assert(initializeCurveConversionPlan(1,0,1,plan));
    const auto empty=plan.estimatedBytes;
    assert(!admitCurveConversionCounts(2000000,2000002,plan) && plan.estimatedBytes==empty);
    assert(!admitCurveConversionCounts(-1,2,plan));
    assert(!admitCurveConversionCounts(2,-1,plan));
    assert(!admitCurveConversionCounts(2,1,plan));
    assert(!admitCurveConversionCounts(2,4,plan));
    assert(initializeCurveConversionPlan(2,0,2,plan));
    assert(admitCurveConversionCounts(300000,300002,plan));
    const auto first=plan;
    assert(!admitCurveConversionCounts(1000000,1000002,plan));
    assert(plan.estimatedBytes==first.estimatedBytes && plan.points==first.points && plan.countedCurves==1);
    // Group and collection-only preflight runs before native construction.
    std::size_t low=0,high=100000;
    while(low<high){const auto mid=low+(high-low+1)/2;if(initializeCurveConversionPlan(mid,mid,mid,plan))low=mid;else high=mid-1;}
    assert(low>0 && low<10000 && initializeCurveConversionPlan(low,low,low,plan));
    assert(!initializeCurveConversionPlan(low+1,low+1,low+1,plan));
    CurveConversionPlan excessive;excessive.curves=10001;excessive.groups=10000;excessive.inputSlots=10001;
    std::size_t unchanged=17;
    assert(!curveConversionStorage(excessive,unchanged) && unchanged==17);

    SpiralCurvePlan spiral;
    assert(buildSpiralCurvePlan(50,spiral));
    assert(spiral.samples==52 && spiral.knots==54 && spiral.estimatedBytes==4096+52*256+54*32);
    const auto original=spiral;
    for(int bad:{-1,0,1,9999996,9999997,9999998,std::numeric_limits<int>::max()})
        assert(!buildSpiralCurvePlan(bad,spiral) && spiral.samples==original.samples && spiral.estimatedBytes==original.estimatedBytes);
    assert(buildSpiralCurvePlan(2,spiral) && spiral.samples==4 && spiral.knots==6);
    const std::size_t maxSamples=(kMaxCurveGenerationBytes-4096-64)/288;
    assert(buildSpiralCurvePlan(static_cast<int>(maxSamples-2),spiral));
    assert(spiral.samples==maxSamples && spiral.estimatedBytes<=kMaxCurveGenerationBytes);
    assert(!buildSpiralCurvePlan(static_cast<int>(maxSamples-1),spiral));
    std::cout<<"Curve conversion/Spiral plans: counts, exact charges, limits and transactional rejection passed\n";
}
