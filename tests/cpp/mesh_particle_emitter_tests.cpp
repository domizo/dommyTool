#include "mesh_particle_emitter.inc"

#include <functional>

namespace {
enum Node { polygon, radial, volume };
int evaluations=0, failures=0, exceptions=0;
const double nan=std::numeric_limits<double>::quiet_NaN();
void reset(Node node) {
    fixture=ParticleFixture(); resetCalls(); shortAt=-1;
    badAccesses=oversizedAllocations=shortApplied=0; injectedStatus=MS::kFailure;
    fixture.numbers={{"mIsFull",0},{"mCurrentTime",2},{"mStartTime",1},{"mDeltaTime",1},{"mRate",1},
        {"isVertexEmit",1},{"isEdgeEmit",0},{"isFaceEmit",0},{"edgeSampling",1},{"faceSampling",1},
        {"isOutKill",0},{"thickness",0},{"noise",0},{"nSeed",1},
        {"mSpeed",2},{"emitType",D_RADIAL_EMIT_DIRECTION},{"emitSpeedMultiply",1},{"emitSpeedRandom",0},
        {"emitPositionBias",1},{"isNurbsSampling",0},{"sampling",2},{"nFrequency",1}};
    fixture.triples={{"emitSourcePosition",{{0,0,0}}},{"emitDirection",{{0,1,0}}},
        {"directionNoise",{{0,0,0}}},{"noiseScale",{{1,1,1}}},{"noisePositionOffset",{{0,0,0}}}};
    if (node!=volume)
        fixture.meshPoints={MPoint(0,0,0),MPoint(6,0,0),MPoint(8,8,0),MPoint(0,6,0)};
}
MStatus evaluate(Node node) {
    ++evaluations; MDataBlock data; MPlug plug(mOutput,42); MStatus status;
    if (node==polygon) { DPolygonEmitter n; status=n.compute(plug,data); }
    else if (node==radial) { DRadialGeometryEmitter n; status=n.compute(plug,data); }
    else { DMeshVolumeEmitter n; status=n.compute(plug,data); }
    assert(badAccesses==0 && oversizedAllocations==0); return status;
}
std::vector<MVector> positions() {
    const auto found=fixture.published.vectors.find("position");
    return found==fixture.published.vectors.end() ? std::vector<MVector>() : *found->second.values;
}
void success(Node node,unsigned count) {
    assert(evaluate(node)==MS::kSuccess && fixture.publications==1 && fixture.cleans==1);
    assert(fixture.outputIndex==42 && positions().size()==count);
    if (count) assert(fixture.published.vectors.at("velocity").length()==count);
}
void reject(Node node) {
    assert(evaluate(node)==MS::kFailure && fixture.publications==0 && fixture.cleans==0);
}
void goldens() {
    for (Node node:{polygon,radial}) {
        reset(node); fixture.numbers["isEdgeEmit"]=1; fixture.numbers["isFaceEmit"]=1;
        success(node,6);
        assert(positions()==std::vector<MVector>({MVector(0,0,0),MVector(6,0,0),MVector(8,8,0),
            MVector(0,6,0),MVector(3,0,0),MVector(4,4,0)}));
        for (const auto& v:*fixture.published.vectors.at("velocity").values)
            assert(v==(node==polygon ? MVector::zero : MVector(0,2,0)));
        reset(node); fixture.numbers["isVertexEmit"]=0; fixture.numbers["isFaceEmit"]=1;
        fixture.numbers["faceSampling"]=2; success(node,4);
        // Vertex 2 is deliberately off the parallelogram: do not replace the
        // historical two-edge grid with a bilinear quad interpolation.
        assert(positions()==std::vector<MVector>({MVector(2,2,0),MVector(2,4,0),MVector(4,2,0),MVector(4,4,0)}));
        reset(node); fixture.numbers["isFaceEmit"]=1; fixture.polygonVertices={0,1,2};
        success(node,4); // Non-quads remain skipped.
    }
    reset(volume); success(volume,8);
    assert(positions().front()==MVector(-1,-1,-1) && positions().back()==MVector(1,1,1));
    reset(radial); fixture.numbers["emitType"]=D_RADIAL_EMIT_SOURCE;
    fixture.numbers["emitPositionBias"]=0.5; fixture.triples["emitSourcePosition"]={{1,0,0}};
    fixture.matrix[3][0]=10; success(radial,4);
    assert(positions()[0]==MVector(9.5,0,0));
    assert(fixture.published.vectors.at("velocity")[0]==MVector(-2,0,0));
    reset(radial); fixture.geometryType=MFnData::kNurbsCurve; success(radial,4);
    reset(radial); fixture.geometryType=MFnData::kNurbsCurve;
    fixture.numbers["isNurbsSampling"]=1; success(radial,2);
    assert(positions()==std::vector<MVector>({MVector(0,0,0),MVector(3,0,0)}));
    reset(radial); fixture.geometryType=MFnData::kNurbsSurface; success(radial,4);
    reset(radial); fixture.geometryType=MFnData::kNurbsSurface;
    fixture.numbers["isNurbsSampling"]=1; success(radial,9);
    assert(positions().back()==MVector(1,1,0));
}
void preflight() {
    for (Node node:{polygon,radial,volume}) {
        for (int count:{-1,10000001,std::numeric_limits<int>::max()}) {
            reset(node); fixture.overrideMeshVertexCount=true; fixture.meshVertexCount=count;
            reject(node); assert(fixture.meshReads==0);
        }
        reset(node); fixture.overrideMeshVertexCount=true; fixture.meshVertexCount=0;
        success(node,0); assert(fixture.meshReads==0);
        reset(node); fixture.overrideMeshVertexCount=true; fixture.meshVertexCount=10000000;
        reject(node); assert(fixture.meshReads==1); // Inclusive limit; mismatch is rejected after read.
        for (int delta:{-1,1}) {
            reset(node); fixture.meshReadDelta=delta; reject(node); assert(fixture.meshReads==1);
        }
        reset(node); fixture.numbers["isFaceEmit"]=1;
        fixture.polygons=static_cast<int>(dommy::generation::kMaxGeneratedFaceCount)+1;
        reject(node); assert(fixture.meshReads==0);
        reset(node); assert(evaluate(node)==MS::kSuccess);
        const auto countCall=std::find(operations.begin(),operations.end(),"mesh.vertices");
        const auto readCall=std::find(operations.begin(),operations.end(),"mesh.points");
        assert(countCall<readCall);
        for (const char* key:{"mIsFull","mCurrentTime","mDeltaTime"}) {
            reset(node); fixture.numbers[key]=std::string(key)=="mIsFull" ? 1 : 0; success(node,0);
            assert(fixture.meshReads==0);
        }
        reset(node); fixture.meshValid=false; success(node,0);
    }
    // Inactive mRate semantics differ historically: polygon ignores it;
    // radial accepts negative rates, while volume disables nonpositive rates.
    reset(polygon); fixture.numbers["mRate"]=0; success(polygon,4);
    reset(radial); fixture.numbers["mRate"]=-1; success(radial,4);
    reset(volume); fixture.numbers["mRate"]=-1; success(volume,0);
}
void numericSamples() {
    for (Node node:{polygon,radial}) {
        for (unsigned axis=0; axis<3; ++axis) {
            double a[3]={0,0,0}, b[3]={0,0,0}; a[axis]=-1e308; b[axis]=1e308;
            reset(node); fixture.numbers["isVertexEmit"]=0; fixture.numbers["isEdgeEmit"]=1;
            fixture.meshPoints={MPoint(a[0],a[1],a[2]),MPoint(b[0],b[1],b[2])};
            success(node,1); assert(positions()[0]==MVector::zero);
            reset(node); fixture.numbers["isVertexEmit"]=0; fixture.numbers["isFaceEmit"]=1;
            fixture.meshPoints={MPoint(a[0],a[1],a[2]),MPoint(b[0],b[1],b[2]),
                MPoint(b[0],b[1],b[2]),MPoint(a[0],a[1],a[2])};
            success(node,1); assert(positions()[0]==MVector::zero);
            reset(node); fixture.numbers["isVertexEmit"]=0; fixture.numbers["isFaceEmit"]=1;
            fixture.numbers["faceSampling"]=2;
            fixture.meshPoints={MPoint(a[0],a[1],a[2]),MPoint(b[0],b[1],b[2]),
                MPoint(b[0],b[1],b[2]),MPoint(a[0],a[1],a[2])};
            success(node,4);
            const auto samples=positions();
            for (unsigned i=0;i<samples.size();++i) {
                const double expected=i<2 ? -1e308/3 : 1e308/3;
                assert(std::isfinite(samples[i][axis]) && std::fabs(samples[i][axis]/expected-1)<1e-14);
            }
            reset(node); a[axis]=nan; fixture.meshPoints[0]=MPoint(a[0],a[1],a[2]); reject(node);
        }
    }
    const double max=std::numeric_limits<double>::max();
    double value=17;
    assert(dommy::particle::quadSampleCoordinate(-max,max,-max,0.5,0.75,value) && value==0);
    assert(dommy::particle::quadSampleCoordinate(max,max,max,0.75,0.75,value) && value==max);
    value=17;
    assert(!dommy::particle::quadSampleCoordinate(-max,max,max,1,1,value) && value==17);
    assert(!dommy::particle::quadSampleCoordinate(0,1,2,nan,0.5,value) && value==17);
    MPoint output(9,8,7);
    assert(!dommy::particle::blendMeshSample(MPoint(),MPoint(0,nan,0),0.5,output));
    assert(output.x==9 && output.y==8 && output.z==7);
    assert(!dommy::particle::quadMeshSample(MPoint(),MPoint(0,nan,0),MPoint(),0.5,0.5,output));
    assert(output.x==9 && output.y==8 && output.z==7);
}
void inject(Node node,const std::function<void()>& configure) {
    reset(node); configure(); assert(evaluate(node)==MS::kSuccess);
    const auto baseline=operations; const auto expected=positions();
    for (std::size_t i=0;i<baseline.size();++i) {
        for (MS::MStatusCode code:{MS::kFailure,MS::kInvalidParameter}) {
            reset(node); configure(); failAt=static_cast<int>(i); injectedStatus=code;
            assert(evaluate(node)==code && fixture.cleans==0);
            assert(fixture.publications==(baseline[i]=="output.clean" ? 1 : 0));
            assert(callCount==static_cast<int>(i+1));
            if (baseline[i]=="mesh.vertices" || baseline[i]=="mesh.polygons" || baseline[i]=="mesh.edges")
                assert(fixture.meshReads==0);
            reset(node); configure(); assert(evaluate(node)==MS::kSuccess && operations==baseline);
            assert(positions()==expected); ++failures;
        }
        reset(node); configure(); attributeThrowAt=static_cast<int>(i);
        assert(evaluate(node)==MS::kFailure && fixture.cleans==0);
        assert(fixture.publications==(baseline[i]=="output.clean" ? 1 : 0)); ++exceptions;
    }
}
} // namespace

int main() {
    goldens(); preflight(); numericSamples();
    for (Node node:{polygon,radial,volume}) inject(node,[] {});
    for (Node node:{polygon,radial}) {
        inject(node,[] {fixture.numbers["isEdgeEmit"]=1;fixture.numbers["isFaceEmit"]=1;});
        inject(node,[] {fixture.numbers["isVertexEmit"]=0;fixture.numbers["isFaceEmit"]=1;fixture.numbers["faceSampling"]=2;});
    }
    inject(radial,[] {fixture.numbers["isVertexEmit"]=0;});
    inject(radial,[] {fixture.numbers["emitSpeedRandom"]=0.5;fixture.triples["directionNoise"]={{0.1,-0.2,0.3}};});
    for (auto type:{MFnData::kNurbsCurve,MFnData::kNurbsSurface})
        for (int sampling:{0,1}) inject(radial,[=] {fixture.geometryType=type;fixture.numbers["isNurbsSampling"]=sampling;});
    std::cout << "Mesh particle emitters: " << evaluations << " evaluations, " << failures
              << " API failure/retry cases, " << exceptions << " exception cases\n";
}
