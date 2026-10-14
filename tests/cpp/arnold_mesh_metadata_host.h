#pragma once
#include <maya/MStatus.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DMeshMergePlan.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef DOMMY_SDK_CHECK
#include <maya/MPlug.h>
#include <maya/MStringArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#else
int failAt=-1, failureKind=0;
bool tableArray=true, nullOwner=false, nullMesh=false;
unsigned tableCount=3;
unsigned reportedStringLength=0U;
std::string oversizedString;
int nativeEdges=3, endpoint=1;
std::vector<std::string> calls;
MStatus query(const char* name,MStatus* output=nullptr) {
    calls.push_back(name);
    const bool failed=int(calls.size())-1==failAt;
    if(failed&&failureKind==2) throw std::bad_alloc();
    if(failed&&failureKind==3) throw std::runtime_error("metadata fault");
    const MStatus status=failed?MS::kInvalidParameter:MS::kSuccess;
    if(output)*output=status;
    return status;
}
struct MString {
    std::string text;
    MString(const char* s=""):text(s){}
    unsigned length()const{return reportedStringLength&&(oversizedString.empty()||text==oversizedString)?reportedStringLength:text.size();}
    const char* asChar()const{return text.c_str();}
};
struct MStringArray {
    std::vector<MString> values;
    unsigned length()const{return values.size();}
    MStatus append(const MString& value) {const auto s=query("names.append");if(s)values.push_back(value);return s;}
    const MString& operator[](unsigned index)const{return values.at(index);}
};
struct MObject {int id;MObject(int v=0):id(v){}bool isNull()const{return id==0;}};
struct Row {bool enabled=true;std::string name="matte",groups="first second third ignored";};
std::vector<Row> rows;
struct MPlug {
    int row=-1,attribute=-1;bool null=false;
    bool isNull()const{return null;}
    bool isArray(MStatus* s=nullptr)const{query("table.array",s);return tableArray;}
    unsigned numElements(MStatus* s=nullptr)const{query("table.count",s);return tableCount;}
    MPlug elementByPhysicalIndex(unsigned i,MStatus* s=nullptr)const{
        query("table.row",s);MPlug p;p.row=int(i);return p;}
    MPlug child(const MObject& a,MStatus* s=nullptr)const{
        query("row.child",s);MPlug p=*this;p.attribute=a.id;return p;}
};
using int2=int[2];
struct MPointArray{};struct MIntArray{};
struct MFnMeshData {MObject create(MStatus* s=nullptr)const{query("mesh.owner",s);return MObject(nullOwner?0:1);}};
struct MFnMesh {
    MObject create(int,int,const MPointArray&,const MIntArray&,const MIntArray&,MObject,MStatus* s=nullptr){
        query("mesh.create",s);return MObject(nullMesh?0:2);}
    int numEdges(MStatus* s=nullptr)const{query("mesh.edges",s);return nativeEdges;}
    MStatus getEdgeVertices(int edge,int2& vertices)const{
        const auto s=query("mesh.vertices");vertices[0]=edge;vertices[1]=endpoint;return s;}
};
#endif

struct DMeshDisplay {static MObject aGroupMatteEnable,aGroupMatteName,aMatteGroups;};
MStatus readPlugValue(const MPlug&,bool&);
MStatus readPlugValue(const MPlug&,MString&);
using DPIndexList=std::vector<std::size_t>;
using DPIdList=std::vector<bool>;
enum {D_COMP_EDGE=2};
struct DPGroupData {
    using ConstPtr=std::shared_ptr<const DPGroupData>;
    DPIdList ids;
    const DPIdList& getGroupIds()const{return ids;}
};
struct MetadataMesh {
    std::size_t points=3,connections=3;int faces=1;
    bool exists=true;
    DPGroupData::ConstPtr group;
    std::size_t numberOfPoints()const{return points;}
    std::size_t numberOfConnections()const{return connections;}
    int numFace()const{return faces;}
    bool isExistCompGrp(int,const char*)const{return exists;}
    DPGroupData::ConstPtr getCompGrp(int,const char*)const{return group;}
    MPointArray getMPosition()const;
    MIntArray getFaceList()const;
    MIntArray getConnectList()const;
};
struct MeshOwner {
    std::vector<std::shared_ptr<MetadataMesh>> meshes;
    auto beginMeshes()const -> decltype(meshes.begin()){return meshes.begin();}
    auto endMeshes()const -> decltype(meshes.end()){return meshes.end();}
    std::size_t numberOfMesh()const{return meshes.size();}
};
struct AtNode{};
extern std::size_t fixtureFaces,mattePublished,creasePublished;
extern MString fixtureCreaseNames;
MPlug FindMayaPlug(const char*);
void AiMsgError(const char*,...);
void AiMsgWarning(const char*,...);
void AiNodeSetByte(AtNode*,const char*,unsigned char);

#ifndef DOMMY_SDK_CHECK
MObject DMeshDisplay::aGroupMatteEnable(0),DMeshDisplay::aGroupMatteName(1),DMeshDisplay::aMatteGroups(2);
MStatus readPlugValue(const MPlug& p,bool& out){const auto s=query("row.enable");out=rows.at(p.row).enabled;return s;}
MStatus readPlugValue(const MPlug& p,MString& out){const auto s=query("row.string");const auto& r=rows.at(p.row);out=MString((p.attribute==1?r.name:r.groups).c_str());return s;}
MPointArray MetadataMesh::getMPosition()const{return {};}
MIntArray MetadataMesh::getFaceList()const{return {};}
MIntArray MetadataMesh::getConnectList()const{return {};}
std::size_t fixtureFaces=1,mattePublished=999,creasePublished=999;
MString fixtureCreaseNames="selected ignored";
int visibility=1,errors=0;
MPlug FindMayaPlug(const char*){return {};}
void AiMsgError(const char*,...){++errors;}
void AiMsgWarning(const char*,...){}
void AiNodeSetByte(AtNode*,const char*,unsigned char v){visibility=v;}
#endif
