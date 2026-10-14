#include "point_foam_doubles.h"
#include "point_foam.inc"

namespace {
MDataBlock setup(int mode=0){
    auto block=resetRun();inputPoints=DPData();pendingMeshes=nullptr;missingPoints=false;
    publishedMeshes=DMeshData();block.path="pscale";block.meshType=DPData::id;
    block.controls={{"Frequency",1},{"Radius",1},{"RadiusOffset",0},
        {"RadiusResultOffset",0},{"SelfDeform",mode==0?1:0}};
    if(mode!=2)inputPoints.points={{{0,0,0}},{{1,0,0}}};
    resetCalls();return block;
}
MStatus compute(MDataBlock& block){return DPFoamMesh().compute({DPFoamMesh::outDM},block);}
void unchanged(){assert(published==0 && cleaned==0 && output.name=="old");}
void testEmpty(){
    auto block=setup(2);assert(compute(block)==MS::kSuccess);
    assert(published==1 && publishedMeshes.values.empty() && cleaned>0);
}
void testFailures(){
    int failures=0,exceptions=0;
    for(int mode:{0,1,2}){
        auto block=setup(mode);assert(compute(block)==MS::kSuccess);
        const auto trace=operations;
        if(mode!=2){
            assert(publishedMeshes.values.size()==1);
            assert(publishedMeshes.values[0]->points.size()==24);
            assert(publishedMeshes.values[0]->counts.size()==40);
            dommy::geometry::IcosahedronTopology topology;
            assert(dommy::geometry::buildHistoricalIcosahedron(1,topology));
            bool changed=false;
            for(std::size_t point=0;point<topology.points.size();++point){
                const auto& actual=publishedMeshes.values[0]->points[point];
                const auto& expected=topology.points[point];
                const double difference=std::fabs(actual.x-expected.x)+
                    std::fabs(actual.y-expected.y)+std::fabs(actual.z-expected.z);
                if(mode==1)assert(difference<1e-6);
                else if(difference>1e-4)changed=true;
            }
            if(mode==0)assert(changed);
        }
        for(std::size_t at=0;at<trace.size();++at){
            for(auto status:{MS::kFailure,MS::kInvalidParameter}){
                block=setup(mode);failAt=int(at);injectedStatus=status;
                const auto result=compute(block);
                assert(result!=MS::kSuccess && callCount==int(at)+1);
                if(trace[at]=="array.copy")assert(result==status);
                ++failures;
                if(trace[at]!="data.clean")unchanged();
            }
            block=setup(mode);attributeThrowAt=int(at);
            assert(compute(block)==MS::kFailure && callCount==int(at)+1);++exceptions;
            if(trace[at]!="data.clean")unchanged();
            if(trace[at]=="array.length" || trace[at]=="array.copy" || trace[at]=="array.append"){
                block=setup(mode);shortAt=int(at);assert(compute(block)!=MS::kSuccess);unchanged();
            }
            assert(inputPoints.points.size()==(mode==2?0U:2U));
        }
        block=setup(mode);assert(compute(block)==MS::kSuccess);
    }
    std::cout<<"DPFoamMesh: "<<failures<<" status failures and "<<exceptions<<" exceptions passed\n";
}
void testInvalid(){
    auto block=setup();missingPoints=true;assert(compute(block)!=MS::kSuccess);unchanged();
    block=setup();inputPoints.valid=false;assert(compute(block)!=MS::kSuccess);unchanged();
    block=setup();block.meshType=MFnData::kMesh;assert(compute(block)!=MS::kSuccess);unchanged();
    for(int mode:{1,4,5}){
        block=setup();nullMode=mode;assert(compute(block)!=MS::kSuccess);unchanged();
        assert(std::find(operations.begin(),operations.end(),"mesh.convert")==operations.end());
    }
    for(const auto* name:{"Radius","RadiusOffset","RadiusResultOffset"}){
        block=setup();block.controls[name]=NAN;assert(compute(block)!=MS::kSuccess);unchanged();
    }
    block=setup();inputPoints.attributes["pscale"]={1};assert(compute(block)!=MS::kSuccess);unchanged();
    block=setup();inputPoints.attributes["pscale"]={1,2};assert(compute(block)==MS::kSuccess);
    // A later empty evaluation must replace, not retain, this mesh output.
    inputPoints.points.clear();resetCalls();published=cleaned=0;
    assert(compute(block)==MS::kSuccess && published==1 && publishedMeshes.values.empty());
    block=setup();assert(DPFoamMesh().compute({{"unknown"}},block)==MS::kUnknownParameter);
    assert(callCount==0);unchanged();
}
}
int main(){assert(DPFoamMesh::initialize()==MS::kSuccess);testEmpty();testFailures();testInvalid();}
