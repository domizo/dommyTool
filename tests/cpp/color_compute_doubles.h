#pragma once
// Complete color callbacks run with production attributes/converters/color math.
// DG ownership/copy, Maya arrays, shading, SeExpr and TBB execution are explicit
// test boundaries. This is not native Maya allocation/rendering or scheduling.
#define DOMMY_COLOR_COMPUTE_TESTS
#define DOMMY_PROCEDURAL_API_TESTS
#define MRampAttribute UnusedColorRampDeclaration
#define setPluginDataOutput unusedColorFixtureOutput
#include "copy_utility_compute_doubles.h"
#undef setPluginDataOutput
#undef MRampAttribute
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DColorPublication.h"
#include "common/utility/DColorBounds.h"
#include "common/utility/DExpressionResult.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DRandomValueGeneration.h"
#include "math/DRandomGenerator.h"
#include <atomic>

const int DPData::id;
template<int Id> const int PrimitiveData<Id>::id;
using uint = unsigned int;
using float3 = float[3];
using DVec3Data = Imath::V3f;
using DVec3DataList = std::vector<DVec3Data>;
using DColor = DVec3Data;
using DColorList = DVec3DataList;
using DReal = float;
using DCItem = std::shared_ptr<Primitive>;
using DMItem = DCItem;

