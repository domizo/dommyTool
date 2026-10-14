#include "arnold_export_host.h"
#include "arnold_parameter_doubles.h"
#include "light_transform_methods.inc"
#include <cfenv>
#include "integrations/arnold/DArnoldAssWrite.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldSession.h"
#include "common/utility/DStatusOperation.h"
#include "common/file/DExportPath.h"
#include "DCurveArnoldLayout.h"
#include "arnold_export_classes.inc"

const MTypeId DPData::id = 42;
const MTypeId DCurveData::id = 43;
DPData inputPoint;
DCurveData inputCurve;

struct AtArray { std::size_t size; };
struct AtNode { std::vector<AtArray*> arrays; };
void AiBegin() { api("session.begin"); ++sessions; }
void AiEnd() { assert(sessions == 1); --sessions; }
int AiASSWrite(const char*, int mask, bool open, bool binary) {
    api("ass.write"); assert(mask == AI_NODE_ALL && !open);
    ++writes; lastBinary = binary; return writeResult;
}
void AiMsgError(const char*, ...) { api("arnold.error"); }
void AiMsgSetLogFileName(const char*) { api("arnold.log"); }
void AiMsgSetConsoleFlags(int) { api("arnold.flags"); }
AtNode* AiNode(const char*) { api("node.create"); ++liveNodes; return new AtNode; }
void AiArrayDestroy(AtArray* array) { --liveArrays; delete array; }
bool AiNodeDestroy(AtNode* node) {
    for (auto* array : node->arrays) AiArrayDestroy(array);
    parameterFixture::users.erase(node);
    --liveNodes; delete node;
    return true;
}
void AiNodeIteratorDestroy(AtNodeIterator*) {}
void AiUserParamIteratorDestroy(AtUserParamIterator*) {}
AtArray* AiArrayAllocate(std::uint32_t count, std::uint8_t keys, int) {
    api("array.allocate"); ++liveArrays; return new AtArray{std::size_t(count)*keys};
}
void* AiArrayMap(AtArray* array) { return array; }
void AiArrayUnmap(AtArray*) {}
void AiNodeSetArray(AtNode* node, const char*, AtArray* array) { node->arrays.push_back(array); }
void AiNodeSetStr(AtNode*, const char*, const char*) { api("node.string"); }
void AiNodeSetInt(AtNode*, const char*, int) { api("node.int"); }
void AiNodeSetBool(AtNode*, const char*, bool) { api("node.bool"); }
void AiNodeSetByte(AtNode*, const char*, unsigned char) { api("node.byte"); }
void AiNodeSetFlt(AtNode*, const char*, float value) { api("node.float"); number(value); }
void AiNodeSetVec2(AtNode*, const char*, float a, float b) { api("node.vec2"); number(a); number(b); }
void AiNodeSetVec(AtNode*, const char*, float a, float b, float c) { api("node.vec"); number(a); number(b); number(c); }
void AiNodeSetRGB(AtNode*, const char*, float a, float b, float c) { api("node.rgb"); number(a); number(b); number(c); }
void AiNodeSetRGBA(AtNode*, const char*, float a, float b, float c, float d) { api("node.rgba"); number(a); number(b); number(c); number(d); }
void AiNodeSetMatrix(AtNode*, const char*, const AtMatrix& value) {
    api("node.matrix"); for (const auto& row : value.data) for (float item : row) number(item);
}
bool AiNodeDeclare(AtNode* node, const char* name, const char* declaration) {
    return parameterFixture::declare(node,name,declaration);
}
bool failLink = false;
int failLinkAt = -1;
unsigned int linkCalls = 0U;
bool AiNodeLink(AtNode*, const char*, AtNode*) {
    api("node.link"); ++linkCalls;
    return !failLink && int(linkCalls)-1 != failLinkAt;
}
int failElementAt=-1,elementCalls=0;
bool elementWrite(const char* name){api(name);return elementCalls++!=failElementAt;}
bool AiArraySetInt(AtArray* array, std::size_t i, int) { if(!elementWrite("array.int"))return false;assert(i < array->size);return true; }
bool AiArraySetFlt(AtArray* array, std::size_t i, float value) { if(!elementWrite("array.float"))return false;assert(i < array->size); number(value);return true; }
bool AiArraySetVec(AtArray* array, std::size_t i, AtVector v) { if(!elementWrite("array.vec"))return false;assert(i < array->size); number(v.x); number(v.y); number(v.z);return true; }
bool AiArraySetVec2(AtArray* array, std::size_t i, AtVector2 v) { if(!elementWrite("array.vec2"))return false;assert(i < array->size); number(v.x); number(v.y);return true; }
bool AiArraySetPtr(AtArray* array, std::size_t i, void*) { if(!elementWrite("array.ptr"))return false;assert(i < array->size);return true; }

