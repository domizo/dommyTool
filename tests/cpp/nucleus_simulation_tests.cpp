#include "nucleus_simulation_doubles.h"
#include "nucleus_simulation.inc"

static void resetProbe() {
    resetCalls();shortArrayAt=-1;invalidResult=0;
    numericInputs.clear();vectorInputs.clear();stringInputs.clear();
    sampledPositions.clear();sampledVelocities.clear();sampledMasses.clear();sampledSteps.clear();
    fields={MVectorArray{MVector(2,4,6),MVector(1,-2,3),MVector(4,5,6)},
            MVectorArray{MVector(6,-4,2),MVector(3,2,1),MVector(7,8,9)}};
}
static int calls(const char* name){return std::count(operations.begin(),operations.end(),name);}
static DPnPopSolver fieldNode() {
    DPnPopSolver node;node.fNObject.count=3;
    node.fNObject.points={MFloatPoint(10,20,30),MFloatPoint(40,50,60,2),MFloatPoint(70,80,90)};
    node.fNObject.velocities={MFloatPoint(1,2,3,0),MFloatPoint(-4,5,-6,2),MFloatPoint(7,8,9,1)};
    node.fNObject.masses={.5f,2,0};return node;
}
static MStatus apply(DPnPopSolver& node,double step=.25) {
    try{return node.applyFields(step);}catch(...){return MS::kFailure;}
}
static void goldenField(const DPnPopSolver& node) {
    assert((node.fNObject.velocities==MFloatPointArray{MFloatPoint(2,2,4,0),MFloatPoint(-2,5,-4,2),MFloatPoint(7,8,9,1)}));
    assert(sampledPositions.size()==2&&sampledVelocities.size()==2&&sampledMasses.size()==2);
    for(unsigned field=0;field<2;++field) {
        assert((sampledPositions[field]==MPointArray{MPoint(10,20,30),MPoint(40,50,60),MPoint(70,80,90)}));
        assert((sampledVelocities[field]==MVectorArray{MVector(1,2,3),MVector(-4,5,-6),MVector(7,8,9)}));
        assert((sampledMasses[field]==MDoubleArray{2,.5,1}));assert(sampledSteps[field]==.25);
    }
    assert(calls("native.velocities")==1&&calls("field.force")==2);
}
static void rejected(DPnPopSolver& node) {
    const auto before=node.fNObject.velocities;
    assert(!apply(node));assert(calls("native.velocities")==0);
    assert(node.fNObject.velocities==before);
}
template<class Node> MStatus solve(Node& node,float time=2) {
    MDataBlock data;
    try{return node.calculateDPSolver(data,time);}catch(...){return MS::kFailure;}
}
template<> MStatus solve(DPnPopSolver& node,float time) {
    MDataBlock data;
    try{return node.calcSolver(data,time);}catch(...){return MS::kFailure;}
}
static void solverInputs(bool enabled) {
    numericInputs={{"aGravity",9.5},{"aAirDensity",.125},{"aWindSpeed",4},
                   {"aWindNoiseIntensity",.375},{"aSubsteps",7},{"aSolverIterations",11},{"aEnable",enabled?1:0}};
    vectorInputs={{"aGravityDirection",{{1,-2,3}}},{"aWindDirection",{{4,5,-6}}}};
}
template<class Node> static void goldenSolver(const Node& node,bool enabled) {
    const std::map<std::string,std::vector<double>> expected={
        {"setGravity",{9.5}},{"setGravityDir",{1,-2,3}},{"setAirDensity",{.125}},
        {"setWindSpeed",{4}},{"setWindDir",{4,5,-6}},{"setWindNoiseIntensity",{.375}},
        {"setDisabled",{enabled?0.0:1.0}},{"setSubsteps",{7}},{"setMaxIterations",{11}},{"solve",{2}}};
    assert(node.solver.settings==expected&&node.solver.solveCalls==1);
    const std::vector<std::string> expectedOrder={
        "input aGravity","input aGravityDirection","input aAirDensity","input aWindSpeed",
        "input aWindDirection","input aWindNoiseIntensity","input aSubsteps",
        "input aSolverIterations","input aEnable","solver.setGravity","solver.setGravityDir",
        "solver.setAirDensity","solver.setWindSpeed","solver.setWindDir",
        "solver.setWindNoiseIntensity","solver.setDisabled","solver.setSubsteps",
        "solver.setMaxIterations","solver.solve"};
    assert(operations==expectedOrder);
}
template<class Node> static int solverTests() {
    resetProbe();solverInputs(true);Node baseline;assert(solve(baseline));goldenSolver(baseline,true);
    const auto trace=operations;assert(trace.size()==19);
    int cases=0;
    for(int i=0;i<int(trace.size());++i)for(int mode=0;mode<3;++mode) {
        resetProbe();solverInputs(true);Node node;
        if(mode==2)attributeThrowAt=i;
        else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
        assert(solve(node)==(mode==1?MS::kInvalidParameter:MS::kFailure));
        assert(callCount==i+1&&node.solver.solveCalls==0);
        if(i<9)assert(node.solver.settings.empty());
        resetProbe();solverInputs(true);assert(solve(node));goldenSolver(node,true);++cases;
    }
    resetProbe();solverInputs(false);Node disabled;assert(solve(disabled));goldenSolver(disabled,false);
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        resetProbe();solverInputs(true);Node time;
        assert(solve(time,invalid)==MS::kInvalidParameter&&time.solver.settings.empty());
        for(const char* name:{"aGravity","aAirDensity","aWindSpeed","aWindNoiseIntensity"}) {
            resetProbe();solverInputs(true);numericInputs[name]=invalid;Node scalar;
            assert(solve(scalar)==MS::kInvalidParameter&&scalar.solver.settings.empty());
        }
        for(const char* name:{"aGravityDirection","aWindDirection"})for(unsigned axis=0;axis<3;++axis) {
            resetProbe();solverInputs(true);vectorInputs[name][axis]=invalid;Node direction;
            assert(solve(direction)==MS::kInvalidParameter&&direction.solver.settings.empty());
        }
    }
    for(const char* name:{"aSubsteps","aSolverIterations"})for(int invalid:{0,-1}) {
        resetProbe();solverInputs(true);numericInputs[name]=invalid;Node iterations;
        assert(solve(iterations)==MS::kInvalidParameter&&iterations.solver.settings.empty());
    }
    // Validation permits finite signed physical settings and zero directions,
    // matching the old contract rather than silently clamping scene values.
    resetProbe();solverInputs(true);numericInputs["aGravity"]=-1;
    vectorInputs["aWindDirection"]={{0,0,0}};Node signedSettings;assert(solve(signedSettings));
    return cases;
}

