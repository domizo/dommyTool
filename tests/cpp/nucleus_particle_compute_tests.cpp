#include "nucleus_particle_doubles.h"
#include "nucleus_particle_compute.inc"

static void fixture(){
    resetCalls();published=cleaned=positionWrites=velocityWrites=0;invalidResult=0;
    startTime=1;currentTime=2;startTimeUnit=currentTimeUnit=MTime::kSeconds;
    emitEnabled=true;forceEnabled=realForce=false;
    source=DPData();lastOutput=DPData();lastOutput.points[0].x=-100;
    emitters={DPData(),DPData()};emitters[0].points[0].x=11;emitters[1].points[0].x=22;
    activations={"1","1"};mergedSources.clear();
    emitterInputKinds.clear();emitterConnected.clear();emitterLogicalIndices.clear();
    numericInputs.clear();stringInputs.clear();shortArrayAt=-1;optionalInputKind=requiredInputKind=0;
    requiredInputBoundary=true;
}
static MStatus evaluate(DPnParticle& n){MDataBlock b;return n.compute({DPnParticle::outDP},b);}
static void notPublished(){assert(published==0&&cleaned==0&&lastOutput.points[0].x==-100);}
static int occurrences(const char* label){return std::count(operations.begin(),operations.end(),label);}

int main(){
    fixture();DPnParticle unknown;MDataBlock unknownData;
    assert(unknown.compute({MObject{"unknown"}},unknownData)==MS::kUnknownParameter);
    assert(operations.empty());
    for(int branch=0;branch<3;++branch)for(int kind:{2,3,4}){
        fixture();currentTime=branch==0?1:2;requiredInputKind=kind;
        DPnParticle node;MDataBlock data;
        assert(node.compute({branch==2?DPnParticle::outDP:DPnParticle::prevDP},data)==MS::kFailure);
        notPublished();assert(occurrences("solver.solve")==0&&occurrences("particle.init")==0);
    }
    // Present invalid emitter payloads are not the same as an absent input.
    for(int kind:{0,2,3,4}){
        fixture();emitterInputKinds={kind,0};emitters[0].validTopology=kind!=0;
        DPnParticle node;assert(evaluate(node)==(kind==0?MS::kInvalidParameter:MS::kFailure));
        notPublished();assert(occurrences("solver.solve")==0&&occurrences("particle.update")==0);
        resetCalls();emitterInputKinds={0,0};emitters[0].validTopology=true;
        assert(evaluate(node));assert(lastOutput.numberOfPoints()==6);
    }
    fixture();emitterInputKinds={0,1};emitterLogicalIndices={7,42};emitterConnected={true,true};
    DPnParticle connectedNull;assert(evaluate(connectedNull)==MS::kFailure);notPublished();
    assert(occurrences("solver.solve")==0&&occurrences("particle.update")==0);
    fixture();emitterInputKinds={0,1};emitterLogicalIndices={7,42};emitterConnected={true,false};
    DPnParticle absent;assert(evaluate(absent));assert((mergedSources==std::vector<float>{11}));
    const auto absentTrace=operations;
    for(int i=0;i<int(absentTrace.size());++i){
        if(absentTrace[i]!="emit.index"&&absentTrace[i]!="emit.plugElement"&&
           absentTrace[i]!="emit.plugChild"&&absentTrace[i]!="emit.connected")continue;
        for(int mode=0;mode<3;++mode){
            fixture();emitterInputKinds={0,1};emitterLogicalIndices={7,42};emitterConnected={true,false};
            if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
            DPnParticle node;assert(evaluate(node)==(mode==1?MS::kInvalidParameter:MS::kFailure));
            assert(callCount==i+1&&occurrences("solver.solve")==0);notPublished();
        }
    }
    fixture();emitters[0].points.clear();activations[0]="invalid";DPnParticle zeroEmitter;
    assert(evaluate(zeroEmitter));assert((mergedSources==std::vector<float>{22}));
    for(bool enabled:{false,true}){
        fixture();emitEnabled=false;forceEnabled=enabled;
        source.attributes["vel"]=std::make_shared<DPFloatAttr>(std::vector<float>(2,5));
        DPnParticle node;assert(evaluate(node)==MS::kInvalidParameter);notPublished();
        assert(occurrences("solver.solve")==0&&occurrences("native.velocities")==0);
    }
    for(const char* name:{"force","vel"})for(int bad=0;bad<5;++bad){
        fixture();emitEnabled=false;forceEnabled=true;
        if(bad==0)source.attributes[name]=std::make_shared<DPFloatAttr>(std::vector<float>(2,5));
        if(bad==1)source.attributes[name]=nullptr;
        if(bad==2)source.attributes[name]=std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>(1,Imath::V3f(1,2,3)));
        if(bad==3)source.attributes[name]=std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>(2,Imath::V3d(1,1e300,3)));
        if(bad==4)source.attributes[name]=std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>(2,Imath::V3f(1,2,std::numeric_limits<float>::quiet_NaN())));
        DPnParticle node;assert(evaluate(node)==MS::kInvalidParameter);notPublished();
        assert(occurrences("solver.solve")==0&&occurrences("native.velocities")==0);
    }
    for(unsigned axis=0;axis<3;++axis){
        fixture();emitEnabled=false;forceEnabled=true;numericInputs["aForceEnvelope"]=2;
        source.forces[0][axis]=3e38f;source.velocities[0][axis]=-3e38f;
        DPnParticle large;assert(evaluate(large));assert(lastOutput.velocities[0][axis]==3e38f);
        fixture();emitEnabled=false;forceEnabled=realForce=true;numericInputs["aForceEnvelope"]=1e38f;
        source.forces[0][axis]=std::numeric_limits<float>::denorm_min();
        DPnParticle tiny;assert(evaluate(tiny));
        const float expected=float(double(source.forces[0][axis])*(1.0/24)*double(1e38f));
        assert(expected>0&&lastOutput.velocities[0][axis]==expected);
        fixture();emitEnabled=false;forceEnabled=true;numericInputs["aForceEnvelope"]=2;
        source.forces[0][axis]=3e38f;DPnParticle overflow;
        assert(evaluate(overflow)==MS::kInvalidParameter);notPublished();
        assert(occurrences("native.velocities")==0&&occurrences("solver.solve")==0);
    }
    fixture();emitEnabled=false;forceEnabled=true;numericInputs["aForceEnvelope"]=-.5;
    source.forces[0]=MFloatPoint(2,4,6,0);source.velocities[0]=MFloatPoint(10,20,30,0);
    DPnParticle signedForce;assert(evaluate(signedForce));assert(lastOutput.velocities[0]==MFloatPoint(9,18,27,0));
    const auto forceTrace=operations;
    for(int i=0;i<int(forceTrace.size());++i){
        if(forceTrace[i]!="array.resize"&&forceTrace[i]!="array.copy"&&forceTrace[i]!="native.velocities")continue;
        for(int mode=0;mode<3;++mode){
            fixture();emitEnabled=false;forceEnabled=true;
            if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
            DPnParticle node;assert(evaluate(node)==(mode==1?MS::kInvalidParameter:MS::kFailure));
            assert(callCount==i+1&&occurrences("solver.solve")==0);notPublished();
        }
        if(forceTrace[i]!="native.velocities"){
            fixture();emitEnabled=false;forceEnabled=true;shortArrayAt=i;
            DPnParticle node;assert(!evaluate(node));notPublished();assert(occurrences("solver.solve")==0);
        }
    }
    for(const auto& value:std::vector<DPAttributeData::Ptr>{
            std::make_shared<DPVec3iAttr>(std::vector<Imath::V3i>(2,Imath::V3i(2,4,6))),
            std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>(2,Imath::V3d(2,4,6)))}){
        fixture();emitEnabled=false;forceEnabled=true;source.attributes["force"]=value;
        source.attributes["vel"]=value;DPnParticle node;assert(evaluate(node));
        assert(lastOutput.velocities[0]==MFloatPoint(4,8,12,0));
    }
    fixture();DPnParticle baseline;assert(evaluate(baseline));const auto trace=operations;
    assert(published==2&&cleaned==1&&positionWrites==1&&velocityWrites==1);
    assert((mergedSources==std::vector<float>{11,22}));assert(lastOutput.numberOfPoints()==6);
    assert(source.numberOfPoints()==2&&source.points[0].x==1);
    int faults=0;
    for(int i=0;i<int(trace.size());++i){
        const auto& label=trace[i];
        if(label.find("emit.")!=0&&label.find("result.")!=0&&label!="input aVelocityAttribute")continue;
        for(int mode=0;mode<3;++mode){
            fixture();DPnParticle node;
            if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
            const auto s=evaluate(node);
            assert(s==(mode==1?MS::kInvalidParameter:MS::kFailure));
            assert(callCount==i+1);notPublished();assert(positionWrites==0&&velocityWrites==0);++faults;
            if(label.find("emit.")==0)assert(occurrences("solver.solve")==0);
        }
    }
    for(int malformed:{1,2,3,4,5,8}){
        fixture();emitEnabled=false;invalidResult=malformed;DPnParticle node;
        assert(evaluate(node)==MS::kFailure);notPublished();assert(positionWrites==0&&velocityWrites==0);
    }
    int invalidValues=0;
    for(int channel:{6,7})for(unsigned axis=0;axis<4;++axis)
        for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
            fixture();emitEnabled=false;invalidResult=channel;invalidComponent=axis;invalidValue=value;DPnParticle node;
            assert(evaluate(node)==MS::kInvalidParameter);notPublished();assert(positionWrites==0&&velocityWrites==0);++invalidValues;
        }
    fixture();emitEnabled=false;DPnParticle golden;assert(evaluate(golden));
    assert(lastOutput.points==golden.fNObject.points&&lastOutput.velocities==golden.fNObject.velocities);
    fixture();emitters.clear();activations.clear();DPnParticle emptyEmit;assert(evaluate(emptyEmit));assert(occurrences("emit.next")==0);
    fixture();activations[0]="0";DPnParticle inactive;assert(evaluate(inactive));assert((mergedSources==std::vector<float>{22}));
    for(const char* expression:{"id","P","point()","invalid","evaluationFailure"}){
        fixture();activations[0]=expression;DPnParticle node;assert(evaluate(node)==MS::kInvalidParameter);notPublished();
    }
    fixture();emitEnabled=false;source.hasVelocity=false;DPnParticle noVelocity;assert(evaluate(noVelocity));assert(positionWrites==1&&velocityWrites==0);
    fixture();emitEnabled=false;source.points.clear();source.velocities.clear();source.forces.clear();DPnParticle empty;
    empty.fNObject.count=0;empty.fNObject.points.clear();empty.fNObject.velocities.clear();assert(evaluate(empty));assert(lastOutput.points.empty());
    fixture();emitEnabled=false;forceEnabled=realForce=true;DPnParticle force;assert(evaluate(force));assert(lastOutput.velocities[0].x==float(1.0/24));
    for(double time:{1.0,2.0}){
        fixture();currentTime=time;DPnParticle previous;MDataBlock b;assert(previous.compute({DPnParticle::prevDP},b));
        assert(published==1&&cleaned==1&&lastOutput.points==source.points);
    }
    int handleFaults=0,invalidTimes=0;
    for(int branch=0;branch<3;++branch){
        const MObject output=branch==2?DPnParticle::outDP:DPnParticle::prevDP;
        fixture();currentTime=branch==0?1:2;forceEnabled=true;DPnParticle baseline;MDataBlock data;
        assert(baseline.compute({output},data));const auto inputs=operations;
        for(int i=0;i<int(inputs.size());++i){
            if(inputs[i].find("input ")!=0)continue;
            for(int mode=0;mode<3;++mode){
                fixture();currentTime=branch==0?1:2;forceEnabled=true;DPnParticle node;
                if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
                assert(node.compute({output},data)==(mode==1?MS::kInvalidParameter:MS::kFailure));
                assert(callCount==i+1);notPublished();++handleFaults;
                fixture();currentTime=branch==0?1:2;forceEnabled=true;assert(node.compute({output},data));
            }
        }
        for(double invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
            for(bool start:{false,true}){
                fixture();if(start)startTime=invalid;else currentTime=invalid;DPnParticle node;
                assert(node.compute({output},data)==MS::kInvalidParameter);notPublished();
                assert(occurrences("particle.init")==0&&occurrences("particle.update")==0&&occurrences("solver.init")==0&&occurrences("solver.solve")==0);
                ++invalidTimes;
            }
        }
    }
    for(double huge:{double(std::numeric_limits<float>::max())*2,-double(std::numeric_limits<float>::max())*2}){
        fixture();currentTime=huge;DPnParticle node;assert(evaluate(node)==MS::kInvalidParameter);notPublished();
    }
    fixture();currentTime=48;startTime=24;startTimeUnit=currentTimeUnit=MTime::kFrames;
    DPnParticle frames;MDataBlock data;assert(frames.compute({DPnParticle::prevDP},data));assert(occurrences("particle.init")==0);
    // Preserve the existing prevDP raw-value comparison even with mixed units.
    fixture();startTime=24;startTimeUnit=MTime::kFrames;DPnParticle mixedUnits;
    assert(mixedUnits.compute({DPnParticle::prevDP},data));assert(occurrences("particle.init")==1);
    std::cout<<"Nucleus particle compute: "<<faults<<" API faults, 6 count mismatches, "
             <<invalidValues<<" nonfinite values, "<<handleFaults<<" input faults/exceptions, "
             <<invalidTimes<<" invalid times; emitter/output/force/previous-output goldens passed\n";
}
