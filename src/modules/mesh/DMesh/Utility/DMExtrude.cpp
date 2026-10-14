//
//  DMExtrude.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/24.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMExtrude.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshMergePlan.h"
#include <cmath>
#include <utility>
#include <vector>



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"




const MTypeId DMExtrude::typeId( ID_DM_EXTRUDE );

MObject DMExtrude::inDM;

MObject DMExtrude::aThickNess;
MObject DMExtrude::aOffset;

MObject DMExtrude::aExtrudeTogether;
MObject DMExtrude::aExtrusionCount;


MObject DMExtrude::aFaceGroup;

MObject DMExtrude::outDM;


void *DMExtrude::creator() {
    return ::dommy::maya::createObject("DMExtrude", [&]() {
        return new DMExtrude();
    });
}





MStatus DMExtrude::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* inputMeshes = nullptr;
    MStatus stat = getValidInputDM(inDM, data, inputMeshes);
    if (!stat) return stat;
    if (!inputMeshes) return MS::kFailure;
    const std::size_t meshCount = inputMeshes->numberOfMesh();
    std::vector<std::vector<int>> selections;
    dommy::mesh::MeshMergePlan planned;
    float thickness = 0.0f, offset = 0.0f;
    bool faceTogether = false;
    int extdiv = 1;
    if (meshCount != 0U) {
        const MDataHandle thicknessHandle = data.inputValue(aThickNess, &stat);
        if (!stat) return stat;
        thickness = thicknessHandle.asFloat();
        const MDataHandle offsetHandle = data.inputValue(aOffset, &stat);
        if (!stat) return stat;
        offset = offsetHandle.asFloat();
        const MDataHandle togetherHandle = data.inputValue(aExtrudeTogether, &stat);
        if (!stat) return stat;
        faceTogether = togetherHandle.asBool();
        const MDataHandle divisionsHandle = data.inputValue(aExtrusionCount, &stat);
        if (!stat) return stat;
        extdiv = divisionsHandle.asInt();
        const MDataHandle groupHandle = data.inputValue(aFaceGroup, &stat);
        if (!stat) return stat;
        const std::string faceGrpName = groupHandle.asString().asChar();
        if (!std::isfinite(thickness) || !std::isfinite(offset) || extdiv < 1)
            return MS::kInvalidParameter;
        selections.reserve(meshCount);
        // Preflight every primitive before copying or invoking Maya extrusion.
        for (auto mesh = inputMeshes->beginMeshes(); mesh != inputMeshes->endMeshes(); ++mesh) {
            const MIntArray faces = (*mesh)->getFaceList();
            if (faces.length() != static_cast<unsigned int>((*mesh)->numFace())) return MS::kFailure;
            DPGroupData::ConstPtr group;
            if (!faceGrpName.empty() && (*mesh)->isExistCompGrp(D_COMP_FACE, faceGrpName))
                group = (*mesh)->getCompGrp(D_COMP_FACE, faceGrpName);
            if (group && group->getGroupIds().size() != faces.length()) return MS::kFailure;
            std::vector<int> selected;
            std::size_t selectedCorners = 0U;
            for (unsigned int face = 0U; face < faces.length(); ++face) {
                if (!group || group->getGroupIds()[face]) {
                    if (!dommy::generation::checkedSum(selectedCorners,
                            static_cast<std::size_t>(faces[face]),
                            dommy::generation::kMaxWorkItemCount, selectedCorners)) return MS::kInvalidParameter;
                    selected.push_back(static_cast<int>(face));
                }
            }
            if (!dommy::mesh::appendExtrusionPlan((*mesh)->numberOfPoints(), faces.length(),
                    (*mesh)->numberOfConnections(), (*mesh)->numberOfUVs(),
                    selectedCorners, extdiv, planned)) return MS::kInvalidParameter;
            selections.push_back(std::move(selected));
        }
    }
    MObject outputObject;
    DMeshData* outputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, outputMeshes));
    if (!outputMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));
    std::size_t meshIndex = 0U;
    dommy::mesh::MeshMergePlan actual;
    for (auto mesh = outputMeshes->beginMeshes(); mesh != outputMeshes->endMeshes(); ++mesh, ++meshIndex) {
        MObject object;
        DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmeshWithUV(**mesh, object));
        MFnMesh meshFn(object, &stat);
        if (!stat) return stat;
        MIntArray selectedFaces;
        const auto& selected = selections.at(meshIndex);
        DOMMY_RETURN_IF_FAILURE(selectedFaces.setLength(static_cast<unsigned int>(selected.size())));
        if (selectedFaces.length() != selected.size()) return MS::kFailure;
        for (unsigned int i = 0U; i < selectedFaces.length(); ++i) selectedFaces[i] = selected[i];
        if (selectedFaces.length() != 0U)
            DOMMY_RETURN_IF_FAILURE(meshFn.extrudeFaces(selectedFaces, extdiv, nullptr,
                faceTogether, thickness, offset));
        // Check returned counts before the replacement adapter acquires arrays.
        const int points = meshFn.numVertices(&stat);
        if (!stat) return stat;
        const int faces = meshFn.numPolygons(&stat);
        if (!stat) return stat;
        const int corners = meshFn.numFaceVertices(&stat);
        if (!stat) return stat;
        if (points < 0 || faces < 0 || corners < 0 ||
            !dommy::mesh::appendMeshMergePlan(static_cast<std::size_t>(points),
                static_cast<std::size_t>(faces), static_cast<std::size_t>(corners), 0U, actual) ||
            !dommy::mesh::validMeshOperationStorage(actual) || actual.points > planned.points ||
            actual.faces > planned.faces || actual.connections > planned.connections)
            return MS::kInvalidParameter;
        if (!(*mesh)->setReplaceMeshData(object)) return MS::kFailure;
    }
    if (meshCount != 0U) outputMeshes->updateBBox();
    MDataHandle output = data.outputValue(outDM, &stat);
    if (!stat) return stat;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, outputObject, DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMExtrude")





MStatus DMExtrude::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    aThickNess = nAttr.create( "ThickNess", "thk", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aThickNess));

    aOffset = nAttr.create( "Offset", "off", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    
    aExtrudeTogether = nAttr.create( "ExtrudeTogether", "exto", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExtrudeTogether));

    aExtrusionCount = nAttr.create( "ExtrusionCount", "excnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExtrusionCount));
    
    
    
    const MObject aFaceGroupDefault = fnStringData.create("outer", &stat);
    
    
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    
    aFaceGroup = tAttr.create("FaceGroup", "fcattr", MFnData::kString, aFaceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceGroup));
    
    
    
    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aThickNess, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOffset, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExtrudeTogether, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExtrusionCount, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFaceGroup, outDM ));
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMExtrude", "initialize", std::current_exception());
}
