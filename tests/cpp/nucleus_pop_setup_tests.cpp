#include "nucleus_particle_doubles.h"
#include "nucleus_pop_setup.inc"
#include "nucleus_attribute_cases.h"

// Execute production particle setup/common-control bodies. The native setters
// and DPoint storage are boundaries, not an implementation of Nucleus.
static void resetProbe() {
    resetCalls(); shortArrayAt=-1; numericInputs.clear(); stringInputs.clear();
    source=DPData(); source.scalars["pscale"]={.25f,.5f}; source.scalars["mass"]={1,2};
}
static int calls(const char* name) {
    return std::count(operations.begin(),operations.end(),name);
}
static MStatus configure(DPnPopSolver& node,const DPData& input) {
    MDataBlock data;
    try { return node.initDPParticle(input,data); }
    catch (...) { return MS::kFailure; } // existing owning compute exception boundary
}
static MStatus common(DPnPopSolver& node) {
    MDataBlock data;
    try { return node.updateCommonDPParticle(data); }
    catch (...) { return MS::kFailure; }
}
static void unchanged(const ParticleBoundary& before,const ParticleBoundary& after) {
    assert(before.count==after.count&&before.points==after.points);
    assert(before.velocities==after.velocities&&before.radii==after.radii);
    assert(before.masses==after.masses&&before.controls==after.controls);
}
static void golden(const DPnPopSolver& node,const DPData& input) {
    assert(node.fNObject.count==input.numberOfPoints());
    assert(node.fNObject.points==input.points&&node.fNObject.velocities==input.velocities);
    if(!input.points.empty()) {
        assert(node.fNObject.radii==input.scalars.at("pscale"));
        assert(node.fNObject.masses==input.scalars.at("mass"));
    }
}
static void invalid(const DPData& input) {
    DPnPopSolver node; const auto before=node.fNObject;
    assert(!configure(node,input)); assert(calls("native.topology")==0);
    unchanged(before,node.fNObject);
}

