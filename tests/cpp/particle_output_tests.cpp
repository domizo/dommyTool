#include "particle_output_doubles.h"
#include "particle_output.inc"
#include "point_input_cases.h"

#include <functional>

namespace {
int evaluations=0, failures=0, shortCases=0;
const double nan=std::numeric_limits<double>::quiet_NaN();
void reset() {
    fixture=ParticleFixture();
    resetPointInputFixture();
    fixture.numbers={
        {"mIsFull",0},{"mCurrentTime",2},{"mStartTime",1},{"mDeltaTime",1},{"mRate",1},
        {"aEmitTiming",2},{"aVelocityType",0},{"isOutKill",0},{"thickness",0},{"noise",0},{"nSeed",0},
        {"killType",4},{"killDistance",0.5},{"killAttributeType",0},{"killCondition",0},{"killAngleValue",45}};
    fixture.strings={{"aVelocityAttribute","vel"},{"killAttributeName","value"},{"killExpression","0;2"}};
    fixture.triples={{"mDirection",{{0,3,0}}},{"minKillSpaceRange",{{-1,-1,-1}}},
        {"maxKillSpaceRange",{{1,1,1}}},{"killAttributeValue",{{0,0,0}}}};
    resetCalls(); shortAt=-1; shortApplied=badAccesses=oversizedAllocations=0;
    injectedStatus=MS::kFailure;
}
enum Node { emitter, volume, kill };
MStatus evaluate(Node node,int logicalIndex=42,bool supported=true) {
    ++evaluations;
    MDataBlock data;
    MPlug plug(supported ? (node==kill ? mOutputForce : mOutput) : MObject{"other"},logicalIndex);
    MStatus status;
    if (node==emitter) { DPEmitter n; status=n.compute(plug,data); }
    else if (node==volume) { DMeshVolumeEmitter n; status=n.compute(plug,data); }
    else { DKillField n; status=n.compute(plug,data); }
    assert(badAccesses==0 && oversizedAllocations==0);
    return status;
}
template<class T> void attribute(const std::string& name,std::initializer_list<T> values) {
    fixture.input.attributes[name]=std::make_shared<TypedDPAttributeData<T>>(std::vector<T>(values));
}
void successful(Node node,unsigned count) {
    assert(evaluate(node)==MS::kSuccess);
    assert(fixture.publications==1 && fixture.cleans==1 && fixture.outputIndex==42);
    if (node==kill) assert(fixture.publishedForces.length()==count);
    else {
        const auto& values=fixture.published.vectors;
        const auto p=values.find("position"), v=values.find("velocity");
        if (count) assert(p!=values.end() && v!=values.end());
        if (p!=values.end()) assert(p->second.length()==count);
        if (v!=values.end()) assert(v->second.length()==count);
    }
}
void goldens() {
    for (int mode=0; mode<4; ++mode) {
        reset(); fixture.numbers["aVelocityType"]=mode;
        attribute<Imath::V3f>("vel",{Imath::V3f(1,2,3),Imath::V3f(4,5,6)});
        successful(emitter,2);
        const MVectorArray& v=fixture.published.vectors.at("velocity");
        if (mode==0) assert(v[0]==MVector::zero && v[1]==MVector::zero);
        if (mode==1) assert(v[0]==MVector(0,3,0) && v[1]==MVector(0,3,0));
        if (mode==2) assert(v[0]==MVector(1,0,0) && v[1]==MVector(-1,0,0));
        if (mode==3) assert(v[0]==MVector(1,2,3) && v[1]==MVector(4,5,6));
    }
    reset();
    attribute<Imath::V3d>("v",{Imath::V3d(1,2,3),Imath::V3d(4,5,6)});
    attribute<double>("d",{1.5,2.5}); attribute<int32_t>("i",{1,2});
    attribute<std::string>("s",{"first","second"});
    fixture.transfers={{"v","vector"},{"d","double"},{"i","int"},{"s","string"},
        {"d","position"},{"i","double"},{"missing","ignored"}};
    successful(emitter,2);
    assert(fixture.published.vectors.at("vector")[1]==MVector(4,5,6));
    assert(fixture.published.doubles.at("double")[1]==2.5);
    assert(fixture.published.ints.at("int")[1]==2);
    assert(fixture.published.strings.at("string")[1]==MString("second"));

    reset(); successful(volume,8);
    assert(fixture.published.vectors.at("position")[0]==MVector(-1,-1,-1));
    assert(fixture.published.vectors.at("position")[7]==MVector(1,1,1));
    for (const auto& v:*fixture.published.vectors.at("velocity").values) assert(v==MVector::zero);
    reset(); fixture.matrix[3][0]=4; successful(volume,8);
    assert(fixture.published.vectors.at("position")[0]==MVector(3,-1,-1));
    reset(); fixture.numbers["noise"]=0.1; fixture.numbers["isOutKill"]=1;
    assert(evaluate(volume)==MS::kSuccess);
    const auto jittered=*fixture.published.vectors.at("position").values;
    reset(); fixture.numbers["noise"]=0.1; fixture.numbers["isOutKill"]=1;
    assert(evaluate(volume)==MS::kSuccess);
    assert(*fixture.published.vectors.at("position").values==jittered);

    for (int mode=0; mode<6; ++mode) {
        reset(); fixture.numbers["killType"]=mode;
        successful(kill,2);
        assert(fixture.lifespanWrites==1);
        assert(fixture.lifespans[0]==(mode==D_KILL_SPACERANGE ? 100000 : 0));
        assert(fixture.lifespans[1]==(mode==D_KILL_BBOX || mode==D_KILL_MESH || mode==D_KILL_ATTRIBUTE ? 100000 : 0));
        const auto prepare=std::find(operations.begin(),operations.end(),"forces.create");
        const auto mutate=std::find(operations.begin(),operations.end(),"lifespan.set");
        assert(prepare<mutate);
    }
    reset(); fixture.numbers["killType"]=D_KILL_ATTRIBUTE; fixture.numbers["killAttributeType"]=D_ATTR_3D;
    successful(kill,2); assert(fixture.lifespans[0]==0 && fixture.lifespans[1]==100000);
}
void optionalPointInputs() {
    optionalPointReaderCases(fixture.input, fixture.hasInput, DPEmitter::inDP, reset);
    reset();
    assert(evaluate(emitter) == MS::kSuccess);
    const auto trace = operations;
    for (const char* operationName : {"input inDP", "point.bind"}) {
        const auto found = std::find(trace.begin(), trace.end(), operationName);
        assert(found != trace.end());
        const int at = static_cast<int>(found - trace.begin());
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset();
            failAt = at;
            injectedStatus = code;
            assert(evaluate(emitter) == code);
            assert(fixture.publications == 0 && fixture.cleans == 0);
            if (std::string(operationName) == "input inDP") {
                assert(pointInputFixture.dataReads == 0);
            }
            reset();
            successful(emitter, 2);
        }
        reset();
        attributeThrowAt = at;
        assert(evaluate(emitter) == MS::kFailure);
        assert(fixture.publications == 0 && fixture.cleans == 0);
    }
    for (int invalid = 0; invalid < 4; ++invalid) {
        reset();
        if (invalid == 0) pointInputFixture.wrongMayaType = true;
        if (invalid == 1) pointInputFixture.wrongCppType = true;
        if (invalid == 2) pointInputFixture.nullRawData = true;
        if (invalid == 3) fixture.input.topology = false;
        assert(evaluate(emitter) == (invalid == 3 ? MS::kInvalidParameter : MS::kFailure));
        assert(fixture.publications == 0 && fixture.cleans == 0);
    }
    reset();
    fixture.hasInput = false;
    pointInputFixture.connected = true;
    assert(evaluate(emitter) == MS::kFailure);
    assert(fixture.publications == 0 && fixture.cleans == 0);
    reset();
    fixture.hasInput = false;
    successful(emitter, 0);
    const auto found = std::find(operations.begin(), operations.end(), "point.connection");
    assert(found != operations.end());
    const int query = static_cast<int>(found - operations.begin());
    for (bool connected : {false, true}) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset();
            fixture.hasInput = false;
            pointInputFixture.connected = connected;
            failAt = query;
            injectedStatus = code;
            assert(evaluate(emitter) == code);
            assert(fixture.publications == 0 && fixture.cleans == 0);
        }
    }
}

