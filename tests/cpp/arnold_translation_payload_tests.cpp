#include "arnold_translation_doubles.h"
#include <ImathVec.h>
#include <maya/MMatrix.h>
#include "integrations/arnold/DArnoldNumeric.h"
#include "common/utility/DPointTransform.h"
#include <limits>

// Exact conversion expressions use explicit renderer value sinks. This is not
// a full array/shape publication or native Arnold ownership test.
struct AtVector2 { float x, y; };
struct AtVector { float x, y, z; };
float submitted = 0.0f;
struct Array { void* get() const { return nullptr; } };
bool AiArraySetFlt(void*,std::size_t,float value) { submitted=value;return true; }
using DVec3Data = Imath::V3f;
using DVec3DataList = std::vector<Imath::V3f>;
enum DP_ATTR_DATA_TYPE { DP_ATTR_VEC3F, DP_ATTR_VEC3D };
DP_ATTR_DATA_TYPE orientationType=DP_ATTR_VEC3D;
struct OrientationAttribute { DP_ATTR_DATA_TYPE getAttrType() const { return orientationType; } };
struct OrientationCurve {
    std::vector<Imath::V3d> values;
    MMatrix matrix;
    bool identity=false;
    bool isExistAttr(const std::string&) const { return true; }
    const OrientationAttribute* getAttr(const std::string&) const { static OrientationAttribute attr;return &attr; }
    template<typename T> std::vector<T> attrListValue(const std::string&) const { return std::vector<T>(values.begin(),values.end()); }
    bool isIdentity() const { return identity; }
    MMatrix getTransformMatrix() const { return matrix; }
};
#ifndef DOMMY_SDK_CHECK
// Only needed to compile the old unchecked transform for red/mutation probes.
struct MVector {
    double v[3];
    MVector(double x,double y,double z):v{x,y,z}{}
    double operator[](std::size_t i) const { return v[i]; }
    MVector operator*(const MMatrix& m) const {
        return {v[0]*m[0][0]+v[1]*m[1][0]+v[2]*m[2][0],
                v[0]*m[0][1]+v[1]*m[1][1]+v[2]*m[2][1],
                v[0]*m[0][2]+v[1]*m[1][2]+v[2]*m[2][2]};
    }
};
struct MFloatVector {
    float v[3];
    MFloatVector(const MVector& value):v{float(value[0]),float(value[1]),float(value[2])}{}
    float operator[](std::size_t i) const { return v[i]; }
};
#endif
#include "translation_numeric.inc"

#ifndef DOMMY_SDK_CHECK
bool payloadValid=true,payloadEmpty=false;
int liveHandles=0;
struct MDataHandle {};
namespace dommy {namespace maya {
struct ScopedPlugDataHandle {
    MDataHandle value;
    ScopedPlugDataHandle(){++liveHandles;}
    ~ScopedPlugDataHandle(){--liveHandles;}
    MDataHandle& handle(){return value;}
    MStatus read(const MPlug&){return api("payload.read");}
};
}}
struct DPData { std::size_t numberOfPoints()const{return payloadEmpty?0U:1U;} };
struct DCurveData { std::size_t numberOfCurves()const{return payloadEmpty?0U:1U;} };
struct DMeshData { std::size_t numberOfMesh()const{return payloadEmpty?0U:1U;} };
template<typename T>const T* payloadBinding(){
    const MStatus status=api("payload.bind");static T data;
    return status&&payloadValid?&data:nullptr;
}
const DPData* getInputDPfromHandle(const MDataHandle&){return payloadBinding<DPData>();}
const DCurveData* getInputDCfromHandle(const MDataHandle&){return payloadBinding<DCurveData>();}
const DMeshData* getInputDMfromHandle(const MDataHandle&){return payloadBinding<DMeshData>();}
#include "translation_payload.inc"

