#pragma once
#include <maya/MStatus.h>
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DExportAttributeState.h"
#include "common/file/DAtomicFile.h"
#include "common/utility/DAbcRootAdmission.h"
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "alembic_box_double.h"
#ifdef DOMMY_SDK_CHECK
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MDagPath.h>
#include <maya/MPlug.h>
#include <maya/MMatrix.h>
#include <maya/MIntArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MTypeId.h>
#include <maya/MStringArray.h>
#else
int failAt=-1,kind=0;
std::vector<std::string> calls;
std::map<std::string,double> numbers;
std::map<std::string,std::string> strings;
bool missingPlug=false,shortArrays=false;
unsigned stringLength=0U;
MStatus query(const std::string& name,MStatus* output=nullptr){calls.push_back(name);const bool failed=int(calls.size())-1==failAt;
    if(failed&&kind==2)throw std::bad_alloc();if(failed&&kind==3)throw std::runtime_error("geometry fault");
    const MStatus status=failed?MS::kInvalidParameter:MS::kSuccess;if(output)*output=status;return status;}
void backend(const char* name){if(!query(name))throw std::runtime_error("archive/payload fault");}
struct MStringArray;
struct MString {std::string text;bool allowOverride;MString(const char* s="",bool allow=true):text(s),allowOverride(allow){}
    const char* asChar()const{return text.c_str();}
    unsigned length()const{return allowOverride&&stringLength?stringLength:text.size();}
    MString& operator+=(const MString& other){text+=other.text;return *this;}
    MStatus split(char separator,MStringArray&)const;};
struct MStringArray {std::vector<MString> values;unsigned length()const{return values.size();}
    const MString& operator[](unsigned index)const{return values.at(index);}};
MStatus MString::split(char separator,MStringArray& output)const{
    const auto status=query("namespace.split");if(!status)return status;std::size_t start=0;
    for(std::size_t i=0;i<=text.size();++i) {
        if(i!=text.size()&&text[i]!=separator)continue;
        if(i!=start)output.values.emplace_back(text.substr(start,i-start).c_str(),allowOverride);
        start=i+1U;
    }
    return status;
}
struct MObject {std::string key;MObject(const char* s="node"):key(s){}};
using MTypeId=int;
struct MMatrix {double data[4][4]{};MMatrix(){for(unsigned i=0;i<4;++i)data[i][i]=1;}
    const double* operator[](unsigned i)const{return data[i];}};
template<typename T>struct MayaArray {std::vector<T> data;unsigned length()const{return data.size();}
    T operator[](unsigned i)const{return data.at(i);}};
using MIntArray=MayaArray<int>;using MDoubleArray=MayaArray<double>;
struct MDagPath {MObject node(MStatus* s=nullptr)const{query("path.node",s);return {};}};
struct MPlug {std::string key;MPlug()=default;MPlug(const MObject&,const MObject& a):key(a.key){}
    explicit MPlug(const MString& s):key(s.asChar()){}bool isNull()const{return key.empty();}};
struct MFnDependencyNode {MFnDependencyNode(MObject,MStatus* s=nullptr){query("node.bind",s);}
    MString name(MStatus* s=nullptr)const{query("node.name",s);
        return MString((strings.count("node.name")?strings.at("node.name"):std::string("shape")).c_str(),false);}
    MPlug findPlug(const MObject& a,bool,MStatus* s=nullptr)const{query("node.plug",s);return missingPlug?MPlug():MPlug(MString(a.key.c_str()));}
    MPlug findPlug(const MString& a,bool,MStatus* s=nullptr)const{query("node.plug",s);return missingPlug?MPlug():MPlug(a);}};
struct MFnDagNode:MFnDependencyNode {MFnDagNode(const MDagPath&,MStatus* s=nullptr):MFnDependencyNode({},s){}};
#endif
template<typename T>MStatus readPlugValue(const MPlug&,T&);

enum DP_ATTR_DATA_TYPE {DP_ATTR_BOOL,DP_ATTR_INT,DP_ATTR_UINT32,DP_ATTR_INT64,DP_ATTR_FLOAT,DP_ATTR_DOUBLE,
    DP_ATTR_VEC2I,DP_ATTR_VEC2F,DP_ATTR_VEC2D,DP_ATTR_VEC3I,DP_ATTR_VEC3F,DP_ATTR_VEC3D,DP_ATTR_STRING};
