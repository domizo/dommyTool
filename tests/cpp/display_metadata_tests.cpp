#include "display_metadata_host.h"
#include "display_metadata_methods.inc"
#ifdef DOMMY_DISPLAY_ALLOCATION_TESTS
#include "allocation_failure_fixture.h"
#endif
#include <cstring>

void resetMetadata()
{
    operations.clear(); failAttribute=failOperation=throwOperation=-1;
    injectedApiStatus=MS::kInvalidParameter;
    exceptionKind=reports=publications=cleanCalls=0;
    connected=validInput=true; outputInfo.clear();
}

#ifdef DOMMY_DISPLAY_ALLOCATION_TESTS
template <typename Node, typename Data>
void metadataFormattingFailures(Node& node, Data& data, unsigned int attributeCount = 128U)
{
    data.attrs.clear();
    for (unsigned int index = 0U; index < attributeCount; ++index)
        data.template field<float>("field_" + std::to_string(index) + "_abcdefgh", "float", {1});
    MDataBlock block;
    resetMetadata();
    assert(node.compute(MPlug(Node::aNodeInfo), block));
    const std::string expected = block.values[Node::aNodeInfo.id].string.text;
    unsigned int faults = 0U, rejected = 0U;
    for (std::size_t failure = 1U; failure <= 96U; ++failure) {
        resetMetadata();
        block.values[Node::aNodeInfo.id].string = "prior";
        fault::arm(failure);
        const MStatus status = node.compute(MPlug(Node::aNodeInfo), block);
        fault::armed = false;
        faults += fault::triggered;
        rejected += !status;
        const std::string& text = block.values[Node::aNodeInfo.id].string.text;
        // Native output/clean is not an atomic transaction. A late host failure
        // may leave a complete replacement, but never a formatted prefix.
        assert(text == "prior" || text == expected);
        if (status) assert(text == expected && publications == 1 && cleanCalls == 1);
    }
    assert(faults != 0U && rejected != 0U);
    data.attrs.clear();
    resetMetadata();
    assert(node.compute(MPlug(Node::aNodeInfo), block));
}

void allMetadataFormattingFailures()
{
    DPointDisplay points; DPMarker pointMarker;
    DCurveDisplay curves; DCMarker curveMarker;
    DMeshDisplay meshes; DMMarker meshMarker;
    DPLightGen lights;
    metadataFormattingFailures(points, metadataPoints);
    metadataFormattingFailures(pointMarker, metadataPoints);
    metadataFormattingFailures(curves, metadataCurves);
    metadataFormattingFailures(curveMarker, metadataCurves);
    metadataFormattingFailures(meshes, metadataMeshes);
    metadataFormattingFailures(meshMarker, metadataMeshes);
    metadataFormattingFailures(lights, metadataPoints);
    metadataFormattingFailures(lights, metadataPoints, 0U);
}
#endif

