#include <algorithm>
#include <array>
#include <cassert>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <new>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>
#include "common/utility/DGenerationLimits.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldRayFlags.h"
#ifdef DOMMY_SDK_CHECK
#include <maya/MMatrix.h>
#include <maya/MString.h>
#include <maya/MUuid.h>
#endif
struct AtString {const char* value;explicit AtString(const char* v):value(v){}
    operator bool()const{return value&&*value;}const char* c_str()const{return value;}};
enum {AI_NODE_SHAPE=1,AI_NODE_SHADER=2,AI_NODE_LIGHT=4,AI_NODE_ALL=7};
enum {AI_TYPE_STRING=5,AI_USERDEF_CONSTANT=1};
struct AtUserParamEntry{int type=AI_TYPE_STRING,category=AI_USERDEF_CONSTANT;};
struct AtNode;struct AtNodeEntry{AtNode* node=nullptr;};
struct AtNode{std::string name,owner,type;int category=AI_NODE_SHAPE;AtNode* link=nullptr;AtNode* shader=nullptr;bool alive=true,denyDestroy=false;AtUserParamEntry tag;
    AtNodeEntry entry;bool missingEntry=false,missingTypeName=false;int destroyFault=0;};
struct AtNodeIterator{std::vector<AtNode*> values;std::size_t index=0;};
static std::vector<AtNode*> universe;static std::vector<std::unique_ptr<AtNode>> storage;
static std::vector<std::string> destructionOrder;static int creationCalls=0,failCreateAt=-1,renameCalls=0,failRenameAt=-1,throwKind=0;
static bool dependencyRetiredFirst=false,loggerThrows=false,failIterator=false;
static bool denyAllocation=false;
static int allocationFaultAt=-1,allocationCalls=0;
void* operator new(std::size_t bytes){if(denyAllocation||allocationCalls++==allocationFaultAt)throw std::bad_alloc();
    if(void* value=std::malloc(bytes?bytes:1U))return value;throw std::bad_alloc();}
void operator delete(void* value)noexcept{std::free(value);}
static int destroyException=0,readFaultAt=-1,readCalls=0;
static int roleFaultAt=-1,roleCalls=0;
void roleApi(){if(roleCalls++==roleFaultAt){if(throwKind)throw std::bad_alloc();throw std::runtime_error("role seam");}}
const AtNodeEntry* AiNodeGetNodeEntry(AtNode* n){roleApi();assert(n&&n->alive);if(n->missingEntry)return nullptr;n->entry.node=n;return &n->entry;}
int AiNodeEntryGetType(const AtNodeEntry* e){roleApi();return e->node->category;}
const char* AiNodeEntryGetName(const AtNodeEntry* e){roleApi();return e->node->missingTypeName?nullptr:e->node->type.c_str();}
AtNodeIterator* AiUniverseGetNodeIterator(int mask){if(failIterator)return nullptr;std::unique_ptr<AtNodeIterator> i(new AtNodeIterator);
    for(auto* n:universe)if(n&&n->alive&&(n->category&mask))i->values.push_back(n);return i.release();}
bool AiNodeIteratorFinished(AtNodeIterator* i){return i->index==i->values.size();}
AtNode* AiNodeIteratorGetNext(AtNodeIterator* i){return i->values.at(i->index++);}
void AiNodeIteratorDestroy(AtNodeIterator* i){delete i;}
static int nameCalls=0,nameFaultAt=-1,nameFaultKind=0;
static bool nullName=false;
const char* AiNodeGetName(AtNode* n){if(nameCalls++==nameFaultAt){if(nameFaultKind)throw std::bad_alloc();throw std::runtime_error("name seam");}
    return nullName?nullptr:n->name.c_str();}
