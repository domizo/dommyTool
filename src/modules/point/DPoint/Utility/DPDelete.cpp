//
//  DPDelete.cpp
//  dommyTool
//
//  Created by yuzo on 6/25/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPDelete.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"

#include "DPointDataHeaders.h"

#include "common/utility/DIndexPattern.h"

#include <string>

MTypeId DPDelete::typeId( ID_DP_DELETE );

//Attribute
MObject DPDelete::inDP;

MObject DPDelete::aDeleteGroups;
MObject DPDelete::aDeleteNumber;

MObject DPDelete::outDP;

void *DPDelete::creator() {
    return ::dommy::maya::createObject("DPDelete", [&]() {
        return new DPDelete();
    });
}

MStatus DPDelete::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;

    const std::size_t pointCount = input->numberOfPoints();
    DPIdList selection;
    MString groupNames;
    if (pointCount != 0U) {
        const MDataHandle groupsHandle = data.inputValue(aDeleteGroups, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        groupNames = groupsHandle.asString();
        const MDataHandle patternHandle = data.inputValue(aDeleteNumber, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MString pattern = patternHandle.asString();
        if (groupNames.length() > dommy::generation::kMaxGeneratedStringByteCount ||
            pattern.length() > dommy::generation::kMaxGeneratedStringByteCount)
            return MS::kInvalidParameter;
        const std::string groupsText(groupNames.asChar()), patternText(pattern.asChar());
        // Group names are space-delimited; index patterns additionally accept
        // whitespace and semicolons. Do not change the shared selection helper:
        // its empty-group == all behavior is required by other consumers.
        const bool hasGroups = groupsText.find_first_not_of(' ') != std::string::npos;
        const bool hasPattern = patternText.find_first_not_of(" \t\r\n\f\v;") != std::string::npos;
        DPIndexList indices;
        bool reverse = false;
        if (!getPatternStrIndex(pattern, pointCount, reverse, indices)) return MS::kInvalidParameter;
        selection.assign(pointCount, false);
        // Retain the historical delete-all result when both selectors are empty.
        if (hasGroups || !hasPattern) {
            bool groupUse = false;
            checkDPGroupActiveIdsFromString(*input, groupNames, selection, groupUse);
        }
        if (!dommy::component::mergeIndexPatternSelection(indices, reverse, selection))
            return MS::kInvalidParameter;
    }
    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, pointCount, &selection, &groupNames](DPData& output) -> MStatus {
            output.setDPData(*input);
            if (pointCount != 0U) {
                if (!output.deletePointFromIds(selection)) return MS::kFailure;
                output.removeGrps(groupNames);
            }
            return MS::kSuccess;
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPDelete")

MStatus DPDelete::initialize() try {
    MStatus stat;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnStringData fnStringData;
    const MObject defaultGroups = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDeleteGroups = tAttr.create("DeleteGroups", "delgrpn", MFnData::kString, defaultGroups, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeleteGroups));

    MObject patAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDeleteNumber = tAttr.create("DeleteNumber", "delnum", MFnData::kString, patAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeleteNumber));

    // Output data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeleteGroups, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeleteNumber, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPDelete", "initialize", std::current_exception());
}
