#pragma once
// Only export admission control flow is modeled; this is not Maya or Arnold.
#include <maya/MStatus.h>
#include <algorithm>
#include <cassert>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef DOMMY_SDK_CHECK
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MUuid.h>
#include "common/utility/DMayaPlugDataHandle.h"
extern bool reachedPayload;
#else
std::vector<std::string> operations;
int failStep=-1, failureKind=0, errors=0, reports=0, destroyed=0, liveHandles=0;
bool reachedPayload=false, connected=true, nullPlug=false, validUuid=true, validData=true;
MStatus operation(const char* name, MStatus* output=nullptr) {
    operations.push_back(name);
    const bool failed=int(operations.size())-1==failStep;
    if (failed && failureKind==2) throw std::bad_alloc();
    if (failed && failureKind==3) throw std::runtime_error("injected export admission failure");
    const MStatus result=failed?MS::kInvalidParameter:MS::kSuccess;
    if (output) *output=result;
    return result;
}
struct MString {
    std::string value;
    MString(const char* text=""):value(text){}
    const char* asChar() const {return value.c_str();}
};
struct MObject { int id=0; explicit MObject(int value=0):id(value){} };
struct MUuid {
    bool valid() const {return validUuid;}
    MString asString() const {return "owner-123";}
};
struct MPlug {
    bool isNull() const {operation("plug.null");return nullPlug;}
    bool isConnected(MStatus* status=nullptr) const {
        operation("plug.connected",status);return connected;
    }
};
struct MFnDependencyNode {
    MFnDependencyNode(const MObject&,MStatus* status=nullptr) {operation("node.bind",status);}
    MUuid uuid(MStatus* status=nullptr) const {operation("node.uuid",status);return {};}
};
struct MDataHandle {};
namespace dommy {namespace maya {
struct ScopedPlugDataHandle {
    MDataHandle value; bool live=false;
    ~ScopedPlugDataHandle(){if(live)--liveHandles;}
    MDataHandle& handle(){return value;}
    MStatus read(const MPlug&){const MStatus s=operation("plug.handle");if(s){live=true;++liveHandles;}return s;}
};
}}
#endif

struct AtNode {int id=0;};
struct DPData {std::size_t count=1;std::size_t numberOfPoints()const{return count;}};
const DPData* getInputDPfromHandle(MDataHandle);
bool collectGeneratedNodes(const std::string&,std::vector<AtNode*>&);
bool destroyGeneratedNodes(const std::vector<AtNode*>&);
void AiMsgError(const char*,...);
void AiNodeSetByte(AtNode*,const char*,unsigned char);
namespace dommy {namespace arnold {
void reportTranslatorException(const char*,AtNode*,const std::exception_ptr&) noexcept;
}}
struct PendingNodeBatchBoundary { bool retire(); };
struct DPLightTranslatorAi {
    PendingNodeBatchBoundary pendingNodes_;
    MObject GetMayaObject() const;
    MPlug FindMayaPlug(const char*) const;
    void ExportMatrix(AtNode*);
    unsigned char ComputeVisibility() const;
    void Update(AtNode*);
};

#ifndef DOMMY_SDK_CHECK
DPData points;
std::vector<AtNode*> existingLights;
bool PendingNodeBatchBoundary::retire() { return bool(operation("arnold.pending.retire")); }
MObject DPLightTranslatorAi::GetMayaObject() const {return MObject(1);}
MPlug DPLightTranslatorAi::FindMayaPlug(const char*) const {operation("plug.find");return {};}
void DPLightTranslatorAi::ExportMatrix(AtNode*) {operation("arnold.matrix");}
unsigned char DPLightTranslatorAi::ComputeVisibility() const {return 1;}
void AiNodeSetByte(AtNode*,const char*,unsigned char){operation("arnold.visibility");}
void AiMsgError(const char*,...){++errors;}
bool collectGeneratedNodes(const std::string&,std::vector<AtNode*>& output){
    if(!operation("arnold.collect"))return false;output=existingLights;return true;
}
bool destroyGeneratedNodes(const std::vector<AtNode*>& input){
    destroyed+=int(input.size());existingLights.clear();return true;
}
const DPData* getInputDPfromHandle(MDataHandle){
    return operation("data.read") && validData?&points:nullptr;
}
namespace dommy {namespace arnold {
void reportTranslatorException(const char*,AtNode*,const std::exception_ptr&) noexcept {++reports;}
}}
#endif
