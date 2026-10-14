#pragma once
// Failure-injectable DG/container boundaries, not a Maya geometry emulator.
// Attribute values, bounded merge planning and point arithmetic are production.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMeshCopyBudget.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <functional>

static int publications = 0, geometryCopies = 0, positionViews = 0, positionCopies = 0;
static bool rejectFinal = false, nullCreated = false, badMatrix = false;
#ifdef DOMMY_PROCEDURAL_API_TESTS
static bool rejectUvField = false;
#endif
#ifdef DOMMY_AFFECT_SPIRAL_TESTS
static bool shortCenters=false;
#endif
static void resetCompute(int failure = -1) {
    resetCalls(failure); publications = geometryCopies = positionViews = positionCopies = 0;
    rejectFinal = nullCreated = badMatrix = false;
    injectedStatus = MS::kFailure;
}
static void voidApi(const char* name) {
    if (!operation(name)) throw std::runtime_error("injected container failure");
}
struct CopyMatrix : MMatrix {
    static const CopyMatrix identity;
    CopyMatrix operator*(const CopyMatrix& other) const {
        CopyMatrix result;
        for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c) {
            result[r][c]=0;
            for (unsigned k=0;k<4;++k) result[r][c]+=(*this)[r][k]*other[k][c];
        }
        return result;
    }
};
const CopyMatrix CopyMatrix::identity;
#define MMatrix CopyMatrix
#ifdef DOMMY_PROCEDURAL_API_TESTS
struct MVector : Imath::V3d {
    using Imath::V3d::V3d;
    MVector(const Imath::V3d& v):Imath::V3d(v){}
    explicit MVector(const double* v):Imath::V3d(v[0],v[1],v[2]){}
};
#else
using MVector = Imath::V3d;
#endif
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using MTypeId = int;
using double3 = double[3];
using MPoint = Imath::V3d;
struct MBoundingBox {
    MPoint minimum, maximum;
    MPoint min()const{return minimum;}
    MPoint max()const{return maximum;}
};
namespace MSpace { enum Space { kObject, kWorld }; }
namespace dommy { namespace maya {
inline bool isFiniteVector(const MVector& p) {
    return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]);
}
inline bool isFiniteMatrix(const MMatrix& matrix) {return attribute::isFiniteMatrix(matrix);}
} namespace math {
inline double degreesToRadians(double value) {return value * 0.017453292519943295;}
}}
// Records the exact API argument/order contract. The affine model below only
// provides deterministic payloads for control-flow tests, not native Maya math.
struct MTransformationMatrix {
    enum RotationOrder { kXYZ, kYZX, kZXY, kXZY, kYXZ, kZYX };
    MMatrix base;
    MVector rotatePivot, scalePivot, scale{1,1,1}, rotation, translation;
    RotationOrder order = kXYZ;
    MTransformationMatrix() = default;
    explicit MTransformationMatrix(const MMatrix& m):base(m) {}
#ifdef DOMMY_AFFECT_SPIRAL_TESTS
    MVector getTranslation(MSpace::Space, MStatus* status)const {
        *status=operation("matrix.getTranslation");return MVector(base[3][0],base[3][1],base[3][2]);
    }
#endif
    MStatus setRotatePivot(const MVector& v, MSpace::Space, bool balance) {
        assert(!balance); const auto s=operation("matrix.rotatePivot");if(s)rotatePivot=v;return s;
    }
    MStatus setScalePivot(const MVector& v, MSpace::Space, bool balance) {
        assert(!balance);const auto s=operation("matrix.scalePivot");if(s)scalePivot=v;return s;
    }
    MStatus addScale(const double* v, MSpace::Space) {
        const auto s=operation("matrix.scale");if(s)scale=MVector(v[0],v[1],v[2]);return s;
    }
    MStatus addRotation(const double* v, RotationOrder o, MSpace::Space) {
        const auto s=operation("matrix.rotation");if(s){rotation=MVector(v[0],v[1],v[2]);order=o;}return s;
    }
    MStatus addTranslation(const MVector& v, MSpace::Space) {
        const auto s=operation("matrix.translation");if(s)translation=v;return s;
    }
    static MMatrix move(const MVector& v) {
        MMatrix m;for(unsigned i=0;i<3;++i)m[3][i]=v[i];return m;
    }
    MMatrix asMatrix() const {
        voidApi("matrix.result");
        MMatrix s;for(unsigned i=0;i<3;++i)s[i][i]=scale[i];
        const unsigned orders[6][3]={{0,1,2},{1,2,0},{2,0,1},{0,2,1},{1,0,2},{2,1,0}};
        MMatrix r;
        for (unsigned axis:orders[order]) {
            MMatrix turn;unsigned a=(axis+1)%3,b=(axis+2)%3;
            turn[a][a]=turn[b][b]=std::cos(rotation[axis]);
            turn[a][b]=std::sin(rotation[axis]);turn[b][a]=-turn[a][b];r=r*turn;
        }
        MMatrix result=base*move(-scalePivot)*s*move(scalePivot)*move(-rotatePivot)*r*
            move(rotatePivot)*move(translation);
        if(badMatrix)result[0][0]=NAN;
        return result;
    }
};
struct Group {
    using ConstPtr=std::shared_ptr<const Group>;
    std::vector<bool> ids;const std::vector<bool>& getGroupIds()const{return ids;}
};
#ifdef DOMMY_RAY_TESTS
template<typename T>struct RayArray : std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length()const{return static_cast<unsigned int>(this->size());}
};
using MIntArray=RayArray<int>;
#endif
struct Payload {virtual ~Payload(){};virtual int type()const=0;virtual bool valid()const=0;};
struct DPData : Payload {
    static const int id=22;
    DPointList points;
    DPAttrList attributes;
    std::map<std::string,std::shared_ptr<Group>> groups;
    bool topology=true;
    int type()const override{return id;}
    std::size_t numberOfPoints()const{return points.size();}
    auto beginPt()const->DPointList::const_iterator{return points.begin();}
    auto endPt()const->DPointList::const_iterator{return points.end();}
    auto beginAttr()const->ConstDPAttrIter{return attributes.begin();}
    auto endAttr()const->ConstDPAttrIter{return attributes.end();}
    auto beginAttr()->DPAttrIter{return attributes.begin();}
    auto endAttr()->DPAttrIter{return attributes.end();}
    auto beginGrp()const->decltype(groups.begin()){return groups.begin();}
    auto endGrp()const->decltype(groups.end()){return groups.end();}
    bool valid()const override {
        dommy::attribute::ValidationBudget budget(points.size());
        if(!topology || !dommy::attribute::validateAttributeList(attributes,points.size(),budget))return false;
        for(const auto& p:points)for(unsigned i=0;i<3;++i)if(!std::isfinite(p[i]))return false;
        for(const auto& g:groups)if(!g.second||g.second->ids.size()!=points.size())return false;
        return true;
    }
    bool hasValidPointTopology()const{return valid();}
    void setDPData(const DPData& source) {
        voidApi("point.copy");++geometryCopies;points=source.points;topology=source.topology;
        attributes.clear();groups.clear();
        for(const auto& a:source.attributes)attributes[a.first]=a.second->copy();
        for(const auto& g:source.groups)groups[g.first]=std::make_shared<Group>(*g.second);
    }
    DPointList getPositionConst()const{++positionCopies;return points;}
    const DPointList* getPositionPtrConst()const{++positionViews;return &points;}
    DPAttributeData::ConstPtr getAttr(const std::string& name)const {
        auto found=attributes.find(name);return found==attributes.end()?nullptr:found->second;
    }
    bool isExistAttr(const std::string& name)const{return attributes.count(name)!=0;}
#ifdef DOMMY_PROCEDURAL_API_TESTS
    template<typename T>std::shared_ptr<TypedDPAttributeData<T>> getValidTypedAttributedata(const std::string& name){
        return std::dynamic_pointer_cast<TypedDPAttributeData<T>>(attributes.at(name));
    }
#endif
    void removeAttr(const std::string& name){voidApi("point.attribute.remove");attributes.erase(name);}
    void addAttr(const std::string& name,const DPAttributeData& value){
        voidApi("point.attribute.add");
#ifdef DOMMY_PROCEDURAL_API_TESTS
        if(rejectUvField&&name=="rejectedUV")return;
#endif
        attributes[name]=value.copy();
    }
    void addAttrWithValues(const std::string& name,const DPointList& values){addAttr(name,DPVec3fAttr(values));}
    template<typename T>void addAttrWithValues(const std::string& name,const std::vector<T>& values){
        addAttr(name,TypedDPAttributeData<T>(values));
    }
    void setDPAttributeValues(const std::string& name,const DPointList& values){
        voidApi("point.attribute.set");const auto field=getAttr(name);assert(field);
        if(field->getAttrType()==DP_ATTR_VEC3F)attributes[name]=std::make_shared<DPVec3fAttr>(values);
        else if(field->getAttrType()==DP_ATTR_VEC3I){
            std::vector<Imath::V3i> converted;
            for(const auto& value:values){Imath::V3i candidate;
                if(!dommy::numeric::checkedValueCast(value,candidate))throw std::range_error("integer velocity");
                converted.push_back(candidate);}
            attributes[name]=std::make_shared<TypedDPAttributeData<Imath::V3i>>(converted);
        }
        else {assert(field->getAttrType()==DP_ATTR_VEC3D);std::vector<Imath::V3d> converted;
            for(const auto& value:values)converted.push_back(Imath::V3d(value));
            attributes[name]=std::make_shared<DPVec3dAttr>(converted);}
    }
    bool isExistGrp(const std::string& name)const{return groups.count(name)!=0;}
#ifdef DOMMY_RAY_TESTS
    bool replaceGrp(const std::string& name,const DPIdList& mask){
        if(!operation("point.group.replace"))return false;
        if(mask.size()!=points.size())return false;
        auto group=std::make_shared<Group>();group->ids=mask;groups[name]=group;return true;
    }
#endif
    std::shared_ptr<const Group> getGrp(const std::string& name)const{return groups.at(name);}
    bool deletePointFromIds(const DPIdList& ids) {
        if(!operation("point.delete"))return false;
        assert(ids.size()==points.size());DPointList kept;
        for(std::size_t i=0;i<ids.size();++i)if(!ids[i])kept.push_back(points[i]);
        for(auto& a:attributes)if(!a.second->deleteArrayFromIds(ids))return false;
        for(auto& g:groups){std::vector<bool> mask;for(std::size_t i=0;i<ids.size();++i)if(!ids[i])mask.push_back(g.second->ids[i]);g.second->ids.swap(mask);}
        points.swap(kept);return true;
    }
    bool sortPointFromIds(const DPIndexList& ids) {
        if(!operation("point.sort"))return false;
        DPointList sorted;for(auto i:ids)sorted.push_back(points.at(i));
        for(auto& a:attributes)if(!a.second->sortArrayFromIds(ids))return false;
        for(auto& g:groups){std::vector<bool> mask;for(auto i:ids)mask.push_back(g.second->ids.at(i));g.second->ids.swap(mask);}
        points.swap(sorted);return true;
    }
    template<typename String>void removeGrps(const String& names) {
        voidApi("groups.remove");std::istringstream stream(names.asChar());std::string name;
        while(stream>>name)groups.erase(name);
    }
    void getBBox(MBoundingBox& box)const {
        voidApi("bounds.read");box=MBoundingBox();if(points.empty())return;
        for(unsigned axis=0;axis<3;++axis)box.minimum[axis]=box.maximum[axis]=points[0][axis];
        for(const auto& point:points)for(unsigned axis=0;axis<3;++axis){
            box.minimum[axis]=std::min(box.minimum[axis],double(point[axis]));
            box.maximum[axis]=std::max(box.maximum[axis],double(point[axis]));
        }
    }
    bool replacePositions(const DPointList& p) {
        if(!operation("point.replace"))return false;points=p;return true;
    }
    bool mergeDPData(const DPData& source) {
        if(!operation("point.merge"))return false;
        if(source.points.empty())return true;
        points.insert(points.end(),source.points.begin(),source.points.end());
        for(const auto& a:source.attributes) {
            auto found=attributes.find(a.first);
            if(found==attributes.end())attributes[a.first]=a.second->copy();
            else if(!found->second->appendValues(*a.second))return false;
        }
        for(const auto& g:source.groups) {
            auto& out=groups[g.first];if(!out)out=std::make_shared<Group>();
            out->ids.insert(out->ids.end(),g.second->ids.begin(),g.second->ids.end());
        }
        return true;
    }
    // Explicit owning-data boundary. Complete production transactions run in
    // point_batch_transactions; this fixture tests the DG placement contract.
    using PointCopyOperation=std::function<MStatus(DPoint&,std::size_t,std::size_t)>;
    MStatus copyRepeatedFrom(const DPData& source,std::size_t count,const PointCopyOperation& placement) {
        const MStatus status=operation("point.batch");if(!status)return status;
        std::size_t work=0;
        if(!dommy::point::buildPointCopyBudget(source,count,work))return MS::kInvalidParameter;
        ++geometryCopies;DPData candidate;
        for(const auto& field:source.attributes)candidate.attributes[field.first]=field.second->copy();
        for(const auto& group:source.groups)candidate.groups[group.first]=std::make_shared<Group>(*group.second);
        if(!source.points.empty())for(std::size_t step=0;step<count;++step) {
            for(std::size_t index=0;index<source.points.size();++index) {
                DPoint point=source.points[index];const MStatus result=placement(point,step,index);
                if(!result)return result;candidate.points.push_back(point);
            }
            if(step) {
                for(const auto& field:source.attributes)
                    if(!candidate.attributes.at(field.first)->appendValues(*field.second))return MS::kFailure;
                for(const auto& group:source.groups) {
                    auto& mask=candidate.groups.at(group.first)->ids;
                    mask.insert(mask.end(),group.second->ids.begin(),group.second->ids.end());
                }
            }
        }
        if(!candidate.valid())return MS::kFailure;
        candidate.updateBBox();*this=std::move(candidate);return MS::kSuccess;
    }
    void updateBBox(){voidApi("bounds.update");}
};
enum DComponentType { D_COMP_POINT, D_COMP_FACE, D_COMP_PER_VTX, D_COMP_EDGE };
struct Metadata {
    DPAttrList attributes;
    std::map<std::string,std::shared_ptr<Group>> groups;
    auto beginAttr()const->ConstDPAttrIter{return attributes.begin();}
    auto endAttr()const->ConstDPAttrIter{return attributes.end();}
#if defined(DOMMY_POINT_NOISE_TESTS) || defined(DOMMY_COLOR_COMPUTE_TESTS)
    auto beginAttr()->DPAttrIter{return attributes.begin();}
    auto endAttr()->DPAttrIter{return attributes.end();}
#endif
    auto beginGrp()const->decltype(groups.begin()){return groups.begin();}
    auto endGrp()const->decltype(groups.end()){return groups.end();}
#ifdef DOMMY_PROCEDURAL_API_TESTS
    template<typename T>std::shared_ptr<TypedDPAttributeData<T>> getValidTypedAttributedata(const std::string& name){
        return std::dynamic_pointer_cast<TypedDPAttributeData<T>>(attributes.at(name));
    }
#endif
};
struct Primitive : Metadata {
    DPointList points{{1,2,3}};
    MMatrix matrix;
    int faces=1;
    std::size_t connections=3,uvs=3,reportedPoints=0;
    std::map<DComponentType,DPAttrList> componentAttributes;
    std::map<DComponentType,decltype(groups)> componentGroups;
    int tag=7;
    bool member=true;
#ifdef DOMMY_RAY_TESTS
    MIntArray faceCounts{2},indices{0,1};
    bool replaceGrp(const std::string& name,const DPIdList& mask){
        if(!operation("primitive.group.replace"))return false;
        if(mask.size()!=points.size())return false;
        auto group=std::make_shared<Group>();group->ids=mask;groups[name]=group;return true;
    }
    bool isExistCompGrp(DComponentType type,const std::string& name)const{
        return GroupListConst(type)->count(name)!=0;
    }
    Group::ConstPtr getCompGrp(DComponentType type,const std::string& name)const{
        return GroupListConst(type)->at(name);
    }
    const MIntArray& getFaceList()const{return faceCounts;}
    const MIntArray& getConnectList()const{return indices;}
#endif
    DPointList getPositionConst()const{return points;}
#ifdef DOMMY_COLOR_COMPUTE_TESTS
    DPointList getPosition()const{++positionCopies;return points;}
#endif
    std::size_t numberOfPoints()const{return reportedPoints?reportedPoints:points.size();}
    std::size_t numberOfKnots()const{return points.size()+1U;}
    const DPointList* getPositionPtrConst()const{++positionViews;return &points;}
    bool hasValidCurveTopology()const{return true;}
    bool hasValidMeshData()const{return true;}
    bool replacePositions(const DPointList& values){
        if(!operation("primitive.replace"))return false;points=values;return true;
    }
    DPAttributeData::ConstPtr getAttr(const std::string& name)const {
        const auto found=attributes.find(name);return found==attributes.end()?nullptr:found->second;
    }
    using Metadata::beginAttr;
    using Metadata::endAttr;
    auto beginAttr()->DPAttrIter{return attributes.begin();}
    auto endAttr()->DPAttrIter{return attributes.end();}
    bool isCurveOpen()const{return true;}
    const DPointList& getCvs()const{return points;}
    std::vector<double> getKnots()const{return {0,1};}
    int getDegree()const{return 1;}
    bool isExistAttr(const std::string& name)const{return attributes.count(name)!=0;}
    void removeAttr(const std::string& name){voidApi("primitive.point.remove");attributes.erase(name);}
#ifdef DOMMY_PROCEDURAL_API_TESTS
    void addAttr(const std::string& name,const DPAttributeData& value){
        voidApi("primitive.point.add");
        if(rejectUvField&&name=="rejectedUV")return;
        attributes[name]=value.copy();
    }
#endif
    void addAttrWithValues(const std::string& name,const DPointList& values){
        voidApi("primitive.point.add");attributes[name]=std::make_shared<DPVec3fAttr>(values);
    }
    template<typename T>void addAttrWithValues(const std::string& name,const std::vector<T>& values){
        voidApi("primitive.point.add");attributes[name]=std::make_shared<TypedDPAttributeData<T>>(values);
    }
    DPoint getCenter()const {
        voidApi("primitive.center");if(points.empty())return DPoint();
        Imath::V3d mean;for(const auto& p:points)mean+=Imath::V3d(p);
        mean=mean/static_cast<double>(points.size());
        std::array<double,3> transformed;
        if(!dommy::numeric::checkedTransformPoint(mean,matrix,transformed))throw std::runtime_error("center");
        return DPoint(transformed[0],transformed[1],transformed[2]);
    }
    std::size_t numberOfConnections()const{return connections;}
    std::size_t numberOfUVs()const{return uvs;}
    const DPAttrList* AttributeListConst(DComponentType type)const {
        static const DPAttrList empty;const auto found=componentAttributes.find(type);
        return found==componentAttributes.end()?&empty:&found->second;
    }
    const decltype(groups)* GroupListConst(DComponentType type)const {
        static const decltype(groups) empty;const auto found=componentGroups.find(type);
        return found==componentGroups.end()?&empty:&found->second;
    }
    auto beginFaceAttr()const->ConstDPAttrIter{return AttributeListConst(D_COMP_FACE)->begin();}
    auto endFaceAttr()const->ConstDPAttrIter{return AttributeListConst(D_COMP_FACE)->end();}
    auto beginPVtxAttr()const->ConstDPAttrIter{return AttributeListConst(D_COMP_PER_VTX)->begin();}
    auto endPVtxAttr()const->ConstDPAttrIter{return AttributeListConst(D_COMP_PER_VTX)->end();}
    auto beginFaceGrp()const->decltype(groups.begin()){return GroupListConst(D_COMP_FACE)->begin();}
    auto endFaceGrp()const->decltype(groups.end()){return GroupListConst(D_COMP_FACE)->end();}
    auto beginEdgeGrp()const->decltype(groups.begin()){return GroupListConst(D_COMP_EDGE)->begin();}
    auto endEdgeGrp()const->decltype(groups.end()){return GroupListConst(D_COMP_EDGE)->end();}
    int numFace()const{return faces;}
    auto beginPt()->DPointList::iterator{return points.begin();}
    auto endPt()->DPointList::iterator{return points.end();}
    const MMatrix& getTransformMatrix()const{return matrix;}
    void setTransformMatrix(const MMatrix& m){voidApi("primitive.matrix");matrix=m;}
    // Baking is a host boundary here; the real mesh bake transaction is
    // exercised by mesh_normal_tests. Use production point/normal arithmetic
    // so the complete generic node must forward its full matrix correctly.
    MStatus bakeTransform(const MMatrix& m) {
        const auto status=operation("primitive.bake");if(!status)return status;
        auto stagedPoints=points;
        for(auto& p:stagedPoints) {
            std::array<float,3> value;
            if(!dommy::numeric::checkedTransformPoint(p,m,value))return MS::kInvalidParameter;
            p=DPoint(value[0],value[1],value[2]);
        }
        auto found=componentAttributes[D_COMP_PER_VTX].find("__dommy_corner_normal");
        DPAttributeData::Ptr stagedNormals;
        if(found!=componentAttributes[D_COMP_PER_VTX].end()) {
            auto typed=std::dynamic_pointer_cast<DPVec3fAttr>(found->second);
            if(!typed)return MS::kInvalidParameter;
            DPointList values;
            for(const auto& n:typed->value()) {
                std::array<float,3> value;
                if(!dommy::numeric::checkedTransformNormal(n,m,value))return MS::kInvalidParameter;
                values.emplace_back(value[0],value[1],value[2]);
            }
            stagedNormals=std::make_shared<DPVec3fAttr>(values,DPoint(0,0,0));
        }
        points.swap(stagedPoints);
        if(stagedNormals)found->second.swap(stagedNormals);
        matrix=MMatrix::identity;return MS::kSuccess;
    }
};
using DCurveInternalData=Primitive;
using DMeshInternalData=Primitive;
using DPGrpList=std::map<std::string,std::shared_ptr<Group>>;
#include "common/utility/DCurveOperationBudget.h"
template<int Id> struct PrimitiveData : Payload, Metadata {
    static const int id=Id;
    std::vector<std::shared_ptr<Primitive>> primitives;
    bool topology=true;
    int type()const override{return id;}
    auto beginPrims()const->decltype(primitives.begin()){return primitives.begin();}
    auto endPrims()const->decltype(primitives.end()){return primitives.end();}
    using ConstDCurveIter=typename std::vector<std::shared_ptr<Primitive>>::const_iterator;
    using ConstDMeshIter=ConstDCurveIter;
    ConstDCurveIter beginCurves()const{return primitives.begin();}
    ConstDCurveIter endCurves()const{return primitives.end();}
    ConstDMeshIter beginMeshes()const{return primitives.begin();}
    ConstDMeshIter endMeshes()const{return primitives.end();}
    bool isExistAttr(const std::string& name)const{return attributes.count(name)!=0;}
#ifdef DOMMY_PROCEDURAL_API_TESTS
    bool isExistPointAttr(const std::string& name)const{
        return !primitives.empty() && primitives.front()->isExistAttr(name);
    }
#endif
    void removeAttr(const std::string& name){voidApi("primitive.attribute.remove");attributes.erase(name);}
    void addAttr(const std::string& name,const DPAttributeData& value){
        voidApi("primitive.attribute.add");
        if(!dommy::attribute::validAttributeName(name))return;
        const auto existing=attributes.find(name);
        if(existing!=attributes.end()&&existing->second->typeName()!=value.typeName())return;
        attributes[name]=value.copy();
    }
    void addDPAttributeValues(const std::string& name,const DPointList& values){addAttr(name,DPVec3fAttr(values));}
    template<typename T>void addAttrWithValues(const std::string& name,const std::vector<T>& values){
        addAttr(name,TypedDPAttributeData<T>(values));
    }
    template<typename T>void addDPAttributeValues(const std::string& name,const std::vector<T>& values){
        addAttrWithValues(name,values);
    }
    void getPrimitiveCenterList(DPointList& centers)const{
        centers.clear();for(const auto& p:primitives)centers.push_back(p->getCenter());
#ifdef DOMMY_AFFECT_SPIRAL_TESTS
        if(shortCenters)centers.clear();
#endif
    }
    std::size_t numberOfCurves()const{return primitives.size();}
    std::size_t numberOfMesh()const{return primitives.size();}
    bool valid()const override {
        if(!topology)return false;
        for(const auto& p:primitives) {
            if(!p||!dommy::maya::isFiniteMatrix(p->matrix))return false;
            for(const auto& point:p->points)for(unsigned a=0;a<3;++a)
                if(!std::isfinite(point[a]))return false;
        }
        return true;
    }
    bool hasValidCurveTopology()const{return valid();}
    bool hasValidMeshTopology()const{return valid();}
#ifdef DOMMY_POINT_NOISE_TESTS
    bool getCheckedTotalCvs(std::size_t& count)const {
        count=0U;for(const auto& primitive:primitives) {
            if(!primitive || !dommy::generation::checkedSum(count,primitive->numberOfPoints(),
                    dommy::generation::kMaxGeneratedPointCount,count))return false;
        }return true;
    }
    bool getCheckedTotalVertex(std::size_t& count)const{return getCheckedTotalCvs(count);}
#endif
    DPAttributeData::ConstPtr getAttr(const std::string& name)const {
        const auto found=attributes.find(name);return found==attributes.end()?nullptr:found->second;
    }
    MStatus copyFrom(const PrimitiveData& source) {
        const auto s=operation("primitive.copy");if(!s)return s;++geometryCopies;
#ifdef DOMMY_PROCEDURAL_API_TESTS
        topology=source.topology;attributes.clear();
        for(const auto& a:source.attributes)attributes[a.first]=a.second->copy();
        groups=source.groups;primitives.clear();
        for(const auto& p:source.primitives){
            auto copy=std::make_shared<Primitive>(*p);copy->attributes.clear();
            for(const auto& a:p->attributes)copy->attributes[a.first]=a.second->copy();
            primitives.push_back(copy);
        }
#else
        attributes=source.attributes;groups=source.groups;
        primitives.clear();for(const auto& p:source.primitives)primitives.push_back(std::make_shared<Primitive>(*p));
#endif
        return s;
    }
    template<typename Operation>
    MStatus copyRepeatedFrom(const PrimitiveData& source,std::size_t copies,Operation callback) {
        // Owning collection transport boundary; real mesh/curve transactions
        // and their clone/bounds counters are executed in separate fixtures.
        const auto status=operation("primitive.copy");if(!status)return status;++geometryCopies;
        PrimitiveData candidate;candidate.attributes=source.attributes;candidate.groups=source.groups;
        for(std::size_t step=0;step<copies;++step)for(std::size_t i=0;i<source.primitives.size();++i) {
            auto item=std::make_shared<Primitive>(*source.primitives[i]);
            if(step){const auto result=callback(*item,step,i);if(!result)return result;}
            candidate.primitives.push_back(item);
        }
        candidate.updateBBox();primitives.swap(candidate.primitives);
        attributes.swap(candidate.attributes);groups.swap(candidate.groups);return MS::kSuccess;
    }
    bool expand(const std::shared_ptr<Primitive>& p,std::size_t index) {
        if(!operation("primitive.expand"))return false;
        auto copy=std::make_shared<Primitive>(*p);copy->tag=primitives.at(index)->tag;
        copy->member=primitives.at(index)->member;primitives.push_back(copy);return true;
    }
    void updateBBox(){voidApi("bounds.update");}
};
using DCurveData=PrimitiveData<23>;
using DMeshData=PrimitiveData<24>;
static DPData inputPoints,targetPoints,outputPoints;
static DCurveData inputCurves,outputCurves;
static DMeshData inputMeshes,outputMeshes;
struct Input {
    double number=0;MVector vector;const Payload* payload=nullptr;int type=0;
    std::string text="weight";std::map<std::string,Input> children;
#ifdef DOMMY_PROCEDURAL_API_TESTS
    // Native array-valued getters refer to data-block-owned storage, not the
    // lifetime of a temporary MDataHandle wrapper.
    mutable double3 double3Value={0,0,0};
    using Double2=double[2];mutable Double2 double2Value={0,0};
#endif
#ifdef DOMMY_AFFECT_SPIRAL_TESTS
    MMatrix matrix;
#endif
};
struct CopyString {
    std::string value;
#ifdef DOMMY_PROCEDURAL_API_TESTS
    CopyString()=default;
    CopyString(const char* s):value(s){}
    CopyString(std::string s):value(std::move(s)){}
    MStatus split(char,std::vector<CopyString>&)const;
#endif
    const char* asChar()const{return value.c_str();}
    unsigned int length()const{return static_cast<unsigned int>(value.size());}
};
#define MString CopyString
struct MDataHandle {
    bool valid=false;const Input* input=nullptr;
    MDataHandle()=default;
    MDataHandle(bool v,const Input* i):valid(v),input(i){}
    mutable double3 vectorCache={0,0,0};
    using Float3=float[3];using Float2=float[2];
    mutable Float3 float3Cache={0,0,0};mutable Float2 float2Cache={0,0};
    void check()const{assert(valid&&input);}
#ifdef DOMMY_PROCEDURAL_API_TESTS
    struct TypeId {int value;int id()const{return value;}operator int()const{return value;}};
    TypeId typeId()const{check();return {input->type};}
    using Double2=double[2];mutable Double2 double2Cache={0,0};
    const Double2& asDouble2()const{check();for(unsigned i=0;i<2;++i)input->double2Value[i]=input->vector[i];return input->double2Value;}
    MStatus set(const MObject&);
    void setClean(){voidApi("handle.clean");}
#else
    int typeId()const{check();return input->type;}
#endif
    MFnData::Type type()const{check();return static_cast<MFnData::Type>(input->type);}
#ifdef DOMMY_PROCEDURAL_API_TESTS
    MObject data(){check();return MObject{input->text};}
#else
    MObject data()const{check();return MObject{input->text};}
#endif
    int asInt()const{check();return static_cast<int>(input->number);}
    short asShort()const{check();return static_cast<short>(input->number);}
    double asDouble()const{check();return input->number;}
    float asFloat()const{check();return static_cast<float>(input->number);}
    MString asString()const{check();return MString{input->text};}
    MDataHandle child(const MObject& attribute) {
        check();return MDataHandle{true,&input->children.at(attribute.name)};
    }
    bool asBool()const{check();return input->number!=0;}
    MVector asVector()const{check();return input->vector;}
#ifdef DOMMY_COLOR_COMPUTE_TESTS
    MVector asFloatVector()const{check();return input->vector;}
#endif
#ifdef DOMMY_AFFECT_SPIRAL_TESTS
    MMatrix asMatrix()const{check();return input->matrix;}
#endif
    const double3& asDouble3()const {
#ifdef DOMMY_PROCEDURAL_API_TESTS
        check();for(unsigned i=0;i<3;++i)input->double3Value[i]=input->vector[i];return input->double3Value;
#else
        check();for(unsigned i=0;i<3;++i)vectorCache[i]=input->vector[i];return vectorCache;
#endif
    }
    const Float3& asFloat3()const {
        check();for(unsigned i=0;i<3;++i)float3Cache[i]=static_cast<float>(input->vector[i]);return float3Cache;
    }
    const Float2& asFloat2()const {
        check();for(unsigned i=0;i<2;++i)float2Cache[i]=static_cast<float>(input->vector[i]);return float2Cache;
    }
};
#if defined(DOMMY_AFFECT_SPIRAL_TESTS) || defined(DOMMY_RAY_TESTS) || defined(DOMMY_PROCEDURAL_API_TESTS)
static std::set<std::string> connectedPlugs;
#endif
struct MPlug {MObject attribute;
#ifdef DOMMY_PROCEDURAL_API_TESTS
    MPlug(const MObject& a):attribute(a){}
#else
    explicit MPlug(const MObject& a):attribute(a){}
#endif
#if defined(DOMMY_AFFECT_SPIRAL_TESTS) || defined(DOMMY_RAY_TESTS) || defined(DOMMY_PROCEDURAL_API_TESTS)
    MPlug(const MObject&,const MObject& a):attribute(a){}
    bool isConnected(MStatus* status=nullptr)const {
        const MStatus s=operation("plug.connected "+attribute.name);if(status)*status=s;
        return connectedPlugs.count(attribute.name)!=0;
    }
#endif
    bool operator!=(const MObject& other)const{return attribute.name!=other.name;}};
