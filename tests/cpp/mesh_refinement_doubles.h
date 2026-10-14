#pragma once

// Host boundaries only. Native edge subdivision, DG and nearest-surface geometry
// are not emulated. Optional real OpenSubdiv executes the actual face algorithm.
#include "mesh_emitter_doubles.h"
#include "common/utility/DSubdivisionBudget.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DNumericValidation.h"
#include <functional>

#ifdef DOMMY_TEST_REAL_OPENSUBDIV
#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>
#else
namespace OpenSubdiv { namespace Sdc {
enum SchemeType { SCHEME_BILINEAR, SCHEME_CATMARK, SCHEME_LOOP };
struct Options {
    enum { VTX_BOUNDARY_NONE, VTX_BOUNDARY_EDGE_ONLY, VTX_BOUNDARY_EDGE_AND_CORNER,
           FVAR_LINEAR_NONE, FVAR_LINEAR_CORNERS_ONLY, FVAR_LINEAR_CORNERS_PLUS1,
           FVAR_LINEAR_CORNERS_PLUS2, FVAR_LINEAR_BOUNDARIES, FVAR_LINEAR_ALL,
           CREASE_UNIFORM, CREASE_CHAIKIN, TRI_SUB_CATMARK, TRI_SUB_SMOOTH };
    void SetVtxBoundaryInterpolation(int) {}
    void SetFVarLinearInterpolation(int) {}
    void SetCreasingMethod(int) {}
    void SetTriangleSubdivision(int) {}
};
}}
#endif
using namespace OpenSubdiv;
enum {D_COMP_POINT=0, D_COMP_EDGE, D_COMP_FACE, D_COMP_PRIM, D_COMP_PER_VTX, D_COMP_COUNT};
using DPIdList = std::vector<bool>;
inline MPoint DPToMP(const DPoint& p) {return p;}
inline MVector& operator*=(MVector& v, double scale) {v=v*scale;return v;}
namespace Imath {
struct V3d:MVector {explicit V3d(double value=0):MVector(value,value,value){}
    V3d(double x,double y,double z):MVector(x,y,z){} };
}
enum {VectorAttribute, ScalarAttribute, UnsupportedAttribute};
inline bool isVector3DAttrType(int type){return type==VectorAttribute;}
inline bool isSingleRealAttrType(int type){return type==ScalarAttribute;}
struct RefinementItem;
struct RefinementAttribute {
    const RefinementItem* owner;int type;
    RefinementAttribute(const RefinementItem* owner,int type):owner(owner),type(type){}
    int getAttrType()const{return type;}
};