const DPData* getInputDP(const MObject&, MDataBlock&) { api("point.input"); return validInput ? &inputPoint : nullptr; }
const DCurveData* getInputDC(const MObject&, MDataBlock&) { api("curve.input"); return validInput ? &inputCurve : nullptr; }
const DPData* getInputDPfromPlug(const MPlug&, MObject&) { api("plug.data"); return validInput ? &inputPoint : nullptr; }
MBoundingBox DPData::getBoundingBox() const { api("point.bounds"); return {{-2,-3,-4},{5,6,7}}; }
bool getSequenceFileName(const MString& source, double, MString& result) { result = source; return pathValid; }
void EchoWarning(const char*, const char*) { api("warning"); }
void EchoWarning(const std::string&, const char*) { api("warning"); }
void EchoInfo(const char*, const char*) { api("info"); }
namespace dommy { namespace maya {
MStatus getEvaluationTime(MDataBlock&, MTime&) { return apiStatus("time.read"); }
template <> MStatus getPluginDataFromHandle(const MDataHandle&, MTypeId id,
                                           const DPData*& output) {
    output = nullptr;
    const MStatus status = apiStatus("point.read");
    if (!status) return status;
    if (id != DPData::id || !validInput) return MS::kInvalidParameter;
    output = &inputPoint;
    return MS::kSuccess;
}
template <> MStatus getPluginDataFromHandle(const MDataHandle&, MTypeId id,
                                           const DCurveData*& output) {
    output = nullptr;
    const MStatus status = apiStatus("curve.read");
    if (!status) return status;
    if (id != DCurveData::id || !validInput) return MS::kInvalidParameter;
    output = &inputCurve;
    return MS::kSuccess;
}
template <> MStatus getPluginData(const MFnPluginData& owner, DPData*& result) { result = owner.point.get(); return MS::kSuccess; }
template <> MStatus getPluginData(const MFnPluginData& owner, DCurveData*& result) { result = owner.curve.get(); return MS::kSuccess; }
template <typename T> MStatus publishPluginDataObject(MDataHandle&, const MObject&, const T&) {
    const MStatus status = apiStatus("data.publish"); if (status) ++publications; return status;
}
template <typename T> MStatus publishTypedPluginDataObject(MDataHandle&, const MObject&, MTypeId) {
    const MStatus status = apiStatus("data.publish"); if (status) ++publications; return status;
}
}}
void getMDoubleArrayFromAttribute(const DPData& owner, const std::string& name, MDoubleArray& result) {
    if (owner.getAttrTypeName(name) == "double") result.values = owner.attrListValue<double>(name);
    else if (owner.getAttrTypeName(name) == "float") { auto src = owner.attrListValue<float>(name); result.values.assign(src.begin(),src.end()); }
    else if (owner.getAttrTypeName(name) == "int") { auto src = owner.attrListValue<int32_t>(name); result.values.assign(src.begin(),src.end()); }
    else if (owner.getAttrTypeName(name) == "uint32") { auto src = owner.attrListValue<uint32_t>(name); result.values.assign(src.begin(),src.end()); }
    else if (owner.getAttrTypeName(name) == "int64") { auto src = owner.attrListValue<int64_t>(name); result.values.assign(src.begin(),src.end()); }
}
void getMIntArrayFromAttribute(const DPData& owner, const std::string& name, MIntArray& result) { result.values = owner.attrListValue<int>(name); }
void getMVectorArrayFromAttribute(const DPData& owner, const std::string& name, MVectorArray& result) {
    if (owner.getAttrTypeName(name) == "vec3d") {
        result.values.clear();
        for (const auto& v : owner.attrListValue<Imath::V3d>(name)) result.values.push_back({v[0],v[1],v[2]});
    } else if (owner.getAttrTypeName(name) == "vec3f") {
        result.values.clear();
        for (const auto& v : owner.attrListValue<Imath::V3f>(name)) result.values.push_back({v[0],v[1],v[2]});
    } else if (owner.getAttrTypeName(name) == "vec3i") {
        result.values.clear();
        for (const auto& v : owner.attrListValue<Imath::V3i>(name)) result.values.push_back(MVector(v[0],v[1],v[2]));
    }
}
void getMVector2DArrayFromAttribute(const DPData& owner, const std::string& name, MVectorArray& result) {
    result.values.clear();
    for (const auto& v : owner.attrListValue<Imath::V2d>(name)) result.values.push_back({v[0],v[1],0});
}
void getMMatrixArrayFromAttribute(const DPData& owner, const std::string& name, MMatrixArray& result) {
    if(owner.getAttrTypeName(name)=="matrix")result.values = owner.attrListValue<MMatrix>(name);
}