const AtUserParamEntry* AiNodeLookUpUserParameter(AtNode* n,const char*){return n->owner.empty()?nullptr:&n->tag;}
int AiUserParamGetType(const AtUserParamEntry* p){return p->type;}
int AiUserParamGetCategory(const AtUserParamEntry* p){return p->category;}
AtString AiNodeGetStr(AtNode* n,const char*){if(readCalls++==readFaultAt)throw std::bad_alloc();return AtString(n->owner.c_str());}
void AiMsgError(const char*,...){if(loggerThrows)throw 42;}void AiMsgWarning(const char*,...){if(loggerThrows)throw 42;}
bool AiNodeDestroy(AtNode* n){assert(n&&n->alive);destructionOrder.push_back(n->type);
    if(destroyException==1||n->destroyFault==1)throw std::runtime_error("destroy seam");if(destroyException==2||n->destroyFault==2)throw std::bad_alloc();
    if(n->denyDestroy)return false;
    for(auto* other:universe)if(other->alive&&other->link==n)dependencyRetiredFirst=true;
    n->alive=false;return true;}
AtNode* make(const char* type){storage.emplace_back(new AtNode);auto* n=storage.back().get();n->type=type;n->name=type;
    n->category=std::string(type)=="image"||std::string(type)=="gobo"?AI_NODE_SHADER:std::string(type)=="light"?AI_NODE_LIGHT:AI_NODE_SHAPE;
    universe.push_back(n);return n;}
AtNode* AiNode(const char* type){if(creationCalls++==failCreateAt)return nullptr;return make(type);}
void AiNodeSetStr(AtNode* n,const char* parameter,const char* value){
    if(std::string(parameter)=="name"){if(renameCalls++==failRenameAt){if(throwKind)throw std::bad_alloc();throw std::runtime_error("rename seam");}n->name=value;}
    else n->owner=value;}
void AiNodeSetByte(AtNode*,const AtString&,int){}void AiNodeSetBool(AtNode*,const AtString&,bool){}
void AiNodeSetPtr(AtNode* n,const char* parameter,AtNode* v){if(std::string(parameter)=="node")n->link=v;
    if(std::string(parameter)=="shader")n->shader=v;}
void AiNodeSetPtr(AtNode* n,const AtString& p,AtNode* v){AiNodeSetPtr(n,p.c_str(),v);}
struct AtMatrix{};
#ifndef DOMMY_SDK_CHECK
struct MMatrix{};
#endif
struct LazyMatrices{MMatrix operator[](std::size_t)const{return {};}};
void ConvertMatrix(AtMatrix&,MMatrix){}void AiNodeSetMatrix(AtNode*,const char*,const AtMatrix&){}
#ifndef DOMMY_SDK_CHECK
struct MString{const char* v;const char* asChar()const{return v;}};struct MUuid{MString asString()const{return {"owner"};}};
#endif
struct NameArray{unsigned length()const{return 0;}};
struct Points{std::size_t count;struct Iterator{std::size_t i;bool operator!=(Iterator b)const{return i!=b.i;}void operator++(){++i;}};
    Iterator beginPt()const{return {0};}Iterator endPt()const{return {count};}};