// Compare each failure/retry against the same successful path. Legacy particle
// connection type probing may skip a failed bind; it must not mutate that node.
void inject(Node node,const std::function<void()>& configure) {
    reset(); configure(); assert(evaluate(node)==MS::kSuccess);
    const auto baseline=operations;
    for (std::size_t i=0; i<baseline.size(); ++i) {
        for (MS::MStatusCode code:{MS::kFailure,MS::kInvalidParameter}) {
            reset(); configure(); failAt=static_cast<int>(i); injectedStatus=code;
            const MStatus status=evaluate(node);
            const bool skippedBind=baseline[i]=="particle.bind";
            if (skippedBind) {
                assert(status==MS::kSuccess && fixture.lifespanWrites==0);
            } else {
                assert(!status && fixture.cleans==0);
                assert(fixture.publications==(baseline[i]=="output.clean" ? 1 : 0));
                assert(fixture.lifespanWrites==(node==kill &&
                    (baseline[i]=="output.clean" || baseline[i]=="output.set") ? 1 : 0));
                assert(callCount==static_cast<int>(i+1));
            }
            reset(); configure(); assert(evaluate(node)==MS::kSuccess);
            assert(operations==baseline); ++failures;
        }
        reset(); configure(); attributeThrowAt=static_cast<int>(i);
        assert(!evaluate(node) && fixture.cleans==0);
        assert(fixture.lifespanWrites==(node==kill &&
            (baseline[i]=="output.clean" || baseline[i]=="output.set") ? 1 : 0));
        if (baseline[i]=="array.resize" || baseline[i]=="array.copy") {
            reset(); configure(); shortAt=static_cast<int>(i);
            assert(!evaluate(node) && shortApplied==1 && fixture.cleans==0);
            assert(fixture.publications==0 && fixture.lifespanWrites==0);
            ++shortCases;
        }
    }
}