#include "arnold_export_methods.inc"

void reset()
{
    assert(sessions == 0 && liveNodes == 0 && liveArrays == 0);
    failAttribute = failOperation = throwOperation = -1;
    injectedApiStatus=MS::kInvalidParameter;
    reports = publications = cleanCalls = writes = exceptionKind = 0;
    writeResult = AI_SUCCESS; connected = validInput = pathValid = true;
    failLink = false; failLinkAt = -1; linkCalls = 0U;
    failElementAt=-1;elementCalls=0;
    fileExists = false; outputInfo.clear(); operations.clear(); numericSubmissions.clear();
    inputPoint = DPData(); inputCurve = DCurveData();
    parameterFixture::reset();
}
MDataBlock lightData()
{
    MDataBlock data;
    data.values[DPLightGen::aFilePath.id].string = MString("/tmp/output.ass");
    data.values[DPLightGen::aAspectRatio.id].scalar = 1;
    data.values[DPLightGen::aIntensity.id].scalar = 2;
    data.values[DPLightGen::aColor.id].components[0] = 2;
    data.values[DPLightGen::aColor.id].components[1] = -1;
    data.values[DPLightGen::aColor.id].components[2] = 3;
    data.values[DPLightGen::aBaseDirection.id].components[2] = -1;
    for (uint i = 0; i < 3; ++i) data.values[DPLightGen::aGlobalScale.id].components[i] = 1;
    return data;
}
MDataBlock pointData()
{
    MDataBlock data;
    data.values[DPWriteToAi::aFilePath.id].string = MString("/tmp/output.ass");
    data.values[DPWriteToAi::aConstantRadius.id].scalar = 0.25;
    return data;
}
MDataBlock curveData()
{
    MDataBlock data;
    data.values[DWriteDCurveToAi::aFilePath.id].string = MString("/tmp/output.ass");
    data.values[DWriteDCurveToAi::aConstantWidth.id].scalar = 0.25;
    return data;
}
MDataBlock pointPassThroughData(){
    auto data=pointData();data.values[DPWriteToAi::aExportMode.id].scalar=2;return data;
}
MDataBlock curvePassThroughData(){
    auto data=curveData();data.values[DWriteDCurveToAi::aExportMode.id].scalar=2;return data;
}
template <typename Operation> void failsWithoutPublishing(Operation operation)
{
    assert(operation() != MS::kSuccess);
    assert(publications == 0 && cleanCalls == 0);
    assert(sessions == 0 && liveNodes == 0 && liveArrays == 0);
    assert(outputInfo.find("ASS export completed") == std::string::npos);
}
void numericHelpers()
{
    using namespace dommy::arnold;
    const double max = std::numeric_limits<float>::max();
    assert(checkedFloat(max) == float(max));
    assert(checkedFloat(-2.5) == -2.5f);
    assert(checkedFloatProduct(3,0.5) == 1.5f);
    assert(checkedScaleProduct(1e300,1e-300) > 0.99);
    assert(checkedTextureReciprocal(0) == 0);
    assert(checkedTextureReciprocal(-4) == -0.25f);
    for (double bad : {1e40, -1e40, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool caught = false;
        try { (void)checkedFloat(bad); } catch (const std::invalid_argument&) { caught = true; }
        assert(caught);
    }
    bool caught = false;
    for (double repeat : {1e-308, 1e-320, std::numeric_limits<double>::denorm_min()}) {
        std::feclearexcept(FE_ALL_EXCEPT); caught = false;
        try { (void)checkedTextureReciprocal(repeat); } catch (const std::invalid_argument&) { caught = true; }
        assert(caught && std::fetestexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW) == 0);
    }
    MMatrix matrix; float result[4][4];
    for (auto& row : result) for (float& item : row) item = 17;
    matrix[3][3] = 1e40; caught = false;
    try { checkedFloatMatrix(matrix,result); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught); for (const auto& row : result) for (float item : row) assert(item == 17);
    const auto vectors = checkedFloatVectors<Imath::V2f>(std::vector<Imath::V2d>{{2,-3}});
    assert(vectors[0][0] == 2 && vectors[0][1] == -3);
}