#ifdef DOMMY_PROCEDURAL_API_TESTS
inline bool operator==(const MPlug& p,const MObject& o){return !(p!=o);}
#endif
struct MArrayDataHandle {
    bool valid=false;const std::vector<Input>* elements=nullptr;std::size_t position=0;
    unsigned int reportedCount=0;
    MArrayDataHandle(bool v,const std::vector<Input>* e,std::size_t p,unsigned int c)
        :valid(v),elements(e),position(p),reportedCount(c){}
    unsigned int elementCount(MStatus* status=nullptr)const {
        assert(valid);const MStatus s=operation("array.count");if(status)*status=s;
        return reportedCount?reportedCount:static_cast<unsigned int>(elements->size());
    }
    MStatus jumpToArrayElement(unsigned int index) {
        assert(valid);const auto status=operation("array.jump");if(status)position=index;return status;
    }
    MDataHandle inputValue(MStatus* status)const {
        assert(valid);*status=operation("array.element");return MDataHandle{bool(*status),&elements->at(position)};
    }
#ifdef DOMMY_PROCEDURAL_API_TESTS
    MStatus next(){assert(valid);const auto s=operation("array.next");if(s)++position;return s;}
#endif
};
struct MDataBlock {
    std::map<std::string,Input> inputs;
    std::vector<Input> elements;
    unsigned int reportedCount=0;
    MArrayDataHandle inputArrayValue(const MObject&,MStatus* status) {
        *status=operation("array.input");return MArrayDataHandle{bool(*status),&elements,0,reportedCount};
    }
    MDataHandle inputValue(const MObject& attr,MStatus* status=nullptr) {
        const MStatus s=operation("input "+attr.name);if(status)*status=s;
        MDataHandle h;h.valid=bool(s);h.input=&inputs[attr.name];
#ifdef DOMMY_PROCEDURAL_API_TESTS
        // A failed acquisition may still return a valid-looking handle.
        h.valid=true;
#endif
        return h;
    }
#ifdef DOMMY_PROCEDURAL_API_TESTS
    MDataHandle outputValue(const MObject&,MStatus* status=nullptr){
        const MStatus s=operation("output.handle");if(status)*status=s;
        // A failure can still return a valid-looking handle: status must be checked.
        return MDataHandle{true,nullptr};
    }
#endif
    MStatus setClean(const MPlug&){return operation("output.clean");}
};
namespace dommy {namespace maya {
template<typename T>MStatus getPluginDataFromHandle(const MDataHandle& h,int id,const T*& result) {
    h.check();const auto s=operation("input.bind");if(!s)return s;
    if(h.typeId()!=id)return MS::kFailure;
    result=dynamic_cast<const T*>(h.input->payload);return result?MS::kSuccess:MS::kFailure;
}
inline DPData& outputFor(DPData*){return outputPoints;}
inline DCurveData& outputFor(DCurveData*){return outputCurves;}
inline DMeshData& outputFor(DMeshData*){return outputMeshes;}
template<typename T,typename Populate>
MStatus setPluginDataOutput(MDataBlock&,const MObject&,int,Populate populate) {
    MStatus s=operation("output.create");if(!s)return s;
    if(nullCreated)return MS::kFailure;
    T candidate;s=populate(candidate);if(!s)return s;
    if(rejectFinal||!candidate.valid())return MS::kInvalidParameter;
    s=operation("output.handle");if(!s)return s;
    s=operation("output.publish");if(!s)return s;
    outputFor(static_cast<T*>(nullptr))=candidate;++publications;return s;
}
}}