struct DPAttributeData {using ConstPtr=std::shared_ptr<const DPAttributeData>;std::size_t size=1U;
    DP_ATTR_DATA_TYPE type=DP_ATTR_INT;
    std::size_t attrSize()const{return size;}DP_ATTR_DATA_TYPE getAttrType()const{return type;}};
using DPoint=std::array<float,3>;using DPointList=std::vector<DPoint>;
using DVec3Data=DPoint;using DScalarDataList=std::vector<float>;
struct GeometryPart {
    DPointList positions={DPoint{{1,2,3}},DPoint{{4,5,6}},DPoint{{7,8,9}}};
    std::size_t reportedPoints=0U,reportedConnections=3U;bool valid=true,identity=true,open=true;int degree=1,faces=1;
    std::size_t numberOfPoints()const{return reportedPoints?reportedPoints:positions.size();}
    std::size_t numberOfConnections()const{return reportedConnections;}
    int numFace()const{return faces;}bool hasValidPolygonTopology()const{return valid;}
    bool hasValidCurveTopology()const{return valid;}unsigned getDegree()const{return degree;}
    std::size_t numberOfKnots()const{return 3U;}bool isCurveOpen()const{return open;}
    bool isIdentity()const{return identity;}const DPointList* getPositionPtrConst()const{return &positions;}
    DPointList getPointsInWorld()const{return positions;}MMatrix getTransformMatrix()const{return {};}
    MIntArray getFaceList()const;MIntArray getConnectList()const;MDoubleArray getKnots()const;
};
using PartPtr=std::shared_ptr<GeometryPart>;
struct DPData {
    static const MTypeId id;DPointList positions={DPoint{{1,2,3}},DPoint{{4,5,6}}};bool valid=true;
    std::size_t reportedPoints=0U;std::size_t numberOfPoints()const{return reportedPoints?reportedPoints:positions.size();}
    bool hasValidPointTopology()const{return valid;}const DPointList* getPositionPtrConst()const{return &positions;}
    bool isExistAttr(const std::string&)const;
    std::map<std::string,DPAttributeData::ConstPtr> attributes;
    DPAttributeData::ConstPtr getAttr(const std::string& name)const {
        const auto found=attributes.find(name);return found==attributes.end()?DPAttributeData::ConstPtr():found->second;}
};
struct GeometryOwner {std::vector<PartPtr> parts;bool uniform=false,vertex=false;
    DP_ATTR_DATA_TYPE pointType=DP_ATTR_FLOAT;
    DPAttributeData::ConstPtr attribute=std::make_shared<DPAttributeData>();
    bool isExistAttr(const std::string&)const{return uniform;}
    bool isExistPointAttr(const std::string&)const{return vertex;}
    DPAttributeData::ConstPtr getAttr(const std::string&)const{return attribute;}
    DP_ATTR_DATA_TYPE getPointAttributeType(const std::string&)const{return pointType;}
};
struct DCurveData:GeometryOwner {static const MTypeId id;
    std::size_t numberOfCurves()const{return parts.size();}
    const std::vector<PartPtr>* getCurveListConst()const{return &parts;}
    auto beginCurves()const->decltype(parts.begin()){return parts.begin();}auto endCurves()const->decltype(parts.end()){return parts.end();}};
struct DMeshData:GeometryOwner {static const MTypeId id;std::size_t numberOfMesh()const{return parts.size();}
    const std::vector<PartPtr>* getMeshListConst()const{return &parts;}
    void getTotalVertex(std::size_t& out)const{out=0;for(const auto& p:parts)out+=p->numberOfPoints();}};
struct DPointDisplay {static MObject inDP,aIsStaticObject,aVelocityAttributeName,aPointWidthType,aConstantWidth,aWidthAttributeName,aExportPointAttribute;};
struct DCurveDisplay {static MObject inDC,aIsStaticObject,aCurveWidthType,aConstantWidth,aWidthAttributeName,aIsMultiEndKnot,aExportCurveAttribute,aExportPointAttribute;};
struct DMeshDisplay {static MObject inDM,aIsStaticObject,aExportMeshAttribute,aExportPointAttribute;};
namespace dommy {namespace maya {
template<typename T>MStatus getPluginDataFromPlug(const MPlug&,MTypeId,MObject&,const T*&);
}}
namespace dommy {namespace attribute {
struct ValidationBudget {};
inline bool validateAttribute(const DPAttributeData::ConstPtr& value,std::size_t count,ValidationBudget&) {
    return value&&value->attrSize()==count;
}
}}
void getMVectorArrayFromAttribute(const DPData&,const std::string&,std::vector<DVec3Data>&);
void getDScalarArrayFromAttribute(const DPData&,const std::string&,std::vector<float>&);
void getDCAttributeMDoubleArrayFromScalar(const DCurveData&,const std::string&,std::vector<float>&);
void payloadAttributeStage();
template<typename T,typename U>void DGetAbcDCPScalarAttribute(const DCurveData*,DP_ATTR_DATA_TYPE,const std::string&,std::vector<U>&);

