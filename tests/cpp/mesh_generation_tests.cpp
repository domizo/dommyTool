#include "mesh_emitter_doubles.h"
#include "common/utility/DWireTopologyPlan.h"
#include <fstream>
#include <functional>

namespace MFnNurbsCurveData { constexpr auto kMesh = MFnData::kMesh; }
enum {ROTATE_ORDER_XYZ,ROTATE_ORDER_YZX,ROTATE_ORDER_ZXY,ROTATE_ORDER_XZY,ROTATE_ORDER_YXZ,ROTATE_ORDER_ZYX};
struct ProfileVector : MVector {
    using MVector::MVector;
    ProfileVector(const MVector& v) : MVector(v) {}
    enum Axis { kYaxis };
    ProfileVector rotateBy(Axis, double angle) const {
        return {x*std::cos(angle)+z*std::sin(angle),y,-x*std::sin(angle)+z*std::cos(angle)};
    }
};
struct ProfileCurves {
    std::size_t count;
    explicit ProfileCurves(std::size_t n):count(n) {}
    std::size_t numberOfCurves() const {return count;}
};
static MPointArray profileResult;
static dommy::mesh::WireTopologyPlan topologyResult;
// Record exact transform arguments/status handling, not Maya matrix algebra.
struct TransformBoundary {
    MStatus setRotatePivot(const MVector& v,MSpace::Space s,bool balance) {
        assert(v.x==2 && v.y==3 && v.z==4 && s==MSpace::kObject && !balance);
        return api("transform.rotatePivot");
    }
    MStatus setScalePivot(const MVector& v,MSpace::Space s,bool balance) {
        assert(v.x==2 && v.y==3 && v.z==4 && s==MSpace::kObject && !balance);
        return api("transform.scalePivot");
    }
    MStatus addScale(const double* v,MSpace::Space s) {
        assert(v[0]==8 && v[1]==9 && v[2]==10 && s==MSpace::kObject);
        return api("transform.scale");
    }
    MStatus addRotation(const double* v,int order,MSpace::Space s) {
        assert(v[0]==.1 && v[1]==.2 && v[2]==.3 && order==2 && s==MSpace::kObject);
        return api("transform.rotate");
    }
    MStatus addTranslation(const MVector& v,MSpace::Space s) {
        assert(v.x==5 && v.y==6 && v.z==7 && s==MSpace::kObject);
        return api("transform.translate");
    }
};
#define MVector ProfileVector
#define MFnNurbsCurve EmitterCurve
#include "mesh_generation.inc"
#undef MVector
#undef MFnNurbsCurve

static void fixture() {
    reset(); inputs.clear(); geometries.clear();
    geometries["profile"].points={{0,0,0},{1,0,0},{0,0,1}};
    inputs["ProfileCurve"].object={"profile"};
    inputs["ProfileCurve"].type=MFnData::kNurbsCurve;
    inputs["ProfileCurve"].connected=true;
    inputs["profileCurve"]=inputs["ProfileCurve"];
    profileResult={{99,99,99}};
}
static MStatus guarded(const std::function<MStatus()>& fn) {
    try {return fn();} catch (...) {return MS::kFailure;}
}
static MStatus profile(int node,int mode=1,std::size_t rings=2,std::size_t circle=3,std::size_t meshes=1) {
    return guarded([&] {
        return node==0 ? profileDMWireGen(rings,circle,mode,1,meshes) :
                         profileDPolyWire(rings,circle,mode,1,meshes);
    });
}
static std::size_t calls(const std::string& name) {
    return std::count(operations.begin(),operations.end(),name);
}
static int boundaries() {
    int tested=0;
    fixture(); assert(transformCalls()==MS::kSuccess && callCount==5);
    for (int i=0;i<5;++i) {
        for (auto status : {MS::kFailure,MS::kInvalidParameter}) {
            fixture(); failAt=i; injectedStatus=status;
            assert(transformCalls()==status && callCount==i+1); ++tested;
        }
        fixture(); throwAt=i; assert(!guarded(transformCalls)); ++tested;
    }
    for (auto reader : controlReaders) {
        fixture(); MDataBlock data;
        assert(reader(data)==MS::kSuccess && callCount==1);
        for (auto status : {MS::kFailure,MS::kInvalidParameter}) {
            fixture(); failAt=0; injectedStatus=status;
            assert(reader(data)==status && callCount==1); ++tested;
        }
        fixture(); throwAt=0; assert(!guarded([&]{return reader(data);})); ++tested;
    }
    for (int node : {0,1}) {
        for (int mode : {0,1}) {
            fixture(); assert(profile(node,mode)==MS::kSuccess);
            assert(profileResult.length()==3 && dommy::mesh::hasValidWireTopology(topologyResult));
            if (mode==1) assert(profileResult[1].x==1 && profileResult[2].z==1);
            else assert(profileResult[0].z==-1 && std::abs(profileResult[1].x+std::sqrt(3.0)/2)<1e-12);
            const auto trace=operations;
            for (unsigned i=0;i<trace.size();++i) {
                for (auto status : {MS::kFailure,MS::kInvalidParameter}) {
                    fixture(); failAt=i; injectedStatus=status;
                    assert(profile(node,mode)==status && callCount==int(i)+1);
                    assert(profileResult.size()==1 && profileResult[0].x==99); ++tested;
                }
                fixture(); throwAt=i;
                assert(!profile(node,mode) && profileResult[0].x==99); ++tested;
                if (trace[i]=="curve.cvs") {
                    fixture(); shortAt=i;
                    assert(!profile(node,mode) && shortApplied && profileResult[0].x==99); ++tested;
                }
            }
        }
        fixture(); assert(!profile(node,0,2,10000000));
        assert(calls("array.append")==0 && calls("curve.cvs")==0);
        fixture(); geometries["profile"].reportedCvCount=10000000;
        assert(!profile(node) && calls("curve.cvs")==0);
        fixture(); geometries["profile"].points[1].x=NAN;
        assert(!profile(node) && profileResult[0].x==99);
        fixture(); geometries["profile"].reportedCvCount=2;
        assert(!profile(node) && calls("curve.cvs")==0);
    }
    fixture(); assert(!profile(0,0,1000,1000)); assert(calls("array.append")==0);
    fixture(); assert(!profile(0,0,100,100,100)); assert(calls("array.append")==0);
    fixture(); inputs["ProfileCurve"].connected=false;
    assert(profile(0)==MS::kSuccess && calls("curve.cvs")==0);
    fixture(); inputs["profileCurve"].connected=false;
    assert(!profile(1) && calls("curve.cvs")==0);
    return tested;
}
int main(int argc,char** argv) {
    const auto first=schemaTrace("DMCopyTransform",DMCopyTransform::initialize); const int a=callCount;
    const auto second=schemaTrace("DMWireGen",DMWireGen::initialize); const int b=callCount;
    const auto third=schemaTrace("DPolyWire",DPolyWire::initialize); const int c=callCount;
    const auto schema=first+second+third;
    if (argc==2 && std::string(argv[1])=="--print-schema") {std::cout<<schema;return 0;}
    assert(argc==2); std::ifstream file(argv[1]); assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)),{}); assert(schema==expected);
    int tested=testFailures("DMCopyTransform",DMCopyTransform::initialize,a);
    tested+=testFailures("DMWireGen",DMWireGen::initialize,b);
    tested+=testFailures("DPolyWire",DPolyWire::initialize,c);
    tested+=boundaries();
    std::cout<<"Mesh initializer/input/profile contracts and "<<tested<<" injected failures passed\n";
}
