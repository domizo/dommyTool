#include "bullet_mesh_input_doubles.h"
#include "bullet_mesh_input.inc"

#include <algorithm>
#include <functional>
#include <limits>

namespace {
int evaluations=0, failures=0, exceptions=0;
enum Kind { collision, softBody };
void reset() {
    fixture=MeshFixture(); calls.clear(); failAt=throwAt=-1;
    injected=MS::kInvalidParameter; pointReads=triangleReads=normalReads=0;
}
struct Result {
    MeshCollisionData mesh;
    std::vector<int> indices={97};
    std::vector<float> coordinates={98};
    Result() { mesh.vertices={vec3f(7,8,9)}; mesh.normals={vec3f(1,2,3)}; mesh.indices={99}; }
};
bool evaluate(Kind kind,Result& result,MS::Code* returned=nullptr) {
    ++evaluations;
    if (kind==collision) return readMeshCollisionData(MObject(),result.mesh);
    MFnMesh mesh;
    const MStatus status=DBSoftBodyNode::createHelperMesh(
        mesh,result.indices,result.coordinates,MSpace::kTransform);
    if (returned) *returned=status.code;
    return bool(status);
}
void unchanged(Kind kind,const Result& result) {
    if (kind==collision) {
        assert(result.mesh.vertices.size()==1 && result.mesh.vertices[0][0]==7);
        assert(result.mesh.normals.size()==1 && result.mesh.normals[0][0]==1);
        assert(result.mesh.indices==std::vector<unsigned>({99}));
    } else assert(result.coordinates.empty() && result.indices.empty());
}
void reject(Kind kind,bool beforePoints=false) {
    Result result; assert(!evaluate(kind,result)); unchanged(kind,result);
    if (beforePoints) assert(pointReads==0 && triangleReads==0 && normalReads==0);
}
void goldens() {
    for (Kind kind:{collision,softBody}) {
        reset(); Result result; assert(evaluate(kind,result));
        assert(pointReads==1 && triangleReads==1);
        assert(fixture.pointSpace==(kind==collision ? MSpace::kWorld : MSpace::kTransform));
        assert(std::find(calls.begin(),calls.end(),"iterator.triangles")<
               std::find(calls.begin(),calls.end(),"mesh.points"));
        if (kind==collision) {
            assert(result.mesh.indices==std::vector<unsigned>({0,1,2,0,2,3}));
            assert(result.mesh.vertices.size()==4 && result.mesh.normals.size()==4);
            for (unsigned i=0;i<4;++i) {
                assert(result.mesh.vertices[i][0]==fixture.points[i].x);
                assert(result.mesh.vertices[i][1]==fixture.points[i].y);
                assert(result.mesh.vertices[i][2]==fixture.points[i].z);
                assert(result.mesh.normals[i][0]==fixture.normals[i].x);
                assert(result.mesh.normals[i][1]==fixture.normals[i].y);
                assert(result.mesh.normals[i][2]==fixture.normals[i].z);
            }
        } else {
            assert(result.indices==fixture.indices);
            assert(result.coordinates==std::vector<float>({0,0,0,1,0,0,1,1,0,0,1,0}));
        }
        // Actual host triangle counts are used, not an n-2 estimate.
        reset(); fixture.faces=1; fixture.faceTriangles={2}; fixture.counts={2};
        Result ngon; assert(evaluate(kind,ngon));
    }
}
void boundsAndMalformed() {
    for (Kind kind:{collision,softBody}) {
        for (int count:{-1,0,1,2,10000001,std::numeric_limits<int>::max()}) {
            reset(); fixture.vertices=count; reject(kind,true);
        }
        reset(); fixture.vertices=10000000; reject(kind);
        assert(pointReads==(kind==collision?1:0) && triangleReads==0);
        for (int faces:{-1,2000001,std::numeric_limits<int>::max()}) {
            reset(); fixture.faces=faces; reject(kind,true);
        }
        reset(); fixture.faces=0; fixture.faceTriangles.clear(); reject(kind,true);
        for (std::vector<int> triangles: {std::vector<int>{-1,1},{2000000,1},{0,0}}) {
            reset(); fixture.faceTriangles=triangles; reject(kind,true);
        }
        reset(); fixture.faceTriangles={1000000,1000000};
        reject(kind); assert(pointReads==(kind==collision?1:0) && triangleReads==(kind==collision?1:0));
        reset(); fixture.stalled=true; reject(kind,true);
        reset(); fixture.faceTriangles={1}; reject(kind,true);
        reset(); fixture.faceTriangles={1,1,1}; reject(kind,true);
        for (int length:{0,3,5}) {
            reset(); fixture.pointReadLength=length; reject(kind); assert(triangleReads==0);
        }
        for (int length:{0,1,3}) {
            reset(); fixture.countReadLength=length; reject(kind);
        }
        for (int length:{0,3,5,7,9}) {
            reset(); fixture.indexReadLength=length; reject(kind);
        }
        for (std::vector<int> counts:{std::vector<int>{-1,3},{0,1},{2,1}}) {
            reset(); fixture.counts=counts; reject(kind);
        }
        for (int invalid:{-1,4,std::numeric_limits<int>::max()}) {
            reset(); fixture.indices[2]=invalid; reject(kind);
        }
        reset(); fixture.indices[2]=fixture.indices[1]; reject(kind);
        for (float invalid:{std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity()}) {
            for (unsigned axis=0;axis<3;++axis) {
                reset(); auto& point=fixture.points[2];
                (axis==0 ? point.x : axis==1 ? point.y : point.z)=invalid;
                reject(kind);
            }
        }
    }
    reset(); Result result; MObject notMesh; notMesh.mesh=false;
    assert(!readMeshCollisionData(notMesh,result.mesh) && calls.empty()); unchanged(collision,result);
}
void normals() {
    for (unsigned length:{0U,3U,5U}) {
        reset(); fixture.normals.resize(length); Result result; assert(evaluate(collision,result));
        for (const auto& normal:result.mesh.normals)
            assert(normal[0]==0 && normal[1]==0 && normal[2]==0);
    }
    for (unsigned axis=0;axis<3;++axis) {
        reset(); auto& normal=fixture.normals[2];
        (axis==0 ? normal.x : axis==1 ? normal.y : normal.z)=std::numeric_limits<float>::quiet_NaN();
        Result result; assert(evaluate(collision,result));
        assert(result.mesh.normals[2][0]==0 && result.mesh.normals[2][1]==0 && result.mesh.normals[2][2]==0);
        assert(result.mesh.normals[0][1]==1); // Only the invalid vertex falls back.
    }
}
void inject(Kind kind) {
    reset(); Result baselineResult; assert(evaluate(kind,baselineResult)); const auto baseline=calls;
    for (unsigned i=0;i<baseline.size();++i) {
        for (MS::Code code:{MS::kFailure,MS::kInvalidParameter}) {
            reset(); failAt=int(i); injected=code; Result result; MS::Code returned=MS::kSuccess;
            const bool success=evaluate(kind,result,&returned);
            if (baseline[i]=="mesh.normals") {
                assert(success);
                for (const auto& n:result.mesh.normals) assert(n[0]==0 && n[1]==0 && n[2]==0);
            } else {
                assert(!success); unchanged(kind,result);
                assert(calls.size()==i+1);
                if (kind==softBody) assert(returned==code);
            }
            ++failures;
            reset(); Result retry; assert(evaluate(kind,retry) && calls==baseline);
        }
        reset(); throwAt=int(i); Result result; bool caught=false;
        try { evaluate(kind,result); } catch (const std::runtime_error&) { caught=true; }
        assert(caught); unchanged(kind,result); ++exceptions;
        // These readers deliberately propagate exceptions to their existing
        // compute-entry guards; this suite does not model native DG rollback.
    }
}
}
int main() {
    goldens(); boundsAndMalformed(); normals(); inject(collision); inject(softBody);
    std::cout<<"Bullet mesh intake: "<<evaluations<<" evaluations, "<<failures
             <<" API failure/retry cases, "<<exceptions<<" exception cases\n";
}