template<typename Operation>void rejects(Operation operation){
    bool rejected=false;
    try{operation();}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
}
const double floatLimit=double(std::numeric_limits<float>::max());
const double outsideFloat=std::nextafter(floatLimit,std::numeric_limits<double>::infinity());
void scalarCase(float(*convert)(double)){
    for(double value:{0.0,1.25,-3.5,floatLimit,-floatLimit})assert(convert(value)==float(value));
    for(double value:{outsideFloat,-outsideFloat,1.0e300,-1.0e300,
                      std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        rejects([&]{(void)convert(value);});
}
void vector2Case(AtVector2(*convert)(Imath::V2d)){
    const auto value=convert(Imath::V2d(-1.25,floatLimit));assert(value.x==-1.25f&&value.y==floatLimit);
    for(std::size_t axis=0U;axis<2U;++axis)for(double invalid:{outsideFloat,-outsideFloat,1.0e300,std::numeric_limits<double>::quiet_NaN()}){
        Imath::V2d input(1.0,2.0);input[axis]=invalid;rejects([&]{(void)convert(input);});}
}
void vector3Case(AtVector(*convert)(Imath::V3d)){
    const auto value=convert(Imath::V3d(-1.25,floatLimit,2.0));assert(value.x==-1.25f&&value.y==floatLimit&&value.z==2.0f);
    for(std::size_t axis=0U;axis<3U;++axis)for(double invalid:{outsideFloat,-outsideFloat,1.0e300,std::numeric_limits<double>::quiet_NaN()}){
        Imath::V3d input(1.0,2.0,3.0);input[axis]=invalid;rejects([&]{(void)convert(input);});}
}
void scalarArrayCase(std::vector<float>(*convert)(const std::vector<double>&)){
    assert(convert({}).empty());assert(convert({1.25,-3.5,floatLimit})==std::vector<float>({1.25f,-3.5f,float(floatLimit)}));
    rejects([&]{(void)convert({1.25,outsideFloat});});
}
template<typename To,typename From>void vectorArrayCase(std::vector<To>(*convert)(const std::vector<From>&)){
    const From good(1.25,-2.0);const auto values=convert({good});assert(values.size()==1U&&values[0][0]==1.25f&&values[0][1]==-2.0f);
    From invalid=good;invalid[1]=outsideFloat;rejects([&]{(void)convert({good,invalid});});assert(convert({}).empty());
}
template<typename To,typename From>void integerVectorArrayCase(std::vector<To>(*convert)(const std::vector<From>&)){
    From input(std::numeric_limits<int>::max(),std::numeric_limits<int>::min());
    const auto values=convert({input});
    assert(values.size()==1U&&values[0][0]==float(input[0])&&values[0][1]==float(input[1]));
    assert(convert({}).empty());
}
void orientationCases(){
    MMatrix matrix;
    matrix[0][0]=-2.0;matrix[1][0]=0.5;matrix[1][1]=3.0;matrix[2][2]=0.0;
    matrix[3][0]=1.0e300;matrix[3][1]=-1.0e300; // Directions ignore translation.
    auto values=pointOrientation({Imath::V3d(1.25,2.0,1.0e300)},matrix,false);
    assert(values.size()==1U&&values[0][0]==-1.5f&&values[0][1]==6.0f&&values[0][2]==0.0f);
    rejects([&]{(void)pointOrientation({Imath::V3d(outsideFloat,1.0,2.0)},matrix,true);});
    matrix[0][0]=floatLimit;
    rejects([&]{(void)pointOrientation({Imath::V3d(2.0,0.0,0.0)},matrix,false);});
    matrix[0][0]=1.0;matrix[0][3]=1.0;
    rejects([&]{(void)pointOrientation({Imath::V3d(1.0,0.0,0.0)},matrix,false);});
    matrix[0][3]=0.0;orientationType=DP_ATTR_VEC3F;
    values=pointOrientation({Imath::V3d(1.25,2.0,3.0)},matrix,false);
    assert(values.size()==1U&&values[0][0]==2.25f&&values[0][1]==6.0f&&values[0][2]==0.0f);
    orientationType=DP_ATTR_VEC3D;
}
void resetPayload(){
    calls.clear();writes.clear();connections.clear();sources.clear();failAt=-1;kind=errors=reports=0;
    admitted=false;renderEnabled=motionEnabled=payloadValid=true;payloadEmpty=throwVisibility=false;
}
void visibilityGuardCases(){
    resetPayload();AtNode output;
    {dommy::arnold::UpdateVisibilityGuard guard(&output);output.visibility=4;guard.complete();}
    assert(output.visibility==4&&writes.empty());
    {dommy::arnold::UpdateVisibilityGuard guard(nullptr);}
    assert(writes.empty());
    throwVisibility=true;
    {dommy::arnold::UpdateVisibilityGuard guard(&output);}
    try{
        dommy::arnold::UpdateVisibilityGuard guard(&output);
        throw std::invalid_argument("original failure");
    }catch(const std::invalid_argument& error){assert(std::string(error.what())=="original failure");}
    dommy::arnold::reportTranslatorException("cleanup",&output,
        std::make_exception_ptr(std::invalid_argument("original failure")));
    assert(errors==1);throwVisibility=false;
}
template<typename TranslatorType>void payloadCase(void(TranslatorType::*method)(AtNode*),const char* input,bool renderControl,bool motion){
    TranslatorType translator;AtNode output;
    resetPayload();connections[input]=true;output.visibility=7;(translator.*method)(&output);
    assert(admitted&&errors==0&&liveHandles==0);const auto trace=calls;
    for(std::size_t index=0U;index<trace.size();++index)if(trace[index].find("payload.")==0||trace[index]=="renderer.stage"){
        for(int failure:{1,2,3}){
            resetPayload();connections[input]=true;output.visibility=7;failAt=int(index);kind=failure;
            (translator.*method)(&output);assert(!admitted&&errors>0&&output.visibility==0&&liveHandles==0);
            resetPayload();connections[input]=true;output.visibility=7;(translator.*method)(&output);
            assert(admitted&&errors==0&&liveHandles==0&&output.visibility==(renderControl?1:7));
        }
    }
    resetPayload();connections[input]=true;payloadValid=false;output.visibility=7;(translator.*method)(&output);
    assert(!admitted&&errors>0&&output.visibility==0&&liveHandles==0);
    resetPayload();connections[input]=true;payloadEmpty=true;output.visibility=7;(translator.*method)(&output);
    assert(!admitted&&errors==0&&output.visibility==(renderControl?1:7)&&liveHandles==0);
    resetPayload();connections[input]=false;output.visibility=7;(translator.*method)(&output);
    assert(!admitted&&errors==0&&output.visibility==(renderControl?1:7)&&liveHandles==0);
    if(renderControl){resetPayload();connections[input]=true;renderEnabled=false;(translator.*method)(&output);assert(!admitted&&output.visibility==0&&liveHandles==0);}
    if(motion){resetPayload();connections[input]=true;motionEnabled=false;output.visibility=7;(translator.*method)(&output);assert(!admitted&&writes.empty()&&output.visibility==7&&liveHandles==0);}
}
int main(){
    visibilityGuardCases();
#include "translation_payload_checks.inc"
}
#endif