template <typename Node>
void metadataStatusPaths(Node& node,MObject output,MObject input,
                         bool checkedConnection = true)
{
    MDataBlock block;
    resetMetadata(); assert(node.compute(MPlug(output),block));
    assert(publications==1 && cleanCalls==1);
    const auto calls=operations;
    for (std::size_t i=0;i<calls.size();++i) {
        if (checkedConnection && calls[i]=="plug.connected") {
            for (bool connectionValue : {false,true}) {
                resetMetadata(); connected=connectionValue; failOperation=int(i);
                block.values[output.id].string="preserved";
                block.values[output.id].scalar=17;
                assert(node.compute(MPlug(output),block)==MS::kInvalidParameter);
                assert(publications==0 && cleanCalls==0);
                assert(block.values[output.id].string.text=="preserved" &&
                       block.values[output.id].scalar==17);
                assert(operations.size()==i+1);
                resetMetadata(); assert(node.compute(MPlug(output),block));
                assert(publications==1 && cleanCalls==1);
            }
        }
        if (calls[i]=="output" || calls[i]=="data.read" || calls[i]=="clean") {
            resetMetadata(); failOperation=int(i);
            assert(node.compute(MPlug(output),block)==MS::kInvalidParameter);
            if (calls[i]!="clean") assert(publications==0 && cleanCalls==0);
            resetMetadata(); assert(node.compute(MPlug(output),block));
        }
        for (int kind=0;kind<2;++kind) {
            resetMetadata(); throwOperation=int(i); exceptionKind=kind;
            assert(!node.compute(MPlug(output),block) && reports==1);
        }
    }
    resetMetadata(); validInput=false;
    assert(node.compute(MPlug(output),block)==MS::kInvalidParameter && publications==0 && cleanCalls==0);
    resetMetadata(); failAttribute=input.id;
    assert(node.compute(MPlug(output),block)==MS::kInvalidParameter && publications==0 && cleanCalls==0);
    resetMetadata(); assert(node.compute(MPlug(output),block));
    assert(publications==1 && cleanCalls==1);
    resetMetadata(); connected=false; failAttribute=input.id;
    assert(node.compute(MPlug(output),block));
    assert(publications==1 && cleanCalls==1);
    for (const auto& operation : operations)
        assert(operation!="input" && operation!="data.read");
    resetMetadata(); assert(node.compute(MPlug(MObject(-1)),block)==MS::kUnknownParameter);
    assert(operations.empty() && publications==0 && cleanCalls==0);
}

template <typename Node,typename Data>
void markerOutputs(Node& node,Data& data,MObject input)
{
    MDataBlock block;
    for (MObject output : {Node::aAllAttributeNames,Node::aAllGroupNames}) {
        resetMetadata(); block.values[output.id].string="stale";
        block.values[Node::aNodeInfo.id].string="preserved";
        connected=false;
        assert(node.compute(MPlug(output),block));
        assert(publications==1 && cleanCalls==1 && outputInfo.empty());
        // The recording handle observes the exact requested attribute.
        assert(block.values[output.id].string.text.empty());
        assert(block.values[Node::aNodeInfo.id].string.text=="preserved");
        metadataStatusPaths(node,output,input);
    }
    resetMetadata(); data.template field<float>("velocity","float",{1});
    data.groups["selected"]=std::make_shared<Group>();
    assert(node.compute(MPlug(Node::aAllAttributeNames),block) && outputInfo=="velocity");
    resetMetadata(); assert(node.compute(MPlug(Node::aAllGroupNames),block) && outputInfo=="selected");
    resetMetadata(); connected=false;
    assert(node.compute(MPlug(Node::aAllGroupNames),block) && outputInfo.empty());
    data.attrs.clear(); data.groups.clear();
    metadataStatusPaths(node,Node::aNodeInfo,input);
}

template <typename Node>
void displayBounds(Node& node,double fallback,bool checkedConnection=true)
{
    resetMetadata(); const MBoundingBox ordinary=node.boundingBox();
    assert(ordinary.first.x==-2 && ordinary.first.y==-3 && ordinary.second.z==7);
    const auto calls=operations;
    for (std::size_t i=0;i<calls.size();++i) {
        if (checkedConnection && calls[i]=="plug.connected") {
            for (bool connectionValue : {false,true}) {
                resetMetadata(); connected=connectionValue; failOperation=int(i);
                const MBoundingBox bounds=node.boundingBox();
                assert(bounds.first.x==-fallback && bounds.second.x==fallback);
                assert(operations.size()==i+2 && operations.back()=="error");
                // Even diagnostic exceptions must stay inside the callback.
                for (int kind=0;kind<2;++kind) {
                    resetMetadata(); failOperation=int(i); throwOperation=int(i+1);
                    exceptionKind=kind;
                    assert(node.boundingBox().first.x==-fallback && reports==1);
                }
                resetMetadata(); assert(node.boundingBox().second.z==7 && reports==0);
            }
        }
        for (int kind=0;kind<2;++kind) {
            resetMetadata(); throwOperation=int(i); exceptionKind=kind;
            const MBoundingBox bounds=node.boundingBox();
            assert(reports==1 && bounds.first.x==-fallback && bounds.first.y==-fallback &&
                   bounds.first.z==-fallback && bounds.second.x==fallback &&
                   bounds.second.y==fallback && bounds.second.z==fallback);
        }
        if (calls[i]=="point.plug" || calls[i]=="mesh.plug" || calls[i]=="curve.handle") {
            resetMetadata(); failOperation=int(i);
            const MBoundingBox bounds=node.boundingBox();
            assert(bounds.first.x==-fallback && bounds.second.x==fallback);
        }
    }
    resetMetadata(); connected=false;
    assert(node.boundingBox().first.x==-fallback);
    assert(operations.back()=="plug.connected");
    resetMetadata(); validInput=false;
    assert(node.boundingBox().second.x==fallback);
    resetMetadata(); assert(node.boundingBox().second.z==7 && reports==0);
}

