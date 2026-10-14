#pragma once
// Production callbacks, attribute math, planners, random streams, knot/array
// helpers and publication sequencing. Custom payload transport, ramps and
// native NURBS queries are explicit failure-injectable boundaries. The native
// curve query model is a straight segment, not a Maya geometry emulator.
#define DOMMY_PROCEDURAL_API_TESTS
#define MRampAttribute UnusedGeneratorRamp
#include "copy_utility_compute_doubles.h"
#undef MRampAttribute
#undef MMatrix
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DMayaCurveKnots.h"
#include "common/utility/DFurGenerationPlan.h"
#include "common/utility/DAhoGenerationPlan.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DPointPayloadBudget.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include "math/DRandomGenerator.h"
#include <cstring>

using PointBoundary=DPData;
struct GeneratorPointData:PointBoundary {
    template<class T>const std::vector<T>& attrListValue(const std::string& name)const{
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(getAttr(name))->value();
    }
    template<class T>T attrDefaultValue(const std::string& name)const{
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(getAttr(name))->getDefaultValue();
    }
};
#define DPData GeneratorPointData
using ConstDPointIter=DPointList::const_iterator;
using DScalarDataList=std::vector<double>;
using float3=float[3];
struct MStringArray:std::vector<CopyString> {
    unsigned length()const{return static_cast<unsigned>(size());}
};
MStatus CopyString::split(char separator,std::vector<CopyString>& output)const{
    const auto status=operation("names.split");if(!status)return status;
    std::istringstream stream(value);std::string token;
    while(std::getline(stream,token,separator))if(!token.empty())output.emplace_back(token);
    return status;
}
struct GeneratorQuaternion;
struct GeneratorVector:Imath::V3d {
    using Imath::V3d::V3d;
    GeneratorVector(const Imath::V3d& value):Imath::V3d(value){}
    static const GeneratorVector xAxis,yAxis;
#ifdef DOMMY_BEND_TESTS
    double w=1;
#endif
    double length()const{return std::sqrt(x*x+y*y+z*z);}
    MStatus normalize(){const auto status=operation("vector.normalize");if(status){const double n=length();if(n==0)return MS::kFailure;x/=n;y/=n;z/=n;}return status;}
    GeneratorVector operator^(const GeneratorVector& v)const{return {y*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x};}
    GeneratorVector operator*(const MMatrix& m)const{
        GeneratorVector result(x*m[0][0]+y*m[1][0]+z*m[2][0],x*m[0][1]+y*m[1][1]+z*m[2][1],x*m[0][2]+y*m[1][2]+z*m[2][2]);
#ifdef DOMMY_BEND_TESTS
        result+=GeneratorVector(m[3][0],m[3][1],m[3][2]);
#endif
        return result;
    }
#ifdef DOMMY_BEND_TESTS
    GeneratorVector transformAsNormal(const MMatrix& m)const{return {x*m[0][0]+y*m[1][0]+z*m[2][0],x*m[0][1]+y*m[1][1]+z*m[2][1],x*m[0][2]+y*m[1][2]+z*m[2][2]};}
#endif
    using Imath::V3d::operator*;
    GeneratorVector rotateBy(const GeneratorQuaternion&)const;
    GeneratorQuaternion rotateTo(const GeneratorVector&)const;
};
const GeneratorVector GeneratorVector::xAxis(1,0,0),GeneratorVector::yAxis(0,1,0);
struct GeneratorQuaternion {
    double angle;GeneratorVector axis;
    GeneratorQuaternion(double a,const GeneratorVector& v):angle(a),axis(v){}
};
GeneratorVector GeneratorVector::rotateBy(const GeneratorQuaternion& q)const{
    const double n=q.axis.length();if(n==0)return *this;
    const GeneratorVector axis(q.axis/n);const double c=std::cos(q.angle),s=std::sin(q.angle);
    const double dot=x*axis.x+y*axis.y+z*axis.z;
    return GeneratorVector(*this*c+(axis^*this)*s+axis*(dot*(1-c)));
}
GeneratorQuaternion GeneratorVector::rotateTo(const GeneratorVector& target)const{
    const double dot=std::max(-1.0,std::min(1.0,(x*target.x+y*target.y+z*target.z)/(length()*target.length())));
    GeneratorVector axis=*this^target;if(axis.length()<1e-12)axis=xAxis;
    return {std::acos(dot),axis};
}
#define MVector GeneratorVector
#define MPoint GeneratorVector
#define MQuaternion GeneratorQuaternion
struct GeneratorTransform {
    MMatrix matrix;
#ifdef DOMMY_BEND_TESTS
    MPoint pivot;
    MStatus setRotatePivot(const MPoint& value,MSpace::Space,bool balance){assert(balance);const auto status=operation("matrix.pivot");if(status)pivot=value;return status;}
#endif
    MStatus setToRotationAxis(const MVector& axis,double angle){
        const auto status=operation("matrix.axis");if(!status)return status;
        for(unsigned i=0;i<3;++i){MVector basis;basis[i]=1;const auto rotated=basis.rotateBy({angle,axis});for(unsigned j=0;j<3;++j)matrix[i][j]=rotated[j];}
        return status;
    }
    MMatrix asMatrix()const{
#ifdef DOMMY_BEND_TESTS
        voidApi("matrix.result");MMatrix result=matrix;
        const MVector rotated(pivot.x*matrix[0][0]+pivot.y*matrix[1][0]+pivot.z*matrix[2][0],
            pivot.x*matrix[0][1]+pivot.y*matrix[1][1]+pivot.z*matrix[2][1],
            pivot.x*matrix[0][2]+pivot.y*matrix[1][2]+pivot.z*matrix[2][2]);
        for(unsigned i=0;i<3;++i)result[3][i]=pivot[i]-rotated[i];return result;
#else
        return matrix;
#endif
    }
};
#define MTransformationMatrix GeneratorTransform
static int shortArrayAt=-1;
template<class T>struct GeneratorArray {
    std::vector<T> values;
    GeneratorArray()=default;
    explicit GeneratorArray(unsigned count){resize(count,"array.construct");}
    MStatus resize(unsigned count,const char* label){const auto status=operation(label);if(status)values.resize(callCount-1==shortArrayAt&&count?count-1:count);return status;}
    unsigned length()const{return static_cast<unsigned>(values.size());}
    MStatus setLength(unsigned count){return resize(count,"array.length");}
    MStatus copy(const GeneratorArray& source){const auto status=operation("array.copy");if(status){values=source.values;if(callCount-1==shortArrayAt&&!values.empty())values.pop_back();}return status;}
    MStatus append(const T& value){const auto status=operation("array.append");if(status&&callCount-1!=shortArrayAt)values.push_back(value);return status;}
    T& operator[](unsigned i){return values.at(i);}
    const T& operator[](unsigned i)const{return values.at(i);}
};
using MDoubleArray=GeneratorArray<double>;
using MFloatArray=GeneratorArray<float>;
using MPointArray=GeneratorArray<MPoint>;
using MVectorArray=GeneratorArray<MVector>;
static bool shortRamp=false,badRamp=false,shortBatch=false,nullNative=false;
struct GeneratorCurve:Metadata {
    DPointList points;std::vector<double> knots;int degree=1;bool topology=true;
    GeneratorCurve()=default;
    GeneratorCurve(const MPointArray& p,const MDoubleArray& k,int d):knots(k.values),degree(d){
        voidApi("curve.construct");for(const auto& v:p.values){DPoint converted;if(!dommy::numeric::checkedValueCast(Imath::V3d(v.x,v.y,v.z),converted))throw std::range_error("curve CV");points.push_back(converted);}
    }
    bool hasValidCurveTopology()const{return topology&&!points.empty();}
    std::size_t numberOfPoints()const{return points.size();}
#ifdef DOMMY_BEND_TESTS
    const DPointList* getPositionPtrConst()const{return &points;}
    bool isExistAttr(const std::string& name)const{return attributes.count(name)!=0;}
    DPAttributeData::ConstPtr getAttr(const std::string& name)const{return attributes.at(name);}
    bool replacePositions(const MPointArray& values){
        if(!operation("curve.replace"))return false;
        DPointList candidate;for(const auto& p:values.values){DPoint v;
            if(!dommy::numeric::checkedValueCast(Imath::V3d(p.x,p.y,p.z),v))return false;candidate.push_back(v);}
        points.swap(candidate);return true;
    }
#endif
};
#define DCurveInternalData GeneratorCurve
using DCItem=std::shared_ptr<GeneratorCurve>;
struct GeneratorCurveData:Payload,Metadata {
    static const int id=23;
    using DCurveList=std::vector<DCItem>;
    using ConstDCurveIter=DCurveList::const_iterator;
    DCurveList curves;bool topology=true;
    int type()const override{return id;}
    ConstDCurveIter beginCurves()const{return curves.begin();}
    ConstDCurveIter endCurves()const{return curves.end();}
    std::size_t numberOfCurves()const{return curves.size();}
    std::size_t getTotalCvs()const{std::size_t n=0;for(const auto& curve:curves)n+=curve->points.size();return n;}
    bool valid()const override{
        dommy::attribute::ValidationBudget budget(curves.size());
        if(!topology||!dommy::attribute::validateAttributeList(attributes,curves.size(),budget))return false;
        for(const auto& curve:curves){if(!curve||!curve->hasValidCurveTopology())return false;for(const auto& p:curve->points)for(unsigned a=0;a<3;++a)if(!std::isfinite(p[a]))return false;}return true;
    }
    bool hasValidCurveTopology()const{return valid();}
#ifdef DOMMY_BEND_TESTS
    MStatus copyFrom(const GeneratorCurveData& input){
        const auto status=operation("curve.copy");if(!status)return status;
        curves.clear();topology=input.topology;attributes.clear();groups=input.groups;
        for(const auto& attr:input.attributes)attributes[attr.first]=attr.second->copy();
        for(const auto& curve:input.curves){auto copy=std::make_shared<GeneratorCurve>(*curve);
            copy->attributes.clear();for(const auto& attr:curve->attributes)copy->attributes[attr.first]=attr.second->copy();curves.push_back(copy);}
        return status;
    }
#endif
    bool insert(const DCurveList& source){
        if(!operation("curve.batch"))return false;
        for(const auto& curve:source)curves.push_back(std::make_shared<GeneratorCurve>(*curve));
        if(shortBatch&&!curves.empty())curves.pop_back();return true;
    }
    void addAttr(const std::string& name,const DPAttributeData& field){voidApi("curve.attribute");attributes[name]=field.copy();}
    void updateBBox(){voidApi("curve.bounds");}
};
const int GeneratorCurveData::id;
#define DCurveData GeneratorCurveData
static DPData generatorPoints;
static DCurveData generatorInput,generatorStaged,generatorOutput;
static std::vector<MObject> nativeOutput,nativeStaged;
static std::map<std::string,const GeneratorCurve*> nativeObjects;
struct MFnPluginData {
    MObject value;
    MObject create(int id,MStatus* status){*status=operation("factory.create");if(!*status||nullCreated)return MObject::kNullObj;assert(id==DCurveData::id);generatorStaged=DCurveData();value={"curves"};return value;}
    MObject object()const{return value;}
};
using BaseHandle=MDataHandle;
struct GeneratorHandle:BaseHandle {
    bool element=false;std::size_t index=0;
    GeneratorHandle()=default;
    GeneratorHandle(const BaseHandle& h):BaseHandle(h){}
    MStatus set(const MObject& object){
        assert(valid);const auto status=operation(element?"array.element.set":"output.publish");if(!status)return status;
        if(element)nativeStaged.at(index)=object;
        else{assert(object.name=="curves");generatorOutput=generatorStaged;++publications;}return status;
    }
};
#define MDataHandle GeneratorHandle
struct MArrayDataBuilder;
struct GeneratorArrayHandle {
    MStatus set(const MArrayDataBuilder&);
    MStatus setAllClean(){return operation("array.clean");}
};
#define MArrayDataHandle GeneratorArrayHandle
using BaseBlock=MDataBlock;
struct GeneratorBlock:BaseBlock {
    MDataHandle inputValue(const MObject& attribute,MStatus* status=nullptr){return BaseBlock::inputValue(attribute,status);}
    MDataHandle outputValue(const MObject& attribute,MStatus* status=nullptr){return BaseBlock::outputValue(attribute,status);}
    MArrayDataHandle outputArrayValue(const MObject&,MStatus* status){*status=operation("array.output");return {};}
};
#define MDataBlock GeneratorBlock
struct MArrayDataBuilder {
    MArrayDataBuilder(MDataBlock*,const MObject&,unsigned n,MStatus* status){*status=operation("array.builder");if(*status)nativeStaged.assign(n,MObject::kNullObj);}
    MDataHandle addElement(unsigned i,MStatus* status){*status=operation("array.element");MDataHandle handle;handle.valid=true;handle.element=true;handle.index=i;return handle;}
};
MStatus GeneratorArrayHandle::set(const MArrayDataBuilder&){const auto status=operation("array.publish");if(status){nativeOutput=nativeStaged;++publications;}return status;}
namespace dommy {namespace maya {
inline bool isFloatRepresentablePoint(const MPoint& p){return numeric::canRepresentAsFloat(p.x)&&numeric::canRepresentAsFloat(p.y)&&numeric::canRepresentAsFloat(p.z);}
template<class T>MStatus getPluginData(MFnPluginData& factory,T*& output){output=nullptr;const auto status=operation("factory.bind");if(!status)return status;if(factory.value.isNull())return MS::kFailure;output=&generatorStaged;return status;}
template<class T>MStatus getPluginDataFromObject(const MObject& object,int,const T*& output){output=nullptr;const auto status=operation("output.bind");if(status&&object.name=="curves")output=&generatorStaged;return status;}
template<class T>bool validatePluginData(const T& value){return !rejectFinal&&value.valid();}
}}
struct MRampAttribute {
    explicit MRampAttribute(const MPlug&,MStatus* status=nullptr){const auto result=operation("ramp.bind");if(status)*status=result;else if(!result)throw std::runtime_error("ramp binding");}
    void sampleValueRamp(unsigned n,MFloatArray& values,MStatus* status){*status=operation("ramp.sample");if(*status){values.values.assign(shortRamp&&n?n-1:n,badRamp?NAN:1.0f);}}
};
inline MStatus generateMCurveFromDcurve(const DCItem& curve,MObject& object){
    const auto status=operation("native.curve");if(!status)return status;
    object=nullNative?MObject::kNullObj:MObject{"native"+std::to_string(nativeObjects.size())};
    if(!object.isNull())nativeObjects[object.name]=curve.get();return status;
}
constexpr double kMFnNurbsEpsilon=1e-6;
struct MFnNurbsCurve {
    const GeneratorCurve* curve=nullptr;
#ifdef DOMMY_BEND_TESTS
    MPointArray nativeCVs;
    MFnNurbsCurve()=default;
    MStatus setObject(const MObject& object){const auto status=operation("native.bind");if(!status)return status;
        const auto found=nativeObjects.find(object.name);if(found==nativeObjects.end())return MS::kFailure;curve=found->second;return status;}
    MPoint closestPoint(const MPoint& point,double* parameter,double,MSpace::Space,MStatus* status)const {
        *status=operation("native.closest");*parameter=point.x;return point;
    }
    MVector normal(double,MSpace::Space,MStatus* status)const{*status=operation("native.normal");return {0,1,0};}
    MStatus setCVs(const MPointArray& points,MSpace::Space){const auto status=operation("native.setCVs");if(status)nativeCVs=points;return status;}
    MStatus updateCurve(){return operation("native.update");}
#endif
    MFnNurbsCurve(const MObject& object,MStatus* status){*status=operation("native.bind");if(*status){const auto found=nativeObjects.find(object.name);if(found==nativeObjects.end())*status=MS::kFailure;else curve=found->second;}}
    MStatus getKnotDomain(double& start,double& end){const auto status=operation("native.domain");start=0;end=1;return status;}
    MVector direction()const{assert(curve);const auto& a=curve->points.front();const auto& b=curve->points.back();return {b.x-a.x,b.y-a.y,b.z-a.z};}
    double length(double,MStatus* status)const{*status=operation("native.length");return direction().length();}
    double findParamFromLength(double value,MStatus* status)const{*status=operation("native.parameter");return value/direction().length();}
    MStatus getPointAtParam(double value,MPoint& point,MSpace::Space){const auto status=operation("native.point");const auto& p=curve->points.front();point=MPoint(p.x,p.y,p.z)+direction()*value;return status;}
    MVector tangent(double,MSpace::Space,MStatus* status)const{*status=operation("native.tangent");return direction();}
};
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
