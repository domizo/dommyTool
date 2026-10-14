// The real name-list helper, caller statements, and velocity commit block.
// String/renderer failure injection is not a native Maya/MtoA simulation.
#include <maya/MStatus.h>
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include <cassert>
#include <cstring>
#include <exception>
#include <map>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef DOMMY_SDK_CHECK
#include <maya/MString.h>
#include <maya/MStringArray.h>
#else
MStatus splitStatus=MS::kSuccess;
unsigned splitCalls=0,reportedCount=0,reportedBytes=0;
bool nullNameView=false,retainEmptyFields=false;
int failSplitAt=-1,throwSplitAt=-1,throwSplitKind=0;
struct MStringArray;
struct MString {
    std::string text;
    MString(const char* value=""):text(value){}
    const char* asChar()const{return nullNameView?nullptr:text.c_str();}
    unsigned length()const{return reportedBytes?reportedBytes:static_cast<unsigned>(text.size());}
    MStatus split(char,MStringArray&)const;
};
struct MStringArray {
    std::vector<MString> values;
    unsigned length()const{return reportedCount?reportedCount:static_cast<unsigned>(values.size());}
    const MString& operator[](unsigned i)const{return values.at(i);}
};
MStatus MString::split(char delimiter,MStringArray& output)const {
    const int index=int(splitCalls++);
    if(index==throwSplitAt){if(throwSplitKind)throw std::bad_alloc();throw std::runtime_error("split failure");}
    std::size_t start=0U;
    for(std::size_t end=0U;end<=text.size();++end)if(end==text.size()||text[end]==delimiter){
        if(end!=start||(retainEmptyFields&&!text.empty()))output.values.emplace_back(text.substr(start,end-start).c_str());
        start=end+1U;
    }
    return failSplitAt<0 || index==failSplitAt ? splitStatus : MS::kSuccess;
}
#endif
struct CountingPoints{std::size_t count;std::size_t size()const{return count;}};
struct AtString{const char* value;explicit AtString(const char* v):value(v){}};
struct AtNode{
    unsigned char visibility=7;
    std::map<std::string,std::vector<std::string>> arrays;
    std::map<std::string,float> scalars;
};
struct AtArray{std::vector<std::string> values;};
bool accepted=false,failAllocation=false;
unsigned cacheCommits=0;
int throwAt=-1,throwKind=0,liveArrays=0,failElementAt=-1,elementCalls=0;
std::vector<std::string> calls;
void api(const std::string& name){calls.push_back(name);if(int(calls.size())-1==throwAt){
    if(throwKind)throw std::bad_alloc();throw std::runtime_error("injected renderer failure");}}
void AiMsgError(const char*,...){}
void AiNodeSetByte(AtNode* node,AtString,unsigned char value){node->visibility=value;}
const int AI_TYPE_STRING=1;
AtArray* AiArrayAllocate(unsigned count,unsigned char,int){api("allocate");if(failAllocation)return nullptr;
    auto* result=new AtArray{std::vector<std::string>(count)};++liveArrays;return result;}
void AiArrayDestroy(AtArray* array){--liveArrays;delete array;}
bool AiArraySetStr(AtArray* array,unsigned i,const char* value){api("array.string");if(elementCalls++==failElementAt)return false;array->values.at(i)=value;return true;}
void AiNodeSetArray(AtNode* node,const char* name,AtArray* value){
    // This boundary consumes an array at entry, including injected exceptions.
    // It is not a claim about native renderer exception/ownership semantics.
    struct Delete{void operator()(AtArray* p)const{AiArrayDestroy(p);}};
    std::unique_ptr<AtArray,Delete> owner(value);api("set."+std::string(name));node->arrays[name]=value->values;
}
void AiNodeSetFlt(AtNode* node,const char* name,float value){api("set."+std::string(name));node->scalars[name]=value;}
void AiNodeResetParameter(AtNode* node,const char* name){api("reset."+std::string(name));node->arrays.erase(name);node->scalars.erase(name);}
namespace dommy {namespace arnold {void requireVelocityGridsParameter(AtNode*){api("declaration");}}}
#include "renderer_list_methods.inc"

