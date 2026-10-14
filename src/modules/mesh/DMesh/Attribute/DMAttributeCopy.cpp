//
//  DMAttributeCopy.cpp
//  dommyTool
//
//  Created by yuzo on 5/11/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMAttributeCopy.h"
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
#include "DMeshDataHeaders.h"

const MTypeId DMAttributeCopy::typeId( ID_DM_ATTRIBUTE_COPY );

MObject DMAttributeCopy::inDM;

MObject DMAttributeCopy::aTargetDP;
MObject DMAttributeCopy::aAttributeNames;

MObject DMAttributeCopy::outDM;

void *DMAttributeCopy::creator()
{
    return ::dommy::maya::createObject("DMAttributeCopy", [&]() {
        return new DMAttributeCopy();
    });
}

MStatus DMAttributeCopy::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    const DPData* source = nullptr;
    std::size_t totalPoints = 0U;
    std::vector<std::string> names;
    if (input->numberOfMesh() != 0U) {
        MStatus status;
        const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, source));
        if (!source || !source->hasValidPointTopology()) return MS::kInvalidParameter;
        if (!input->getCheckedTotalVertex(totalPoints)) return MS::kFailure;
        if (totalPoints != source->numberOfPoints()) {
            // Preserve the historical count-mismatch pass-through.
            MGlobal::displayWarning("DMAttributeCopy: point counts differ; attributes were not copied.");
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
    DMeshData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    dommy::attribute::ValidationBudget copyBudget;
    for (const std::string& name : names) {
        if (!source) return MS::kFailure;
        if (!dommy::attribute::copyAttributePartitions(source->getAttr(name), name,
                output->beginPrims(), output->endPrims(), totalPoints, copyBudget))
            return MS::kInvalidParameter;
    }
    if (!output->hasValidMeshTopology()) return MS::kFailure;
    MStatus status;
    MDataHandle outputHandle = data.outputValue(outDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            outputHandle, pluginData.object(), DMeshData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMAttributeCopy")

MStatus DMAttributeCopy::initialize() try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	MFnStringData fnStringData;

	// ----------------------- INPUTS --------------------------
	inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

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
	outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeNames, outDM ));

	return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMAttributeCopy", "initialize", std::current_exception());
}
