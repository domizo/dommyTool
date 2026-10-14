#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

static MStatus apiStatus = MS::kSuccess;
static int readAt = -1, calls = 0;
static float tracking[2] = {0.25f,-0.5f};
static bool nullTracking=false;
struct MString {const char* value;const char* asChar()const{return value;}};
struct MObject {int index;};
struct MDataHandle {
    bool valid;int value;int index;
    int asInt()const {assert(valid);return index==3 ? 4 : value;}
    short asShort()const {assert(valid);return static_cast<short>(value);}
    float asFloat()const {assert(valid);return 1.0f;}
    bool asBool()const {assert(valid);return true;}
    MString asString()const {assert(valid);return {index==0 ? "AB" : index==1 ? "font" : "piece"};}
    const float* asFloat2()const {assert(valid);return nullTracking ? nullptr : tracking;}
};
struct MDataBlock {
    MDataHandle inputValue(MObject object, MStatus* status) {
        *status = calls++ == readAt ? apiStatus : MS::kSuccess;
        if(object.index==4){tracking[0]=0.25f;tracking[1]=-0.5f;}
        if(object.index>4){tracking[0]=91;tracking[1]=92;}
        return {*status == MS::kSuccess, object.index == 0 ? 10 : object.index == 1 ? 2 : 1, object.index};
    }
};
struct MArrayDataHandle {
    unsigned elementCount(MStatus* status=nullptr) {*status=apiStatus;return 3;}
};
constexpr double kMFnNurbsEpsilon = 1e-3;
struct MFnNurbsCurve {
    double length(double,MStatus* status=nullptr) {*status=apiStatus;return 7.0;}
};
struct CountArray {std::size_t count;std::size_t length()const{return count;}};
struct MRenderLine {std::size_t count;CountArray getLine()const{return {count};}};
struct MRenderLineArray {
    std::vector<std::size_t> counts;
    int fail = -1;
    int length()const{return static_cast<int>(counts.size());}
    MRenderLine renderLine(unsigned index,MStatus* status)const {
        *status=static_cast<int>(index)==fail ? MS::kFailure : MS::kSuccess;
        return {counts.at(index)};
    }
};
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "curve_source_plans.inc"

