//
//  DTDelayTransformMatrix.cpp
//  dommyTool
//
//  Created by yuzo on 3/27/15.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DTDelayTransformMatrix.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MTime.h>
#include <maya/MTypeId.h>

#include <cmath>
#include <vector>

MTypeId DTDelayTransformMatrix::typeId(0x0049A);

MObject DTDelayTransformMatrix::transformMatrix;
MObject DTDelayTransformMatrix::delay;
MObject DTDelayTransformMatrix::time;
MObject DTDelayTransformMatrix::outputTransformMatrix;

DTDelayTransformMatrix::DTDelayTransformMatrix() {}

DTDelayTransformMatrix::~DTDelayTransformMatrix() {}

void* DTDelayTransformMatrix::creator() {
    return ::dommy::maya::createObject("DTDelayTransformMatrix", [&]() {
        return new DTDelayTransformMatrix();
    });
}

MStatus DTDelayTransformMatrix::compute(const MPlug& plug, MDataBlock& block) try {
    if (plug != outputTransformMatrix)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle timeHandle = block.inputValue(time, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTime evaluationTime = timeHandle.asTime();
    if (!std::isfinite(evaluationTime.value()))
        return MS::kFailure;

    double delayValue = 0.0;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFiniteDoubleInput(block, delay, delayValue));

    MFnDependencyNode dependencyNode(thisMObject(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MPlug matrixPlug = dependencyNode.findPlug(transformMatrix, true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool matrixConnected = matrixPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!matrixConnected)
        return MS::kFailure;

    std::vector<MMatrix> matrices;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::sampleMatrixPlugSeries(
        matrices, matrixPlug, 1U, evaluationTime, -delayValue, 0.0, std::vector<double>()));
    if (matrices.size() != 1U)
        return MS::kFailure;

    MArrayDataHandle output = block.outputArrayValue(outputTransformMatrix, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataBuilder builder(&block, outputTransformMatrix, 1U, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MDataHandle element = builder.addElement(0U, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    element.setMMatrix(matrices[0]);
    DOMMY_RETURN_IF_FAILURE(output.set(builder));
    DOMMY_RETURN_IF_FAILURE(output.setAllClean());
    return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DTDelayTransformMatrix")

MStatus DTDelayTransformMatrix::initialize() {
    MFnNumericAttribute nAttr;
    MFnUnitAttribute unitFn;
    MFnMatrixAttribute matAttr;
    MStatus status;

    transformMatrix = matAttr.create("transformMatrix", "tm", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));

    delay = nAttr.create("delay", "de", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    // Preserve the legacy create overload and its zero default.
    time = unitFn.create("time", "ti", MFnUnitAttribute::kTime, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(unitFn.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(unitFn.setKeyable(true));

    outputTransformMatrix =
        matAttr.create("outputTransformMatrix", "otm", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(matAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(matAttr.setUsesArrayDataBuilder(true));

    DOMMY_RETURN_IF_FAILURE(addAttribute(transformMatrix));
    DOMMY_RETURN_IF_FAILURE(addAttribute(delay));
    DOMMY_RETURN_IF_FAILURE(addAttribute(time));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputTransformMatrix));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(transformMatrix, outputTransformMatrix));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(delay, outputTransformMatrix));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(time, outputTransformMatrix));

    return MS::kSuccess;
}
