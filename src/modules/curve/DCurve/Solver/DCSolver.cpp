#include "common/utility/DMayaEntryPointGuard.h"
#include "DCSolver.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DFeedbackSolver.h"
#include "dcurve_ids.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>

const MTypeId DCSolver::typeId(ID_DC_SOLVER);

MObject DCSolver::inDC;
MObject DCSolver::aCurrentTime;
MObject DCSolver::prevDC;
MObject DCSolver::nextDC;
MObject DCSolver::outDC;

void* DCSolver::creator()
{
    return ::dommy::maya::createObject("DCSolver", [&]() {
        return new DCSolver();
    });
}

MStatus DCSolver::compute(const MPlug& plug, MDataBlock& data) try
{
    return dommy::maya::computeFeedbackSolver<DCurveData>(
        plug, data, inDC, aCurrentTime, prevDC, nextDC, outDC,
        DCurveData::id, &getInputDC,
        [](DCurveData& destination, const DCurveData& source) {
            return destination.copyFrom(source);
        });
}
DOMMY_COMPUTE_CATCH("DCSolver")

MStatus DCSolver::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;

    inDC = typedAttribute.create(
        "inDC", "idc", DCurveData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(inDC);
    if (!status) return status;

    MFnUnitAttribute unitAttribute;
    aCurrentTime = unitAttribute.create(
        "CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &status);
    if (!status) return status;
    status = addAttribute(aCurrentTime);
    if (!status) return status;

    prevDC = typedAttribute.create(
        "prevDC", "prdc", DCurveData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(prevDC);
    if (!status) return status;

    nextDC = typedAttribute.create(
        "nextDC", "nxdc", DCurveData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(nextDC);
    if (!status) return status;

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(outDC);
    if (!status) return status;

    status = attributeAffects(inDC, prevDC);
    if (!status) return status;
    status = attributeAffects(nextDC, prevDC);
    if (!status) return status;
    status = attributeAffects(aCurrentTime, prevDC);
    if (!status) return status;
    status = attributeAffects(nextDC, outDC);
    if (!status) return status;
    return attributeAffects(aCurrentTime, outDC);
}