template <typename Node, typename Factory>
void requiredWriterInput(Node& node, MObject input, MObject output, Factory factory,
                         const char* binding) {
    for (auto code : {MS::kFailure,MS::kInvalidParameter}) {
        reset(); auto data=factory(); failAttribute=input.id; injectedApiStatus=code;
        assert(node.compute(MPlug(output),data)==code);
        assert(writes==0&&publications==0&&cleanCalls==0&&sessions==0);
        reset(); data=factory(); assert(node.compute(MPlug(output),data)==MS::kSuccess);
        const auto found=std::find(operations.begin(),operations.end(),binding);
        assert(found!=operations.end()); const int index=int(found-operations.begin());
        reset(); data=factory(); failOperation=index; injectedApiStatus=code;
        assert(node.compute(MPlug(output),data)==code&&int(operations.size())==index+1);
        assert(writes==0&&publications==0&&cleanCalls==0&&sessions==0);
    }
    reset(); auto data=factory(); validInput=false;
    assert(node.compute(MPlug(output),data)==MS::kInvalidParameter&&writes==0&&publications==0);
    reset(); data=factory(); inputPoint.topology=inputCurve.topology=false;
    assert(node.compute(MPlug(output),data)==MS::kInvalidParameter&&writes==0&&publications==0);
    reset(); data=factory(); assert(node.compute(MPlug(output),data)==MS::kSuccess);
    const auto created=std::find(operations.begin(),operations.end(),"data.create");assert(created!=operations.end());
    const int createIndex=int(created-operations.begin());
    for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        reset();data=factory();failOperation=createIndex;injectedApiStatus=code;
        assert(node.compute(MPlug(output),data)==code&&publications==0&&writes==0&&cleanCalls==0);
    }
    reset(); data=factory(); assert(node.compute(MPlug(MObject(-1)),data)==MS::kUnknownParameter);
    assert(operations.empty());
}

template <typename Node, typename Factory>
void writerFailures(Node& node, MObject output, Factory factory,
                    const std::vector<MObject>& attributes)
{
    for (const auto& attribute : attributes) {
        reset(); auto data = factory(); failAttribute = attribute.id;
        failsWithoutPublishing([&]{return node.compute(MPlug(output),data);});
        assert(writes == 0);
    }
    reset(); auto data = factory(); assert(node.compute(MPlug(output),data));
    assert(writes == 1 && publications == 1 && cleanCalls == 1);
    const auto events = operations;
    const int elementWrites=elementCalls;
    for(int at=0;at<elementWrites;++at){
        reset();auto failed=factory();failElementAt=at;
        failsWithoutPublishing([&]{return node.compute(MPlug(output),failed);});
        assert(writes==0&&elementCalls==at+1);
        reset();auto retry=factory();assert(node.compute(MPlug(output),retry));
        assert(writes==1&&publications==1&&cleanCalls==1);
    }
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i] == "file.path" || events[i] == "time.read" || events[i] == "data.create" ||
            events[i] == "output" || events[i] == "data.publish" || events[i] == "string.split") {
            reset(); auto failed = factory(); failOperation = int(i);
            failsWithoutPublishing([&]{return node.compute(MPlug(output),failed);});
            if (events[i] == "string.split") {
                assert(writes == 0 && publications == 0 && cleanCalls == 0);
                assert(std::find(operations.begin(),operations.end(),"session.begin")==operations.end());
                reset();failed=factory();failOperation=int(i);
                assert(node.compute(MPlug(output),failed)==injectedApiStatus);
            }
        }
        for (int kind = 0; kind < 2; ++kind) {
            reset(); auto failed = factory(); throwOperation = int(i); exceptionKind = kind;
            assert(node.compute(MPlug(output),failed) != MS::kSuccess);
            assert(reports == 1 && sessions == 0 && liveNodes == 0 && liveArrays == 0);
        }
    }
}