int main() {
    const int typedCases=testNucleusAttributeAdmission([](DPData& input){
        DPnPopSolver node;const auto status=configure(node,input);
        if(status)assertNucleusTypedResult(node.fNObject,input);return status;
    });
    int faults=0,shortArrays=0;
    for(bool empty:{false,true}) {
        resetProbe(); if(empty) {source.points.clear();source.velocities.clear();}
        DPnPopSolver baseline; assert(configure(baseline,source)); golden(baseline,source);
        const auto trace=operations;
        const int firstWrite=std::find(trace.begin(),trace.end(),"native.topology")-trace.begin();
        const auto sourceBefore=source;
        for(int i=0;i<int(trace.size());++i) {
            for(int mode=0;mode<3;++mode) {
                resetCalls();shortArrayAt=-1;DPnPopSolver node;const auto before=node.fNObject;
                if(mode==2)attributeThrowAt=i;
                else {failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
                assert(configure(node,source)==(mode==1?MS::kInvalidParameter:MS::kFailure));
                assert(callCount==i+1);
                if(i<firstWrite) {assert(calls("native.topology")==0);unchanged(before,node.fNObject);}
                assert(source.points==sourceBefore.points&&source.velocities==sourceBefore.velocities);
                assert(source.scalars==sourceBefore.scalars);
                resetCalls();assert(configure(node,source));golden(node,source);++faults;
            }
            if(!empty&&(trace[i]=="array.resize"||trace[i]=="array.copy")) {
                resetCalls();shortArrayAt=i;DPnPopSolver node;const auto before=node.fNObject;
                assert(!configure(node,source));assert(calls("native.topology")==0);
                unchanged(before,node.fNObject);++shortArrays;
            }
        }
    }
    // The separate common-only path uses the same preflight and setter mapping.
    resetProbe();DPnPopSolver commonBaseline;assert(common(commonBaseline));const auto trace=operations;
    for(int i=0;i<int(trace.size());++i)for(int mode=0;mode<3;++mode) {
        resetCalls();DPnPopSolver node;const auto before=node.fNObject;
        if(mode==2)attributeThrowAt=i;
        else {failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
        assert(common(node)==(mode==1?MS::kInvalidParameter:MS::kFailure));
        assert(callCount==i+1);if(i<16)unchanged(before,node.fNObject);++faults;
    }
    for(int bad=0;bad<10;++bad) {
        resetProbe();DPData input=source;
        if(bad==0)input.points[0].x=std::numeric_limits<float>::quiet_NaN();
        if(bad==1)input.velocities[0].z=std::numeric_limits<float>::infinity();
        if(bad==2)input.velocities.pop_back();
        if(bad==3)input.velocities.push_back({});
        if(bad==4)input.scalars["pscale"].pop_back();
        if(bad==5)input.scalars["mass"].push_back(1);
        if(bad==6)input.scalars["pscale"][0]=-1;
        if(bad==7)input.scalars["mass"][0]=std::numeric_limits<float>::quiet_NaN();
        if(bad==8)input.reportedCount=dommy::generation::kMaxGeneratedPointCount+1;
        if(bad==9) {
            input.reportedCount=input.points.size()+1;
            input.velocities.push_back({});
            input.scalars["pscale"].push_back(.5f);input.scalars["mass"].push_back(1);
        }
        invalid(input);
    }
    for(float value:{std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        for(const char* name:{"aBounce","aFriction","aDamping","aIncompressibility","aRestDensity",
                              "aLiquidRadiusScale","aViscosity","aSurfaceTension","aSelfCollisionSoftness","aSelfCollideWidth"}) {
            resetProbe();numericInputs[name]=value;invalid(source);
        }
        for(const char* name:{"aRadius","aInverseMass"}) {
            resetProbe();source.scalars.clear();numericInputs[name]=value;invalid(source);
        }
    }
    for(const char* name:{"aRadius","aInverseMass","aMaxIterations","aMaxSelfCollisionIterations"}) {
        resetProbe();source.scalars.clear();numericInputs[name]=-1;invalid(source);
    }
    // Missing attributes use defaults without creating attributes in input.
    resetProbe();source.hasVelocity=false;source.scalars.clear();DPnPopSolver fallback;
    numericInputs["aRadius"]=.125;numericInputs["aInverseMass"]=.5;
    assert(configure(fallback,source));assert(source.scalars.empty()&&!source.hasVelocity);
    assert(fallback.fNObject.velocities==MFloatPointArray(2,MFloatPoint(0,0,0)));
    assert(fallback.fNObject.radii==DScalarDataList(2,.125f));
    assert(fallback.fNObject.masses==DScalarDataList(2,.5f));
    // Existing valid arrays override even unused invalid defaults, as before.
    resetProbe();numericInputs["aRadius"]=numericInputs["aInverseMass"]=-1;
    DPnPopSolver overridden;assert(configure(overridden,source));golden(overridden,source);
    resetProbe();stringInputs["aMassAttribute"]="pscale";DPnPopSolver alias;
    assert(configure(alias,source));assert(alias.fNObject.masses==source.scalars["pscale"]);
    // Empty data must not acquire radius/mass handles or pass empty pointers.
    resetProbe();source.points.clear();source.velocities.clear();DPnPopSolver empty;
    assert(configure(empty,source));assert(calls("input aRadius")==0&&calls("input aInverseMass")==0);
    assert(calls("native.radius")==0&&calls("native.mass")==0);
    // Independent values catch field/setter swaps in the shared implementation.
    resetProbe();DPnPopSolver controls;
    const char* names[]={"Bounce","Friction","Damping","Incompressibility","RestDensity",
        "LiquidRadiusScale","Viscosity","SurfaceTension","SelfCollisionSoftness","SelfCollideWidth"};
    for(unsigned i=0;i<10;++i)numericInputs[std::string("a")+names[i]]=float(i+1)/16;
    numericInputs["aMaxIterations"]=7;numericInputs["aMaxSelfCollisionIterations"]=9;
    for(const char* n:{"DisableGravity","LiquidSimulation","Collide","SelfCollide"})numericInputs[std::string("a")+n]=1;
    assert(configure(controls,source));
    for(unsigned i=0;i<10;++i)assert(controls.fNObject.controls.at(std::string("set")+names[i])==float(i+1)/16);
    assert(controls.fNObject.controls.at("setMaxIterations")==7&&controls.fNObject.controls.at("setMaxSelfCollisionIterations")==9);
    for(const char* n:{"DisableGravity","LiquidSimulation","Collide","SelfCollide"})assert(controls.fNObject.controls.at(std::string("set")+n)==1);
    std::cout<<"Nucleus pop setup: "<<faults<<" API faults/exceptions, "<<shortArrays
             <<" short arrays, "<<typedCases<<" real typed-attribute cases; preflight, recovery, source ownership and policy goldens passed\n";
}
