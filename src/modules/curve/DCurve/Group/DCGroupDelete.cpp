//
//  DCGroupDelete.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/10.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCGroupDelete.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DMayaPluginData.h"
#include <array>
#include <string>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"

#include "DMayaHeaders.h"

#include "DCurveDataHeaders.h"

#include "DNodeOperation.h"

MTypeId DCGroupDelete::typeId( ID_DC_GROUP_DELETE );

MObject DCGroupDelete::inDC;

MObject DCGroupDelete::aIsEnable;
MObject DCGroupDelete::aPrimitiveGroupName;
MObject DCGroupDelete::aPointGroupName;

MObject DCGroupDelete::outDC;

void *DCGroupDelete::creator() {
    return ::dommy::maya::createObject("DCGroupDelete", [&]() {
        return new DCGroupDelete();
    });
}

MStatus DCGroupDelete::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));

    const std::size_t curveCount = input->numberOfCurves();
    std::array<MStringArray, 2> names;
    if (curveCount != 0U) {
        MStatus status;
        const MDataHandle enableHandle = data.inputValue(aIsEnable, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (enableHandle.asBool()) {
            const MObject attributes[] = {
                aPrimitiveGroupName, aPointGroupName};
            dommy::maya::NameListBudget budget;
            for (std::size_t domain = 0U; domain < names.size(); ++domain) {
                DOMMY_RETURN_IF_FAILURE(dommy::maya::readCheckedNameList(
                    data, attributes[domain], domain == 0U ? 1U : curveCount,
                    budget, names[domain]));
            }
        }
    }

    // Parse every requested domain before editing an unpublished copy. Disabled
    // and empty inputs preserve the legacy pass-through behavior.
    MFnPluginData pluginData;
    DCurveData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    for (unsigned int i = 0U; i < names[0].length(); ++i) {
        const std::string name = names[0][i].asChar();
        if (output->isExistGrp(name)) output->removeGrp(name);
    }
    for (auto curve = output->beginCurves(); curve != output->endCurves(); ++curve) {
        if (!*curve) return MS::kFailure;
        for (unsigned int i = 0U; i < names[1].length(); ++i) {
            const std::string name = names[1][i].asChar();
            if ((*curve)->isExistGrp(name)) (*curve)->removeGrp(name);
        }
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
DOMMY_COMPUTE_CATCH("DCGroupDelete")

MStatus DCGroupDelete::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aIsEnable = nAttr.create( "IsEnable", "isen", MFnNumericData::kBoolean, false, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEnable));

    const MObject defaultString1 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPrimitiveGroupName = tAttr.create("PrimitiveGroupName", "pgrpnm", MFnData::kString, defaultString1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPrimitiveGroupName));

    const MObject defaultString2 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPointGroupName = tAttr.create("PointGroupName", "ptgrpnm", MFnData::kString, defaultString2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointGroupName));

    //create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEnable, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPrimitiveGroupName, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPointGroupName, outDC ));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCGroupDelete", "initialize", std::current_exception());
}
