// Exact production mesh/attribute functions and numeric staging expressions.
// Maya mesh results and renderer ownership are seams, not native runtime proof.
#include <maya/MStatus.h>
#include <ImathVec.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DMeshUVTransfer.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "arnold_parameter_api.h"
#ifndef DOMMY_SDK_CHECK
#include "arnold_parameter_doubles.h"
#endif
#include "integrations/arnold/DArnoldUserParameters.h"
#ifdef DOMMY_SDK_CHECK
#include <maya/MDataHandle.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MFloatArray.h>
#include <maya/MMatrix.h>
#include <maya/MVector.h>
#include <maya/MStringArray.h>
#else
int failAt=-1,exceptionKind=0,errors=0,createdNodes=0,liveNodes=0,liveArrays=0,rendererWrites=0;
int allocationAt=-1,throwAllocationAt=-1,allocationCalls=0,submittedInt=0;
std::vector<std::string> queries;
MStatus query(const char* name,MStatus* output=nullptr){
    queries.push_back(name);const bool fail=int(queries.size())-1==failAt;
    if(fail&&exceptionKind==1)throw std::bad_alloc();
    if(fail&&exceptionKind==2)throw std::runtime_error("mesh query failure");
    const MStatus status=fail?MS::kInvalidParameter:MS::kSuccess;if(output)*output=status;return status;
}
template<typename T>struct MayaArray {
    std::vector<T> values;
    unsigned length()const{return unsigned(values.size());}
    const T& operator[](std::size_t index)const{return values.at(index);}
};
using MPoint=std::array<double,3>;
using MPointArray=MayaArray<MPoint>;using MIntArray=MayaArray<int>;using MFloatArray=MayaArray<float>;
struct MObject { bool isNull()const{return false;} };
struct MDataHandle {MObject data()const{return {};}};
namespace MSpace {enum Space{kWorld};}
int mismatch=0;
double pointCoordinate=0.0;
std::vector<int> faceValues{3},indexValues{0,1,2},uvCountValues{3},uvIndexValues{0,1,2};
std::vector<float> uValues{0,1,0},vValues{0,0,1};
struct MFnMesh {
    explicit MFnMesh(MObject,MStatus* output=nullptr){query("mesh.bind",output);}
    int numVertices(MStatus* output=nullptr)const{query("mesh.points.count",output);return mismatch==6?-1:3;}
    int numPolygons(MStatus* output=nullptr)const{query("mesh.faces.count",output);return 1;}
    int numFaceVertices(MStatus* output=nullptr)const{query("mesh.connections.count",output);return 3;}
    int numUVs(MStatus* output=nullptr)const{query("mesh.uvs.count",output);return int(uValues.size());}
    MStatus getPoints(MPointArray& output,MSpace::Space)const{
        const MStatus status=query("mesh.points");output.values={MPoint{{pointCoordinate,0,0}},MPoint{{1,0,0}},MPoint{{0,1,0}}};
        if(mismatch==1)output.values.pop_back();return status;
    }
    MStatus getVertices(MIntArray& counts,MIntArray& ids)const{
        const MStatus status=query("mesh.vertices");counts.values=faceValues;ids.values=indexValues;
        if(mismatch==2)counts.values.clear();return status;
    }
    MStatus getUVs(MFloatArray& u,MFloatArray& v)const{
        const MStatus status=query("mesh.uvs");u.values=uValues;v.values=vValues;return status;
    }
    MStatus getAssignedUVs(MIntArray& counts,MIntArray& ids)const{
        const MStatus status=query("mesh.uv.ids");counts.values=uvCountValues;ids.values=uvIndexValues;return status;
    }
};
struct MString {std::string text;MString(const char* value=""):text(value){}const char* asChar()const{return text.c_str();}};
using MStringArray=MayaArray<MString>;
struct MMatrix {
    double cells[4][4]{};
    MMatrix(){for(unsigned i=0;i<4;++i)cells[i][i]=1.0;}
    double* operator[](std::size_t row){return cells[row];}
    const double* operator[](std::size_t row)const{return cells[row];}
    void get(float (&result)[4][4])const{for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)result[r][c]=float(cells[r][c]);}
};
struct MVector {double values[3];MVector(double a,double b,double c):values{a,b,c}{}double operator[](std::size_t axis)const{return values[axis];}};
#endif
struct AtString {const char* value;explicit AtString(const char* v):value(v){}};
struct AtVector {float x,y,z;};struct AtVector2 {float x,y;};
struct AtArray {std::vector<bool> assigned;std::vector<AtVector> vectors;std::vector<unsigned> integers;unsigned char keys;};
struct AtNode {std::map<std::string,AtArray*> arrays;};
AtNode* AiNode(const char*);bool AiNodeDestroy(AtNode*);AtArray* AiArrayAllocate(unsigned,unsigned char,int);void AiArrayDestroy(AtArray*);
bool AiArraySetVec(AtArray*,std::size_t,AtVector);bool AiArraySetVec2(AtArray*,std::size_t,AtVector2);bool AiArraySetUInt(AtArray*,std::size_t,unsigned);
void AiNodeSetArray(AtNode*,AtString,AtArray*);void AiNodeSetByte(AtNode*,const char*,unsigned char);
void AiMsgError(const char*,...);void AiMsgWarning(const char*,...);
bool AiNodeDeclare(AtNode*,const char*,const char*);void AiNodeSetInt(AtNode*,const char*,int);void AiNodeSetUInt(AtNode*,const char*,unsigned);
void AiNodeSetFlt(AtNode*,const char*,float);void AiNodeSetVec2(AtNode*,const char*,float,float);void AiNodeSetVec(AtNode*,const char*,float,float,float);
float submitted=0.0f;
bool attributeAccepted=false;
enum DP_ATTR_DATA_TYPE {DP_ATTR_BOOL,DP_ATTR_INT,DP_ATTR_UINT32,DP_ATTR_INT64,DP_ATTR_FLOAT,DP_ATTR_DOUBLE,
    DP_ATTR_VEC2I,DP_ATTR_VEC2F,DP_ATTR_VEC2D,DP_ATTR_VEC3I,DP_ATTR_VEC3F,DP_ATTR_VEC3D,DP_ATTR_STRING};