template <typename Node>
void emptySummary(Node& node,MObject output,const char* count)
{
    MDataBlock block;
    resetMetadata(); assert(node.compute(MPlug(output),block));
    assert(publications==1 && cleanCalls==1);
    assert(outputInfo.find(count)!=std::string::npos);
    assert(outputInfo.find("not connected")==std::string::npos);
    assert(outputInfo.find("isEmpty")==std::string::npos);
}

void vdbMetadata(DOpenVDBDisplayVP2& node)
{
    MDataBlock block;
    for (MObject output : {node.aNodeInfo,node.aVdbAllGridNames}) {
        metadataStatusPaths(node,output,node.aVdbInput);
        resetMetadata(); metadataVdb.valid=false;
        assert(node.compute(MPlug(output),block)==MS::kInvalidParameter);
        assert(publications==0 && cleanCalls==0);
        metadataVdb.valid=true;
        resetMetadata(); connected=false; failAttribute=node.aVdbInput.id;
        block.values[node.aNodeInfo.id].string="preserved";
        block.values[node.aVdbAllGridNames.id].string="stale";
        assert(node.compute(MPlug(output),block));
        assert(publications==1 && cleanCalls==1);
        for (const auto& operation : operations) assert(operation!="input");
        if (output.id==node.aVdbAllGridNames.id) {
            assert(outputInfo.empty());
            assert(block.values[node.aNodeInfo.id].string.text=="preserved");
        } else {
            assert(outputInfo.find("not connected")!=std::string::npos);
            assert(block.values[node.aVdbAllGridNames.id].string.text=="stale");
        }
        resetMetadata(); assert(node.compute(MPlug(output),block));
        assert(outputInfo.find("not connected")==std::string::npos);
    }
    resetMetadata(); failAttribute=node.aVdbSelectedGridNames.id;
    assert(node.compute(MPlug(node.aNodeInfo),block)==MS::kInvalidParameter);
    assert(publications==0 && cleanCalls==0);
    resetMetadata(); block.values[node.aVdbSelectedGridNames.id].string="density";
    assert(node.compute(MPlug(node.aNodeInfo),block));
    assert(outputInfo.find("selection: density")!=std::string::npos);
    resetMetadata(); assert(node.compute(MPlug(node.aVdbAllGridNames),block));
    assert(outputInfo=="density velocity");
    metadataVdb.names.clear();
    resetMetadata(); assert(node.compute(MPlug(node.aNodeInfo),block));
    assert(outputInfo.find("Grid is empty")!=std::string::npos);
    assert(outputInfo.find("not connected")==std::string::npos);
    resetMetadata(); assert(node.compute(MPlug(node.aVdbAllGridNames),block));
    assert(outputInfo.empty() && publications==1 && cleanCalls==1);
    metadataVdb=DOpenVDBData();
}

void instancerCompute(DPInstancer& node)
{
    MDataBlock block;
    for (MObject attribute : {node.inDP,node.inMesh,node.aDisplayType,MObject(-1)}) {
        resetMetadata();
        assert(node.compute(MPlug(attribute),block)==MS::kUnknownParameter);
        assert(operations.empty() && publications==0 && cleanCalls==0);
    }
}