// Explicit archive transport, attribute payload and geometry kernel seams.
namespace Alembic {namespace Util {using uint32_t=unsigned;using uint64_t=std::uint64_t;using int32_t=std::int32_t;using uint8_t=unsigned char;enum{kFloat64POD};}
namespace AbcCoreAbstract {struct DataType {DataType(int,int){}};}
namespace Abc {struct OObject{};struct OCompoundProperty{};
struct OArrayProperty {};
struct V3f {float values[3];V3f(float x,float y,float z):values{x,y,z}{}float operator[](unsigned i)const{return values[i];}};
struct M44d {double values[4][4]{};void makeIdentity(){for(unsigned i=0;i<4;++i)values[i][i]=1;}
    double* operator[](unsigned i){return values[i];}};
// Match Alembic's non-owning array handoff. Only Schema::set consumes data.
template<typename T>struct ArraySample {
    const T* data=nullptr;std::size_t count=0U;bool supplied=false;
    ArraySample()=default;
    ArraySample(const std::vector<T>& value):data(value.data()),count(value.size()),supplied(true){}
    ArraySample(const T* p,std::size_t n):data(p),count(n),supplied(true){}
    std::vector<T> snapshot()const {assert(data||count==0U);return count?std::vector<T>(data,data+count):std::vector<T>();}
};
using V3fArraySample=ArraySample<V3f>;using FloatArraySample=ArraySample<float>;using UInt64ArraySample=ArraySample<Util::uint64_t>;
using Int32ArraySample=ArraySample<Util::int32_t>;using UcharArraySample=ArraySample<Util::uint8_t>;
template<typename T>struct State {std::vector<T> frames;int widthScope=-1;};
template<typename T>struct Schema {std::shared_ptr<State<T>> state=std::make_shared<State<T>>();
    void set(const T& value){
#ifndef DOMMY_SDK_CHECK
        backend("archive.set");
#endif
        state->frames.push_back(value.snapshot(state->widthScope));}
    OCompoundProperty getArbGeomParams()const{return {};}OCompoundProperty getUserProperties()const{return {};}};
struct OScalarProperty {std::shared_ptr<State<std::array<double,6>>> state=std::make_shared<State<std::array<double,6>>>();
    OScalarProperty()=default;template<typename... T>OScalarProperty(T&&...){
#ifndef DOMMY_SDK_CHECK
        backend("archive.property");
#endif
    }
    void set(const double* values){
#ifndef DOMMY_SDK_CHECK
        backend("archive.scalar");
#endif
        std::array<double,6> a;std::copy(values,values+6,a.begin());state->frames.push_back(a);}
    void reset(){}
};
}
namespace AbcGeom {
using Abc::V3f;using Abc::V3fArraySample;
enum GeometryScope{kConstantScope=0,kUniformScope=1,kVaryingScope=2,kVertexScope=3,kFacevaryingScope=4,kUnknownScope=127};
enum CurveType{kLinear,kCubic,kVariableOrder};enum {kBsplineBasis,kNonPeriodic,kPeriodic,kMatrixOperation,kMatrixHint};
struct OFloatGeomParam {struct Sample {Abc::FloatArraySample values;GeometryScope scope=kUnknownScope;
    Sample()=default;Sample(Abc::FloatArraySample a,GeometryScope s):values(a),scope(s){}};};
struct ShapeSample {
    std::vector<V3f> positions,velocities;std::vector<float> widths,knots;std::vector<Util::uint64_t> ids;
    std::vector<Util::int32_t> vertices,connections,faces;std::vector<Util::uint8_t> orders;CurveType type=kLinear;
    int basis=kBsplineBasis,wrap=kNonPeriodic;GeometryScope widthScope=kUnknownScope;
    V3fArraySample positionView,velocityView;Abc::UInt64ArraySample idView;
    Abc::Int32ArraySample vertexView,connectionView,faceView;Abc::UcharArraySample orderView;
    Abc::FloatArraySample knotView;OFloatGeomParam::Sample widthView;
    ShapeSample()=default;
    ShapeSample(V3fArraySample p,Abc::Int32ArraySample c,Abc::Int32ArraySample f):positionView(p),connectionView(c),faceView(f){}
    void setPositions(V3fArraySample a){positionView=a;}void setVelocities(V3fArraySample a){velocityView=a;}
    void setWidths(OFloatGeomParam::Sample a){widthView=a;}void setIds(Abc::UInt64ArraySample a){idView=a;}
    void setBasis(int value){basis=value;}void setWrap(int value){wrap=value;}
    void setType(CurveType t){type=t;}CurveType getType()const{return type;}
    void setCurvesNumVertices(Abc::Int32ArraySample a){vertexView=a;}void setOrders(Abc::UcharArraySample a){orderView=a;}
    void setKnots(Abc::FloatArraySample a){knotView=a;}
    ShapeSample snapshot(int& schemaWidthScope)const {
        ShapeSample result;
        result.positions=positionView.snapshot();result.velocities=velocityView.snapshot();result.ids=idView.snapshot();
        result.vertices=vertexView.snapshot();result.connections=connectionView.snapshot();result.faces=faceView.snapshot();
        result.knots=knotView.snapshot();result.orders=orderView.snapshot();result.widths=widthView.values.snapshot();
        result.type=type;result.basis=basis;result.wrap=wrap;
        // Native width metadata is fixed when the property is created.
        if(widthView.values.supplied&&schemaWidthScope<0)schemaWidthScope=widthView.scope;
        result.widthScope=schemaWidthScope<0?kUnknownScope:static_cast<GeometryScope>(schemaWidthScope);
        return result;
    }
};
struct XformOp {XformOp(int,int){}void setMatrix(const Abc::M44d&){};};
struct XformSample {void addOp(const XformOp&){};XformSample snapshot(int&)const{return *this;}};
struct OPointsSchema:Abc::Schema<ShapeSample> {using Sample=ShapeSample;};
struct OCurvesSchema:Abc::Schema<ShapeSample> {using Sample=ShapeSample;};
struct OPolyMeshSchema:Abc::Schema<ShapeSample> {using Sample=ShapeSample;};
using OXformSchema=Abc::Schema<XformSample>;
void createArchiveObject(const std::string&);
struct OPoints:Abc::OObject {OPoints(const Abc::OObject&,const char* s,unsigned){createArchiveObject(s);}OPointsSchema getSchema()const{return {};}};
struct OCurves:Abc::OObject {OCurves(const Abc::OObject&,const char* s,unsigned){createArchiveObject(s);}OCurvesSchema getSchema()const{return {};}};
struct OPolyMesh:Abc::OObject {OPolyMesh(const Abc::OObject&,const char* s,unsigned){createArchiveObject(s);}OPolyMeshSchema getSchema()const{return {};}};
struct OXform:Abc::OObject {OXform()=default;OXform(const Abc::OObject&,const std::string& s,unsigned){createArchiveObject(s);}
    Abc::OCompoundProperty getProperties()const{return {};}OXformSchema getSchema()const{return {};}};
}}
#include "builtin_sample_state.inc"
struct JobArgs {unsigned stripNamespace=0U;std::shared_ptr<dommy::attribute::ExportAttributeBudget> exportAttributeBudget=
    std::make_shared<dommy::attribute::ExportAttributeBudget>();};