struct DPGroupData {
    using ConstPtr = std::shared_ptr<const DPGroupData>;
    DPIdList ids;
    explicit DPGroupData(const DPIdList& ids):ids(ids) {}
    const DPIdList& getGroupIds() const {return ids;}
    bool isExistActive() const {return std::find(ids.begin(),ids.end(),true)!=ids.end();}
};
struct RefinementItem {
    DPointList points{{0,0,0},{2,0,0},{0,2,0},{0,0,2}};
    MIntArray faces{3,3,3,3}, connects{0,2,1,0,1,3,1,2,3,2,0,3};
    std::map<std::string,DPGroupData::ConstPtr> groups;
    MVectorArray vectors=MVectorArray(4,MVector(1,0,0));
    MDoubleArray scalars=MDoubleArray(4,3.0);
    bool vectorAttribute=true, scalarAttribute=true;
    bool unsupportedVector=false, unsupportedScalar=false;
    std::size_t reportedPoints=0, reportedConnections=0;
    int replacements=0, edits=0;
    std::size_t numberOfPoints()const{return reportedPoints?reportedPoints:points.size();}
    int numFace()const{return faces.length();}
    std::size_t numberOfConnections()const{return reportedConnections?reportedConnections:connects.size();}
    std::size_t numberOfUVs()const{return 0;}
    const DPointList& getPositionConst()const{return points;}
    const DPointList* getPositionPtrConst()const{return &points;}
    MIntArray getFaceList()const {return faces;}
    MIntArray getConnectList()const {return connects;}
    bool replacePositions(const DPointList& value){if(!api("mesh.positions"))return false;points=value;++edits;return true;}
    bool isExistCompGrp(int,const std::string& name)const{return groups.count(name);}
    DPGroupData::ConstPtr getCompGrp(int,const std::string& name)const{return groups.at(name);}
    bool removeCompGrp(int,const std::string& name){if(!api("group.remove"))return false;groups.erase(name);return true;}
    bool addCompGrp(int,const std::string& name,const DPGroupData& group){
        if(!api("group.add"))return false;groups[name]=std::make_shared<DPGroupData>(group);return true;
    }
    bool isExistAttr(const std::string& name)const{return (name=="vel" && vectorAttribute)||(name=="disp" && scalarAttribute);}
    std::shared_ptr<const RefinementAttribute> getAttr(const std::string& name)const{
        const int type=name=="vel"?(unsupportedVector?UnsupportedAttribute:VectorAttribute):
            (unsupportedScalar?UnsupportedAttribute:ScalarAttribute);
        return std::make_shared<RefinementAttribute>(this,type);
    }
    bool setReplaceMeshData(MObject){if(!api("mesh.replace"))return false;++replacements;return true;}
    bool setReplaceMeshData(const DPointList& p,const MIntArray& f,const MIntArray& c){
        if(!api("mesh.replace"))return false;points=p;faces=f;connects=c;groups.clear();++replacements;return true;
    }
};
struct RefinementData {
    static constexpr MFnData::Type id=MFnData::kAny;
    std::vector<std::shared_ptr<RefinementItem>> items;
    bool valid=true;
    std::size_t numberOfMesh()const{return items.size();}
    auto beginMeshes()const -> decltype(items.begin()){return items.begin();}
    auto endMeshes()const -> decltype(items.end()){return items.end();}
    bool hasValidMeshTopology()const{return valid;}
    MStatus copyFrom(const RefinementData& value){
        const auto s=api("mesh.copy");if(s){items.clear();valid=value.valid;
            for(const auto& p:value.items)items.push_back(std::make_shared<RefinementItem>(*p));}return s;
    }
    void updateBBox(){if(!api("mesh.bounds"))throw std::runtime_error("bounds failed");}
};
static RefinementData refinementInput,refinementStorage,refinementOutput;
static std::map<std::string,MIntArray> nativeFaces,nativeConnects;
static std::map<std::string,std::vector<MIntArray>> vertexFaces,vertexEdges,edgeFaces;
static bool invalidVertexId=false, invalidEdgeId=false, invalidFaceId=false, invalidEndpoint=false;
static bool invalidCenter=false, excessiveRefinement=false;
static double edgeLength=1.0, closestOffset=0.0;
static int lastDivisions=0;
static int nativeFault=0;
static MIntArray lastSelection;
inline MStatus getValidInputDM(MObject,MDataBlock&,const RefinementData*& value){
    const auto s=api("mesh.input");if(s)value=&refinementInput;return s;
}
inline MStatus convertRefinementMesh(const RefinementItem& mesh,MObject& object,const char* call){
    const auto s=api(call);if(!s)return s;
    object={"converted"+std::to_string(emitterMeshes.size())};
    auto& native=emitterMeshes[object.name];native.points=mesh.points;native.centers={};native.edges.clear();
    native.triangleCounts={};native.triangleIds={};
    nativeFaces[object.name]=mesh.faces;nativeConnects[object.name]=mesh.connects;
    auto& vf=vertexFaces[object.name];vf.resize(mesh.points.size());
    auto& ve=vertexEdges[object.name];ve.resize(mesh.points.size());
    auto& ef=edgeFaces[object.name];std::size_t offset=0;
    for(unsigned f=0;f<mesh.faces.length();++f){
        const int count=mesh.faces[f];MVector center;
        for(int c=0;c<count;++c){
            const int a=mesh.connects.at(offset+c),b=mesh.connects.at(offset+(c+1)%count);
            center=center+mesh.points.at(a);vf.at(a).push_back(f);
            std::array<int,2> pair{{std::min(a,b),std::max(a,b)}};
            auto found=std::find(native.edges.begin(),native.edges.end(),pair);
            int id=int(found-native.edges.begin());
            if(found==native.edges.end()){native.edges.push_back(pair);ef.emplace_back();ve.at(a).push_back(id);ve.at(b).push_back(id);}
            ef.at(id).push_back(f);
        }
        native.centers.push_back(center*(1.0/count));native.triangleCounts.push_back(count-2);
        for(int c=1;c<count-1;++c){native.triangleIds.push_back(mesh.connects.at(offset));
            native.triangleIds.push_back(mesh.connects.at(offset+c));native.triangleIds.push_back(mesh.connects.at(offset+c+1));}
        offset+=count;
    }
    if(nativeFault==1)vf.at(0)={99};
    if(nativeFault==2)ve.at(0)={99};
    if(nativeFault==3)ef.at(0)={99};
    if(nativeFault==4)native.stuck=true;
    if(nativeFault==5)native.centers.pop_back();
    return MS::kSuccess;
}
inline MStatus generateMMeshFromDmesh(const RefinementItem& m,MObject& o){return convertRefinementMesh(m,o,"mesh.convert");}
inline MStatus generateMMeshFromDmeshWithUV(const RefinementItem& m,MObject& o){return convertRefinementMesh(m,o,"mesh.convertUV");}
namespace dommy {namespace attribute {
// Conversion itself is covered by the core typed-attribute suite. Here inject
// its bool/exception boundary while executing the node's complete control flow.
inline bool convertVector3Attribute(const std::shared_ptr<const RefinementAttribute>& a,
                                    std::size_t count,std::vector<Imath::V3d>& result){
    const int index=callCount;if(!api("attribute.vector") || a->owner->vectors.size()!=count)return false;
    result.clear();for(const auto& v:a->owner->vectors){
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return false;
        result.emplace_back(v.x,v.y,v.z);
    }
    if(index==shortAt){result.pop_back();shortApplied=true;}return true;
}
inline bool convertScalarAttribute(const std::shared_ptr<const RefinementAttribute>& a,
                                   std::size_t count,std::vector<double>& result){
    const int index=callCount;if(!api("attribute.scalar") || a->owner->scalars.size()!=count)return false;
    result=a->owner->scalars;for(double v:result)if(!std::isfinite(v))return false;
    if(index==shortAt){result.pop_back();shortApplied=true;}return true;
}
}}
struct RefinementMesh:MFnMesh {
    using MFnMesh::MFnMesh;
    int numFaceVertices(MStatus* status)const{*status=api("mesh.connectionCount");return nativeConnects.at(object.name).length();}
    MStatus subdivideEdges(MIntArray& selected,int divisions){const auto s=api("mesh.subdivideEdges");
        if(s){lastSelection=selected;lastDivisions=divisions;
            if(excessiveRefinement)emitterMeshes[object.name].pointCount=10000000;}return s;}
    MStatus updateSurface(){return api("mesh.updateSurface");}
};
struct RefinementPolygon:MItMeshPolygon {
    using MItMeshPolygon::MItMeshPolygon;
    int index(MStatus* status)const{*status=api("face.index");return invalidFaceId?-1:int(ordinal);}
    MPoint center(MSpace::Space space,MStatus* status){
        const auto result=MItMeshPolygon::center(space,status);return invalidCenter?MPoint(NAN,0,0):result;
    }
    MStatus numTriangles(int& count){const auto s=api("face.triangleCount");if(s)count=mesh().triangleCounts.at(ordinal);return s;}
};
struct MItMeshVertex {
    MObject object;unsigned ordinal=0;
    MItMeshVertex(MObject o,MStatus* s):object(o){*s=api("vertex.bind");}
    bool isDone(MStatus* s){*s=api("vertex.done");return ordinal>=emitterMeshes.at(object.name).points.size();}
    MStatus next(){const auto s=api("vertex.next");if(s && !emitterMeshes.at(object.name).stuck)++ordinal;return s;}
    int index(MStatus* s)const{*s=api("vertex.index");return invalidVertexId?-1:int(ordinal);}
    MStatus getConnectedFaces(MIntArray& out)const{const auto s=api("vertex.faces");if(s)out=vertexFaces.at(object.name).at(ordinal);return s;}
    MStatus getConnectedEdges(MIntArray& out)const{const auto s=api("vertex.edges");if(s)out=vertexEdges.at(object.name).at(ordinal);return s;}
};
struct MItMeshEdge {
    MObject object;unsigned ordinal=0;
    MItMeshEdge(MObject o,MStatus* s):object(o){*s=api("edge.bind");}
    bool isDone(MStatus* s){*s=api("edge.done");return ordinal>=emitterMeshes.at(object.name).edges.size();}
    MStatus next(){const auto s=api("edge.next");if(s && !emitterMeshes.at(object.name).stuck)++ordinal;return s;}
    int index(MStatus* s)const{*s=api("edge.index");return invalidEdgeId?-1:int(ordinal);}
    int index(int endpoint,MStatus* s)const{*s=api("edge.endpoint");
        return invalidEndpoint?-1:emitterMeshes.at(object.name).edges.at(ordinal).at(endpoint);}
    MStatus setIndex(int index,int& previous){const auto s=api("edge.setIndex");if(s){previous=ordinal;ordinal=index;}return s;}
    MStatus getLength(double& value)const{const auto s=api("edge.length");if(s)value=edgeLength;return s;}
    int getConnectedFaces(MIntArray& out,MStatus* status)const{
        *status=api("edge.faces");if(*status)out=edgeFaces.at(object.name).at(ordinal);return out.length();
    }
    bool onBoundary(MStatus* status)const{*status=api("edge.boundary");return edgeFaces.at(object.name).at(ordinal).size()==1;}
};
namespace dommy {namespace maya {
template<> MStatus createPluginDataObject<RefinementData>(MFnData::Type,MObject& object,RefinementData*& value){
    const auto s=api("plugin.create");if(s){refinementStorage=RefinementData();object={"output"};value=&refinementStorage;}return s;
}
template<> MStatus publishTypedPluginDataObject<RefinementData>(MDataHandle& handle,MObject,MFnData::Type){
    handle.check();const auto s=api("publish");if(s){refinementOutput=refinementStorage;++published;}return s;
}
struct MeshClosestPointQuery {
    MStatus create(MObject){return api("closest.create");}
    MStatus getClosestPoint(const MPoint& p,MPoint& q){const auto s=api("closest.query");if(s)q=p+MVector(closestOffset,0,0);return s;}
};
}}
