#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

#define CHECK_MSTATUS(s) {const MStatus checked=(s);if(!checked)checked.pAPIerror(__FILE__,__LINE__);}
struct DPData {static constexpr MFnData::Type id=MFnData::kPointArray;};
struct DMeshData {static constexpr MFnData::Type id=MFnData::kMesh;};
struct MFnStringData {
    MObject create(const char* value,MStatus* status=nullptr) {
        const auto s=operation(std::string("string ")+value);
        if(status)*status=s;
        return s?MObject{value}:MObject::kNullObj;
    }
};
struct MFnMessageAttribute:Attribute {
    MObject create(const char* a,const char* b,MStatus* s=nullptr){return bind(a,b,"message",s);}
};
#include "nucleus_particle_init.inc"

int main(int argc,char** argv) {
    assert(argc==2);
    const std::string trace=schemaTrace("DPnParticle",DPnParticle::initialize)+
        schemaTrace("DPnPopSolver",DPnPopSolver::initialize);
    if(std::string(argv[1])=="--emit"){std::cout<<trace;return 0;}
    std::ifstream file(argv[1]);assert(file.good());
    assert(trace==std::string(std::istreambuf_iterator<char>(file),{}));
    int cases=0;
    for(auto initialize:{DPnParticle::initialize,DPnPopSolver::initialize}){
        resetCalls();assert(initialize());const int count=callCount;
        for(int i=0;i<count;++i){
            for(auto code:{MS::kFailure,MS::kInvalidParameter}){
                resetCalls(i);injectedStatus=code;
                assert(initialize()==code&&callCount==i+1);++cases;
            }
            resetCalls();attributeThrowAt=i;
            assert(initialize()==MS::kFailure&&callCount==i+1);++cases;
            resetCalls();assert(initialize());
        }
    }
    std::cout<<"Nucleus initializers: frozen schema and "<<cases<<" failure/exception cases passed\n";
}
