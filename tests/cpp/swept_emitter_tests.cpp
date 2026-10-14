#include "swept_emitter_doubles.h"
#include "swept_emitter.inc"

#include <functional>

namespace {
enum Node { triangle, curve };
int evaluations=0, failures=0, exceptions=0;
const double nan=std::numeric_limits<double>::quiet_NaN();
const double inf=std::numeric_limits<double>::infinity();

void reset() {
    fixture=ParticleFixture(); swept=SweptFixture(); resetCalls();
    shortAt=-1; shortApplied=badAccesses=oversizedAllocations=0;
    injectedStatus=MS::kFailure;
    fixture.numbers={{"mIsFull",0},{"mCurrentTime",2},{"mStartTime",1},{"mDeltaTime",1},{"mRate",1},
        {"timeSampling",1},{"timeSamples",2},{"timeNoise",0},{"startPosition",0},{"endPosition",1},
        {"curveRadius",0},{"samplingArea",1},{"radiusNoise",0},{"rNoisefrequency",1},{"rNoiseScale",1}};
}
MStatus evaluate(Node node,int index=42,bool supported=true) {
    ++evaluations; MDataBlock data;
    const MPlug plug(supported ? mOutput : MObject{"other"},index);
    MStatus status;
    if (node==triangle) { DEmitter emitter; status=emitter.compute(plug,data); }
    else { curveEmitter emitter; status=emitter.compute(plug,data); }
    assert(badAccesses==0 && oversizedAllocations==0);
    return status;
}
void success(Node node,unsigned count) {
    assert(evaluate(node)==MS::kSuccess);
    assert(fixture.publications==1 && fixture.cleans==1 && fixture.outputIndex==42);
    const auto& arrays=fixture.published.vectors;
    if (count) assert(arrays.count("position") && arrays.count("velocity"));
    for (const auto& array:arrays) assert(array.second.length()==count);
    if (count) for (const auto& v:*arrays.at("velocity").values) assert(v==MVector::zero);
}
void reject(Node node) {
    assert(evaluate(node)==MS::kFailure);
    assert(fixture.publications==0 && fixture.cleans==0);
}
void positions(std::initializer_list<MVector> expected) {
    assert(*fixture.published.vectors.at("position").values==std::vector<MVector>(expected));
}
void goldens() {
    reset(); success(triangle,3);
    positions({MVector(4,5,6),MVector(3,4,5),MVector(2,3,4)});
    assert(fixture.inputIndex==42);
    reset(); fixture.numbers["timeSampling"]=0; fixture.numbers["mRate"]=0.25;
    success(triangle,1); positions({MVector(4,5,6)});
    reset(); fixture.numbers["timeSamples"]=0; success(triangle,1);
    reset(); swept.current.push_back(swept.current[0]); swept.previous.push_back(swept.previous[0]);
    success(triangle,6);
    positions({MVector(4,5,6),MVector(3,4,5),MVector(2,3,4),
               MVector(4,5,6),MVector(3,4,5),MVector(2,3,4)});

    reset(); success(curve,5);
    positions({MVector(0,0,0),MVector(3,6,9),MVector(6,12,18),MVector(9,18,27),MVector(12,24,36)});
    reset(); fixture.numbers["mRate"]=1.25; success(curve,10);
    positions({MVector(0,0,0),MVector(0,0,0),MVector(3,6,9),MVector(6,12,18),
               MVector(3,6,9),MVector(6,12,18),MVector(9,18,27),MVector(12,24,36),
               MVector(9,18,27),MVector(12,24,36)});
    reset(); fixture.numbers["startPosition"]=1; fixture.numbers["endPosition"]=0.5;
    success(curve,4);
    positions({MVector(3,6,9),MVector(6,12,18),MVector(9,18,27),MVector(12,24,36)});
    reset(); fixture.numbers["startPosition"]=1; success(curve,2);
    positions({MVector(9,18,27),MVector(12,24,36)});
    reset(); fixture.numbers["samplingArea"]=0; success(curve,3);
    positions({MVector(0,0,0),MVector(6,12,18),MVector(12,24,36)});
    reset(); swept.lines.resize(1); success(curve,1); positions({MVector(0,0,0)});

    // Jitter still uses the archived seed and distribution. Sample order and
    // untouched interpolation samples are independently checked above.
    for (Node node:{triangle,curve}) {
        reset(); fixture.numbers["timeNoise"]=0.25; fixture.numbers["curveRadius"]=2;
        fixture.numbers["radiusNoise"]=0.5;
        assert(evaluate(node)==MS::kSuccess);
        const auto first=*fixture.published.vectors.at("position").values;
        reset(); fixture.numbers["timeNoise"]=0.25; fixture.numbers["curveRadius"]=2;
        fixture.numbers["radiusNoise"]=0.5;
        assert(evaluate(node)==MS::kSuccess);
        assert(*fixture.published.vectors.at("position").values==first);
    }
}
void numericExtremes() {
    for (unsigned axis=0; axis<3; ++axis) {
        reset(); fixture.numbers["timeSampling"]=0;
        double a[3]={0,0,0}, b[3]={0,0,0}; a[axis]=1e308; b[axis]=-1e308;
        swept.current[0]={{MVector(a[0],a[1],a[2]),MVector(a[0],a[1],a[2]),MVector(b[0],b[1],b[2])}};
        success(triangle,1);
        const MVector center=fixture.published.vectors.at("position")[0];
        assert(std::isfinite(center[axis]) && std::fabs(center[axis]/(1e308/3)-1)<1e-15);

        // Accurate centers make the previously unreachable opposite-sign
        // center interpolation path relevant as well.
        reset(); fixture.numbers["timeSamples"]=1;
        swept.current[0].fill(MVector(a[0],a[1],a[2]));
        swept.previous[0].fill(MVector(b[0],b[1],b[2]));
        success(triangle,2);
        assert(fixture.published.vectors.at("position")[1]==MVector::zero);

        reset(); swept.lines={MVector(b[0],b[1],b[2]),MVector(a[0],a[1],a[2])};
        success(curve,3);
        assert(fixture.published.vectors.at("position")[1]==MVector::zero);
        for (double invalid:{nan,inf,-inf}) {
            reset(); a[axis]=invalid;
            swept.current[0][1]=MVector(a[0],a[1],a[2]); reject(triangle);
            reset(); swept.previous[0][1]=MVector(a[0],a[1],a[2]); reject(triangle);
            reset(); swept.lines[1]=MVector(a[0],a[1],a[2]); reject(curve);
        }
    }
    reset(); fixture.numbers["timeSampling"]=0;
    swept.current[0]={{MVector(1e308,1e308,1e308),MVector(3,6,9),MVector(-1e308,-1e308,-1e308)}};
    success(triangle,1); positions({MVector(1,2,3)});
    reset(); fixture.numbers["timeSamples"]=1;
    const double max=std::numeric_limits<double>::max();
    swept.current[0].fill(MVector(max,max,max)); swept.previous[0]=swept.current[0];
    success(triangle,2); positions({MVector(max,max,max),MVector(max,max,max)});
    reset(); swept.lines={MVector(max,max,max),MVector(max,max,max)};
    success(curve,3); positions({MVector(max,max,max),MVector(max,max,max),MVector(max,max,max)});
}
void invalidInputs() {
    for (Node node:{triangle,curve}) {
        reset(); assert(evaluate(node,-1)==MS::kFailure && fixture.publications==0 && fixture.cleans==0);
        for (const char* key:{"mIsFull","mCurrentTime","mDeltaTime","mRate"}) {
            reset(); fixture.numbers[key]=(std::string(key)=="mIsFull" ? 1 : 0); success(node,0);
        }
        reset(); fixture.sweptConnected=false; success(node,0);
        assert(std::find(operations.begin(),operations.end(),"input mSweptGeometry")==operations.end());
        reset(); fixture.sweptValid=false; success(node,0);
        reset(); swept.current.clear(); swept.lines.clear(); success(node,0);
        for (const char* key:{"mCurrentTime","mStartTime","mDeltaTime","mRate"})
            for (double invalid:{nan,inf,-inf}) { reset(); fixture.numbers[key]=invalid; reject(node); }
        for (int count:{-1,10000001}) {
            reset(); swept.overrideCount=true; swept.reportedCount=count; reject(node);
        }
        reset(); swept.overrideCount=true; swept.reportedCount=10000000; reject(node);
    }
    for (const char* key:{"timeSamples","timeNoise"}) {
        reset(); fixture.numbers[key]=-1; reject(triangle);
    }
    for (double value:{nan,inf,-inf}) {
        reset(); fixture.numbers["timeNoise"]=value; reject(triangle);
    }
    reset(); fixture.numbers["timeSamples"]=10000000; reject(triangle);
    for (const char* key:{"startPosition","endPosition","curveRadius","radiusNoise","rNoiseScale"})
        for (double invalid:{nan,inf,-inf}) { reset(); fixture.numbers[key]=invalid; reject(curve); }
    for (const char* key:{"startPosition","endPosition","curveRadius","radiusNoise","samplingArea","rNoisefrequency"}) {
        reset(); fixture.numbers[key]=-1; reject(curve);
    }
    for (const char* key:{"startPosition","endPosition"}) {
        reset(); fixture.numbers[key]=1.1; reject(curve);
    }
    reset(); fixture.numbers["rNoisefrequency"]=1000001; reject(curve);
    reset(); fixture.numbers["mRate"]=10000001; reject(curve);
    reset(); fixture.numbers["samplingArea"]=10000000; reject(curve);
}
void inject(Node node,const std::function<void()>& configure) {
    reset(); configure(); assert(evaluate(node)==MS::kSuccess);
    const auto baseline=operations;
    const auto expected=*fixture.published.vectors.at("position").values;
    for (std::size_t i=0; i<baseline.size(); ++i) {
        for (MS::MStatusCode code:{MS::kFailure,MS::kInvalidParameter}) {
            reset(); configure(); failAt=static_cast<int>(i); injectedStatus=code;
            assert(evaluate(node)==code && fixture.cleans==0);
            assert(fixture.publications==(baseline[i]=="output.clean" ? 1 : 0));
            assert(callCount==static_cast<int>(i+1));
            reset(); configure(); assert(evaluate(node)==MS::kSuccess && operations==baseline);
            assert(*fixture.published.vectors.at("position").values==expected); ++failures;
        }
        reset(); configure(); attributeThrowAt=static_cast<int>(i);
        assert(evaluate(node)==MS::kFailure && fixture.cleans==0);
        assert(fixture.publications==(baseline[i]=="output.clean" ? 1 : 0));
        assert(callCount==static_cast<int>(i+1)); ++exceptions;
    }
}
} // namespace

int main() {
    goldens(); numericExtremes(); invalidInputs();
    for (Node node:{triangle,curve}) {
        reset(); assert(evaluate(node,42,false)==MS::kUnknownParameter && operations.empty());
        inject(node,[] {});
        inject(node,[] { fixture.numbers["timeNoise"]=0.25; fixture.numbers["curveRadius"]=2;
                        fixture.numbers["radiusNoise"]=0.5; });
    }
    std::cout << "Swept emitter safety: " << evaluations << " evaluations, " << failures
              << " API failure/retry cases, " << exceptions << " exception cases\n";
}