int main()
{
    numericHelpers();
    DPLightGen light; DPWriteToAi point; DWriteDCurveToAi curve;
    requiredWriterInput(point,DPWriteToAi::inDP,DPWriteToAi::outDP,pointPassThroughData,"point.read");
    requiredWriterInput(curve,DWriteDCurveToAi::inDC,DWriteDCurveToAi::outDC,curvePassThroughData,"curve.read");
    reset();auto inputBaseline=lightData();
    assert(light.compute(MPlug(DPLightGen::aNodeInfo),inputBaseline));
    const auto inputEvents=operations;
    for(std::size_t at=0;at<inputEvents.size();++at){
        if(inputEvents[at]!="plug.connected" && inputEvents[at]!="point.read")continue;
        for(auto code:{MS::kFailure,MS::kInvalidParameter})for(bool connection:{false,true}){
            reset();auto data=lightData();connected=connection;failOperation=int(at);injectedApiStatus=code;
            // A disconnected node does not acquire or bind the input payload.
            if(inputEvents[at]=="point.read" && !connection)continue;
            data.values[DPLightGen::aNodeInfo.id].string="preserved";
            assert(light.compute(MPlug(DPLightGen::aNodeInfo),data)==code);
            assert(publications==0 && cleanCalls==0 && writes==0 && operations.size()==at+1U);
            assert(data.values[DPLightGen::aNodeInfo.id].string.text=="preserved");
        }
    }
    for(auto code:{MS::kFailure,MS::kInvalidParameter}){
        reset();auto data=lightData();failAttribute=DPLightGen::inDP.id;injectedApiStatus=code;
        assert(light.compute(MPlug(DPLightGen::aNodeInfo),data)==code);
        assert(publications==0 && cleanCalls==0 && writes==0);
    }
    for (int condition=0;condition<3;++condition) {
        reset();auto data=lightData();
        data.values[DPLightGen::aNodeInfo.id].string="preserved";
        if(condition==0)failAttribute=DPLightGen::inDP.id;
        if(condition==1)validInput=false;
        if(condition==2)inputPoint.topology=false;
        assert(light.compute(MPlug(DPLightGen::aNodeInfo),data)==MS::kInvalidParameter);
        assert(publications==0 && cleanCalls==0 && writes==0);
        assert(data.values[DPLightGen::aNodeInfo.id].string.text=="preserved");
        reset();data=lightData();assert(light.compute(MPlug(DPLightGen::aNodeInfo),data));
    }
    reset();auto disconnectedData=lightData();connected=false;failAttribute=DPLightGen::inDP.id;
    assert(light.compute(MPlug(DPLightGen::aNodeInfo),disconnectedData));
    assert(writes==0 && publications==1 && cleanCalls==1);
    assert(outputInfo.find("not connected")!=std::string::npos);
    assert(std::find(operations.begin(),operations.end(),"input")==operations.end());
    reset();auto emptyData=lightData();inputPoint.points.clear();
    assert(light.compute(MPlug(DPLightGen::aNodeInfo),emptyData));
    assert(publications==1 && cleanCalls==1 && outputInfo.find("Create light count: 0")!=std::string::npos);
#if DOMMY_WITH_DG_FILE_WRITERS
    for (MObject control : {DPLightGen::aIntensityAttribute,DPLightGen::aColorAttribute,
             DPLightGen::aSpotAngleAttribute,DPLightGen::aScaleAttribute,
             DPLightGen::aAnimMatrixAttribute,DPLightGen::aDirectionAttribute}) {
        reset();auto data=lightData();
        data.values[control.id].string="wrong";
        data.values[DPLightGen::aDirectionType.id].scalar=1;
        inputPoint.field<std::string>("wrong","string",{"not numeric"});
        data.values[DPLightGen::aNodeInfo.id].string="preserved";
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),data);});
        assert(writes==0 && data.values[DPLightGen::aNodeInfo.id].string.text=="preserved");
        inputPoint.attrs.erase("wrong");
        assert(light.compute(MPlug(DPLightGen::aNodeInfo),data));
    }
    reset();auto wrongRadius=pointData();
    wrongRadius.values[DPWriteToAi::aRadiusAttributeName.id].string="wrong";
    inputPoint.field<std::string>("wrong","string",{"not numeric"});
    failsWithoutPublishing([&]{return point.compute(MPlug(DPWriteToAi::outDP),wrongRadius);});
    assert(writes==0);inputPoint.attrs.erase("wrong");
    assert(point.compute(MPlug(DPWriteToAi::outDP),wrongRadius));
    for (int writer : {0,1}) for (int condition : {0,1,2}) {
        reset(); auto p=pointData(); auto c=curveData();
        const char* field=condition==1?"radius":"custom";
        p.values[DPWriteToAi::aOutputAttributes.id].string=field;
        c.values[DWriteDCurveToAi::aExportCurveAttribute.id].string=field;
        inputPoint.field<double>(field,"double",{2.5});
        inputCurve.field<double>(field,"double",{2.5});
        parameterFixture::failDeclaration=condition==0;
        if (condition==2) parameterFixture::failDeclarationAt=1;
        if (condition==2) {
            p.values[DPWriteToAi::aOutputAttributes.id].string="custom late";
            c.values[DWriteDCurveToAi::aExportCurveAttribute.id].string="custom late";
            inputPoint.field<double>("late","double",{1});inputCurve.field<double>("late","double",{1});
        }
        failsWithoutPublishing([&]{return writer==0?point.compute(MPlug(DPWriteToAi::outDP),p):curve.compute(MPlug(DWriteDCurveToAi::outDC),c);});
        assert(writes==0);
    }
    for (int type : {2, 3}) {
        reset(); auto data = lightData();
        data.values[DPLightGen::aLightType.id].scalar = type;
        data.values[DPLightGen::aUseGobo.id].scalar = 1;
        data.values[DPLightGen::aGoboTexture.id].string = "fixture.tx";
        data.values[DPLightGen::aLightTexture.id].string = "fixture.tx";
        failLink = true;
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),data);});
        assert(writes == 0 && linkCalls == 1U);
        failLink = false; operations.clear();
        assert(light.compute(MPlug(DPLightGen::aNodeInfo),data) == MS::kSuccess);
        assert(writes == 1 && publications == 1 && cleanCalls == 1);
        if(type!=2)continue; // Only spot lights publish a gobo filter array.
        reset();data=lightData();data.values[DPLightGen::aLightType.id].scalar=type;
        data.values[DPLightGen::aUseGobo.id].scalar=1;
        data.values[DPLightGen::aGoboTexture.id].string="fixture.tx";
        data.values[DPLightGen::aLightTexture.id].string="fixture.tx";
        failElementAt=0;
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),data);});
        assert(writes==0&&elementCalls==1);
        failElementAt=-1;assert(light.compute(MPlug(DPLightGen::aNodeInfo),data));assert(writes==1);
    }
    // Ignore All uses the animation matrix, not an unused overflowing scale.
    reset(); auto ignoredScale = lightData();
    ignoredScale.values[DPLightGen::aLightType.id].scalar = 3;
    ignoredScale.values[DPLightGen::aApplyAnimMatType.id].scalar = DP_MAT_FULL_MAT_IGNORE_ALL;
    ignoredScale.values[DPLightGen::aGlobalScale.id].components[0] = 1e300;
    ignoredScale.values[DPLightGen::aScaleAttribute.id].string = "scale";
    inputPoint.field<float>("scale", "float", {1e20f});
    assert(light.compute(MPlug(DPLightGen::aNodeInfo),ignoredScale) == MS::kSuccess);
    assert(writes == 1 && publications == 1 && cleanCalls == 1);
    assert(std::find(operations.begin(),operations.end(),"transform.scale") == operations.end());
    for (int type : {4,5}) {
        reset(); auto radius = ignoredScale;
        radius.values[DPLightGen::aLightType.id].scalar = type;
        inputPoint.field<float>("scale", "float", {1e20f});
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),radius);});
        assert(writes == 0); // Cylinder/disk radius still uses this scale.
    }
    reset(); auto lateLink = lightData();
    lateLink.values[DPLightGen::aLightType.id].scalar = 3;
    lateLink.values[DPLightGen::aLightTexture.id].string = "fixture.tx";
    inputPoint.points.push_back(DPoint(4,5,6)); failLinkAt = 1;
    failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),lateLink);});
    assert(writes == 0 && linkCalls == 2U);
    failLinkAt = -1; operations.clear();
    assert(light.compute(MPlug(DPLightGen::aNodeInfo),lateLink) == MS::kSuccess);
    assert(writes == 1 && publications == 1 && cleanCalls == 1);
    reset();auto pathData=pointData();assert(point.compute(MPlug(DPWriteToAi::outDP),pathData));
    const auto pathEvent=std::find(operations.begin(),operations.end(),"file.path");assert(pathEvent!=operations.end());
    const int pathIndex=int(pathEvent-operations.begin());
    reset();pathData=pointData();pathData.values[DPWriteToAi::aExportMode.id].scalar=1;fileExists=true;failOperation=pathIndex;
    failsWithoutPublishing([&]{return point.compute(MPlug(DPWriteToAi::outDP),pathData);});assert(writes==0);
    for (int result : {-1, 1, 9}) {
        reset(); writeResult = result; auto a = lightData();
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),a);});
        reset(); writeResult = result; auto b = pointData();
        failsWithoutPublishing([&]{return point.compute(MPlug(DPWriteToAi::outDP),b);});
        reset(); writeResult = result; auto c = curveData();
        failsWithoutPublishing([&]{return curve.compute(MPlug(DWriteDCurveToAi::outDC),c);});
    }
    for (int type = 0; type < 7; ++type) for (int mode = 0; mode < DP_MAT_APPLY_TYPE_COUNT; ++mode) {
        reset(); auto data = lightData(); data.values[DPLightGen::aLightType.id].scalar = type;
        data.values[DPLightGen::aApplyAnimMatType.id].scalar = mode;
        assert(light.compute(MPlug(DPLightGen::aNodeInfo),data) == MS::kSuccess);
        assert(writes == 1 && publications == 1 && cleanCalls == 1);
        assert(outputInfo.find("ASS export completed") != std::string::npos);
        const auto events = operations;
        for (std::size_t i = 0; i < events.size(); ++i) {
            if (events[i] != "transform.scale" && events[i] != "transform.rotation" &&
                events[i] != "transform.translate" && events[i] != "transform.getTranslation") continue;
            reset(); auto failed = lightData();
            failed.values[DPLightGen::aLightType.id].scalar = type;
            failed.values[DPLightGen::aApplyAnimMatType.id].scalar = mode;
            failOperation = int(i);
            failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),failed);});
            assert(writes == 0);
        }
    }
    // Every exporter input must preserve its failed read instead of writing.
    for (const auto& attribute : lightInputAttributes) {
        reset(); auto data = lightData(); failAttribute = attribute.id;
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),data);});
        assert(writes == 0);
    }
    writerFailures(point, DPWriteToAi::outDP, pointData, DPWriteToAiInputAttributes);
    writerFailures(curve, DWriteDCurveToAi::outDC, curveData, DWriteDCurveToAiInputAttributes);
    reset(); auto baseline = lightData(); assert(light.compute(MPlug(DPLightGen::aNodeInfo),baseline));
    const auto events = operations;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i] == "file.path" || events[i] == "time.read" || events[i] == "output") {
            reset(); auto data = lightData(); failOperation = int(i);
            failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),data);});
        }
        for (int kind = 0; kind < 2; ++kind) {
            reset(); auto data = lightData(); throwOperation = int(i); exceptionKind = kind;
            assert(light.compute(MPlug(DPLightGen::aNodeInfo),data) != MS::kSuccess);
            assert(reports == 1 && sessions == 0 && liveNodes == 0 && liveArrays == 0);
        }
    }
    for (int condition = 0; condition < 7; ++condition) {
        reset(); auto data = lightData();
        if (condition == 0) { inputPoint.field<double>("intensity","double",{1e40}); data.values[DPLightGen::aIntensityAttribute.id].string = MString("intensity"); }
        if (condition == 1) { inputPoint.field<Imath::V3d>("color","vec3d",{{1e40,0,0}}); data.values[DPLightGen::aColorAttribute.id].string = MString("color"); }
        if (condition == 2) { MMatrix m; m[3][0] = 1e40; inputPoint.field<MMatrix>("matrix","matrix",{m}); data.values[DPLightGen::aAnimMatrixAttribute.id].string = MString("matrix"); data.values[DPLightGen::aApplyAnimMatType.id].scalar = DP_MAT_FULL_MAT_IGNORE_ALL; }
        if (condition == 3) { data.values[DPLightGen::aLightType.id].scalar = 3; data.values[DPLightGen::aLightTexture.id].string = MString("texture.tx"); data.values[DPLightGen::aRepeatTex.id].components[0] = 1e-308; }
        if (condition == 4) { data.values[DPLightGen::aLightType.id].scalar = 4; data.values[DPLightGen::aGlobalScale.id].components[0] = 1e200; inputPoint.field<double>("scale","double",{1e200}); data.values[DPLightGen::aScaleAttribute.id].string = MString("scale"); }
        if (condition == 5) data.values[DPLightGen::aExposure.id].scalar = 1e40;
        if (condition == 6) { data.values[DPLightGen::aLightType.id].scalar = 5; data.values[DPLightGen::aGlobalScale.id].components[0] = 1e40; }
        failsWithoutPublishing([&]{return light.compute(MPlug(DPLightGen::aNodeInfo),data);}); assert(writes == 0);
    }
    reset(); auto p = pointData(); p.values[DPWriteToAi::aConstantRadius.id].scalar = 1e40;
    failsWithoutPublishing([&]{return point.compute(MPlug(DPWriteToAi::outDP),p);}); assert(writes == 0);
    reset(); auto c = curveData(); c.values[DWriteDCurveToAi::aConstantWidth.id].scalar = 1e40;
    failsWithoutPublishing([&]{return curve.compute(MPlug(DWriteDCurveToAi::outDC),c);}); assert(writes == 0);
    for (int mode = 0; mode < 6; ++mode) {
        reset(); auto data = curveData();
        if (mode == 0 || mode == 1) {
            data.values[DWriteDCurveToAi::aCurveWidthType.id].scalar = mode+1;
            data.values[DWriteDCurveToAi::aWidthAttributeName.id].string = MString("width");
            if (mode == 0) inputCurve.field<double>("width","double",{1e40});
            else inputCurve.curves[0]->field<double>("width","double",{1e40,1,1,1});
        } else if (mode == 2 || mode == 3) {
            data.values[DWriteDCurveToAi::aExportCurveAttribute.id].string = MString("payload");
            if (mode == 2) inputCurve.field<double>("payload","double",{1e40});
            else inputCurve.field<Imath::V3d>("payload","vec3d",{{1e40,0,0}});
        } else {
            data.values[DWriteDCurveToAi::aExportPointAttribute.id].string = MString("payload");
            if (mode == 4) inputCurve.curves[0]->field<Imath::V2d>("payload","vec2d",{{1e40,0},{1,0},{1,0},{1,0}});
            else inputCurve.curves[0]->field<Imath::V3d>("payload","vec3d",{{1e40,0,0},{1,0,0},{1,0,0},{1,0,0}});
        }
        failsWithoutPublishing([&]{return curve.compute(MPlug(DWriteDCurveToAi::outDC),data);}); assert(writes == 0);
    }
    for (int mode = 0; mode < 3; ++mode) {
        reset(); auto data = pointData(); data.values[DPWriteToAi::aOutputAttributes.id].string = MString("payload");
        if (mode == 0) inputPoint.field<double>("payload","double",{1e40});
        if (mode == 1) inputPoint.field<Imath::V2d>("payload","vec2d",{{1e40,0}});
        if (mode == 2) inputPoint.field<Imath::V3d>("payload","vec3d",{{1e40,0,0}});
        failsWithoutPublishing([&]{return point.compute(MPlug(DPWriteToAi::outDP),data);}); assert(writes == 0);
    }
    for (bool binary : {false,true}) {
        reset(); auto data = curveData(); data.values[DWriteDCurveToAi::aIsBinary.id].scalar = binary;
        assert(curve.compute(MPlug(DWriteDCurveToAi::outDC),data)); assert(writes == 1 && lastBinary == binary);
    }
