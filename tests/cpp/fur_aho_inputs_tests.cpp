#include "common/utility/DStatusOperation.h"
#include <array>
#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>

static MStatus returnedStatus=MS::kSuccess;
static bool nullTuple=false;
static int acquisitions=0, getters=0, admissions=0;
static float raw[]={1,2,3};
struct MObject {};
struct MString {
    const char* asChar() const {return "attribute";}
};
struct MDataHandle {
    void read() const {
        ++getters;
        if(returnedStatus!=MS::kSuccess)throw std::runtime_error("failed handle read");
    }
    int asInt()const{read();return 1;}
    float asFloat()const{read();return 1;}
    bool asBool()const{read();return true;}
    const float* asFloat3()const{read();return nullTuple?nullptr:raw;}
    MString asString()const{read();return {};}
};
struct MDataBlock {
    MDataHandle inputValue(MObject,MStatus* status) {
        ++acquisitions;*status=returnedStatus;return {};
    }
};
#include "fur_aho_inputs.inc"
int main(){
    for(auto probe:probes){
        for(auto status:{MS::kSuccess,MS::kFailure,MS::kInvalidParameter}){
            acquisitions=getters=admissions=0;raw[0]=1;nullTuple=false;returnedStatus=status;
            assert(probe()==status);
            assert(acquisitions==1&&getters==(status==MS::kSuccess?1:0));
            assert(admissions==(status==MS::kSuccess?1:0));
        }
    }
    returnedStatus=MS::kSuccess;nullTuple=true;acquisitions=getters=admissions=0;
    assert(probes[7]()==MS::kFailure&&getters==1&&admissions==0);
    std::cout<<"Fur: 14 fail-fast input reads and owned/null tuple handling passed\n";
}
