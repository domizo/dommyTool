#include "common/utility/DMayaEntryPointGuard.h"
#include "polyBlastNode.h"

#include "polyBlastSelection.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshSelection.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>

#include <cstddef>
#include <vector>

namespace {

MStatus readSelectedFaces(MDataBlock& data,
                          const MObject& componentAttribute,
                          int polygonCount,
                          MIntArray& faceIds)
{
    MStatus status;
    MDataHandle componentHandle =
        data.inputValue(componentAttribute, &status);
    if (!status) return status;

    const MObject componentData = componentHandle.data();
    if (componentData.isNull()) {
        return MS::kSuccess;
    }

    MFnComponentListData componentList(componentData, &status);
    if (!status) return status;
    const unsigned int componentCount = componentList.length(&status);
    if (!status) return status;
    if (static_cast<std::size_t>(componentCount) >
        dommy::generation::kMaxConnectedGeometryCount) {
        return MS::kInvalidParameter;
    }

    std::vector<int> selectedFaces;
    selectedFaces.reserve(static_cast<std::size_t>(componentCount));
    for (unsigned int componentIndex = 0;
         componentIndex < componentCount; ++componentIndex) {
        const MObject component = componentList[componentIndex];
        if (component.isNull()) {
            continue;
        }
        if (!component.hasFn(MFn::kMeshPolygonComponent)) {
            return MS::kInvalidParameter;
        }

        std::vector<int> componentFaces;
        status = dommy::poly_blast::readPolygonFaceComponent(
            component,
            static_cast<std::size_t>(polygonCount),
            dommy::generation::kMaxGeneratedFaceCount,
            componentFaces);
        if (!status) return status;
        if (componentFaces.size() >
            dommy::generation::kMaxGeneratedFaceCount -
                selectedFaces.size()) {
            return MS::kInvalidParameter;
        }
        selectedFaces.insert(
            selectedFaces.end(), componentFaces.begin(), componentFaces.end());
    }

    std::vector<int> normalizedFaces;
    if (!dommy::mesh::normalizeFaceSelection(
            selectedFaces,
            static_cast<std::size_t>(polygonCount),
            dommy::generation::kMaxGeneratedFaceCount,
            normalizedFaces)) {
        return MS::kInvalidParameter;
    }
    return dommy::poly_blast::copyFaceIdsToMaya(
        normalizedFaces, faceIds);
}

MStatus buildPercentageFaces(int polygonCount,
                             double deletePercent,
                             MIntArray& faceIds)
{
    if (polygonCount < 0) {
        return MS::kInvalidParameter;
    }

    std::vector<int> selectedFaces;
    if (!dommy::mesh::buildPercentageFaceSelection(
            static_cast<std::size_t>(polygonCount),
            deletePercent,
            dommy::generation::kMaxGeneratedFaceCount,
            selectedFaces)) {
        return MS::kInvalidParameter;
    }
    return dommy::poly_blast::copyFaceIdsToMaya(
        selectedFaces, faceIds);
}

} // namespace

MTypeId polyBlastNode::id(0x34567);
MObject polyBlastNode::faceList;
MObject polyBlastNode::deletePercent;

MStatus polyBlastNode::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    MDataHandle stateHandle = data.inputValue(state, &status);
    if (!status) return status;
    MDataHandle input = data.inputValue(inMesh, &status);
    if (!status) return status;
    const MObject inputMesh = input.asMesh();
    if (inputMesh.isNull()) {
        return MS::kInvalidParameter;
    }

    if (stateHandle.asShort() == 1) {
        MDataHandle output = data.outputValue(outMesh, &status);
        if (!status) return status;
        status = output.set(inputMesh);
        if (!status) return status;
        output.setClean();
        return data.setClean(plug);
    }

    const MDataHandle percentHandle = data.inputValue(deletePercent, &status);
    if (!status) return status;
    const double percent = percentHandle.asDouble();

    MFnMeshData meshDataFunction;
    const MObject outputData = meshDataFunction.create(&status);
    if (!status) return status;
    if (outputData.isNull()) return MS::kFailure;

    MFnMesh meshFunction;
    const MObject copiedMesh =
        meshFunction.copy(inputMesh, outputData, &status);
    if (!status) return status;
    if (copiedMesh.isNull()) return MS::kFailure;

    const int polygonCount = meshFunction.numPolygons(&status);
    if (!status || polygonCount < 0 ||
        static_cast<std::size_t>(polygonCount) >
            dommy::generation::kMaxGeneratedFaceCount) {
        return MS::kInvalidParameter;
    }

    MIntArray faceIds;
    status = readSelectedFaces(data, faceList, polygonCount, faceIds);
    if (!status) {
        MGlobal::displayError(
            "polyBlastNode: inputComponents contains invalid polygon faces.");
        return status;
    }
    if (faceIds.length() == 0U) {
        status = buildPercentageFaces(polygonCount, percent, faceIds);
        if (!status) {
            MGlobal::displayError(
                "polyBlastNode: deletePercent must be finite and in [0, 100].");
            return status;
        }
    }

    if (faceIds.length() != 0U) {
        pBlastFactory.setMesh(copiedMesh);
        pBlastFactory.setFaces(faceIds);
        status = pBlastFactory.doIt();
        if (!status) return status;
    }

    MDataHandle output = data.outputValue(outMesh, &status);
    if (!status) return status;
    status = output.set(outputData);
    if (!status) return status;
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("polyBlastNode")

void* polyBlastNode::creator()
{
    return ::dommy::maya::createObject("polyBlastNode", [&]() {
        return new polyBlastNode();
    });
}

MStatus polyBlastNode::initialize()
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    deletePercent = numericAttribute.create(
        "deletePercent", "dp", MFnNumericData::kDouble,
        dommy::poly_blast::kDefaultDeletePercent, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = numericAttribute.setMax(100.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;

    MFnTypedAttribute typedAttribute;
    faceList = typedAttribute.create(
        "inputComponents", "ics", MFnComponentListData::kComponentList,
        MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(true);
    if (!status) return status;

    inMesh = typedAttribute.create(
        "inMesh", "im", MFnMeshData::kMesh, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(true);
    if (!status) return status;

    outMesh = typedAttribute.create(
        "outMesh", "om", MFnMeshData::kMesh, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;

    status = addAttribute(deletePercent);
    if (!status) return status;
    status = addAttribute(faceList);
    if (!status) return status;
    status = addAttribute(inMesh);
    if (!status) return status;
    status = addAttribute(outMesh);
    if (!status) return status;

    status = attributeAffects(inMesh, outMesh);
    if (!status) return status;
    status = attributeAffects(faceList, outMesh);
    if (!status) return status;
    return attributeAffects(deletePercent, outMesh);
}
