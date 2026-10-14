#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#define CHECK_MSTATUS(x) do {const MStatus s=(x);(void)s;} while(false)
struct MFnStringData {MObject create(const char* v,MStatus* s=nullptr){const auto status=operation("string.create");if(s)*s=status;return {v};}};
struct DOpenVDBData {static constexpr MFnData::Type id=MFnData::kAny;};
struct DMeshData {static constexpr MFnData::Type id=MFnData::kAny;};
#include "vdb_meshing_init.inc"
int main(int argc,char** argv){
    assert(argc==2);
    const auto trace=schemaTrace("DOpenVDBToMesh",DOpenVDBToMesh::initialize)+schemaTrace("DOpenVDBConvert",DOpenVDBConvert::initialize);
    if(std::string(argv[1])=="--emit"){std::cout<<trace;return 0;}
    std::ifstream file(argv[1]);assert(file.good());assert(trace==std::string((std::istreambuf_iterator<char>(file)),{}));
    testFailures("DOpenVDBToMesh",DOpenVDBToMesh::initialize,48);testFailures("DOpenVDBConvert",DOpenVDBConvert::initialize,120);
    for(auto init:{DOpenVDBToMesh::initialize,DOpenVDBConvert::initialize}){
        resetCalls();assert(init());const int count=callCount;
        for(int i=0;i<count;++i){resetCalls();attributeThrowAt=i;assert(init()==MS::kFailure&&callCount==i+1);}}
    std::cout<<"ToMesh/Convert: frozen 48/120 operations, 336 status failures, 168 exceptions passed\n";
}