// All four actual copyArrayAttribute overloads run independently, including
// string element-set failures. Shared-array doubles observe the output view.
template<class Array> void sharedCopy(const Array& values) {
    reset(); MFnArrayAttrsData output;
    assert(dommy::particle::copyArrayAttribute(output,"test",values)==MS::kSuccess);
    const auto baseline=operations;
    assert(baseline.size()>=2);
    for (std::size_t i=0; i<baseline.size(); ++i) {
        reset(); failAt=static_cast<int>(i);
        assert(dommy::particle::copyArrayAttribute(output,"test",values)==MS::kFailure);
        assert(callCount==static_cast<int>(i+1)); ++failures;
        if (baseline[i]=="array.copy" || baseline[i]=="array.resize") {
            reset(); shortAt=static_cast<int>(i);
            assert(dommy::particle::copyArrayAttribute(output,"test",values)==MS::kFailure);
            assert(shortApplied==1 && badAccesses==0); ++shortCases;
        }
    }
}
void transferFailures() {
    const auto configure=[] {
        attribute<Imath::V3f>("v",{Imath::V3f(1,2,3),Imath::V3f(4,5,6)});
        attribute<double>("d",{1,2}); attribute<int32_t>("i",{1,2});
        attribute<std::string>("s",{"one","two"});
        fixture.transfers={{"v","v"},{"d","d"},{"i","i"},{"s","s"}};
    };
    reset(); configure(); successful(emitter,2);
    const auto baseline=operations;
    bool copying=false;
    for (std::size_t i=0; i<baseline.size(); ++i) {
        const auto& name=baseline[i];
        if (name.compare(0,5,"attr.")==0) copying=true;
        if (name=="array.physical" || name=="inputArray aTransferList" || name=="output.set") copying=false;
        if (!copying) continue;
        for (MS::MStatusCode code:{MS::kFailure,MS::kInvalidParameter}) {
            reset(); configure(); failAt=static_cast<int>(i); injectedStatus=code;
            assert(evaluate(emitter)==code && fixture.publications==0 && fixture.cleans==0);
            assert(callCount==static_cast<int>(i+1));
            reset(); configure(); successful(emitter,2); assert(operations==baseline); ++failures;
        }
    }
    // The pre-existing bool attribute reader intentionally rejects a failed
    // source conversion as an invalid optional transfer, rather than changing
    // the whole node's error policy in this publication-only fix.
    const auto conversion=std::find(baseline.begin(),baseline.end(),"string.set");
    assert(conversion!=baseline.end());
    reset(); configure(); failAt=static_cast<int>(conversion-baseline.begin());
    successful(emitter,2); assert(fixture.published.strings.count("s")==0);
}
void boundaries() {
    for (Node n:{emitter,volume,kill}) {
        reset(); assert(evaluate(n,42,false)==MS::kUnknownParameter && callCount==0);
        reset(); assert(!evaluate(n,-1) && fixture.publications==0 && fixture.lifespanWrites==0);
    }
    for (Node n:{emitter,volume}) {
        reset(); fixture.numbers["mIsFull"]=1; successful(n,0);
        reset(); fixture.numbers["mDeltaTime"]=0; successful(n,0);
        reset(); fixture.numbers["mRate"]=0; successful(n,0);
        reset(); fixture.numbers["mRate"]=nan; assert(!evaluate(n));
    }
    reset(); fixture.hasInput=false; successful(emitter,0);
    reset(); fixture.input.points.clear(); successful(emitter,0);
    reset(); fixture.input.points.reportedSize=10000001; assert(!evaluate(emitter));
    reset(); fixture.input.points[0][0]=static_cast<float>(nan); assert(!evaluate(emitter));
    reset(); fixture.numbers["aEmitTiming"]=0; successful(emitter,0);
    reset(); fixture.numbers["aEmitTiming"]=1; fixture.range={{5,1}}; successful(emitter,2);
    reset(); fixture.numbers["aEmitTiming"]=99; successful(emitter,0);
    reset(); fixture.numbers["aVelocityType"]=99; successful(emitter,0);
    reset(); fixture.numbers["aVelocityType"]=3; successful(emitter,2);
    reset(); fixture.numbers["aVelocityType"]=3; attribute<double>("vel",{1,2}); successful(emitter,0);
    reset(); fixture.transfers={{"bad","skip"}}; attribute<double>("bad",{nan,2}); successful(emitter,2);
    assert(fixture.published.doubles.count("skip")==0);

    reset(); fixture.meshValid=false; successful(volume,0);
    reset(); fixture.density[0]=0; assert(!evaluate(volume) && fixture.meshReads==0);
    reset(); fixture.density[0]=10000000; assert(!evaluate(volume) && fixture.meshReads==0);
    reset(); fixture.matrix[0][0]=nan; assert(!evaluate(volume));
    reset(); fixture.badSurface=true; assert(!evaluate(volume));
    reset(); fixture.numbers["thickness"]=0.1; successful(volume,4);

    reset(); fixture.foundParticle=false; successful(kill,2); assert(fixture.lifespanWrites==0);
    reset(); fixture.particleCount=1; successful(kill,2); assert(fixture.lifespanWrites==0);
    reset(); fixture.positions.values->clear(); fixture.particleCount=0; successful(kill,0);
    reset(); fixture.positions.reportedLength=10000001; assert(!evaluate(kill));
    reset(); fixture.positions[0].x=nan; assert(!evaluate(kill));
    reset(); fixture.multipleParticles=true; assert(!evaluate(kill));
    reset(); fixture.numbers["killType"]=99; assert(!evaluate(kill));
    reset(); fixture.numbers["killType"]=D_KILL_ATTRIBUTE; fixture.scalarAttribute.values->pop_back(); assert(!evaluate(kill));
    reset(); fixture.numbers["killType"]=D_KILL_EXPRESSION; fixture.strings["killExpression"]="bad"; assert(!evaluate(kill));
    reset(); fixture.numbers["killType"]=D_KILL_EXPRESSION; fixture.ids.values->pop_back(); assert(!evaluate(kill));
    reset(); fixture.numbers["killType"]=D_KILL_BBOX; fixture.foundBounds=false; assert(!evaluate(kill));
}
} // namespace

int main() {
    optionalPointInputs();
    goldens(); boundaries(); transferFailures();
    inject(emitter,[]{fixture.numbers["aVelocityType"]=2;});
    inject(volume,[]{});
    for (int mode=0; mode<6; ++mode) inject(kill,[mode]{fixture.numbers["killType"]=mode;});
    sharedCopy(MVectorArray{MVector(1,2,3),MVector(4,5,6)});
    sharedCopy(MDoubleArray{1,2}); sharedCopy(MIntArray{1,2});
    sharedCopy(MStringArray{MString("one"),MString("two")});
    std::cout << "Particle output: " << evaluations << " evaluations, " << failures
              << " API failure/retry cases, " << shortCases << " short-array cases: OK\n";
}
