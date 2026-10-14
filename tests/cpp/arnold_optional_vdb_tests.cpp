#include "arnold_translation_doubles.h"
bool validVdb=true;
int liveVdbHandles=0;
struct MDataHandle {};
namespace dommy {namespace maya {
struct ScopedPlugDataHandle {
    MDataHandle value;
    ScopedPlugDataHandle(){++liveVdbHandles;}
    ~ScopedPlugDataHandle(){--liveVdbHandles;}
    MDataHandle& handle(){return value;}
    MStatus read(const MPlug&){return api("vdb.read");}
};
}}
struct DOpenVDBData {};
const DOpenVDBData* getInputVDBfromHandle(const MDataHandle&){
    const MStatus status=api("vdb.bind");static DOpenVDBData value;
    return status&&validVdb?&value:nullptr;
}
#include "optional_vdb.inc"
void resetVdb(){calls.clear();writes.clear();connections.clear();failAt=-1;kind=errors=0;admitted=false;validVdb=true;connections["VDBInput"]=true;}
int main(){
    DVDBTranslatorAi translator;AtNode output;
    resetVdb();translator.Update(&output);assert(admitted&&errors==0&&liveVdbHandles==0);
    const auto trace=calls;
    for(std::size_t index=0U;index<trace.size();++index)if(trace[index].find("vdb.")==0||trace[index]=="cache/render.stage"){
        for(int failure:{1,2,3}){
            resetVdb();output.visibility=7;failAt=int(index);kind=failure;translator.Update(&output);
            assert(!admitted&&errors>0&&output.visibility==0&&liveVdbHandles==0);
            resetVdb();translator.Update(&output);assert(admitted&&errors==0&&output.visibility==1&&liveVdbHandles==0);
        }
    }
    resetVdb();validVdb=false;translator.Update(&output);assert(!admitted&&errors>0&&output.visibility==0&&liveVdbHandles==0);
    resetVdb();connections["VDBInput"]=false;translator.Update(&output);assert(!admitted&&errors==0&&output.visibility==1&&liveVdbHandles==0);
}