int main(int argc, char** argv)
{
#ifdef DOMMY_DISPLAY_ALLOCATION_TESTS
    if (argc == 2 && std::strcmp(argv[1], "format") == 0) {
        allMetadataFormattingFailures();
        return 0;
    }
#endif
    (void)argc; (void)argv;
    DPointDisplay points; DPMarker pointMarker; DCMarker curveMarker;
    DMMarker meshMarker; DMeshDisplay meshes; DCurveDisplay curves;
    DOpenVDBDisplayVP2 vdb; DPLightGen lights; MDataBlock block;
    metadataStatusPaths(lights,DPLightGen::aNodeInfo,DPLightGen::inDP);
    resetMetadata(); metadataPoints.topology=false;
    block.values[DPLightGen::aNodeInfo.id].string="preserved";
    assert(lights.compute(MPlug(DPLightGen::aNodeInfo),block)==MS::kInvalidParameter);
    assert(publications==0 && cleanCalls==0 && outputInfo.empty());
    assert(block.values[DPLightGen::aNodeInfo.id].string.text=="preserved");
    metadataPoints.topology=true;
    resetMetadata(); assert(lights.compute(MPlug(DPLightGen::aNodeInfo),block));
    metadataStatusPaths(points,DPointDisplay::aNodeInfo,DPointDisplay::inDP);
    metadataStatusPaths(points,DPointDisplay::aCachedDObject,DPointDisplay::inDP);
    resetMetadata(); assert(points.compute(MPlug(DPointDisplay::aNodeInfo),block));
    assert(outputInfo.find("point count: 1")!=std::string::npos && outputInfo.find("isEmpty")==std::string::npos);
    resetMetadata(); connected=false;
    assert(points.compute(MPlug(DPointDisplay::aNodeInfo),block));
    assert(outputInfo.find("not connected")!=std::string::npos);
    resetMetadata(); connected=false;
    assert(points.compute(MPlug(DPointDisplay::aCachedDObject),block) && publications==1);
    assert(block.values[DPointDisplay::aCachedDObject.id].scalar==1);
    markerOutputs(pointMarker,metadataPoints,DPMarker::inDP);
    markerOutputs(curveMarker,metadataCurves,DCMarker::inDC);
    markerOutputs(meshMarker,metadataMeshes,DMMarker::inDM);
    metadataStatusPaths(meshes,DMeshDisplay::aNodeInfo,DMeshDisplay::inDM);
    metadataStatusPaths(curves,DCurveDisplay::aNodeInfo,DCurveDisplay::inDC);
    resetMetadata(); connected=false;
    assert(curves.compute(MPlug(DCurveDisplay::aNodeInfo),block));
    assert(outputInfo.find("not connected")!=std::string::npos);
    for (const auto& operation : operations) assert(operation!="input" && operation!="data.read");
    vdbMetadata(vdb);
    metadataPoints.points.clear(); metadataCurves.curves.clear(); metadataMeshes.meshCount=0;
    emptySummary(points,DPointDisplay::aNodeInfo,"point count: 0");
    emptySummary(pointMarker,DPMarker::aNodeInfo,"point count: 0");
    emptySummary(curveMarker,DCMarker::aNodeInfo,"DCurve count: 0");
    emptySummary(meshMarker,DMMarker::aNodeInfo,"DMesh count: 0");
    emptySummary(meshes,DMeshDisplay::aNodeInfo,"DMesh count: 0");
    emptySummary(curves,DCurveDisplay::aNodeInfo,"DCurve count: 0");
    emptySummary(lights,DPLightGen::aNodeInfo,"Create light count: 0");
    metadataPoints=DPData(); metadataCurves=DCurveData(); metadataMeshes=DMeshData();
    DPInstancer instancer;
    instancerCompute(instancer);
    displayBounds(points,1.0); displayBounds(pointMarker,1.0);
    displayBounds(curves,0.5); displayBounds(curveMarker,0.5);
    displayBounds(meshes,0.5); displayBounds(meshMarker,0.5);
    displayBounds(instancer,0.5);
#ifdef DOMMY_DISPLAY_ALLOCATION_TESTS
    allMetadataFormattingFailures();
#endif
}
