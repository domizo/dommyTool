#include "voronoi_safety_doubles.h"
#include "voronoi_safety.inc"
#include <functional>

namespace {
void reset() {
    calls.clear(); failAt=throwAt=shortAt=-1;
    published=cleaned=releases=bulkReads=0;
    shortApplied=stuck=earlyDone=nullCreate=badInputType=false;
    connected=useCrop=checkCell=true; toleranceValue=0.001; edgeSpan=1.0;
    rayHits.clear(); rayIndex=0; rayLengths.clear(); rayTolerances.clear();
    geometries.clear(); geometries[1]=Geometry(); geometries[2]=Geometry();
    curves.curves.clear(); meshes.meshes.clear();
    cellCount=1U;
}
MStatus caught(const std::function<MStatus()>& operation) {
    try { return operation(); } catch(const std::exception&) { return MS::kFailure; }
}
MStatus crop() {
    std::vector<MObject> cells{MObject(2)};
    std::vector<bool> mask{false,true};
    const auto result=caught([&]{return DPolyIsIntersectMeshCheck(MObject(1),cells,mask,checkCell,toleranceValue);});
    if (!result) assert(mask==std::vector<bool>({false,true}));
    else assert(mask.size()==1U);
    return result;
}
MStatus build() {
    MObject result(99); MBoundingBox bounds; bounds.expand(MPoint(42,42,42));
    const auto status=caught([&]{return buildCropMesh(MObject(1),result,bounds);});
    if (!status) assert(result.id==99 && bounds.min().x==42);
    else assert(result.id==3 && bounds.min().x==0 && bounds.max().x==1);
    return status;
}
MStatus prepare() {
    DPVoronoi node; MDataBlock data; bool hasCrop=false; MBoundingBox bounds; MObject mesh;
    return prepareBoundsAndCrop(&node,data,{{0,0,0},{1,1,1}},0,hasCrop,bounds,mesh);
}
MStatus curve() {
    MDataBlock data; MPlug plug;
    return publishCurveOutput(data,plug,geometries[1].points,{4},{0,1,2,3});
}
MStatus mesh() {
    MDataBlock data; MPlug plug; voro::container container;
    return publishMeshOutput(data,plug,container,0.0,connected,MObject(1));
}
std::size_t failureCases=0, shortCases=0;
void failures(const std::string& label,const std::function<MStatus()>& operation,
              const std::function<void()>& setup=[] {}) {
    reset(); setup(); assert(caught(operation));
    const auto baseline=calls;
    const int expectedPublished=published;
    for (int mode=0; mode<2; ++mode) {
        for (std::size_t i=0; i<baseline.size(); ++i) {
            reset(); setup(); (mode==0 ? failAt : throwAt)=static_cast<int>(i);
            if (caught(operation)) { std::cerr<<label<<" missed "<<baseline[i]<<" at "<<i<<"\n"; assert(false); }
            const auto publish=std::find(baseline.begin(),baseline.end(),"publish");
            if (publish==baseline.end() || i<=static_cast<std::size_t>(publish-baseline.begin())) {
                assert(published==0 && cleaned==0);
            }
            assert(releases<=1);
            reset(); setup(); assert(caught(operation)); assert(published==expectedPublished);
            ++failureCases;
        }
    }
}
void shortArrays(const std::function<MStatus()>& operation) {
    reset(); assert(caught(operation)); const auto baseline=calls;
    for(std::size_t i=0;i<baseline.size();++i) {
        if(baseline[i]!="array.length" && baseline[i]!="array.copy" &&
           baseline[i]!="mesh.points" && baseline[i]!="mesh.vertices") continue;
        reset(); shortAt=int(i); assert(!caught(operation)); assert(shortApplied);
        assert(published==0 && cleaned==0); ++shortCases;
        reset(); assert(caught(operation));
    }
}
}