struct FixtureAttribute {DP_ATTR_DATA_TYPE type=DP_ATTR_DOUBLE;std::size_t count=2U;std::size_t attrSize()const{return count;}DP_ATTR_DATA_TYPE getAttrType()const{return type;}};
struct DPData {
    bool exists=true;
    FixtureAttribute attribute;
    std::vector<double> scalarValues{1.25,2.5};
    std::vector<std::int64_t> integerValues{1,2};
    bool isExistAttr(const std::string&)const{return exists;}
    const FixtureAttribute* getAttr(const std::string&)const{return &attribute;}
    template<typename T>T attrListValueWithIndex(const std::string&,std::size_t)const;
};
#ifndef DOMMY_SDK_CHECK
#define ATTR_SCALAR(TYPE) template<>TYPE DPData::attrListValueWithIndex<TYPE>(const std::string&,std::size_t i)const{return TYPE(scalarValues.at(i));}
ATTR_SCALAR(double) ATTR_SCALAR(float)
#undef ATTR_SCALAR
#define ATTR_INT(TYPE) template<>TYPE DPData::attrListValueWithIndex<TYPE>(const std::string&,std::size_t i)const{return TYPE(integerValues.at(i));}
ATTR_INT(std::int64_t) ATTR_INT(std::int32_t) ATTR_INT(std::uint32_t)
#undef ATTR_INT
#define ATTR_VEC(TYPE) template<>TYPE DPData::attrListValueWithIndex<TYPE>(const std::string&,std::size_t i)const{return TYPE(scalarValues.at(i),2);}
ATTR_VEC(Imath::V2i) ATTR_VEC(Imath::V2f) ATTR_VEC(Imath::V2d) ATTR_VEC(Imath::V3i) ATTR_VEC(Imath::V3f) ATTR_VEC(Imath::V3d)
#undef ATTR_VEC
#endif
using DScalarDataList=std::vector<float>;
void getDScalarArrayFromAttribute(const DPData& points,const std::string&,DScalarDataList& values){
    if(points.attribute.type<DP_ATTR_INT || points.attribute.type>DP_ATTR_DOUBLE)return;
    values=dommy::arnold::checkedFloatValues(points.scalarValues);values.resize(1U);
}
bool getCheckedDScalarArrayFromAttribute(const DPData& points,const std::string& name,std::size_t count,DScalarDataList& values){
    if(points.attribute.type<DP_ATTR_INT || points.attribute.type>DP_ATTR_DOUBLE || points.attribute.count!=count)return false;
    getDScalarArrayFromAttribute(points,name,values);return values.size()==count;
}
struct DPInstancerTranslatorAi {
    AtNode* generateAiMeshFromMPlug(MDataHandle,unsigned);
    void instanceAssignDPAttribute(const DPData*,const MStringArray&,AtNode*,std::size_t);
    void attributeAdmission(const DPData*,const MStringArray&,std::size_t);
    bool RequiresMotionData();
    void ExportMotion(AtNode*);
};
#include "optional_translation.inc"
#ifndef DOMMY_SDK_CHECK
void allocation(){if(allocationCalls++==throwAllocationAt)throw std::bad_alloc();}
AtNode* AiNode(const char*){allocation();if(allocationCalls-1==allocationAt)return nullptr;++createdNodes;++liveNodes;return new AtNode;}
void AiArrayDestroy(AtArray* array){--liveArrays;delete array;}
bool AiNodeDestroy(AtNode* node){for(auto& entry:node->arrays)AiArrayDestroy(entry.second);parameterFixture::users.erase(node);--liveNodes;delete node;return true;}
AtArray* AiArrayAllocate(unsigned count,unsigned char keys,int){allocation();if(allocationCalls-1==allocationAt)return nullptr;++liveArrays;auto* result=new AtArray;
    result->keys=keys;result->assigned.assign(std::size_t(count)*keys,false);result->vectors.resize(std::size_t(count)*keys);result->integers.resize(std::size_t(count)*keys);return result;}
