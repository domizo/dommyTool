//
//  DCAttributeCopy.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/30.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCAttributeCopy.h"
#include "common/utility/DAttributePartitionCopy.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DMayaPluginData.h"
#include <set>
#include <string>
#include <vector>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DCurveDataHeaders.h"

const MTypeId DCAttributeCopy::typeId( ID_DC_ATTRIBUTE_COPY );

MObject DCAttributeCopy::inDC;

MObject DCAttributeCopy::aTargetDP;
MObject DCAttributeCopy::aAttributeNames;

MObject DCAttributeCopy::outDC;

void *DCAttributeCopy::creator()
{
    return ::dommy::maya::createObject("DCAttributeCopy", [&]() {
        return new DCAttributeCopy();
    });
}

MStatus DCAttributeCopy::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
    const DPData* source = nullptr;
    std::size_t totalPoints = 0U;
    std::vector<std::string> names;
    if (input->numberOfCurves() != 0U) {
        MStatus status;
        const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, source));
        if (!source || !source->hasValidPointTopology()) return MS::kInvalidParameter;
        if (!input->getCheckedTotalCvs(totalPoints)) return MS::kFailure;
        if (totalPoints != source->numberOfPoints()) {
            // Preserve the historical count-mismatch pass-through.
            MGlobal::displayWarning("DCAttributeCopy: point counts differ; attributes were not copied.");
        } else {
            dommy::maya::NameListBudget nameBudget;
            MStringArray requested;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::readCheckedNameList(
                data, aAttributeNames, totalPoints, nameBudget, requested));
            std::set<std::string> seen;
            for (unsigned int i = 0U; i < requested.length(); ++i) {
                const std::string name = requested[i].asChar();
                if (source->isExistAttr(name) && seen.insert(name).second)
                    names.push_back(name);
            }
        }
    }

    MFnPluginData pluginData;
    DCurveData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    dommy::attribute::ValidationBudget copyBudget;
    for (const std::string& name : names) {
        if (!source) return MS::kFailure;
        if (!dommy::attribute::copyAttributePartitions(source->getAttr(name), name,
                output->beginPrims(), output->endPrims(), totalPoints, copyBudget))
            return MS::kInvalidParameter;
    }
    if (!output->hasValidCurveTopology()) return MS::kFailure;
    MStatus status;
    MDataHandle outputHandle = data.outputValue(outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DCurveData>(
            outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCAttributeCopy")

MStatus DCAttributeCopy::initialize() try {
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    const MObject defaultString1 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeNames = tAttr.create("AttributeNames", "attrnms", MFnData::kString, defaultString1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeNames));

    //create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeNames, outDC ));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCAttributeCopy", "initialize", std::current_exception());
}