#ifndef DOMMY_SDK_CHECK
void checkList(MStatus(*stage)(const MString&,std::size_t,dommy::maya::NameListBudget&,MStringArray&),bool repeated){
    using dommy::maya::NameListBudget;
    for(const char* input:{"","alpha"," alpha  alpha beta "}){
        NameListBudget budget;MStringArray output;splitStatus=MS::kSuccess;
        assert(stage(MString(input),2U,budget,output));
        if(*input)assert(output.length()>0U);
        const auto before=budget;
        for(auto status:{MS::kFailure,MS::kInvalidParameter}){
            MStringArray partial;splitStatus=status;
            assert(stage(MString(input),2U,budget,partial)!=MS::kSuccess);
            assert(budget.bytes==before.bytes&&budget.names==before.names&&budget.workItems==before.workItems);
        }
        splitStatus=MS::kSuccess;MStringArray retry;assert(stage(MString(input),2U,budget,retry));
    }
    NameListBudget budget;MStringArray output;splitCalls=0;
    reportedBytes=dommy::generation::kMaxGeneratedStringByteCount+1U;
    assert(!stage(MString("alpha"),1U,budget,output)&&splitCalls==0);reportedBytes=0;
    reportedCount=dommy::generation::kMaxGeneratedStringCount+1U;
    assert(!stage(MString("alpha"),1U,budget,output));reportedCount=0;
    output={};
    budget.bytes=dommy::generation::kMaxGeneratedStringByteCount;
    assert(!stage(MString("alpha"),1U,budget,output));budget={};
    output={};
    budget.names=dommy::generation::kMaxGeneratedStringCount;
    assert(!stage(MString("alpha"),1U,budget,output));budget={};
    output={};
    budget.workItems=dommy::generation::kMaxWorkItemCount;
    assert(!stage(MString("alpha"),1U,budget,output));budget={};
    output={};
    if(repeated)assert(!stage(MString("alpha"),dommy::generation::kMaxWorkItemCount+1U,budget,output));

    // Rejection must precede the SDK split, not merely discard its allocation.
    // These tiny inputs exercise exhausted/aggregate limits without huge arrays.
    for(int condition=0;condition<6;++condition){
        NameListBudget bounded;
        if(condition==0)bounded.names=dommy::generation::kMaxGeneratedStringCount;
        if(condition==1)bounded.workItems=dommy::generation::kMaxWorkItemCount;
        if(condition==2)bounded.names=dommy::generation::kMaxGeneratedStringCount-1U;
        if(condition==3)bounded.workItems=dommy::generation::kMaxWorkItemCount-5U;
        if(condition==4)++(bounded.names=dommy::generation::kMaxGeneratedStringCount);
        if(condition==5)++(bounded.workItems=dommy::generation::kMaxWorkItemCount);
        const auto before=bounded;MStringArray fresh;
        splitCalls=0;throwSplitAt=0;
        assert(!stage(MString(condition==2?"a b":"alpha"),1U,bounded,fresh));
        throwSplitAt=-1;
        assert(splitCalls==0U&&fresh.values.empty());
        assert(bounded.bytes==before.bytes&&bounded.names==before.names&&bounded.workItems==before.workItems);
    }
    if(repeated){
        budget={};output={};splitCalls=0;throwSplitAt=0;
        assert(!stage(MString("a"),dommy::generation::kMaxWorkItemCount+1U,budget,output));
        throwSplitAt=-1;assert(splitCalls==0U);
    }
    budget={};output={};splitCalls=0;nullNameView=true;
    assert(!stage(MString("a"),1U,budget,output)&&splitCalls==0U);
    nullNameView=false;
    reportedBytes=1U;budget={};output={};splitCalls=0;
    assert(!stage(MString("alpha"),1U,budget,output)&&splitCalls==0U);reportedBytes=0U;
    MString embedded;embedded.text=std::string("a\0b",3U);budget={};output={};splitCalls=0;
    assert(!stage(embedded,1U,budget,output)&&splitCalls==0U);
    budget={};budget.bytes=dommy::generation::kMaxGeneratedStringByteCount-3U;
    budget.names=dommy::generation::kMaxGeneratedStringCount-2U;
    budget.workItems=dommy::generation::kMaxWorkItemCount-(9U+(repeated?4U:2U));
    output={};splitCalls=0;
    assert(stage(MString("a b"),2U,budget,output)&&splitCalls==1U&&output.length()==2U);
    assert(budget.bytes==dommy::generation::kMaxGeneratedStringByteCount&&
           budget.names==dommy::generation::kMaxGeneratedStringCount&&
           budget.workItems==dommy::generation::kMaxWorkItemCount);
    for(bool keep:{false,true}){retainEmptyFields=keep;budget={};output={};
        assert(stage(MString(" a  b "),2U,budget,output));
        assert(output.length()==(keep?5U:2U)&&budget.names==output.length());
        assert(budget.workItems==18U+output.length()*(repeated?2U:1U));
        assert(output.values[keep?1U:0U].text=="a"&&output.values[keep?3U:1U].text=="b");
    }
    retainEmptyFields=false;
    if(repeated){budget={};output={};
        assert(stage(MString(""),std::numeric_limits<std::size_t>::max(),budget,output)&&output.length()==0U);
    }
}
void reset(){accepted=false;calls.clear();throwAt=-1;throwKind=0;failAllocation=false;cacheCommits=0;
    failElementAt=-1;elementCalls=0;
    splitCalls=0;splitStatus=MS::kSuccess;failSplitAt=throwSplitAt=-1;throwSplitKind=0;nullNameView=retainEmptyFields=false;assert(liveArrays==0);}
