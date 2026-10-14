#include "common/utility/DStatusOperation.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct MObject {std::string name;};
struct MString {std::string value;const char* asChar()const{return value.c_str();}};
using float3 = float[3];
using NoiseDeformType = short;
using NoiseCalcPositionType = short;
struct DVec3Data {DVec3Data(float,float,float){}};
static int reads=0, failAt=-1;
static MStatus failure=MS::kFailure;
static std::vector<std::string> attributes;
struct MDataHandle {
    bool valid;
    bool asBool()const{assert(valid);return true;}
    short asShort()const{assert(valid);return 0;}
    int asInt()const{assert(valid);return 3;}
    float asFloat()const{assert(valid);return 1.0f;}
    double asDouble()const{assert(valid);return 1.0;}
    MString asString()const{assert(valid);return {"param"};}
    const float3& asFloat3()const{assert(valid);static const float3 v={1,2,3};return v;}
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attribute,MStatus* status){
        assert(failAt<0 || reads<=failAt);
        attributes.push_back(attribute.name);
        *status=reads++==failAt ? failure : MStatus(MS::kSuccess);
        return {*status==MS::kSuccess};
    }
};
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "curve_conversion_controls.inc"

int main(){
    MStatus (*functions[])(MDataBlock&)={DCNoise::controls,DSpiralCurve::controls,woolCurve::controls};
    const std::vector<std::vector<std::string>> expected={
        {"aUseSeedOffset","aIsNoiseAttribute","aDeformType","aNoiseCalcPosition","aAmplitube",
         "aDeformVector","aPositionAttribute","aNoiseAttributeName","aVectorAttributeName"},
        {"curveLength","sampling","offset","spiralOffset","radius","arcStep"},
        {"startRadius","endRadius","round","sections","zTrans"}};
    MDataBlock data;
    for(int n=0;n<3;++n){
        reads=0;failAt=-1;attributes.clear();
        assert(functions[n](data)==MS::kSuccess && attributes==expected[n]);
        for(MS::MStatusCode status:{MS::kFailure,MS::kInvalidParameter}) {
            failure=status;
            for(std::size_t i=0;i<expected[n].size();++i){
                reads=0;failAt=static_cast<int>(i);attributes.clear();
                assert(functions[n](data)==status && reads==failAt+1);
                assert(attributes==std::vector<std::string>(expected[n].begin(),expected[n].begin()+i+1));
            }
        }
    }
    std::cout<<"Noise/Spiral/wool controls: 20 reads, 40 fail-before-access cases passed\n";
}
