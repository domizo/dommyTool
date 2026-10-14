#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#define CHECK_MSTATUS(x) do {const MStatus s=(x);(void)s;} while(false)
struct MFnStringData {MObject create(const char* v,MStatus* s=nullptr){const auto status=operation("string.create");if(s)*s=status;return {v};}};
struct DOpenVDBData {static constexpr MFnData::Type id=MFnData::kAny;};
struct DPData {static constexpr MFnData::Type id=MFnData::kAny;};
const MObject message{"message"};
namespace openvdb {
enum VecType {V0,V1,V2,V3,V4};
const int NUM_VEC_TYPES=5;
struct GridBase {static std::string vecTypeExamples(VecType t){return "enum_"+text(int(t));}};
}
#include "vdb_particle_init.inc"
int main(int argc,char** argv) {
    assert(argc==2);
    const auto trace=schemaTrace("DOpenVDBFromParticle",DOpenVDBFromParticle::initialize)+
        schemaTrace("DOpenVDBFromDP",DOpenVDBFromDP::initialize)+schemaTrace("DVDBFoamGen",DVDBFoamGen::initialize);
    if(std::string(argv[1])=="--emit"){std::cout<<trace;return 0;}
    std::ifstream file(argv[1]);assert(file.good());
    assert(trace==std::string((std::istreambuf_iterator<char>(file)),{}));
    for(auto init:{DOpenVDBFromParticle::initialize,DOpenVDBFromDP::initialize,DVDBFoamGen::initialize}) {
        resetCalls();assert(init());const int count=callCount;
        testFailures("particle initializer",init,count);
        for(int i=0;i<count;++i){resetCalls();attributeThrowAt=i;assert(init()==MS::kFailure&&callCount==i+1);}
        std::cout<<count<<" frozen operations: failure/exception checks passed\n";
    }
}
