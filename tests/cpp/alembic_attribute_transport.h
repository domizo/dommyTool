#pragma once
#include "common/utility/DAttributeExport.h"
#include "common/utility/DExportAttributeState.h"
#include "common/file/DAtomicFile.h"
#include <cassert>
#include <fstream>
#include <list>
#include <typeindex>
#include <stdexcept>

using DA_BOOL=bool;using DA_INT=std::int32_t;using DA_UINT=std::uint32_t;using DA_INT64=std::int64_t;
using DA_FLOAT=float;using DA_DOUBLE=double;
using DA_VEC2I=Imath::V2i;using DA_VEC2F=Imath::V2f;using DA_VEC2D=Imath::V2d;
using DA_VEC3I=Imath::V3i;using DA_VEC3F=Imath::V3f;using DA_VEC3D=Imath::V3d;

std::vector<std::string> calls;int failAt=-1,kind=0;
void transport(const char* label) {
    calls.push_back(label);if(int(calls.size())-1==failAt) {
        if(kind==2)throw std::bad_alloc();throw std::runtime_error("attribute transport fault");
    }
}

// Explicit typed archive transport. No native Alembic library is linked.
namespace Alembic {namespace Util {using bool_t=std::uint8_t;using uint32_t=std::uint32_t;
using int32_t=std::int32_t;using int64_t=std::int64_t;using float32_t=float;using float64_t=double;}
namespace Abc {
struct Payload {virtual ~Payload()=default;virtual std::size_t size()const=0;virtual std::type_index type()const=0;};
template<typename T>struct Values:Payload {std::vector<T> data;Values(const T* p,std::size_t n){if(n)data.assign(p,p+n);}
    std::size_t size()const override{return data.size();}std::type_index type()const override{return typeid(T);}};
struct ArraySample {std::shared_ptr<const Payload> payload;
    std::size_t size()const{return payload?payload->size():0;}};
template<typename T>struct TypedArraySample:ArraySample {
    TypedArraySample()=default;TypedArraySample(const T* p,std::size_t n){this->payload=std::make_shared<Values<T>>(p,n);}};
struct Record {int scope=0;unsigned sampling=0;std::type_index type{typeid(void)};std::vector<ArraySample> frames;};
struct OArrayProperty {std::shared_ptr<Record> record;
    std::size_t getNumSamples()const{transport("property.count");return record->frames.size();}
    void setTimeSampling(unsigned value){transport("property.sampling");record->sampling=value;}
    void set(ArraySample sample){transport("property.set");if(!sample.payload||sample.payload->type()!=record->type)throw std::invalid_argument("wrong POD");record->frames.push_back(sample);}};
struct OCompoundProperty {std::shared_ptr<std::map<std::string,std::shared_ptr<Record>>> records;
    const Record* getPropertyHeader(const std::string& name)const{transport("property.header");auto found=records->find(name);return found==records->end()?nullptr:found->second.get();}};
}
namespace AbcGeom {
using bool_t=Util::bool_t;using float64_t=double;
using V2i=Imath::V2i;using V2f=Imath::V2f;using V2d=Imath::V2d;
using V3i=Imath::V3i;using V3f=Imath::V3f;using V3d=Imath::V3d;
enum GeometryScope {kConstantScope=0,kUniformScope=1,kVaryingScope=2,kVertexScope=3,kFacevaryingScope=4,kUnknownScope=127};
template<typename T>struct GeomParam {
    struct Sample {Abc::TypedArraySample<T> data;GeometryScope scope=kVertexScope;
        void setScope(GeometryScope value){scope=value;}void setVals(Abc::TypedArraySample<T> value){data=value;}
        GeometryScope getScope()const{return scope;}
        const Abc::TypedArraySample<T>& getVals()const{return data;}};
    Abc::OArrayProperty property;
    GeomParam(Abc::OCompoundProperty& parent,const std::string& name,bool,GeometryScope scope,unsigned,unsigned time) {
        transport("property.new");property.record=std::make_shared<Abc::Record>();property.record->type=typeid(T);
        property.record->scope=scope;property.record->sampling=time;parent.records->emplace(name,property.record);
    }
    void set(const Sample& sample){property.set(sample.getVals());}
    Abc::OArrayProperty getValueProperty()const{transport("property.handle");return property;}
};
#define PARAM(name,type) using name##ArraySample=Abc::TypedArraySample<type>;using O##name##GeomParam=GeomParam<type>
PARAM(Bool,Util::bool_t);PARAM(Int32,std::int32_t);PARAM(UInt32,std::uint32_t);PARAM(Int64,std::int64_t);
PARAM(Float,float);PARAM(Double,double);PARAM(V2i,Imath::V2i);PARAM(V2f,Imath::V2f);PARAM(V2d,Imath::V2d);
PARAM(V3i,Imath::V3i);PARAM(V3f,Imath::V3f);PARAM(V3d,Imath::V3d);
#undef PARAM
}}

