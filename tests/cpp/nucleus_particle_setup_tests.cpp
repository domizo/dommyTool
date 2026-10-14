#include "nucleus_particle_doubles.h"
#include "nucleus_particle_setup.inc"
#include "nucleus_attribute_cases.h"

static void resetProbe(){
    resetCalls();shortArrayAt=-1;published=cleaned=positionWrites=velocityWrites=0;
    invalidResult=0;numericInputs.clear();startTime=1;currentTime=2;
    emitEnabled=forceEnabled=realForce=false;source=DPData();lastOutput=DPData();
    source.points.push_back({7,8,9});source.velocities.push_back({4,5,6,0});source.forces.push_back({});
    source.scalars["pscale"]={.1f,.2f,.3f};source.scalars["mass"]={1,2,3};
    emitters.clear();activations.clear();mergedSources.clear();
}
static int calls(const char* label){return std::count(operations.begin(),operations.end(),label);}
static MStatus evaluate(DPnParticle& node){MDataBlock data;return node.compute({DPnParticle::outDP},data);}
static MStatus configure(DPnParticle& node,DPData& candidate,bool initial){
    MDataBlock data;
    try {return initial?node.initializeDPDataParticle(candidate,data):node.updateDPDataParticle(candidate,data);}
    catch(...){return MS::kFailure;}
}
template<class Node> MStatus initSolver(Node& node){
    MDataBlock data;
    try {return node.initializeDPSolver(data);}catch(...){return MS::kFailure;}
}
template<> MStatus initSolver(DPnPopSolver& node){
    MDataBlock data;
    try {return node.initSolver(data);}catch(...){return MS::kFailure;}
}

static void nativeGolden(const DPnParticle& node,const DPData& expected){
    assert(node.fNObject.count==expected.numberOfPoints());
    assert(node.fNObject.points==expected.points);
    assert(node.fNObject.velocities==expected.velocities);
    assert(node.fNObject.radii==expected.scalars.at("pscale"));
    assert(node.fNObject.masses==expected.scalars.at("mass"));
}

