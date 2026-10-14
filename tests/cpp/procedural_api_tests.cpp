// Host boundaries are doubles. Real attribute storage, normalization,
// publication helpers, entry bodies and transform parameter struct are used.
#define DOMMY_PROCEDURAL_API_TESTS
#define setPluginDataOutput unusedFixtureSetPluginDataOutput
#include "copy_utility_compute_doubles.h"
#undef setPluginDataOutput
#include "modules/point/DPoint/math/DUnaryMath.h"
#include "math/DRemapMath.h"
#include "common/utility/DAttributeMathInput.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DUvParameter.h"
#include "common/utility/DPointPayloadBudget.h"
#include <utility>
#include <cstring>

const int DPData::id;
template<int Id>const int PrimitiveData<Id>::id;

using double2=double[2];
using MStringArray=std::vector<MString>;
MStatus CopyString::split(char,MStringArray& out)const{
    const auto status=operation("string.split");if(!status)return status;
    std::istringstream stream(value);std::string word;while(stream>>word)out.emplace_back(word);return status;
}
static int meshConstructions=0, mathCalls=0, seenOperation=-1;
static std::string seenSource,seenOutput;
static bool seenCreate=false,seenDelete=false;
static DPData stagedPoints;
static DCurveData stagedCurves;
static DMeshData stagedMeshes;
static DPData& stagedFor(DPData*){return stagedPoints;}
static DCurveData& stagedFor(DCurveData*){return stagedCurves;}
static DMeshData& stagedFor(DMeshData*){return stagedMeshes;}
using float2=float[2];
using float3=float[3];
struct MFnPluginData {
    MObject value;
    MObject object()const{return value;}
    MObject create(int id,MStatus* status);
};
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
MStatus MDataHandle::set(const MObject& object){
    assert(valid);const auto status=operation("output.publish");if(!status)return status;
    if(object.name=="22")outputPoints=stagedPoints;
    else if(object.name=="23")outputCurves=stagedCurves;
    else if(object.name=="24")outputMeshes=stagedMeshes;
    else assert(false);
    ++publications;return status;
}
namespace dommy {namespace maya {
template<class T>bool validatePluginData(const T& data){
    return data.valid()&&(!rejectFinal||&data!=&stagedFor(static_cast<T*>(nullptr)));
}
template<class T>MStatus getPluginDataFromObject(const MObject& object,int id,const T*& out){
    const auto status=operation("output.bind");if(!status)return status;
    assert(object.name==std::to_string(id));out=&stagedFor(static_cast<T*>(nullptr));return status;
}
template<class T>MStatus createPluginDataObject(int id,MObject& object,T*& out){
    const auto status=operation("output.create");if(!status)return status;
    if(nullCreated)return MS::kFailure;
    T& candidate=stagedFor(static_cast<T*>(nullptr));candidate=T();
    object={std::to_string(id)};out=&candidate;return status;
}
template<class T,class Populate>MStatus createPluginDataObject(int id,Populate populate,MObject& object){
    T* candidate=nullptr;auto status=createPluginDataObject<T>(id,object,candidate);
    if(!status)return status;
    status=invokeStatusOperation(populate,*candidate);if(!status)return status;
    return validatePluginData(*candidate)?MS::kSuccess:MS::kInvalidParameter;
}
template<class T>MStatus createPluginData(MFnPluginData& owner,int id,T*& out){
    return createPluginDataObject(id,owner.value,out);
}
template<class T>MStatus getPluginData(MFnPluginData& owner,T*& out){
    const auto status=operation("factory.bind");out=nullptr;if(!status)return status;
    if(owner.value.name!=std::to_string(T::id))return MS::kFailure;
    out=&stagedFor(static_cast<T*>(nullptr));return status;
}
template<class T>T* pluginDataPointer(MFnPluginData& owner,MStatus* status){
    T* out=nullptr;*status=getPluginData(owner,out);return out;
}
// Array normalization is a seam here; complete native-array mapping has its
// own failure-injection suite. Never claim native Maya geometry coverage.
template<class Op>MStatus mapMayaUnaryArray(MDataBlock&,const MPlug&,const MObject&,const MObject&,Op,bool=false,bool=false){
    ++mathCalls;return MS::kSuccess;
}
}}
MObject MFnPluginData::create(int id,MStatus* status){
    *status=operation("output.create");if(!*status||nullCreated)return {"null"};
    if(id==DPData::id)stagedPoints=DPData();
    else if(id==DCurveData::id)stagedCurves=DCurveData();
    else if(id==DMeshData::id)stagedMeshes=DMeshData();
    else return {"null"};
    value={std::to_string(id)};return value;
}
template<class T>const T* getInput(const MObject& attr,MDataBlock& data){
    MStatus status;auto handle=data.inputValue(attr,&status);if(!status)return nullptr;
    const T* result=nullptr;status=dommy::maya::getPluginDataFromHandle(handle,T::id,result);
    return status&&result&&result->valid()?result:nullptr;
}
static const DPData* getInputDP(const MObject& a,MDataBlock& d){return getInput<DPData>(a,d);}
static const DCurveData* getInputDC(const MObject& a,MDataBlock& d){return getInput<DCurveData>(a,d);}
static const DMeshData* getInputDM(const MObject& a,MDataBlock& d){return getInput<DMeshData>(a,d);}
enum {ID_DP_DATA=22,ID_DC_DATA=23,ID_DM_DATA=24};
static void EchoWarning(const char*,const char*){}
static void EchoWarning(const std::string&,const char*){}
namespace dommy {namespace maya {
inline bool isFinitePoint(const MPoint& point){return isFiniteVector(MVector(point));}
}}
const double kMFnNurbsEpsilon=1e-3;
static bool nonfiniteUv=false;
static std::vector<std::array<double,3>> uvQueryPoints;
struct MFnNurbsCurve {
    MFnNurbsCurve(const MObject&,MStatus* status){*status=operation("uv.curve.bind");}
    MStatus getKnotDomain(double& first,double& last)const{first=10;last=20;return operation("uv.curve.domain");}
    MPoint closestPoint(const MPoint& point,double* parameter,double,MSpace::Space space,MStatus* status)const{
        assert(space==MSpace::kWorld);uvQueryPoints.push_back({{point[0],point[1],point[2]}});
        *status=operation("uv.curve.sample");*parameter=nonfiniteUv?NAN:10.0+10.0*point[0];return point;
    }
};
struct MFnNurbsSurface {
    MFnNurbsSurface(const MObject&,MStatus* status){*status=operation("uv.surface.bind");}
    MStatus getParamAtPoint(const MPoint& point,double& u,double& v,MSpace::Space space)const{
        assert(space==MSpace::kWorld);uvQueryPoints.push_back({{point[0],point[1],point[2]}});
        u=nonfiniteUv?NAN:point[0];v=point[1];return operation("uv.surface.sample");
    }
};
struct MFnMesh {
    MFnMesh(const MObject&,MStatus* status){*status=operation("uv.mesh.bind");}
    MStatus getUVAtPoint(const MPoint& point,float (&uv)[2],MSpace::Space space)const{
        assert(space==MSpace::kWorld);uvQueryPoints.push_back({{point[0],point[1],point[2]}});
        uv[0]=nonfiniteUv?NAN:static_cast<float>(point[0]);uv[1]=static_cast<float>(point[1]);
        return operation("uv.mesh.sample");
    }
};
namespace shared_math_body {
// This boundary exercises admission/copy/status/publication, not operand reads
// or Maya math execution. Production numeric kernels have independent tests.
static bool corruptOutput=false;
template<class Owner,class... Args>MStatus processingBoundary(Owner& output,Args&&...){
    ++mathCalls;const auto status=operation("math.process");
    if(status&&corruptOutput)output.topology=false;return status;
}
template<class... Args>MStatus processDPointMathFncSingle(Args&&... args){return processingBoundary(std::forward<Args>(args)...);}
template<class... Args>MStatus processDCurveMathFncSingle(Args&&... args){return processingBoundary(std::forward<Args>(args)...);}
template<class... Args>MStatus processDMeshMathFncSingle(Args&&... args){return processingBoundary(std::forward<Args>(args)...);}
template<class... Args>MStatus processDPointMathFnc(Args&&... args){return processingBoundary(std::forward<Args>(args)...);}
template<class... Args>MStatus processDCurveMathFnc(Args&&... args){return processingBoundary(std::forward<Args>(args)...);}
template<class... Args>MStatus processDMeshMathFnc(Args&&... args){return processingBoundary(std::forward<Args>(args)...);}
}
struct dupMesh {
    explicit dupMesh(const MObject&){voidApi("mesh.admit");++meshConstructions;}
    explicit dupMesh(const std::shared_ptr<Primitive>&){voidApi("mesh.admit");++meshConstructions;}
    bool isValid()const{return true;}
};
static MStatus mathProcess(const std::string& source,const std::string& output,bool create,bool remove,int op){
    ++mathCalls;seenSource=source;seenOutput=output;seenCreate=create;seenDelete=remove;seenOperation=op;
    return MS::kSuccess;
}
static MStatus processDDataMathOutput(const MObject&,const MObject&,const char*,const std::string& source,
    const std::string& output,bool create,bool remove,const MObject&,const MObject&,const MObject&,
    MDataBlock&,const MPlug&,int op){return mathProcess(source,output,create,remove,op);}
