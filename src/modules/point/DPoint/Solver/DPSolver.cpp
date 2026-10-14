#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSolver.h"

#include "DPData.h"
#include "DPointDataAccess.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DFeedbackSolver.h"
#include "dpoint_ids.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>

const MTypeId DPSolver::typeId(ID_DP_SOLVER);

MObject DPSolver::inDP;
MObject DPSolver::aCurrentTime;
MObject DPSolver::prevDP;
MObject DPSolver::nextDP;
MObject DPSolver::outDP;

void* DPSolver::creator()
{
    return ::dommy::maya::createObject("DPSolver", [&]() {
        return new DPSolver();
    });
}

MStatus DPSolver::compute(const MPlug& plug, MDataBlock& data) try
{
    return dommy::maya::computeFeedbackSolver<DPData>(
        plug, data, inDP, aCurrentTime, prevDP, nextDP, outDP, DPData::id,
        &getInputDP,
        [](DPData& destination, const DPData& source) {
            destination.setDPData(source);
        });
}
DOMMY_COMPUTE_CATCH("DPSolver")

MStatus DPSolver::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;

    inDP = typedAttribute.create(
        "inDP", "idp", DPData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(inDP);
    if (!status) return status;

    MFnUnitAttribute unitAttribute;
    aCurrentTime = unitAttribute.create(
        "CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &status);
    if (!status) return status;
    status = addAttribute(aCurrentTime);
    if (!status) return status;

    prevDP = typedAttribute.create(
        "prevDP", "prdp", DPData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(prevDP);
    if (!status) return status;

    nextDP = typedAttribute.create(
        "nextDP", "nxdp", DPData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(nextDP);
    if (!status) return status;

    outDP = typedAttribute.create(
        "outDP", "odp", DPData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(outDP);
    if (!status) return status;

    status = attributeAffects(inDP, prevDP);
    if (!status) return status;
    status = attributeAffects(nextDP, prevDP);
    if (!status) return status;
    status = attributeAffects(aCurrentTime, prevDP);
    if (!status) return status;
    status = attributeAffects(nextDP, outDP);
    if (!status) return status;
    return attributeAffects(aCurrentTime, outDP);
}