struct MColor {
    float r=0, g=0, b=0, a=1;
    MColor()=default;
    MColor(double x,double y,double z):r(x),g(y),b(z){}
    float& operator[](unsigned i){assert(i<4);return i==0?r:i==1?g:i==2?b:a;}
    const float& operator[](unsigned i)const{assert(i<4);return i==0?r:i==1?g:i==2?b:a;}
};
struct MFloatPoint {
    float x=0,y=0,z=0,w=1;
    MFloatPoint()=default;
    MFloatPoint(float a,float b,float c):x(a),y(b),z(c){}
    float& operator[](unsigned i){assert(i<4);return i==0?x:i==1?y:i==2?z:w;}
    const float& operator[](unsigned i)const{assert(i<4);return i==0?x:i==1?y:i==2?z:w;}
};
struct MFloatVector : Imath::V3f {using Imath::V3f::V3f;};
static int shortArrayAt=-1, badArrayAccesses=0, samples=0;
template<class T> struct ColorArray {
    std::vector<T> values;
    static std::string apiName(const char* action) {
        return std::string(std::is_same<T,MFloatPoint>::value?"array.point.":"array.other.")+action;
    }
    ColorArray()=default;
    ColorArray(unsigned n,const T& value) {
        const auto status=operation(apiName("constructor"));
        if(status)values.assign(callCount-1==shortArrayAt&&n?n-1:n,value);
    }
    unsigned length()const{return static_cast<unsigned>(values.size());}
    MStatus setLength(unsigned n) {
        const auto status=operation(apiName("length"));
        if(status)values.resize(callCount-1==shortArrayAt&&n?n-1:n);
        return status;
    }
    MStatus copy(const ColorArray& source) {
        const auto status=operation(apiName("copy"));
        if(status){values=source.values;if(callCount-1==shortArrayAt&&!values.empty())values.pop_back();}
        return status;
    }
    T& operator[](unsigned i){if(i>=values.size())++badArrayAccesses;return values.at(i);}
    const T& operator[](unsigned i)const{if(i>=values.size())++badArrayAccesses;return values.at(i);}
};
using MDoubleArray=ColorArray<double>;
using MColorArray=ColorArray<MColor>;
using MFloatPointArray=ColorArray<MFloatPoint>;
using MFloatVectorArray=ColorArray<MFloatVector>;
using MVectorArray=ColorArray<MVector>;
static bool nonfiniteSample=false, shortSample=false, expressionValid=true;
static DPData stagedPoints;
static DCurveData stagedCurves;
static DMeshData stagedMeshes;
static DPData& stagedFor(DPData*){return stagedPoints;}
static DCurveData& stagedFor(DCurveData*){return stagedCurves;}
static DMeshData& stagedFor(DMeshData*){return stagedMeshes;}
struct MFnPluginData {
    MObject value;
    MObject object()const{return value;}
    MObject create(int id,MStatus* status) {
        *status=operation("factory.create");
        if(!*status||nullCreated)return {"null"};
        if(id==DPData::id)stagedPoints=DPData();
        else if(id==DCurveData::id)stagedCurves=DCurveData();
        else if(id==DMeshData::id)stagedMeshes=DMeshData();
        else return {"null"};
        value={std::to_string(id)};return value;
    }
};
MStatus MDataHandle::set(const MObject& object) {
    assert(valid);
    const auto status=operation("output.publish");if(!status)return status;
    if(object.name==std::to_string(DPData::id))outputPoints=stagedPoints;
    else if(object.name==std::to_string(DCurveData::id))outputCurves=stagedCurves;
    else if(object.name==std::to_string(DMeshData::id))outputMeshes=stagedMeshes;
    else assert(false);
    ++publications;return status;
}
namespace dommy { namespace maya {
template<class T>MStatus getPluginData(MFnPluginData& owner,T*& result) {
    result=nullptr;const auto status=operation("factory.bind");if(!status)return status;
    if(owner.value.name!=std::to_string(T::id))return MS::kFailure;
    result=&stagedFor(static_cast<T*>(nullptr));return status;
}
template<class T>T* pluginDataPointer(MFnPluginData& owner,MStatus* status) {
    T* result=nullptr;*status=getPluginData(owner,result);return result;
}
template<class T>MStatus getPluginDataFromObject(const MObject& object,int id,const T*& result) {
    result=nullptr;const auto status=operation("output.bind");if(!status)return status;
    if(object.name!=std::to_string(id)||id!=T::id)return MS::kFailure;
    result=&stagedFor(static_cast<T*>(nullptr));return status;
}
template<class T>bool validatePluginData(const T& value){return !rejectFinal&&value.valid();}
}}
inline void EchoWarning(const char*,const char*){}
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
#define DOMMY_MAYA_WARNING(context,message) do {(void)(context);}while(false)
struct MRampAttribute {
    explicit MRampAttribute(const MPlug&){}
    void getColorAtPosition(float position,MColor& color,MStatus* status) {
        *status=operation("ramp.sample");++samples;
        color=MColor(nonfiniteSample?NAN:position,0,1-position);
    }
};
namespace tbb {
template<class T>class blocked_range {
    T first_,last_;
public:
    blocked_range(T first,T last,std::size_t):first_(first),last_(last){}
    T begin()const{return first_;}T end()const{return last_;}
};
template<class Range,class Body>void parallel_for(const Range& range,Body body){body(range);}
}
inline std::size_t DGetGlobalProcessNum(std::size_t){return 1;}
struct DPointSeExpr {
    DPoint position;
    explicit DPointSeExpr(const std::string&){}
    bool isValid()const{return expressionValid;}
    bool usesPointDataFunctions()const{return false;}
    const char* parseError()const{return "fixture";}
    void setDPData(DPData*){}
    void setId(std::size_t){}
    void setPos(const DPoint& point){position=point;}
    Imath::V3d evaluate(){voidApi("expression.evaluate");return Imath::V3d(position);}
    bool evaluationSucceeded()const{return true;}
};
namespace dommy { namespace maya {
inline MStatus getEvaluationTime(MDataBlock&,MTime&){return operation("time.read");}
inline MStatus setExpressionTime(DPointSeExpr&,const MTime&){return operation("time.bind");}
}}
inline MStatus getColorFromTex(const MObject&,const MObject&,const MDoubleArray& u,
                              const MDoubleArray& v,MVectorArray& colors,MDoubleArray&) {
    const auto status=operation("texture.sample");++samples;if(!status)return status;
    assert(u.length()==v.length());
    colors.values.clear();
    for(unsigned i=0;i<u.length();++i)colors.values.emplace_back(nonfiniteSample?NAN:u[i],v[i],0);
    if(shortSample&&!colors.values.empty())colors.values.pop_back();
    return status;
}
inline MStatus getColorFromShaderNetwork(const MObject&,const MObject&,const MFloatPointArray& points,
                                        const MDoubleArray& u,const MDoubleArray& v,
                                        MFloatVectorArray& colors,MFloatVectorArray&) {
    const auto status=operation("shader.sample");++samples;if(!status)return status;
    assert(points.length()==u.length()&&u.length()==v.length());
    colors.values.clear();
    for(const auto& p:points.values) {
        assert(p.w==1); // Preserve Cartesian point export, not vector w=0.
        colors.values.emplace_back(nonfiniteSample?NAN:p.x,p.y,p.z);
    }
    if(shortSample&&!colors.values.empty())colors.values.pop_back();
    return status;
}
