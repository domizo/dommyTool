//
//  DMotionTrailCurveNode.cpp
//  dommyTool
//
//  Created by yuzo on 3/10/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMotionTrailCurveNode.h"
#include "DRandomGenerator.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMotionGenerationPlan.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGeometryData.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MTime.h>
#include <maya/MTypeId.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

MTypeId DMotionTrailCurveNode::typeId(0x0048A);

// Attributes
MObject DMotionTrailCurveNode::inputObject;
MObject DMotionTrailCurveNode::transformMatrix;

MObject DMotionTrailCurveNode::trailType;
MObject DMotionTrailCurveNode::seed;
MObject DMotionTrailCurveNode::randomCount;

MObject DMotionTrailCurveNode::trailLength;
MObject DMotionTrailCurveNode::timeSampling;
MObject DMotionTrailCurveNode::trailLengthRandom;
MObject DMotionTrailCurveNode::time;

MObject DMotionTrailCurveNode::outputObjects;

DMotionTrailCurveNode::DMotionTrailCurveNode() {}

DMotionTrailCurveNode::~DMotionTrailCurveNode() {}

void* DMotionTrailCurveNode::creator() {
    return ::dommy::maya::createObject("DMotionTrailCurveNode", [&]() {
        return new DMotionTrailCurveNode();
    });
}

namespace {

MStatus createTrailCurve(const MPoint& position, const std::vector<MMatrix>& matrices,
                         MObject& curveData) {
    if (!dommy::maya::isFinitePoint(position) || matrices.size() < 4U ||
        matrices.size() > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return MS::kFailure;
    }

    MPointArray editPoints;
    MStatus status = editPoints.setLength(static_cast<unsigned int>(matrices.size()));
    if (!status)
        return status;
    for (std::size_t index = 0U; index < matrices.size(); ++index) {
        const MPoint transformed = position * matrices[index];
        if (!dommy::maya::isFinitePoint(transformed)) {
            return MS::kFailure;
        }
        editPoints[static_cast<unsigned int>(index)] = transformed;
    }

    MFnNurbsCurveData dataCreator;
    MObject candidateData = dataCreator.create(&status);
    if (!status || candidateData.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }

    MFnNurbsCurve curveFn;
    const MObject curve = curveFn.createWithEditPoints(editPoints, 3U, MFnNurbsCurve::kOpen, false,
                                                       true, true, candidateData, &status);
    if (!status || curve.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }

    curveData = candidateData;
    return MS::kSuccess;
}

} // namespace

