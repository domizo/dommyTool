#pragma once
#include <maya/MStatus.h>
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DExportAttributeState.h"
#include "common/file/DAtomicFile.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef DOMMY_SDK_CHECK
#include <maya/MFnAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MString.h>
#include <maya/MGlobal.h>
#include <maya/MFnDependencyNode.h>
#else
namespace MFn {enum Type {kNumericAttribute=1,kTypedAttribute,kUnitAttribute,kEnumAttribute,
    kData2Double,kData3Double,kData4Double,kData2Float,kData3Float,kData2Int,kData3Int,
    kData2Short,kData3Short,kStringData,kStringArrayData,kFloatVectorArrayData,kVectorArrayData,
    kFloatArrayData,kDoubleArrayData,kIntArrayData,kPointArrayData,kUInt64ArrayData};}
struct MFnData {enum Type {kNumeric,kString,kMatrix,kStringArray,kDoubleArray,kIntArray,kPointArray,kVectorArray};};
struct MFnNumericData {
    enum Type {kInvalid,kBoolean,kByte,kChar,kShort,kInt,kFloat,kDouble,k2Short,k3Short,
        k2Int,k3Int,k2Float,k3Float,k2Double,k3Double,k4Double};
    MFnNumericData(const struct MObject&,MStatus*);
    Type numericType(MStatus* =nullptr);
    MStatus getData2Short(short&,short&);MStatus getData3Short(short&,short&,short&);
    MStatus getData2Int(int&,int&);MStatus getData3Int(int&,int&,int&);
    MStatus getData2Float(float&,float&);MStatus getData3Float(float&,float&,float&);
    MStatus getData2Double(double&,double&);MStatus getData3Double(double&,double&,double&);
    MStatus getData4Double(double&,double&,double&,double&);
};
int failAt=-1,failKind=0,reads=0;
unsigned arrayCount=2U;
unsigned nodeAttributeCount=1U;
bool uniqueAttributeNames=false;
bool attributeReadable=true;
bool multi=true;
bool usedAsColor=false;
MFnNumericData::Type numericKind=MFnNumericData::k3Double;
MFnData::Type typedKind=MFnData::kNumeric;
MStatus query(MStatus* status=nullptr){
    const bool failed=reads++==failAt;
    if(failed&&failKind==2)throw std::bad_alloc();
    if(failed&&failKind==3)throw std::runtime_error("sample fault");
    const MStatus result=failed?MS::kInvalidParameter:MS::kSuccess;
    if(status)*status=result;return result;
}
struct MString {
    std::string value;MString(const char* text=""):value(text){}
    const char* asChar()const{return value.c_str();}
    void toLowerCase(){}bool operator==(const char* text)const{return value==text;}
    MString& operator+=(const MString& other){value+=other.value;return *this;}
    MString operator+(const char* text)const{return MString((value+text).c_str());}
};
struct MObject {unsigned index=0U;MFn::Type type;explicit MObject(MFn::Type t=MFn::kData3Double):type(t){}
    bool hasFn(MFn::Type kind)const{return type==kind;}MFn::Type apiType()const{return type;}};
struct MPlug {
    unsigned index=0U;bool null=true;
    bool isNull()const{return null;}
    bool isElement(MStatus* s=nullptr)const{query(s);return false;}
    bool isChild(MStatus* s=nullptr)const{query(s);return false;}
    MPlug parent(MStatus* s=nullptr)const{query(s);return MPlug();}
    bool isArray(MStatus* s=nullptr)const{query(s);return multi;}
    unsigned numElements(MStatus* s=nullptr)const{query(s);return arrayCount;}
    MPlug elementByPhysicalIndex(unsigned i,MStatus* s=nullptr)const{query(s);MPlug p;p.index=i;return p;}
    MString partialName(bool,bool,bool,bool,bool,bool,MStatus* s=nullptr)const{query(s);return uniqueAttributeNames?MString(("sample"+std::to_string(index)).c_str()):MString("sample");}
    MObject asMObject(MStatus* s=nullptr)const{query(s);return MObject();}
    char asChar(MStatus* s=nullptr)const{query(s);return 7;}
};
struct MMatrix {double m[4][4]={{0}};const double* operator[](unsigned i)const{return m[i];}};
struct MPoint {double x=1,y=2,z=3,w=1;};
struct MVector {double x=1,y=2,z=3;};
struct MFnAttribute {MFnAttribute(const MObject&,MStatus* s=nullptr){query(s);}
    bool isUsedAsColor(MStatus* s=nullptr)const{query(s);return usedAsColor;}
    bool isReadable(MStatus* s=nullptr)const{query(s);return attributeReadable;}};