using DAbcPointAttrList=std::map<std::string,Alembic::Abc::OArrayProperty>;
template<typename Data>void fakeCollectionAttributeStage(Data const&,const std::list<std::string>&,
    dommy::attribute::ExportAttributeState&,DAbcPointAttrList&,bool,bool);
struct MayaAttributeWriter {template<typename... T>MayaAttributeWriter(T&&...){
#ifndef DOMMY_SDK_CHECK
    backend("attributes.new");
#endif
}static bool hasAnyAttr(const MFnDependencyNode&,const JobArgs&){return false;}};
using MayaAttributeWriterPtr=std::shared_ptr<MayaAttributeWriter>;
struct DPointWriter {bool mIsAnimated;MDagPath mRootDagPath;Alembic::AbcGeom::OPointsSchema mSchema;std::size_t mPointCount;unsigned mTimeIndex;
    MBoundingBox mSampleBounds;
    ddata_export_util::BuiltinSampleState mBuiltinState;
    DAbcPointAttrList pointAttributeList;dommy::attribute::ExportAttributeState mAttributeState;
    DPointWriter(MDagPath&,Alembic::Abc::OObject&,unsigned,const JobArgs&);void write();bool isAnimated()const;size_t getNumPoints();
    void getDPointPointAttribute(const DPData&,const std::list<std::string>&,bool);};
