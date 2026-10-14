#include "nucleus_particle_doubles.h"
#include "nucleus_pop_compute.inc"

// Execute the complete production compute body and real typed input reader.
// Setup, field forces and native solve remain separately tested boundaries.
static double fieldStep=0;
static float solveTime=0;
MStatus DPnPopSolver::initDPParticle(const DPData& input,MDataBlock&){
    const auto s=operation("pop.initParticle");
    if(s){fNObject.count=input.numberOfPoints();fNObject.points=input.points;}return s;
}
MStatus DPnPopSolver::updateCommonDPParticle(MDataBlock&){return operation("pop.common");}
MStatus DPnPopSolver::initSolver(MDataBlock&){return operation("pop.initSolver");}
MStatus DPnPopSolver::calcSolver(MDataBlock&,float time){
    const auto s=operation("pop.solve");if(s){solveTime=time;fNObject.solved=true;}return s;
}
MStatus DPnPopSolver::applyFields(double step){const auto s=operation("pop.fields");if(s)fieldStep=step;return s;}

static void fixture(double time=2,bool enabled=true,bool emptyInput=false){
    resetCalls();published=cleaned=positionWrites=velocityWrites=0;invalidResult=0;
    startTime=1;currentTime=time;startTimeUnit=currentTimeUnit=MTime::kSeconds;
    source=DPData();lastOutput=DPData();lastOutput.points[0].x=-100;
    optionalInputKind=emptyInput?1:0;optionalInputConnected=false;
    numericInputs.clear();numericInputs["aEnable"]=enabled?1:0;fieldStep=0;solveTime=0;
}
static MStatus evaluate(DPnPopSolver& n){MDataBlock b;return n.compute({DPnPopSolver::outDP},b);}
static int calls(const char* label){return std::count(operations.begin(),operations.end(),label);}
static void noPublication(){assert(published==0&&cleaned==0&&lastOutput.points[0].x==-100);}
static void noSimulation(){assert(calls("pop.initParticle")==0&&calls("pop.common")==0&&calls("pop.initSolver")==0&&calls("pop.solve")==0&&calls("pop.fields")==0&&calls("collisions")==0);}

int main(){
    int faults=0;
    for(int branch=0;branch<4;++branch){
        const double time=branch<2?1:2;const bool enabled=branch!=3,empty=branch==1;
        fixture(time,enabled,empty);DPnPopSolver baseline;assert(evaluate(baseline));
        const auto trace=operations;const auto golden=lastOutput.points;
        assert(published==1&&cleaned==1&&lastOutput.points==baseline.fNObject.points);
        assert(calls("pop.initParticle")==int(branch==0)&&calls("pop.common")==int(branch==1));
        assert(calls("pop.initSolver")==int(branch<2)&&calls("pop.solve")==int(enabled));
        assert(calls("pop.fields")==int(branch==2)&&calls("collisions")==int(branch==2));
        if(branch==2)assert(fieldStep==1.0/24&&solveTime==2);
        if(!enabled)assert(calls("input aStartTime")==0&&calls("input aCurrentTime")==0);
        for(int i=0;i<int(trace.size());++i)for(int mode=0;mode<3;++mode){
            fixture(time,enabled,empty);DPnPopSolver n;
            if(mode==2)attributeThrowAt=i;else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
            assert(evaluate(n)==(mode==1?MS::kInvalidParameter:MS::kFailure));
            assert(callCount==i+1&&cleaned==0);
            // setClean is after publication: do not pretend it rolls back output.
            if(trace[i]!="data.clean")noPublication();else assert(published==1);
            fixture(time,enabled,empty);assert(evaluate(n));assert(lastOutput.points==golden);++faults;
        }
    }
    for(int kind:{2,3,4}){
        fixture(1);optionalInputKind=kind;DPnPopSolver node;
        assert(evaluate(node)==MS::kFailure);noSimulation();noPublication();
    }
    fixture(1);source.validTopology=false;DPnPopSolver invalidTopology;
    assert(evaluate(invalidTopology)==MS::kInvalidParameter);noSimulation();noPublication();
    fixture(1,true,true);optionalInputConnected=true;DPnPopSolver connectedNull;
    assert(evaluate(connectedNull)==MS::kFailure);noSimulation();noPublication();
    for(bool connected:{false,true}){
        fixture(1);optionalInputConnected=connected;DPnPopSolver valid;
        assert(evaluate(valid)&&calls("pop.initParticle")==1&&calls("input.connected")==0);
        fixture(1);source.points.clear();DPnPopSolver empty;
        assert(evaluate(empty)&&lastOutput.points.empty()&&calls("pop.initParticle")==1);
    }
    int invalidTimes=0;
    for(double invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
        for(bool start:{false,true}){
            fixture();if(start)startTime=invalid;else currentTime=invalid;DPnPopSolver node;
            assert(evaluate(node)==MS::kInvalidParameter);noSimulation();noPublication();++invalidTimes;
        }
    }
    for(double huge:{double(std::numeric_limits<float>::max())*2,-double(std::numeric_limits<float>::max())*2}){
        fixture(huge);DPnPopSolver node;assert(evaluate(node)==MS::kInvalidParameter);noSimulation();noPublication();
    }
    // Disabled evaluation does not consume time/input or advance the solver.
    fixture(2,false);currentTime=startTime=std::numeric_limits<double>::quiet_NaN();optionalInputKind=3;
    DPnPopSolver disabled;assert(evaluate(disabled));noSimulation();assert(published==1);
    for(double time:{-1.0,0.0,1.0}){
        fixture(time);DPnPopSolver initial;assert(evaluate(initial));assert(calls("pop.initSolver")==1&&solveTime==time);
    }
    fixture(48);startTime=24;startTimeUnit=currentTimeUnit=MTime::kFrames;DPnPopSolver frames;
    assert(evaluate(frames)&&solveTime==2&&fieldStep==1.0/24&&calls("pop.initSolver")==0);
    fixture(2);startTime=24;startTimeUnit=MTime::kFrames;DPnPopSolver mixedUnits;
    assert(evaluate(mixedUnits)&&calls("pop.initSolver")==0);
    fixture();DPnPopSolver emptyResult;emptyResult.fNObject.count=0;emptyResult.fNObject.points.clear();
    assert(evaluate(emptyResult)&&lastOutput.points.empty()&&calls("result.positions")==0);
    for(int mismatch:{1,2,5}){
        fixture();invalidResult=mismatch;DPnPopSolver node;assert(evaluate(node)==MS::kFailure);noPublication();
    }
    fixture();DPnPopSolver unknown;MDataBlock data;
    assert(unknown.compute({MObject{"unknown"}},data)==MS::kUnknownParameter&&operations.empty());
    std::cout<<"Nucleus pop compute: "<<faults<<" API faults/exceptions, "<<invalidTimes
             <<" invalid times; optional input, branch, time-unit, publication and retry goldens passed\n";
}
