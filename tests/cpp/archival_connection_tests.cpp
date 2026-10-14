#include <maya/MStatus.h>
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include <cassert>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
static std::vector<std::string> calls;
static int failAt=-1,kind=0,cleaned=0,processed=0;
static bool connected=true,otherNode=false,oversized=false;
static std::vector<int> values{-3,4},published{99};
MStatus api(const char* name,MStatus* status=nullptr){
    calls.push_back(name);const bool failed=int(calls.size())-1==failAt;
    if(failed&&kind==2)throw std::bad_alloc();
    if(failed&&kind==3)throw std::runtime_error("archival input failure");
    const MStatus result=failed?MS::kInvalidParameter:MS::kSuccess;
    if(status)*status=result;return result;
}
namespace MFnData{enum Type{kIntArray,kDoubleArray};}
static MFnData::Type inputType=MFnData::kIntArray;
struct MObject{int id;MObject(int value=0):id(value){}};
struct MPlugArray;
struct MPlug{
    int attr=0;
    MPlug()=default;MPlug(MObject,MObject value):attr(value.id){}
    explicit MPlug(int value):attr(value){}
    bool operator==(MObject value)const{return attr==value.id;}
    bool operator!=(MObject value)const{return !(*this==value);}
    bool isConnected(MStatus* status=nullptr)const{api("connection",status);return connected;}
    bool connectedTo(MPlugArray&,bool,bool,MStatus* =nullptr)const;
    MObject node(MStatus* status=nullptr)const{api("node",status);return MObject(attr);}
};
struct MPlugArray{
    unsigned length()const{return 2;}MPlug operator[](unsigned index)const{return MPlug(index+1);}
};
bool MPlug::connectedTo(MPlugArray&,bool,bool,MStatus* status)const{api("graph",status);return true;}
struct MPxNode{virtual ~MPxNode()=default;};
struct DPnPopBase:MPxNode{
    virtual MStatus runProcess(){const MStatus status=api("process");if(status)++processed;return status;}
};
static DPnPopBase pop;
static MPxNode plain;
struct MFnDependencyNode{
    bool valid=true;
    MFnDependencyNode(MObject,MStatus* status=nullptr){valid=bool(api("bind",status));}
    MPxNode* userNode(MStatus* status=nullptr)const{assert(valid);api("user",status);return otherNode?&plain:&pop;}
};
struct MIntArray{
    unsigned length()const{return oversized?unsigned(dommy::generation::kMaxGeneratedPointCount+1):values.size();}
    int operator[](unsigned index)const{return values[index];}
};
struct MFnIntArrayData{
    bool valid;
    MFnIntArrayData(MObject,MStatus* status):valid(bool(api("int.bind",status))){}
    MIntArray array(MStatus* status){assert(valid);api("int.array",status);return {};}
};
struct MDataHandle{
    bool valid=true,input=false;unsigned index=0;std::vector<int>* staging=nullptr;
    MFnData::Type type()const{assert(valid&&input);return inputType;}
    MObject data(){assert(valid&&input);return MObject(1);}
    void set(int value){assert(valid&&!input);assert(staging&&index<staging->size());(*staging)[index]=value;}
};
struct MDataBlock;
struct MArrayDataBuilder{
    bool valid;
    std::vector<int> staging;
    MArrayDataBuilder(MDataBlock*,MObject,unsigned count,MStatus* status):valid(bool(api("builder",status))){
        if(valid)staging.resize(count);
    }
    MDataHandle addElement(unsigned index,MStatus* status){
        assert(valid);const bool ok=bool(api("element",status));MDataHandle handle;
        handle.valid=ok;handle.index=index;handle.staging=&staging;return handle;
    }
};
struct MArrayDataHandle{
    bool valid=true;
    MStatus set(const MArrayDataBuilder& builder){
        assert(valid&&builder.valid);const MStatus status=api("publish");
        if(status)published=builder.staging;return status;
    }
    MStatus setAllClean(){assert(valid);const MStatus status=api("array.clean");if(status)++cleaned;return status;}
};
struct MDataBlock{
    MDataHandle inputValue(MObject,MStatus* status){
        MDataHandle handle;handle.valid=bool(api("input",status));handle.input=true;return handle;
    }
    MArrayDataHandle outputArrayValue(MObject,MStatus* status){MArrayDataHandle handle;handle.valid=bool(api("output",status));return handle;}
    MStatus setClean(const MPlug&){const MStatus status=api("clean");if(status)++cleaned;return status;}
};
struct MGlobal{static void displayWarning(const char*){}};
namespace dommy{namespace maya{
MStatus reportEntryPointException(const char*,const char*,const std::exception_ptr&)noexcept{return MS::kFailure;}
MStatus reportComputeException(const char*,const std::exception_ptr&)noexcept{return MS::kFailure;}
}}
struct DPArrayToCompound{
    static MObject outValue,inputData;
    MObject thisMObject()const{return MObject(1);}
    MStatus compute(const MPlug&,MDataBlock&);
};
MObject DPArrayToCompound::outValue(1),DPArrayToCompound::inputData(2);
#include "archival_methods.inc"
void reset(){
    calls.clear();failAt=-1;kind=cleaned=processed=0;
    connected=true;otherNode=oversized=false;values={-3,4};published={99};inputType=MFnData::kIntArray;
}
int main(){
    DPArrayToCompound node;MDataBlock block;const MPlug output(1);
    reset();assert(node.compute(output,block));assert(published==values);
    const auto arrayTrace=calls;
    for(std::size_t i=0;i<arrayTrace.size();++i)for(int failure:{1,2,3})for(bool connection:{false,true}){
        reset();if(i==0)connected=connection;failAt=int(i);kind=failure;
        const MStatus status=node.compute(output,block);
        assert(status==(failure==1?MS::kInvalidParameter:MS::kFailure));
        assert(calls.size()==i+1);
        if(arrayTrace[i]!="array.clean"&&arrayTrace[i]!="clean")assert(published==std::vector<int>{99});
        reset();assert(node.compute(output,block)&&published==values);
    }
    reset();assert(node.compute(MPlug(100),block)==MS::kUnknownParameter&&calls.empty());
    reset();connected=false;assert(node.compute(output,block)==MS::kUnknownParameter&&calls.size()==1);
    reset();values.clear();assert(node.compute(output,block)&&published.empty());
    reset();inputType=MFnData::kDoubleArray;assert(node.compute(output,block)==MS::kFailure&&published==std::vector<int>{99});
    reset();oversized=true;assert(node.compute(output,block)==MS::kInvalidParameter&&published==std::vector<int>{99});
    reset();assert(runPopOperation(MObject(1),MObject(2))&&processed==2);
    const auto popTrace=calls;
    for(std::size_t i=0;i<popTrace.size();++i)for(int failure:{1,2,3})for(bool connection:{false,true}){
        reset();if(i==0)connected=connection;failAt=int(i);kind=failure;
        const MStatus status=runPopOperation(MObject(1),MObject(2));
        assert(status==(failure==1?MS::kInvalidParameter:MS::kFailure));
        assert(calls.size()==i+1);
        reset();assert(runPopOperation(MObject(1),MObject(2))&&processed==2);
    }
    reset();connected=false;assert(runPopOperation(MObject(1),MObject(2))&&processed==0&&calls.size()==1);
    reset();otherNode=true;assert(runPopOperation(MObject(1),MObject(2))&&processed==0);
}
