// Actual production callback/lifecycle bodies; Maya, MtoA and Arnold are seams.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>
#include "common/utility/DGenerationLimits.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "lifecycle_ray_flags.inc"

static int faultAt=-1, faultKind=0, calls=0, iteratorFailAt=-1, iteratorCalls=0;
static bool badBinding=false,badUuid=false,badName=false,badAttribute=false;
static bool absentAttribute=false,verboseList=false;static short volumeMode=0;
static std::string configuredPrefix="asset_";
static bool badRead=false,badPath=false,missingFile=false,loggerFault=false,hideFault=false;
static bool renderMode=true,pointConnection=true,meshConnection=true,payloadReached=false;
static bool invalidPoint=false,invalidMesh=false,pointReadFailure=false,meshReadFailure=false;
static bool connectionFailure=false;
static std::string missingPlug,failedConnection;
static std::string failedSetting;
static int settingFaultKind=0;
static unsigned generationSettingFinds=0U,generationSettingReads=0U,matrixReads=0U,motionReads=0U,radiusWrites=0U;
static bool matrixReadFault=false,motionReadFault=false,rootHideFault=false;
static short configuredCopyMode=1;
static std::size_t pointCount=1U;
static unsigned meshReads=0U,pointReads=0U,meshQueries=0U,pointQueries=0U,liveHandles=0U;
static unsigned incompatibleTagReads=0U,reportedStringLength=0U;
static bool nullStringPointer=false;
#ifdef DOMMY_SDK_CHECK
#include <maya/MFileObject.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MUuid.h>
#include <maya/MDataHandle.h>
#include <maya/MFnData.h>
#endif
static int loadResult=0,loads=0,liveIterators=0;static unsigned reports=0;
static int allocationFaultAt=-1,allocationCalls=0;
void* operator new(std::size_t size){if(allocationCalls++==allocationFaultAt)throw std::bad_alloc();
    if(void* value=std::malloc(size?size:1U))return value;throw std::bad_alloc();}
void operator delete(void* value)noexcept{std::free(value);}
void api(){if(calls++==faultAt){if(faultKind)throw std::bad_alloc();throw std::runtime_error("API seam");}}
#ifndef DOMMY_SDK_CHECK
struct MStatus{bool ok;MStatus(bool value=true):ok(value){}operator bool()const{return ok;}
    bool operator==(const MStatus& o)const{return ok==o.ok;}bool operator!=(const MStatus& o)const{return ok!=o.ok;}};
namespace MS{const MStatus kSuccess{},kInvalidParameter{false};}
struct MString{std::string text;MString(const char* s=""):text(s){}const char* asChar()const{api();return nullStringPointer?nullptr:text.c_str();}
    unsigned length()const{api();return reportedStringLength?reportedStringLength:static_cast<unsigned>(text.size());}
    bool operator==(const char* s)const{api();return text==s;}};
struct MObject{};
struct MUuid{bool valid()const{api();return !badUuid;}MString asString()const{api();return MString("owner");}};
struct MFnDependencyNode{explicit MFnDependencyNode(const MObject&,MStatus* s=nullptr){api();if(s)s->ok=!badBinding;}
    MUuid uuid(MStatus* s=nullptr)const{api();if(s)s->ok=!badUuid;return {};}
    bool hasAttribute(const MString&,MStatus* s=nullptr)const{api();if(s)s->ok=!badAttribute;return !absentAttribute;}};
struct MPlug{std::string name;MPlug(const char* s=""):name(s){}
    MString partialName(bool,bool,bool,bool,bool,bool longName,MStatus* s=nullptr)const{api();assert(longName);if(s)s->ok=!badName;return MString(name.c_str());}
    bool isNull()const{api();return name==missingPlug;}bool isConnected(MStatus* s)const{api();s->ok=!connectionFailure&&name!=failedConnection;
        if(name=="inMesh"){++meshQueries;return meshConnection;}++pointQueries;return pointConnection;}
    MPlug source(MStatus* s)const{api();s->ok=true;return *this;}};
#endif
enum{DP_INST_COPY_MESH,DP_INST_COPY_ASS,DP_INST_COPY_TYPE_COUNT};
typedef int DP_INST_COPY_TYPE;
#ifdef DOMMY_SDK_CHECK
MStatus readPlugValue(const MPlug&,MString&);
MStatus readPlugValue(const MPlug&,short&);
MStatus readPlugValue(const MPlug&,bool&);
#else
bool settingRead(const MPlug& p){api();if(p.name!="IsRender"&&p.name!="VolumeMode")++generationSettingReads;
    if(p.name==failedSetting){if(settingFaultKind==1)throw std::runtime_error("setting seam");
        if(settingFaultKind==2)throw std::bad_alloc();return false;}return !badRead;}
MStatus readPlugValue(const MPlug& p,MString& out){const bool ok=settingRead(p);out=MString(p.name=="AssPrefix"?configuredPrefix.c_str():"file.ass");return {ok};}
MStatus readPlugValue(const MPlug& p,short& out){const bool ok=settingRead(p);out=p.name=="VolumeMode"?volumeMode:configuredCopyMode;return {ok};}
MStatus readPlugValue(const MPlug& p,bool& out){const bool ok=settingRead(p);out=p.name=="IsRender"?renderMode:verboseList;return {ok};}
struct MFileObject{MStatus setRawFullName(const char*){api();return {!badPath};}
    MString resolvedFullName()const{api();return MString("file.ass");}bool exists()const{api();return !missingFile;}};
#endif
enum{AI_NODE_SHAPE=1,AI_NODE_SHADER=2,AI_NODE_LIGHT=4,AI_NODE_ALL=7,MTOA_MBLUR_DEFORM=1};
enum{AI_TYPE_STRING=10,AI_TYPE_INT=1,AI_TYPE_ARRAY=13,AI_TYPE_UNDEFINED=255,
     AI_USERDEF_CONSTANT=1,AI_USERDEF_UNIFORM=2,AI_USERDEF_VARYING=3};
struct AtUserParamEntry{int present=-1,type=AI_TYPE_STRING,category=AI_USERDEF_CONSTANT;};
struct AtNodeEntry{int type=AI_NODE_SHAPE;};
struct AtString{const char* value;explicit AtString(const char* s):value(s){}operator bool()const{return value&&*value;}
    const char* c_str()const{return value;}};