struct DCurveWriter {bool mIsAnimated;MDagPath mRootDagPath;Alembic::AbcGeom::OCurvesSchema mSchema;unsigned mCVCount;std::size_t mCurveCount;unsigned mTimeIndex;
    MBoundingBox mSampleBounds;
    ddata_export_util::BuiltinSampleState mBuiltinState;
    DAbcPointAttrList pointAttributeList;dommy::attribute::ExportAttributeState mAttributeState;
    DCurveWriter(MDagPath&,Alembic::Abc::OObject&,unsigned,const JobArgs&);void write();bool isAnimated()const;size_t getNumCVs();size_t getNumCurves();
    void getDCurvePrimAttribute(const DCurveData&,const std::list<std::string>&,bool);void getDCurvePointAttribute(const DCurveData&,const std::list<std::string>&,bool);};
struct DMeshWriter {bool mIsAnimated;MDagPath mRootDagPath;Alembic::AbcGeom::OPolyMeshSchema mSchema;std::size_t mPtCount,mMeshCount;unsigned mTimeIndex;
    MBoundingBox mSampleBounds;
    DAbcPointAttrList pointAttributeList;dommy::attribute::ExportAttributeState mAttributeState;
    std::vector<Alembic::AbcGeom::OXformSchema> mTransSchemaList;
    DMeshWriter(MDagPath&,Alembic::Abc::OObject&,unsigned,const JobArgs&);void write();bool isAnimated()const;size_t getNumPoints();size_t getNumMeshs();
    void getDMeshPrimAttribute(const DMeshData&,const std::list<std::string>&,bool);void getDMeshPointAttribute(const DMeshData&,const std::list<std::string>&,bool);};
struct MayaLocatorWriter {bool mIsAnimated;MDagPath mDagPath;Alembic::Abc::OScalarProperty mSp;Alembic::AbcGeom::OXform mXform;MayaAttributeWriterPtr mAttrs;
    MayaLocatorWriter(MDagPath&,Alembic::Abc::OObject&,unsigned,const JobArgs&);~MayaLocatorWriter();void write();bool isAnimated()const;};
namespace ddata_export_util {int getSampledType(const MPlug&);MString stripNamespaces(const MString&,unsigned);}
namespace ddata_export_util {MString exportObjectName(const MFnDependencyNode&,unsigned);}