struct MFnDependencyNode {
    unsigned attributeCount(MStatus* s=nullptr)const{query(s);return nodeAttributeCount;}
    MObject attribute(unsigned i,MStatus* s=nullptr)const{query(s);MObject o(MFn::kTypedAttribute);o.index=i;return o;}
    MPlug findPlug(const MObject& o,bool,MStatus* s=nullptr)const{query(s);MPlug p;p.null=false;p.index=o.index;return p;}
    MPlug findPlug(const MString&,bool,MStatus* s=nullptr)const{query(s);return MPlug();}
    bool hasAttribute(const MString&,MStatus* s=nullptr)const{query(s);return false;}
};
struct MFnNumericAttribute {
    MFnNumericAttribute(const MObject& o,MStatus* s){query(s);if(o.type!=MFn::kNumericAttribute)*s=MS::kInvalidParameter;}
    MFnNumericData::Type unitType(MStatus* s=nullptr)const{query(s);return numericKind;}
};
struct MFnTypedAttribute {
    MFnTypedAttribute(const MObject&,MStatus* s){query(s);}
    MFnData::Type attrType(MStatus* s=nullptr)const{query(s);return typedKind;}
};
std::vector<MString> stringValues{MString("a"),MString("b")};
template<typename T>struct ArrayData {
    ArrayData(const MObject&,MStatus* s){query(s);}
    unsigned length(MStatus* s=nullptr)const{query(s);return arrayCount;}
    T operator[](unsigned)const{return T();}
};
struct MFnStringArrayData:ArrayData<MString>{using ArrayData::ArrayData;
    unsigned length(MStatus* s=nullptr)const{return ArrayData::length(s);}
    const MString& operator[](unsigned i)const{return stringValues.at(i);}};
#define ARRAY_DATA(name,T) struct name:ArrayData<T>{using ArrayData<T>::ArrayData;unsigned length(MStatus* s=nullptr)const{return ArrayData<T>::length(s);}};
ARRAY_DATA(MFnDoubleArrayData,double) ARRAY_DATA(MFnIntArrayData,int)
ARRAY_DATA(MFnPointArrayData,MPoint) ARRAY_DATA(MFnVectorArrayData,MVector)
#undef ARRAY_DATA
struct MFnMatrixData {MMatrix value;MFnMatrixData(const MObject&,MStatus* s){query(s);}
    const MMatrix& matrix(MStatus* s=nullptr)const{query(s);return value;}};
MFnNumericData::MFnNumericData(const MObject&,MStatus* s){query(s);}
MFnNumericData::Type MFnNumericData::numericType(MStatus* s){query(s);return numericKind;}
#define NUMERIC_READ2(name,T) MStatus MFnNumericData::name(T& a,T& b){auto s=query();a=1;b=2;return s;}
#define NUMERIC_READ3(name,T) MStatus MFnNumericData::name(T& a,T& b,T& c){auto s=query();a=1;b=2;c=3;return s;}
NUMERIC_READ2(getData2Short,short) NUMERIC_READ3(getData3Short,short)
NUMERIC_READ2(getData2Int,int) NUMERIC_READ3(getData3Int,int)
NUMERIC_READ2(getData2Float,float) NUMERIC_READ3(getData3Float,float)
NUMERIC_READ2(getData2Double,double) NUMERIC_READ3(getData3Double,double)
MStatus MFnNumericData::getData4Double(double& a,double& b,double& c,double& d){auto s=query();a=1;b=2;c=3;d=4;return s;}
#undef NUMERIC_READ2
#undef NUMERIC_READ3
#endif

