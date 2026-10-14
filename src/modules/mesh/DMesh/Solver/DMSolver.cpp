#include "common/utility/DMayaEntryPointGuard.h"
#include "DMSolver.h"

#include "DMeshData.h"
#include "DMeshUtil.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DFeedbackSolver.h"
#include "dmesh_ids.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>

const MTypeId DMSolver::typeId(ID_DM_SOLVER);

MObject DMSolver::inDM;
MObject DMSolver::aCurrentTime;
MObject DMSolver::prevDM;
MObject DMSolver::nextDM;
MObject DMSolver::outDM;

void* DMSolver::creator()
{
    return ::dommy::maya::createObject("DMSolver", [&]() {
        return new DMSolver();
    });
}

MStatus DMSolver::compute(const MPlug& plug, MDataBlock& data) try
{
    return dommy::maya::computeFeedbackSolver<DMeshData>(
        plug, data, inDM, aCurrentTime, prevDM, nextDM, outDM,
        DMeshData::id, &getInputDM,
        [](DMeshData& destination, const DMeshData& source) {
            return destination.copyFrom(source);
        });
}
DOMMY_COMPUTE_CATCH("DMSolver")

MStatus DMSolver::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(inDM);
    if (!status) return status;

    MFnUnitAttribute unitAttribute;
    aCurrentTime = unitAttribute.create(
        "CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &status);
    if (!status) return status;
    status = addAttribute(aCurrentTime);
    if (!status) return status;

    prevDM = typedAttribute.create(
        "prevDM", "prdm", DMeshData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(prevDM);
    if (!status) return status;

    nextDM = typedAttribute.create(
        "nextDM", "nxdm", DMeshData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(nextDM);
    if (!status) return status;

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setWritable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(outDM);
    if (!status) return status;

    status = attributeAffects(inDM, prevDM);
    if (!status) return status;
    status = attributeAffects(nextDM, prevDM);
    if (!status) return status;
    status = attributeAffects(aCurrentTime, prevDM);
    if (!status) return status;
    status = attributeAffects(nextDM, outDM);
    if (!status) return status;
    return attributeAffects(aCurrentTime, outDM);
}