int main() {
    resetProbe();auto baseline=fieldNode();assert(apply(baseline));goldenField(baseline);
    const auto trace=operations;int faults=0,shortArrays=0;
    for(int i=0;i<int(trace.size());++i) {
        for(int mode=0;mode<3;++mode) {
            resetProbe();auto node=fieldNode();const auto before=node.fNObject.velocities;
            if(mode==2)attributeThrowAt=i;
            else{failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
            assert(apply(node)==(mode==1?MS::kInvalidParameter:MS::kFailure));
            assert(callCount==i+1&&node.fNObject.velocities==before);
            if(i+1<int(trace.size()))assert(calls("native.velocities")==0);
            resetProbe();assert(apply(node));goldenField(node);++faults;
        }
        if(trace[i].find("resize")!=std::string::npos||trace[i].find("copy")!=std::string::npos) {
            resetProbe();shortArrayAt=i;auto node=fieldNode();rejected(node);++shortArrays;
        }
    }
    for(double step:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        resetProbe();auto node=fieldNode();assert(!apply(node,step));assert(operations.empty());
    }
    resetProbe();auto empty=fieldNode();empty.fNObject.count=0;assert(apply(empty));
    assert(operations==std::vector<std::string>{"particle.count"});
    resetProbe();auto huge=fieldNode();huge.fNObject.count=dommy::generation::kMaxGeneratedPointCount+1;
    assert(apply(huge)==MS::kInvalidParameter&&callCount==1);
    resetProbe();fields.clear();auto noFields=fieldNode();const auto unchanged=noFields.fNObject.velocities;
    assert(apply(noFields)&&noFields.fNObject.velocities==unchanged&&calls("native.velocities")==0);
    for(int mismatch=0;mismatch<8;++mismatch) {
        resetProbe();auto node=fieldNode();
        if(mismatch==0)node.fNObject.points.pop_back();
        if(mismatch==1)node.fNObject.points.push_back({});
        if(mismatch==2)node.fNObject.velocities.pop_back();
        if(mismatch==3)node.fNObject.velocities.push_back({});
        if(mismatch==4)node.fNObject.masses.pop_back();
        if(mismatch==5)node.fNObject.masses.push_back(1);
        if(mismatch==6)fields[1].pop_back();
        if(mismatch==7)fields[1].push_back({});
        rejected(node);
    }
    for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        for(int which=0;which<2;++which)for(unsigned axis=0;axis<4;++axis) {
            resetProbe();auto node=fieldNode();
            auto& point=which?node.fNObject.velocities[0]:node.fNObject.points[0];
            float* components[]={&point.x,&point.y,&point.z,&point.w};*components[axis]=value;
            // Compare only unchanged write count here: NaN does not equal itself.
            assert(!apply(node)&&calls("native.velocities")==0);
        }
        resetProbe();auto mass=fieldNode();mass.fNObject.masses[0]=value;rejected(mass);
        for(unsigned axis=0;axis<3;++axis) {
            resetProbe();auto node=fieldNode();double* components[]={&fields[1][0].x,&fields[1][0].y,&fields[1][0].z};
            *components[axis]=value;rejected(node);
        }
    }
    resetProbe();auto negativeMass=fieldNode();negativeMass.fNObject.masses[0]=-1;rejected(negativeMass);
    resetProbe();auto overflow=fieldNode();fields[0][0].x=fields[1][0].x=std::numeric_limits<double>::max();rejected(overflow);
    resetProbe();auto narrow=fieldNode();fields[0][0].x=fields[1][0].x=double(std::numeric_limits<float>::max())*8;rejected(narrow);
    resetProbe();auto timeOverflow=fieldNode();assert(!apply(timeOverflow,std::numeric_limits<double>::max()));assert(calls("native.velocities")==0);

    const int solverFaults=solverTests<DPnParticle>()+solverTests<DPnPopSolver>();
    std::cout<<"Nucleus simulation: "<<faults<<" field API faults/exceptions, "<<shortArrays
             <<" short arrays, "<<solverFaults<<" solver faults; field/solver mapping and numeric goldens passed\n";
}
