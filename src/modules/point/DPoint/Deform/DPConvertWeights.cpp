//
//  DPConvertWeights.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/30.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPConvertWeights.h"
#include "DPointCommon.h"
#include "dpoint_ids.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatArray.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MTypeId.h>

#include <cmath>

const MTypeId DPConvertWeights::typeId(ID_DP_CONVERT_WEIGHT);

MObject DPConvertWeights::inputDataArray;
MObject DPConvertWeights::aWeightsList;
MObject DPConvertWeights::aWeights;

void* DPConvertWeights::creator()
{
    return ::dommy::maya::createObject("DPConvertWeights", [&]() {
        return new DPConvertWeights();
    });
}

MStatus DPConvertWeights::compute(const MPlug& plug, MDataBlock& block) try
{
    if (plug != aWeightsList && plug != aWeights) return MS::kUnknownParameter;

    // One input array maps to weightsList[0]. Accept requests for that row's
    // children as well as the complete output, without inventing other rows.
    MStatus status;
    MPlug rowPlug = plug;
    if (plug == aWeights) {
        const bool element = plug.isElement(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        MPlug weightsPlug = plug;
        if (element) {
            weightsPlug = plug.array(&status);
            DOMMY_RETURN_IF_FAILURE(status);
        }
        rowPlug = weightsPlug.parent(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (rowPlug != aWeightsList) return MS::kUnknownParameter;
    }
    const bool rowElement = rowPlug.isElement(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (rowElement) {
        const unsigned int rowIndex = rowPlug.logicalIndex(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (rowIndex != 0U) return MS::kUnknownParameter;
    } else if (plug == aWeights) {
        return MS::kUnknownParameter;
    }

    MFloatArray values;
    DOMMY_RETURN_IF_FAILURE(getFloatArrayFromData(block, inputDataArray, values));
    if (values.length() == 0U) {
        MGlobal::displayError("DPConvertWeights: input data array is empty.");
        return MS::kFailure;
    }
    if (static_cast<std::size_t>(values.length()) >
        dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }
    for (unsigned int i = 0U; i < values.length(); ++i) {
        if (!std::isfinite(values[i])) return MS::kInvalidParameter;
    }

    MArrayDataHandle output = block.outputArrayValue(aWeightsList, &status);
    DOMMY_RETURN_IF_FAILURE(status);

    // Fresh builders replace the complete nested array. Reusing the existing
    // builders would retain stale weights (and rows) when the input shrinks.
    MArrayDataBuilder rows(&block, aWeightsList, 1U, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MDataHandle row = rows.addElement(0U, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataHandle weights(row.child(aWeights), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataBuilder elements(&block, aWeights, values.length(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    for (unsigned int i = 0U; i < values.length(); ++i) {
        MDataHandle element = elements.addElement(i, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        element.set(values[i]);
    }
    DOMMY_RETURN_IF_FAILURE(weights.set(elements));
    DOMMY_RETURN_IF_FAILURE(output.set(rows));

    // Clean the committed child array, not a handle into the staging builder.
    // Native set/clean calls do not offer atomic rollback after publication.
    DOMMY_RETURN_IF_FAILURE(output.jumpToElement(0U));
    MDataHandle committedRow = output.outputValue(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataHandle committedWeights(committedRow.child(aWeights), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(committedWeights.setAllClean());
    DOMMY_RETURN_IF_FAILURE(output.setAllClean());
    return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPConvertWeights")

MStatus DPConvertWeights::initialize() try
{
    MStatus status;
    MFnNumericAttribute numeric;
    MFnGenericAttribute generic;
    MFnCompoundAttribute compound;

    inputDataArray = generic.create("inputDataArray", "ida", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(generic.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(generic.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputDataArray));

    // Preserve the historical missing-element default; explicit elements are
    // created by the builder, including values equal to this default.
    aWeights = numeric.create("weights", "w", MFnNumericData::kFloat, -1000.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numeric.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numeric.setArray(true));
    DOMMY_RETURN_IF_FAILURE(numeric.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(numeric.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(numeric.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(numeric.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeights));

    aWeightsList = compound.create("weightsList", "wl", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(compound.setArray(true));
    DOMMY_RETURN_IF_FAILURE(compound.addChild(aWeights));
    DOMMY_RETURN_IF_FAILURE(compound.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(compound.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(compound.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(compound.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightsList));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputDataArray, aWeightsList));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPConvertWeights", "initialize", std::current_exception());
}
