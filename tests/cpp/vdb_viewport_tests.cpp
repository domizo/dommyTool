#include "vdb_viewport.inc"
#include <iostream>
#include <cfenv>
#if defined(__clang__)
#pragma STDC FENV_ACCESS ON
#endif

static DOpenVDBData input;
static std::shared_ptr<openvdb::FloatGrid> grid;
static void reset() {
    calls.clear();failAt=-1;failureKind=0;values.clear();initializeAttributes();
    tbb::reversePartitions=false;openvdb::valueQueries.clear();
    input.mGrids.clear();grid=std::make_shared<openvdb::FloatGrid>();input.mGrids.push_back(grid);
    inputObject=MObject();inputObject.data=&input;inputObject.type=DOpenVDBData::id;inputConnected=true;
    openvdb::mesherPoints={openvdb::Vec3s(0,0,0),openvdb::Vec3s(1,0,0),openvdb::Vec3s(0,1,0),openvdb::Vec3s(1,1,0)};
    openvdb::triangle[0]=0;openvdb::triangle[1]=1;openvdb::triangle[2]=2;openvdb::mesherQuad=false;
    for(const MObject& a:{DOpenVDBDisplayVP2::aVisualizeBBox,DOpenVDBDisplayVP2::aVisualizeInternalNodes,
        DOpenVDBDisplayVP2::aVisualizeLeafNodes,DOpenVDBDisplayVP2::aVisualizeActiveTiles,
        DOpenVDBDisplayVP2::aVisualizeSurface,DOpenVDBDisplayVP2::aVisualizeActiveVoxels,
        DOpenVDBDisplayVP2::aDisplayVector,DOpenVDBDisplayVP2::aIsClip})values[a.key].boolean=true;
    values[DOpenVDBDisplayVP2::aClipPosition.key].number=.5f;
    values[DOpenVDBDisplayVP2::aClipWidth.key].number=.2f;
    values[DOpenVDBDisplayVP2::aClipAxis.key].integer=1;
    values[DOpenVDBDisplayVP2::aPointSize.key].number=7.0f;
    values[DOpenVDBDisplayVP2::aVectorScale.key].number=3.0f;
    values[DOpenVDBDisplayVP2::aIsDrawHeads.key].boolean=false;
    values[DOpenVDBDisplayVP2::aVectorColor.key].color=MColor(.2f,.3f,.4f,1);
}
static bool empty(const DOpenVDBDisplayVP2Override& node){
    return node.mBBoxBuffersVP2.empty() && node.mNodeBuffersVP2.empty() && node.mLeafBuffersVP2.empty() &&
        node.mTileBuffersVP2.empty() && node.mPointBuffersVP2.empty() && node.mVectorBuffersVP2.empty() && node.mSurfaceBuffersVP2.empty();
}
static void stale(DOpenVDBDisplayVP2Override& node){
    node.mMultiplierChanged=false;
    node.visBBox=node.visNode=node.visLeaf=node.visTile=node.visSurf=node.visPoint=node.visVector=true;
    node.mBBoxBuffersVP2.resize(1);node.mNodeBuffersVP2.resize(1);node.mLeafBuffersVP2.resize(1);
    node.mTileBuffersVP2.resize(1);node.mPointBuffersVP2.resize(1);node.mVectorBuffersVP2.resize(1);node.mSurfaceBuffersVP2.resize(1);
    node.mBBoxBuffersVP2[0].points={99};node.mSurfaceBuffersVP2[0].points={MPoint(99,99,99)};
}
static bool fallback(const MBoundingBox& box){return box.min().x==-1 && box.max().x==1 && box.min().y==-1 && box.max().z==1;}
static void wireTests(){
    reset();std::vector<unsigned int> indices(48,99);std::vector<float> points(48,99),colors(48,99);
    WireBoxBuilder builder(grid->map,indices,points,colors);
    builder.add(1,grid->bounds,openvdb::Vec3s(.1f,.2f,.3f));
    assert(points[0]==99 && points[24]==-.5f && points[27]==-.5f && points[29]==1.5f);
    assert(indices[24]==8 && indices[25]==9 && indices[47]==15);
    const auto oldPoints=points,oldColors=colors;const auto oldIndices=indices;
    auto reject=[&](const openvdb::CoordBBox& bounds,unsigned int index=1,openvdb::Vec3s color=openvdb::Vec3s(.1f)){
        bool threw=false;try{builder.add(index,bounds,color);}catch(const std::exception&){threw=true;}
        assert(threw && points==oldPoints && colors==oldColors && indices==oldIndices);
    };
    reject(openvdb::CoordBBox());reject(grid->bounds,2);reject(grid->bounds,std::numeric_limits<unsigned int>::max());
    reject(grid->bounds,1,openvdb::Vec3s(std::numeric_limits<float>::quiet_NaN()));
    for(double value:{1e40,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        grid->map.translation[0]=value;std::feclearexcept(FE_ALL_EXCEPT);reject(grid->bounds);
        assert((std::fetestexcept(FE_OVERFLOW)&FE_OVERFLOW)==0);
    }
    grid->map=openvdb::math::Transform();calls.clear();builder.add(1,grid->bounds,openvdb::Vec3s(.1f));const int count=static_cast<int>(calls.size());
    points=oldPoints;colors=oldColors;indices=oldIndices;
    for(int step=0;step<count;++step){calls.clear();failAt=step;failureKind=2;reject(grid->bounds);}
    failAt=-1;points.resize(25);bool threw=false;try{builder.add(1,grid->bounds,openvdb::Vec3s(.1f));}catch(const std::exception&){threw=true;}assert(threw);

    reset();lineBufferObjectVP2 buffer;BoundingBoxGeoVP2 bbox(buffer);bbox(grid);assert(bbox.hasBounds() && buffer.points.size()==24);
    grid->bounds=openvdb::CoordBBox();bbox(grid);assert(!bbox.hasBounds() && buffer.points.empty() && buffer.indexArray.empty());
    bbox(nullptr);assert(!bbox.hasBounds());
    grid->bounds=openvdb::CoordBBox(openvdb::Coord(0),openvdb::Coord(1));
    grid->map.matrix[0][1]=-2;grid->map.matrix[0][0]=-3;bbox(grid);
    assert(bbox.min()[0]==-7.5f && bbox.max()[0]==2.5f);
    grid->map.translation[0]=1e40;try{bbox(grid);}catch(const std::exception&){}
    assert(!bbox.hasBounds() && buffer.points.empty());
}
static void surfaceTests(){
    reset();BufferObjectVP2 buffer;
    auto reject=[&](float iso=0.0f,std::size_t vertices=100,std::size_t indices=100){
        buffer.points={MPoint(99)};buffer.normals={MPoint(99)};buffer.indexArray={99};
        SurfaceGeoVP2 draw(buffer,iso,false,vertices,indices);draw.operator()<openvdb::GridBase>(grid);
        assert(!draw.succeeded() && draw.generatedVertexCount()==0 && draw.generatedIndexCount()==0);
        assert(buffer.points.empty() && buffer.normals.empty() && buffer.indexArray.empty());
    };
    for(float iso:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        calls.clear();reject(iso);assert(std::find(calls.begin(),calls.end(),"mesher.create")==calls.end());
    }
    grid->bounds=openvdb::CoordBBox(openvdb::Coord(0),openvdb::Coord(5));grid->map.matrix[0][0]=1e38;
    calls.clear();reject();assert(std::find(calls.begin(),calls.end(),"mesher.create")==calls.end());
    grid->map=openvdb::math::Transform();grid->work=dommy::viewport::kMaxVdbViewportSurfaceWorkItemCount+1;reject();grid->work=8;
    reject(0,0);reject(0,100,0);reject(0,2);reject(0,100,2);
    for(std::size_t vertex:{2U,3U})for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        openvdb::mesherPoints[vertex][2]=value;reject();openvdb::mesherPoints[vertex][2]=0;
    }
    openvdb::mesherPoints[2][2]=0;openvdb::triangle[2]=-1;reject();openvdb::triangle[2]=200;reject();openvdb::triangle[2]=2;
    for(bool inverted:{false,true}){
        SurfaceGeoVP2 draw(buffer,.25f,inverted,100,100);draw.operator()<openvdb::GridBase>(grid);
        assert(draw.succeeded() && draw.generatedIndexCount()==3 && buffer.normals[0].z==(inverted?-1:1));
        assert(openvdb::lastIso==.25f);
    }
    openvdb::mesherQuad=true;SurfaceGeoVP2 quad(buffer,0,false,100,100);quad.operator()<openvdb::GridBase>(grid);
    assert(quad.succeeded() && quad.generatedIndexCount()==6 && buffer.indexArray==std::vector<unsigned int>({0,1,2,2,3,0}));
    openvdb::mesherQuad=false;calls.clear();SurfaceGeoVP2 valid(buffer,0,false,100,100);valid.operator()<openvdb::GridBase>(grid);const int count=static_cast<int>(calls.size());
    for(int step=0;step<count;++step){calls.clear();failAt=step;failureKind=2;bool threw=false;try{reject();}catch(const std::exception&){threw=true;}
        assert(threw && buffer.points.empty() && buffer.normals.empty() && buffer.indexArray.empty());}
    failAt=-1;
    GeometryBufferLayout layout;BufferObjectVP2 invalid;invalid.points={MPoint(1e40)};invalid.normals={MPoint(0,0,1)};invalid.indexArray={0};
    assert(!getSurfaceBufferLayout({invalid},layout));
    invalid.points={MPoint(0)};invalid.normals={MPoint(std::numeric_limits<double>::quiet_NaN())};assert(!getSurfaceBufferLayout({invalid},layout));
    lineBufferObjectVP2 line;line.points={0,std::numeric_limits<float>::infinity(),0};line.indexArray={0};assert(!getLineBufferLayout({line},layout));
}
static bool clear(const valueBufferObjectVP2& b){return b.points.empty() && b.colors.empty() && b.normals.empty() && b.indexArray.empty();}
template <typename F> static void rejected(F function){bool threw=false;try{function();}catch(const std::exception&){threw=true;}assert(threw);}
static void pointTests(){
    reset();grid->data.leaves.resize(3);grid->data.leaves[1].samples.clear();
    grid->data.leaves[0].samples.clear();grid->data.leaves[2].samples.clear();
    for(int i=0;i<5;++i)grid->data.leaves[0].samples.push_back({openvdb::Coord(i,0,0),float(i)});
    for(int i=8;i<11;++i)grid->data.leaves[2].samples.push_back({openvdb::Coord(i,0,0),float(i)});
    valueBufferObjectVP2 buffer;
    auto generate=[&](std::size_t budget,bool low=false,short color=0){
        ActiveVoxelGeoVP2 draw(buffer,low,color,budget);draw.operator()<openvdb::FloatGrid>(grid);
        assert(draw.generatedPointCount()==buffer.indexArray.size());
    };
    generate(4);assert(buffer.indexArray==std::vector<unsigned int>({0,1,2,3}));
    assert(buffer.points==std::vector<float>({2,0,0,4,0,0,9,0,0,10,0,0}));
    const auto baseline=buffer.points;tbb::reversePartitions=true;generate(4);assert(buffer.points==baseline);
    tbb::reversePartitions=false;generate(4,true);assert(buffer.points==std::vector<float>({4,0,0,10,0,0}));
    generate(1);assert(buffer.points==std::vector<float>({10,0,0}));generate(0);assert(clear(buffer));

    reset();grid->data.leaves[0].samples={{openvdb::Coord(0,0,0),-1},{openvdb::Coord(1,0,0),0},{openvdb::Coord(2,0,0),1}};
    generate(3);assert(buffer.colors==std::vector<float>({.3f,.3f,.9f,.3f,.9f,.3f,.9f,.3f,.3f}));
    assert(buffer.normals==std::vector<float>({0,0,1,0,0,1,0,0,1}));
    generate(3,false,1);assert(buffer.colors==buffer.normals);
    grid->gridClass=openvdb::GRID_LEVEL_SET;generate(3);assert(buffer.colors[3]==.9f && buffer.colors[4]==.3f);
    grid->data.gradient=openvdb::Vec3d(0);generate(3);assert(buffer.normals[1]==-1 && buffer.normals[0]==0);
    BufferObject legacy;ActiveVoxelGeo legacyDraw(legacy);legacyDraw.operator()<openvdb::FloatGrid>(grid);
    assert(legacy.points==buffer.points && legacy.colors==buffer.colors && legacy.normals==buffer.normals);

    // Float positions lose low bits, but scalar/vector lookups retain source Coord.
    reset();grid->map.translation[0]=1e10+17;grid->data.leaves[0].samples[0].value=-1;
    generate(1);assert(buffer.points[0]==float(1e10+17) && buffer.colors[2]==.9f);
    assert(openvdb::valueQueries==std::vector<openvdb::Coord>({openvdb::Coord(0)}));
    assert(std::find(calls.begin(),calls.end(),"transform.inverse")==calls.end());
    auto vectors=std::make_shared<openvdb::Vec3DGrid>();vectors->map.translation[0]=1e10+17;
    vectors->data.leaves[0].samples[0].value=openvdb::Vec3d(3,4,-2);
    ActiveVoxelVecVP2 vectorDraw(buffer,false,1);openvdb::valueQueries.clear();
    vectorDraw.operator()<openvdb::Vec3DGrid>(vectors);
    assert(buffer.normals==std::vector<float>({3,4,-2}) && buffer.colors==buffer.normals);
    assert(openvdb::valueQueries==std::vector<openvdb::Coord>({openvdb::Coord(0)}));
    auto floatVectors=std::make_shared<openvdb::Vec3SGrid>();
    floatVectors->data.leaves[0].samples[0].value=openvdb::Vec3s(-3,0,4);
    vectorDraw.operator()<openvdb::Vec3SGrid>(floatVectors);assert(buffer.normals==std::vector<float>({-3,0,4}));

    for(int axis=0;axis<3;++axis)for(double invalid:{1e40,-1e40,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        reset();grid->map.translation[axis]=invalid;std::feclearexcept(FE_ALL_EXCEPT);
        ActiveVoxelGeoVP2 draw(buffer,false,0,1);rejected([&]{draw.operator()<openvdb::FloatGrid>(grid);});
        assert(clear(buffer) && draw.generatedPointCount()==0 && (std::fetestexcept(FE_OVERFLOW)&FE_OVERFLOW)==0);
        assert(std::find(calls.begin(),calls.end(),"accessor.create")==calls.end());
    }
    const double floatMaximum=std::numeric_limits<float>::max();
    for(int axis=0;axis<3;++axis)for(double invalid:{std::nextafter(floatMaximum,std::numeric_limits<double>::infinity()),1e40,-1e40,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        vectors->data.leaves[0].samples[0].value=openvdb::Vec3d(1);vectors->data.leaves[0].samples[0].value[axis]=invalid;
        std::feclearexcept(FE_ALL_EXCEPT);rejected([&]{vectorDraw.operator()<openvdb::Vec3DGrid>(vectors);});
        assert(clear(buffer) && vectorDraw.generatedPointCount()==0 && (std::fetestexcept(FE_OVERFLOW)&FE_OVERFLOW)==0);
    }
    vectors->data.leaves[0].samples[0].value=openvdb::Vec3d(floatMaximum,-floatMaximum,0);
    vectorDraw.operator()<openvdb::Vec3DGrid>(vectors);assert(buffer.normals[0]==std::numeric_limits<float>::max());
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        reset();grid->data.leaves[0].samples[0].value=invalid;rejected([&]{generate(1);});assert(clear(buffer));
    }
    reset();grid->data.leaves[0].samples={{openvdb::Coord(0),std::numeric_limits<float>::quiet_NaN()},{openvdb::Coord(1),1}};
    rejected([&]{generate(1);});assert(clear(buffer)); // Invalid value is not selected by the sample plan.
    reset();grid->data.gradient[2]=std::numeric_limits<double>::quiet_NaN();rejected([&]{generate(1);});assert(clear(buffer));
    reset();grid->data.leaves[0].samples[0].coordinate[0]=std::numeric_limits<int>::max();
    calls.clear();rejected([&]{generate(1);});assert(clear(buffer) && std::find(calls.begin(),calls.end(),"gradient")==calls.end());
    grid->data.leaves[0].samples[0].coordinate[0]=std::numeric_limits<int>::max()-1;generate(1);assert(!clear(buffer));
    reset();ActiveVoxelGeoVP2 badColor(buffer,false,0,1);badColor.setColorMaxPosValue(openvdb::Vec3s(std::numeric_limits<float>::quiet_NaN()));
    rejected([&]{badColor.operator()<openvdb::FloatGrid>(grid);});assert(clear(buffer));

    // Validate the complete range plan before parallel writes, then leaf cardinality.
    reset();openvdb::tree::LeafManager<const openvdb::FloatGrid::TreeType> leaves(grid->data);
    std::vector<float> points(3,99);std::vector<unsigned int> indices(1,99);std::vector<openvdb::Coord> coordinates(1,openvdb::Coord(99));
    std::vector<std::size_t> offsets{0},counts{1};
    PointGenerator<const openvdb::FloatGrid::TreeType> generator(points,indices,coordinates,leaves,offsets,counts,grid->map);
    coordinates.clear();rejected([&]{generator.runParallel();});assert(points[0]==99 && indices[0]==99);
    coordinates.resize(1);offsets[0]=1;rejected([&]{generator.runParallel();});assert(points[0]==99);
    offsets[0]=0;counts[0]=2;rejected([&]{generator.runParallel();});counts[0]=0;rejected([&]{generator.runParallel();});
    counts[0]=1;points.resize(2);rejected([&]{generator.runParallel();});points.resize(3);counts.clear();rejected([&]{generator.runParallel();});
    grid->data.leaves.resize(2);points.resize(6);indices.resize(2);coordinates.resize(2);offsets={0,0};counts={1,1};
    rejected([&]{generator.runParallel();});
    reset();grid->data.leaves[0].samples.push_back({openvdb::Coord(1),1});grid->data.leaves[0].countOverride=1;
    rejected([&]{generate(1);});assert(clear(buffer));
    grid->data.leaves[0].countOverride=3;rejected([&]{generate(3);});assert(clear(buffer));
    reset();coordinates={openvdb::Coord(0)};std::vector<float> colors(2),normals(3);
    openvdb::Vec3s colorMap[4]={openvdb::Vec3s(.3f),openvdb::Vec3s(.9f),openvdb::Vec3s(.5f),openvdb::Vec3s(.7f)};
    PointAttributeGenerator<openvdb::FloatGrid> scalarAttributes(coordinates,colors,normals,*grid,0,1,colorMap);
    rejected([&]{scalarAttributes.runParallel();});colors.resize(3);normals.resize(2);rejected([&]{scalarAttributes.runParallel();});
    VectorAttributeGenerator<openvdb::Vec3DGrid> vectorAttributes(coordinates,colors,normals,*vectors);
    rejected([&]{vectorAttributes.runParallel();});
    normals.resize(3);scalarAttributes.runParallel();
    PointAttributeGenerator<openvdb::FloatGrid> colorsOnly(coordinates,colors,*grid,0,1,colorMap);colorsOnly.runParallel();
    VectorAttributeGenerator<openvdb::Vec3DGrid> vectorsOnly(coordinates,colors,*vectors);vectorsOnly.runParallel();

    // Every component stream is independently rejected at final GPU admission.
    valueBufferObjectVP2 gpu;gpu.points={0,0,0};gpu.colors={1,1,1};gpu.normals={0,0,1};gpu.indexArray={0};
    std::size_t pointCount=0,componentCount=0;assert(getValueBufferLayout({gpu},pointCount,componentCount));
    for(auto member:{&valueBufferObjectVP2::points,&valueBufferObjectVP2::colors}){
        for(int axis=0;axis<3;++axis){(gpu.*member)[axis]=std::numeric_limits<float>::infinity();assert(!getValueBufferLayout({gpu},pointCount,componentCount));(gpu.*member)[axis]=0;}
    }
    for(int axis=0;axis<3;++axis){gpu.normals[axis]=std::numeric_limits<float>::quiet_NaN();assert(!getValueBufferLayout({gpu},pointCount,componentCount));gpu.normals[axis]=0;}
}
static void callbackTests(){
    reset();DOpenVDBDisplayVP2Override node;
    assert(node.mPointSize==2.0f && node.mVectorScale==1.0f && node.mDrawHeads && node.mVectorColor.r==1.0f);
    node.updateDG();assert(node.mMultiplierChanged && !empty(node));
    assert(node.mPointSize==7.0f && node.mVectorScale==3.0f && !node.mDrawHeads && node.mVectorColor.g==.3f);
    const auto baseline=calls;
    for(int step=0;step<static_cast<int>(baseline.size());++step)for(int kind=0;kind<3;++kind){
        reset();stale(node);failAt=step;failureKind=kind;node.updateDG();
        assert(node.mMultiplierChanged && empty(node));
        assert(!node.visBBox && !node.visNode && !node.visLeaf && !node.visTile && !node.visSurf && !node.visPoint && !node.visVector);
        calls.clear();failAt=-1;node.updateDG();assert(!empty(node));
    }
    reset();stale(node);grid->memory=dommy::vdb::kMaxVdbPayloadBytes+1;node.updateDG();assert(empty(node));
    DOpenVDBDisplayVP2 locator;assert(fallback(locator.boundingBox()));
    reset();inputObject.type=999;stale(node);node.updateDG();assert(empty(node) && fallback(locator.boundingBox()));
    struct OtherData:MPxData{} other;inputObject.type=DOpenVDBData::id;inputObject.data=&other;node.updateDG();assert(empty(node));
    reset();inputObject=MObject();node.updateDG();assert(empty(node) && fallback(locator.boundingBox()));
    reset();input.mGrids={nullptr};node.updateDG();assert(empty(node) && fallback(locator.boundingBox()));
    reset();input.mGrids.assign(dommy::vdb::kMaxVdbGridCount+1,grid);node.updateDG();assert(empty(node));
    reset();input.mGrids.clear();node.updateDG();assert(empty(node));
    reset();values[DOpenVDBDisplayVP2::aVdbSelectedGridNames.key].text=MString("missing");node.updateDG();assert(empty(node));
    reset();grid->bounds=openvdb::CoordBBox();node.updateDG();assert(node.mBBoxBuffersVP2[0].points.empty() && fallback(locator.boundingBox()));
    reset();grid->map.translation[0]=1e40;node.updateDG();assert(empty(node) && fallback(locator.boundingBox()));
    reset();grid->map.translation[0]=std::numeric_limits<double>::quiet_NaN();node.updateDG();assert(empty(node) && fallback(locator.boundingBox()));
    reset();auto vector=std::make_shared<openvdb::Vec3SGrid>();input.mGrids.push_back(vector);node.updateDG();
    assert(!node.mVectorBuffersVP2[1].indexArray.empty() && node.mSurfaceBuffersVP2[1].points.empty());
    reset();const MBoundingBox box=locator.boundingBox();assert(box.min().x==-.5 && box.max().x==1.5);
    const auto boundCalls=calls;
    // Exclude the preceding reset's empty trace: every callback boundary is injected.
    for(int step=0;step<static_cast<int>(boundCalls.size());++step)for(int kind=0;kind<3;++kind){
        reset();failAt=step;failureKind=kind;assert(fallback(locator.boundingBox()));
    }
    reset();inputConnected=false;assert(fallback(locator.boundingBox()));
    for(short color:{short(-1),short(2)}){reset();values[DOpenVDBDisplayVP2::aColorType.key].integer=color;node.updateDG();assert(empty(node));}
    for(short axis:{short(-1),short(3)}){reset();values[DOpenVDBDisplayVP2::aClipAxis.key].integer=axis;node.updateDG();assert(empty(node));}
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        for(const MObject& attribute:{DOpenVDBDisplayVP2::aClipWidth,DOpenVDBDisplayVP2::aClipPosition,DOpenVDBDisplayVP2::aIsovalue,
            DOpenVDBDisplayVP2::aPointSize,DOpenVDBDisplayVP2::aVectorScale}){
            reset();values[attribute.key].number=invalid;node.updateDG();assert(empty(node));
        }
    }
    reset();values[DOpenVDBDisplayVP2::aVisualizeSurface.key].boolean=false;
    values[DOpenVDBDisplayVP2::aIsovalue.key].number=std::numeric_limits<float>::quiet_NaN();
    node.updateDG();assert(!empty(node)); // Disabled controls are not consumed.
    reset();values[DOpenVDBDisplayVP2::aVisualizeActiveVoxels.key].boolean=false;
    values[DOpenVDBDisplayVP2::aDisplayVector.key].boolean=false;
    values[DOpenVDBDisplayVP2::aPointSize.key].number=std::numeric_limits<float>::quiet_NaN();
    values[DOpenVDBDisplayVP2::aVectorScale.key].number=std::numeric_limits<float>::quiet_NaN();
    values[DOpenVDBDisplayVP2::aVectorColor.key].color=MColor(std::numeric_limits<float>::quiet_NaN(),0,0,1);
    node.updateDG();assert(!empty(node));
    reset();values[DOpenVDBDisplayVP2::aVectorColor.key].color=MColor(std::numeric_limits<float>::quiet_NaN(),0,0,1);
    node.updateDG();assert(empty(node));
    reset();node.updateDG();assert(node.visBBox);node.clearGeometryBuffers();assert(empty(node) && node.visBBox);
    auto pointOnly=[&]{
        for(const MObject& attribute:{DOpenVDBDisplayVP2::aVisualizeBBox,DOpenVDBDisplayVP2::aVisualizeInternalNodes,
            DOpenVDBDisplayVP2::aVisualizeLeafNodes,DOpenVDBDisplayVP2::aVisualizeActiveTiles,
            DOpenVDBDisplayVP2::aVisualizeSurface,DOpenVDBDisplayVP2::aIsClip})values[attribute.key].boolean=false;
    };
    reset();pointOnly();grid->map.translation[0]=1e40;stale(node);node.updateDG();assert(empty(node) && node.mMultiplierChanged);
    grid->map.translation[0]=1e10+17;node.updateDG();assert(node.mPointBuffersVP2[0].indexArray.size()==1);
    reset();pointOnly();auto invalidVector=std::make_shared<openvdb::Vec3DGrid>();
    invalidVector->data.leaves[0].samples[0].value[1]=1e40;input.mGrids.push_back(invalidVector);stale(node);node.updateDG();
    assert(empty(node) && node.mMultiplierChanged);invalidVector->data.leaves[0].samples[0].value[1]=4;
    node.updateDG();assert(node.mPointBuffersVP2[0].indexArray.size()==1 && node.mVectorBuffersVP2[1].normals[1]==4);
    VdbViewportSettings settings;settings.clipWidth=91;reset();failAt=0;assert(!readVdbViewportSettings(MObject(),settings));assert(settings.clipWidth==91);
}
int main(){wireTests();surfaceTests();pointTests();callbackTests();std::cout<<"VDB wire/surface/point/vector builders, complete DG/bounds callbacks, typed payload/settings failures and recovery passed (API-shaped native boundaries)\n";}