int main(){
#include "renderer_list_cases.inc"
    AtNode node;MStringArray grids,empty;grids.values={MString("vel.x"),MString("vel.y")};
    reset();velocityStage(&node,grids);assert(accepted&&liveArrays==0);
    assert(node.arrays.at("velocity_grids")==std::vector<std::string>({"vel.x","vel.y"}));
    const auto connected=node.arrays;const auto controls=node.scalars;const auto connectCalls=calls;
    for(int at:{0,1}){
        reset();failElementAt=at;velocityStage(&node,grids);
        assert(!accepted&&node.visibility==0&&liveArrays==0&&node.arrays==connected);
        reset();velocityStage(&node,grids);assert(accepted&&liveArrays==0);
    }
    node.arrays["unrelated"]={"keep"};node.scalars["density"]=3;
    reset();velocityStage(&node,empty);assert(accepted&&liveArrays==0);
    assert(node.arrays.count("velocity_grids")==0 && node.scalars.size()==1U);
    assert(node.arrays.at("unrelated")==std::vector<std::string>{"keep"}&&node.scalars.at("density")==3);
    const auto disconnectCalls=calls;
    reset();velocityStage(&node,grids);assert(accepted);
    for(const auto& entry:controls)assert(node.scalars.at(entry.first)==entry.second);
    for(std::size_t i=0;i<connectCalls.size();++i)for(int kind:{0,1}){
        reset();throwAt=int(i);throwKind=kind;velocityStage(&node,grids);
        assert(!accepted&&node.visibility==0&&liveArrays==0);
        reset();velocityStage(&node,grids);assert(accepted);
    }
    reset();failAllocation=true;velocityStage(&node,grids);
    assert(!accepted&&node.visibility==0&&liveArrays==0&&node.arrays.at("velocity_grids")==connected.at("velocity_grids"));
    for(std::size_t i=0;i<disconnectCalls.size();++i)for(int kind:{0,1}){
        reset();throwAt=int(i);throwKind=kind;velocityStage(&node,empty);
        assert(!accepted&&node.visibility==0&&liveArrays==0);
        reset();velocityStage(&node,grids);assert(accepted);
    }
    // Both real VDB admission blocks run before the cache/velocity commit seam.
    reset();velocityUpdate(&node,MString("density"),MString("vel"));assert(accepted&&cacheCommits==1U);
    const auto oldArrays=node.arrays;const auto oldScalars=node.scalars;
    for(int at:{0,1}) {
        for(auto status:{MS::kFailure,MS::kInvalidParameter}){
            reset();failSplitAt=at;splitStatus=status;
            velocityUpdate(&node,MString("density"),MString(""));
            assert(!accepted&&cacheCommits==0U&&calls.empty()&&node.visibility==0);
            assert(node.arrays==oldArrays&&node.scalars==oldScalars);
        }
        for(int kind:{0,1}){
            reset();throwSplitAt=at;throwSplitKind=kind;
            velocityUpdate(&node,MString("density"),MString(""));
            assert(!accepted&&cacheCommits==0U&&calls.empty());
            assert(node.arrays==oldArrays&&node.scalars==oldScalars);
        }
    }
    reset();velocityUpdate(&node,MString("density"),MString(""));assert(accepted&&cacheCommits==1U);
    assert(node.arrays.count("velocity_grids")==0&&node.scalars.size()==1U);
}
#endif