static MStatus processDDataMathOutputSingle(const MObject&,const MObject&,const char*,const std::string& source,
    const std::string& output,bool create,bool remove,MDataBlock&,const MPlug&,int op){
    return mathProcess(source,output,create,remove,op);
}
static MStatus processMayaNumericMathFunc(const MObject&,const char*,const MObject&,const MObject&,
    const MObject&,const MObject&,MDataBlock&,const MPlug&,int op){++mathCalls;seenOperation=op;return MS::kSuccess;}
static MStatus processMayaNumericMathFuncSingle(const MObject&,const char*,const MObject&,const MObject&,
    MDataBlock&,const MPlug&,int op){++mathCalls;seenOperation=op;return MS::kSuccess;}

#include "procedural_api.inc"

static void reset(int failure=-1){
    resetCompute(failure);mathCalls=meshConstructions=0;connectedPlugs.clear();
    nonfiniteUv=rejectUvField=false;uvQueryPoints.clear();shared_math_body::corruptOutput=false;
}
static std::size_t callsNamed(const char* name){return std::count(operations.begin(),operations.end(),name);}
static MDataBlock blockFor(const Payload& payload){
    MDataBlock data;data.inputs["inDD"].payload=&payload;data.inputs["inDD"].type=payload.type();
    data.inputs["aSourceAttribute"].text="velocity";
    data.inputs["aOutAttribute"].text="result";
    return data;
}
template<class Node>static void outputFailures(Node& node,const Payload& payload){
    auto data=blockFor(payload);reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);
    assert(publications==1);const auto trace=operations;
    for(std::size_t i=0;i<trace.size();++i){
        if(trace[i]!="output.handle"&&trace[i]!="output.publish"&&trace[i]!="output.bind")continue;
        for(auto code:{MS::kFailure,MS::kInvalidParameter}){
            reset(static_cast<int>(i));injectedStatus=code;
            assert(node.compute(Node::outDD,data)==code);assert(publications==0);
            assert(callCount==static_cast<int>(i)+1);
        }
    }
    for(std::size_t i=0;i<trace.size();++i){
        if(trace[i].find("input ")==0||trace[i]=="input.bind"||trace[i]=="output.create"||trace[i]=="primitive.copy"){
            reset(static_cast<int>(i));assert(node.compute(Node::outDD,data)==MS::kFailure);
            assert(publications==0&&callCount==static_cast<int>(i)+1);
        }
        if(trace[i]=="output.clean"||trace[i]=="handle.clean")continue;
        reset();attributeThrowAt=static_cast<int>(i);
        assert(node.compute(Node::outDD,data)==MS::kFailure);assert(publications==0);
    }
    reset();rejectFinal=true;assert(node.compute(Node::outDD,data)==MS::kInvalidParameter);assert(publications==0);
    reset();nullCreated=true;assert(node.compute(Node::outDD,data)==MS::kFailure);assert(publications==0);
    reset();assert(node.compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);assert(callCount==0);
}
static void genericOutputs(){
    DPNull nullNode;DPNormalize normalize;
    DPData points;points.points={{1,2,3}};
    DCurveData curves;curves.primitives.push_back(std::make_shared<Primitive>());
    DMeshData meshes;meshes.primitives.push_back(std::make_shared<Primitive>());
    for(const Payload* input:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        outputFailures(nullNode,*input);outputFailures(normalize,*input);
        auto data=blockFor(*input);reset(0);assert(nullNode.compute(DPNull::outDD,data)==MS::kFailure);assert(publications==0);
        reset(4);assert(normalize.compute(DPNormalize::outDD,data)==MS::kFailure);assert(publications==0);
        data.inputs["inDD"].payload=nullptr;reset();assert(!nullNode.compute(DPNull::outDD,data));assert(publications==0);
        reset();assert(!normalize.compute(DPNormalize::outDD,data));assert(publications==0);
    }
    points.topology=false;curves.topology=false;meshes.topology=false;
    for(const Payload* input:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=blockFor(*input);reset();assert(nullNode.compute(DPNull::outDD,data)==MS::kInvalidParameter);assert(publications==0);
        reset();assert(normalize.compute(DPNormalize::outDD,data)==MS::kInvalidParameter);assert(publications==0);
    }
    points=DPData();curves=DCurveData();meshes=DMeshData();
    outputFailures(nullNode,points);outputFailures(nullNode,curves);outputFailures(nullNode,meshes);
    outputFailures(normalize,points);outputFailures(normalize,curves);outputFailures(normalize,meshes);
    assert(outputPoints.points.empty()&&outputCurves.primitives.empty()&&outputMeshes.primitives.empty());
    struct Unsupported:Payload{int type()const override{return 99;}bool valid()const override{return true;}} unsupported;
    auto wrong=blockFor(unsupported);reset();assert(nullNode.compute(DPNull::outDD,wrong)==MS::kInvalidParameter);assert(publications==0);
    reset();assert(normalize.compute(DPNormalize::outDD,wrong)==MS::kInvalidParameter);assert(publications==0);
    // Exact input retrieval count is a regression for the retained handle.
    auto data=blockFor(points);reset();assert(nullNode.compute(DPNull::outDD,data));assert(callsNamed("input inDD")==1);
    reset();assert(normalize.compute(DPNormalize::outDD,data));assert(callsNamed("input inDD")==1);
    std::cout<<"Generic DP/DC/DM outputs: checked admission, copy, publication and empty results\n";
}
template<class Owner>static void addVelocity(Owner& owner){
    owner.attributes["velocity"]=std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{{3,4,0}});
}
static void normalized(const DPAttributeData::ConstPtr& attr){
    auto vector=std::dynamic_pointer_cast<const DPVec3dAttr>(attr);assert(vector&&vector->value().size()==1);
    assert(std::abs(vector->value()[0][0]-0.6)<1e-12&&std::abs(vector->value()[0][1]-0.8)<1e-12);
}
static void originalVelocity(const DPAttributeData::ConstPtr& attr){
    auto vector=std::dynamic_pointer_cast<const DPVec3dAttr>(attr);assert(vector);
    assert(vector->value()[0][0]==3&&vector->value()[0][1]==4);
}
static void normalization(){
    DPData points;points.points={{1,2,3}};addVelocity(points);
    DCurveData curves;curves.primitives.push_back(std::make_shared<Primitive>());addVelocity(curves);
    DMeshData meshes;meshes.primitives.push_back(std::make_shared<Primitive>());addVelocity(meshes);
    DPNormalize node;
    for(const Payload* input:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=blockFor(*input);reset();assert(node.compute(DPNormalize::outDD,data)==MS::kSuccess);
        if(input->type()==22)normalized(outputPoints.getAttr("velocity"));
        if(input->type()==23)normalized(outputCurves.getAttr("velocity"));
        if(input->type()==24)normalized(outputMeshes.getAttr("velocity"));
    }
    originalVelocity(points.getAttr("velocity"));originalVelocity(curves.getAttr("velocity"));
    originalVelocity(meshes.getAttr("velocity"));
    curves.attributes.clear();meshes.attributes.clear();addVelocity(*curves.primitives[0]);addVelocity(*meshes.primitives[0]);
    for(const Payload* input:{static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=blockFor(*input);reset();assert(node.compute(DPNormalize::outDD,data)==MS::kSuccess);
        if(input->type()==23)normalized(outputCurves.primitives[0]->getAttr("velocity"));
        else normalized(outputMeshes.primitives[0]->getAttr("velocity"));
    }
    originalVelocity(curves.primitives[0]->getAttr("velocity"));originalVelocity(meshes.primitives[0]->getAttr("velocity"));
    // Legacy new-output float storage, same-name delete and vec2 dispatch.
    auto data=blockFor(points);data.inputs["aIsOutAttribute"].number=1;data.inputs["aIsDeleteOriginal"].number=1;
    data.inputs["aOutAttribute"].text="velocity";reset();
    // Existing double output cannot silently become the legacy float type.
    assert(node.compute(DPNormalize::outDD,data)==MS::kInvalidParameter);assert(publications==0);
    data.inputs["aOutAttribute"].text="result";reset();assert(node.compute(DPNormalize::outDD,data));
    assert(!outputPoints.isExistAttr("velocity")&&outputPoints.getAttr("result")->getAttrType()==DP_ATTR_VEC3F);
    originalVelocity(points.getAttr("velocity"));
    points.attributes["velocity"]=std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{{3,4,0}});
    data.inputs["aOutAttribute"].text="velocity";reset();assert(node.compute(DPNormalize::outDD,data));
    auto same=std::dynamic_pointer_cast<const DPVec3fAttr>(outputPoints.getAttr("velocity"));assert(same);
    assert(std::abs(same->value()[0][0]-0.6f)<1e-6f);
    auto untouched=std::dynamic_pointer_cast<const DPVec3fAttr>(points.getAttr("velocity"));
    assert(untouched&&untouched->value()[0][0]==3&&untouched->value()[0][1]==4);
    points.attributes["velocity"]=std::make_shared<DPVec2dAttr>(std::vector<Imath::V2d>{{3,4}});
    data=blockFor(points);reset();assert(node.compute(DPNormalize::outDD,data));
    auto two=std::dynamic_pointer_cast<const DPVec2dAttr>(outputPoints.getAttr("velocity"));assert(two);
    assert(std::abs(two->value()[0][0]-0.6)<1e-12&&std::abs(two->value()[0][1]-0.8)<1e-12);
    std::cout<<"Normalize: production math and storage at point, primitive and primitive-point levels\n";
}
template<class Node>static void mathNode(int op,const char* mode=nullptr){
    Node node;MDataBlock data;data.inputs["aSourceAttribute"].text="source";data.inputs["aOutAttribute"].text="result";
    data.inputs["aIsOutAttribute"].number=1;data.inputs["aIsDeleteOriginal"].number=1;
    if(mode)data.inputs[mode].number=op;
    reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);assert(mathCalls==1&&seenOperation==op);
    assert(seenSource=="source"&&seenOutput=="result"&&seenCreate&&seenDelete);
    const int reads=mode?5:4;
    for(int at=0;at<reads;++at)for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        reset(at);injectedStatus=code;assert(node.compute(Node::outDD,data)==code);
        assert(mathCalls==0&&callCount==at+1);
    }
    reset();connectedPlugs.insert("inSourceDataArray");assert(node.compute(Node::outDataArray,data)==MS::kSuccess);
    assert(mathCalls==1&&seenOperation==op);assert(callsNamed("input aSourceAttribute")==0);
    if(mode){reset(0);assert(node.compute(Node::outDataArray,data)==MS::kFailure);assert(mathCalls==0);}
    reset(mode?1:0);connectedPlugs.insert("inSourceDataArray");
    assert(node.compute(Node::outDataArray,data)==MS::kFailure);assert(mathCalls==0);
    reset();assert(node.compute(Node::outDataArray,data)==MS::kFailure);assert(mathCalls==0);
    data.inputs["aOutAttribute"].text="";reset();assert(node.compute(Node::outDD,data)==MS::kInvalidParameter);assert(mathCalls==0);
    reset();assert(node.compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);assert(callCount==0);
}
static void mathSettings(){
    mathNode<DPNegative>(D_SRC_NEGATIVE);mathNode<DPAdd>(D_CAL_ADD);mathNode<DPRound>(D_SRC_ROUND);
    mathNode<DPMin>(D_CAL_MIN);mathNode<DPDivide>(D_CAL_DIV);mathNode<DPExponential>(D_SRC_EXPONENTIAL);
    mathNode<DPAbsolute>(D_SRC_ABS);mathNode<DPMathFunc>(D_SRC_SQRT,"aFunc");mathNode<DPModulo>(D_CAL_MOD);
    mathNode<DPSubtract>(D_CAL_SUB);mathNode<DPSqrt>(D_SRC_SQRT);mathNode<DPCeil>(D_SRC_CEIL);
    mathNode<DPMax>(D_CAL_MAX);mathNode<DPFloor>(D_SRC_FLOOR);mathNode<DPMath>(D_CAL_POW,"aMethod");
    mathNode<DPPow>(D_CAL_POW);mathNode<DPMultiply>(D_CAL_MULT);
#ifdef DOMMY_MATH_SETTINGS_AVAILABLE
    MDataBlock data;data.inputs["aSourceAttribute"].text="new";data.inputs["aOutAttribute"].text="new-result";
    for(int at=0;at<4;++at){
        dommy::maya::MathNodeSettings settings;settings.sourceAttribute="old";settings.outputAttribute="old-result";
        reset(at);assert(!dommy::maya::readMathNodeSettings<DPMin>(data,settings));
        assert(settings.sourceAttribute=="old"&&settings.outputAttribute=="old-result");
        reset();attributeThrowAt=at;
        try{(void)dommy::maya::readMathNodeSettings<DPMin>(data,settings);assert(false);}
        catch(const std::runtime_error&){}
        assert(settings.sourceAttribute=="old"&&settings.outputAttribute=="old-result");
    }
    int op=99;reset(0);assert(!dommy::maya::readMathNodeOperation(data,DPMin::inDD,op));assert(op==99);
#endif
    std::cout<<"17 math entry bodies: settings/mode failures never reach processing\n";
}
template<class Node>static void rangeInputStatus(){
    Node node;DPData points;points.points={{1,2,3}};
    DCurveData curves;curves.primitives.push_back(std::make_shared<Primitive>());
    DMeshData meshes;meshes.primitives.push_back(std::make_shared<Primitive>());
    for(Payload* payload:{static_cast<Payload*>(&points),static_cast<Payload*>(&curves),static_cast<Payload*>(&meshes)}){
        outputFailures(node,*payload);
        auto data=blockFor(*payload);
        data.inputs["aSourceMax"].vector=MVector(1,1,1);
        data.inputs["aDistinationMax"].vector=MVector(1,1,1);
        for(const char* attr:{"aRange1D","aRange2D","aRange3D"})data.inputs[attr].vector=MVector(0,1,0);
        reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);assert(publications==1);
        const auto trace=operations;
        for(std::size_t at=0;at<trace.size();++at){
            if(trace[at].find("input ")!=0&&trace[at]!="input.bind"&&trace[at]!="ramp.create")continue;
            for(auto code:{MS::kFailure,MS::kInvalidParameter}){
                reset(static_cast<int>(at));injectedStatus=code;
                assert(node.compute(Node::outDD,data)==code);
                assert(publications==0&&geometryCopies==0&&callsNamed("output.create")==0);
                assert(callCount==static_cast<int>(at)+1);
                reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);
            }
        }
        data.inputs["inDD"].payload=nullptr;reset();
        assert(node.compute(Node::outDD,data)==MS::kFailure);
        assert(publications==0&&geometryCopies==0&&callsNamed("output.create")==0);
        data.inputs["inDD"].payload=payload;
        if(payload->type()==22)points.topology=false;
        if(payload->type()==23)curves.topology=false;
        if(payload->type()==24)meshes.topology=false;
        reset();assert(node.compute(Node::outDD,data)==MS::kInvalidParameter);
        assert(publications==0&&geometryCopies==0&&callsNamed("output.create")==0);
        points.topology=curves.topology=meshes.topology=true;
        data.inputs["inDD"].type=99;reset();
        assert(node.compute(Node::outDD,data)==MS::kInvalidParameter);
        assert(publications==0&&callsNamed("output.create")==0);
        data.inputs["inDD"].type=payload->type();
        // Valid empty typed payloads still publish an empty result.
        points.points.clear();curves.primitives.clear();meshes.primitives.clear();
        reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);assert(publications==1);
        reset();assert(node.compute(Node::outDataArray,data)==MS::kSuccess);assert(mathCalls==1);
        reset();assert(node.compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);
        assert(operations.empty()&&publications==0);
    }
}
template<class Values,class Original>static void compareRangeValues(const Values& values,
                                                                 bool fit,const Original& original){
    using Components=dommy::numeric::ValueComponents<typename Original::value_type>;
    using Actual=dommy::numeric::ValueComponents<typename Values::value_type>;
    assert(values.size()==1U);
    for(unsigned int axis=0U;axis<Components::count;++axis){
        const double source=Components::get(original[0],axis);
        assert(std::abs(Actual::get(values[0],axis)-(fit?2.0+2.0*source:source))<1e-6);
    }
}
template<class Values>static void checkedRangeValues(const DPAttributeData::ConstPtr& attribute,
                                                    bool fit,const Values& original){
    using Components=dommy::numeric::ValueComponents<typename Values::value_type>;
    DMathScalarDataList scalar;DMathVec2DataList vec2;DMathVec3DataList vec3;
    assert(attribute);DPData owner;owner.attributes["value"]=attribute->copy();
    if(Components::count==1U){assert(dommy::attribute::readNumericAttributeValues(owner,"value",1U,scalar));compareRangeValues(scalar,fit,original);}
    else if(Components::count==2U){assert(dommy::attribute::readNumericAttributeValues(owner,"value",1U,vec2));compareRangeValues(vec2,fit,original);}
    else{assert(dommy::attribute::readNumericAttributeValues(owner,"value",1U,vec3));compareRangeValues(vec3,fit,original);}
}
template<class Node,class T>static void rangeAttributePaths(bool fit,const std::vector<T>& values){
    Node node;
    for(bool primitivePoint:{false,true})for(bool create:{false,true}){
        DPData points;points.points={{1,2,3}};
        DCurveData curves;curves.primitives.push_back(std::make_shared<Primitive>());
        DMeshData meshes;meshes.primitives.push_back(std::make_shared<Primitive>());
        points.attributes["velocity"]=std::make_shared<TypedDPAttributeData<T>>(values);
        if(primitivePoint){
            curves.primitives[0]->attributes["velocity"]=points.attributes["velocity"]->copy();
            meshes.primitives[0]->attributes["velocity"]=points.attributes["velocity"]->copy();
        }else{
            curves.attributes["velocity"]=points.attributes["velocity"]->copy();
            meshes.attributes["velocity"]=points.attributes["velocity"]->copy();
        }
        for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
            auto data=blockFor(*payload);
            data.inputs["aSourceMax"].vector=MVector(1,1,1);
            data.inputs["aDistinationMin"].vector=MVector(2,2,2);
            data.inputs["aDistinationMax"].vector=MVector(4,4,4);
            for(const char* attr:{"aRange1D","aRange2D","aRange3D"})data.inputs[attr].vector=MVector(0,1,0);
            data.inputs["aIsOutAttribute"].number=create;data.inputs["aIsDeleteOriginal"].number=create;
            reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);assert(publications==1);
            const auto trace=operations;
            const std::string outputName=create?"result":"velocity";
            if(payload->type()==22)checkedRangeValues(outputPoints.getAttr(outputName),fit,values);
            if(payload->type()==23)checkedRangeValues(primitivePoint?outputCurves.primitives[0]->getAttr(outputName):outputCurves.getAttr(outputName),fit,values);
            if(payload->type()==24)checkedRangeValues(primitivePoint?outputMeshes.primitives[0]->getAttr(outputName):outputMeshes.getAttr(outputName),fit,values);
            // The staged output must never change Maya-owned input attributes.
            checkedRangeValues(points.getAttr("velocity"),false,values);
            if(primitivePoint){
                checkedRangeValues(curves.primitives[0]->getAttr("velocity"),false,values);
                checkedRangeValues(meshes.primitives[0]->getAttr("velocity"),false,values);
            }else{
                checkedRangeValues(curves.getAttr("velocity"),false,values);
                checkedRangeValues(meshes.getAttr("velocity"),false,values);
            }
            for(std::size_t at=0;at<trace.size();++at)if(trace[at]=="ramp.sample"){
                reset(static_cast<int>(at));
                assert(node.compute(Node::outDD,data)==MS::kInvalidParameter);assert(publications==0);
                reset();assert(node.compute(Node::outDD,data)==MS::kSuccess);
            }
        }
    }
}
static void rangeInputs(){
    rangeInputStatus<DPFitRange>();rangeInputStatus<DPRemap>();
    rangeAttributePaths<DPFitRange>(true,std::vector<double>{0.25});
    rangeAttributePaths<DPFitRange>(true,std::vector<Imath::V2d>{{0.25,0.5}});
    rangeAttributePaths<DPFitRange>(true,std::vector<Imath::V3d>{{0.25,0.5,0.75}});
    rangeAttributePaths<DPRemap>(false,std::vector<double>{0.25});
    rangeAttributePaths<DPRemap>(false,std::vector<Imath::V2d>{{0.25,0.5}});
    rangeAttributePaths<DPRemap>(false,std::vector<Imath::V3d>{{0.25,0.5,0.75}});
    std::cout<<"Fit/remap full computes: typed admission status, topology, empty results and retry\n";
}
static MDataBlock uvBlock(const Payload& payload,MFnData::Type objectType){
    auto data=blockFor(payload);data.inputs["inObject"].text="geometry";
    data.inputs["inObject"].type=objectType;data.inputs["uvName"].text="uv";return data;
}
static void checkedUv(const DPAttributeData::ConstPtr& attribute,MFnData::Type type){
    assert(attribute&&attribute->attrSize()==1U);
    if(type==MFnData::kNurbsCurve){
        const auto values=std::dynamic_pointer_cast<const DPFloatAttr>(attribute);
        assert(values&&values->value()[0]==0.25f);
    }else{
        const auto values=std::dynamic_pointer_cast<const DPVec2fAttr>(attribute);
        assert(values&&values->value()[0][0]==0.25f&&values->value()[0][1]==0.5f);
    }
}
static void uvCoordinates(){
    struct Case {MMatrix matrix; std::array<double,3> expected;};
    std::vector<Case> cases;
    MMatrix matrix;matrix[3][0]=10;cases.push_back({matrix,{{10.25,.5,.75}}});
    matrix=MMatrix();matrix[0][0]=2;matrix[1][1]=3;matrix[2][2]=4;
    cases.push_back({matrix,{{.5,1.5,3}}});
    matrix=MMatrix();matrix[0][0]=0;matrix[0][1]=1;matrix[1][0]=-1;matrix[1][1]=0;
    cases.push_back({matrix,{{-.5,.25,.75}}});
    matrix=MMatrix();matrix[1][0]=2;cases.push_back({matrix,{{1.25,.5,.75}}});
    matrix=MMatrix();matrix[0][0]=-1;cases.push_back({matrix,{{-.25,.5,.75}}});
    matrix=MMatrix();matrix[0][0]=0;cases.push_back({matrix,{{0,.5,.75}}});
    DPUVGen node;
    for(auto type:{MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}){
        for(const auto& value:cases){
            DCurveData curves;DMeshData meshes;
            for(auto* list:{&curves.primitives,&meshes.primitives}){
                auto primitive=std::make_shared<Primitive>();primitive->points={{.25f,.5f,.75f}};
                primitive->matrix=value.matrix;list->push_back(primitive);
                auto other=std::make_shared<Primitive>();other->points=primitive->points;
                other->matrix[3][1]=2;list->push_back(other);
            }
            for(const Payload* payload:{static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
                auto data=uvBlock(*payload,type);reset();
                assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess&&uvQueryPoints.size()==2);
                assert(uvQueryPoints[0]==value.expected);
                assert((uvQueryPoints[1]==std::array<double,3>{{.25,2.5,.75}}));
                const auto& output=payload->type()==23?outputCurves.primitives:outputMeshes.primitives;
                assert(output[0]->points[0][0]==.25f);
                for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                    assert(output[0]->matrix[r][c]==value.matrix[r][c]);
            }
        }
        DPData empty;empty.attributes["uv"]=std::make_shared<DPIntAttr>(std::vector<int>{},9);
        auto data=uvBlock(empty,type);reset();
        assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess&&uvQueryPoints.empty());
        const auto retained=std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("uv"));
        assert(retained&&retained->value().empty()&&retained->getDefaultValue()==9);
        assert(empty.getAttr("uv")&&retained!=empty.getAttr("uv"));
    }
    for(int bad=0;bad<3;++bad){
        DCurveData curves;auto primitive=std::make_shared<Primitive>();curves.primitives.push_back(primitive);
        if(bad==0)primitive->matrix[0][0]=NAN;
        if(bad==1)primitive->matrix[3][0]=INFINITY;
        if(bad==2)primitive->matrix[0][3]=1;
        auto data=uvBlock(curves,MFnData::kNurbsSurface);reset();
        assert(node.compute(DPUVGen::outDD,data)==MS::kInvalidParameter);
        assert(uvQueryPoints.empty()&&publications==0&&geometryCopies==0);
    }
    for(auto type:{MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}){
        DPData points;DCurveData curves;DMeshData meshes;
        points.points={{.25f,.5f,.75f}};
        curves.primitives.push_back(std::make_shared<Primitive>());
        meshes.primitives.push_back(std::make_shared<Primitive>());
        for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
            auto data=uvBlock(*payload,type);data.inputs["uvName"].text="rejectedUV";
            reset();rejectUvField=true;
            assert(node.compute(DPUVGen::outDD,data)==MS::kFailure&&publications==0);
            reset();assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess);
        }
    }
    std::cout<<"UV query coordinates: six transform cases, per-primitive frames, source isolation and empty-field preservation\n";
}
static void uvBudget(){
    DPData points;points.points.assign(16,DPoint(.25f,.5f,.75f));
    DCurveData curves;DMeshData meshes;
    curves.primitives.push_back(std::make_shared<Primitive>());
    meshes.primitives.push_back(std::make_shared<Primitive>());
    for(auto* primitive:{curves.primitives[0].get(),meshes.primitives[0].get()})primitive->points=points.points;
    auto field=std::make_shared<DPMat4Attr>();field->incrementDefaultValue(16);
    points.attributes["retained"]=field;
    curves.primitives[0]->attributes["retained"]=field->copy();
    meshes.primitives[0]->attributes["retained"]=field->copy();
    DPUVGen node;
    for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=uvBlock(*payload,MFnData::kNurbsSurface);reset();
        assert(node.compute(DPUVGen::outDD,data)==MS::kFailure);
        assert(geometryCopies==0&&publications==0&&uvQueryPoints.empty());
        assert(callsNamed("output.create")==0);
    }
    points.attributes.clear();curves.primitives[0]->attributes.clear();meshes.primitives[0]->attributes.clear();
    for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=uvBlock(*payload,MFnData::kNurbsSurface);reset();
        assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess&&publications==1);
    }
    for(int scope=0;scope<4;++scope){
        auto& primitive=*meshes.primitives[0];primitive.componentAttributes.clear();primitive.componentGroups.clear();
        if(scope<2){
            primitive.faces=16;primitive.connections=48;
            auto values=std::make_shared<DPMat4Attr>();values->incrementDefaultValue(scope==0?16:48);
            primitive.componentAttributes[scope==0?D_COMP_FACE:D_COMP_PER_VTX]["retained"]=values;
        }else{
            primitive.faces=1;primitive.connections=3;
            auto group=std::make_shared<Group>();group->ids.assign(scope==2?1:3,true);
            primitive.componentGroups[scope==2?D_COMP_FACE:D_COMP_EDGE][std::string(3000,'g')]=group;
        }
        auto data=uvBlock(meshes,MFnData::kNurbsSurface);reset();
        assert(node.compute(DPUVGen::outDD,data)==MS::kFailure&&geometryCopies==0&&publications==0);
        assert(uvQueryPoints.empty());
        primitive.componentAttributes.clear();primitive.componentGroups.clear();reset();
        assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess);
    }
    std::cout<<"UV aggregate metadata/workspace budget: rejection before copying/querying and successful retry\n";
}
static void uvInputs(){
    DPUVGen node;
    for(auto objectType:{MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}){
        DPData points;points.points={{0.25f,0.5f,0.75f}};
        DCurveData curves;curves.primitives.push_back(std::make_shared<Primitive>());
        DMeshData meshes;meshes.primitives.push_back(std::make_shared<Primitive>());
        curves.primitives[0]->points=meshes.primitives[0]->points=points.points;
        points.attributes["uv"]=std::make_shared<DPIntAttr>(std::vector<int>{9});
        curves.primitives[0]->attributes["uv"]=points.attributes["uv"]->copy();
        meshes.primitives[0]->attributes["uv"]=points.attributes["uv"]->copy();
        for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
            auto data=uvBlock(*payload,objectType);reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess&&publications==1);
            const auto trace=operations;
            if(payload->type()==22)checkedUv(outputPoints.getAttr("uv"),objectType);
            if(payload->type()==23)checkedUv(outputCurves.primitives[0]->getAttr("uv"),objectType);
            if(payload->type()==24)checkedUv(outputMeshes.primitives[0]->getAttr("uv"),objectType);
            for(std::size_t at=0;at<trace.size();++at){
                const auto& op=trace[at];
                if(op.find("input ")==0||op=="input.bind"||op=="factory.bind"||op=="output.create"||
                   op=="output.handle"||op=="output.publish"||op.find("uv.")==0){
                    for(auto code:{MS::kFailure,MS::kInvalidParameter}){
                        reset(static_cast<int>(at));injectedStatus=code;
                        assert(node.compute(DPUVGen::outDD,data)==code);assert(publications==0);
                        assert(callCount==static_cast<int>(at)+1);
                        if(op.find("input ")==0||op=="input.bind")
                            assert(geometryCopies==0&&callsNamed("output.create")==0);
                        reset();assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess);
                    }
                }
                if(op=="handle.clean"||op=="output.clean")continue;
                reset();attributeThrowAt=static_cast<int>(at);
                assert(node.compute(DPUVGen::outDD,data)==MS::kFailure&&publications==0);
            }
            reset();nonfiniteUv=true;
            assert(node.compute(DPUVGen::outDD,data)==MS::kInvalidParameter&&publications==0);
            reset();nullCreated=true;
            assert(node.compute(DPUVGen::outDD,data)==MS::kFailure&&publications==0);
            data.inputs["inDD"].payload=nullptr;reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kFailure&&publications==0&&geometryCopies==0);
            data.inputs["inDD"].payload=payload;
            points.topology=curves.topology=meshes.topology=false;reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kInvalidParameter&&publications==0&&geometryCopies==0);
            points.topology=curves.topology=meshes.topology=true;
            data.inputs["inDD"].type=99;reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kInvalidParameter&&publications==0);
            data.inputs["inDD"].type=payload->type();data.inputs["inObject"].text="null";reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kInvalidParameter&&publications==0);
            data.inputs["inObject"].text="geometry";data.inputs["uvName"].text="";reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kInvalidParameter&&publications==0&&geometryCopies==0);
            data.inputs["uvName"].text="uv";data.inputs["inObject"].type=MFnData::kAny;reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kFailure&&publications==0&&geometryCopies==0);
            reset();assert(node.compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter&&callCount==0);
        }
        // Point/primitive cleanup and replacement never alter input attributes.
        assert(std::dynamic_pointer_cast<const DPIntAttr>(points.getAttr("uv"))->value()[0]==9);
        assert(std::dynamic_pointer_cast<const DPIntAttr>(curves.primitives[0]->getAttr("uv"))->value()[0]==9);
        assert(std::dynamic_pointer_cast<const DPIntAttr>(meshes.primitives[0]->getAttr("uv"))->value()[0]==9);
        points=DPData();curves=DCurveData();meshes=DMeshData();
        for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
            auto data=uvBlock(*payload,objectType);reset();
            assert(node.compute(DPUVGen::outDD,data)==MS::kSuccess&&publications==1);
            assert(std::none_of(operations.begin(),operations.end(),[](const std::string& op){return op.find("uv.")==0;}));
        }
    }
    std::cout<<"UV full compute/helpers: DP/DC/DM, three geometry types, checked failures, empty data and retry\n";
}
static void sharedMathInputs(){
    DPData points;points.points={{1,2,3}};
    DCurveData curves;curves.primitives.push_back(std::make_shared<Primitive>());
    DMeshData meshes;meshes.primitives.push_back(std::make_shared<Primitive>());
    for(bool single:{false,true})for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=blockFor(*payload);
        auto invoke=[&](){
            if(single)return shared_math_body::processDDataMathOutputSingle(
                {"inDD"},{"outDD"},"node","velocity","result",false,false,data,MPlug({"outDD"}),D_SRC_NEGATIVE);
            return shared_math_body::processDDataMathOutput(
                {"inDD"},{"outDD"},"node","velocity","result",false,false,
                {"constants"},{"targets"},{"attributes"},data,MPlug({"outDD"}),D_CAL_ADD);
        };
        reset();assert(invoke()==MS::kSuccess&&publications==1&&geometryCopies==1&&mathCalls==1);
        const auto trace=operations;
        for(std::size_t at=0;at<trace.size();++at){
            const auto& op=trace[at];
            if(op.find("input ")==0||op=="input.bind"||op=="output.create"||op=="primitive.copy"||
               op=="math.process"||op=="output.handle"||op=="output.publish"){
                for(auto code:{MS::kFailure,MS::kInvalidParameter}){
                    reset(static_cast<int>(at));injectedStatus=code;
                    assert(invoke()==code&&publications==0&&callCount==static_cast<int>(at)+1);
                    if(op.find("input ")==0||op=="input.bind")
                        assert(geometryCopies==0&&mathCalls==0&&callsNamed("output.create")==0);
                    reset();assert(invoke()==MS::kSuccess);
                }
            }
            if(op=="handle.clean"||op=="output.clean")continue;
            reset();attributeThrowAt=static_cast<int>(at);
            try{(void)invoke();assert(false);}catch(const std::runtime_error&){}
            assert(publications==0);
        }
        reset();shared_math_body::corruptOutput=true;
        assert(invoke()==MS::kFailure&&publications==0&&callsNamed("output.handle")==0);
        data.inputs["inDD"].payload=nullptr;reset();assert(invoke()==MS::kFailure&&publications==0);
        data.inputs["inDD"].payload=payload;points.topology=curves.topology=meshes.topology=false;
        reset();assert(invoke()==MS::kInvalidParameter&&publications==0&&geometryCopies==0);
        points.topology=curves.topology=meshes.topology=true;
        data.inputs["inDD"].type=99;reset();assert(invoke()==MS::kInvalidParameter&&publications==0);
        data.inputs["inDD"].type=payload->type();data.inputs["inDD"].text="null";reset();
        assert(invoke()==MS::kInvalidParameter&&publications==0);
        data.inputs["inDD"].text="weight";reset();nullCreated=true;assert(invoke()==MS::kFailure&&publications==0);
        reset();assert(invoke()==MS::kSuccess&&publications==1);
    }
    points=DPData();curves=DCurveData();meshes=DMeshData();
    for(bool single:{false,true})for(const Payload* payload:{static_cast<const Payload*>(&points),static_cast<const Payload*>(&curves),static_cast<const Payload*>(&meshes)}){
        auto data=blockFor(*payload);reset();
        const MStatus status=single?shared_math_body::processDDataMathOutputSingle(
            {"inDD"},{"outDD"},"node","velocity","result",false,false,data,MPlug({"outDD"}),D_SRC_NEGATIVE):
            shared_math_body::processDDataMathOutput({"inDD"},{"outDD"},"node","velocity","result",false,false,
                {"constants"},{"targets"},{"attributes"},data,MPlug({"outDD"}),D_CAL_ADD);
        assert(status==MS::kSuccess&&publications==1);
        assert(mathCalls==(payload->type()==22?1:0));
    }
    auto data=blockFor(points);reset();
    assert(shared_math_body::processDDataMathOutputSingle({"inDD"},{"outDD"},"node","velocity","result",false,false,
        data,MPlug({"outDD"}),D_SRC_COUNT)==MS::kInvalidParameter&&operations.empty());
    assert(shared_math_body::processDDataMathOutput({"inDD"},{"outDD"},"node","velocity","result",false,false,
        {"constants"},{"targets"},{"attributes"},data,MPlug({"outDD"}),D_CAL_COUNT)==MS::kInvalidParameter&&operations.empty());
    std::cout<<"Two complete shared math output handlers: typed status, staged copy, processing/validation/publication failures and empty results\n";
}
static MDataBlock duplicatorData(){
    MDataBlock data;data.inputs["inDP"].payload=&inputPoints;data.inputs["inDP"].type=22;
    data.elements.resize(2);data.elements[0].text="mesh1";data.elements[1].text="mesh2";
    return data;
}
static void duplicator(){
    DPMeshDuplicator node;auto data=duplicatorData();
    data.inputs["aLocalTranslate"].vector=MVector(2,3,4);data.inputs["aUniformScale"].number=2;
    data.inputs["aAnimMatrixAttribute"].text="anim";
    DTransformParams params;params.mExpressionStr="preserved";
    reset();assert(node.evalAnimMatrixParms(data,params)==MS::kSuccess);
    const int reads=callCount;assert(reads>=30);
    assert(params.mLocalTranslate[0]==2&&params.mUniformScale==2&&params.mAnimationAttribute=="anim");
    assert(params.mExpressionStr=="preserved");
    for(int at=0;at<reads;++at)for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        DTransformParams unchanged;unchanged.mExpressionStr="preserved";reset(at);injectedStatus=code;
        assert(node.evalAnimMatrixParms(data,unchanged)==code);assert(callCount==at+1);
        assert(unchanged.mAnimationAttribute.empty()&&unchanged.mLocalTranslate[0]==0&&unchanged.mUniformScale==1);
        assert(unchanged.mExpressionStr=="preserved");
    }
    reset();assert(node.compute(DPMeshDuplicator::outDM,data)==MS::kSuccess);const auto trace=operations;
    for(std::size_t at=0;at<trace.size();++at){
        if(trace[at].find("input ")!=0&&trace[at]!="string.split"&&trace[at].find("array.")!=0)continue;
        if(trace[at]=="input inDP")continue; // Existing typed helper maps a missing payload to invalid.
        reset(static_cast<int>(at));assert(node.compute(DPMeshDuplicator::outDM,data)==MS::kFailure);
        assert(callCount==static_cast<int>(at)+1&&publications==0);
    }
    reset();data.inputs["aAnimationType"].number=2;params=DTransformParams();
    assert(node.evalAnimMatrixParms(data,params));assert(params.mAnimMatrixType==MAT_ANIM_NONE);
    data.inputs["aInAnimMatrix"].type=MFnData::kMatrixArray;
    data.inputs["aInAnimMatrix"].text="block-matrices";
    reset();connectedPlugs.insert("aInAnimMatrix");assert(node.evalAnimMatrixParms(data,params));
    assert(params.mAnimMatrixType==MAT_ANIM_INPUT_DATA);
    assert(params.mAnimationInputIsData&&params.mAnim_Mat_InputData_Object.name=="block-matrices");
    const auto connectedTrace=operations;
    for(std::size_t at=0;at<connectedTrace.size();++at)for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        DTransformParams unchanged;reset(static_cast<int>(at));connectedPlugs.insert("aInAnimMatrix");injectedStatus=code;
        assert(node.evalAnimMatrixParms(data,unchanged)==code);
        assert(unchanged.mAnimMatrixType==MAT_ANIM_NONE&&!unchanged.mAnimationInputIsData);
    }
    data.inputs["aInAnimMatrix"].type=MFnData::kFloatArray;reset();connectedPlugs.insert("aInAnimMatrix");
    DTransformParams unchanged;
    assert(node.evalAnimMatrixParms(data,unchanged)==MS::kInvalidParameter);
    assert(!unchanged.mAnimationInputIsData);
    data.inputs["aInAnimMatrix"].type=MFnData::kMatrixArray;
    data.inputs["aInAnimMatrix"].text="null";reset();connectedPlugs.insert("aInAnimMatrix");
    assert(node.evalAnimMatrixParms(data,unchanged)==MS::kInvalidParameter);
    data.inputs["aInAnimMatrix"].text="block-matrices";
    reset(2);params=DTransformParams();assert(node.evalAnimMatrixParms(data,params)==MS::kFailure);
    assert(params.mAnimMatrixType==MAT_ANIM_NONE&&params.mAnimationAttribute.empty());
    reset();assert(node.compute(MPlug(MObject{"other"}),data)==MS::kUnknownParameter);assert(callCount==0);
    std::cout<<"Duplicator: transactional real parameters and checked scalar/array admission prefix\n";
}
static void duplicatorNameBudget(){
    auto data=duplicatorData();DPMeshDuplicator node;
    data.inputs["aTransferAttributes"].text=std::string(8192U,'x');
    reset();assert(node.compute(DPMeshDuplicator::outDM,data)==MS::kFailure);
    assert(meshConstructions==0&&publications==0);
    assert(std::find(operations.begin(),operations.end(),"string.split")==operations.end());
    data.inputs["aTransferAttributes"].text.clear();reset();
    assert(node.compute(DPMeshDuplicator::outDM,data)==MS::kSuccess);
    assert(meshConstructions==2);
    std::cout<<"Duplicator: raw transfer names admitted before split/import, no split/import on rejection and retry passed\n";
}
int main(int argc,char** argv){
    const std::string part=argc>1?argv[1]:"all";
    if(part=="all"||part=="outputs")genericOutputs();
    if(part=="all"||part=="normalize")normalization();
    if(part=="all"||part=="math")mathSettings();
    if(part=="all"||part=="ranges")rangeInputs();
    if(part=="all"||part=="uv"){uvInputs();uvCoordinates();}
    if(part=="uv-budget")uvBudget();
    if(part=="all"||part=="shared")sharedMathInputs();
    if(part=="all"||part=="duplicator")duplicator();
    if(part=="duplicator-budget")duplicatorNameBudget();
}