int main() {
    failures("crop",crop);
    failures("edge",crop,[]{rayHits={0,0,0,0,1};});
    failures("collapse",crop,[]{geometries[2].edgeLength=0.0;});
    failures("zero-area",crop,[]{geometries[2].edgeLength=0.0001;geometries[2].zeroArea=true;});
    failures("Boolean-only",crop,[]{checkCell=false;});
    failures("build",build);
    failures("prepare",prepare);
    failures("curve",curve);
    failures("mesh",mesh);
    failures("mesh-clip",mesh,[]{checkCell=false;});
    shortArrays(build); shortArrays(curve); shortArrays(crop);

    reset(); assert(curve()); assert(published==1 && curves.curves.size()==1U);
    assert(curves.curves[0]->points.size()==4U);
    for(unsigned i=0;i<4U;++i)assert(curves.curves[0]->points[i].x==geometries[1].points[i].x);

    for(bool stop:{false,true}) {
        reset(); stuck=!stop; earlyDone=stop; assert(!crop()); assert(releases==1);
    }
    for(double invalid:{-1.0,std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max()}) {
        reset(); toleranceValue=invalid; assert(!crop()); assert(calls.empty());
    }
    reset(); toleranceValue=0; assert(crop()); assert(rayTolerances.front()==1.0e-6f);
    assert(rayLengths.front()==std::numeric_limits<float>::max());
    reset(); rayHits={0,0,0,0,1}; assert(crop()); assert(rayLengths.back()==1.0f);
    reset(); rayHits={0,0,0,0,1}; toleranceValue=0.0; edgeSpan=1.0e-13;
    assert(crop()); assert(rayLengths.back()>0.0f && rayLengths.back()<1.0e-12f);
    reset(); rayHits={-1}; assert(!crop());

    // A successful ray with zero hits must not be mistaken for an inside point.
    reset(); rayHits={1,0,0}; std::vector<MObject> cells{MObject(2)};
    std::vector<bool> mask{true}; assert(DPolyIsIntersectMeshCheck(MObject(1),cells,mask,true,0.001));
    assert(!mask[0]); assert(std::find(calls.begin(),calls.end(),"mesh.boolean")==calls.end());

    for(int mode=0;mode<7;++mode) {
        reset(); auto& g=geometries[1];
        if(mode==0)g.reportedPoints=10000001;
        if(mode==1)g.reportedFaces=2000001;
        if(mode==2)g.reportedConnections=100000001;
        if(mode==3)g.reportedPoints=2100000; // Count-valid but exceeds the byte estimate.
        if(mode==4)g.reportedUVs=-2;
        if(mode==5)g.reportedPoints=0;
        if(mode==6)g.reportedFaces=-2;
        assert(!caught(prepare)); assert(bulkReads==0);
    }
    // Exercise the admission edge using scalar counts, without giant allocation.
    reset(); geometries[1].reportedPoints=2097149;
    MStatus status; MFnMesh fn(MObject(1),&status); dommy::mesh::MeshMergePlan plan;
    assert(dommy::maya::preflightMeshImport(fn,plan));
    geometries[1].reportedPoints=2097150; assert(!dommy::maya::preflightMeshImport(fn,plan));
    assert(bulkReads==0);
    reset(); geometries[1].reportedPoints=5; assert(!build());
    reset(); geometries[1].reportedFaces=2; assert(!build());

    // Explicit cache release is idempotent; the destructor must not release twice.
    reset();
    {
        MFnMesh source(MObject(1),&status);
        dommy::maya::MeshAcceleratorGuard<MFnMesh> guard(source);
        assert(guard.release()); assert(guard.release());
    }
    assert(releases==1);
    for(float invalid:{0.0f,-1.0f,std::numeric_limits<float>::infinity()}) {
        reset(); MFnMesh source(MObject(1),&status); MMeshIsectAccelParams accel;
        MFloatPointArray hits{{42,0,0}};
        assert(!dommy::maya::checkedAllIntersections(source,MFloatPoint(),MFloatVector(0,1,0),
            MSpace::kObject,&accel,hits,invalid,0.001f));
        assert(!dommy::maya::checkedAllIntersections(source,MFloatPoint(),MFloatVector(0,1,0),
            MSpace::kObject,&accel,hits,1.0f,invalid));
        assert(hits.size()==1U && hits[0].x==42 && rayLengths.empty());
    }

    for(int mode=0;mode<6;++mode) {
        reset();auto& g=geometries[1];
        if(mode==0)g.points[0].x=std::numeric_limits<double>::infinity();
        if(mode==1)g.points[0].x=std::numeric_limits<double>::max();
        if(mode==2)g.connections[0]=-1;
        if(mode==3)g.connections[0]=4;
        if(mode==4)g.counts[0]=2;
        if(mode==5)g.counts[0]=3;
        assert(!build());
    }
    reset();badInputType=true;assert(!prepare());assert(bulkReads==0);
    reset();nullCreate=true;assert(!build());
    reset();connected=false;assert(prepare());assert(bulkReads==0);
    reset();assert(prepare());assert(bulkReads==2); // One point read, one topology read.
    reset();useCrop=false;assert(mesh());assert(releases==0);
    reset();assert(mesh());assert(meshes.meshes.size()==1U);
    for(std::size_t count:{2U,100U}) {
        reset();cellCount=count;useCrop=false;assert(mesh());
        assert(meshes.meshes.size()==count && std::count(calls.begin(),calls.end(),"mesh.insert")==1);
    }
    std::cout<<"Voronoi safety: "<<failureCases<<" status/exception cases with recovery, "
             <<shortCases<<" short-array cases, coordinate/admission/topology checks passed\n";
}