#ifndef DOMMY_SDK_CHECK
DPData points;DCurveData curves;DMeshData meshes;
std::vector<std::string> archiveNames;
bool velocityPresent=false,widthPresent=false;int widthSize=-1;float widthValue=1.0f;double knotValue=0.5;
const MTypeId DPData::id=1,DCurveData::id=2,DMeshData::id=3;
#define ATTR(owner,name) MObject owner::name(#name)
ATTR(DPointDisplay,inDP);ATTR(DPointDisplay,aIsStaticObject);ATTR(DPointDisplay,aVelocityAttributeName);ATTR(DPointDisplay,aPointWidthType);ATTR(DPointDisplay,aConstantWidth);ATTR(DPointDisplay,aWidthAttributeName);ATTR(DPointDisplay,aExportPointAttribute);
ATTR(DCurveDisplay,inDC);ATTR(DCurveDisplay,aIsStaticObject);ATTR(DCurveDisplay,aCurveWidthType);ATTR(DCurveDisplay,aConstantWidth);ATTR(DCurveDisplay,aWidthAttributeName);ATTR(DCurveDisplay,aIsMultiEndKnot);ATTR(DCurveDisplay,aExportCurveAttribute);ATTR(DCurveDisplay,aExportPointAttribute);
ATTR(DMeshDisplay,inDM);ATTR(DMeshDisplay,aIsStaticObject);ATTR(DMeshDisplay,aExportMeshAttribute);ATTR(DMeshDisplay,aExportPointAttribute);
#undef ATTR
template<typename T>MStatus readPlugValue(const MPlug& p,T& out){const auto s=query("plug."+p.key);out=static_cast<T>(numbers.count(p.key)?numbers.at(p.key):0);return s;}
template<>MStatus readPlugValue<MString>(const MPlug& p,MString& out){const auto s=query("plug."+p.key);out=MString(strings[p.key].c_str());return s;}
template<typename T>const T* payload();template<>const DPData* payload<DPData>(){return &points;}template<>const DCurveData* payload<DCurveData>(){return &curves;}template<>const DMeshData* payload<DMeshData>(){return &meshes;}
template<typename T>MStatus dommy::maya::getPluginDataFromPlug(const MPlug&,MTypeId,MObject&,const T*& out){const auto s=query("data.read");out=payload<T>();return s;}
MIntArray GeometryPart::getFaceList()const{const auto s=query("array.faces");MIntArray a;if(s&&!shortArrays)a.data={3};return a;}
MIntArray GeometryPart::getConnectList()const{const auto s=query("array.connections");MIntArray a;if(s&&!shortArrays)a.data={0,1,2};return a;}
MDoubleArray GeometryPart::getKnots()const{const auto s=query("array.knots");MDoubleArray a;if(s&&!shortArrays)a.data={0,knotValue,1};return a;}
bool DPData::isExistAttr(const std::string& name)const{return name=="velocity"?velocityPresent:name=="width"?widthPresent:attributes.count(name)!=0;}
void getMVectorArrayFromAttribute(const DPData& p,const std::string&,std::vector<DVec3Data>& out){backend("payload.velocity");out=p.positions;if(widthSize>=0)out.resize(widthSize);}
void getDScalarArrayFromAttribute(const DPData& p,const std::string&,std::vector<float>& out){backend("payload.width");out.assign(widthSize>=0?widthSize:p.numberOfPoints(),widthValue);}
void getDCAttributeMDoubleArrayFromScalar(const DCurveData& c,const std::string&,std::vector<float>& out){backend("payload.width");out.assign(widthSize>=0?widthSize:c.numberOfCurves(),widthValue);}
template<typename T,typename U>void DGetAbcDCPScalarAttribute(const DCurveData* c,DP_ATTR_DATA_TYPE,const std::string&,std::vector<U>& out){backend("payload.width");std::size_t n=0;for(const auto& p:c->parts)n+=p->numberOfPoints();out.assign(widthSize>=0?widthSize:n,widthValue);}
void Alembic::AbcGeom::createArchiveObject(const std::string& name){backend("archive.object");if(std::find(archiveNames.begin(),archiveNames.end(),name)!=archiveNames.end())throw std::runtime_error("duplicate object");archiveNames.push_back(name);}
int ddata_export_util::getSampledType(const MPlug&){backend("sampling");return 1;}
void DPointWriter::getDPointPointAttribute(const DPData& data,const std::list<std::string>& names,bool sampled){backend("attributes.point");
    for(const auto& name:names)if(data.isExistAttr(name)&&data.getAttr(name)) {
        if(data.getAttr(name)->getAttrType()>DP_ATTR_VEC3D)continue;
        mAttributeState.requireCurrent(name,dommy::attribute::ExportAttributeDomain::Point,data.getAttr(name)->getAttrType(),sampled);
        pointAttributeList.emplace(name,Alembic::Abc::OArrayProperty());mAttributeState.markWritten(name);
    }}
void payloadAttributeStage(){backend("attributes.primitive");}
template<typename Data>void fakeCollectionAttributeStage(Data const& data,const std::list<std::string>& names,
    dommy::attribute::ExportAttributeState& state,DAbcPointAttrList& properties,bool point,bool sampled) {
    for(const auto& name:names)if(point?data.isExistPointAttr(name):data.isExistAttr(name)) {
        const auto type=point?data.getPointAttributeType(name):data.getAttr(name)->getAttrType();
        if(type>DP_ATTR_VEC3D)continue;
        state.requireCurrent(name,point?dommy::attribute::ExportAttributeDomain::Point:dommy::attribute::ExportAttributeDomain::Primitive,type,sampled);
        properties.emplace(name,Alembic::Abc::OArrayProperty());state.markWritten(name);
    }
}
void DCurveWriter::getDCurvePointAttribute(const DCurveData& data,const std::list<std::string>& names,bool sampled){backend("attributes.point");
    fakeCollectionAttributeStage(data,names,mAttributeState,pointAttributeList,true,sampled);}
void DMeshWriter::getDMeshPointAttribute(const DMeshData& data,const std::list<std::string>& names,bool sampled){backend("attributes.point");
    fakeCollectionAttributeStage(data,names,mAttributeState,pointAttributeList,true,sampled);}
#endif
