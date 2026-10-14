//
//  DPCopyTransform.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/15.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPCopyTransform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointMergeBudget.h"
#include "common/utility/DPrimitiveCopyBudget.h"
#include "common/utility/DMeshCopyBudget.h"
#include "common/utility/DCurveOperationBudget.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"

#include "DPData.h"
#include "DCurveData.h"
#include "DMeshData.h"
#include "dpoint_ids.h"

#include "dommyMath.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

const MTypeId DPCopyTransform::typeId(ID_DP_COPY_TRANSFORM);

MObject DPCopyTransform::inDD;

MObject DPCopyTransform::aCount;

MObject DPCopyTransform::aTranslate;
MObject DPCopyTransform::aRotate;
MObject DPCopyTransform::aScale;
MObject DPCopyTransform::aPivot;
MObject DPCopyTransform::aUniformScale;

MObject DPCopyTransform::aRotateOrder;

MObject DPCopyTransform::aFreezeTransform;

MObject DPCopyTransform::outDD;

void* DPCopyTransform::creator() {
    return ::dommy::maya::createObject("DPCopyTransform", [&]() {
        return new DPCopyTransform();
    });
}

namespace {

struct CopyTransformRequest {
    std::size_t count = 0U;
    MVector translation, rotation, scale, pivot;
    MTransformationMatrix::RotationOrder order = MTransformationMatrix::kXYZ;
    bool freeze = false;
    std::size_t effectiveCount() const { return count == 0U ? 1U : count; }
};

MStatus readCopyTransformRequest(MDataBlock& data, CopyTransformRequest& request) {
    MStatus status;
    const MDataHandle countHandle = data.inputValue(DPCopyTransform::aCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!dommy::generation::checkedCount(countHandle.asInt(),
            dommy::generation::kMaxGeneratedTransformCount, request.count))
        return MS::kInvalidParameter;
    const MDataHandle translationHandle = data.inputValue(DPCopyTransform::aTranslate, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.translation = translationHandle.asVector();
    const MDataHandle rotationHandle = data.inputValue(DPCopyTransform::aRotate, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.rotation = rotationHandle.asVector();
    const MDataHandle scaleHandle = data.inputValue(DPCopyTransform::aScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.scale = scaleHandle.asVector();
    const MDataHandle pivotHandle = data.inputValue(DPCopyTransform::aPivot, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.pivot = pivotHandle.asVector();
    const MDataHandle uniformScaleHandle = data.inputValue(DPCopyTransform::aUniformScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double uniformScale = uniformScaleHandle.asDouble();
    const MDataHandle freezeHandle = data.inputValue(DPCopyTransform::aFreezeTransform, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.freeze = freezeHandle.asBool();
    const MDataHandle rotateOrderHandle = data.inputValue(DPCopyTransform::aRotateOrder, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short rotateOrder = rotateOrderHandle.asShort();

    if (!dommy::maya::isFiniteVector(request.translation) ||
        !dommy::maya::isFiniteVector(request.rotation) ||
        !dommy::maya::isFiniteVector(request.scale) ||
        !dommy::maya::isFiniteVector(request.pivot) || !std::isfinite(uniformScale))
        return MS::kInvalidParameter;
    request.scale = request.scale * uniformScale;
    if (!dommy::maya::isFiniteVector(request.scale)) return MS::kInvalidParameter;
    switch (rotateOrder) {
    case ROTATE_ORDER_XYZ: request.order = MTransformationMatrix::kXYZ; break;
    case ROTATE_ORDER_YZX: request.order = MTransformationMatrix::kYZX; break;
    case ROTATE_ORDER_ZXY: request.order = MTransformationMatrix::kZXY; break;
    case ROTATE_ORDER_XZY: request.order = MTransformationMatrix::kXZY; break;
    case ROTATE_ORDER_YXZ: request.order = MTransformationMatrix::kYXZ; break;
    case ROTATE_ORDER_ZYX: request.order = MTransformationMatrix::kZYX; break;
    default: return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

// Keep the historical object-space operation order and translated pivot.
// accumulatedScale and output change only after a complete finite transform.
MStatus buildCopyTransform(const CopyTransformRequest& request, std::size_t index,
                           MVector& accumulatedScale, MMatrix& output) {
    const MVector move = request.translation * static_cast<double>(index);
    const MVector pivot = move + request.pivot;
    const MVector scale(accumulatedScale[0] * request.scale[0],
                        accumulatedScale[1] * request.scale[1],
                        accumulatedScale[2] * request.scale[2]);
    const MVector rotation(
        dommy::math::degreesToRadians(request.rotation[0]) * static_cast<double>(index),
        dommy::math::degreesToRadians(request.rotation[1]) * static_cast<double>(index),
        dommy::math::degreesToRadians(request.rotation[2]) * static_cast<double>(index));
    if (!dommy::maya::isFiniteVector(move) || !dommy::maya::isFiniteVector(pivot) ||
        !dommy::maya::isFiniteVector(scale) || !dommy::maya::isFiniteVector(rotation))
        return MS::kInvalidParameter;
    MTransformationMatrix transform;
    DOMMY_RETURN_IF_FAILURE(transform.setRotatePivot(pivot, MSpace::kObject, false));
    DOMMY_RETURN_IF_FAILURE(transform.setScalePivot(pivot, MSpace::kObject, false));
    const double scaleValues[3] = {scale.x, scale.y, scale.z};
    DOMMY_RETURN_IF_FAILURE(transform.addScale(scaleValues, MSpace::kObject));
    const double rotationValues[3] = {rotation.x, rotation.y, rotation.z};
    DOMMY_RETURN_IF_FAILURE(transform.addRotation(rotationValues, request.order, MSpace::kObject));
    DOMMY_RETURN_IF_FAILURE(transform.addTranslation(move, MSpace::kObject));
    const MMatrix candidate = transform.asMatrix();
    if (!dommy::maya::isFiniteMatrix(candidate)) return MS::kInvalidParameter;
    accumulatedScale = scale;
    output = candidate;
    return MS::kSuccess;
}

template <typename Point>
bool transformCopyPoint(Point& point, const MMatrix& matrix) {
    using Scalar = typename std::decay<decltype(point[0])>::type;
    std::array<Scalar, 3> value;
    if (!dommy::numeric::checkedTransformPoint(point, matrix, value)) return false;
    point = Point(value[0], value[1], value[2]);
    return true;
}

// Both containers own one clone per output primitive and repeat collection
// rows/groups in one transaction. Freeze applies only to later copy batches.
template <typename Data, typename Freeze>
MStatus copyTransformedPrimitives(const Data& input, const CopyTransformRequest& request,
                                 Data& output, Freeze freeze) {
    MVector scale(1.0, 1.0, 1.0);
    MMatrix delta;
    using Item = typename std::decay<decltype(*input.beginPrims())>::type;
    return output.copyRepeatedFrom(input, request.effectiveCount(),
        [&](typename Item::element_type& copy, std::size_t step, std::size_t index) -> MStatus {
            if (index == 0U)
                DOMMY_RETURN_IF_FAILURE(buildCopyTransform(request, step, scale, delta));
            const MTransformationMatrix original(copy.getTransformMatrix());
            const MMatrix matrix = original.asMatrix() * delta;
            if (!dommy::maya::isFiniteMatrix(matrix)) return MS::kInvalidParameter;
            if (request.freeze) {
                DOMMY_RETURN_IF_FAILURE(freeze(copy, matrix));
            } else {
                copy.setTransformMatrix(matrix);
            }
            return MS::kSuccess;
        });
}

} // namespace

MStatus DPCopyTransform::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDD) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDD, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTypeId inputType = inputHandle.typeId();
    CopyTransformRequest request;
    DOMMY_RETURN_IF_FAILURE(readCopyTransformRequest(data, request));

    if (inputType == DPData::id) {
        const DPData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
        if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;
        std::size_t workItems = 0U;
        if (!dommy::point::buildPointCopyBudget(*input, request.effectiveCount(), workItems))
            return MS::kInvalidParameter;
        status = dommy::maya::setPluginDataOutput<DPData>(
            data, outDD, DPData::id, [input, &request](DPData& output) -> MStatus {
                MVector scale(1.0, 1.0, 1.0);
                MMatrix matrix;
                return output.copyRepeatedFrom(*input, request.effectiveCount(),
                    [&](DPoint& point, std::size_t step, std::size_t index) -> MStatus {
                        if (step == 0U) return MS::kSuccess;
                        if (index == 0U)
                            DOMMY_RETURN_IF_FAILURE(buildCopyTransform(request, step, scale, matrix));
                        return transformCopyPoint(point, matrix) ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
                    });
            });
    } else if (inputType == DCurveData::id) {
        const DCurveData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, input));
        if (!input || !input->hasValidCurveTopology()) return MS::kInvalidParameter;
        dommy::mesh::MeshMergePlan plan;
        if (!dommy::curve::buildCurveCopyPlan(*input, request.effectiveCount(), plan))
            return MS::kInvalidParameter;
        status = dommy::maya::setPluginDataOutput<DCurveData>(
            data, outDD, DCurveData::id, [input, &request](DCurveData& output) {
                return copyTransformedPrimitives(*input, request, output,
                    [](DCurveInternalData& copy, const MMatrix& matrix) -> MStatus {
                        for (auto point = copy.beginPt(); point != copy.endPt(); ++point)
                            if (!transformCopyPoint(*point, matrix)) return MS::kInvalidParameter;
                        copy.setTransformMatrix(MMatrix::identity);
                        return MS::kSuccess;
                    });
            });
    } else if (inputType == DMeshData::id) {
        const DMeshData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DMeshData::id, input));
        if (!input || !input->hasValidMeshTopology()) return MS::kInvalidParameter;
        dommy::mesh::MeshMergePlan sourcePlan, outputPlan;
        if (!dommy::mesh::appendMeshRangePlan(input->beginMeshes(), input->endMeshes(), sourcePlan) ||
            !dommy::mesh::repeatMeshMergePlan(sourcePlan, request.effectiveCount(), outputPlan) ||
            !dommy::mesh::validMeshOperationStorage(outputPlan) ||
            !dommy::mesh::validateRepeatedMeshMetadata(*input, request.effectiveCount(), outputPlan))
            return MS::kInvalidParameter;
        status = dommy::maya::setPluginDataOutput<DMeshData>(
            data, outDD, DMeshData::id, [input, &request](DMeshData& output) {
                return copyTransformedPrimitives(*input, request, output,
                    [](DMeshInternalData& copy, const MMatrix& matrix) {
                        return copy.bakeTransform(matrix);
                    });
            });
    } else {
        return MS::kInvalidParameter;
    }
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPCopyTransform")

MStatus DPCopyTransform::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;

    // ----------------------- INPUTS --------------------------
    inDD = gAttr.create("inDD", "idd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    aCount = nAttr.create("Count", "cnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));

    aTranslate = nAttr.create("Translate", "t", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslate));

    aRotate = nAttr.create("Rotate", "rot", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotate));

    aScale = nAttr.create("Scale", "scl", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    aPivot = nAttr.create("Pivot", "pvt", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPivot));

    aUniformScale = nAttr.create("UniformScale", "uscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));

    MFnEnumAttribute rotEnumAttr;
    aRotateOrder = rotEnumAttr.create("RotateOrder", "ro", ROTATE_ORDER_XYZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("xyz", ROTATE_ORDER_XYZ));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("yzx", ROTATE_ORDER_YZX));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("zxy", ROTATE_ORDER_ZXY));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("xzy", ROTATE_ORDER_XZY));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("yxz", ROTATE_ORDER_YXZ));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("zyx", ROTATE_ORDER_ZYX));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateOrder));

    aFreezeTransform = nAttr.create("FreezeTransform", "frztrans", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFreezeTransform));

    //create custom data
    outDD = gAttr.create("outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCount, outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTranslate, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotate, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotateOrder, outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFreezeTransform, outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPivot, outDD));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPCopyTransform", "initialize", std::current_exception());
}