template<class Predicate> std::size_t lastAdmitted(std::size_t maximum, Predicate check) {
    std::size_t lo=0,hi=maximum;
    while(lo<hi) {const auto mid=lo+(hi-lo+1)/2;if(check(mid))lo=mid;else hi=mid-1;}
    assert(check(lo) && !check(lo+1));return lo;
}
int main() {
    using namespace dommy::curve;
    using namespace dommy::generation;
    CurveFromCurvePlan curve;
    assert(makeCurveFromCurvePlan(0,8,1,1,1.0,3,{4,5},curve));
    assert(curve.outputCurveCount==6 && curve.outputPointCount==48);
    assert(curve.estimatedBytes==6*2048+48*96+8*512+9*256+2*2112);
    const auto original=curve.estimatedBytes;
    assert(!makeCurveFromCurvePlan(0,10000000,0,0,1,1,{2},curve));
    assert(curve.estimatedBytes==original && curve.outputPointCount==48);
    assert(makeCurveFromCurvePlan(1,8,2,1,1,2,{4,5},curve));
    assert((curve.samplingPerSourceCurve==std::vector<std::size_t>{10,13}) && curve.outputPointCount==46);
    assert(!makeCurveFromCurvePlan(0,2,0,0,1,1,{2000000},curve));
    assert(!makeCurveFromCurvePlan(0,2,0,0,1,1,{std::numeric_limits<std::size_t>::max()},curve));
    lastAdmitted(10000000,[&](std::size_t n){return n<2 || makeCurveFromCurvePlan(0,static_cast<int>(n),0,0,1,1,{2},curve);});

    LinkGenerationPlan link;
    assert(checkedLinkGenerationPlan(3,10,2,link));
    assert(link.curveCount==10 && link.pointsPerCurve==7 && link.outputPointCount==70);
    assert(link.estimatedBytes==10*2048+70*96+7*512+30*64+10*64+3*128);
    const auto old=link.estimatedBytes;
    assert(!checkedLinkGenerationPlan(2,1000000,0,link) && link.estimatedBytes==old);
    assert(!checkedLinkGenerationPlan(1,10,0,link));
    assert(!checkedLinkGenerationPlan(2,-1,0,link));
    assert(!checkedLinkGenerationPlan(2,1,-1,link));
    lastAdmitted(1000000,[&](std::size_t n){return n==0 || checkedLinkGenerationPlan(2,static_cast<int>(n),0,link);});
    std::size_t bytes=17;
    assert(checkedMeshCurveStorage(4,4,bytes));
    assert(bytes==4*2048+8*96+2*512+4*64);
    bytes=17;
    assert(!checkedMeshCurveStorage(1000000,1000000,bytes) && bytes==17);
    assert(!checkedMeshCurveStorage(10000000,0,bytes) && bytes==17);

    PfxExportOptions options{3,true,true,true,true,true,true,true};
    PfxGenerationBudget pfx;
    assert(reserveLineBudget(4,options,pfx)==MS::kSuccess);
    assert(pfx.estimatedBytes==2048+4*96+4*512+4*(3*32+4*48)+7*1024);
    const auto pfxBytes=pfx.estimatedBytes;
    assert(reserveLineBudget(10000000,options,pfx)!=MS::kSuccess);
    assert(pfx.estimatedBytes==pfxBytes && pfx.curveCount==1 && pfx.pointCount==4);
    MRenderLineArray lines;lines.counts={0,1,2,4};
    PfxGenerationBudget scanned;
    assert(preflightLines(lines,options,scanned)==MS::kSuccess && scanned.curveCount==2 && scanned.pointCount==6);
    lines.fail=2;
    assert(preflightLines(lines,options,scanned)==MS::kFailure);
    lines.fail=-1;lines.counts={2,10000000};
    assert(preflightLines(lines,options,scanned)!=MS::kSuccess);
    MRenderLineArray mainLines,leafLines,flowerLines;
    mainLines.counts={2};leafLines.counts={4};flowerLines.counts={6};
    for(int flags=0;flags<8;++flags){
        PfxGenerationBudget planned;
        assert(pfxAdmission(mainLines,leafLines,flowerLines,flags&1,flags&2,flags&4,options,planned)==MS::kSuccess);
        assert(planned.curveCount==static_cast<std::size_t>(!!(flags&1)+!!(flags&2)+!!(flags&4)));
        assert(planned.pointCount==static_cast<std::size_t>((flags&1 ? 2 : 0)+(flags&2 ? 4 : 0)+(flags&4 ? 6 : 0)));
    }
    leafLines.counts={10000000};PfxGenerationBudget planned;planned.curveCount=19;
    assert(pfxAdmission(mainLines,leafLines,flowerLines,true,true,true,options,planned)!=MS::kSuccess && planned.curveCount==19);
    assert(pfxAdmission(mainLines,leafLines,flowerLines,true,false,true,options,planned)==MS::kSuccess && planned.pointCount==8);

    TextOutputBudget text;
    assert(initializeTextOutputBudget(3,3,5,true,text));
    assert(text.estimatedBytes==300);
    assert(reserveTextGroup(text));
    assert(commitTextCurve(text,4));
    assert(text.estimatedBytes==2048+4*96+4*512+300+4+512+4*15+32);
    assert(text.curves==1 && text.points==4 && text.groups==1);
    assert(reserveTextGroup(text));assert(commitTextCurve(text,7));
    assert(text.curves==2 && text.points==11 && text.groups==2 && text.largestCurve==7);
    bytes=text.estimatedBytes;
    assert(!commitTextCurve(text,10000000) && text.estimatedBytes==bytes && text.curves==2);
    assert(!textOutputStorage(text,20000,80000,4,20000,bytes));
    assert(initializeTextOutputBudget(20000,20000,5,false,text));
    assert(reserveTextGroup(text) && text.groups==0);
    assert(textOutputStorage(text,20000,80000,4,0,bytes));
    TextOutputBudget boundary;
    assert(initializeTextOutputBudget(0,0,0,false,boundary));
    assert(admitTextCurvePoints(boundary,2,bytes));
    boundary.baseBytes=kMaxCurveGenerationBytes-bytes;
    assert(commitTextCurve(boundary,2) && boundary.estimatedBytes==kMaxCurveGenerationBytes);
    assert(!reserveTextGroup(boundary) || !boundary.createGroups);
    assert(!commitTextCurve(boundary,2));
    assert(!initializeTextOutputBudget(std::numeric_limits<std::size_t>::max(),0,0,false,text));

    MDataBlock block;
    for(auto failure:{MS::kFailure,MS::kInvalidParameter}) {
        apiStatus=failure;
        for(readAt=0;readAt<3;++readAt) {
            calls=0;int c=-1,s=-1;short d=-1;
            assert(linkControls(block,c,s,d)==failure && calls==readAt+1 && c==-1 && s==-1 && d==-1);
        }
        MArrayDataHandle array;bytes=19;
        assert(linkCount(array,bytes)==failure && bytes==19);
        MFnNurbsCurve function;double length=19;
        assert(linkLength(function,length)==failure && length==19);
        for(readAt=0;readAt<7;++readAt){
            calls=0;TextGenerationPlan textPlan;textPlan.sampling=99;
            assert(textControls(block,textPlan)==failure && textPlan.sampling==99 && calls==readAt+1);
        }
    }
    readAt=-1;calls=0;apiStatus=MS::kSuccess;int c=0,s=0;short d=0;
    assert(linkControls(block,c,s,d)==MS::kSuccess && c==10 && s==2 && d==1);
    MArrayDataHandle array;assert(linkCount(array,bytes)==MS::kSuccess && bytes==3);
    MFnNurbsCurve function;double length=0;assert(linkLength(function,length)==MS::kSuccess && length==7);
    TextGenerationPlan textPlan;
    assert(textControls(block,textPlan)==MS::kSuccess && textPlan.sampling==4 && textPlan.fontSize==1);
    assert(textPlan.trackingX==0.25 && textPlan.trackingY==-0.5);
    nullTracking=true;calls=0;textPlan.sampling=99;
    assert(textControls(block,textPlan)==MS::kFailure && textPlan.sampling==99 && calls==5);
    std::cout<<"Source curves: aggregate storage, group work, transactional plans, Pfx preflight, Link/Text input failures and owned tracking passed\n";
}