struct AtArray{std::vector<void*> values;};
struct AtNode{std::string name,meshTag,objTag;AtNodeEntry entry;bool destroyed=false,denyDestroy=false;unsigned visibility=7;
    AtNode* dependency=nullptr;int destroyFault=0;AtUserParamEntry objSchema,meshSchema;};
static std::vector<AtNode*> universe,importNodes;static AtNode output;
static std::array<AtNode*,64> destroyAttempts;static std::size_t destroyAttemptCount=0;
static bool liveDependencyDeleted=false;
static AtNode* renameOnLoad=nullptr;
struct AtNodeIterator{std::vector<AtNode*> nodes;std::size_t index=0;};
AtNodeIterator* AiUniverseGetNodeIterator(int mask){api();if(iteratorCalls++==iteratorFailAt)return nullptr;
    std::unique_ptr<AtNodeIterator> i(new AtNodeIterator);for(auto* n:universe)if(!n->destroyed&&(n->entry.type&mask))i->nodes.push_back(n);
    ++liveIterators;return i.release();}
void AiNodeIteratorDestroy(AtNodeIterator* i){delete i;--liveIterators;}
bool AiNodeIteratorFinished(AtNodeIterator* i){api();return i->index==i->nodes.size();}
AtNode* AiNodeIteratorGetNext(AtNodeIterator* i){api();return i->nodes.at(i->index++);}
const AtNodeEntry* AiNodeGetNodeEntry(AtNode* n){api();assert(n&&!n->destroyed);return &n->entry;}
int AiNodeEntryGetType(const AtNodeEntry* e){api();return e->type;}
const char* AiNodeGetName(AtNode* n){api();assert(!n->destroyed);return n->name.c_str();}
const AtUserParamEntry* tagSchema(const AtNode* n,const char* p){
    const bool mesh=std::string(p)=="dInstMeshTag";const auto& schema=mesh?n->meshSchema:n->objSchema;
    const bool present=schema.present<0?!(mesh?n->meshTag:n->objTag).empty():schema.present!=0;
    return present?&schema:nullptr;}
const AtUserParamEntry* AiNodeLookUpUserParameter(const AtNode* n,const char* p){api();assert(n&&!n->destroyed);return tagSchema(n,p);}
int AiUserParamGetType(const AtUserParamEntry* p){api();return p->type;}
int AiUserParamGetCategory(const AtUserParamEntry* p){api();return p->category;}
int AiUserParamGetArrayType(const AtUserParamEntry*){api();return AI_TYPE_UNDEFINED;}
AtString AiNodeGetStr(AtNode* n,const char* p){api();assert(!n->destroyed);
    const auto* schema=tagSchema(n,p);if(!schema||schema->type!=AI_TYPE_STRING||schema->category!=AI_USERDEF_CONSTANT)++incompatibleTagReads;
    return AtString(std::string(p)=="dInstMeshTag"?n->meshTag.c_str():n->objTag.c_str());}
bool AiNodeDestroy(AtNode* n){api();assert(!n->destroyed);assert(destroyAttemptCount<destroyAttempts.size());
    destroyAttempts[destroyAttemptCount++]=n;
    if(n->destroyFault==1)throw std::runtime_error("destroy seam");if(n->destroyFault==2)throw std::bad_alloc();
    if(n->denyDestroy)return false;
    for(auto* consumer:universe)if(!consumer->destroyed&&consumer->dependency==n)liveDependencyDeleted=true;
    n->destroyed=true;return true;}
void AiNodeSetByte(AtNode* n,const AtString&,int v){if(hideFault||(n==&output&&rootHideFault))throw 7;assert(n&&!n->destroyed);n->visibility=v;}
int AiASSLoad(const char*,int){api();++loads;for(auto* n:importNodes)universe.push_back(n);
    if(renameOnLoad)renameOnLoad->name="asset_renamedExisting";return loadResult;}
void AiMsgError(const char*,...){++reports;if(loggerFault)throw 7;}
void AiMsgWarning(const char*,...){if(loggerFault)throw 7;api();}void AiMsgInfo(const char*,...){api();}void AiMsgDebug(const char*,...){api();}
AtArray* AiNodeGetArray(AtNode*,const char*){api();return nullptr;}
AtNode* AiNode(const char*){throw std::logic_error("creation is outside the input fixture");}
void AiNodeSetFlt(AtNode*,const char*,float){++radiusWrites;api();}
unsigned AiArrayGetNumElements(AtArray* a){return a->values.size();}void* AiArrayGetPtr(AtArray* a,unsigned i){return a->values.at(i);}
#define INST_DEBUG(...) ((void)0)
#define DOMMY_MAYA_DEBUG(...) ((void)0)
struct CNodeTranslator{enum{AI_RECREATE_TRANSLATOR=7,AI_RECREATE_NODE=8};};
struct NameCount{unsigned length()const{return 0U;}};
// Real shared metadata/name helper bodies with explicit renderer/string seams.
#include "lifecycle_schema.inc"
#include "lifecycle_names.inc"
namespace dommy{namespace arnold{
#include "lifecycle_resources.inc"
}}
struct BaseTranslator{
    bool shouldRefreshAss=false;std::string referenceNodePrefix;std::vector<AtNode*> refNodeList,ownedReferenceNodes;
    dommy::arnold::NodeBatch pendingNodes_;
    unsigned updates=0,requests=0;int mode=-1;
    AtNode* AddArnoldNode(const char*){api();return &output;}MString GetMayaNodeName(){api();return MString("mayaOwner");}
    MObject GetMayaObject(){api();return {};}
#ifdef DOMMY_SDK_CHECK
    MPlug FindMayaPlug(const char*){return MPlug();}
#else
    MPlug FindMayaPlug(const char* n){api();if(std::string(n)!="IsRender"&&std::string(n)!="inDP"&&std::string(n)!="inMesh")++generationSettingFinds;return MPlug(n);}
#endif
    void ExportMatrix(AtNode*){++matrixReads;api();if(matrixReadFault)throw std::runtime_error("matrix seam");}
    unsigned GetNumMotionSteps()const{++motionReads;if(motionReadFault)throw std::runtime_error("motion seam");return 1U;}
    void Update(AtNode*){api();++updates;}void SetUpdateMode(int m){api();mode=m;}void RequestUpdate(){api();++requests;}
    bool IsMotionBlurEnabled(int){api();return true;}bool IsLocalMotionBlurEnabled(){api();return true;}
};
#ifndef DOMMY_SDK_CHECK
namespace MFnData{enum{ kMesh=1,kInvalid=0 };}
struct MDataHandle{int type()const{api();return invalidMesh?MFnData::kInvalid:MFnData::kMesh;}};
#endif
namespace dommy{namespace maya{
struct ScopedPlugDataHandle{
    MDataHandle value;
    ~ScopedPlugDataHandle(){if(acquired)--liveHandles;}
    MDataHandle& handle(){return value;}
    MStatus read(const MPlug& plug);
    bool acquired=false;
};
#ifndef DOMMY_SDK_CHECK
MStatus ScopedPlugDataHandle::read(const MPlug& plug){api();
    const bool mesh=plug.name=="inMesh";if(mesh)++meshReads;else ++pointReads;
    const bool success=!badRead&&!(mesh?meshReadFailure:pointReadFailure);
    if(success){acquired=true;++liveHandles;}return {success};}
#endif
}}
struct DPData{std::size_t numberOfPoints()const{api();return pointCount;}};
const DPData* getInputDPfromHandle(const MDataHandle&){api();static DPData value;return invalidPoint?nullptr:&value;}
// The real production batch and retirement body share this modeled universe.
#include "lifecycle_methods.inc"