int ComputeVisibility(){return 255;}
void instanceAssignDPAttribute(const Points*,const NameArray&,AtNode*,std::size_t){}
namespace dommy{namespace arnold{
void requireOwnershipTag(AtNode*,const char*){}
#include "node_batch_resources.inc"
}}
const char* const kLightOwnerTag="dlightTag";
#define INST_DEBUG(...) ((void)0)
enum {DP_INST_COPY_MESH,DP_INST_COPY_ASS};
struct ReplaceShaderData{AtNode* shaderNode=nullptr;std::vector<std::string> shapeNameList;};
static std::vector<ReplaceShaderData> replacementRows;
static std::vector<std::string> referenceNames;
static bool overrideShader=false;
static AtNode* commonShader=nullptr;
#include "node_batch_stages.inc"
std::size_t live(){std::size_t count=0;for(auto* n:universe)if(n->alive)++count;return count;}
void checkBudgets(){using namespace dommy::arnold;NodeGenerationLayout plan;
    assert(checkedNodeGenerationLayout(0,0,0,0,0,plan)&&plan.nodeCount==0);
    assert(checkedNodeGenerationLayout(2,3,4,5,2,plan)&&plan.instanceCount==6&&plan.nodeCount==8);
    assert(checkedNodeGenerationLayout(kMaxArnoldGeneratedNodes,1,0,0,0,plan));
    assert(!checkedNodeGenerationLayout(kMaxArnoldGeneratedNodes,1,0,0,1,plan)&&plan.nodeCount==0);
    assert(!checkedNodeGenerationLayout(1000,2000,0,0,0,plan));
    assert(!checkedNodeGenerationLayout(1,0,0,0,0,plan));
    assert(!checkedNodeGenerationLayout(std::numeric_limits<std::size_t>::max(),2,0,0,0,plan));
    assert(!checkedNodeGenerationLayout(2,1,std::numeric_limits<std::size_t>::max(),0,0,plan));
    assert(!checkedNodeGenerationLayout(1,2,40000000U,0,0,plan)); // combined validation + export work
    assert(!checkedNodeGenerationLayout(1,2,0,50000000U,0,plan));
    assert(!checkedNodeGenerationLayout(0,0,0,0,std::numeric_limits<std::size_t>::max(),plan));
}
void retryBatches(){for(int error=0;error<3;++error){dommy::arnold::NodeBatch batch;batch.reserve(2);
    AtNode* base=batch.create("polymesh");AtNode* child=batch.create("ginstance");child->link=base;
    child->denyDestroy=error==0;destroyException=error;loggerThrows=true;
    {dommy::arnold::NodeBatchScope failure(batch);}
    assert(!batch.empty()&&base->alive&&child->alive);assert(!dependencyRetiredFirst);
    child->denyDestroy=false;destroyException=0;loggerThrows=false;assert(batch.retire());assert(!base->alive&&!child->alive&&batch.empty());}
    {dommy::arnold::NodeBatch persistent;persistent.reserve(2);AtNode* base=persistent.create("polymesh");AtNode* child=persistent.create("ginstance");child->link=base;
        {dommy::arnold::NodeBatchScope failure(persistent);}
        assert(persistent.empty()&&!base->alive&&!child->alive&&!dependencyRetiredFirst);}
    dommy::arnold::NodeBatch capacity;capacity.reserve(1);capacity.create("points");const int before=creationCalls;
    bool denied=false;try{capacity.create("points");}catch(const std::length_error&){denied=true;}assert(denied&&creationCalls==before);
    assert(capacity.retire());
    AtNode* a=make("a");AtNode* b=make("b");dommy::arnold::NodeOwner left(a),right(b);a->denyDestroy=true;
    bool threw=false;try{left=std::move(right);}catch(const std::runtime_error&){threw=true;}
    assert(threw&&left.get()==a&&right.get()==b);a->denyDestroy=false;left=std::move(right);assert(!right&&left.get()==b);assert(left.retire());
}
void reservations(){dommy::arnold::NodeBatch batch;const int previous=creationCalls;
    denyAllocation=true;bool rejected=false;try{batch.reserve(2);}catch(const std::bad_alloc&){rejected=true;}
    denyAllocation=false;assert(rejected&&creationCalls==previous&&batch.empty());
    batch.reserve(2);dommy::arnold::NodeOwner owner(make("polymesh"));AtNode* value=owner.get();
    denyAllocation=true;batch.adopt(std::move(owner));denyAllocation=false;
    assert(!owner&&value->alive);assert(batch.retire()&&!value->alive);
}
void oldLightRetirement(){
    std::array<int,3> permutation{{0,1,2}};
    do{
        AtNode* image=make("image"),*gobo=make("gobo"),*light=make("light");
        gobo->link=image;light->link=gobo;image->owner=gobo->owner=light->owner="owner";
        const std::array<AtNode*,3> nodes{{image,gobo,light}};
        std::vector<AtNode*> ordered;for(int i:permutation)ordered.push_back(nodes[std::size_t(i)]);
        destructionOrder.clear();dependencyRetiredFirst=false;
        assert(destroyGeneratedNodes(ordered)&&!dependencyRetiredFirst);
        assert((destructionOrder==std::vector<std::string>{"light","gobo","image"}));
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    for(int failedStage=0;failedStage<3;++failedStage)for(int fault=0;fault<4;++fault){
        AtNode* image=make("image"),*gobo=make("gobo"),*a=make("light"),*b=make("light");
        gobo->link=image;a->link=b->link=gobo;image->owner=gobo->owner=a->owner=b->owner="owner";
        AtNode* failed=failedStage==0?a:failedStage==1?gobo:image;
        failed->denyDestroy=fault==0||fault==3;failed->destroyFault=fault==1||fault==2?fault:0;
        loggerThrows=fault==3;dependencyRetiredFirst=false;destructionOrder.clear();
        bool threw=false;try{assert(!destroyGeneratedNodes({image,gobo,a,b}));}catch(...){threw=true;}
        assert(threw==(fault!=0)&&!dependencyRetiredFirst&&failed->alive);
        assert(destructionOrder.back()==failed->type);
        if(failedStage==0)assert(gobo->alive&&image->alive);
        if(failedStage==1)assert(!a->alive&&!b->alive&&image->alive);
        if(failedStage==2)assert(!a->alive&&!b->alive&&!gobo->alive);
        failed->denyDestroy=false;failed->destroyFault=0;loggerThrows=false;
        // Retry re-enumerates live owned nodes; it never reuses stale pointers.
        std::vector<AtNode*> remaining;assert(collectGeneratedNodes("owner",remaining));
        assert(destroyGeneratedNodes(remaining)&&!dependencyRetiredFirst);
        assert(!image->alive&&!gobo->alive&&!a->alive&&!b->alive);
    }
    for(int invalid=0;invalid<5;++invalid){AtNode* light=make("light"),*unknown=make("image");
        if(invalid==0)unknown->missingEntry=true;
        if(invalid==1)unknown->missingTypeName=true;
        if(invalid==2)unknown->type="custom_shader";
        if(invalid==3)unknown->category=AI_NODE_SHAPE;
        if(invalid==4)unknown->type.clear();
        assert(!destroyGeneratedNodes({light,unknown})&&light->alive&&unknown->alive);
        unknown->missingEntry=unknown->missingTypeName=false;unknown->type="image";unknown->category=AI_NODE_SHADER;
        assert(destroyGeneratedNodes({light,unknown}));
    }
    AtNode* light=make("light");assert(!destroyGeneratedNodes({light,nullptr})&&light->alive);
    assert(destroyGeneratedNodes({light,light}));
    // Every classification/API fault and initial plan allocation precedes deletion.
    int roleCount=0;
    for(int kind=-1;kind<2;++kind){const int total=kind<0?1:roleCount;
        for(int at=0;at<total;++at){AtNode* image=make("image"),*gobo=make("gobo"),*light=make("light");
            gobo->link=image;light->link=gobo;roleCalls=0;roleFaultAt=kind<0?-1:at;throwKind=kind;
            bool threw=false;try{assert(destroyGeneratedNodes({image,gobo,light}));}catch(const std::exception&){threw=true;}
            if(kind<0)roleCount=roleCalls;
            else assert(threw&&image->alive&&gobo->alive&&light->alive);
            roleFaultAt=-1;if(threw)assert(destroyGeneratedNodes({image,gobo,light}));
        }
    }
    AtNode* image=make("image");const std::vector<AtNode*> plan{image};denyAllocation=true;
    bool rejected=false;try{(void)destroyGeneratedNodes(plan);}catch(const std::bad_alloc&){rejected=true;}
    denyAllocation=false;assert(rejected&&image->alive);assert(destroyGeneratedNodes(plan));
    int allocations=0;destructionOrder.reserve(4U);
    for(int at=-1;at<allocations;++at){AtNode* image=make("image"),*gobo=make("gobo"),*light=make("light");
        gobo->link=image;light->link=gobo;const std::vector<AtNode*> plan{image,gobo,light};
        destructionOrder.clear();allocationCalls=0;allocationFaultAt=at;
        bool threw=false;try{assert(destroyGeneratedNodes(plan));}catch(const std::bad_alloc&){threw=true;}
        if(at<0)allocations=allocationCalls;
        else assert(threw&&destructionOrder.empty()&&image->alive&&gobo->alive&&light->alive);
        allocationFaultAt=-1;if(threw)assert(destroyGeneratedNodes(plan));
    }
    assert(!dependencyRetiredFirst);
}
void shaderPlans(){
    AtNode* first=make("image"),*second=make("image"),*common=make("image");
    ReplaceShaderData a,b;a.shaderNode=first;a.shapeNameList={"reference_0","reference_0"};
    b.shaderNode=second;b.shapeNameList={"reference_0","reference_1"};replacementRows={a,b};
    for(bool overrideValue:{false,true}){
        overrideShader=overrideValue;commonShader=common;nameCalls=0;
        const std::size_t before=universe.size();stage(DP_INST_COPY_ASS,4U,3U);
        std::size_t instances=0U;
        for(std::size_t i=before;i<universe.size();++i){AtNode* n=universe[i];if(n->type!="ginstance")continue;
            assert(n->shader==(n->link->name=="reference_0"?first:n->link->name=="reference_1"?second:overrideValue?common:nullptr));
            ++instances;
        }
        assert(instances==12U&&nameCalls==3); // One query per reference, not per point.
    }
    replacementRows.clear();nameCalls=0;stage(DP_INST_COPY_ASS,4U,3U);assert(nameCalls==0);
#if DOMMY_SHADER_PLAN
    using dommy::maya::NameListBudget;
    AtNode* r0=make("reference"),*r1=make("reference");r0->name="alpha";r1->name="beta";
    const std::vector<AtNode*> refs{r0,r1};
    a.shapeNameList={"alpha"};b.shapeNameList={"alpha","beta"};
    const std::vector<ReplaceShaderData> rows{a,b};
    NameListBudget baseline;std::vector<AtNode*> plan;
    nameCalls=allocationCalls=0;
    assert(buildReplacementShaderPlan(refs,rows,baseline,plan));
    const int allocations=allocationCalls;
    assert(plan==std::vector<AtNode*>({first,second})&&nameCalls==2&&baseline.bytes==9U);
    // 2 references + 9 scanned bytes + 3 rows + 4 tokens + 9 compared bytes.
    assert(baseline.workItems==27U&&baseline.names==0U);
    for(int condition=0;condition<8;++condition){
        NameListBudget bounded;
        if(condition==0)bounded.bytes=dommy::generation::kMaxGeneratedStringByteCount-8U;
        if(condition==1)bounded.workItems=dommy::generation::kMaxWorkItemCount-26U;
        if(condition==2)++(bounded.bytes=dommy::generation::kMaxGeneratedStringByteCount);
        if(condition==3)++(bounded.names=dommy::generation::kMaxGeneratedStringCount);
        if(condition==4)++(bounded.workItems=dommy::generation::kMaxWorkItemCount);
        std::vector<AtNode*> output{common};auto input=refs;
        if(condition==5)input[0]=nullptr;
        nullName=condition==6;
        if(condition==7)bounded.workItems=dommy::generation::kMaxWorkItemCount;
        const auto before=bounded;
        assert(!buildReplacementShaderPlan(input,rows,bounded,output));
        nullName=false;assert(output==std::vector<AtNode*>{common});
        assert(bounded.bytes==before.bytes&&bounded.names==before.names&&bounded.workItems==before.workItems);
    }
    NameListBudget exact;exact.bytes=dommy::generation::kMaxGeneratedStringByteCount-9U;
    exact.workItems=dommy::generation::kMaxWorkItemCount-27U;plan={common};
    assert(buildReplacementShaderPlan(refs,rows,exact,plan)&&exact.bytes==dommy::generation::kMaxGeneratedStringByteCount&&
           exact.workItems==dommy::generation::kMaxWorkItemCount);
    for(int kind=0;kind<2;++kind)for(int at=0;at<2;++at){
        NameListBudget budget;std::vector<AtNode*> output{common};nameCalls=0;nameFaultAt=at;nameFaultKind=kind;
        bool threw=false;try{buildReplacementShaderPlan(refs,rows,budget,output);}catch(const std::exception&){threw=true;}
        nameFaultAt=-1;assert(threw&&output==std::vector<AtNode*>{common}&&budget.bytes==0U&&budget.workItems==0U);
        assert(buildReplacementShaderPlan(refs,rows,budget,output));
    }
    for(int at=0;at<allocations;++at){NameListBudget budget;std::vector<AtNode*> output{common};
        allocationCalls=0;allocationFaultAt=at;bool threw=false;
        try{buildReplacementShaderPlan(refs,rows,budget,output);}catch(const std::bad_alloc&){threw=true;}
        allocationFaultAt=-1;assert(threw&&output==std::vector<AtNode*>{common}&&budget.bytes==0U&&budget.workItems==0U);
        assert(buildReplacementShaderPlan(refs,rows,budget,output));
    }
    // A first matching row with no shader still suppresses later rows. Native
    // table admission normally omits such rows; keep the selected-body policy.
    a.shaderNode=nullptr;NameListBudget budget;plan.clear();
    assert(buildReplacementShaderPlan(refs,{a,b},budget,plan)&&plan[0]==nullptr&&plan[1]==second);
    // Long equal-length nonmatches are charged by bytes, not just token count.
    r0->name=std::string(4096U,'x');a.shapeNameList={r0->name};a.shapeNameList[0].back()='y';
    budget={};budget.workItems=dommy::generation::kMaxWorkItemCount-8193U;plan={common};
    assert(!buildReplacementShaderPlan({r0},{a},budget,plan)&&plan==std::vector<AtNode*>{common});
    r0->name.clear();a.shapeNameList={""};a.shaderNode=first;budget={};plan.clear();
    assert(buildReplacementShaderPlan({r0},{a},budget,plan)&&plan==std::vector<AtNode*>{first});
    budget={};plan={common};nameCalls=0;
    assert(buildReplacementShaderPlan({nullptr},{},budget,plan)&&plan.empty()&&nameCalls==0);
    replacementRows={b};nullName=true;const int beforeCreate=creationCalls;
    stage(DP_INST_COPY_ASS,2U,1U);nullName=false;
    assert(creationCalls==beforeCreate); // Actual admission failure never generates nodes.
#endif
}
int main(int argc,char** argv){const std::string mode=argc>1?argv[1]:"collect";
    if(mode=="collect"){
        AtNode* owned=make("light");owned->name="dommy_dlight_owner_light";owned->owner="owner";
        AtNode* foreign=make("light");foreign->name="dommy_dlight_owner_foreign";foreign->owner="otherOwner";
        AtNode* unrelated=make("image");unrelated->name="dommy_dlight_owner_userShader";
        AtNode* shape=make("unrelatedShape");shape->name="dommy_dlight_owner_userShape";
        AtNode* wrong=make("image");wrong->owner="owner";wrong->tag.type=0;
        AtNode* wrongCategory=make("light");wrongCategory->owner="owner";wrongCategory->tag.category=0;
        std::vector<AtNode*> collected;assert(collectGeneratedNodes("owner",collected));
        assert(collected.size()==1U);assert(destroyGeneratedNodes(collected));assert(foreign->alive&&unrelated->alive&&shape->alive);std::cout<<"collected="<<collected.size()<<" foreign_deleted="<<!foreign->alive
          <<" unrelated_shader_deleted="<<!unrelated->alive<<" unrelated_shape_deleted="<<!shape->alive<<'\n';
    }else if(mode=="collectfail"){
        AtNode* old=make("old");AtNode* owned=make("light");owned->owner="owner";std::vector<AtNode*> values{old};
        failIterator=true;assert(!collectGeneratedNodes("owner",values)&&values==std::vector<AtNode*>{old});failIterator=false;
        readCalls=0;readFaultAt=0;try{collectGeneratedNodes("owner",values);assert(false);}catch(const std::bad_alloc&){}
        assert(values==std::vector<AtNode*>{old});readFaultAt=-1;universe.push_back(owned);
        assert(collectGeneratedNodes("owner",values)&&values==std::vector<AtNode*>{owned});
    }else if(mode=="oldretire"){oldLightRetirement();
    }else if(mode=="budget"){checkBudgets();
    }else if(mode=="shaderplan"){shaderPlans();
    }else if(mode=="retry"){retryBatches();
    }else if(mode=="reservation"){reservations();
    }else if(mode=="fanout"){
        std::size_t count=0;const bool transforms=dommy::generation::checkedProduct(1000U,1U,dommy::generation::kMaxGeneratedTransformCount,count);
        const bool admissibleFanout=dommy::generation::checkedProduct(1000U,2000U,dommy::generation::kMaxGeneratedTransformCount,count);
        failCreateAt=2;stage(DP_INST_COPY_ASS,1000U,2000U);
        assert(creationCalls==0);std::cout<<"transform_count_accepted="<<transforms<<" recommended_fanout_accepted="<<admissibleFanout<<" renderer_create_calls="<<creationCalls<<'\n';
    }else if(mode=="meshorder"){
        failCreateAt=1;stage(DP_INST_COPY_MESH,2U);
        assert(!dependencyRetiredFirst);std::cout<<"dependency_retired_while_instance_alive="<<dependencyRetiredFirst<<" order=";
        for(const auto& type:destructionOrder)std::cout<<type<<',';std::cout<<'\n';
    }else if(mode=="reset"){
        AtNode* old=make("old");old->denyDestroy=true;
        {dommy::arnold::NodeOwner owner(old),replacementOwner(make("replacement"));AtNode* replacement=replacementOwner.get();assert(!owner.reset(replacement));
         assert(owner.get()==old&&old->alive);std::cout<<"old_alive="<<old->alive<<" old_still_owned="<<(owner.get()==old)<<" replacement_owned="<<(owner.get()==replacement)<<'\n';}
        std::cout<<"remaining_after_owner_destruction="<<live()<<'\n';
    }else if(mode=="commit"){
        failRenameAt=3;throwKind=argc>2?std::atoi(argv[2]):0;
        try{commitStage();}catch(...){std::cout<<"caught_rename_failure=1 ";}
        assert(live()==0U);std::cout<<"remaining_nodes="<<live()<<" surviving_light=";
        for(auto* n:universe)if(n->alive&&n->type=="light")std::cout<<n->name;
        std::cout<<'\n';
    }else if(mode=="commitall"){
        for(int kind=0;kind<2;++kind)for(int at=0;at<4;++at){renameCalls=0;failRenameAt=at;throwKind=kind;
            try{commitStage();assert(false);}catch(const std::exception&){}assert(live()==0U&&!dependencyRetiredFirst);}
        failRenameAt=-1;commitStage();assert(live()==4U&&!dependencyRetiredFirst);
        for(auto it=universe.rbegin();it!=universe.rend();++it)if((*it)->alive)assert(AiNodeDestroy(*it));
    }
}
