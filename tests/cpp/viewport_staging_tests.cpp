#include <cstdlib>
#include <new>
// One-shot failure for the actual std::vector<float> staging allocation.
std::size_t failAllocationBytes=0U;
void* operator new(std::size_t size) {
    if (failAllocationBytes != 0U && size == failAllocationBytes) {
        failAllocationBytes=0U;
        throw std::bad_alloc();
    }
    if (void* memory=std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
#include "viewport_safety_host.h"
#include <cfenv>
#include <iomanip>
#include <iostream>
#include <set>
#include "viewport_staging.inc"

std::map<std::string,int> arnoldAttributes;
std::vector<std::string> arnoldQueries;
std::string arnoldMissingPlug;
bool arnoldNullNode=false;
MPlug DPInstancerTranslatorAi::FindMayaPlug(const char* name)const {
    arnoldQueries.push_back(name);
    if(arnoldMissingPlug==name)return MPlug(MObject(9),MObject::kNullObj);
    const auto found=arnoldAttributes.find(name);
    const int key=found==arnoldAttributes.end()?int(arnoldAttributes.size()+100):found->second;
    arnoldAttributes[name]=key;
    return MPlug(MObject(9),MObject(key));
}
MObject DPInstancerTranslatorAi::GetMayaObject()const{return arnoldNullNode?MObject::kNullObj:MObject(9);}
void arnoldTransformInputTests() {
    DPInstancerTranslatorAi translator;
    for(short mode:{short(MAT_ANIM_NONE),short(MAT_ANIM_ATTRIBUTE),short(MAT_ANIM_INPUT_DATA)}) {
        reset();arnoldQueries.clear();
        const int animation=translator.FindMayaPlug("AnimationType").attr.key;
        const int seed=translator.FindMayaPlug("Seed").attr.key;
        settingValues[animation].mode=mode;settingValues[seed].integer=7;
        DTransformParams params;
        params.mAnimationAttribute="old";params.mSeed=123;
        assert(translator.evalAnimMatrixParms(params)==MS::kSuccess);
        assert(!params.mAnimationInputIsData && params.mSeed==7);
        const bool queried=std::find(arnoldQueries.begin(),arnoldQueries.end(),"InAnimMatrix")!=arnoldQueries.end();
        assert(queried==(mode==MAT_ANIM_INPUT_DATA));
        if(mode==MAT_ANIM_INPUT_DATA)
            assert(params.mAnim_Mat_InputData_Object.key==arnoldAttributes.at("InAnimMatrix"));
        else assert(params.mAnim_Mat_InputData_Object.isNull());
        const std::size_t steps=operations.size();
        for(std::size_t step=0;step<steps;++step) {
            reset();failAt=int(step);failureKind=1;
            DTransformParams failed;failed.mAnimationAttribute="old";failed.mSeed=123;
            assert(translator.evalAnimMatrixParms(failed)!=MS::kSuccess);
            assert(failed.mAnimationAttribute=="old" && failed.mSeed==123);
            assert(operations.size()==step+1U);
        }
    }
    reset();settingValues[arnoldAttributes.at("AnimationType")].mode=MAT_ANIM_TYPE_COUNT;
    DTransformParams invalid;invalid.mSeed=123;
    assert(translator.evalAnimMatrixParms(invalid)==MS::kInvalidParameter && invalid.mSeed==123);
    reset();settingValues[arnoldAttributes.at("AnimationType")].mode=MAT_ANIM_INPUT_DATA;
    arnoldMissingPlug="InAnimMatrix";
    assert(translator.evalAnimMatrixParms(invalid)==MS::kFailure && invalid.mSeed==123);
    arnoldMissingPlug.clear();arnoldNullNode=true;reset();
    assert(translator.evalAnimMatrixParms(invalid)==MS::kFailure && invalid.mSeed==123);
    arnoldNullNode=false;
}

template <typename Node> void gpuFailures(Node node, const MHWRender::MGeometryRequirements& requirements,
                                         const MHWRender::MRenderItemList& items) {
    reset();
    MHWRender::MGeometry geometry;
    node.populateGeometry(requirements, items, geometry);
    assert(!node.isDirty());
    const auto trace = operations;
    assert(!trace.empty());
    for (std::size_t i=0; i<trace.size(); ++i) for (int kind : {1,2,3}) {
        reset(); failAt=static_cast<int>(i); failureKind=kind;
        Node failed=node; failed.setDirty(false);
        MHWRender::MGeometry failedGeometry;
        failed.populateGeometry(requirements,items,failedGeometry);
        if (!failed.isDirty()) std::cerr << "Failure was marked clean: " << trace[i] << " at " << i << ", kind " << kind << '\n';
        assert(failed.isDirty());
        reset(); MHWRender::MGeometry retry;
        failed.populateGeometry(requirements,items,retry);
        assert(!failed.isDirty());
    }
    // Every emitted stream must reject wrong scalar types/sizes and dimensions
    // before acquiring storage and leave the callback eligible for retry.
    for (std::size_t i=0;i<requirements.list.values.size();++i) {
        for (int mode=0;mode<4;++mode) {
            auto malformed=requirements;
            auto& descriptor=malformed.list.values[i];
            if (mode==0) descriptor.type=MHWRender::MGeometry::kUnsignedInt32;
            else if (mode==1) {
                descriptor.type=MHWRender::MGeometry::kDouble;
                descriptor.elementBytes=8U;
            }
            else if (mode==2) descriptor.elementBytes=1U;
            else descriptor.components=descriptor.components == 3 ? 2 : 3;
            Node rejected=node; rejected.setDirty(false);
            reset(); MHWRender::MGeometry rejectedGeometry;
            rejected.populateGeometry(malformed,items,rejectedGeometry);
            assert(rejected.isDirty());
        }
    }
}
MHWRender::MGeometryRequirements requirements(std::initializer_list<MHWRender::MVertexBufferDescriptor> list) {
    MHWRender::MGeometryRequirements result; result.list.values=list; return result;
}
MHWRender::MRenderItemList items(std::initializer_list<const char*> names) {
    MHWRender::MRenderItemList result;
    for (const auto name : names) result.items.emplace_back(name);
    return result;
}

void vertexRequirementTests() {
    using namespace MHWRender;
    const auto empty=requirements({});
    int visited=0;
    reset();
    assert(dommy::viewport::forEachVertexRequirement(empty,
        [&](const MVertexBufferDescriptor&) {++visited; return true;}));
    assert(visited==0 && operations.empty());

    auto invalid=empty;
    invalid.list.overrideCount=true;
    invalid.list.reportedCount=-1;
    assert(!dommy::viewport::forEachVertexRequirement(invalid,
        [&](const MVertexBufferDescriptor&) {++visited; return true;}));
    assert(visited==0 && operations.empty());

    const auto streams=requirements({{"first",MGeometry::kPosition,3},
        {"unknown",MGeometry::kTexture,1},{"last",MGeometry::kPosition,3}});
    std::vector<std::string> names;
    reset();
    assert(dommy::viewport::forEachVertexRequirement(streams,
        [&](const MVertexBufferDescriptor& stream) {
            names.emplace_back(stream.name().asChar()); return true;
        }));
    // Unknown requests belong to the consumer, not the helper.
    assert(names==std::vector<std::string>({"first","unknown","last"}));
    assert(operations==std::vector<std::string>(3,"gpu.descriptor"));
    auto duplicates=streams;
    duplicates.list.values.emplace_back("first",MGeometry::kPosition,3);
    names.clear(); reset();
    assert(dommy::viewport::forEachVertexRequirement(duplicates,
        [&](const MVertexBufferDescriptor& stream) {
            names.emplace_back(stream.name().asChar()); return true;
        }));
    assert(names==std::vector<std::string>({"first","unknown","last","first"}));
    assert(operations==std::vector<std::string>(4,"gpu.descriptor"));

    for (int failure=0;failure<3;++failure) {
        reset(); failAt=failure; failureKind=1; names.clear();
        assert(!dommy::viewport::forEachVertexRequirement(streams,
            [&](const MVertexBufferDescriptor& stream) {
                names.emplace_back(stream.name().asChar()); return true;
            }));
        assert(names.size()==static_cast<std::size_t>(failure));
        assert(operations.size()==static_cast<std::size_t>(failure+1));
    }
    reset(); names.clear();
    assert(!dommy::viewport::forEachVertexRequirement(streams,
        [&](const MVertexBufferDescriptor& stream) {
            names.emplace_back(stream.name().asChar()); return names.size()!=2U;
        }));
    assert(names==std::vector<std::string>({"first","unknown"}) && operations.size()==2U);

    reset(); names.clear(); bool caught=false;
    try {
        dommy::viewport::forEachVertexRequirement(streams,
            [&](const MVertexBufferDescriptor& stream) -> bool {
                names.emplace_back(stream.name().asChar());
                if (names.size()==2U) throw std::runtime_error("visitor rejected stream");
                return true;
            });
    } catch (const std::runtime_error&) {caught=true;}
    assert(caught && names.size()==2U && operations.size()==2U);
    for (int kind:{2,3}) {
        reset(); failAt=1; failureKind=kind; visited=0; caught=false;
        try {
            dommy::viewport::forEachVertexRequirement(streams,
                [&](const MVertexBufferDescriptor&) {++visited; return true;});
        } catch (const std::exception&) {caught=true;}
        assert(caught && visited==1 && operations.size()==2U);
    }
    reset();
}

void gpuTests() {
    using namespace MHWRender;
    DMeshDisplayOverride mesh;
    auto& d=mesh.dData;
    d.isVertex=true; d.pointCount=3; d.primCount=1;
    d.mPointBuffer={0,0,0,1,0,0,0,1,0}; d.mFaceIndexList={0,1,2}; d.mWireIndexList={0,1};
    d.normBuffer={0,0,1,0,0,1,0,0,1}; d.colBuffer=std::vector<float>(12,1);
    gpuFailures(mesh,requirements({{"DMeshItem",MGeometry::kPosition,3},
        {"DMeshItem",MGeometry::kNormal,3},{"DMeshColorItem",MGeometry::kColor,4},
        {"DMeshWireItem",MGeometry::kPosition,3},{"DMeshPointItem",MGeometry::kPosition,3}}),
        items({"DMeshItem","DMeshWireItem","DMeshPointItem"}));
    for (int mode=0;mode<4;++mode) {
        DMMarkerOverride marker; auto& m=marker.dData;
        m.isColor=m.showIndex=m.isVector=m.isValue=m.isGroup=true;
        m.pointCount=3; m.primCount=1; m.meshFirstIndex={0}; m.indexBuffer={0,1,2};
        m.indexType=m.infoClass=mode==3?0:1; m.mPointBuffer=d.mPointBuffer; m.colBuffer=d.colBuffer;
        m.vecBuffer=std::vector<float>(m.infoClass==0?3:9,1);
        m.grpPtBuffer={0,0,0,1,0,0}; m.grpIndexBuffer={0,1}; m.totalGroupPointCount=2;
        const char* valueName=mode==1?"DMeshVal2Item":(mode==2?"DMeshVal1Item":"DMeshVal3Item");
        const int dimension=mode==1?2:(mode==2?1:3);
        const char* semantic=mode==1?"numeric2value":(mode==2?"numericvalue":"numeric3value");
        m.valType=mode==1?DP_VAL_FLOAT2:(mode==2?DP_VAL_FLOAT:0);
        m.val3Buffer=std::vector<float>((m.infoClass==0?1:3)*dimension,1);
        auto r=requirements({{"DMeshColorItem",MGeometry::kPosition,3},
            {"DMeshColorItem",MGeometry::kColor,4},{"DMeshIndexItem",MGeometry::kPosition,3},
            {"DMeshIndexItem",MGeometry::kTexture,1,"numericvalue"},
            {"DMeshVecItem",MGeometry::kPosition,3},{"DMeshVecItem",MGeometry::kTexture,3},
            {valueName,MGeometry::kPosition,3},{valueName,MGeometry::kTexture,dimension,semantic},
            {"DMGroupPolyItem",MGeometry::kPosition,3}});
        if (mode==0 || mode==1) r.list.values.emplace_back(valueName,MGeometry::kTexture,1,"numericvalue");
        gpuFailures(marker,r,items({"DMeshColorItem","DMeshIndexItem","DMeshVecItem",valueName,"DMGroupPolyItem"}));
    }
    DPInstancerOverride instancer;
    instancer.dData.pointList=d.mPointBuffer; instancer.dData.indexList={0,1,2}; instancer.dData.normalList=d.normBuffer;
    gpuFailures(instancer,requirements({{"instance",MGeometry::kPosition,3},
        {"instance",MGeometry::kNormal,3}}),items({"instance"}));
    instancer.dData.pointList.clear(); reset(); MGeometry empty;
    instancer.populateGeometry(requirements({}),items({}),empty);
    assert(!instancer.mDataChanged && operations.empty());

    DPointDisplayOverride point;
    point.dData.ptBuffer=d.mPointBuffer; point.dData.pointCount=3;
    gpuFailures(point,requirements({{"DPointItem",MGeometry::kPosition,3}}),items({"DPointItem"}));
    DCurveDisplayOverride curve;
    curve.dData.cvsBuffer=d.mPointBuffer; curve.dData.pointCount=3; curve.dData.primCount=1;
    curve.dData.colBuffer=d.colBuffer; curve.dData.indexBuffer={0,1,1,2};
    gpuFailures(curve,requirements({{"DCurveItem",MGeometry::kPosition,3},
        {"DCurveColorItem",MGeometry::kColor,4}}),items({"DCurveItem","DCurveColorItem"}));
    for (int mode=0;mode<4;++mode) {
        const bool primitive=mode==3;
        const int dimension=mode==1?2:(mode==2?1:3);
        const char* semantic=mode==1?"numeric2value":(mode==2?"numericvalue":"numeric3value");
        DPMarkerOverride pm; auto& p=pm.dData;
        p.isColor=p.showIndex=p.isVector=p.isValue=p.isGroup=true;
        p.pointCount=3; p.ptBuffer=d.mPointBuffer; p.colBuffer=d.colBuffer;
        p.vecBuffer=std::vector<float>(9,1); p.val3Buffer=std::vector<float>(3*dimension,1);
        p.valType=mode==1?DP_VAL_FLOAT2:(mode==2?DP_VAL_FLOAT:0);
        p.grpPtBuffer={0,0,0,1,0,0}; p.totalGroupPointCount=2;
        const char* pv=mode==1?"DPMarkVal2Item":(mode==2?"DPMarkVal1Item":"DPMarkVal3Item");
        auto pr=requirements({{"DPMarkColItem",MGeometry::kPosition,3},
            {"DPMarkColItem",MGeometry::kColor,4},{"DPMarkIndexItem",MGeometry::kPosition,3},
            {"DPMarkIndexItem",MGeometry::kTexture,1,"numericvalue"},
            {"DPMarkVecItem",MGeometry::kPosition,3},{"DPMarkVecItem",MGeometry::kTexture,3},
            {pv,MGeometry::kPosition,3},{pv,MGeometry::kTexture,dimension,semantic},
            {"DPMarkGroupPointItem",MGeometry::kPosition,3}});
        if (mode==0 || mode==1 || mode==3) pr.list.values.emplace_back(pv,MGeometry::kTexture,1,"numericvalue");
        gpuFailures(pm,pr,items({"DPMarkColItem","DPMarkIndexItem","DPMarkVecItem",pv,"DPMarkGroupPointItem"}));
        DCMarkerOverride cm; auto& c=cm.dData;
        c.isColor=c.showIndex=c.isVector=c.isValue=c.isGroup=c.isCenterVis=true;
        c.pointCount=3; c.primCount=1; c.cvsBuffer=d.mPointBuffer; c.colBuffer=d.colBuffer;
        c.indexBuffer={0,1,1,2}; c.crvFirstIndex={0}; c.centerPtBuffer={1,0,0};
        c.indexType=c.infoClass=primitive?0:1;
        c.vecBuffer=std::vector<float>(primitive?3:9,1);
        c.val3Buffer=std::vector<float>((primitive?1:3)*dimension,1); c.valType=p.valType;
        c.grpPtBuffer=p.grpPtBuffer; c.grpIndexBuffer={0,1}; c.totalGroupPointCount=2;
        const char* cv=mode==1?"DCMarkVal2Item":(mode==2?"DCMarktVal1Item":"DCMarkVal3Item");
        auto cr=requirements({{"DCMarkColorItem",MGeometry::kPosition,3},
            {"DCMarkColorItem",MGeometry::kColor,4},{"DCMarkCenterItem",MGeometry::kPosition,3},
            {"DCMarkIndexItem",MGeometry::kPosition,3},{"DCMarkIndexItem",MGeometry::kTexture,1,"numericvalue"},
            {"DCMarkVecItem",MGeometry::kPosition,3},{"DCMarkVecItem",MGeometry::kTexture,3},
            {cv,MGeometry::kPosition,3},{cv,MGeometry::kTexture,dimension,semantic},
            {"DCMarkGroupLineItem",MGeometry::kPosition,3}});
        if (mode==0 || mode==1 || mode==3) cr.list.values.emplace_back(cv,MGeometry::kTexture,1,"numericvalue");
        gpuFailures(cm,cr,items({"DCMarkColorItem","DCMarkCenterItem","DCMarkIndexItem", "DCMarkVecItem",cv,"DCMarkGroupLineItem"}));
    }
    DOpenVDBDisplayVP2Override vdb;
    BufferObjectVP2 surface; surface.points={MPoint(0,0,0),MPoint(1,0,0),MPoint(0,1,0)};
    surface.normals=std::vector<MPoint>(3,MPoint(0,0,1)); surface.indexArray={0,1,2};
    vdb.mSurfaceBuffersVP2={surface};
    valueBufferObjectVP2 voxels; voxels.points={0,0,0,1,0,0};
    voxels.normals=voxels.colors=voxels.points; voxels.indexArray={0,1};
    vdb.mPointBuffersVP2=vdb.mVectorBuffersVP2={voxels};
    lineBufferObjectVP2 line; line.points={0,0,0,1,0,0}; line.indexArray={0,1};
    vdb.mBBoxBuffersVP2=vdb.mNodeBuffersVP2=vdb.mLeafBuffersVP2=vdb.mTileBuffersVP2={line};
    gpuFailures(vdb,requirements({{"volumeSurface",MGeometry::kPosition,3},
        {"volumeSurface",MGeometry::kNormal,3},{"volumeVoxel",MGeometry::kPosition,3},
        {"volumeVoxel",MGeometry::kNormal,3},{"volumeVoxel",MGeometry::kColor,4},
        {"DVDBVecItem",MGeometry::kPosition,3},{"DVDBVecItem",MGeometry::kTexture,3},
        {"bboxShader",MGeometry::kPosition,3},{"inNodeShader",MGeometry::kPosition,3},
        {"leafShader",MGeometry::kPosition,3},{"tileShader",MGeometry::kPosition,3}}),
        items({"volumeSurface","volumeVoxel","DVDBVecItem","bboxShader","inNodeShader","leafShader","tileShader"}));
    // Even with staged VDB geometry and render items, no requirements means
    // no GPU packing or index publication (the historical outer count gate).
    reset(); MGeometry noStreams;
    vdb.populateGeometry(requirements({}),items({"volumeSurface"}),noStreams);
    assert(!vdb.isDirty() && operations.empty() && noStreams.vertices.empty() && noStreams.indices.empty());
}

template <typename Node> void emptyGpuTest() {
    Node node; node.setDirty(false); reset(); MHWRender::MGeometry geometry;
    node.populateGeometry(requirements({}),items({}),geometry);
    assert(!node.isDirty() && operations.empty());
}

void pointUpdateTests() {
    DPData data; data.count=13; data.points=std::vector<MVector>(13,MVector(1,2,3));
    inputPointData=&data;
    DPointDisplayOverride node;
    tbb::pointStagingObserver=[&] {
        assert(node.dData.pointCount==0 && node.dData.ptBuffer.empty());
    };
    const auto update=[&] {
        node.dData.ptBuffer={9,9,9}; node.dData.pointCount=1; node.setDirty(false);
        node.updateDG(); assert(node.isDirty());
    };
    reset(); update(); assert(node.dData.pointCount==13 && node.dData.ptBuffer.size()==39);
    const auto trace=operations;
    for (std::size_t i=0;i<trace.size();++i) for (int kind : {1,2,3}) {
        reset(); failAt=static_cast<int>(i); failureKind=kind; update();
        if (kind==1 && (trace[i]=="point.color" || trace[i]=="point.size"))
            assert(node.dData.pointCount==13); // A safe display-setting fallback.
        else assert(node.dData.pointCount==0 && node.dData.ptBuffer.empty());
        reset(); update(); assert(node.dData.pointCount==13);
    }
    const auto connection=std::find(trace.begin(),trace.end(),"plug.connected");
    assert(connection!=trace.end() && std::count(trace.begin(),trace.end(),"plug.connected")==1);
    for (bool connectionValue : {false,true}) {
        reset(); pointConnected=connectionValue;
        failAt=int(connection-trace.begin()); failureKind=1; update();
        assert(node.dData.pointCount==0U && node.dData.ptBuffer.empty());
        assert(operations.back()=="plug.connected" && apiErrorReports==1 && diagnosticReports==0);
        reset(); pointConnected=true; update(); assert(node.dData.pointCount==13U);
    }
    reset(); failAt=int(connection-trace.begin()); failureKind=1; failDiagnosticOutput=true;
    update(); failDiagnosticOutput=false;
    assert(node.dData.pointCount==0U && node.dData.ptBuffer.empty() && diagnosticReports==1);
    reset(); update(); assert(node.dData.pointCount==13U);
    reset(); failAllocationBytes=13U*3U*sizeof(float); update();
    assert(failAllocationBytes==0 && node.dData.pointCount==0 && node.dData.ptBuffer.empty());
    reset(); update(); assert(node.dData.pointCount==13);
    data.count=dommy::viewport::kMaxViewportVertexCount+1U;
    reset(); update(); assert(node.dData.pointCount==0 && node.dData.ptBuffer.empty());
    assert(std::find(operations.begin(),operations.end(),"point.positions")==operations.end());
    data.count=13; data.nullPositions=true; reset(); update(); assert(node.dData.pointCount==0);
    data.nullPositions=false; data.points.pop_back(); reset(); update(); assert(node.dData.pointCount==0);
    data.points.emplace_back(1,2,3);
    for (const double bad : {std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),
         double(std::numeric_limits<float>::max())*2}) {
        data.points[0][0]=bad; reset(); update(); assert(node.dData.pointCount==0 && node.dData.ptBuffer.empty());
    }
    data.points[0][0]=1;
    inputPointData=nullptr; reset(); update(); assert(node.dData.pointCount==0);
    inputPointData=&data; pointConnected=false; reset(); update(); assert(node.dData.pointCount==0);
    pointConnected=true;
    for (const double bad : {-1.0,std::numeric_limits<double>::infinity(),
         std::numeric_limits<double>::quiet_NaN(),double(std::numeric_limits<float>::max())*2}) {
        inputPointSize=bad; reset(); update(); assert(node.dData.pointSize==3 && node.dData.pointCount==13);
    }
    inputPointSize=3; inputPointColor.r=std::numeric_limits<float>::infinity();
    reset(); update(); assert(node.dData.solidColor.r==1 && node.dData.pointCount==13);
    inputPointColor=MColor(1,1,1,1);
    data.count=0; data.points.clear(); reset(); update(); assert(node.dData.pointCount==0 && node.dData.ptBuffer.empty());
    emptyGpuTest<DPointDisplayOverride>(); emptyGpuTest<DPMarkerOverride>();
    emptyGpuTest<DCurveDisplayOverride>(); emptyGpuTest<DCMarkerOverride>();
    emptyGpuTest<DOpenVDBDisplayVP2Override>();
    tbb::pointStagingObserver={};
    inputPointData=nullptr;
}

template <typename Node> void renderItemTests(int expectedItems) {
    using MHWRender::pendingRenderItems;
    Node node;
    MHWRender::MShaderInstance shader; node.setAllShaders(&shader);
    std::vector<std::string> trace;
    reset(); {
        MHWRender::MRenderItemList list;
        node.updateRenderItems(MDagPath(),list);
        assert(list.length()==expectedItems && pendingRenderItems==expectedItems);
        trace=operations;
        reset(); node.updateRenderItems(MDagPath(),list);
        assert(list.length()==expectedItems && pendingRenderItems==expectedItems);
    }
    assert(pendingRenderItems==0);
    for (std::size_t i=0;i<trace.size();++i) for (int kind : {1,2,3}) {
        reset(); failAt=static_cast<int>(i); failureKind=kind;
        {
            MHWRender::MRenderItemList list;
            node.updateRenderItems(MDagPath(),list);
            assert(pendingRenderItems==list.length());
            assert(node.mShaderUpdateFailed);
            reset(); node.updateRenderItems(MDagPath(),list);
            assert(list.length()==expectedItems && pendingRenderItems==expectedItems);
        }
        assert(pendingRenderItems==0);
    }
    // Item registration and ownership do not depend on optional shader setup.
    node.setAllShaders(nullptr);
    reset(); {
        MHWRender::MRenderItemList list;
        node.updateRenderItems(MDagPath(),list);
        assert(list.length()==expectedItems && pendingRenderItems==expectedItems);
    }
    assert(pendingRenderItems==0);
}
void pointRenderItemTests() {
    renderItemTests<DPointDisplayOverride>(1);
    renderItemTests<DPMarkerOverride>(7);
    renderItemTests<DCurveDisplayOverride>(2);
    renderItemTests<DCMarkerOverride>(9);
    renderItemTests<DMeshDisplayOverride>(5);
    renderItemTests<DMMarkerOverride>(9);
    renderItemTests<DPInstancerOverride>(3);
    renderItemTests<DOpenVDBDisplayVP2Override>(7);
}

template <typename Data> void prepareMarkerShaderData(Data& data, int mode) {
    data.pointCount=1U;
    data.showIndex=data.isColor=data.isVector=data.isValue=data.isGroup=true;
    data.totalGroupPointCount=1U;
    data.colBuffer={1,1,1,1};
    data.grpPtBuffer={0,0,0};
    const int valueTypes[]={DP_VAL_FLOAT,DP_VAL_INT,DP_VAL_FLOAT2,DP_VAL_INT2,DP_VAL_FLOAT3,DP_VAL_INT3};
    const int groupTypes[]={D_COMP_PRIM,D_COMP_POINT,D_COMP_EDGE,D_COMP_FACE};
    data.valType=valueTypes[mode%6]; data.grpType=groupTypes[mode%4];
}
void prepareShaderData(DPointDisplayOverride& node,int) {node.dData.pointCount=1U;}
void prepareShaderData(DPMarkerOverride& node,int mode) {prepareMarkerShaderData(node.dData,mode);}
void prepareShaderData(DCMarkerOverride& node,int mode) {
    prepareMarkerShaderData(node.dData,mode);node.dData.primCount=1U;node.dData.isCenterVis=true;
}
void prepareShaderData(DMMarkerOverride& node,int mode) {
    prepareMarkerShaderData(node.dData,mode);node.dData.primCount=1U;node.dData.grpIndexBuffer={0};
}
void prepareShaderData(DCurveDisplayOverride& node,int mode) {
    node.dData.primCount=1U;node.dData.colorType=static_cast<short>(mode%2);node.dData.colBuffer={1,1,1,1};
}
void prepareShaderData(DMeshDisplayOverride& node,int mode) {
    node.dData.primCount=1U;node.dData.isShade=node.dData.isVertex=node.dData.isWireframe=true;
    node.dData.displayType=mode%2 ? DM_DPY_FLAT : DM_DPY_SMOOTH;
    node.dData.colorType=static_cast<short>(mode%3==2 ? DM_DISPLAY_COLOR_RANDOM : 0);
    node.dData.colBuffer={1,1,1,1};
}
void prepareShaderData(DPInstancerOverride& node,int mode) {
    const int types[]={DP_INST_SHADE,DP_INST_BBOX,DP_INST_WIRE,DP_INST_POINT};
    node.dData.displayType=types[mode%4];
    node.dData.pointList={0,0,0};
}
void prepareShaderData(DOpenVDBDisplayVP2Override& node,int) {
    node.visBBox=node.visNode=node.visLeaf=node.visTile=node.visSurf=node.visPoint=node.visVector=true;
    node.mPointBuffersVP2.resize(1);node.mPointBuffersVP2[0].indexArray={0};
    node.mVectorBuffersVP2.resize(1);node.mVectorBuffersVP2[0].indexArray={0};
}
template <typename Node> void shaderStateTests(int expectedItems) {
    Node node;MHWRender::MShaderInstance shader;node.setAllShaders(&shader);
    std::vector<bool> covered(static_cast<std::size_t>(expectedItems),false);
    reset();MHWRender::MRenderItemList list;node.updateRenderItems(MDagPath(),list);
    for(int mode=0;mode<12;++mode) {
        prepareShaderData(node,mode);reset();node.updateRenderItems(MDagPath(),list);
        assert(list.length()==expectedItems && !node.mShaderUpdateFailed);
        const auto trace=operations;
        std::vector<bool> visible;
        for(int i=0;i<list.length();++i) {
            visible.push_back(list.itemAt(i)->enabled);
            covered[static_cast<std::size_t>(i)]=covered[static_cast<std::size_t>(i)] || visible.back();
        }
        for(std::size_t i=0;i<trace.size();++i) {
            if(trace[i]!="render.parameter" && trace[i]!="render.shader") continue;
            for(int kind : {1,2,3}) {
                reset();failAt=static_cast<int>(i);failureKind=kind;
                node.updateRenderItems(MDagPath(),list);
                assert(failedShaderItem && !static_cast<MHWRender::MRenderItem*>(failedShaderItem)->enabled);
                assert(node.mShaderUpdateFailed);
                node.setDirty(false);assert(node.isDirty()); // A geometry success must not erase shader failure.
                reset();node.updateRenderItems(MDagPath(),list);
                assert(!node.mShaderUpdateFailed && list.length()==expectedItems);
                for(int j=0;j<list.length();++j) assert(list.itemAt(j)->enabled==visible[static_cast<std::size_t>(j)]);
            }
        }
        node.setAllShaders(nullptr);reset();MHWRender::stockShaderAvailable=false;
        node.updateRenderItems(MDagPath(),list);
        for(int i=0;i<list.length();++i) assert(!list.itemAt(i)->enabled);
        assert(node.mShaderUpdateFailed);
        MHWRender::stockShaderAvailable=true;
        node.setAllShaders(&shader);reset();node.updateRenderItems(MDagPath(),list);
        assert(!node.mShaderUpdateFailed);
    }
    for(bool enabled : covered) assert(enabled); // Every one of the 43 item paths was active at least once.
}
void shaderTests() {
    shaderStateTests<DPointDisplayOverride>(1);shaderStateTests<DPMarkerOverride>(7);
    shaderStateTests<DCurveDisplayOverride>(2);shaderStateTests<DCMarkerOverride>(9);
    shaderStateTests<DMeshDisplayOverride>(5);shaderStateTests<DMMarkerOverride>(9);
    shaderStateTests<DPInstancerOverride>(3);shaderStateTests<DOpenVDBDisplayVP2Override>(7);
    for(float value : {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        reset();inputPointColor=MColor(value,0,0,1);MColor color;curveSolidColor(color);
        assert(color.r==1 && color.g==1 && color.b==1 && color.a==1);
    }
    reset();inputPointColor=MColor(.2f,.3f,.4f,1);MColor color;curveSolidColor(color);
    assert(color.r==.2f && color.g==.3f && color.b==.4f);
    reset();failAt=0;curveSolidColor(color);assert(color.r==1 && color.g==1 && color.b==1);
    reset();inputPointColor=MColor(1,1,1,1);
}

template <typename Node> void assertAcquiredShaders(const Node& node) {
    const auto pointers=node.shaderPointers(); const auto types=node.shaderTypes();
    assert(pointers.size()==types.size());
    for (std::size_t i=0;i<pointers.size();++i)
        assert(pointers[i] && pointers[i]->stockType==types[i]);
}
void resetShaderAcquisition() {
    reset();
    MHWRender::rendererAvailable=MHWRender::shaderManagerAvailable=MHWRender::stockShaderAvailable=true;
    MHWRender::failedStockType=-1; MHWRender::stockShaderRequests.clear();
    MHWRender::shaderReleaseRequests.clear(); MHWRender::releaseThrowsAfterCompletion=false;
}
template <typename Node> void stockShaderRecoveryTests() {
    using namespace MHWRender;
    for (int missing=0;missing<3;++missing) {
        Node node; prepareShaderData(node,0); MRenderItemList list;
        resetShaderAcquisition();
        if (missing==0) rendererAvailable=false;
        if (missing==1) shaderManagerAvailable=false;
        if (missing==2) stockShaderAvailable=false;
        node.updateRenderItems(MDagPath(),list);
        assert(node.mShaderUpdateFailed);
        for (auto* shader : node.shaderPointers()) assert(!shader);
        for (int i=0;i<list.length();++i) assert(!list.itemAt(i)->enabled);
        resetShaderAcquisition(); node.updateRenderItems(MDagPath(),list);
        assertAcquiredShaders(node); assert(!node.mShaderUpdateFailed);
        assert(stockShaderRequests.size()==node.shaderPointers().size());
        const auto retained=node.shaderPointers();
        resetShaderAcquisition(); node.updateRenderItems(MDagPath(),list);
        assert(node.shaderPointers()==retained && stockShaderRequests.empty());
        assert(std::find(operations.begin(),operations.end(),"shader.renderer")==operations.end());
    }
    Node baseline; prepareShaderData(baseline,0); MRenderItemList initial;
    resetShaderAcquisition(); baseline.updateRenderItems(MDagPath(),initial);
    const auto trace=operations;
    for (std::size_t call=0;call<trace.size();++call) {
        if (trace[call].find("shader.")!=0) continue;
        for (int kind : {1,2,3}) {
            Node node; prepareShaderData(node,0); MRenderItemList list;
            resetShaderAcquisition(); failAt=int(call); failureKind=kind;
            node.updateRenderItems(MDagPath(),list);
            assert(node.mShaderUpdateFailed && diagnosticReports==(kind==1?0:1));
            const auto retained=node.shaderPointers();
            resetShaderAcquisition(); node.updateRenderItems(MDagPath(),list);
            assertAcquiredShaders(node); assert(!node.mShaderUpdateFailed);
            const auto corrected=node.shaderPointers();
            for (std::size_t i=0;i<retained.size();++i)
                if (retained[i]) assert(retained[i]==corrected[i]);
        }
    }
    for (int type : baseline.shaderTypes()) {
        Node node; prepareShaderData(node,0); MRenderItemList list;
        resetShaderAcquisition(); failedStockType=type; node.updateRenderItems(MDagPath(),list);
        assert(node.mShaderUpdateFailed);
        const auto retained=node.shaderPointers(); const auto types=node.shaderTypes();
        std::size_t missing=0;
        for (std::size_t i=0;i<types.size();++i) {
            assert(bool(retained[i])==(types[i]!=type));
            if (types[i]==type) ++missing;
        }
        resetShaderAcquisition(); node.updateRenderItems(MDagPath(),list);
        assertAcquiredShaders(node); assert(stockShaderRequests.size()==missing);
        for (std::size_t i=0;i<retained.size();++i)
            if (retained[i]) assert(retained[i]==node.shaderPointers()[i]);
    }
    resetShaderAcquisition();
}
void stockShaderTests() {
    using namespace MHWRender;
    for (int missing=0;missing<3;++missing) {
        resetShaderAcquisition();
        if (missing==0) rendererAvailable=false;
        if (missing==1) shaderManagerAvailable=false;
        if (missing==2) stockShaderAvailable=false;
        bool retry=false;
        MShaderInstance* first=nullptr; MShaderInstance* second=nullptr;
        dommy::viewport::StockShaderAcquisition acquisition(retry,"fixture");
        acquisition.acquire(MShaderManager::k3dFatPointShader,first);
        acquisition.acquire(MShaderManager::k3dThickLineShader,second);
        assert(retry && !first && !second);
        assert(std::count(operations.begin(),operations.end(),"shader.renderer")==1);
        assert(std::count(operations.begin(),operations.end(),"shader.manager")==
               (missing==0?0:1));
    }
    stockShaderRecoveryTests<DPointDisplayOverride>();stockShaderRecoveryTests<DPMarkerOverride>();
    stockShaderRecoveryTests<DCurveDisplayOverride>();stockShaderRecoveryTests<DCMarkerOverride>();
    stockShaderRecoveryTests<DMeshDisplayOverride>();stockShaderRecoveryTests<DMMarkerOverride>();
    stockShaderRecoveryTests<DPInstancerOverride>();stockShaderRecoveryTests<DOpenVDBDisplayVP2Override>();
}
template <typename Run> MStatus guarded(Run run) { try { return run(); } catch (...) { return MS::kFailure; } }
template <typename Run> void traversalFailures(Run run) {
    reset(); assert(guarded(run)==MS::kSuccess); const auto trace=operations;
    for (std::size_t i=0;i<trace.size();++i) for(int kind : {1,2,3}) {
        reset();failAt=static_cast<int>(i);failureKind=kind;assert(!guarded(run));
    }
    reset();stuck=true;assert(!guarded(run));stuck=false;
    reset();early=true;assert(!guarded(run));early=false;
}
void traversalTests() {
    auto run0=[] { DMeshDisplayOverride::DrawData d{}; return traversal0(d,3,true); };
    auto run1=[] { DMeshDisplayOverride::DrawData d{}; d.displayType=DM_DPY_SMOOTH;d.normBuffer.resize(27);return traversal1(d,3,true); };
    auto run2=[] { DMMarkerOverride::DrawData d{};return traversal2(d,3,true); };
    auto run3=[] { DMMarkerOverride::DrawData d{};return traversal3(d,3,true); };
    traversalFailures(run0);traversalFailures(run1);traversalFailures(run2);traversalFailures(run3);
    traversalFailures([] { DMMarkerOverride::DrawData d{};return traversal2(d,3,false); });
    traversalFailures([] { DMMarkerOverride::DrawData d{};return traversal3(d,3,false); });
    reset();invalidIndex=true;assert(!run0());assert(!run1());assert(!run2());assert(!run3());invalidIndex=false;
    degenerate=true;
    traversalFailures([] { DMeshDisplayOverride::DrawData d{};d.displayType=DM_DPY_HARD;return traversal1(d,3,true); });
    degenerate=false;
}
void instanceTests() {
    MPointArray source;source.values={MPoint(0,0,0)};
    MIntArray connections;connections.values={0};
    MFloatVectorArray sourceNormals;sourceNormals.values={MFloatVector(float(1/std::sqrt(2.0)),float(1/std::sqrt(2.0)),0)};
    MMatrix scale;scale[0][0]=2;
    std::vector<float> points{91},normals{91};std::vector<unsigned int> indices{91};
    assert(buildInstanceBuffers(source,connections,&sourceNormals,{scale},points,indices,normals));
    assert(std::fabs(normals[0]*2-normals[1])<1e-6);
    assert(std::fabs(std::sqrt(normals[0]*normals[0]+normals[1]*normals[1])-1)<1e-6);
    MMatrix shear; shear[0][1]=1;
    assert(buildInstanceBuffers(source,connections,&sourceNormals,{shear},points,indices,normals));
    assert(std::fabs(normals[0])<1e-6 && std::fabs(normals[1]-1)<1e-6);
    scale[0][0]=-2;
    assert(buildInstanceBuffers(source,connections,&sourceNormals,{scale},points,indices,normals));
    assert(normals[0]<0 && normals[1]>0 && std::fabs(-2*normals[0]-normals[1])<1e-6);
    for (int mode=0;mode<5;++mode) {
        MMatrix bad=scale;
        if(mode==0)bad[0][0]=0;
        if(mode==1)bad[0][0]=std::numeric_limits<double>::infinity();
        if(mode==2)bad[3][0]=std::numeric_limits<double>::max();
        if(mode==3)bad[0][3]=1;
        auto wrong=connections;if(mode==4)wrong.values={1};
        points={91};normals={91};indices={91};
        assert(!buildInstanceBuffers(source,wrong,&sourceNormals,{bad},points,indices,normals));
        assert(points==std::vector<float>{91} && normals==std::vector<float>{91} && indices==std::vector<unsigned int>{91});
    }
    points={91};normals={91};indices={91};
    assert(!resizeInstanceBuffers(8,24,250001,false,points,indices,normals));
    assert(points==std::vector<float>{91});
    std::size_t p=91,s=91,i=91;
    assert(!dommy::viewport::checkedInstanceLayout(SIZE_MAX,2,2,p,s,i));
    assert(p==91 && s==91 && i==91);
    assert(dommy::viewport::checkedInstanceLayout(8,24,250000,p,s,i));
    assert(p==2000000 && i==6000000);
    sourceNormals.values.push_back(MFloatVector(0,0,1));
    assert(!buildInstanceBuffers(source,connections,&sourceNormals,{scale},points,indices,normals));
    sourceNormals.values.resize(1);
    sourceNormals.values[0]=MFloatVector();
    assert(!buildInstanceBuffers(source,connections,&sourceNormals,{scale},points,indices,normals));
    source.values[0].x=std::numeric_limits<double>::quiet_NaN();
    assert(!buildInstanceBuffers(source,connections,nullptr,{scale},points,indices,normals));
    source.values[0].x=0;
    assert(buildInstanceBuffers(source,connections,nullptr,{scale,scale},points,indices,normals));
    assert(normals.empty() && indices==std::vector<unsigned int>({0,1}) && points.size()==6U);
}
void bboxTests() {
    BoundsPointList vertices={DPoint(0,0,0),DPoint(0,0,0),DPoint(3,6,9)};
    MBoundingBox bounds; DPoint mean;
    auto onlyBounds=[&]() {
        ObservedMeanAccumulator::additions=0U;
        const MBoundingBox fromData=calculatePointBounds(vertices);
        MBoundingBox direct;
        CalBBoxFromPointArray(vertices,direct);
        assert(ObservedMeanAccumulator::additions==0U);
        for(std::size_t axis=0U;axis<3U;++axis) {
            assert(fromData.min()[axis]==bounds.min()[axis] && fromData.max()[axis]==bounds.max()[axis]);
            assert(direct.min()[axis]==bounds.min()[axis] && direct.max()[axis]==bounds.max()[axis]);
        }
    };
    CalBBoxFromPointArray(vertices,bounds,mean);
    assert(mean.x==1 && mean.y==2 && mean.z==3); // Not the box midpoint.
    assert(bounds.min().x==0 && bounds.max().x==3);
    onlyBounds();
    const double large=double(std::numeric_limits<float>::max());
    vertices={DPoint(large,large,large),DPoint(large,large,large),DPoint(5,10,15),
              DPoint(-large,-large,-large),DPoint(-large,-large,-large)};
    std::feclearexcept(FE_ALL_EXCEPT);
    CalBBoxFromPointArray(vertices,bounds,mean);
    assert(mean.x==1 && mean.y==2 && mean.z==3);
    assert(bounds.min().x==-large && bounds.max().x==large);
    assert(std::fetestexcept(FE_OVERFLOW|FE_INVALID)==0);
    onlyBounds();
    vertices.push_back(DPoint(std::numeric_limits<float>::quiet_NaN(),1,2));
    CalBBoxFromPointArray(vertices,bounds,mean);
    assert(mean.x==1 && mean.y==2 && mean.z==3 && bounds.max().x==large);
    onlyBounds();
    vertices={DPoint(std::numeric_limits<float>::infinity(),0,0)};
    CalBBoxFromPointArray(vertices,bounds,mean);
    assert(mean.x==0 && mean.y==0 && mean.z==0);
    onlyBounds();
    vertices.clear(); CalBBoxFromPointArray(vertices,bounds,mean);
    assert(mean.x==0 && mean.y==0 && mean.z==0);
    onlyBounds();
    reset();MPointArray points;MIntArray edges;
    assert(DGenerateDisplayBBoxDataFromMinMaxPoint(MPoint(-1,-1,-1),MPoint(1,1,1),points,edges)==MS::kSuccess);
    assert(points.length()==8 && edges.length()==24);
    const auto trace=operations;
    for(std::size_t i=0;i<trace.size();++i) {
        reset();failAt=static_cast<int>(i);
        assert(!DGenerateDisplayBBoxDataFromMinMaxPoint(MPoint(-1,-1,-1),MPoint(1,1,1),points,edges));
    }
    for(int i : {0,1}) { reset();shortAt=i;assert(!DGenerateDisplayBBoxDataFromMinMaxPoint(MPoint(-1,-1,-1),MPoint(1,1,1),points,edges)); }
}
void displayNormalTests() {
    const MVector source(0,0,1);
    MVector output(91,92,93);
    std::feclearexcept(FE_ALL_EXCEPT);
    for(double scale : {1e20,1e-40,1e-300,1e300}) {
        MMatrix transform;
        transform[0][0]=transform[1][1]=transform[2][2]=scale;
        transform[3][0]=1e300;
        assert(transformDisplayNormal(source,transform,output));
        assert(output.x==0 && output.y==0 && output.z==1);
    }
    MMatrix transform;
    transform[0][0]=transform[1][1]=1e-100;
    assert(transformDisplayNormal(source,transform,output) && output.z==1);
    transform[2][2]=-1;
    assert(transformDisplayNormal(source,transform,output) && output.z==-1);
    transform=MMatrix(); transform[0][0]=2; transform[1][1]=4;
    assert(transformDisplayNormal(MVector(1,1,0),transform,output));
    assert(std::fabs(output.x-2/std::sqrt(5.0))<1e-14 && std::fabs(output.y-1/std::sqrt(5.0))<1e-14);
    output=MVector(91,92,93);
    assert(!transformDisplayNormal(MVector(0,0,0),transform,output));
    assert(output.x==91 && output.y==92 && output.z==93);
    transform[0][0]=0;
    assert(!transformDisplayNormal(source,transform,output));
    assert(output.x==91 && output.y==92 && output.z==93);
    transform=MMatrix();
    assert(!transformDisplayNormal(MVector(NAN,0,1),transform,output));
    assert(output.x==91 && output.y==92 && output.z==93);
    assert(std::fetestexcept(FE_OVERFLOW|FE_INVALID)==0);
}
void shaderDiagnosticTests() {
    reset(); reportInstancerShaderParameters(nullptr);
    assert(operations.empty() && debugMessages.empty() && diagnosticReports==0);
    MHWRender::MShaderInstance shader;
    reset(); reportInstancerShaderParameters(&shader);
#if DOMMY_ENABLE_DEBUG_LOGS
    assert(debugMessages.size()==56U && diagnosticReports==0);
    assert(debugMessages[1]=="\ttype:kInvalid" && debugMessages[53]=="\ttype:kSampler");
    const auto trace=operations;
    for (std::size_t i=0;i<trace.size();++i) for (int kind : {1,2,3}) {
        reset(); failAt=int(i); failureKind=kind;
        reportInstancerShaderParameters(&shader);
        assert(diagnosticReports==1);
        reset(); reportInstancerShaderParameters(&shader);
        assert(debugMessages.size()==56U && diagnosticReports==0);
    }
    for (int type : {-1,14,std::numeric_limits<int>::max()}) {
        shader.parameterTypes={type}; reset(); reportInstancerShaderParameters(&shader);
        assert(debugMessages.size()==4U && debugMessages[1]=="\ttype:unknown");
    }
#else
    assert(operations.empty() && debugMessages.empty() && diagnosticReports==0);
#endif
}
template <typename Data> void settingsFailureTests(void (*read)(Data&),std::initializer_list<MObject> required) {
    Data data;reset();settingValues.clear();reachedSettingsData=false;read(data);
    assert(reachedSettingsData && data.pointCount==0U);
    const auto trace=operations;
    for(const MObject& attribute:required) {
        const std::string operation="setting."+std::to_string(attribute.key);
        auto found=std::find(trace.begin(),trace.end(),operation);assert(found!=trace.end());
        for(int kind:{1,2,3}) {
            reset();reachedSettingsData=false;failAt=int(found-trace.begin());failureKind=kind;
            read(data);assert(!reachedSettingsData && data.pointCount==0U);
            reset();reachedSettingsData=false;read(data);assert(reachedSettingsData);
        }
    }
}

void seedAdmissionGeometry(DPMarkerOverride::DrawData& d) {
    d.pointCount=d.totalGroupPointCount=1U;
    d.ptBuffer=d.colBuffer=d.vecBuffer=d.val3Buffer=d.grpPtBuffer={9};
}
void seedAdmissionGeometry(DCMarkerOverride::DrawData& d) {
    d.pointCount=d.primCount=d.totalGroupPointCount=1U;
    d.centerPtBuffer=d.cvsBuffer=d.colBuffer=d.vecBuffer=d.val3Buffer=d.grpPtBuffer={9};
    d.indexBuffer=d.grpIndexBuffer={0}; d.crvFirstIndex={0};
}
void seedAdmissionGeometry(DMMarkerOverride::DrawData& d) {
    d.pointCount=d.primCount=d.totalGroupPointCount=1U;
    d.mPointBuffer=d.colBuffer=d.vecBuffer=d.val3Buffer=d.grpPtBuffer={9};
    d.indexBuffer=d.grpIndexBuffer={0}; d.meshFirstIndex={0};
}
void seedAdmissionGeometry(DCurveDisplayOverride::DrawData& d) {
    d.pointCount=d.primCount=1U;
    d.cvsBuffer=d.colBuffer={9}; d.indexBuffer={0};
}
void seedAdmissionGeometry(DMeshDisplayOverride::DrawData& d) {
    d.pointCount=d.primCount=1U;
    d.mPointBuffer=d.colBuffer=d.normBuffer={9};
    d.mFaceIndexList=d.mWireIndexList={0};
}

bool emptyAdmissionGeometry(const DPMarkerOverride::DrawData& d) {
    return d.pointCount==0U && d.totalGroupPointCount==0U && d.ptBuffer.empty() &&
        d.colBuffer.empty() && d.vecBuffer.empty() && d.val3Buffer.empty() && d.grpPtBuffer.empty();
}
bool emptyAdmissionGeometry(const DCMarkerOverride::DrawData& d) {
    return d.pointCount==0U && d.primCount==0U && d.totalGroupPointCount==0U &&
        d.centerPtBuffer.empty() && d.cvsBuffer.empty() && d.indexBuffer.empty() &&
        d.crvFirstIndex.empty() && d.colBuffer.empty() && d.vecBuffer.empty() &&
        d.val3Buffer.empty() && d.grpPtBuffer.empty() && d.grpIndexBuffer.empty();
}
bool emptyAdmissionGeometry(const DMMarkerOverride::DrawData& d) {
    return d.pointCount==0U && d.primCount==0U && d.totalGroupPointCount==0U &&
        d.mPointBuffer.empty() && d.indexBuffer.empty() && d.meshFirstIndex.empty() &&
        d.colBuffer.empty() && d.vecBuffer.empty() && d.val3Buffer.empty() &&
        d.grpPtBuffer.empty() && d.grpIndexBuffer.empty();
}
bool emptyAdmissionGeometry(const DCurveDisplayOverride::DrawData& d) {
    return d.pointCount==0U && d.primCount==0U && d.cvsBuffer.empty() &&
        d.indexBuffer.empty() && d.colBuffer.empty();
}
bool emptyAdmissionGeometry(const DMeshDisplayOverride::DrawData& d) {
    return d.pointCount==0U && d.primCount==0U && d.mPointBuffer.empty() &&
        d.mFaceIndexList.empty() && d.mWireIndexList.empty() &&
        d.colBuffer.empty() && d.normBuffer.empty();
}

template <typename Node>
void connectionAdmissionTests(void (*read)(typename Node::DrawData&)) {
    Node node;
    reset(); settingValues.clear(); pointConnected=true; reachedSettingsData=false;
    prepareShaderData(node,0); seedAdmissionGeometry(node.dData); read(node.dData);
    assert(reachedSettingsData && emptyAdmissionGeometry(node.dData));
    const auto trace=operations;
    const auto query=std::find(trace.begin(),trace.end(),"plug.connected");
    assert(query!=trace.end() && std::count(trace.begin(),trace.end(),"plug.connected")==1);
    const int queryStep=int(query-trace.begin());
    for (bool connectionValue : {false,true}) for (int kind : {1,2,3}) {
        reset(); pointConnected=connectionValue; reachedSettingsData=false;
        prepareShaderData(node,0); seedAdmissionGeometry(node.dData); failAt=queryStep; failureKind=kind;
        read(node.dData);
        assert(!reachedSettingsData && emptyAdmissionGeometry(node.dData));
        assert(operations.back()=="plug.connected");
        if (kind==1) assert(apiErrorReports==1 && diagnosticReports==0);
        else assert(diagnosticReports==1);
        MHWRender::MRenderItemList list;
        node.updateRenderItems(MDagPath(),list);
        for (int i=0;i<list.length();++i) assert(!list.itemAt(i)->enabled);
        reset(); pointConnected=true; reachedSettingsData=false;
        read(node.dData);
        assert(reachedSettingsData && apiErrorReports==0 && diagnosticReports==0);
    }
    reset(); pointConnected=true; reachedSettingsData=false;
    prepareShaderData(node,0); seedAdmissionGeometry(node.dData); failAt=queryStep; failureKind=1; failDiagnosticOutput=true;
    read(node.dData); failDiagnosticOutput=false;
    assert(!reachedSettingsData && emptyAdmissionGeometry(node.dData) && diagnosticReports==1);
    reset(); pointConnected=false; reachedSettingsData=false;
    prepareShaderData(node,0); seedAdmissionGeometry(node.dData); read(node.dData);
    assert(!reachedSettingsData && emptyAdmissionGeometry(node.dData));
    assert(apiErrorReports==0 && diagnosticReports==0 && operations.back()=="plug.connected");
    pointConnected=true;
}
void drawingDefaultsTests() {
    DPointDisplayOverride point;DPMarkerOverride pointMarker;DCurveDisplayOverride curve;DCMarkerOverride curveMarker;
    DMeshDisplayOverride mesh;DMMarkerOverride meshMarker;DPInstancerOverride instancer;
    assert(point.dData.pointCount==0U && point.dData.pointSize==3.0f && point.dData.solidColor.r==1.0f);
    assert(curve.dData.primCount==0U && curve.dData.pointCount==0U && curve.dData.pointSize==3.0f && curve.dData.lineWidth==2.0f && curve.dData.colorType==0);
    assert(mesh.dData.primCount==0U && mesh.dData.pointCount==0U && mesh.dData.pointSize==3.0f && mesh.dData.displayType==DM_DPY_HARD);
    assert(!mesh.dData.isWireframe && !mesh.dData.isVertex && !mesh.dData.isShade && mesh.dData.lineWidth==1.0f);
    auto markerDefaults=[](const DPMarkerOverride::DrawData& data) {
        assert(data.pointCount==0U && data.pointSize==3.0f && data.totalGroupPointCount==0U && !data.showIndex && !data.isColor && !data.isValue && !data.isVector);
        assert(!data.isDrawHeads && !data.isBlackTransparency && !data.isGroup && data.velocityScale==1.0f);
        assert(data.infoClass==0 && data.valType==DP_VAL_FLOAT && data.grpType==D_COMP_POINT && data.groupAlpha==1.0f);
    };markerDefaults(pointMarker.dData);
    assert(curveMarker.dData.primCount==0U && curveMarker.dData.pointCount==0U && !curveMarker.dData.isCenterVis);
    assert(curveMarker.dData.pointSize==5.0f && curveMarker.dData.totalGroupPointCount==0U && curveMarker.dData.groupAlpha==0.7f);
    assert(meshMarker.dData.primCount==0U && meshMarker.dData.pointCount==0U && !meshMarker.dData.isGroup);
    assert(meshMarker.dData.totalGroupPointCount==0U && meshMarker.dData.pointSize==3.0f);
    assert(instancer.dData.displayType==0 && instancer.dData.mCopyType==DP_INST_COPY_MESH);
    assert(instancer.dData.minPt.x==-1 && instancer.dData.maxPt.z==1);
    // A populated mode with no staged geometry must still be disabled.
    MHWRender::MRenderItemList list;reset();instancer.dData.displayType=DP_INST_SHADE;instancer.updateRenderItems(MDagPath(),list);
    for(int i=0;i<list.length();++i)assert(!list.itemAt(i)->enabled);
    // First DG failure occurs before pointSize would have been assigned.
    reset();failAt=0;failureKind=2;point.updateDG();assert(point.dData.pointCount==0U && point.dData.pointSize==3.0f);
    reset();point.updateRenderItems(MDagPath(),list);
}
void viewportSettingsTests() {
    drawingDefaultsTests();
    connectionAdmissionTests<DMeshDisplayOverride>(settingsDMeshDisplay);
    connectionAdmissionTests<DMMarkerOverride>(settingsDMMarker);
    connectionAdmissionTests<DPMarkerOverride>(settingsDPMarker);
    connectionAdmissionTests<DCurveDisplayOverride>(settingsDCurveDisplay);
    connectionAdmissionTests<DCMarkerOverride>(settingsDCMarker);
    {
        DPData points;points.count=1U;points.points={MVector(1,2,3)};inputPointData=&points;
        DPInstancerOverride node;
        useConnectionAdmission=true;
        reset();settingValues.clear();settingValues[DPInstancer::aCopyObjectType.key].mode=DP_INST_COPY_MESH;
        reachedSettingsData=false;node.updateDG();
        assert(reachedSettingsData && node.dData.pointList.empty() && node.mDataChanged);
        const auto trace=operations;std::vector<int> queries;
        for(std::size_t i=0;i<trace.size();++i)if(trace[i]=="plug.connected")queries.push_back(int(i));
        assert(queries.size()==2U);
        for(int index:{0,1})for(bool value:{false,true})for(int kind:{1,2,3}) {
            reset();reachedSettingsData=false;node.dData.pointList={9};node.mDataChanged=false;
            connectionValues[index==0?DPInstancer::inDP.key:DPInstancer::inMesh.key]=value;
            failAt=queries[index];failureKind=kind;
            node.updateDG();
            assert(!reachedSettingsData && node.dData.pointList.empty() && node.mDataChanged);
            if(kind==1)assert(apiErrorReports==1);
            else assert(diagnosticReports==1);
            reset();reachedSettingsData=false;node.updateDG();assert(reachedSettingsData);
        }
        reset();settingValues[DPInstancer::aDisplayType.key].mode=0;reachedSettingsData=false;
        node.updateDG();assert(!reachedSettingsData);
        assert(std::count(operations.begin(),operations.end(),"plug.connected")==0);
        inputPointData=nullptr;
        useConnectionAdmission=false;
    }
    settingsFailureTests(settingsDMeshDisplay,{DMeshDisplay::aIsWireFrame,DMeshDisplay::aIsVertex,DMeshDisplay::aIsShade,DMeshDisplay::aDisplayMode,DMeshDisplay::aColorType});
    settingsFailureTests(settingsDPMarker,{DPMarker::aShowIndex,DPMarker::aIsDrawHeads,DPMarker::aIsBlackTransparency,
        DPMarker::aIsColor,DPMarker::aIsValue,DPMarker::aIsVector,DPMarker::aVisibleAttributeName,DPMarker::aIsGroup,DPMarker::aGroupName});
    settingsFailureTests(settingsDCMarker,{DCMarker::aIsCenterVisible,DCMarker::aShowIndex,DCMarker::aIndexType,
        DCMarker::aIsDrawHeads,DCMarker::aIsBlackTransparency,DCMarker::aIsColor,DCMarker::aIsValue,DCMarker::aIsVector,
        DCMarker::aVisibleAttributeName,DCMarker::aIsGroup,DCMarker::aGroupName});
    settingsFailureTests(settingsDMMarker,{DMMarker::aShowIndex,DMMarker::aIndexType,DMMarker::aIsDrawHeads,
        DMMarker::aIsBlackTransparency,DMMarker::aIsColor,DMMarker::aIsValue,DMMarker::aIsVector,
        DMMarker::aVisibleAttributeName,DMMarker::aIsGroup,DMMarker::aGroupName});
    reset();DPInstancerOverride instancer;DTransformParams parms;
    assert(instancer.evalParms(parms));assert(parms.mSeed==static_cast<std::size_t>(-7));
    assert(parms.mUniformScale==2.5 && parms.mAnimationAttribute=="attribute" && parms.mLocalTransAttribute=="attribute");
    assert(parms.mNormalVec[2]==3 && parms.mUniformScaleMinMax[1]==5);
    const auto trace=operations;
    for(std::size_t step=0;step<trace.size();++step) {
        reset();failAt=int(step);assert(!instancer.evalParms(parms));
        reset();assert(instancer.evalParms(parms));
    }
    for(int kind:{1,2,3}) {
        reset();instancer.dData.pointList={1,2,3};instancer.dData.displayType=DP_INST_SHADE;instancer.mDataChanged=false;
        failAt=0;failureKind=kind;failDiagnosticOutput=true;
        instancer.updateDG();failDiagnosticOutput=false;
        assert(instancer.dData.pointList.empty() && instancer.dData.displayType==0 && instancer.mDataChanged && diagnosticReports==1);
        reset();instancer.updateDG();assert(diagnosticReports==0);
    }
}
void writeAssFixture(const std::string& path,const std::string& text) {
    std::ofstream output(path.c_str(),std::ios::binary|std::ios::trunc);
    assert(output); output<<text; output.close(); assert(output);
}
bool samePoint(const MPoint& a,const MPoint& b) {
    return a.x==b.x && a.y==b.y && a.z==b.z;
}
// Model a stream that outgrows the initial file-size probe without allocating
// the generated metadata. Keep the source finite so broken-reader mutations
// also finish, and observe exactly where reading stops.
class GeneratedAssText : public std::streambuf {
    std::size_t prefixBytes_,lineWidth_;
    std::string tail_;
public:
    std::size_t consumed=0U;
    GeneratedAssText(std::size_t prefixBytes,std::size_t lineWidth,const std::string& tail)
        : prefixBytes_(prefixBytes),lineWidth_(lineWidth),tail_(tail) {}
    int_type underflow() override {
        if(consumed<prefixBytes_)
            return consumed%lineWidth_==lineWidth_-1U ? '\n' : '#';
        const std::size_t offset=consumed-prefixBytes_;
        return offset<tail_.size() ? traits_type::to_int_type(tail_[offset]) : traits_type::eof();
    }
    int_type uflow() override {
        const int_type value=underflow();
        if(!traits_type::eq_int_type(value,traits_type::eof())) ++consumed;
        return value;
    }
};
void assStreamLimitsTests() {
    const MPoint initialMin(91,92,93),initialMax(94,95,96);
    MPoint minimum=initialMin,maximum=initialMax;
    GeneratedAssText hostileLine(kMaxAssTocLineBytes+100U,kMaxAssTocLineBytes+100U,
                                 "bounds 0 0 0 2 4 6");
    std::istream lineInput(&hostileLine);
    assert(!readAssBoundingBoxFromStream(lineInput,minimum,maximum));
    assert(hostileLine.consumed==kMaxAssTocLineBytes+1U);
    assert(samePoint(minimum,initialMin) && samePoint(maximum,initialMax));

    const std::string record="bounds 0 0 0 2 4 6";
    const std::size_t byteLimit=static_cast<std::size_t>(kMaxAssTocFileBytes);
    GeneratedAssText growing(byteLimit,1024U,record);
    std::istream growingInput(&growing);
    assert(!readAssBoundingBoxFromStream(growingInput,minimum,maximum));
    assert(growing.consumed==byteLimit+1U);
    assert(samePoint(minimum,initialMin) && samePoint(maximum,initialMax));
    GeneratedAssText exact(byteLimit-record.size()-1U,1024U,"\n"+record);
    std::istream exactInput(&exact);
    assert(readAssBoundingBoxFromStream(exactInput,minimum,maximum));
    assert(exact.consumed==byteLimit && samePoint(maximum,MPoint(1,2,3)));

    std::string exactLine=record;
    exactLine.resize(kMaxAssTocLineBytes,' ');
    std::istringstream atLineLimit(exactLine);
    assert(readAssBoundingBoxFromStream(atLineLimit,minimum,maximum));
    exactLine.push_back(' ');
    std::istringstream overLineLimit(exactLine);
    minimum=initialMin;maximum=initialMax;
    assert(!readAssBoundingBoxFromStream(overLineLimit,minimum,maximum));
    assert(samePoint(minimum,initialMin) && samePoint(maximum,initialMax));
    std::istringstream failed(record);
    failed.setstate(std::ios::badbit);
    assert(!readAssBoundingBoxFromStream(failed,minimum,maximum));
    assert(samePoint(minimum,initialMin) && samePoint(maximum,initialMax));
    class Interrupted : public std::streambuf {
        unsigned int count_=0U;
        int_type underflow() override { return '#'; }
        int_type uflow() override {
            if(++count_==20U) throw std::runtime_error("interrupted read");
            return '#';
        }
    } interrupted;
    std::istream interruptedInput(&interrupted);
    assert(!readAssBoundingBoxFromStream(interruptedInput,minimum,maximum));
    assert(interruptedInput.bad() && samePoint(minimum,initialMin) && samePoint(maximum,initialMax));
}
void assBoundsTests(const std::string& directory) {
    const std::string file=directory+"/bounds.asstoc";
    const MPoint initialMin(91,92,93),initialMax(94,95,96);
    MPoint minimum=initialMin,maximum=initialMax;
    auto rejected=[&](const std::string& contents) {
        writeAssFixture(file,contents); minimum=initialMin; maximum=initialMax;
        assert(!loadAssBoundingBoxFromFile(file,minimum,maximum));
        assert(samePoint(minimum,initialMin) && samePoint(maximum,initialMax));
    };
    assert(!loadAssBoundingBoxFromFile(file,minimum,maximum));
    assert(!loadAssBoundingBoxFromFile(directory,minimum,maximum));
    writeAssFixture(file,"metadata ignored\r\nbounds 2 4 6 8 10 14");
    assert(loadAssBoundingBoxFromFile(file,minimum,maximum));
    assert(samePoint(minimum,MPoint(-3,-3,-4)) && samePoint(maximum,MPoint(3,3,4)));
    for(const char* malformed : {"", "bound 0 0 0 1 1 1", "bounds 0 0 0 1 1",
        "bounds 2 0 0 1 1 1", "bounds 0 2 0 1 1 1", "bounds 0 0 2 1 1 1",
        "bounds nan 0 0 1 1 1", "bounds 0 0 0 inf 1 1", "bounds 0 0 0 1e309 1 1"}) rejected(malformed);
    std::feclearexcept(FE_ALL_EXCEPT);
    rejected("bounds -1e308 0 0 1e308 1 1");
    assert((std::fetestexcept(FE_OVERFLOW|FE_INVALID))==0);
    // A huge common offset is valid when the local extent is representable.
    writeAssFixture(file,"bounds 1e308 -1e308 1e308 1e308 -1e308 1e308");
    assert(loadAssBoundingBoxFromFile(file,minimum,maximum));
    assert(samePoint(minimum,MPoint()) && samePoint(maximum,MPoint()));
    std::ostringstream atLimit;
    atLimit<<std::setprecision(17)<<"bounds "<<-double(std::numeric_limits<float>::max())
           <<" 0 0 "<<double(std::numeric_limits<float>::max())<<" 0 0";
    writeAssFixture(file,atLimit.str());
    assert(loadAssBoundingBoxFromFile(file,minimum,maximum));
    assert(minimum.x==-double(std::numeric_limits<float>::max()) && maximum.x==double(std::numeric_limits<float>::max()));
    rejected("bounds -4e38 0 0 4e38 1 1");
    rejected("bounds 0 -4e38 0 1 4e38 1");
    rejected(std::string(kMaxAssTocLineBytes+1U,'x')+"\nbounds 0 0 0 1 1 1");
    rejected(std::string(kMaxAssTocLineCount,'\n')+"bounds 0 0 0 1 1 1");
    rejected(std::string(static_cast<std::size_t>(kMaxAssTocFileBytes)+1U,'x'));
    writeAssFixture(file,std::string(kMaxAssTocLineCount-1U,'\n')+"bounds 0 0 0 2 4 6");
    assert(loadAssBoundingBoxFromFile(file,minimum,maximum));
    assert(samePoint(minimum,MPoint(-1,-2,-3)) && samePoint(maximum,MPoint(1,2,3)));
#if !defined(_WIN32)
    const std::string link=directory+"/bounds-link.asstoc";
    assert(::symlink(file.c_str(),link.c_str())==0);
    assert(!loadAssBoundingBoxFromFile(link,minimum,maximum));
#endif
    const std::string filename=directory+"/cache.ass",toc=filename+"toc";
    std::string cached="previous.ass"; minimum=initialMin; maximum=initialMax;
    auto failedCache=[&](const std::string& requested) {
        const MPoint beforeMin=minimum,beforeMax=maximum;
        assert(!readAssBoundingBoxCache(requested,cached,minimum,maximum));
        assert(cached.empty() && samePoint(minimum,beforeMin) && samePoint(maximum,beforeMax));
    };
    failedCache(filename); // missing -> created under exactly the same name
    writeAssFixture(toc,"bounds 0 0 0 2 4 6");
    assert(readAssBoundingBoxCache(filename,cached,minimum,maximum) && cached==filename);
    assert(samePoint(minimum,MPoint(-1,-2,-3)) && samePoint(maximum,MPoint(1,2,3)));
    // Successful entries retain historical filename-based cache behavior.
    writeAssFixture(toc,"bounds 0 0 0 4 6 8");
    assert(readAssBoundingBoxCache(filename,cached,minimum,maximum));
    assert(samePoint(maximum,MPoint(1,2,3)));
    assert(std::remove(toc.c_str())==0); failedCache(filename);
    writeAssFixture(toc,"malformed"); failedCache(filename);
    writeAssFixture(toc,"bounds 0 0 0 4 6 8");
    assert(readAssBoundingBoxCache(filename,cached,minimum,maximum));
    assert(samePoint(minimum,MPoint(-2,-3,-4)) && samePoint(maximum,MPoint(2,3,4)));
    const std::string other=directory+"/other.ass";
    writeAssFixture(other+"toc","bounds 0 -4e38 0 1 4e38 1"); failedCache(other);
    writeAssFixture(other+"toc","bounds -2 -4 -6 2 4 6");
    assert(readAssBoundingBoxCache(other,cached,minimum,maximum) && cached==other);
    assert(samePoint(maximum,MPoint(2,4,6)));
    failedCache("");
}

template <typename Node> std::size_t shaderLifecycleTests() {
    using namespace MHWRender;
    resetShaderAcquisition();
    std::vector<MShaderInstance*> owned;
    std::vector<std::string> acquireTrace;
    {
        Node node(MObject(9)); assert(!node.mShaderUpdateFailed);
        assertAcquiredShaders(node); owned=node.releaseOrder(); acquireTrace=operations;
        assert(std::set<MShaderInstance*>(owned.begin(),owned.end()).size()==owned.size());
        assert(stockShaderRequests.size()==owned.size());
        reset();
    }
    assert(shaderReleaseRequests==owned);
    for(const auto* shader:owned) assert(shader->released);
    for(int missing:{0,1,2}) {
        resetShaderAcquisition();
        if(missing==0)rendererAvailable=false;
        if(missing==1)shaderManagerAvailable=false;
        if(missing==2)stockShaderAvailable=false;
        {
            Node node(MObject(9)); assert(node.mShaderUpdateFailed);
            for(const auto* shader:node.shaderPointers())assert(!shader);
            reset(); rendererAvailable=shaderManagerAvailable=stockShaderAvailable=true;
        }
        assert(shaderReleaseRequests.empty() && operations.empty());
    }
    for(std::size_t step=0;step<acquireTrace.size();++step) for(int kind:{1,2,3}) {
        resetShaderAcquisition(); failAt=int(step);failureKind=kind;failDiagnosticOutput=true;
        {
            Node node(MObject(9)); assert(node.mShaderUpdateFailed);
            const auto pointers=node.releaseOrder(); owned.clear();
            for(auto* shader:pointers)if(shader)owned.push_back(shader);
            assert(std::set<MShaderInstance*>(owned.begin(),owned.end()).size()==owned.size());
            assert(diagnosticReports==(kind==1?0:1));
            reset();
        }
        failDiagnosticOutput=false; assert(shaderReleaseRequests==owned);
    }
    resetShaderAcquisition();
    std::unique_ptr<Node> baseline(new Node(MObject(9)));
    owned=baseline->releaseOrder(); reset(); baseline.reset();
    const auto releaseTrace=operations;
    assert(releaseTrace.size()==owned.size()+2U && shaderReleaseRequests==owned);
    for(std::size_t step=0;step<releaseTrace.size();++step) for(int kind:{1,2,3}) {
        resetShaderAcquisition(); std::unique_ptr<Node> node(new Node(MObject(9)));
        owned=node->releaseOrder(); reset();
        failAt=int(step);failureKind=kind;failDiagnosticOutput=true;node.reset();
        assert(diagnosticReports==((kind==1 && step<2U)?0:1));
        if(step<2U)assert(shaderReleaseRequests.empty());
        else assert(shaderReleaseRequests==owned); // later entries still released
        failDiagnosticOutput=false;
    }
    resetShaderAcquisition(); std::unique_ptr<Node> node(new Node(MObject(9)));
    owned=node->releaseOrder(); reset();
    releaseThrowsAfterCompletion=true; failDiagnosticOutput=true; node.reset();
    assert(shaderReleaseRequests==owned && diagnosticReports==int(owned.size()));
    for(const auto* shader:owned)assert(shader->released);
    failDiagnosticOutput=false; resetShaderAcquisition(); return owned.size();
}
void stockShaderReleaseTests() {
    using namespace MHWRender;
    resetShaderAcquisition(); rendererAvailable=false;
    {
        LifecycleDPInstancerOverride node(MObject(9));
        assert(node.mShaderUpdateFailed && node.dData.displayType==0);
        assert(samePoint(node.dData.minPt,MPoint(-1,-1,-1)) && samePoint(node.dData.maxPt,MPoint(1,1,1)));
    }
    resetShaderAcquisition(); MShaderInstance first,second;
    MShaderInstance* a=&first; MShaderInstance* b=&second; MShaderInstance* empty=nullptr;
    dommy::viewport::StockShaderRelease release("host lifecycle");
    release.release(empty);assert(operations.empty());
    failAt=2; failureKind=2;failDiagnosticOutput=true;
    release.release(a);assert(!a && !first.released && diagnosticReports==1);
    const auto count=operations.size();release.release(a);assert(operations.size()==count);
    failAt=-1;release.release(b);assert(!b && second.released);
    assert(std::count(operations.begin(),operations.end(),"shader.renderer")==1);
    assert(std::count(operations.begin(),operations.end(),"shader.manager")==1);
    assert(shaderReleaseRequests==std::vector<MShaderInstance*>({&first,&second}));
    failDiagnosticOutput=false;
    const std::size_t countShaders=shaderLifecycleTests<LifecycleDMeshDisplayOverride>()+
        shaderLifecycleTests<LifecycleDMMarkerOverride>()+shaderLifecycleTests<LifecycleDPInstancerOverride>()+
        shaderLifecycleTests<LifecycleDPointDisplayOverride>()+shaderLifecycleTests<LifecycleDPMarkerOverride>()+
        shaderLifecycleTests<LifecycleDCurveDisplayOverride>()+shaderLifecycleTests<LifecycleDCMarkerOverride>()+
        shaderLifecycleTests<LifecycleDOpenVDBDisplayVP2Override>();
    assert(countShaders==43U);
}
int main(int argc,char** argv) {
    assert(argc==2); assStreamLimitsTests(); assBoundsTests(argv[1]); stockShaderReleaseTests();
    viewportSettingsTests();arnoldTransformInputTests();
    shaderDiagnosticTests();
    vertexRequirementTests(); gpuTests(); pointUpdateTests(); pointRenderItemTests(); shaderTests(); stockShaderTests(); traversalTests(); instanceTests(); bboxTests(); displayNormalTests();
    std::cout<<"Separated bounds/mean, bounded Ass streams/cache repair, eight constructor/destructor bodies, 43 shader release/acquisition/state/recovery paths, seven drawing defaults, checked settings/DG exception recovery, eight populate/render-item callbacks, debug profile "
             <<DOMMY_ENABLE_DEBUG_LOGS<<", ordered/fail-fast stream visitor, typed layouts, point CPU staging, retry and traversal/instance budgets passed\n";
}