// Function-set and archive behavior below are explicit seams. No Alembic
// archive library, scene evaluator or target Maya process is executed here.
MStatus readPlugValue(const MPlug&,bool&);MStatus readPlugValue(const MPlug&,short&);
MStatus readPlugValue(const MPlug&,int&);MStatus readPlugValue(const MPlug&,float&);
MStatus readPlugValue(const MPlug&,double&);MStatus readPlugValue(const MPlug&,MObject&);
MStatus readPlugValue(const MPlug&,MString&);
#ifndef DOMMY_SDK_CHECK
#define VALUE_READ(T,v) MStatus readPlugValue(const MPlug&,T& output){auto s=query();output=v;return s;}
VALUE_READ(bool,true) VALUE_READ(short,7) VALUE_READ(int,7) VALUE_READ(float,7) VALUE_READ(double,7)
VALUE_READ(MObject,MObject()) VALUE_READ(MString,stringValues.front())
#undef VALUE_READ
#endif
#ifndef DOMMY_SDK_CHECK
struct MGlobal {static void displayWarning(const MString&){};};
#endif
namespace Alembic {
unsigned nativePropertyConstructions=0U;
bool failNativeProperty=false;
inline void nativePropertyConstruction(){++nativePropertyConstructions;if(failNativeProperty)throw std::runtime_error("native constructor fault");}
namespace Util {using bool_t=char;using int8_t=signed char;using int16_t=short;using int32_t=int;using uint32_t=unsigned;
enum PlainOldDataType {kBooleanPOD,kInt8POD,kInt16POD,kInt32POD,kFloat32POD,kFloat64POD,kStringPOD};
struct Dimensions {std::size_t count;explicit Dimensions(std::size_t n):count(n){}};}
namespace AbcCoreAbstract {
struct DataType {Util::PlainOldDataType pod=Util::kFloat64POD;unsigned extent=3;std::size_t byteOverride=0;
    DataType()=default;DataType(Util::PlainOldDataType p,unsigned n):pod(p),extent(n){}
    unsigned getExtent()const{return extent;}Util::PlainOldDataType getPod()const{return pod;}
    std::size_t getNumBytes()const{
        const std::size_t widths[]={1,1,2,4,4,8,sizeof(std::string)};return byteOverride?byteOverride:widths[pod]*extent;}};
struct ArraySample {const void* data;DataType type;std::size_t count;
    ArraySample(const void* p,DataType t,Util::Dimensions n):data(p),type(t),count(n.count){}};
struct MetaData {void set(const char*,const char*){}};
}
namespace Abc {
enum {kWrapExisting};
struct SampleState {unsigned sets=0;std::size_t count=0;std::string text;
    AbcCoreAbstract::DataType type;std::vector<unsigned char> bytes;std::vector<std::string> strings;
    void consume(const void* data,AbcCoreAbstract::DataType schema,std::size_t size){
        if(schema.getPod()==Util::kStringPOD){
            const auto* values=static_cast<const std::string*>(data);
            strings.clear();for(std::size_t i=0;i<size*schema.getExtent();++i)strings.push_back(values[i]);
        }else{
            bytes.resize(schema.getNumBytes()*size);if(!bytes.empty())std::memcpy(bytes.data(),data,bytes.size());
        }
        ++sets;count=size;
    }};
struct OCompoundProperty {bool valid()const{return true;}};
struct OObject {OCompoundProperty getProperties()const{return {};}};
struct OScalarProperty {
    std::shared_ptr<SampleState> state=std::make_shared<SampleState>();
    OScalarProperty()=default;
    AbcCoreAbstract::DataType getDataType()const{return state->type;}
    template<typename T>void set(const T* data){state->consume(data,state->type,1U);}
    void set(int value){const signed char data=static_cast<signed char>(value);set(&data);}
    void set(const std::string& value){++state->sets;state->text=value;}
    std::shared_ptr<SampleState> getPtr()const{return state;}
};
template<Util::PlainOldDataType Pod,unsigned Extent>struct TypedScalar:OScalarProperty {
    TypedScalar(){state->type={Pod,Extent};}
    template<typename... Args>TypedScalar(Args&&...){nativePropertyConstruction();state->type={Pod,Extent};}
};
struct OStringProperty:TypedScalar<Util::kStringPOD,1> {
    using TypedScalar::TypedScalar;
    OStringProperty(std::shared_ptr<SampleState> value,decltype(kWrapExisting)){state=value;}
};
struct OArrayProperty {
    std::shared_ptr<SampleState> state=std::make_shared<SampleState>();
    AbcCoreAbstract::DataType type;
    OArrayProperty()=default;
    OArrayProperty(const OCompoundProperty&,const std::string&,AbcCoreAbstract::DataType t,unsigned):type(t){nativePropertyConstruction();}
    AbcCoreAbstract::DataType getDataType()const{return type;}
    void set(const AbcCoreAbstract::ArraySample& value){
        assert(value.type.getPod()==type.getPod()&&value.type.getExtent()==type.getExtent());
        state->consume(value.data,type,value.count);}
};
template<Util::PlainOldDataType Pod,unsigned Extent>struct TypedArray:OArrayProperty {
    TypedArray(){type={Pod,Extent};}
    template<typename... Args>TypedArray(Args&&...){nativePropertyConstruction();type={Pod,Extent};}
};
#define SCALAR_ALIAS(name,pod,n) using name=TypedScalar<Util::pod,n>;
SCALAR_ALIAS(OBoolProperty,kBooleanPOD,1) SCALAR_ALIAS(OCharProperty,kInt8POD,1) SCALAR_ALIAS(OInt16Property,kInt16POD,1)
SCALAR_ALIAS(OInt32Property,kInt32POD,1) SCALAR_ALIAS(OFloatProperty,kFloat32POD,1) SCALAR_ALIAS(ODoubleProperty,kFloat64POD,1)
SCALAR_ALIAS(OV2sProperty,kInt16POD,2) SCALAR_ALIAS(OV3sProperty,kInt16POD,3) SCALAR_ALIAS(OV2iProperty,kInt32POD,2) SCALAR_ALIAS(OV3iProperty,kInt32POD,3)
SCALAR_ALIAS(OV2fProperty,kFloat32POD,2) SCALAR_ALIAS(OV3fProperty,kFloat32POD,3) SCALAR_ALIAS(OV2dProperty,kFloat64POD,2) SCALAR_ALIAS(OV3dProperty,kFloat64POD,3)
SCALAR_ALIAS(OM44dProperty,kFloat64POD,16)
#undef SCALAR_ALIAS
#define ARRAY_ALIAS(name,pod,n) using name=TypedArray<Util::pod,n>;
ARRAY_ALIAS(ODoubleArrayProperty,kFloat64POD,1) ARRAY_ALIAS(OInt32ArrayProperty,kInt32POD,1) ARRAY_ALIAS(OStringArrayProperty,kStringPOD,1)
ARRAY_ALIAS(ON2dArrayProperty,kFloat64POD,2) ARRAY_ALIAS(ON3dArrayProperty,kFloat64POD,3) ARRAY_ALIAS(OP2dArrayProperty,kFloat64POD,2)
ARRAY_ALIAS(OP3dArrayProperty,kFloat64POD,3) ARRAY_ALIAS(OV2dArrayProperty,kFloat64POD,2) ARRAY_ALIAS(OV3dArrayProperty,kFloat64POD,3)
#undef ARRAY_ALIAS
}
namespace AbcGeom {
enum GeometryScope {kConstantScope,kFacevaryingScope,kUniformScope,kUnknownScope,kVaryingScope,kVertexScope};
enum {kVisibilityHidden=0,kVisibilityDeferred=-1};
inline Abc::OCharProperty CreateVisibilityProperty(const Abc::OObject&,unsigned){return {};}
template<Util::PlainOldDataType Pod,unsigned Extent>struct GeomParam {
    template<typename... Args>GeomParam(Args&&...){nativePropertyConstruction();}
    Abc::OArrayProperty getValueProperty()const{return Abc::TypedArray<Pod,Extent>();}};
#define GEOM_ALIAS(name,pod,n) using name=GeomParam<Util::pod,n>;
GEOM_ALIAS(OBoolGeomParam,kBooleanPOD,1) GEOM_ALIAS(OC3fGeomParam,kFloat32POD,3) GEOM_ALIAS(OCharGeomParam,kInt8POD,1)
GEOM_ALIAS(ODoubleGeomParam,kFloat64POD,1) GEOM_ALIAS(OFloatGeomParam,kFloat32POD,1) GEOM_ALIAS(OInt16GeomParam,kInt16POD,1)
GEOM_ALIAS(OInt32GeomParam,kInt32POD,1) GEOM_ALIAS(OM44dGeomParam,kFloat64POD,16) GEOM_ALIAS(ON2dGeomParam,kFloat64POD,2) GEOM_ALIAS(ON3dGeomParam,kFloat64POD,3)
GEOM_ALIAS(OP2dGeomParam,kFloat64POD,2) GEOM_ALIAS(OP3dGeomParam,kFloat64POD,3) GEOM_ALIAS(OStringGeomParam,kStringPOD,1)
GEOM_ALIAS(OV2dGeomParam,kFloat64POD,2) GEOM_ALIAS(OV3dGeomParam,kFloat64POD,3) GEOM_ALIAS(OV2fGeomParam,kFloat32POD,2) GEOM_ALIAS(OV3fGeomParam,kFloat32POD,3)
GEOM_ALIAS(OV2iGeomParam,kInt32POD,2) GEOM_ALIAS(OV3iGeomParam,kInt32POD,3) GEOM_ALIAS(OV2sGeomParam,kInt16POD,2) GEOM_ALIAS(OV3sGeomParam,kInt16POD,3)
#undef GEOM_ALIAS
}
}
namespace Abc=Alembic::Abc;
namespace AbcA=Alembic::AbcCoreAbstract;
namespace AbcGeom=Alembic::AbcGeom;
struct PlugAndObjScalar {MPlug plug;MObject obj;Abc::OScalarProperty prop;};
struct PlugAndObjArray {MPlug plug;MObject obj;Abc::OArrayProperty prop;};
struct JobArgs {std::shared_ptr<dommy::attribute::ExportAttributeBudget> exportAttributeBudget=std::make_shared<dommy::attribute::ExportAttributeBudget>();std::vector<std::string> prefixFilters,userPrefixFilters;std::set<std::string> attribs;};
namespace ddata_export_util {int getSampledType(const MPlug&);int getVisibilityType(const MPlug&);}
namespace ddata_export_util {int getSampledType(const MPlug&,dommy::generation::WorkBudget&);}
#ifndef DOMMY_SDK_CHECK
int getSampledKind=1;
int ddata_export_util::getSampledType(const MPlug&){return getSampledKind;}
int ddata_export_util::getSampledType(const MPlug& plug,dommy::generation::WorkBudget& work){
    if(!work.consume(1U))throw std::length_error("sample work");return getSampledType(plug);}
int ddata_export_util::getVisibilityType(const MPlug&){return 0;}
#endif
struct MayaAttributeWriter {
    std::vector<PlugAndObjArray> mPlugObjArrayVec;
    std::vector<PlugAndObjScalar> mPlugObjScalarVec;
    PlugAndObjScalar mAnimVisibility;
    MayaAttributeWriter()=default;
    MayaAttributeWriter(Abc::OCompoundProperty&,Abc::OCompoundProperty&,Abc::OObject&,
        const MFnDependencyNode&,unsigned,const JobArgs&);
    ~MayaAttributeWriter();
    static bool matchFilterOrAttribs(const MPlug&,const JobArgs&,bool&);
    static bool hasAnyAttr(const MFnDependencyNode&,const JobArgs&);
    bool isAnimated();
    void write();
};
