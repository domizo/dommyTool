//
//  DMEdgeDisplace.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/24.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMEdgeDisplace.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaValueValidation.h"
#include <cmath>




#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"




const MTypeId DMEdgeDisplace::typeId( ID_DM_EDGE_DISPLACE );



MObject DMEdgeDisplace::inDM;


MObject DMEdgeDisplace::aDisplace;

MObject DMEdgeDisplace::aDisplaceAttribute;
MObject DMEdgeDisplace::aVectorAttribute;
MObject DMEdgeDisplace::aFaceGroup;
MObject DMEdgeDisplace::aIsConcernCorner;
MObject DMEdgeDisplace::aCheckMinLength;


MObject DMEdgeDisplace::outDM;


void *DMEdgeDisplace::creator() {
    return ::dommy::maya::createObject("DMEdgeDisplace", [&]() {
        return new DMEdgeDisplace();
    });
}


MStatus DMEdgeDisplace::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    MStatus status;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    if (!input) return MS::kFailure;
    MObject outputObject;
    DMeshData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    if (output->numberOfMesh() != 0U) {
        const MDataHandle groupHandle = data.inputValue(aFaceGroup, &status);
        if (!status) return status;
        const std::string outerName = groupHandle.asString().asChar();
        const MDataHandle vectorHandle = data.inputValue(aVectorAttribute, &status);
        if (!status) return status;
        const std::string vecName = vectorHandle.asString().asChar();
        const MDataHandle attributeHandle = data.inputValue(aDisplaceAttribute, &status);
        if (!status) return status;
        const std::string dispName = attributeHandle.asString().asChar();
        const MDataHandle displaceHandle = data.inputValue(aDisplace, &status);
        if (!status) return status;
        const float displace = displaceHandle.asFloat();
        const MDataHandle cornerHandle = data.inputValue(aIsConcernCorner, &status);
        if (!status) return status;
        const bool isCorner = cornerHandle.asBool();
        const MDataHandle lengthHandle = data.inputValue(aCheckMinLength, &status);
        if (!status) return status;
        const bool checkMinLength = lengthHandle.asBool();
        if (!std::isfinite(displace)) return MS::kInvalidParameter;

        for (auto mesh = output->beginMeshes(); mesh != output->endMeshes(); ++mesh) {
            if (!(*mesh)->isExistCompGrp(D_COMP_FACE, outerName)) continue;
            const DPGroupData::ConstPtr group = (*mesh)->getCompGrp(D_COMP_FACE, outerName);
            if (!group || group->getGroupIds().size() != static_cast<std::size_t>((*mesh)->numFace()))
                return MS::kFailure;
            if (!group->isExistActive()) continue;
            const DPIdList& groupIds = group->getGroupIds();
            MObject object;
            DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmeshWithUV(**mesh, object));
            MFnMesh meshFn(object, &status);
            if (!status) return status;
            const int edgeCount = meshFn.numEdges(&status);
            if (!status) return status;
            if (edgeCount < 0 || static_cast<std::size_t>(edgeCount) > (*mesh)->numberOfConnections())
                return MS::kFailure;
            MItMeshVertex vertices(object, &status);
            if (!status) return status;
            MItMeshEdge edges(object, &status);
            if (!status) return status;

            dommy::geometry::StagedPositionEdits<DMeshInternalData> positionEdit;
            if (!positionEdit.add(**mesh)) return MS::kFailure;
            DPointList& points = positionEdit.positions(0U);
            std::vector<Imath::V3d> directions(points.size(), Imath::V3d(0.0));
            std::vector<double> displacements(points.size(), displace);
            // Preserve the legacy fallback for missing/unsupported attribute
            // types, while checking every supported conversion and its size.
            if ((*mesh)->isExistAttr(vecName)) {
                const auto attribute = (*mesh)->getAttr(vecName);
                if (!attribute) return MS::kFailure;
                if (isVector3DAttrType(attribute->getAttrType()) &&
                    !dommy::attribute::convertVector3Attribute(attribute, points.size(), directions))
                    return MS::kInvalidParameter;
            }
            if ((*mesh)->isExistAttr(dispName)) {
                const auto attribute = (*mesh)->getAttr(dispName);
                if (!attribute) return MS::kFailure;
                if (isSingleRealAttrType(attribute->getAttrType()) &&
                    !dommy::attribute::convertScalarAttribute(attribute, points.size(), displacements))
                    return MS::kInvalidParameter;
            }
            if (directions.size() != points.size() || displacements.size() != points.size())
                return MS::kFailure;

            DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(vertices, points.size(),
                [&](MItMeshVertex& vertex) -> MStatus {
                    MStatus stat;
                    const int id = vertex.index(&stat);
                    if (!stat) return stat;
                    if (id < 0 || static_cast<std::size_t>(id) >= points.size()) return MS::kFailure;
                    MIntArray faceIds;
                    DOMMY_RETURN_IF_FAILURE(vertex.getConnectedFaces(faceIds));
                    unsigned int groupedCount = 0U;
                    for (unsigned int f = 0U; f < faceIds.length(); ++f) {
                        const int face = faceIds[f];
                        if (face < 0 || static_cast<std::size_t>(face) >= groupIds.size()) return MS::kFailure;
                        if (groupIds[static_cast<std::size_t>(face)]) ++groupedCount;
                    }
                    if ((isCorner && faceIds.length() == 3U) || groupedCount == 0U ||
                        groupedCount == faceIds.length()) return MS::kSuccess;
                    const MPoint point = DPToMP(points[static_cast<std::size_t>(id)]);
                    const Imath::V3d& vector = directions[static_cast<std::size_t>(id)];
                    MVector direction(vector[0], vector[1], vector[2]);
                    if (!std::isfinite(direction.length())) return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(direction.normalize());
                    MVector deformation = direction * displacements[static_cast<unsigned int>(id)];
                    const double originalLength = deformation.length();
                    if (!std::isfinite(originalLength)) return MS::kInvalidParameter;
                    if (checkMinLength) {
                        MIntArray edgeIds;
                        DOMMY_RETURN_IF_FAILURE(vertex.getConnectedEdges(edgeIds));
                        double length = originalLength;
                        for (unsigned int e = 0U; e < edgeIds.length(); ++e) {
                            if (edgeIds[e] < 0 || edgeIds[e] >= edgeCount) return MS::kFailure;
                            int previous = 0;
                            DOMMY_RETURN_IF_FAILURE(edges.setIndex(edgeIds[e], previous));
                            double edgeLength = 0.0;
                            DOMMY_RETURN_IF_FAILURE(edges.getLength(edgeLength));
                            if (!std::isfinite(edgeLength) || edgeLength < 0.0) return MS::kFailure;
                            if (length > edgeLength) length = edgeLength;
                        }
                        if (length < originalLength) {
                            DOMMY_RETURN_IF_FAILURE(deformation.normalize());
                            deformation *= length;
                        }
                    }
                    const MPoint candidate = point + deformation;
                    if (!dommy::maya::isFloatRepresentablePoint(candidate)) return MS::kInvalidParameter;
                    points[static_cast<std::size_t>(id)] = MPToDP(candidate);
                    return MS::kSuccess;
                }));
            if (!positionEdit.commit()) return MS::kFailure;
        }
        output->updateBBox();
    }
    MDataHandle handle = data.outputValue(outDM, &status);
    if (!status) return status;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        handle, outputObject, DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMEdgeDisplace")






MStatus DMEdgeDisplace::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aDisplace = nAttr.create( "Displace", "dsp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplace));

    const MObject aDisplaceAttributeDefault = fnStringData.create("disp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDisplaceAttribute = tAttr.create("DisplaceAttribute", "dspattr", MFnData::kString, aDisplaceAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplaceAttribute));

    const MObject aVectorAttributeDefault = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttribute = tAttr.create("VectorAttribute", "vattr", MFnData::kString, aVectorAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttribute));

    const MObject aFaceGroupDefault = fnStringData.create("outer", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFaceGroup = tAttr.create("FaceGroup", "fcattr", MFnData::kString, aFaceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceGroup));

    aIsConcernCorner = nAttr.create( "IsConcernCorner", "isconcor", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsConcernCorner));

    aCheckMinLength = nAttr.create( "CheckMinLength", "chkminlen", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCheckMinLength));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDisplace, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDisplaceAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVectorAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFaceGroup, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsConcernCorner, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCheckMinLength, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMEdgeDisplace", "initialize", std::current_exception());
}