int failElementAt=-1,elementCalls=0;
bool elementWrite(){return elementCalls++!=failElementAt;}
bool AiArraySetVec(AtArray* a,std::size_t i,AtVector v){if(!elementWrite())return false;a->assigned.at(i)=true;a->vectors.at(i)=v;return true;}
bool AiArraySetVec2(AtArray* a,std::size_t i,AtVector2 v){if(!elementWrite())return false;assert(std::isfinite(v.x)&&std::isfinite(v.y));a->assigned.at(i)=true;return true;}
bool AiArraySetUInt(AtArray* a,std::size_t i,unsigned v){if(!elementWrite())return false;a->assigned.at(i)=true;a->integers.at(i)=v;return true;}
void AiNodeSetArray(AtNode* n,AtString name,AtArray* a){n->arrays[name.value]=a;}
void AiNodeSetByte(AtNode*,const char*,unsigned char){++rendererWrites;}
void AiMsgError(const char*,...){++errors;}void AiMsgWarning(const char*,...){}
bool AiNodeDeclare(AtNode* node,const char* name,const char* declaration){++rendererWrites;return parameterFixture::declare(node,name,declaration);}
void AiNodeSetInt(AtNode*,const char*,int value){++rendererWrites;submitted=float(value);submittedInt=value;}
void AiNodeSetUInt(AtNode*,const char*,unsigned value){++rendererWrites;submitted=float(value);}
void AiNodeSetFlt(AtNode*,const char*,float value){++rendererWrites;submitted=value;}
void AiNodeSetVec2(AtNode*,const char*,float a,float b){++rendererWrites;assert(std::isfinite(a)&&std::isfinite(b));}
void AiNodeSetVec(AtNode*,const char*,float a,float b,float c){++rendererWrites;assert(std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c));}
void resetMesh(){queries.clear();failAt=-1;exceptionKind=errors=createdNodes=rendererWrites=mismatch=0;pointCoordinate=0;
    parameterFixture::reset();
    allocationAt=throwAllocationAt=-1;allocationCalls=0;
    failElementAt=-1;elementCalls=0;
    faceValues={3};indexValues={0,1,2};uValues={0,1,0};vValues={0,0,1};uvCountValues={3};uvIndexValues={0,1,2};assert(liveNodes==0&&liveArrays==0);}