struct DPData {
    std::size_t points=2;int faces=2;
    std::map<std::string,DPAttributeData::ConstPtr> fields;
    std::size_t numberOfPoints()const{return points;}int numFace()const{return faces;}
    DPAttributeData::ConstPtr getAttr(const std::string& name)const{const auto found=fields.find(name);return found==fields.end()?DPAttributeData::ConstPtr():found->second;}
    bool isExistAttr(const std::string& name)const{return fields.count(name)!=0;}
};
struct Collection {
    std::vector<std::shared_ptr<DPData>> owners;
    std::map<std::string,DPAttributeData::ConstPtr> fields;
    auto beginCurves()const->decltype(owners.begin()){return owners.begin();}auto endCurves()const->decltype(owners.end()){return owners.end();}
    const std::vector<std::shared_ptr<DPData>>* getCurveListConst()const{return &owners;}
    const std::vector<std::shared_ptr<DPData>>* getMeshListConst()const{return &owners;}
    std::size_t numberOfCurves()const{return owners.size();}std::size_t numberOfMesh()const{return owners.size();}
    bool isExistAttr(const std::string& name)const{return fields.count(name)!=0;}
    DPAttributeData::ConstPtr getAttr(const std::string& name)const {auto found=fields.find(name);return found==fields.end()?DPAttributeData::ConstPtr():found->second;}
    bool isExistPointAttr(const std::string& name)const{for(const auto& owner:owners)if(owner&&owner->getAttr(name))return true;return false;}
    DP_ATTR_DATA_TYPE getPointAttributeType(const std::string& name)const{for(const auto& owner:owners)if(owner&&owner->getAttr(name))return owner->getAttr(name)->getAttrType();return DP_ATTR_INVALID;}
    template<typename T>const std::vector<T>& attrListValue(const std::string& name)const{return std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(getAttr(name))->value();}
};
using DCurveData=Collection;using DMeshData=Collection;
using DAbcPointAttrList=std::map<std::string,Alembic::Abc::OArrayProperty>;
struct ShapeSchema {std::shared_ptr<std::map<std::string,std::shared_ptr<Alembic::Abc::Record>>> records=
    std::make_shared<std::map<std::string,std::shared_ptr<Alembic::Abc::Record>>>();
    Alembic::Abc::OCompoundProperty getArbGeomParams()const{return {records};}};
struct WriterState {ShapeSchema mSchema;DAbcPointAttrList pointAttributeList;unsigned mTimeIndex=1;
    dommy::attribute::ExportAttributeState mAttributeState;
    explicit WriterState(std::shared_ptr<dommy::attribute::ExportAttributeBudget> budget):mAttributeState(std::move(budget)){} };
struct DPointWriter:WriterState {using WriterState::WriterState;void getDPointPointAttribute(const DPData&,const std::list<std::string>&,bool);};
struct DCurveWriter:WriterState {using WriterState::WriterState;
    void getDCurvePrimAttribute(const DCurveData&,const std::list<std::string>&,bool);void getDCurvePointAttribute(const DCurveData&,const std::list<std::string>&,bool);};
struct DMeshWriter:WriterState {using WriterState::WriterState;
    void getDMeshPrimAttribute(const DMeshData&,const std::list<std::string>&,bool);void getDMeshPointAttribute(const DMeshData&,const std::list<std::string>&,bool);};