template<class Node> int solverTests(){
    resetProbe();Node baseline;assert(initSolver(baseline));const auto trace=operations;
    int cases=0;
    for(int i=0;i<int(trace.size());++i)for(int mode=0;mode<3;++mode){
        resetProbe();Node node;
        if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
        const auto s=initSolver(node);
        assert(!s&&callCount==i+1);
        if(trace[i]!="solver.replace"&&mode!=2)assert(s==injectedStatus);
        ++cases;
    }
    const double maximum=std::numeric_limits<float>::max();
    for(double value:{0.0,1.0,-1.0,maximum,-maximum}){
        resetProbe();startTime=value;Node node;assert(initSolver(node));
        assert(node.solver.start==static_cast<float>(value));
    }
    for(double value:{std::nextafter(maximum,std::numeric_limits<double>::infinity()),
                      -std::nextafter(maximum,std::numeric_limits<double>::infinity()),
                      maximum*2,-maximum*2,std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
        resetProbe();startTime=value;Node node;node.solver.start=42;node.colObjList.push_back(nullptr);
        assert(initSolver(node)==MS::kInvalidParameter);
        assert(node.solver.start==42&&node.colObjList.size()==1);
        assert(calls("solver.replace")==0&&calls("solver.create")==0&&calls("solver.start")==0);
    }
    return cases;
}

int main(){
    const int typedCases=testNucleusAttributeAdmission([](DPData& input){
        DPnParticle node;const auto status=configure(node,input,true);
        if(status)assertNucleusTypedResult(node.fNObject,input);return status;
    })+testNucleusAttributeAdmission([](DPData& input){
        DPnParticle node;node.m_pendingParticleSetup=DPnParticle::PendingParticleSetup::Update;
        const auto status=configure(node,input,false);
        if(status)assertNucleusTypedResult(node.fNObject,input);return status;
    });
    int faults=0,shortArrays=0,retries=0;
    for(bool initial:{false,true}){
        resetProbe();DPnParticle baseline;DPData candidate=source;
        assert(configure(baseline,candidate,initial));nativeGolden(baseline,source);
        assert(baseline.m_pendingParticleSetup==DPnParticle::PendingParticleSetup::None);
        const auto trace=operations;
        const auto firstWrite=std::find(trace.begin(),trace.end(),"native.topology")-trace.begin();
        for(int i=0;i<int(trace.size());++i){
            for(int mode=0;mode<3;++mode){
                resetProbe();DPnParticle node;DPData stage=source;
                if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
                const auto s=configure(node,stage,initial);
                assert(s==(mode==1?MS::kInvalidParameter:MS::kFailure));
                assert(callCount==i+1);
                if(i<firstWrite)assert(calls("native.topology")==0);
                if(i>=firstWrite)assert(node.m_pendingParticleSetup!=DPnParticle::PendingParticleSetup::None);
                resetCalls();shortArrayAt=-1;DPData retry=source;
                assert(configure(node,retry,false));nativeGolden(node,source);
                assert(node.m_pendingParticleSetup==DPnParticle::PendingParticleSetup::None);
                assert(calls("native.topology")==1);++faults;
            }
            if(trace[i]=="array.resize"||trace[i]=="array.copy"){
                resetProbe();DPnParticle node;DPData stage=source;shortArrayAt=i;
                assert(!configure(node,stage,initial));assert(calls("native.topology")==0);++shortArrays;
            }
        }
    }
    // Real compute + real update: a setter failure after topology changed must
    // not allow the count-only fast path to publish partially configured data.
    resetProbe();DPnParticle baseline;assert(evaluate(baseline));const auto trace=operations;
    for(int i=0;i<int(trace.size());++i)if(trace[i].find("native.")==0){
        for(int mode=0;mode<2;++mode){
            resetProbe();DPnParticle node;
            if(mode)attributeThrowAt=i;else{failAt=i;injectedStatus=MS::kFailure;}
            assert(!evaluate(node));assert(published==0&&cleaned==0&&calls("solver.solve")==0);
            resetCalls();assert(evaluate(node));nativeGolden(node,source);
            assert(calls("native.topology")==1&&calls("native.positions")==1);
            assert(published==2&&lastOutput.points==source.points);++retries;
        }
    }
    // Matching counts with no failed setup keep historical simulation state.
    resetProbe();DPnParticle stable;assert(evaluate(stable));resetCalls();
    source.points[0].x=100;assert(evaluate(stable));assert(calls("native.topology")==0);
    assert(lastOutput.points[0].x!=100);

    // Initial-only missing attributes must still be created when initialization
    // failed and compute retries through the update entry point.
    resetProbe();source.hasVelocity=false;source.scalars.clear();
    DPnParticle initialBaseline;DPData stage=source;assert(configure(initialBaseline,stage,true));
    const int failure=std::find(operations.begin(),operations.end(),"native.positions")-operations.begin();
    resetProbe();source.hasVelocity=false;source.scalars.clear();DPnParticle pending;stage=source;
    failAt=failure;injectedStatus=MS::kFailure;assert(!configure(pending,stage,true));
    resetCalls();assert(evaluate(pending));assert(lastOutput.hasVelocity&&lastOutput.scalars.size()==2);
    assert(!source.hasVelocity&&source.scalars.empty());
    resetProbe();source.hasVelocity=false;source.scalars.clear();DPnParticle update;stage=source;
    assert(configure(update,stage,false));assert(!stage.hasVelocity&&stage.scalars.empty());
    assert(calls("native.velocities")==0&&calls("native.radius")==0&&calls("native.mass")==0);

    for(const char* key:{"aThickness","aInverseMass","aBounce","aFriction","aDamping", "aIncompressibility", "aRestDensity", "aLiquidRadiusScale", "aViscosity", "aSurfaceTension", "aSelfCollisionSoftness", "aSelfCollideWidth"}){
        resetProbe();numericInputs[key]=std::numeric_limits<float>::infinity();DPnParticle node;stage=source;
        assert(configure(node,stage,false)==MS::kInvalidParameter);assert(calls("native.topology")==0);
    }
    for(const char* key:{"aThickness","aInverseMass","aMaxIterations","aMaxSelfCollisionIterations"}){
        resetProbe();numericInputs[key]=-1;DPnParticle node;stage=source;
        assert(configure(node,stage,false)==MS::kInvalidParameter);assert(calls("native.topology")==0);
    }
    for(int invalid=0;invalid<6;++invalid){
        resetProbe();DPnParticle node;stage=source;
        if(invalid==0)stage.points[0].x=std::numeric_limits<float>::quiet_NaN();
        if(invalid==1)stage.velocities[0].y=std::numeric_limits<float>::infinity();
        if(invalid==2)stage.scalars["pscale"][0]=-1;
        if(invalid==3)stage.scalars["mass"][0]=std::numeric_limits<float>::quiet_NaN();
        if(invalid==4)stage.scalars["mass"].pop_back();
        if(invalid==5)stage.velocities.pop_back();
        assert(!configure(node,stage,false));assert(calls("native.topology")==0);
    }
    resetProbe();DPnParticle empty;stage=source;stage.points.clear();stage.velocities.clear();
    stage.scalars["pscale"].clear();stage.scalars["mass"].clear();
    assert(configure(empty,stage,true));assert(calls("native.radiusScalar")==1&&calls("native.massScalar")==1);

    // All common controls keep their original setter mapping.
    resetProbe();DPnParticle controls;stage=source;
    const char* floatNames[]={"Bounce","Friction","Damping","Incompressibility","RestDensity",
        "LiquidRadiusScale","Viscosity","SurfaceTension","SelfCollisionSoftness","SelfCollideWidth"};
    for(unsigned i=0;i<10;++i)numericInputs[std::string("a")+floatNames[i]]=float(i+1)/16;
    numericInputs["aMaxIterations"]=7;numericInputs["aMaxSelfCollisionIterations"]=9;
    for(const char* name:{"DisableGravity","LiquidSimulation","Collide","SelfCollide"})numericInputs[std::string("a")+name]=1;
    assert(configure(controls,stage,false));
    for(unsigned i=0;i<10;++i)assert(controls.fNObject.controls.at(std::string("set")+floatNames[i])==float(i+1)/16);
    assert(controls.fNObject.controls.at("setMaxIterations")==7&&controls.fNObject.controls.at("setMaxSelfCollisionIterations")==9);
    for(const char* name:{"DisableGravity","LiquidSimulation","Collide","SelfCollide"})assert(controls.fNObject.controls.at(std::string("set")+name)==1);

    const int timeFaults=solverTests<DPnParticle>()+solverTests<DPnPopSolver>();
    std::cout<<"Nucleus setup: "<<faults<<" setup faults, "<<shortArrays<<" short arrays, "
             <<retries<<" compute recovery cases, "<<timeFaults<<" solver faults, "<<typedCases
             <<" real typed-attribute cases; validation/time/policy goldens passed\n";
}
