// Exact production preflight and polygon normal helpers, with scalar-only
// host reports. This does not allocate large Maya arrays or run Maya geometry.
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>

struct MPoint {
    double x, y, z, w;
    MPoint(double a=0, double b=0, double c=0) : x(a), y(b), z(c), w(1) {}
};
struct MVector {
    double x, y, z;
    MVector(double a=0, double b=0, double c=0) : x(a), y(b), z(c) {}
    static const MVector zero;
};
const MVector MVector::zero;
struct MPointArray : std::vector<MPoint> {
    using std::vector<MPoint>::vector;
    unsigned int length() const {return static_cast<unsigned int>(size());}
};
struct MMatrix {static const MMatrix identity;};
const MMatrix MMatrix::identity;
struct MObject {std::size_t index=0;};
struct MIntArray {unsigned int count=0; unsigned int length() const {return count;}};
static std::vector<std::array<int,4>> reports;
static int calls=0, failAt=-1, throwAt=-1, bulkReads=0;
static MStatus operation() {
    const int call=calls++;
    if(call==throwAt)throw std::runtime_error("injected scalar query failure");
    return call==failAt ? MS::kFailure : MS::kSuccess;
}
struct MFnMesh {
    std::size_t index;
    MFnMesh(const MObject& object, MStatus* status) : index(object.index) {*status=operation();}
    int read(int field, MStatus* status) {*status=operation();return reports.at(index)[field];}
    int numVertices(MStatus* status) {return read(0,status);}
    int numPolygons(MStatus* status) {return read(1,status);}
    int numFaceVertices(MStatus* status) {return read(2,status);}
    int numUVs(MStatus* status) {return read(3,status);}
    MStatus getVertices(MIntArray& faces, MIntArray& ids) {
        ++bulkReads;faces.count=reports.at(index)[1];ids.count=reports.at(index)[2];
        return operation();
    }
};
#include "mesh_convert_geometry.inc"

static void reset() {calls=0;failAt=-1;throwAt=-1;bulkReads=0;}
static std::vector<MeshInput> inputs(std::size_t count=1) {
    std::vector<MeshInput> result(count);
    for(std::size_t i=0;i<count;++i)result[i].meshObject.index=i;
    return result;
}
static void testPreflight() {
    for(auto mode:{kConvertFull,kConvertFullWithTransform,kConvertPerPolygon}) {
        reports={{{4,3,12,4}}};auto source=inputs();std::size_t count=77;
        reset();assert(validateInputBudget(source,mode,count));
        assert(count==(mode==kConvertPerPolygon?3U:1U));
        assert(source[0].importPlan.points==4 && source[0].importPlan.connections==12);
        assert(source[0].importPlan.uvs==4 && bulkReads==0);
        const int boundaryCount=calls;
        for(int at=0;at<boundaryCount;++at)for(bool throws:{false,true}) {
            reset();count=77;if(throws)throwAt=at;else failAt=at;
            bool caught=false;
            try {assert(!validateInputBudget(source,mode,count));}
            catch(const std::runtime_error&) {caught=true;}
            assert(caught==throws && count==77 && bulkReads==0);
        }
        for(int field=0;field<4;++field)for(int invalid:{-1,std::numeric_limits<int>::max()}) {
            reports={{{4,1,4,4}}};reports[0][field]=invalid;reset();count=77;
            assert(!validateInputBudget(source,mode,count) && count==77 && bulkReads==0);
        }
        for(const auto& report:std::vector<std::array<int,4>>{
                {{101,1000000,101000000,0}}, // Rejected before topology retrieval.
                {{1000000,1000000,4000000,0}}, // Count-safe but >256 MiB estimate.
                {{4,1,4,20000000}}, // Unassigned UV table still consumes memory.
                {{0,1,3,0}}, {{3,0,3,0}}, {{3,1,0,0}}}) {
            reports={report};reset();count=77;
            assert(!validateInputBudget(source,mode,count) && count==77 && bulkReads==0);
        }
    }
    // Both source meshes fit alone but their retained output exceeds the cap.
    reports={{{500000,500000,2000000,0}},{{500000,500000,2000000,0}}};
    auto source=inputs();std::size_t count=77;reset();
    assert(validateInputBudget(source,kConvertFull,count));
    source=inputs(2);count=77;reset();
    assert(!validateInputBudget(source,kConvertFull,count) && count==77 && bulkReads==0);
    // Per-face output duplicates positions and UVs even when source indices share.
    for(const auto& report:std::vector<std::array<int,4>>{
            {{4,500000,2000000,0}}, {{4,400000,1200000,3}}}) {
        reports={report};source=inputs();reset();
        assert(validateInputBudget(source,kConvertFull,count));count=77;
        assert(!validateInputBudget(source,kConvertPerPolygon,count) && count==77);
    }
    // Exactly the existing byte threshold, and one point beyond it.
    reports={{{2097147,4,12,0}}};MStatus status;reset();MFnMesh mesh(MObject(),&status);
    dommy::mesh::MeshMergePlan plan;
    assert(dommy::maya::preflightMeshImport(mesh,plan));const auto before=plan;
    ++reports[0][0];reset();
    assert(!dommy::maya::preflightMeshImport(mesh,plan));
    assert(plan.points==before.points && plan.faces==before.faces);
    std::cout<<"Mesh import counts: scalar-only admission, status/exception propagation, aggregate/UV/per-face storage budgets passed\n";
}

