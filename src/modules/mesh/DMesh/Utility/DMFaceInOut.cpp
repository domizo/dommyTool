//
//  DMFaceInOut.cpp
//  dommyTool
//
//  Created by yuzo on 5/1/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMFaceInOut.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMeshMergePlan.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"

#include <cmath>







const MTypeId DMFaceInOut::typeId( ID_DM_FACE_IN_OUT );


MObject DMFaceInOut::inDM;

MObject DMFaceInOut::aInObject;
MObject DMFaceInOut::aInnerName;
MObject DMFaceInOut::aOuterName;

MObject DMFaceInOut::aTolerance;

MObject DMFaceInOut::outDM;



void *DMFaceInOut::creator()
{
    return ::dommy::maya::createObject("DMFaceInOut", [&]() {
        return new DMFaceInOut();
    });
}




MStatus DMFaceInOut::compute(const MPlug& plug, MDataBlock& data) try
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
        MPlug referencePlug(thisMObject(), aInObject);
        const bool connected = referencePlug.isConnected(&status);
        if (!status) return status;
        MObject referenceObject;
        if (connected) {
            MDataHandle referenceHandle = data.inputValue(aInObject, &status);
            if (!status) return status;
            if (referenceHandle.type() == MFnData::kMesh) referenceObject = referenceHandle.data();
        } else {
            EchoWarning(name().asChar(), "Not connected in object(mesh)!");
        }
        // Preserve pass-through for an absent or non-mesh reference.
        if (!referenceObject.isNull()) {
            MFnMesh reference(referenceObject, &status);
            if (!status) return status;
            const MDataHandle innerHandle = data.inputValue(aInnerName, &status);
            if (!status) return status;
            const std::string innerName = innerHandle.asString().asChar();
            const MDataHandle outerHandle = data.inputValue(aOuterName, &status);
            if (!status) return status;
            const std::string outerName = outerHandle.asString().asChar();
            const MDataHandle toleranceHandle = data.inputValue(aTolerance, &status);
            if (!status) return status;
            const double tolerance = toleranceHandle.asDouble();
            if (!std::isfinite(tolerance) || tolerance < 0.0 || innerName.empty() ||
                outerName.empty() || innerName == outerName) return MS::kInvalidParameter;
            const int referencePoints = reference.numVertices(&status);
            if (!status) return status;
            const int referenceFaces = reference.numPolygons(&status);
            if (!status) return status;
            const int referenceCorners = reference.numFaceVertices(&status);
            if (!status) return status;
            dommy::mesh::MeshMergePlan referenceBudget;
            if (referencePoints < 0 || referenceFaces < 0 || referenceCorners < 0 ||
                !dommy::mesh::appendMeshMergePlan(referencePoints, referenceFaces, referenceCorners,
                    0U, referenceBudget) || !dommy::mesh::validMeshOperationStorage(referenceBudget))
                return MS::kInvalidParameter;
            dommy::maya::MeshClosestPointQuery query;
            DOMMY_RETURN_IF_FAILURE(query.create(referenceObject));
            for (auto mesh = output->beginMeshes(); mesh != output->endMeshes(); ++mesh) {
                MObject object;
                DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmeshWithUV(**mesh, object));
                MItMeshPolygon faces(object, &status);
                if (!status) return status;
                const std::size_t count = static_cast<std::size_t>((*mesh)->numFace());
                DPIdList outerIds(count, false), innerIds(count, false);
                DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshPolygons(faces, count,
                    [&](MItMeshPolygon& face) -> MStatus {
                        MStatus stat;
                        const int id = face.index(&stat);
                        if (!stat) return stat;
                        if (id < 0 || static_cast<std::size_t>(id) >= count) return MS::kFailure;
                        const MPoint center = face.center(MSpace::kObject, &stat);
                        if (!stat) return stat;
                        if (!dommy::maya::isFinitePoint(center)) return MS::kInvalidParameter;
                        MPoint closest;
                        DOMMY_RETURN_IF_FAILURE(query.getClosestPoint(center, closest));
                        const double distance = closest.distanceTo(center);
                        if (!std::isfinite(distance)) return MS::kInvalidParameter;
                        if (distance < tolerance) outerIds[static_cast<std::size_t>(id)] = true;
                        else innerIds[static_cast<std::size_t>(id)] = true;
                        return MS::kSuccess;
                    }));
                for (const std::string& group : {innerName, outerName})
                    if ((*mesh)->isExistCompGrp(D_COMP_FACE, group) &&
                        !(*mesh)->removeCompGrp(D_COMP_FACE, group)) return MS::kFailure;
                if (!(*mesh)->addCompGrp(D_COMP_FACE, outerName, DPGroupData(outerIds)) ||
                    !(*mesh)->addCompGrp(D_COMP_FACE, innerName, DPGroupData(innerIds))) return MS::kFailure;
            }
        }
    }
    MDataHandle handle = data.outputValue(outDM, &status);
    if (!status) return status;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        handle, outputObject, DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMFaceInOut")

			



MStatus DMFaceInOut::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aInObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));

    const MObject aInnerNameDefault = fnStringData.create("inner", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aInnerName = tAttr.create("InnerName", "innm", MFnData::kString, aInnerNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInnerName));

    const MObject aOuterNameDefault = fnStringData.create("outer", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOuterName = tAttr.create("OuterName", "outnm", MFnData::kString, aOuterNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOuterName));

    aTolerance = nAttr.create( "Tolerance", "tol", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInnerName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOuterName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTolerance, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMFaceInOut", "initialize", std::current_exception());
}
