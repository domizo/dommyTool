//
//  DMBoolean.cpp
//  dommyTool
//
//  Maya mesh Boolean adapter. The former exact-volume implementation was
//  retired with the optional computational-geometry dependency.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMBoolean.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DMeshDataHeaders.h"

#include <cstddef>

namespace {

bool accumulateMeshBudget(
    const DMeshInternalData& mesh,
    std::size_t& pointCount,
    std::size_t& faceCount,
    std::size_t& connectionCount)
{
    std::size_t nextPointCount = 0U;
    std::size_t nextFaceCount = 0U;
    std::size_t nextConnectionCount = 0U;
    if (!dommy::generation::checkedSum(
            pointCount, mesh.numberOfPoints(),
            dommy::generation::kMaxGeneratedPointCount,
            nextPointCount) ||
        !dommy::generation::checkedSum(
            faceCount, static_cast<std::size_t>(mesh.numFace()),
            dommy::generation::kMaxGeneratedFaceCount,
            nextFaceCount) ||
        !dommy::generation::checkedSum(
            connectionCount,
            static_cast<std::size_t>(
                mesh.getConnectList().length()),
            dommy::generation::kMaxWorkItemCount,
            nextConnectionCount)) {
        return false;
    }
    pointCount = nextPointCount;
    faceCount = nextFaceCount;
    connectionCount = nextConnectionCount;
    return true;
}

bool hasBoundedGeometry(const DMeshData& meshes)
{
    std::size_t pointCount = 0U;
    std::size_t faceCount = 0U;
    std::size_t connectionCount = 0U;
    for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
         mesh != meshes.endMeshes(); ++mesh) {
        if (!*mesh || !(*mesh)->hasValidMeshData() ||
            !accumulateMeshBudget(
                *mesh->get(), pointCount, faceCount,
                connectionCount)) {
            return false;
        }
    }
    return true;
}

MFnMesh::BoolOperation booleanOperation(short operation, bool& valid)
{
    valid = true;
    switch (operation) {
        case 0:
            return MFnMesh::kUnion;
        case 1:
            return MFnMesh::kDifference;
        case 2:
            return MFnMesh::kIntersection;
        default:
            valid = false;
            return MFnMesh::kUnion;
    }
}

MStatus appendBooleanResult(
    const DMeshInternalData& source,
    const DMeshData& targets,
    MFnMesh::BoolOperation operation,
    DMeshData& output,
    std::size_t& pointCount,
    std::size_t& faceCount,
    std::size_t& connectionCount)
{
    MObject resultObject;
    MStatus status =
        generateMMeshFromDmeshWithUV(
            source, resultObject, DMeshPointSpace::Transformed);
    if (!status) return status;

    MFnMesh resultFunction(resultObject, &status);
    if (!status) return status;

    for (DMeshData::ConstDMeshIter target = targets.beginMeshes();
         target != targets.endMeshes(); ++target) {
        if (!*target || !(*target)->hasValidMeshData()) {
            return MS::kInvalidParameter;
        }

        MObject targetObject;
        status = generateMMeshFromDmeshWithUV(
            *target->get(), targetObject, DMeshPointSpace::Transformed);
        if (!status) return status;

        MObjectArray operands;
        if (!operands.append(resultObject) ||
            !operands.append(targetObject)) {
            return MS::kFailure;
        }

        status = resultFunction.booleanOps(
            operation, operands, true);
        if (!status) return status;
    }

    DMItem converted;
    status = DMeshInternalData::createFromMayaObject(
        resultObject, converted);
    if (!status) return status;
    if (!converted || !converted->hasValidMeshData() ||
        !accumulateMeshBudget(
            *converted, pointCount, faceCount, connectionCount) ||
        !output.insert(converted)) {
        return MS::kFailure;
    }
    return MS::kSuccess;
}

MStatus publishOutput(MDataBlock& data,
                      const MPlug& plug,
                      const MObject& object)
{
    MStatus status;
    MDataHandle output =
        data.outputValue(DMBoolean::outDM, &status);
    if (!status) return status;
    status = output.set(object);
    if (!status) return status;
    output.setClean();
    return data.setClean(plug);
}

} // namespace

const MTypeId DMBoolean::typeId(ID_DM_BOOLEAN);

MObject DMBoolean::inDM;
MObject DMBoolean::targetDM;
MObject DMBoolean::aOperation;
MObject DMBoolean::aMethodType;
MObject DMBoolean::outDM;

void* DMBoolean::creator()
{
    return ::dommy::maya::createObject("DMBoolean", [&]() {
        return new DMBoolean();
    });
}