static void expectNormal(const MPointArray& points, double x, double y, double z) {
    MVector result(7,8,9);assert(polygonNormal(points,result));
    assert(std::fabs(result.x-x)<1e-11 && std::fabs(result.y-y)<1e-11 && std::fabs(result.z-z)<1e-11);
}
static void testNormals() {
    int examples=0;
    for(double scale:{1e-30,1e-9,1.0,1e20,1e30})for(bool translated:{false,true})
        for(bool tilted:{false,true})for(bool reversed:{false,true}) {
            MPointArray points{{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
            for(MPoint& p:points) {
                p.z=tilted?p.x*.25+p.y*.5:0;
                p.x=scale*(p.x+(translated?128:0));
                p.y=scale*(p.y+(translated?-64:0));
                p.z=scale*(p.z+(translated?32:0));
            }
            if(reversed)std::reverse(points.begin(),points.end());
            const double sign=reversed?-1:1;
            const double length=tilted?std::sqrt(.25*.25+.5*.5+1):1;
            for(unsigned int start=0;start<4;++start) {
                expectNormal(points,tilted?sign*(-.25)/length:0,
                             tilted?sign*(-.5)/length:0,sign/length);
                std::rotate(points.begin(),points.begin()+1,points.end());++examples;
            }
            points.push_back(points.front());
            expectNormal(points,tilted?sign*(-.25)/length:0,tilted?sign*(-.5)/length:0,sign/length);
        }
    expectNormal({{0,0,0},{1e-30,0,0},{1e-30,1e30,0},{0,1e30,0}},0,0,1);
    expectNormal({{0,0,0},{0,0,0},{0,0,0}},0,0,0);
    expectNormal({{0,0,0},{1e20,1e20,1e20},{2e20,2e20,2e20}},0,0,0);
    expectNormal({{0,0,0},{1,1,0},{0,1,0},{1,0,0}},0,0,0);
    expectNormal({{0,0,0},{2,0,0},{1,1,0},{2,2,0},{0,2,0}},0,0,1); // Concave.
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::infinity(),1e40}) {
        MPointArray points{{0,0,0},{1,0,0},{0,1,0}};points[1].x=bad;
        MVector result(7,8,9);assert(!polygonNormal(points,result));
        assert(result.x==7 && result.y==8 && result.z==9);
    }
    MVector result(7,8,9);assert(!polygonNormal({},result) && result.x==7);
    std::cout<<"Polygon normals: "<<examples<<" scale/translation/tilt/winding/cyclic cases plus skinny, concave, degenerate and invalid inputs passed\n";
}
int main() {testPreflight();testNormals();}
