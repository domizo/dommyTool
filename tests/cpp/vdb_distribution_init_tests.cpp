#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include <fstream>
#include <iostream>
#include <limits>
// Used only to freeze the pre-hardening schema.
#define CHECK_MSTATUS(x) do {const MStatus s=(x);(void)s;} while(false);
#define CHECK_MSTATUS_AND_RETURN_IT(x) do {const MStatus s=(x);if(!s)return s;} while(false)
struct MFnStringData {
    MObject create(const char* value, MStatus* status=nullptr) {
        const auto s=operation("string.create");if(status)*status=s;
        return s?MObject{value}:MObject::kNullObj;
    }
};
struct DOpenVDBData {static constexpr MFnData::Type id=MFnData::kAny;};
struct DPData {static constexpr MFnData::Type id=MFnData::kAny;};
#include "vdb_distribution_init.inc"
int main(int argc,char**argv){
    assert(argc==2);
    const auto schema=schemaTrace("DOpenVDBScatter",DOpenVDBScatter::initialize)+
                      schemaTrace("DOpenVDBToSpheres",DOpenVDBToSpheres::initialize);
    if(std::string(argv[1])=="--emit"){std::cout<<schema;return 0;}
    std::ifstream file(argv[1]);assert(file.good());
    assert(schema==std::string((std::istreambuf_iterator<char>(file)),{}));
    const int counts[]={62,100};int index=0;
    for(auto init:{DOpenVDBScatter::initialize,DOpenVDBToSpheres::initialize}){
        const int count=counts[index++];testFailures("node",init,count);
        for(int i=0;i<count;++i){resetCalls();attributeThrowAt=i;assert(init()==MS::kFailure&&callCount==i+1);}
    }
    std::cout<<"Scatter/ToSpheres: frozen 62/100 operations, 324 status failures, 162 exceptions passed\n";
}
