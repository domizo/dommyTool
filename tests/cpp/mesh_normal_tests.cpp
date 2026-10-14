#include "common/utility/DPointTransform.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DStatusOperation.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <cstdint>
#include <map>
// Recording Maya/storage boundaries only; no native normals or triangulation.
static int calls=0,failAt=-1,throwAt=-1;
static MStatus operation(){const int call=calls++;if(call==throwAt)throw std::runtime_error("host failure");
    return call==failAt?MS::kFailure:MS::kSuccess;}
struct MObject {bool isNull()const{return false;}};
struct MMatrix {
    double data[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    const double* operator[](int i)const{return data[i];}double* operator[](int i){return data[i];}
    static const MMatrix identity;
};
const MMatrix MMatrix::identity;
struct Vector {
    double x=0,y=0,z=0;
    Vector()=default;Vector(double a,double b,double c):x(a),y(b),z(c){}
    double operator[](int i)const{return i==0?x:i==1?y:z;}
};
using DPoint=Vector;using MVector=Vector;using DPointList=std::vector<DPoint>;
template<typename T>struct Array:std::vector<T>{
    using std::vector<T>::vector;
    unsigned int length()const{return this->size();}
    MStatus setLength(unsigned int n){auto s=operation();if(s)this->resize(n);return s;}
};
using MIntArray=Array<int>;using MFloatArray=Array<float>;using MVectorArray=Array<Vector>;
using MFloatVectorArray=MVectorArray;using MPointArray=MVectorArray;
namespace MSpace {enum Space{kObject,kWorld};}
enum class DMeshNormalImportPolicy {Recompute,PreserveFaceVertex};
struct MGlobal {static void displayWarning(const char*){}};
struct DPAttributeData {using Ptr=std::shared_ptr<DPAttributeData>;virtual ~DPAttributeData(){};};
struct DPVec3fAttr:DPAttributeData {
    static bool allocationFails;
    DPointList values;DPVec3fAttr(const DPointList& v,const DPoint&):values(v){if(allocationFails)throw std::bad_alloc();}
    const DPointList& value()const{return values;}
};
bool DPVec3fAttr::allocationFails=false;
const int D_COMP_PER_VTX=4;
struct DMeshInternalData {
    using Ptr=std::shared_ptr<DMeshInternalData>;
    struct Arrays {MIntArray mFaceList,mConnectList,m_uv_count_list,m_uv_connect_list;MFloatArray m_u_vals,m_v_vals;};
    std::shared_ptr<Arrays> arrays_{new Arrays};
    DPointList pointArray;MMatrix m_mat;std::map<std::string,DPAttributeData::Ptr> mPVtxAttributeList;
    DMeshInternalData()=default;
    DMeshInternalData(const MPointArray& points,const MIntArray& faces,const MIntArray& ids) {
        for(unsigned int i=0;i<points.length();++i)pointArray.emplace_back(points[i].x,points[i].y,points[i].z);
        arrays_->mFaceList=faces;arrays_->mConnectList=ids;
    }
    MIntArray getFaceList()const{return arrays_->mFaceList;}
    MIntArray getConnectList()const{return arrays_->mConnectList;}
    MMatrix getTransformMatrix()const{return m_mat;}
    bool isExistCompAttr(int,const char* key)const{return mPVtxAttributeList.count(key);}
    std::shared_ptr<const DPAttributeData> getCompAttr(int,const char* key)const{
        auto it=mPVtxAttributeList.find(key);return it==mPVtxAttributeList.end()?nullptr:it->second;}
    bool addCompAttr(int,const char* key,const DPVec3fAttr& attr){auto s=operation();if(s)mPVtxAttributeList[key]=std::make_shared<DPVec3fAttr>(attr);return bool(s);}
    std::size_t numberOfConnections()const{return arrays_->mConnectList.length();}
    bool hasUV()const{return arrays_->m_u_vals.length()!=0;}
    bool hasValidMeshData()const{return true;}
    bool hasValidPolygonTopology()const{return true;}
    bool setUvData(const MFloatArray& u,const MFloatArray& v,const MIntArray& counts,const MIntArray& ids) {
        if(!operation())return false;
        arrays_->m_u_vals=u;arrays_->m_v_vals=v;arrays_->m_uv_count_list=counts;arrays_->m_uv_connect_list=ids;
        return true;
    }
    void setTransformMatrix(const MMatrix& matrix){m_mat=matrix;}
    static MStatus createFromMayaObject(const MObject&,Ptr&,MSpace::Space=MSpace::kObject,
        DMeshNormalImportPolicy=DMeshNormalImportPolicy::Recompute);
    static MStatus createMerged(const std::vector<const DMeshInternalData*>&,Ptr&);
    MStatus bakeTransform(const MMatrix&);
};
static MIntArray hostCounts{3,3},hostIds{0,0,0,1,1,1};
static MFloatVectorArray hostNormals{Vector(1,1,0),Vector(0,0,1)};
static MPointArray hostPoints{Vector(1,2,3),Vector(0,1,0),Vector(1,0,0),Vector(1,1,1)};
static bool hostHasUV=true;
static bool importing=false,overrideImportCounts=false;
static int importCounts[4]={4,2,6,4};
static int geometryArrayReads=0,arrayMismatch=-1;
static int reportedNormalCount=-1;
static int normalArrayReads=0;
static MSpace::Space pointSpace=MSpace::kObject,normalSpace=MSpace::kObject;
static MVectorArray writtenNormals;static MIntArray writtenFaces,writtenVertices;
static std::vector<MIntArray> hostPolygons{{0,1,2,3,4},{4,5,6},{1,2,5,6}};
struct MFnMesh {
    MFnMesh()=default;MFnMesh(const MObject&,MStatus* s){*s=operation();}
    int numNormals(MStatus* s){*s=operation();return reportedNormalCount<0?int(hostNormals.size()):reportedNormalCount;}
    MStatus getNormalIds(MIntArray& counts,MIntArray& ids){++normalArrayReads;auto s=operation();if(s){counts=hostCounts;ids=hostIds;}return s;}
    MStatus getNormals(MFloatVectorArray& normals,MSpace::Space space){++normalArrayReads;auto s=operation();if(s){normals=hostNormals;normalSpace=space;}return s;}
    MStatus getPoints(MPointArray& points,MSpace::Space space){++geometryArrayReads;auto s=operation();if(s){points=hostPoints;pointSpace=space;
        if(arrayMismatch==0)points.pop_back();}return s;}
    MStatus getVertices(MIntArray& faces,MIntArray& ids){++geometryArrayReads;auto s=operation();if(s){faces={3,3};ids={0,1,2,2,1,3};
        if(arrayMismatch==1)faces.pop_back();if(arrayMismatch==2)ids.pop_back();}return s;}
    int numUVs(MStatus* s){*s=operation();return overrideImportCounts?importCounts[3]:(hostHasUV?4:0);}
    MStatus getUVs(MFloatArray& u,MFloatArray& v){++geometryArrayReads;auto s=operation();if(s){u={0,1,0,1};v={0,0,1,1};
        if(arrayMismatch==3)u.pop_back();if(arrayMismatch==4)v.pop_back();}return s;}
    MStatus getAssignedUVs(MIntArray& counts,MIntArray& ids){++geometryArrayReads;auto s=operation();if(s){counts={3,3};ids={0,1,2,2,1,3};
        if(arrayMismatch==5)counts.pop_back();if(arrayMismatch==6)ids.push_back(0);}return s;}
    MStatus setFaceVertexNormals(MVectorArray& n,MIntArray& f,MIntArray& v,MSpace::Space){
        auto s=operation();if(s){writtenNormals=n;writtenFaces=f;writtenVertices=v;}return s;}
    int numVertices(MStatus* s){*s=operation();return overrideImportCounts?importCounts[0]:(importing?4:7);}
    int numPolygons(MStatus* s){*s=operation();return overrideImportCounts?importCounts[1]:(importing?2:int(hostPolygons.size()));}
    int numFaceVertices(MStatus* s){*s=operation();return overrideImportCounts?importCounts[2]:6;}
};
struct MItMeshPolygon {
    std::size_t cursor=0;
    MItMeshPolygon(const MObject&,MStatus* s){*s=operation();}
    bool isDone(MStatus* s){*s=operation();return cursor==hostPolygons.size();}
    int index(MStatus* s){*s=operation();return cursor;}
    MStatus getVertices(MIntArray& out){auto s=operation();if(s)out=hostPolygons.at(cursor);return s;}
    MStatus getTriangles(MPointArray&,MIntArray& out,MSpace::Space){auto s=operation();if(s)out={0,1,2,0,2,3,0,3,4};return s;}
    MStatus next(){auto s=operation();if(s)++cursor;return s;}
};
namespace openvdb {
using Index32=std::uint32_t;
struct Vec4I {Index32 values[4];Vec4I(Index32 a,Index32 b,Index32 c,Index32 d):values{a,b,c,d}{}};
namespace util {const Index32 INVALID_IDX=UINT32_MAX;}
}
struct MTransformationMatrix;
using DMItem=DMeshInternalData::Ptr;
#include "mesh_normal.inc"
static DMeshInternalData fixture(){DMeshInternalData m;m.pointArray.resize(4);
    m.arrays_->mFaceList={3,3};m.arrays_->mConnectList={0,1,2,2,1,3};return m;}
static void reset(){calls=0;failAt=-1;throwAt=-1;normalArrayReads=0;geometryArrayReads=0;
    writtenNormals.clear();writtenFaces.clear();writtenVertices.clear();}
static void testBake() {
    for(int kind=0;kind<4;++kind) {
        auto mesh=fixture();mesh.pointArray[0]=DPoint(1,2,3);
        reset();assert(dommy::mesh::captureCornerNormals(MObject(),mesh));
        auto source=mesh;
        MMatrix matrix;
        if(kind==0){matrix[0][0]=2;matrix[1][1]=3;}
        if(kind==1){matrix[0][0]=0;matrix[0][1]=1;matrix[1][0]=-1;matrix[1][1]=0;}
        if(kind==2)matrix[0][0]=-1;
        if(kind==3)matrix[0][1]=.5;
        matrix[3][0]=10;mesh.m_mat=matrix;
        DPointList expectedNormals;
        assert(dommy::mesh::appendCornerNormals(mesh,true,expectedNormals));
        std::array<float,3> expectedPoint;
        assert(dommy::numeric::checkedTransformPoint(mesh.pointArray[0],matrix,expectedPoint));
        assert(mesh.bakeTransform(matrix));
        for(int i=0;i<4;++i)for(int j=0;j<4;++j)assert(mesh.m_mat[i][j]==MMatrix::identity[i][j]);
        for(int i=0;i<3;++i)assert(mesh.pointArray[0][i]==expectedPoint[i]);
        DPointList actual;assert(dommy::mesh::appendCornerNormals(mesh,true,actual));
        for(std::size_t n=0;n<actual.size();++n)for(int i=0;i<3;++i)
            assert(std::fabs(actual[n][i]-expectedNormals[n][i])<1e-6);
        assert(source.pointArray[0].x==1); // Shared metadata must not be mutated.
        assert(source.getCompAttr(4,dommy::mesh::cornerNormalAttribute())!=
               mesh.getCompAttr(4,dommy::mesh::cornerNormalAttribute()));
        const auto beforePoints=mesh.pointArray;const auto beforeAttrs=mesh.mPVtxAttributeList;
        for(int bad=0;bad<4;++bad) {
            auto invalid=matrix;
            if(bad==0)invalid[0][0]=NAN;
            if(bad==1)invalid[0][3]=1;
            if(bad==2){for(int i=0;i<3;++i)invalid[0][i]=0;}
            if(bad==3)invalid[3][0]=std::numeric_limits<double>::max();
            assert(!mesh.bakeTransform(invalid));
            assert(mesh.mPVtxAttributeList==beforeAttrs && mesh.pointArray[0].x==beforePoints[0].x);
            assert(mesh.m_mat[3][0]==0);
        }
        DPVec3fAttr::allocationFails=true;
        assert(!mesh.bakeTransform(matrix));DPVec3fAttr::allocationFails=false;
        assert(mesh.mPVtxAttributeList==beforeAttrs && mesh.pointArray[0].x==beforePoints[0].x);
    }
    auto bare=fixture();MMatrix singular;singular[0][0]=0;
    assert(bare.bakeTransform(singular)); // No explicit directions to invert.
    auto mixed=fixture();reset();assert(dommy::mesh::captureCornerNormals(MObject(),mixed));
    auto attr=std::dynamic_pointer_cast<DPVec3fAttr>(mixed.mPVtxAttributeList.begin()->second);
    attr->values[0]=Vector();MMatrix scale;scale[0][0]=2;
    assert(mixed.bakeTransform(scale));
    assert(std::dynamic_pointer_cast<DPVec3fAttr>(mixed.mPVtxAttributeList.begin()->second)->value()[0].x==0);
    mixed.mPVtxAttributeList.begin()->second=std::make_shared<DPAttributeData>();
    assert(!mixed.bakeTransform(scale));
}
static void testImport() {
    importing=true;
    // The default factory remains automatic for generated geometry.
    reset();DMItem generated;
    assert(DMeshInternalData::createFromMayaObject(MObject(),generated));
    assert(!dommy::mesh::hasCornerNormals(*generated));
    for(bool uv:{false,true})for(bool bake:{false,true})for(int kind=0;kind<4;++kind) {
        hostHasUV=uv;MeshInput input;input.hasWorldMatrix=true;
        if(kind==0){input.worldMatrix[0][0]=2;input.worldMatrix[1][1]=3;}
        if(kind==1){input.worldMatrix[0][0]=0;input.worldMatrix[0][1]=1;input.worldMatrix[1][0]=-1;input.worldMatrix[1][1]=0;}
        if(kind==2)input.worldMatrix[0][0]=-1;
        if(kind==3)input.worldMatrix[0][1]=.5;
        input.worldMatrix[3][0]=10;
        reset();DMItem item;assert(createFullMeshItem(input,bake,item));
        const int count=calls;
        assert(dommy::mesh::hasCornerNormals(*item)&&item->hasUV()==uv);
        assert(pointSpace==MSpace::kObject&&normalSpace==MSpace::kObject);
        MFnMesh output;assert(dommy::mesh::restoreCornerNormals(output,*item,true));
        assert(writtenNormals.size()==6&&writtenVertices[5]==3&&writtenFaces[3]==1);
        for(std::size_t i=0;i<6;++i){
            std::array<float,3> expected;
            assert(dommy::numeric::checkedTransformNormal(hostNormals[hostIds[i]],input.worldMatrix,expected));
            for(int axis=0;axis<3;++axis)assert(std::fabs(writtenNormals[i][axis]-expected[axis])<1e-6);
        }
        std::array<float,3> expected;
        assert(dommy::numeric::checkedTransformPoint(hostPoints[0],input.worldMatrix,expected));
        std::array<float,3> actual;
        assert(dommy::numeric::checkedTransformPoint(item->pointArray[0],item->m_mat,actual));
        for(int axis=0;axis<3;++axis)assert(std::fabs(actual[axis]-expected[axis])<1e-6);
        for(int fail=0;fail<count;++fail){reset();failAt=fail;auto before=item;
            assert(!createFullMeshItem(input,bake,item)&&item==before);}
        if(bake){reset();auto before=item;input.worldMatrix[0][0]=NAN;
            assert(!createFullMeshItem(input,bake,item)&&item==before);}
    }
    hostHasUV=true;
    reset();DMItem item;assert(DMeshInternalData::createFromMayaObject(MObject(),item,MSpace::kWorld,
        DMeshNormalImportPolicy::PreserveFaceVertex));
    assert(pointSpace==MSpace::kWorld&&normalSpace==MSpace::kWorld);
    auto before=item;hostIds[0]=99;reset();
    assert(!DMeshInternalData::createFromMayaObject(MObject(),item,MSpace::kObject,
        DMeshNormalImportPolicy::PreserveFaceVertex)&&item==before);hostIds[0]=0;
    reset();DPVec3fAttr::allocationFails=true;
    assert(!DMeshInternalData::createFromMayaObject(MObject(),item,MSpace::kObject,
        DMeshNormalImportPolicy::PreserveFaceVertex)&&item==before);DPVec3fAttr::allocationFails=false;
    reset();MeshInput input;assert(createFullMeshItem(input,false,item)&&item->m_mat[3][0]==0);
    for(int excessive:{20000000,std::numeric_limits<int>::max()}) {
        reset();reportedNormalCount=excessive;before=item;
        assert(!createFullMeshItem(input,false,item)&&item==before&&normalArrayReads==0);
    }
    reportedNormalCount=-1;
    // All scalar queries precede geometry reads, including native-generated
    // callers of the common factory. Oversized reports allocate no huge arrays.
    overrideImportCounts=true;
    for(int field=0;field<4;++field)for(int value:{-1,std::numeric_limits<int>::max()}) {
        const int old=importCounts[field];importCounts[field]=value;
        reset();before=item;
        assert(!DMeshInternalData::createFromMayaObject(MObject(),item)&&item==before);
        assert(geometryArrayReads==0);importCounts[field]=old;
    }
    importCounts[0]=1000000;importCounts[1]=1000000;importCounts[2]=4000000;importCounts[3]=0;
    reset();assert(!DMeshInternalData::createFromMayaObject(MObject(),item)&&geometryArrayReads==0);
    overrideImportCounts=false;
    for(int mismatch=0;mismatch<7;++mismatch) {
        reset();arrayMismatch=mismatch;before=item;
        assert(!DMeshInternalData::createFromMayaObject(MObject(),item)&&item==before);
    }
    arrayMismatch=-1;
    for(auto policy:{DMeshNormalImportPolicy::Recompute,DMeshNormalImportPolicy::PreserveFaceVertex}) {
        reset();assert(DMeshInternalData::createFromMayaObject(MObject(),item,MSpace::kObject,policy));
        const int importCalls=calls;
        for(int fail=0;fail<importCalls;++fail)for(bool throws:{false,true}) {
            reset();before=item;if(throws)throwAt=fail;else failAt=fail;
            assert(!DMeshInternalData::createFromMayaObject(MObject(),item,MSpace::kObject,policy));
            assert(item==before);
            if(fail<5)assert(geometryArrayReads==0);
        }
    }
    reset();before=item;
    assert(!DMeshInternalData::createFromMayaObject(MObject(),item,MSpace::kObject,
        static_cast<DMeshNormalImportPolicy>(99))&&item==before&&calls==0);
    importing=false;
    std::cout<<"Maya mesh import: preserved corner normals, UV/no-UV, 16 bake/transform cases and failure atomicity passed\n";
    std::cout<<"Common import preflight: excessive/negative counts, storage budget, 7 array mismatches and all status/exception boundaries passed\n";
}
// A reduced policy exercises complete production merge bodies with tiny
// geometry. Only the byte limit is shadowed in a disposable test header.
static void testMergeStorage() {
    using namespace dommy::mesh;
    assert(kMaxMeshOperationBytes == 1500U);
    auto first = fixture(), second = fixture();
    MeshMergePlan plan;
    assert(appendMeshMergePlan(4U, 2U, 6U, 0U, plan) && validMeshOperationStorage(plan));
    assert(appendMeshMergePlan(4U, 2U, 6U, 0U, plan) && !validMeshOperationStorage(plan));
    auto merged = std::make_shared<DMeshInternalData>();
    const auto previous = merged;
    reset();
    assert(DMeshInternalData::createMerged({&first, &second}, merged) == MS::kInvalidParameter);
    assert(merged == previous && calls == 0);
    assert(DMeshInternalData::createMerged({&first}, merged));
    assert(merged != previous && merged->pointArray.size() == 4U);

    assert(first.addCompAttr(D_COMP_PER_VTX, cornerNormalAttribute(),
        DPVec3fAttr(DPointList(6U, Vector(0,0,1)), Vector())));
    reset();
    assert(DMeshInternalData::createMerged({&first}, merged) && hasCornerNormals(*merged));

    auto triangle = fixture();
    triangle.pointArray.resize(3U);
    triangle.arrays_->mFaceList = {3};
    triangle.arrays_->mConnectList = {0,1,2};
    auto ordinary = triangle;
    assert(triangle.addCompAttr(D_COMP_PER_VTX, cornerNormalAttribute(),
        DPVec3fAttr(DPointList(3U, Vector(0,0,1)), Vector())));
    plan = MeshMergePlan();
    assert(appendMeshMergePlan(3U, 1U, 3U, 0U, plan));
    assert(appendMeshMergePlan(3U, 1U, 3U, 0U, plan));
    assert(validMeshOperationStorage(plan) && !validMeshOperationStorage(plan, 6U * 64U));
    const auto beforeNormals = merged;
    reset();
    assert(DMeshInternalData::createMerged({&triangle, &ordinary}, merged) == MS::kInvalidParameter);
    assert(merged == beforeNormals && calls == 0);
    assert(triangle.pointArray.size() == 3U && ordinary.mPVtxAttributeList.empty());
    assert(DMeshInternalData::createMerged({&ordinary, &ordinary}, merged));
    assert(merged->pointArray.size() == 6U && !hasCornerNormals(*merged));
    std::cout << "Merge byte admission is unconditional, includes normal staging and preserves output on rejection\n";
}
int main(int argc, char** argv){
    if (argc == 2 && std::string(argv[1]) == "--merge-storage") {
        testMergeStorage();
        return 0;
    }
    assert(argc == 1);
    testImport();
    testBake();
    meshPrimData primitive;
    MVectorArray primitiveNormals;MIntArray primitiveFaces,primitiveVertices;
    reset();assert(primitive.setNormalPerVtxPerFace(primitiveNormals,primitiveFaces,primitiveVertices) && primitive.hasExplicitNormals);
    const int primitiveCalls=calls;
    for(int i=0;i<primitiveCalls;++i){reset();failAt=i;primitive.hasExplicitNormals=false;
        assert(!primitive.setNormalPerVtxPerFace(primitiveNormals,primitiveFaces,primitiveVertices) && !primitive.hasExplicitNormals);}
    reset();
    auto mesh=fixture();assert(dommy::mesh::captureCornerNormals(MObject(),mesh));
    MFnMesh output;assert(dommy::mesh::restoreCornerNormals(output,mesh,false));
    assert(writtenNormals.size()==6 && writtenFaces[3]==1 && writtenVertices[5]==3);
    assert(std::fabs(writtenNormals[0].x-std::sqrt(.5))<1e-6);
    mesh.m_mat[0][0]=2;mesh.m_mat[3][0]=123;
    reset();assert(dommy::mesh::restoreCornerNormals(output,mesh,true));
    assert(std::fabs(writtenNormals[0].x-1/std::sqrt(5.))<1e-6);
    assert(std::fabs(writtenNormals[0].y-2/std::sqrt(5.))<1e-6);
    mesh.m_mat[0][0]=-2;
    reset();assert(dommy::mesh::restoreCornerNormals(output,mesh,true));assert(writtenNormals[0].x<0);
    auto bare=fixture();DMeshInternalData::Ptr merged;
    assert(DMeshInternalData::createMerged({&mesh,&bare},merged));
    assert(merged->getConnectList().length()==12);
    reset();assert(dommy::mesh::restoreCornerNormals(output,*merged,true));
    assert(writtenNormals.size()==6 && writtenNormals[0].x<0); // Automatic half not overridden.
    mesh.m_mat[0][0]=0;auto preserved=merged;
    assert(!DMeshInternalData::createMerged({&mesh},merged) && merged==preserved);
    mesh.m_mat=MMatrix();mesh.m_mat[0][1]=1; // shear: normal remains perpendicular to transformed tangent
    std::array<double,3> n;assert(dommy::numeric::checkedTransformNormal(Vector(0,1,0),mesh.m_mat,n));
    assert(std::fabs(n[0]+n[1])<1e-12);
    mesh.m_mat=MMatrix();
    int injections=0;
    reset();assert(dommy::mesh::restoreCornerNormals(output,mesh,false));const int count=calls;
    for(int i=0;i<count;++i){reset();failAt=i;assert(!dommy::mesh::restoreCornerNormals(output,mesh,false));assert(writtenNormals.empty());++injections;}
    reset();auto fresh=fixture();assert(dommy::mesh::captureCornerNormals(MObject(),fresh));const int reads=calls;
    for(int i=0;i<reads;++i){reset();failAt=i;fresh=fixture();assert(!dommy::mesh::captureCornerNormals(MObject(),fresh));assert(fresh.mPVtxAttributeList.empty());++injections;}
    reset();hostIds[5]=99;fresh=fixture();assert(!dommy::mesh::captureCornerNormals(MObject(),fresh)&&fresh.mPVtxAttributeList.empty());hostIds[5]=1;
    hostNormals[0].x=NAN;assert(!dommy::mesh::captureCornerNormals(MObject(),fresh)&&fresh.mPVtxAttributeList.empty());hostNormals[0].x=1;
    std::vector<openvdb::Vec4I> primitives;std::vector<int> faces;
    reset();assert(DOpenVDBMakePrimList(MObject(),primitives,&faces));
    assert(primitives.size()==5 && (faces==std::vector<int>{0,0,0,1,2}));const int mapCalls=calls;
    for(int i=0;i<mapCalls;++i){reset();failAt=i;auto before=faces;
        assert(!DOpenVDBMakePrimList(MObject(),primitives,&faces));assert(faces==before&&primitives.size()==5);++injections;}
    std::cout<<"Corner normals/merge/n-gon mapping: "<<injections<<" status failures, mixed normals, scale/reflection/shear passed\n";
}
