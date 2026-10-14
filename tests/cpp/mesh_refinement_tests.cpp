#include "mesh_refinement_doubles.h"
#define DMeshData RefinementData
#define DMeshInternalData RefinementItem
#define MFnMesh RefinementMesh
#define MItMeshPolygon RefinementPolygon
#include "mesh_refinement.inc"
#undef MItMeshPolygon
#undef MFnMesh
#undef DMeshInternalData
#undef DMeshData

static void fixture(){
    reset();injectedStatus=MS::kFailure;inputs.clear();emitterMeshes.clear();
    nativeFaces.clear();nativeConnects.clear();vertexFaces.clear();vertexEdges.clear();edgeFaces.clear();
    refinementInput=RefinementData();refinementStorage=RefinementData();refinementOutput=RefinementData();
    auto item=std::make_shared<RefinementItem>();
    item->groups["outer"]=std::make_shared<DPGroupData>(DPIdList{true,false,false,false});
    item->groups["inner"]=std::make_shared<DPGroupData>(DPIdList{false,true,true,true});
    refinementInput.items.push_back(item);
    invalidVertexId=invalidEdgeId=invalidFaceId=invalidEndpoint=invalidCenter=excessiveRefinement=false;
    invalidPointCount=false;edgeLength=1.0;closestOffset=0.0;lastDivisions=0;lastSelection={};nativeFault=0;
    inputs["FaceGroup"].string="outer";inputs["VectorAttribute"].string="vel";
    inputs["DisplaceAttribute"].string="disp";inputs["Displace"].number=3;
    inputs["CheckMinLength"].number=1;
    inputs["SubdivClassType"].number=D_COMP_FACE;inputs["Iteration"].number=1;
    inputs["SchemeType"].number=1;inputs["EdgeMaxIteration"].number=2;inputs["EdgeMinLength"].number=.5;
    inputs["inObject"].object={"reference"};inputs["inObject"].type=MFnData::kMesh;inputs["inObject"].connected=true;
    emitterMeshes["reference"]=EmitterMesh();nativeFaces["reference"]={3};nativeConnects["reference"]={0,1,2};
    inputs["InnerName"].string="inner";inputs["OuterName"].string="outer";inputs["Tolerance"].number=.1;
}
template<typename Node> static MStatus compute(){Node node;MDataBlock data;return node.compute(MPlug({"node"},Node::outDM),data);}
static std::size_t calls(const std::string& name){return std::count(operations.begin(),operations.end(),name);}
static void inputUnchanged(){
    for(const auto& item:refinementInput.items){
        assert(item->replacements==0 && item->edits==0);
        assert(item->points[0].x==0 && item->points.size()==4);
        assert(item->groups.at("outer")->ids==DPIdList({true,false,false,false}));
    }
}
static int failures(const std::function<MStatus()>& evaluate,const std::function<void()>& setup=[] {}){
    fixture();setup();const auto result=evaluate();
    if(!result)for(const auto& call:operations)std::cerr<<call<<'\n';
    assert(result==MS::kSuccess && published==1 && cleaned==1);inputUnchanged();
    const auto trace=operations;int tested=0;
    for(std::size_t i=0;i<trace.size();++i){
        for(auto status:{MS::kFailure,MS::kInvalidParameter}){
            fixture();setup();failAt=i;injectedStatus=status;
            assert(!evaluate());assert(callCount==int(i)+1);inputUnchanged();
            assert(cleaned==0);if(trace[i]!="clean")assert(published==0);++tested;
        }
        fixture();setup();throwAt=i;assert(!evaluate());inputUnchanged();
        if(trace[i]!="clean")assert(published==0);++tested;
        if(trace[i]=="array.resize" || trace[i]=="attribute.vector" || trace[i]=="attribute.scalar" || trace[i]=="mesh.triangles"){
            fixture();setup();shortAt=i;assert(!evaluate() && published==0 && shortApplied);++tested;
        }
    }
    return tested;
}
static int edgeDisplacement(){
    const int tested=failures(compute<DMEdgeDisplace>);
    fixture();assert(compute<DMEdgeDisplace>()==MS::kSuccess);
    assert(refinementOutput.items[0]->points[0].x==1 && refinementOutput.items[0]->points[3].x==0);
    fixture();inputs["CheckMinLength"].number=0;assert(compute<DMEdgeDisplace>()==MS::kSuccess);
    assert(refinementOutput.items[0]->points[0].x==3);
    fixture();inputs["IsConcernCorner"].number=1;assert(compute<DMEdgeDisplace>()==MS::kSuccess);
    assert(refinementOutput.items[0]->points[0].x==0);
    fixture();refinementInput.items[0]->scalarAttribute=false;inputs["Displace"].number=-.5;
    assert(compute<DMEdgeDisplace>()==MS::kSuccess && refinementOutput.items[0]->points[0].x==-.5);
    fixture();refinementInput.items[0]->unsupportedScalar=true;inputs["Displace"].number=-.5;
    assert(compute<DMEdgeDisplace>()==MS::kSuccess && refinementOutput.items[0]->points[0].x==-.5);
    fixture();refinementInput.items[0]->unsupportedVector=true;
    assert(compute<DMEdgeDisplace>()==MS::kSuccess && refinementOutput.items[0]->points[0].x==0);
    fixture();refinementInput.items[0]->groups.erase("outer");assert(compute<DMEdgeDisplace>()==MS::kSuccess && calls("mesh.convertUV")==0);
    fixture();refinementInput.items[0]->groups["outer"]=std::make_shared<DPGroupData>(DPIdList(4,false));
    assert(compute<DMEdgeDisplace>()==MS::kSuccess && calls("mesh.convertUV")==0);
    fixture();refinementInput.items[0]->groups["outer"]=std::make_shared<DPGroupData>(DPIdList(3,true));
    assert(!compute<DMEdgeDisplace>() && published==0);
    fixture();invalidVertexId=true;assert(!compute<DMEdgeDisplace>() && published==0);
    for(double bad:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        fixture();edgeLength=bad;assert(!compute<DMEdgeDisplace>() && published==0);
    }
    fixture();refinementInput.items[0]->vectors[0].x=NAN;assert(!compute<DMEdgeDisplace>() && published==0);
    fixture();refinementInput.items[0]->scalars[0]=INFINITY;assert(!compute<DMEdgeDisplace>() && published==0);
    fixture();refinementInput.items[0]->vectors[0].x=std::numeric_limits<double>::max();
    assert(!compute<DMEdgeDisplace>() && published==0);
    fixture();inputs["Displace"].number=NAN;assert(!compute<DMEdgeDisplace>() && published==0);
    fixture();refinementInput.items[0]->scalars[0]=std::numeric_limits<double>::max();
    assert(!compute<DMEdgeDisplace>() && published==0);
    for(int fault:{1,2,4}){fixture();nativeFault=fault;assert(!compute<DMEdgeDisplace>() && published==0);}
    return tested;
}
static void edgeMode(){inputs["SubdivClassType"].number=D_COMP_EDGE;}
static void lengthMode(){edgeMode();inputs["EdgeSamplingType"].number=1;}
static void loopMode(){inputs["SchemeType"].number=2;}
static int subdivision(){
    int tested=failures(compute<DMSubDivide>)+failures(compute<DMSubDivide>,edgeMode)+
        failures(compute<DMSubDivide>,lengthMode)+failures(compute<DMSubDivide>,loopMode);
    fixture();edgeMode();inputs["Iteration"].number=2;
    assert(compute<DMSubDivide>()==MS::kSuccess && lastDivisions==2 && lastSelection.length()==6);
    fixture();lengthMode();assert(compute<DMSubDivide>()==MS::kSuccess);
    assert(calls("mesh.subdivideEdges")==2 && lastSelection.length()==3 && lastDivisions==1);
    fixture();lengthMode();edgeLength=.1;assert(compute<DMSubDivide>()==MS::kSuccess && calls("mesh.subdivideEdges")==0);
    fixture();lengthMode();refinementInput.items[0]->groups["outer"]=std::make_shared<DPGroupData>(DPIdList(4,false));
    assert(compute<DMSubDivide>()==MS::kSuccess && calls("mesh.convertUV")==0);
    fixture();edgeMode();excessiveRefinement=true;assert(!compute<DMSubDivide>() && published==0 && calls("mesh.replace")==0);
    fixture();lengthMode();invalidEdgeId=true;assert(!compute<DMSubDivide>() && published==0);
    for(int fault:{3,4}){fixture();lengthMode();nativeFault=fault;assert(!compute<DMSubDivide>() && published==0);}
    fixture();lengthMode();edgeLength=NAN;assert(!compute<DMSubDivide>() && published==0);
    fixture();inputs["Weight"].number=std::numeric_limits<float>::max();inputs["Iteration"].number=8;
    assert(!compute<DMSubDivide>() && calls("mesh.copy")==0 && calls("mesh.convert")==0);
    fixture();inputs["Iteration"].number=9;assert(!compute<DMSubDivide>() && calls("mesh.copy")==0);
    fixture();refinementInput.items[0]->reportedPoints=10000000;
    assert(!compute<DMSubDivide>() && calls("mesh.copy")==0 && calls("mesh.convert")==0);
    fixture();inputs["Iteration"].number=8;
    // Four small inputs jointly exceed the predicted level-buffer budget.
    for(int i=0;i<3;++i)refinementInput.items.push_back(std::make_shared<RefinementItem>(*refinementInput.items[0]));
    assert(!compute<DMSubDivide>() && calls("mesh.copy")==0 && calls("mesh.convert")==0);
    fixture();inputs["Iteration"].number=0;assert(compute<DMSubDivide>()==MS::kSuccess && calls("mesh.convert")==0);
    fixture();loopMode();inputs["Iteration"].number=8;
    for(int i=0;i<3;++i)refinementInput.items.push_back(std::make_shared<RefinementItem>(*refinementInput.items[0]));
    assert(!compute<DMSubDivide>() && calls("mesh.triangles")==0 && calls("mesh.copy")==0);
    for(const char* attr:{"SchemeType","InterpolationType","FVarLinearInterpolationType","CreasingMethod","TriangleSubdivision"}){
        fixture();inputs[attr].number=99;assert(!compute<DMSubDivide>() && calls("mesh.copy")==0);
    }
    return tested;
}
static int faceGroups(){
    const int tested=failures(compute<DMFaceInOut>);
    fixture();assert(compute<DMFaceInOut>()==MS::kSuccess);
    assert(refinementOutput.items[0]->groups.at("outer")->ids==DPIdList(4,true));
    fixture();closestOffset=1;assert(compute<DMFaceInOut>()==MS::kSuccess);
    assert(refinementOutput.items[0]->groups.at("inner")->ids==DPIdList(4,true));
    fixture();inputs["Tolerance"].number=0;assert(compute<DMFaceInOut>()==MS::kSuccess);
    assert(refinementOutput.items[0]->groups.at("inner")->ids==DPIdList(4,true));
    fixture();inputs["inObject"].connected=false;assert(compute<DMFaceInOut>()==MS::kSuccess && calls("closest.create")==0);
    fixture();inputs["inObject"].type=MFnData::kNurbsCurve;assert(compute<DMFaceInOut>()==MS::kSuccess && calls("closest.create")==0);
    fixture();invalidFaceId=true;assert(!compute<DMFaceInOut>() && published==0);
    fixture();invalidCenter=true;assert(!compute<DMFaceInOut>() && published==0 && calls("closest.query")==0);
    for(int fault:{4,5}){fixture();nativeFault=fault;assert(!compute<DMFaceInOut>() && published==0);}
    fixture();closestOffset=INFINITY;assert(!compute<DMFaceInOut>() && published==0);
    fixture();inputs["InnerName"].string="outer";assert(!compute<DMFaceInOut>() && calls("closest.create")==0);
    fixture();inputs["Tolerance"].number=NAN;assert(!compute<DMFaceInOut>() && calls("closest.create")==0);
    fixture();emitterMeshes["reference"].pointCount=10000000;
    assert(!compute<DMFaceInOut>() && calls("closest.create")==0);
    return tested;
}
static int multipleMeshFailures(){
    int tested=0;
    for(auto evaluate:{compute<DMEdgeDisplace>,compute<DMSubDivide>,compute<DMFaceInOut>}){
        fixture();refinementInput.items.push_back(std::make_shared<RefinementItem>(*refinementInput.items[0]));
        assert(evaluate()==MS::kSuccess && refinementOutput.items.size()==2);
        const std::string conversion=evaluate==compute<DMSubDivide>?"mesh.convert":"mesh.convertUV";
        auto first=std::find(operations.begin(),operations.end(),conversion);assert(first!=operations.end());
        auto second=std::find(first+1,operations.end(),conversion);assert(second!=operations.end());
        const int index=int(second-operations.begin());
        for(auto status:{MS::kFailure,MS::kInvalidParameter}){
            fixture();refinementInput.items.push_back(std::make_shared<RefinementItem>(*refinementInput.items[0]));
            failAt=index;injectedStatus=status;assert(!evaluate() && published==0 && cleaned==0);inputUnchanged();++tested;
        }
    }
    return tested;
}
static void traversalAndBudgets(){
    fixture();MObject object;assert(generateMMeshFromDmesh(*refinementInput.items[0],object)==MS::kSuccess);
    emitterMeshes[object.name].stuck=true;MStatus status;MItMeshVertex vertices(object,&status);
    assert(!dommy::maya::visitMeshElements(vertices,4,[](MItMeshVertex&){return MS::kSuccess;}));
    MItMeshEdge edges(object,&status);
    assert(!dommy::maya::visitMeshElements(edges,6,[](MItMeshEdge&){return MS::kSuccess;}));
    RefinementPolygon faces(object,&status);
    assert(!dommy::maya::visitMeshPolygons(faces,4,[](RefinementPolygon&){return MS::kSuccess;}));
    using namespace dommy::mesh;SubdivisionBudget unit,total;
    unit.estimatedFinalVertexCount=10;unit.estimatedFinalFaceCount=4;
    unit.estimatedFinalFaceVertexCount=16;unit.estimatedTotalWorkItems=4194304;
    assert(appendSubdivisionBudget(unit,total) && appendSubdivisionBudget(unit,total));
    const auto previous=total;
    assert(!appendSubdivisionBudget(unit,total) && total.estimatedTotalWorkItems==previous.estimatedTotalWorkItems);
    unit.estimatedTotalWorkItems=std::numeric_limits<std::size_t>::max();
    assert(!appendSubdivisionBudget(unit,total) && total.estimatedFinalVertexCount==previous.estimatedFinalVertexCount);
    float weight=1;assert(!dommy::numeric::checkedNumericCast(static_cast<long double>(std::numeric_limits<float>::max())*8,weight));
    MDataBlock data;fixture();assert(makePieceReader(data)==MS::kSuccess && callCount==1);
    fixture();failAt=0;assert(!makePieceReader(data) && callCount==1);
}
#ifdef DOMMY_TEST_REAL_OPENSUBDIV
static void realSubdivision(){
    for(int scheme=0;scheme<3;++scheme){
        fixture();inputs["SchemeType"].number=scheme;assert(compute<DMSubDivide>()==MS::kSuccess);
        const auto result=*refinementOutput.items[0];
        assert(result.points.size()==(scheme==2?10U:14U));assert(result.faces.length()==(scheme==2?16U:12U));
        if(scheme==0)for(unsigned i=0;i<4;++i)assert(!(result.points[i]!=refinementInput.items[0]->points[i]));
        fixture();inputs["SchemeType"].number=scheme;
        for(auto& p:refinementInput.items[0]->points)p=p+MVector(4,-2,1);
        assert(compute<DMSubDivide>()==MS::kSuccess);
        const auto& translated=*refinementOutput.items[0];
        assert(translated.faces==result.faces && translated.connects==result.connects);
        for(unsigned i=0;i<result.points.size();++i)
            assert(translated.points[i].distanceTo(result.points[i]+MVector(4,-2,1))<1e-5);
        fixture();inputs["SchemeType"].number=scheme;invalidEndpoint=true;
        assert(!compute<DMSubDivide>() && published==0 && calls("mesh.replace")==0);
    }
    std::cout<<"Real OpenSubdiv 3.3.3: bilinear/Catmull-Clark/Loop topology and translation invariance passed\n";
}
#endif
int main(int argc,char** argv){
    const char* names[]={"DMEdgeDisplace","DMSubDivide","DMFaceInOut","DMExplodeView","DMTransform","DMMakePiece","DMMerge","DMDelete"};
    MStatus(*initializers[])()={DMEdgeDisplace::initialize,DMSubDivide::initialize,DMFaceInOut::initialize,
        DMExplodeView::initialize,DMTransform::initialize,DMMakePiece::initialize,DMMerge::initialize,DMDelete::initialize};
    std::string schema;int counts[8];
    for(int i=0;i<8;++i){schema+=schemaTrace(names[i],initializers[i]);counts[i]=callCount;}
    if(argc==2 && std::string(argv[1])=="--print-schema"){std::cout<<schema;return 0;}
    assert(argc==2);std::ifstream file(argv[1]);assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)),{});assert(schema==expected);
    int tested=0;for(int i=0;i<8;++i)tested+=testFailures(names[i],initializers[i],counts[i]);
    tested+=edgeDisplacement();tested+=subdivision();tested+=faceGroups();tested+=multipleMeshFailures();traversalAndBudgets();
    for(auto evaluate:{compute<DMEdgeDisplace>,compute<DMSubDivide>,compute<DMFaceInOut>}){
        fixture();refinementInput.items.clear();assert(evaluate()==MS::kSuccess && published==1 && cleaned==1);
    }
#ifdef DOMMY_TEST_REAL_OPENSUBDIV
    realSubdivision();
#endif
    std::cout<<"Mesh refinement: eight initializer contracts, budgets/transactions and "<<tested<<" injected failures passed\n";
}
