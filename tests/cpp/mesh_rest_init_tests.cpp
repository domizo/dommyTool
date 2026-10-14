#include "maya_attribute_doubles.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#define CHECK_MSTATUS(x) {const MStatus s=(x);(void)s;}
struct MFnStringData {};
struct DMeshData {static constexpr MFnData::Type id=MFnData::kAny;};
#include "mesh_rest_init.inc"
int main(int argc,char** argv) {
    assert(argc==2);
    const auto trace=schemaTrace("DMRest",DMRest::initialize);
    const int count=callCount;
    if(std::string(argv[1])=="--emit"){std::cout<<trace;return 0;}
    std::ifstream file(argv[1]);assert(file.good());
    assert(trace==std::string((std::istreambuf_iterator<char>(file)),{}));
    testFailures("DMRest",DMRest::initialize,count);
    for(int i=0;i<count;++i){resetCalls();attributeThrowAt=i;
        assert(DMRest::initialize()==MS::kFailure && callCount==i+1);}
    std::cout<<"DMRest: "<<count<<" frozen operations, status failures and exceptions passed\n";
}