MStatus DMotionTrailCurveNode::compute(const MPlug& plug, MDataBlock& block) try {
    if (plug != outputObjects)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle timeHandle = block.inputValue(time, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTime evaluationTime = timeHandle.asTime();
    if (!std::isfinite(evaluationTime.value()))
        return MS::kFailure;

    const MDataHandle seedHandle = block.inputValue(seed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int seedValue = seedHandle.asInt();
    const MDataHandle randomCountHandle = block.inputValue(randomCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int randomCountValue = randomCountHandle.asInt();
    const MDataHandle trailTypeHandle = block.inputValue(trailType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int trailTypeValue = trailTypeHandle.asInt();

    double trailLengthValue = 0.0;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::readFiniteDoubleInput(block, trailLength, trailLengthValue));
    double trailLengthRandomValue = 0.0;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::readFiniteDoubleInput(block, trailLengthRandom, trailLengthRandomValue));
    const MDataHandle timeSamplingHandle = block.inputValue(timeSampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int timeSamplingValue = timeSamplingHandle.asInt();

    if (trailTypeValue < DT_TRAIL_OBJECT || trailTypeValue >= DT_TRAIL_TYPE_COUNT ||
        randomCountValue < 0 ||
        !dommy::generation::validMotionTrailControls(
            timeSamplingValue, evaluationTime.value(), trailLengthValue,
            trailLengthRandomValue)) {
        return MS::kFailure;
    }

    MDataHandle inputObjectHandle = block.inputValue(inputObject, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MFnData::Type inputType = inputObjectHandle.type();
    MFnNurbsCurve curveFn;
    MFnNurbsSurface surfaceFn;
    MFnMesh meshFn;
    std::size_t sourcePointCount = 0U;
    switch (inputType) {
    case MFnData::kNurbsCurve: {
        DOMMY_RETURN_IF_FAILURE(curveFn.setObject(inputObjectHandle.asNurbsCurve()));
        const int count = curveFn.numCVs(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!dommy::generation::checkedCount(
                count, dommy::generation::kMaxGeneratedPointCount, sourcePointCount))
            return MS::kInvalidParameter;
        break;
    }
    case MFnData::kNurbsSurface: {
        DOMMY_RETURN_IF_FAILURE(surfaceFn.setObject(inputObjectHandle.asNurbsSurface()));
        const int countU = surfaceFn.numCVsInU(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int countV = surfaceFn.numCVsInV(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t u = 0U, v = 0U;
        if (!dommy::generation::checkedCount(
                countU, dommy::generation::kMaxGeneratedPointCount, u) ||
            !dommy::generation::checkedCount(
                countV, dommy::generation::kMaxGeneratedPointCount, v) ||
            !dommy::generation::checkedProduct(
                u, v, dommy::generation::kMaxGeneratedPointCount, sourcePointCount))
            return MS::kInvalidParameter;
        break;
    }
    case MFnData::kMesh: {
        DOMMY_RETURN_IF_FAILURE(meshFn.setObject(inputObjectHandle.asMesh()));
        const int count = meshFn.numVertices(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!dommy::generation::checkedCount(
                count, dommy::generation::kMaxGeneratedPointCount, sourcePointCount))
            return MS::kInvalidParameter;
        break;
    }
    default:
        return MS::kUnknownParameter;
    }

    const std::size_t plannedCurveCount = trailTypeValue == DT_TRAIL_OBJECT ? 1U :
        trailTypeValue == DT_TRAIL_POINT ? sourcePointCount :
        std::min(static_cast<std::size_t>(randomCountValue), sourcePointCount);
    dommy::generation::MotionTrailBudget budget;
    // The source count is bounded above by kMaxGeneratedPointCount before this cast.
    if (!dommy::generation::buildMotionTrailBudget(
            static_cast<int>(plannedCurveCount), timeSamplingValue, evaluationTime.value(),
            trailLengthValue, trailLengthRandomValue, budget, sourcePointCount,
            trailTypeValue == DT_TRAIL_POINT_RANDOM)) {
        MGlobal::displayError("DMotionTrailCurveNode: invalid or excessive trail request.");
        return MS::kFailure;
    }

    // Do not retrieve positions, copy selections or allocate random indices until
    // the complete request, including source workspace, has been admitted.
    MPointArray objectPoints;
    switch (inputType) {
    case MFnData::kNurbsCurve:
        DOMMY_RETURN_IF_FAILURE(curveFn.getCVs(objectPoints, MSpace::kObject));
        break;
    case MFnData::kNurbsSurface:
        DOMMY_RETURN_IF_FAILURE(surfaceFn.getCVs(objectPoints, MSpace::kObject));
        break;
    case MFnData::kMesh:
        DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(objectPoints, MSpace::kObject));
        break;
    default:
        return MS::kUnknownParameter;
    }
    if (objectPoints.length() != sourcePointCount) return MS::kFailure;
    for (unsigned int index = 0U; index < objectPoints.length(); ++index) {
        if (!dommy::maya::isFinitePoint(objectPoints[index])) {
            return MS::kFailure;
        }
    }
    if (budget.curveCount == 0U) {
        return dommy::maya::setObjectArrayOutput(block, outputObjects, plug,
                                                 std::vector<MObject>());
    }

    dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedValue));
    MPointArray trailPositions;
    switch (trailTypeValue) {
    case DT_TRAIL_OBJECT:
        DOMMY_RETURN_IF_FAILURE(trailPositions.append(MPoint::origin));
        break;
    case DT_TRAIL_POINT:
        DOMMY_RETURN_IF_FAILURE(trailPositions.copy(objectPoints));
        break;
    case DT_TRAIL_POINT_RANDOM: {
        const std::size_t pointCount = objectPoints.length();
        const std::size_t selectionCount =
            std::min(static_cast<std::size_t>(randomCountValue), pointCount);
        std::vector<unsigned int> indices(pointCount);
        std::iota(indices.begin(), indices.end(), 0U);
        for (std::size_t index = 0U; index < selectionCount; ++index) {
            const std::size_t selected = index + randomGenerator.index(pointCount - index);
            std::swap(indices[index], indices[selected]);
            DOMMY_RETURN_IF_FAILURE(trailPositions.append(objectPoints[indices[index]]));
        }
        break;
    }
    default:
        return MS::kFailure;
    }

    if (trailPositions.length() != budget.curveCount) return MS::kFailure;

    MFnDependencyNode dependencyNode(thisMObject(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MPlug matrixPlug = dependencyNode.findPlug(transformMatrix, true, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool matrixConnected = matrixPlug.isConnected(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!matrixConnected) {
        MGlobal::displayError("DMotionTrailCurveNode: transformMatrix must be connected.");
        return MS::kFailure;
    }

    const bool perCurveLength = trailLengthRandomValue != 0.0 && budget.curveCount > 1U;
    std::vector<MMatrix> sharedMatrices;
    if (!perCurveLength) {
        double step = 0.0;
        if (!dommy::generation::motionTrailStep(trailLengthValue, budget.sampleCount, step)) {
            return MS::kFailure;
        }
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::sampleMatrixPlugSeries(sharedMatrices, matrixPlug, budget.sampleCount,
                                                evaluationTime, 0.0, step, std::vector<double>()));
    }

    std::vector<MObject> outputCurves;
    outputCurves.reserve(budget.curveCount);
    for (std::size_t curveIndex = 0U; curveIndex < budget.curveCount; ++curveIndex) {
        std::vector<MMatrix> sampledMatrices;
        const std::vector<MMatrix>* matrices = &sharedMatrices;
        if (perCurveLength) {
            const double randomLength =
                randomGenerator.uniform(trailLengthValue - trailLengthRandomValue * 0.5,
                                        trailLengthValue + trailLengthRandomValue * 0.5);
            double step = 0.0;
            if (!dommy::generation::motionTrailStep(randomLength, budget.sampleCount, step)) {
                return MS::kFailure;
            }
            DOMMY_RETURN_IF_FAILURE(dommy::maya::sampleMatrixPlugSeries(
                sampledMatrices, matrixPlug, budget.sampleCount, evaluationTime, 0.0, step,
                std::vector<double>()));
            matrices = &sampledMatrices;
        }

        MObject curveData;
        DOMMY_RETURN_IF_FAILURE(createTrailCurve(
            trailPositions[static_cast<unsigned int>(curveIndex)], *matrices, curveData));
        outputCurves.push_back(curveData);
    }

    return dommy::maya::setObjectArrayOutput(block, outputObjects, plug, outputCurves);
}
DOMMY_COMPUTE_CATCH("DMotionTrailCurveNode")

MStatus DMotionTrailCurveNode::initialize() try {
    MFnNumericAttribute nAttr;
    MFnTypedAttribute typedAttr;
    MFnUnitAttribute unitFn;
    MFnMatrixAttribute matAttr;
    MFnEnumAttribute enumFn;
    MStatus status;

    inputObject =
        typedAttr.create("inputObject", "io", MFnGeometryData::kAny, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setWritable(true));

    transformMatrix = matAttr.create("transformMatrix", "tm", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setCached(false));

    trailType = enumFn.create("trailType", "tt", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Object", DT_TRAIL_OBJECT));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Point", DT_TRAIL_POINT));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Point Random", DT_TRAIL_POINT_RANDOM));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));

    seed = nAttr.create("seed", "se", MFnNumericData::kInt, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    randomCount = nAttr.create("randomCount", "rc", MFnNumericData::kInt, 10, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    timeSampling = nAttr.create("timeSampling", "ts", MFnNumericData::kInt, 10, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    trailLength = nAttr.create("trailLength", "tl", MFnNumericData::kDouble, 5.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    trailLengthRandom =
        nAttr.create("trailLengthRandom", "tlr", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    time = unitFn.create("time", "ti", MFnUnitAttribute::kTime, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(unitFn.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(unitFn.setKeyable(true));

    outputObjects =
        typedAttr.create("outputObjects", "oos", MFnGeometryData::kAny, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setUsesArrayDataBuilder(true));

    DOMMY_RETURN_IF_FAILURE(addAttribute(inputObject));
    DOMMY_RETURN_IF_FAILURE(addAttribute(transformMatrix));
    DOMMY_RETURN_IF_FAILURE(addAttribute(trailType));
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));
    DOMMY_RETURN_IF_FAILURE(addAttribute(randomCount));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeSampling));
    DOMMY_RETURN_IF_FAILURE(addAttribute(trailLength));
    DOMMY_RETURN_IF_FAILURE(addAttribute(trailLengthRandom));
    DOMMY_RETURN_IF_FAILURE(addAttribute(time));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputObjects));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputObject, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(transformMatrix, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(trailType, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(seed, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(randomCount, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(timeSampling, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(trailLength, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(trailLengthRandom, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(time, outputObjects));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMotionTrailCurveNode", "initialize", std::current_exception());
}
