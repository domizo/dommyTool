#pragma once
#include <maya/MStatus.h>
#include <algorithm>
#include <cassert>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <utility>
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#ifdef DOMMY_SDK_CHECK
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MUuid.h>
extern bool admitted,shaderAccepted,replacementAccepted;
extern std::size_t acceptedReplacements;
extern short copyMode;
extern std::size_t replacementBudgetBytes,replacementBudgetNames,replacementBudgetWork;
#else
bool admitted=false,renderEnabled=true,motionEnabled=true,throwVisibility=false;
bool shaderAccepted=false,replacementAccepted=false,exportShader=true;
short copyMode=1;
unsigned rowCount=2;
bool tableIsArray=true;
bool uuidValid=true;
std::string targetNames="meshA meshB",exportFailure;
std::size_t acceptedReplacements=0;
std::size_t replacementBudgetBytes=0U,replacementBudgetNames=0U,replacementBudgetWork=0U;
int failAt=-1,kind=0,errors=0,reports=0;
std::vector<std::string> calls,writes;
std::map<std::string,bool> connections;
std::map<std::string,std::string> sources;
MStatus api(const std::string& name,MStatus* status=nullptr){
    calls.push_back(name);bool failed=int(calls.size())-1==failAt;
    if(failed&&kind==2)throw std::bad_alloc();
    if(failed&&kind==3)throw std::runtime_error("translator query failure");
    const MStatus result=failed?MS::kInvalidParameter:MS::kSuccess;
    if(status)*status=result;return result;
}
struct MString {std::string text;MString(const char* value=""):text(value){}const char* asChar()const{return text.c_str();}
    unsigned length()const{return static_cast<unsigned>(text.size());}};
struct MObject {int id=0;explicit MObject(int value=0):id(value){}};
struct MUuid {MString asString()const{return "owner-123";}bool valid()const{return uuidValid;}};
struct MPlug {
    std::string key;MPlug()=default;explicit MPlug(const char* name):key(name){}
    bool isNull()const{return key.empty();}
    bool isConnected(MStatus* status=nullptr)const{api("connected."+key,status);return connections[key];}
    MPlug source(MStatus* status=nullptr)const{api("source."+key,status);return MPlug(sources[key].c_str());}
    bool isArray(MStatus* status=nullptr)const{api("array."+key,status);return tableIsArray;}
    unsigned numElements(MStatus* status=nullptr)const{api("elements."+key,status);return rowCount;}
    unsigned numChildren(MStatus* status=nullptr)const{api("children."+key,status);return 2;}
    MPlug elementByPhysicalIndex(unsigned,MStatus* status=nullptr)const{api("element."+key,status);return MPlug("row");}
    MPlug child(unsigned index,MStatus* status=nullptr)const{api("child."+key,status);return MPlug(index==0?"TargetNodeNames":"ReplaceShader");}
    MPlug child(MObject& attr,MStatus* status=nullptr)const{return child(unsigned(attr.id-1),status);}
    MString name(MStatus* status=nullptr)const{api("name."+key,status);return MString(key.c_str());}
};
struct MFnDependencyNode {MFnDependencyNode(const MObject&,MStatus* status=nullptr){api("owner.bind",status);}
    MUuid uuid(MStatus* status=nullptr)const{api("owner.uuid",status);return {};}};
#endif
using DPIdList=std::vector<bool>;
enum {D_COMP_FACE=1};
struct DPGroupData {
    using ConstPtr=std::shared_ptr<const DPGroupData>;
    DPIdList ids;
    const DPIdList& getGroupIds()const{return ids;}
};
struct MatteMesh {
    std::size_t faces=0U;
    bool exists=true;
    DPGroupData::ConstPtr group;
    std::size_t numFace()const{return faces;}
    bool isExistCompGrp(int,const char*)const{return exists;}
    DPGroupData::ConstPtr getCompGrp(int,const char*)const{return group;}
};
struct MatteCollection {
    std::vector<std::shared_ptr<MatteMesh>> meshes;
    bool isExistCompGrp(int,const char*)const{return !meshes.empty()&&meshes.front()->exists;}
    auto beginMeshes()const -> decltype(meshes.begin()){return meshes.begin();}
    auto endMeshes()const -> decltype(meshes.end()){return meshes.end();}
};
struct AtString {const char* name;explicit AtString(const char* value):name(value){}};
struct AtNode { unsigned char visibility=7; std::map<std::string,AtNode*> bindings; };
namespace dommy {namespace arnold {
struct NodeOwner {};
void reportTranslatorException(const char*,AtNode*,const std::exception_ptr&)noexcept;
}}
void AiMsgError(const char*,...);
void AiMsgDebug(const char*,...);
void AiMsgWarning(const char*,...);
void AiNodeSetPtr(AtNode*,const char*,AtNode*);
void AiNodeSetPtr(AtNode*,AtString,AtNode*);
void AiNodeResetParameter(AtNode*,const char*);
const char* AiNodeGetName(AtNode*);
std::vector<std::string> splitStringWithChar(const std::string&,const std::string&);
bool isContainString(const std::string&,const char*);
void AiNodeSetByte(AtNode*,AtString,unsigned char);
void AiNodeSetByte(AtNode*,const char*,unsigned char);
void AiNodeSetFlt(AtNode*,const char*,float);
template<typename T>MStatus readPlugValue(const MPlug&,T&);
struct Translator {
    MObject GetMayaObject()const;MPlug FindMayaPlug(const char*)const;
    unsigned GetMotionStep()const{return 0;}unsigned GetNumMotionSteps()const{return 2;}
    bool IsMotionBlurEnabled()const;
    unsigned char ComputeVisibility()const{return 1;}
    void ExportMatrix(AtNode*);
    AtNode* ExportConnectedNode(const MPlug&);
};
struct DPointTranslatorAi:Translator{void Update(AtNode*);void ExportMotion(AtNode*);void payloadUpdate(AtNode*);void payloadExportMotion(AtNode*);void shaderAdmission(AtNode*);};
struct DCurveTranslatorAi:Translator{void Update(AtNode*);void ExportMotion(AtNode*);void payloadUpdate(AtNode*);void payloadExportMotion(AtNode*);void shaderAdmission(AtNode*);};
struct DMeshTranslatorAi:Translator{void Update(AtNode*);void ExportMotion(AtNode*);void payloadUpdate(AtNode*);void payloadExportMotion(AtNode*);void shaderAdmission(AtNode*);};
struct DVDBTranslatorAi:Translator{void Update(AtNode*);void shaderAdmission(AtNode*);};
struct DPInstancerTranslatorAi:Translator{void Update(AtNode*);void shaderAdmission(AtNode*);void replacementAdmission();bool retireGeneratedNodes();
    struct ReplaceShaderData{std::vector<std::string> shapeNameList;AtNode* shaderNode;};};