#else
    reset(); assert(dommy::arnold::writeAssFile("/tmp/out.ass",false,"fixture") != MS::kSuccess); assert(writes == 0);
    reset(); auto data = lightData(); assert(light.compute(MPlug(DPLightGen::aNodeInfo),data)); assert(writes == 0);
#endif
    for (bool overrideCallback : {false,true}) {
        reset(); DPLightGenOverride draw; MDagPath path;
        const MBoundingBox bounds = overrideCallback ? draw.boundingBox(path,path) : light.boundingBox();
        assert(bounds.first.x == -2 && bounds.second.z == 7);
        const auto events = operations;
        const auto query=std::find(events.begin(),events.end(),"plug.connected");
        assert(query!=events.end());
        for(bool connectionValue:{false,true}) {
            reset();connected=connectionValue;failOperation=int(query-events.begin());
            const auto fallback=overrideCallback?draw.boundingBox(path,path):light.boundingBox();
            assert(fallback.first.x==-1 && fallback.second.z==1);
            assert(operations.back()=="error");
            for(int kind:{0,1}) {
                reset();failOperation=int(query-events.begin());
                throwOperation=failOperation+1;exceptionKind=kind;
                const auto protectedFallback=overrideCallback?draw.boundingBox(path,path):light.boundingBox();
                assert(protectedFallback.first.x==-1 && protectedFallback.second.z==1 && reports==1);
            }
            reset();const auto retry=overrideCallback?draw.boundingBox(path,path):light.boundingBox();
            assert(retry.first.x==-2 && retry.second.z==7);
        }
        for (std::size_t i = 0; i < events.size(); ++i) for (int kind = 0; kind < 2; ++kind) {
            reset(); throwOperation = int(i); exceptionKind = kind;
            const auto fallback = overrideCallback ? draw.boundingBox(path,path) : light.boundingBox();
            assert(reports == 1 && fallback.first.x == -1 && fallback.second.z == 1);
        }
        reset(); connected = false;
        const auto fallback = overrideCallback ? draw.boundingBox(path,path) : light.boundingBox();
        assert(fallback.first.x == -1 && fallback.second.z == 1);
    }
    reset();
}