MStatus DMBoolean::compute(const MPlug& plug,
                           MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle methodHandle =
        data.inputValue(aMethodType, &status);
    if (!status) return status;
    const short method = methodHandle.asShort();
    if (method == 1) {
        EchoWarning(
            name().asChar(),
            "Volume Boolean was retired. Select Maya Internal instead.");
        return MS::kNotImplemented;
    }
    if (method != 0) {
        EchoWarning(name().asChar(), "Unknown Boolean method.");
        return MS::kInvalidParameter;
    }

    MDataHandle operationHandle =
        data.inputValue(aOperation, &status);
    if (!status) return status;
    bool validOperation = false;
    const MFnMesh::BoolOperation operation =
        booleanOperation(operationHandle.asShort(), validOperation);
    if (!validOperation) {
        EchoWarning(name().asChar(), "Unknown Boolean operation.");
        return MS::kInvalidParameter;
    }

    const DMeshData* input = nullptr;
    status = getValidInputDM(inDM, data, input);
    if (!status) {
        EchoWarning(name().asChar(), "Input mesh data is invalid.");
        return status;
    }
    const DMeshData* targets = nullptr;
    status = getValidInputDM(targetDM, data, targets);
    if (!status) {
        EchoWarning(name().asChar(), "Target mesh data is invalid.");
        return status;
    }

    if (input->numberOfMesh() >
            dommy::generation::kMaxConnectedGeometryCount ||
        targets->numberOfMesh() >
            dommy::generation::kMaxConnectedGeometryCount) {
        EchoWarning(name().asChar(),
                    "Boolean input exceeds the geometry safety budget.");
        return MS::kFailure;
    }
    std::size_t booleanWork = 0U;
    if (input->numberOfMesh() != 0U &&
        targets->numberOfMesh() != 0U &&
        !dommy::generation::checkedProduct(
            input->numberOfMesh(), targets->numberOfMesh(),
            dommy::generation::kMaxWorkItemCount,
            booleanWork)) {
        EchoWarning(name().asChar(),
                    "Boolean operation count exceeds the work budget.");
        return MS::kFailure;
    }
    (void)booleanWork;

    MObject outputObject;
    DMeshData* output = nullptr;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, output);
    if (!status) return status;
    if (!output) return MS::kFailure;

    if (input->numberOfMesh() == 0U) {
        if (!hasBoundedGeometry(*targets)) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(output->copyFrom(*targets));
    } else if (targets->numberOfMesh() == 0U) {
        if (!hasBoundedGeometry(*input)) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    } else {
        std::size_t pointCount = 0U;
        std::size_t faceCount = 0U;
        std::size_t connectionCount = 0U;
        for (DMeshData::ConstDMeshIter source = input->beginMeshes();
             source != input->endMeshes(); ++source) {
            if (!*source || !(*source)->hasValidMeshData()) {
                return MS::kInvalidParameter;
            }
            status = appendBooleanResult(
                *source->get(), *targets, operation, *output,
                pointCount, faceCount, connectionCount);
            if (!status) {
                EchoWarning(
                    name().asChar(),
                    "Maya mesh Boolean failed. Check for closed, "
                    "manifold input meshes.");
                return status;
            }
        }
    }

    if (!output->hasValidMeshTopology()) return MS::kFailure;
    output->updateBBox();
    return publishOutput(data, plug, outputObject);
}
DOMMY_COMPUTE_CATCH("DMBoolean")

MStatus DMBoolean::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    targetDM = typedAttribute.create(
        "targetDM", "tdm", DMeshData::id,
        MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetDM));

    MFnEnumAttribute operationAttribute;
    aOperation = operationAttribute.create(
        "Operation", "op", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        operationAttribute.addField("Add", 0));
    DOMMY_RETURN_IF_FAILURE(
        operationAttribute.addField("Subtract", 1));
    DOMMY_RETURN_IF_FAILURE(
        operationAttribute.addField("Multiply", 2));
    DOMMY_RETURN_IF_FAILURE(operationAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOperation));

    MFnEnumAttribute methodAttribute;
    aMethodType = methodAttribute.create(
        "MethodType", "mtty", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        methodAttribute.addField("Maya Internal", 0));
    DOMMY_RETURN_IF_FAILURE(
        methodAttribute.addField("Retired Volume", 1));
    DOMMY_RETURN_IF_FAILURE(methodAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMethodType));

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id,
        MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(targetDM, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aOperation, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aMethodType, outDM));

    return MS::kSuccess;
}
