#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
// Also permits recording the original, pre-hardening schema.
#define CHECK_MSTATUS(x) do {const MStatus s=(x);(void)s;} while(false)
struct MFnMeshData {
    MObject create(MStatus* s){*s=operation("mesh.create");return {"mesh"};}
};
struct MFnStringData {
    MObject create(const char* value,MStatus* s=nullptr){const auto status=operation("string.create");if(s)*s=status;return {value};}
};
struct DOpenVDBData {static constexpr MFnData::Type id=MFnData::kAny;};
struct DMeshData {static constexpr MFnData::Type id=MFnData::kAny;};
#include "vdb_mesh_init.inc"
int main(int argc,char** argv){
    assert(argc==2);const auto baseline=schemaTrace("DOpenVDBFromMesh",DOpenVDBFromMesh::initialize);
    if(std::string(argv[1])=="--emit"){std::cout<<baseline;return 0;}
    std::ifstream file(argv[1]);assert(file.good());
    assert(baseline==std::string((std::istreambuf_iterator<char>(file)),{}));
    testFailures("DOpenVDBFromMesh",DOpenVDBFromMesh::initialize,87);
    for(int i=0;i<87;++i){resetCalls();attributeThrowAt=i;assert(DOpenVDBFromMesh::initialize()==MS::kFailure&&callCount==i+1);}
    std::cout<<"FromMesh: frozen 87 operations, 174 status failures and 87 exceptions passed\n";
}