int rejectionIndex=0;
template<typename Operation>void rejects(Operation operation){++rejectionIndex;bool rejected=false;try{operation();}catch(const std::invalid_argument&){rejected=true;}if(!rejected)std::cerr<<"Missing rejection case "<<rejectionIndex<<'\n';assert(rejected);}
void meshTests(){
    DPInstancerTranslatorAi translator;resetMesh();{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(mesh);}
    const auto trace=queries;
    for(std::size_t i=0U;i<trace.size();++i)for(int failure:{0,1,2}){
        resetMesh();failAt=int(i);exceptionKind=failure;bool threw=false;
        try{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh);}catch(const std::bad_alloc&){threw=true;}catch(const std::runtime_error&){threw=true;}
        assert(threw==(failure!=0)&&createdNodes==0&&liveNodes==0&&liveArrays==0);
    }
    for(int malformed:{1,2,6}){resetMesh();mismatch=malformed;dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh&&createdNodes==0);}
    for(const auto& ids:{std::vector<int>{-1,1,2},std::vector<int>{0,1,99},std::vector<int>{0,1}}){resetMesh();indexValues=ids;dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh&&createdNodes==0);}
    resetMesh();faceValues={2};{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh&&createdNodes==0);}
    resetMesh();vValues.pop_back();{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh&&createdNodes==0);}
    resetMesh();uvIndexValues={0,-1,99};{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh&&createdNodes==0);}
    resetMesh();pointCoordinate=1.0e300;{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(!mesh&&createdNodes==0);}
    resetMesh();uValues.clear();vValues.clear();uvCountValues.clear();uvIndexValues.clear();
    {dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(mesh&&mesh.get()->arrays.count("uvlist")==0U);}
    for(const auto& q:queries)assert(q!="mesh.uvs"&&q!="mesh.uv.ids");
    resetMesh();uvCountValues={0};uvIndexValues.clear();
    {dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(mesh&&mesh.get()->arrays.count("uvlist")==0U);}
    for(int allocationIndex=0;allocationIndex<6;++allocationIndex){
        resetMesh();allocationAt=allocationIndex;
        {dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));
            assert(bool(mesh)==(allocationIndex>=4));if(mesh)assert(mesh.get()->arrays.count("uvlist")==0U);}
        assert(liveNodes==0&&liveArrays==0);
        resetMesh();throwAllocationAt=allocationIndex;bool threw=false;
        try{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));}
        catch(const std::bad_alloc&){threw=true;}assert(threw&&liveNodes==0&&liveArrays==0);
    }
    resetMesh();{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(mesh);}
    const int writes=elementCalls;
    for(int at=0;at<writes;++at){
        resetMesh();failElementAt=at;bool rejected=false;
        try{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));}
        catch(const std::runtime_error&){rejected=true;}
        assert(rejected&&elementCalls==at+1&&liveNodes==0&&liveArrays==0);
        resetMesh();{dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},1));assert(mesh);}
    }
}
void numericTests(){
    resetMesh(); // Keep this phase independent of meshTests' metadata setup.
    DPInstancerTranslatorAi translator;DPData points;MStringArray names;names.values={MString("value")};AtNode target;
    points.integerValues={INT64_C(2147483648),0};points.attribute.type=DP_ATTR_INT64;
    rejects([&]{translator.instanceAssignDPAttribute(&points,names,&target,0);});
    for(std::int64_t value:{std::int64_t(std::numeric_limits<int>::min()),std::int64_t(std::numeric_limits<int>::max())}){
        points.integerValues={value,0};translator.instanceAssignDPAttribute(&points,names,&target,0);assert(submittedInt==value);}
    points.integerValues={std::int64_t(std::numeric_limits<int>::min())-1,0};
    rejects([&]{translator.instanceAssignDPAttribute(&points,names,&target,0);});
    // The following field-type cases represent fresh renderer destinations.
    parameterFixture::users.erase(&target);
    for(auto type:{DP_ATTR_DOUBLE,DP_ATTR_VEC2D,DP_ATTR_VEC3D}){points.attribute.type=type;points.scalarValues={1.0e300,2.5};
        rejects([&]{translator.instanceAssignDPAttribute(&points,names,&target,0);});}
    points.attribute.type=DP_ATTR_DOUBLE;points.scalarValues={1.25,1.0e300};rendererWrites=0;attributeAccepted=false;
    rejects([&]{translator.attributeAdmission(&points,names,2U);});assert(!attributeAccepted&&rendererWrites==0);
    points.scalarValues={1.25,-2.5};translator.attributeAdmission(&points,names,2U);assert(attributeAccepted&&rendererWrites==0);
    translator.instanceAssignDPAttribute(&points,names,&target,1U);assert(submitted==-2.5f);
    for (const char* name : {"radius","matrix","dInstObjTag","dInstMeshTag","dlightTag"}) {
        names.values={MString(name)}; rendererWrites=0; attributeAccepted=false;
        rejects([&]{translator.attributeAdmission(&points,names,2U);});
        assert(!attributeAccepted && rendererWrites==0);
    }
    names.values={MString("fresh")};rendererWrites=0;parameterFixture::failDeclaration=true;
    bool declarationFailed=false;
    try{translator.instanceAssignDPAttribute(&points,names,&target,0);}catch(const std::runtime_error&){declarationFailed=true;}
    assert(declarationFailed && rendererWrites==1); // Declaration, no value setter.
    parameterFixture::failDeclaration=false;translator.instanceAssignDPAttribute(&points,names,&target,0);
    assert(submitted==1.25f);
    assert(repeatStage(0.0)==0.0f&&repeatStage(-2.0)==-0.5f);rejects([]{(void)repeatStage(1.0e-300);});
    assert(exposureStage(1.25)==1.25f);rejects([]{(void)exposureStage(1.0e300);});
    assert(intensityStage(1.0e300,true)==2.5f);rejects([]{(void)intensityStage(1.0e300,false);});
    rejects([]{(void)intensityStage(1.0,true,DP_ATTR_STRING);});
    rejects([]{(void)intensityStage(1.0,true,DP_ATTR_VEC3F);});
    assert(radiusStage(1.0e300,0.0f)==0.0f);rejects([]{(void)radiusStage(1.0e300,1.0f);});
    MMatrix matrix;matrix[0][0]=1.0e300;rejects([&]{(void)matrixStage(matrix);});matrix[0][0]=1.25;assert(matrixStage(matrix)==1.25f);
}
void motionTests(){
    DPInstancerTranslatorAi translator;
    assert(!translator.RequiresMotionData());
    const int writesBefore=rendererWrites;translator.ExportMotion(nullptr);assert(rendererWrites==writesBefore);
    for(unsigned motionSteps:{1U,2U,255U}){resetMesh();dommy::arnold::NodeOwner mesh(translator.generateAiMeshFromMPlug({},motionSteps));assert(mesh);
        const auto* vertices=mesh.get()->arrays.at("vlist");assert(vertices->keys==1U&&vertices->assigned.size()==3U);
        assert(std::count(vertices->assigned.begin(),vertices->assigned.end(),true)==3);}
}
int main(int argc,char** argv){const std::string phase=argc>1?argv[1]:"all";
    if(phase=="all"||phase=="mesh")meshTests();
    if(phase=="all"||phase=="numeric")numericTests();
    if(phase=="all"||phase=="motion")motionTests();
    assert(liveNodes==0&&liveArrays==0);
}
#endif