#ifndef DOMMY_SDK_CHECK
void reset(){faultAt=-1;faultKind=calls=0;iteratorFailAt=-1;iteratorCalls=0;
    badBinding=badUuid=badName=badAttribute=badRead=badPath=missingFile=loggerFault=hideFault=false;
    absentAttribute=verboseList=false;volumeMode=0;configuredPrefix="asset_";
    renderMode=pointConnection=meshConnection=true;payloadReached=false;
    invalidPoint=invalidMesh=pointReadFailure=meshReadFailure=connectionFailure=false;
    missingPlug.clear();failedConnection.clear();configuredCopyMode=DP_INST_COPY_ASS;pointCount=1U;
    pointReads=meshReads=pointQueries=meshQueries=0U;assert(liveHandles==0U);
    incompatibleTagReads=reportedStringLength=0U;
    nullStringPointer=false;
    failedSetting.clear();settingFaultKind=0;
    generationSettingFinds=generationSettingReads=matrixReads=motionReads=radiusWrites=0U;
    matrixReadFault=motionReadFault=rootHideFault=false;
    renameOnLoad=nullptr;
    destroyAttemptCount=0;liveDependencyDeleted=false;
    loads=loadResult=0;reports=0;allocationFaultAt=-1;allocationCalls=0;output=AtNode();assert(liveIterators==0);}
template<class T>void testCallbacks(const char* relevant){
    MObject o;MPlug p(relevant);
    for(int method=0;method<3;++method){reset();T t;
        auto invoke=[&](T& v){if(method==0)return v.CreateArnoldNodes()!=nullptr;
            if(method==1){v.Export(&output);return v.updates==1U;}v.NodeChanged(o,p);return v.requests==1U;};
        assert(invoke(t));const int boundaryCount=calls;
        for(int kind=0;kind<2;++kind)for(int i=0;i<boundaryCount;++i){reset();T failed;faultAt=i;faultKind=kind;
            (void)invoke(failed);assert(reports==1U);if(method==0)assert(failed.updates==0U);
            if(method==1)assert(output.visibility==0);reset();T retry;assert(invoke(retry));}
    }
    for(int bad=0;bad<3;++bad){reset();T t;badBinding=bad==0;badName=bad==1;badAttribute=bad==2;
        t.NodeChanged(o,p);if(bad<2||std::is_same<T,DPInstancerTranslatorAi>::value){assert(t.requests==0U);assert(!t.shouldRefreshAss);}}
    reset();T t;MPlug unrelated("unrelated");t.NodeChanged(o,unrelated);
    if(!std::is_same<T,DPInstancerTranslatorAi>::value)assert(t.requests==0U);
    if(std::is_same<T,DPInstancerTranslatorAi>::value){reset();T absent;absentAttribute=true;absent.NodeChanged(o,p);assert(absent.requests==0U&&!absent.shouldRefreshAss);}
    if(std::is_same<T,DVDBTranslatorAi>::value){for(short value:{short(-1),short(0),short(1),short(2)}){reset();volumeMode=value;T v;
        assert((v.CreateArnoldNodes()!=nullptr)==(value==0||value==1));}}
}
template<class T>void testMotion(bool supported){reset();T t;assert(t.RequiresMotionData()==supported);const int total=calls;
    for(int k=0;k<2;++k)for(int i=0;i<total;++i){reset();faultAt=i;faultKind=k;assert(!t.RequiresMotionData());assert(reports==1U);}}