struct DPInstancer{static MObject aTargetNodeNames,aReplaceShader;};
enum {DP_INST_COPY_MESH=0,DP_INST_COPY_ASS=1,DP_INST_COPY_TYPE_COUNT=2};
using DP_INST_COPY_TYPE=short;
#define DOMMY_ENABLE_DEBUG_LOGS 0
#define DOMMY_MAYA_DEBUG(...) do{}while(false)
#define INST_DEBUG(...) do{}while(false)
#ifndef DOMMY_SDK_CHECK
MObject Translator::GetMayaObject()const{return MObject();}
MPlug Translator::FindMayaPlug(const char* name)const{api("find."+std::string(name));return MPlug(name);}
bool Translator::IsMotionBlurEnabled()const{return motionEnabled;}
void Translator::ExportMatrix(AtNode*){writes.push_back("matrix");}
void AiNodeSetByte(AtNode* node,AtString,unsigned char value){if(throwVisibility)throw std::runtime_error("visibility cleanup failure");node->visibility=value;writes.push_back("visibility");}
void AiNodeSetByte(AtNode* node,const char*,unsigned char value){if(throwVisibility)throw std::runtime_error("visibility cleanup failure");node->visibility=value;writes.push_back("visibility");}
void AiNodeSetFlt(AtNode*,const char*,float){writes.push_back("radius");}
void AiMsgError(const char*,...){++errors;}
void AiMsgDebug(const char*,...){}
void AiMsgWarning(const char*,...){++errors;}
void AiNodeSetPtr(AtNode* node,const char* name,AtNode* value){node->bindings[name]=value;writes.push_back(name);}
void AiNodeSetPtr(AtNode* node,AtString name,AtNode* value){AiNodeSetPtr(node,name.name,value);}
void AiNodeResetParameter(AtNode* node,const char* name){
    (void)api("reset."+std::string(name));
    node->bindings.erase(name);writes.push_back("reset."+std::string(name));
}
const char* AiNodeGetName(AtNode*){return "shader";}
AtNode* Translator::ExportConnectedNode(const MPlug& plug){api("export."+plug.key);static AtNode shader;return exportShader&&plug.key!=exportFailure?&shader:nullptr;}
MObject DPInstancer::aTargetNodeNames(1),DPInstancer::aReplaceShader(2);
// Admission-only prefix seam. Actual retirement/pending batches are exercised
// by the lifecycle fixture; this does not claim native cleanup coverage.
bool DPInstancerTranslatorAi::retireGeneratedNodes(){return true;}
bool isContainString(const std::string& value,const char* token){return value.find(token)!=std::string::npos;}
template<typename T>MStatus readPlugValue(const MPlug& plug,T& value){
    const MStatus status=api("read."+plug.key);value=T();return status;
}
template<>MStatus readPlugValue(const MPlug& plug,bool& value){
    const MStatus status=api("read."+plug.key);value=plug.key=="IsRender"?renderEnabled:false;return status;
}
template<>MStatus readPlugValue(const MPlug& plug,short& value){
    const MStatus status=api("read."+plug.key);value=plug.key=="CopyObjectType"?copyMode:0;return status;
}
template<>MStatus readPlugValue(const MPlug& plug,MString& value){
    const MStatus status=api("read."+plug.key);value=MString(targetNames.c_str());
    return status&&plug.key=="ReplaceShader"?MS::kInvalidParameter:status;
}
#endif