AtNode node(const char* name,int type=AI_NODE_SHAPE,const char* owner=""){AtNode n;n.name=name;n.entry.type=type;n.objTag=owner;return n;}
struct InputFixture {
    AtNode child=node("old_instance",AI_NODE_SHAPE,"owner"),base=node("old_base");
    AtNode reference=node("asset_ref"),shader=node("asset_shader",AI_NODE_SHADER);
    AtNode foreign=node("foreign",AI_NODE_SHAPE,"other"),samePrefix=node("asset_unowned");
    AtNode pendingBase=node("pending_base"),pendingChild=node("pending_instance",AI_NODE_SHAPE,"owner");
    DPInstancerTranslatorAi translator;
    InputFixture(){base.meshTag=pendingBase.meshTag="owner";
        child.dependency=&base;reference.dependency=&shader;pendingChild.dependency=&pendingBase;
        reference.visibility=0U;universe={&shader,&base,&reference,&child,&foreign,&samePrefix};
        translator.referenceNodePrefix="asset_";translator.refNodeList={&reference};
        translator.ownedReferenceNodes={&reference,&shader};}
    void pending(){universe.push_back(&pendingChild);universe.push_back(&pendingBase);
        translator.pendingNodes_.reserve(2U);
        translator.pendingNodes_.adopt(dommy::arnold::NodeOwner(&pendingBase));
        translator.pendingNodes_.adopt(dommy::arnold::NodeOwner(&pendingChild));}
    void cache(bool expectedRefresh=false){assert(!reference.destroyed&&!shader.destroyed&&!foreign.destroyed&&!samePrefix.destroyed);
        assert(translator.referenceNodePrefix=="asset_"&&translator.refNodeList==std::vector<AtNode*>{&reference});
        assert(translator.ownedReferenceNodes==std::vector<AtNode*>({&reference,&shader}));
        assert(translator.shouldRefreshAss==expectedRefresh&&!liveDependencyDeleted&&liveIterators==0&&liveHandles==0U);}
};
void inputModes(){
    for(short mode:{short(DP_INST_COPY_MESH),short(DP_INST_COPY_ASS)}){
        for(int state=0;state<15;++state){reset();InputFixture f;configuredCopyMode=mode;
            renderMode=state!=1;pointConnection=state!=2;meshConnection=state!=3;
            if(state==4)pointCount=0U;
            invalidPoint=state==5;invalidMesh=state==6;pointReadFailure=state==7;meshReadFailure=state==8;
            if(state==9)missingPlug="inDP";if(state==10)missingPlug="inMesh";
            connectionFailure=state==11;
            if(state==12)configuredCopyMode=-1;if(state==13)configuredCopyMode=DP_INST_COPY_TYPE_COUNT;
            if(state==14)failedConnection="inMesh";
            f.translator.inputUpdate(&output);
            const bool cleanup=state==1||state==2||state==4||(mode==DP_INST_COPY_MESH&&state==3);
            const bool invalid=state==5||state==7||state==9||state==11||state==12||state==13||
                (mode==DP_INST_COPY_MESH&&(state==6||state==8||state==10||state==14));
            assert(payloadReached==(!cleanup&&!invalid));
            if(payloadReached)assert(matrixReads==1U&&motionReads==1U&&radiusWrites==1U);
            if(f.child.destroyed!=cleanup||f.base.destroyed!=cleanup)
                std::cerr<<"Input state mismatch: mode="<<mode<<" state="<<state<<" reports="<<reports<<'\n';
            assert(f.child.destroyed==cleanup&&f.base.destroyed==cleanup);
            assert(destroyAttemptCount==(cleanup?2U:0U));
            if(cleanup){assert(destroyAttempts[0]==&f.child&&destroyAttempts[1]==&f.base);
                assert(reports==0U);const auto before=destroyAttemptCount;
                f.translator.inputUpdate(&output);assert(destroyAttemptCount==before);}
            if(invalid)assert(reports>=1U);
            if(mode==DP_INST_COPY_ASS)assert(meshQueries==0U&&meshReads==0U);
            if(state==1||state==2)assert(meshQueries==0U&&pointReads==0U&&meshReads==0U);
            if(state==4)assert(pointReads>0U&&meshReads==0U);
            f.cache();
        }
    }
    // A valid empty point payload does not need to acquire or inspect a mesh.
    reset();InputFixture f;configuredCopyMode=DP_INST_COPY_MESH;pointCount=0U;
    invalidMesh=meshReadFailure=true;f.translator.inputUpdate(&output);
    assert(f.child.destroyed&&f.base.destroyed&&meshReads==0U&&reports==0U);f.cache();
    for(bool refresh:{false,true}){reset();InputFixture transition;transition.translator.shouldRefreshAss=refresh;
        meshConnection=false;transition.translator.inputUpdate(&output);assert(payloadReached);transition.cache(refresh);
        payloadReached=false;renderMode=false;transition.translator.inputUpdate(&output);
        assert(!payloadReached&&transition.child.destroyed&&transition.base.destroyed);transition.cache(refresh);
        renderMode=true;transition.translator.inputUpdate(&output);assert(payloadReached);transition.cache(refresh);
        // Only Mesh mode requires its selected input; ASS stays reusable.
        payloadReached=false;configuredCopyMode=DP_INST_COPY_MESH;transition.translator.inputUpdate(&output);
        assert(!payloadReached);transition.cache(refresh);
        configuredCopyMode=DP_INST_COPY_ASS;transition.translator.inputUpdate(&output);assert(payloadReached);transition.cache(refresh);
    }
}
void inputFailures(){
    for(short mode:{short(DP_INST_COPY_MESH),short(DP_INST_COPY_ASS)}){
        reset();InputFixture baseline;configuredCopyMode=mode;calls=allocationCalls=0;
        baseline.translator.inputUpdate(&output);assert(payloadReached);
        const int apiCount=calls,allocationCount=allocationCalls;
        for(int kind=0;kind<2;++kind)for(int index=0;index<apiCount;++index){reset();InputFixture f;configuredCopyMode=mode;
            calls=0;faultAt=index;faultKind=kind;f.translator.inputUpdate(&output);
            assert(!payloadReached&&!f.child.destroyed&&!f.base.destroyed&&destroyAttemptCount==0U&&reports>=1U);
            f.cache();faultAt=-1;calls=0;f.translator.inputUpdate(&output);assert(payloadReached);f.cache();}
        for(int index=0;index<allocationCount;++index){reset();InputFixture f;configuredCopyMode=mode;
            allocationCalls=0;allocationFaultAt=index;f.translator.inputUpdate(&output);allocationFaultAt=-1;
            assert(!payloadReached&&!f.child.destroyed&&!f.base.destroyed&&destroyAttemptCount==0U&&reports>=1U);
            f.cache();f.translator.inputUpdate(&output);assert(payloadReached);f.cache();}
    }
}
void inactiveSettings(){
    const char* settings[]={"CopyObjectType","IsOverrideShader","Sidedness","InvertNormals","aiOpaque","aiMatte",
        "aiSelfShadows","aiReceiveShadows","ExportAttribute"};
    unsigned cases=0U;
    for(int state=0;state<4;++state)for(const char* setting:settings)for(int failure=0;failure<4;++failure){
        if(state==3&&std::string(setting)=="CopyObjectType")continue; // Needed to identify a Mesh-only disconnection.
        reset();InputFixture f;configuredCopyMode=DP_INST_COPY_MESH;
        renderMode=state!=0;pointConnection=state!=1;pointCount=state==2?0U:1U;meshConnection=state!=3;
        if(failure==3)missingPlug=setting;else {failedSetting=setting;settingFaultKind=failure;}
        f.translator.inputUpdate(&output);
        assert(f.child.destroyed&&f.base.destroyed&&!payloadReached&&destroyAttemptCount==2U&&reports==0U);
        assert(matrixReads==0U&&motionReads==0U&&radiusWrites==0U&&output.visibility==0U);
        assert(generationSettingFinds==(state==3?1U:0U)&&generationSettingReads==(state==3?1U:0U));
        if(state<2)assert(pointReads==0U);
        assert(meshReads==0U);f.cache();++cases;
    }
    for(int state=0;state<3;++state){reset();InputFixture f;
        renderMode=state!=0;pointConnection=state!=1;pointCount=state==2?0U:1U;configuredCopyMode=-1;
        matrixReadFault=motionReadFault=true;f.translator.inputUpdate(&output);
        assert(f.child.destroyed&&f.base.destroyed&&reports==0U&&matrixReads==0U&&motionReads==0U);f.cache();
    }
    for(int state=0;state<4;++state){reset();InputFixture f;
        renderMode=state!=0;pointConnection=state!=1;pointCount=state==2?0U:1U;
        configuredCopyMode=DP_INST_COPY_MESH;meshConnection=state!=3;rootHideFault=true;
        f.translator.inputUpdate(&output);rootHideFault=false;
        assert(f.child.destroyed&&f.base.destroyed&&!payloadReached);f.cache();
    }
    // Unknown stop state is never a successful disabled/empty decision.
    for(bool render:{false,true})for(int failure=0;failure<3;++failure){reset();InputFixture f;f.pending();
        renderMode=render;failedSetting="IsRender";settingFaultKind=failure;
        f.translator.inputUpdate(&output);
        assert(!f.child.destroyed&&!f.base.destroyed&&!f.pendingChild.destroyed&&!f.pendingBase.destroyed&&destroyAttemptCount==0U);
        assert(reports>0U&&!payloadReached);f.cache();
        failedSetting.clear();settingFaultKind=0;renderMode=false;f.translator.inputUpdate(&output);
        assert(f.child.destroyed&&f.base.destroyed&&f.pendingChild.destroyed&&f.pendingBase.destroyed);
    }
    // Nonempty updates still require every selected setting and matrix/motion read.
    for(const char* setting:settings)for(int failure=0;failure<4;++failure){reset();InputFixture f;
        if(failure==3)missingPlug=setting;else {failedSetting=setting;settingFaultKind=failure;}
        f.translator.inputUpdate(&output);
        assert(!payloadReached&&!f.child.destroyed&&!f.base.destroyed&&destroyAttemptCount==0U&&reports>0U);f.cache();
    }
    for(int failure=0;failure<2;++failure){reset();InputFixture f;
        matrixReadFault=failure==0;motionReadFault=failure==1;f.translator.inputUpdate(&output);
        assert(!payloadReached&&!f.child.destroyed&&!f.base.destroyed&&destroyAttemptCount==0U&&reports>0U);f.cache();
        matrixReadFault=motionReadFault=false;f.translator.inputUpdate(&output);assert(payloadReached);f.cache();
    }
    std::cout<<"Inactive unused-setting failure cases passed: "<<cases<<'\n';
}
void inactivePending(){
    for(int failure=0;failure<3;++failure){reset();InputFixture f;f.pending();
        invalidPoint=failure==0;pointReadFailure=failure==1;connectionFailure=failure==2;
        f.translator.inputUpdate(&output);
        assert(!payloadReached&&destroyAttemptCount==0U&&!f.child.destroyed&&!f.base.destroyed&&
               !f.pendingChild.destroyed&&!f.pendingBase.destroyed&&!f.translator.pendingNodes_.empty());f.cache();
        invalidPoint=pointReadFailure=connectionFailure=false;renderMode=false;f.translator.inputUpdate(&output);
        assert(destroyAttemptCount==4U&&f.translator.pendingNodes_.empty());f.cache();
    }
    for(bool empty:{false,true})for(int failure=0;failure<5;++failure){reset();InputFixture f;f.pending();
        renderMode=empty;pointCount=0U;f.pendingChild.denyDestroy=failure==1||failure==4;
        f.pendingChild.destroyFault=failure==2?1:failure==3?2:0;loggerFault=failure==4;
        f.translator.inputUpdate(&output);loggerFault=false;
        assert(!payloadReached&&destroyAttempts[0]==&f.pendingChild);
        if(failure){assert(destroyAttemptCount==1U&&iteratorCalls==0);
            assert(!f.pendingChild.destroyed&&!f.pendingBase.destroyed&&!f.child.destroyed&&!f.base.destroyed);
            assert(!f.translator.pendingNodes_.empty());f.cache();
            f.pendingChild.denyDestroy=false;f.pendingChild.destroyFault=0;
            destroyAttemptCount=0;f.translator.inputUpdate(&output);}
        assert(destroyAttemptCount==4U&&f.translator.pendingNodes_.empty());
        assert(destroyAttempts[0]==&f.pendingChild&&destroyAttempts[1]==&f.pendingBase&&
               destroyAttempts[2]==&f.child&&destroyAttempts[3]==&f.base);
        assert(f.child.destroyed&&f.base.destroyed&&f.pendingChild.destroyed&&f.pendingBase.destroyed);f.cache();
        f.translator.inputUpdate(&output);assert(destroyAttemptCount==4U);f.cache();
    }
    for(int tier=0;tier<2;++tier)for(int failure=0;failure<4;++failure){reset();InputFixture f;renderMode=false;
        AtNode* const failed=tier==0?&f.child:&f.base;
        failed->denyDestroy=failure==0||failure==3;failed->destroyFault=failure==1?1:failure==2?2:0;loggerFault=failure==3;
        f.translator.inputUpdate(&output);loggerFault=false;
        assert(destroyAttemptCount==static_cast<std::size_t>(tier+1)&&!failed->destroyed);
        assert(f.child.destroyed==(tier==1)&&!f.base.destroyed);f.cache();
        failed->denyDestroy=false;failed->destroyFault=0;f.translator.inputUpdate(&output);
        assert(f.child.destroyed&&f.base.destroyed);f.cache();
    }
}
void sidednessFlags(){
    for(auto stage:{sidednessStage0,sidednessStage1,sidednessStage2}){
        reset();stage(false);assert(output.visibility==0U);
        stage(true);assert(output.visibility==std::numeric_limits<std::uint8_t>::max());
        // A reused geometry node must also switch back to single-sided.
        stage(false);assert(output.visibility==0U);
    }
}
void deletion(){
    auto run=[](int failure){reset();AtNode reference=node("asset_ref"),shader=node("asset_shader",AI_NODE_SHADER),generated=node("generated",AI_NODE_SHAPE,"owner");
        AtNode foreign=node("asset_foreign",AI_NODE_SHAPE,"other"),light=node("asset_light",AI_NODE_LIGHT),unrelated=node("asset_otherShader",AI_NODE_SHADER);
        universe={&reference,&shader,&generated,&foreign,&light,&unrelated};DPInstancerTranslatorAi t;t.referenceNodePrefix="asset_";
        t.refNodeList={&reference};t.ownedReferenceNodes={&reference,&shader};if(failure==1)iteratorFailAt=0;if(failure==2)reference.denyDestroy=true;
        if(failure==3)badUuid=true;if(failure==4)badBinding=true;
        const bool ok=t.deleteInstancedNodes(true);assert(ok==(failure==0));assert(!foreign.destroyed&&!light.destroyed&&!unrelated.destroyed);
        if(failure==1||failure==3||failure==4){assert(t.refNodeList.size()==1U&&t.ownedReferenceNodes.size()==2U);assert(!reference.destroyed&&!generated.destroyed);}
        else if(failure==2){assert(!reference.destroyed&&!shader.destroyed&&generated.destroyed);assert(t.refNodeList==std::vector<AtNode*>{&reference});
            assert((t.ownedReferenceNodes==std::vector<AtNode*>{&reference,&shader}));reset();reference.denyDestroy=false;assert(t.deleteInstancedNodes(true));}
        else assert(t.refNodeList.empty()&&t.ownedReferenceNodes.empty()&&t.referenceNodePrefix.empty());
        assert(liveIterators==0);};
    for(int i=0;i<5;++i)run(i);
    reset();AtNode ref=node("asset_ref",AI_NODE_SHAPE,"owner"),gen=node("generated",AI_NODE_SHAPE,"owner");universe={&ref,&gen};DPInstancerTranslatorAi t;
    t.refNodeList={&ref};t.ownedReferenceNodes={&ref};t.referenceNodePrefix="asset_";assert(t.deleteInstancedNodes(false));assert(gen.destroyed&&!ref.destroyed&&t.refNodeList.size()==1U);
    reset();ref.destroyed=true;assert(t.deleteInstancedNodes(true));assert(t.refNodeList.empty()&&t.ownedReferenceNodes.empty());
    // A stale cached address reused by a differently named/type node must fail
    // closed before retirement or subsequent cached-reference use.
    for(bool refresh:{false,true}){reset();AtNode reused=node("foreignReplacement"),generated=node("generated",AI_NODE_SHAPE,"owner");
        universe={&reused,&generated};DPInstancerTranslatorAi v;v.referenceNodePrefix="asset_";
        v.refNodeList={&reused};v.ownedReferenceNodes={&reused};assert(!v.deleteInstancedNodes(refresh));assert(!reused.destroyed&&!generated.destroyed);
        reused.name="asset_replacement";reused.entry.type=AI_NODE_LIGHT;assert(!v.deleteInstancedNodes(refresh));assert(!reused.destroyed&&!generated.destroyed);}
    // Every ordinary/bad_alloc API fault and every allocation failure, including
    // failures after an earlier successful destroy, must leave only live caches.
    int apiCount=0,allocationCount=0;
    for(int kind=-1;kind<3;++kind){const int total=kind<0?1:kind<2?apiCount:allocationCount;
        for(int at=0;at<total;++at){reset();AtNode a=node("asset_a"),b=node("asset_b"),s=node("asset_shader",AI_NODE_SHADER),g=node("generated",AI_NODE_SHAPE,"owner"),f=node("asset_foreign",AI_NODE_SHAPE,"other");
            universe={&a,&b,&s,&g,&f};DPInstancerTranslatorAi v;v.referenceNodePrefix="asset_";
            v.refNodeList={&a,&b};v.ownedReferenceNodes={&a,&b,&s};calls=allocationCalls=0;
            if(kind>=0&&kind<2){faultAt=at;faultKind=kind;}if(kind==2)allocationFaultAt=at;
            try{assert(v.deleteInstancedNodes(true));}catch(...){assert(kind>=0);}
            if(kind<0){apiCount=calls;allocationCount=allocationCalls;}
            faultAt=allocationFaultAt=-1;
            assert(!f.destroyed);for(auto* n:v.refNodeList)assert(!n->destroyed);for(auto* n:v.ownedReferenceNodes)assert(!n->destroyed);
            assert(liveIterators==0);assert(v.deleteInstancedNodes(true));assert(v.refNodeList.empty()&&v.ownedReferenceNodes.empty());}}
}
void deletionDependencies(){
    // A failed consumer stops the entire ordered retirement, including imported
    // references and shaders. Renderer links are explicit model state, not native.
    for(int stage=0;stage<4;++stage)for(int fault=0;fault<4;++fault){reset();
        AtNode child=node("instance",AI_NODE_SHAPE,"owner"),base=node("base"),
            reference=node("asset_reference"),shader=node("asset_shader",AI_NODE_SHADER);
        base.meshTag="owner";child.dependency=&base;base.dependency=&shader;reference.dependency=&shader;
        universe={&shader,&reference,&base,&child};
        DPInstancerTranslatorAi t;t.referenceNodePrefix="asset_";
        t.refNodeList={&reference};t.ownedReferenceNodes={&reference,&shader};
        const std::array<AtNode*,4> ordered{{&child,&base,&reference,&shader}};
        AtNode* failed=ordered[std::size_t(stage)];failed->denyDestroy=fault==0||fault==3;
        failed->destroyFault=fault==1||fault==2?fault:0;loggerFault=fault==3;
        bool threw=false;try{assert(!t.deleteInstancedNodes(true));}catch(...){threw=true;}
        assert(threw==(fault!=0));assert(!liveDependencyDeleted);
        assert(destroyAttemptCount==std::size_t(stage+1));
        for(std::size_t i=0;i<ordered.size();++i)assert(ordered[i]->destroyed==(i<std::size_t(stage)));
        assert(t.referenceNodePrefix=="asset_");
        assert(t.refNodeList.empty()==reference.destroyed);
        assert((std::find(t.ownedReferenceNodes.begin(),t.ownedReferenceNodes.end(),&reference)!=t.ownedReferenceNodes.end())==!reference.destroyed);
        assert(std::find(t.ownedReferenceNodes.begin(),t.ownedReferenceNodes.end(),&shader)!=t.ownedReferenceNodes.end());
        failed->denyDestroy=false;failed->destroyFault=0;loggerFault=false;
        assert(t.deleteInstancedNodes(true));assert(!liveDependencyDeleted);
        assert(t.refNodeList.empty()&&t.ownedReferenceNodes.empty()&&t.referenceNodePrefix.empty());
    }
    reset();AtNode child=node("instance",AI_NODE_SHAPE,"owner"),base=node("base");
    base.meshTag="owner";child.dependency=&base;child.denyDestroy=true;universe={&base,&child};
    DPInstancerTranslatorAi t;assert(!t.deleteInstancedNodes(false));
    assert(!child.destroyed&&!base.destroyed&&!liveDependencyDeleted);
    child.denyDestroy=false;assert(t.deleteInstancedNodes(false));assert(!liveDependencyDeleted);
}
void imports(){
    for(int collision=0;collision<3;++collision){reset();AtNode existing=node("asset_collision",collision==0?AI_NODE_SHAPE:collision==1?AI_NODE_SHADER:AI_NODE_LIGHT);
        universe={&existing};importNodes.clear();DPInstancerTranslatorAi t;assert(!t.refreshReferenceFile());assert(loads==0&&!existing.destroyed&&t.refNodeList.empty());}
    reset();AtNode existing=node("unrelated"),ref=node("asset_ref"),shader=node("asset_shader",AI_NODE_SHADER),extra=node("outside"),shared=node("sharedShader",AI_NODE_SHADER);
    universe={&existing};importNodes={&ref,&shader,&extra,&shared};renameOnLoad=&existing;DPInstancerTranslatorAi t;assert(t.refreshReferenceFile());
    assert(t.refNodeList==std::vector<AtNode*>{&ref});assert(t.ownedReferenceNodes.size()==2U);assert(t.referenceNodePrefix=="asset_");
    reset();assert(t.deleteInstancedNodes(true));assert(ref.destroyed&&shader.destroyed&&!extra.destroyed&&!shared.destroyed&&!existing.destroyed);
    for(int failure=0;failure<5;++failure){reset();AtNode n=node("asset_new");universe.clear();importNodes={&n};DPInstancerTranslatorAi failed;
        if(failure==0)badRead=true;if(failure==1)badPath=true;if(failure==2)missingFile=true;if(failure==3)iteratorFailAt=0;if(failure==4)loadResult=-1;
        assert(!failed.refreshReferenceFile());if(failure<4)assert(loads==0);else {assert(failed.ownedReferenceNodes.size()==1U);reset();assert(failed.deleteInstancedNodes(true));assert(n.destroyed);}assert(liveIterators==0);}
    reset();AtNode shaderOnly=node("asset_shader",AI_NODE_SHADER);universe.clear();importNodes={&shaderOnly};DPInstancerTranslatorAi empty;
    assert(!empty.refreshReferenceFile());assert(empty.refNodeList.empty()&&empty.ownedReferenceNodes.size()==1U);reset();assert(empty.deleteInstancedNodes(true));assert(shaderOnly.destroyed);
    reset();DPInstancerTranslatorAi noPrefix;configuredPrefix.clear();universe.clear();importNodes.clear();assert(!noPrefix.refreshReferenceFile());assert(loads==0);
    reset();AtNode visible=node("asset_verbose"),material=node("asset_material",AI_NODE_SHADER);universe.clear();importNodes={&visible,&material};
    DPInstancerTranslatorAi verbose;verboseList=true;assert(verbose.refreshReferenceFile());reset();assert(verbose.deleteInstancedNodes(true));
    // A post-load iterator failure must not adopt any existing node. Native
    // recovery of imports that cannot be enumerated remains an explicit gate.
    reset();AtNode retained=node("existing"),untracked=node("asset_untracked");universe={&retained};importNodes={&untracked};iteratorFailAt=1;
    DPInstancerTranslatorAi failed;assert(!failed.refreshReferenceFile());assert(loads==1&&failed.ownedReferenceNodes.empty());
    reset();assert(failed.deleteInstancedNodes(true));assert(!retained.destroyed&&!untracked.destroyed);
}
void assReferenceVisibility(){
    for(int failure=0;failure<4;++failure){reset();
        AtNode a=node("asset_a"),b=node("asset_b"),shader=node("asset_shader",AI_NODE_SHADER),foreign=node("outside");
        universe.clear();importNodes={&a,&shader,&b,&foreign};DPInstancerTranslatorAi t;t.shouldRefreshAss=true;
        output.visibility=0U;if(failure==3)loadResult=-1;
        t.assAdmission(failure==1?500001U:1U,failure==2);allocationFaultAt=-1;
        assert(loads==1&&a.visibility==0U&&b.visibility==0U&&!a.destroyed&&!b.destroyed);
        assert(shader.visibility==7U&&foreign.visibility==7U&&!foreign.destroyed);
        assert(t.refNodeList==std::vector<AtNode*>({&a,&b})&&t.ownedReferenceNodes.size()==3U);
        assert(t.shouldRefreshAss==(failure==3));
        assert((reports>0U)==(failure!=0));assert(t.pendingNodes_.empty());
        // Normal inactive cleanup must not revive or retire cached references.
        renderMode=false;t.inputUpdate(&output);assert(a.visibility==0U&&b.visibility==0U);
        assert(!a.destroyed&&!b.destroyed&&!shader.destroyed&&t.refNodeList.size()==2U);
    }
    reset();InputFixture f;f.reference.visibility=7U;renderMode=false;
    f.translator.inputUpdate(&output);assert(f.reference.visibility==0U);f.cache();
}
void ownershipSchemas(){
    for(bool mesh:{false,true})for(int condition=0;condition<8;++condition){reset();AtNode candidate=node("unowned_shape");
        auto& tag=mesh?candidate.meshTag:candidate.objTag;auto& schema=mesh?candidate.meshSchema:candidate.objSchema;
        tag=condition==5?"other":condition==6?"":"owner";schema.present=condition==0?0:1;
        if(condition==1)schema.type=AI_TYPE_INT;if(condition==2)schema.type=AI_TYPE_ARRAY;
        if(condition==3)schema.category=AI_USERDEF_UNIFORM;if(condition==4)schema.category=AI_USERDEF_VARYING;
        universe={&candidate};DPInstancerTranslatorAi t;assert(t.deleteInstancedNodes(false));
        assert(candidate.destroyed==(condition==7));assert(incompatibleTagReads==0U);
    }
    // Invalid instance-tag metadata must not prevent a valid base tag read.
    reset();AtNode base=node("base",AI_NODE_SHAPE,"owner");base.objSchema.type=AI_TYPE_INT;base.meshTag="owner";
    universe={&base};DPInstancerTranslatorAi t;assert(t.deleteInstancedNodes(false));
    assert(base.destroyed&&incompatibleTagReads==0U);
}
#if DOMMY_REPLACEMENT_NAMES_HELPER
void nonemptyNames(){
    using dommy::maya::NameListBudget;using dommy::maya::splitCheckedNonemptyNameList;
    for(const auto& input:std::vector<std::pair<std::string,std::vector<std::string>>>{
        {"",{}},{"   ",{}},{" alpha  beta alpha ",{"alpha","beta","alpha"}},
        {"\t",{"\t"}},{"shape_\xE6\x97\xA5 shape_\xE6\x9C\xAC",{"shape_\xE6\x97\xA5","shape_\xE6\x9C\xAC"}}}){
        reset();NameListBudget budget;std::vector<std::string> result{"old"};
        assert(splitCheckedNonemptyNameList(MString(input.first.c_str()),2U,budget,result)==MS::kSuccess);
        assert(result==input.second&&budget.bytes==input.first.size()&&budget.names==input.second.size());
        assert(budget.workItems==3U*input.first.size()+2U*input.second.size());
    }
    for(int condition=0;condition<10;++condition){reset();NameListBudget budget;
        if(condition==0)budget.bytes=dommy::generation::kMaxGeneratedStringByteCount;
        if(condition==1)budget.names=dommy::generation::kMaxGeneratedStringCount;
        if(condition==2)budget.workItems=dommy::generation::kMaxWorkItemCount;
        if(condition==3)reportedStringLength=dommy::generation::kMaxGeneratedStringByteCount+1U;
        if(condition==4)++(budget.bytes=dommy::generation::kMaxGeneratedStringByteCount);
        if(condition==5)++(budget.names=dommy::generation::kMaxGeneratedStringCount);
        if(condition==6)++(budget.workItems=dommy::generation::kMaxWorkItemCount);
        if(condition==7)budget.names=dommy::generation::kMaxGeneratedStringCount-1U;
        nullStringPointer=condition==9;
        const auto previous=budget;std::vector<std::string> result{"old"};MString input(condition==7?"a b":"a");
        allocationCalls=0;
        assert(splitCheckedNonemptyNameList(input,condition==8?std::numeric_limits<std::size_t>::max():1U,budget,result)!=MS::kSuccess);
        assert(allocationCalls==0&&result==std::vector<std::string>{"old"});
        assert(budget.bytes==previous.bytes&&budget.names==previous.names&&budget.workItems==previous.workItems);
    }
    reset();NameListBudget exact;exact.bytes=dommy::generation::kMaxGeneratedStringByteCount-3U;
    exact.names=dommy::generation::kMaxGeneratedStringCount-2U;exact.workItems=dommy::generation::kMaxWorkItemCount-13U;
    std::vector<std::string> result;assert(splitCheckedNonemptyNameList(MString("a b"),2U,exact,result)==MS::kSuccess);
    assert(exact.bytes==dommy::generation::kMaxGeneratedStringByteCount&&exact.names==dommy::generation::kMaxGeneratedStringCount&&
           exact.workItems==dommy::generation::kMaxWorkItemCount);
    reset();NameListBudget emptyBudget;std::vector<std::string> emptyNames{"old"};
    assert(splitCheckedNonemptyNameList(MString(),std::numeric_limits<std::size_t>::max(),emptyBudget,emptyNames)==MS::kSuccess);
    assert(emptyNames.empty()&&emptyBudget.bytes==0U&&emptyBudget.names==0U&&emptyBudget.workItems==0U);
    reset();NameListBudget apiBaseline;std::vector<std::string> apiNames;calls=0;
    assert(splitCheckedNonemptyNameList(MString("name"),1U,apiBaseline,apiNames)==MS::kSuccess);
    const int apiCount=calls;
    for(int kind=0;kind<2;++kind)for(int index=0;index<apiCount;++index){reset();
        NameListBudget budget;std::vector<std::string> names{"old"};calls=0;faultAt=index;faultKind=kind;bool threw=false;
        try{splitCheckedNonemptyNameList(MString("name"),1U,budget,names);}catch(const std::exception&){threw=true;}
        faultAt=-1;assert(threw&&budget.bytes==0U&&budget.names==0U&&budget.workItems==0U&&names==std::vector<std::string>{"old"});
        assert(splitCheckedNonemptyNameList(MString("name"),1U,budget,names)==MS::kSuccess);
    }
    // Every staging allocation failure retains the caller's output and budget.
    const MString input("long_first_name_abcdefghijklmnopqrstuvwxyz long_second_name_abcdefghijklmnopqrstuvwxyz");
    reset();NameListBudget baseline;allocationCalls=0;assert(splitCheckedNonemptyNameList(input,1U,baseline,result)==MS::kSuccess);
    const int total=allocationCalls;
    for(int index=0;index<total;++index){reset();NameListBudget budget;std::vector<std::string> outputNames{"old"};
        allocationCalls=0;allocationFaultAt=index;bool threw=false;
        try{splitCheckedNonemptyNameList(input,1U,budget,outputNames);}catch(const std::bad_alloc&){threw=true;}
        allocationFaultAt=-1;assert(threw&&budget.bytes==0U&&budget.names==0U&&budget.workItems==0U&&outputNames==std::vector<std::string>{"old"});
        assert(splitCheckedNonemptyNameList(input,1U,budget,outputNames)==MS::kSuccess);
    }
}
#endif
void reporting(){std::set_terminate([](){std::_Exit(86);});
    for(int error=0;error<3;++error)for(int hide=0;hide<2;++hide){reset();std::exception_ptr e;
        try{if(error==1)throw std::runtime_error("original");if(error==2)throw 42;}catch(...){e=std::current_exception();}
        loggerFault=true;hideFault=hide;dommy::arnold::reportTranslatorException("test",&output,e);assert(reports>=1U);}
}
int main(int argc,char** argv){const std::string phase=argc>1?argv[1]:"all";
    if(phase=="all"||phase=="report")reporting();
    if(phase=="all"||phase=="callbacks"){
#include "lifecycle_cases.inc"
    }
    if(phase=="all"||phase=="dependencies")deletionDependencies();
    if(phase=="all"||phase=="input")inputModes();
    if(phase=="all"||phase=="inputfail")inputFailures();
    if(phase=="all"||phase=="inactivesettings")inactiveSettings();
    if(phase=="all"||phase=="inactive")inactivePending();
    if(phase=="all"||phase=="sidedness")sidednessFlags();
    if(phase=="all"||phase=="assvisibility")assReferenceVisibility();
    if(phase=="all"||phase=="schema")ownershipSchemas();
#if DOMMY_REPLACEMENT_NAMES_HELPER
    if(phase=="all"||phase=="names")nonemptyNames();
#endif
    if(phase=="all"||phase=="delete")deletion();if(phase=="all"||phase=="import")imports();
    std::cout<<"Arnold callback/lifecycle phase passed: "<<phase<<'\n';
}
#endif
