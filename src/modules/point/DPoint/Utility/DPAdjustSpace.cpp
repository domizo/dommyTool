//
//  DPAdjustSpace.cpp
//  dommyTool
//
//  Deterministic, dependency-free point-set regularization.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAdjustSpace.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "geometry/DPointSetRegularizer.h"

#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "common/utility/DPointPayloadBudget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

const std::size_t kRegularizerNeighborLimit = 128U;

// All reduced arrays have at most the input cardinality. Use that upper bound
// for admission before copying points/metadata or constructing spatial maps.
// Four pointer widths per map entry estimate tree links/alignment. Combined
// with the shared four-copy allowance, this covers staging and vector growth;
// it is not a guarantee about native allocator overhead or Maya peak memory.
void preflightAdjustSpace(const DPData& input, bool regularize, bool uniform)
{
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(input, sizeof(DPoint), workspace);
    if (!regularize) return;

    const std::size_t count = input.numberOfPoints();
    using namespace dommy::geometry::regularizer_detail;
    workspace.add(count, 3U * sizeof(dommy::geometry::Point3) + sizeof(DPoint) +
                  2U * sizeof(std::size_t) + sizeof(bool));
    const std::size_t gridEntryBytes = sizeof(CellKey) +
        sizeof(std::vector<std::size_t>) + sizeof(std::size_t) + 4U * sizeof(void*);
    workspace.add(count, 2U * gridEntryBytes);
    if (uniform) {
        // Conservatively retain the former copied-group allowance. Stable
        // active-cell pointers and one candidate layer fit inside this budget.
        workspace.add(count, gridEntryBytes + sizeof(std::vector<std::size_t>) +
                      2U * sizeof(std::size_t));
    }
    workspace.add(2U * std::min(count, kRegularizerNeighborLimit), sizeof(WeightedNeighbor));
}

bool collectPoints(const DPData& input,
                   std::vector<dommy::geometry::Point3>& points)
{
    if (!input.hasValidPointTopology() ||
        input.numberOfPoints() >
            dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }

    points.reserve(input.numberOfPoints());
    for (ConstDPointIter point = input.beginPt();
         point != input.endPt(); ++point) {
        const dommy::geometry::Point3 converted(
            (*point)[0], (*point)[1], (*point)[2]);
        if (!dommy::geometry::isFinite(converted)) return false;
        points.push_back(converted);
    }
    return points.size() == input.numberOfPoints();
}

MStatus publishOutput(MDataBlock& data,
                      const MPlug& plug,
                      const MObject& object)
{
    MStatus status;
    MDataHandle output = data.outputValue(
        DPAdjustSpace::outDP, &status);
    if (!status) return status;
    status = output.set(object);
    if (!status) return status;
    output.setClean();
    return data.setClean(plug);
}

} // namespace

const MTypeId DPAdjustSpace::typeId(ID_DP_ADJUST_SPACE);

MObject DPAdjustSpace::inDP;
MObject DPAdjustSpace::aNeighborRadius;
MObject DPAdjustSpace::aIteration;
MObject DPAdjustSpace::aIsUniform;
MObject DPAdjustSpace::aRetainPercentage;
MObject DPAdjustSpace::outDP;

void* DPAdjustSpace::creator()
{
    return ::dommy::maya::createObject("DPAdjustSpace", [&]() {
        return new DPAdjustSpace();
    });
}

MStatus DPAdjustSpace::compute(const MPlug& plug,
                              MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;

    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kInvalidParameter;

    MStatus status;
    MDataHandle radiusHandle =
        data.inputValue(aNeighborRadius, &status);
    if (!status) return status;
    const double neighborRadius = radiusHandle.asDouble();

    MDataHandle iterationHandle =
        data.inputValue(aIteration, &status);
    if (!status) return status;
    const int rawIterations = iterationHandle.asInt();

    MDataHandle uniformHandle =
        data.inputValue(aIsUniform, &status);
    if (!status) return status;
    const bool uniform = uniformHandle.asBool();

    MDataHandle retainHandle =
        data.inputValue(aRetainPercentage, &status);
    if (!status) return status;
    const double retainPercentage = retainHandle.asDouble();

    if (!std::isfinite(neighborRadius) || neighborRadius < 0.0 ||
        rawIterations < 0 || rawIterations > 100 ||
        !std::isfinite(retainPercentage) ||
        retainPercentage < 0.0 || retainPercentage > 100.0) {
        EchoWarning(name().asChar(),
                    "Point regularization parameters are invalid.");
        return MS::kInvalidParameter;
    }

    // A zero radius or iteration count intentionally acts as a pass-through,
    // matching the historical node behavior.
    const bool regularize = input->numberOfPoints() != 0U &&
        neighborRadius != 0.0 && rawIterations != 0;
    preflightAdjustSpace(*input, regularize, uniform);

    dommy::geometry::RegularizedPointSet regularized;
    DPointList stagedPoints;
    if (regularize) {
        dommy::generation::WorkBudget regularizationBudget;
        if (!regularizationBudget.consume(input->numberOfPoints())) return MS::kFailure;
        std::vector<dommy::geometry::Point3> points;
        if (!collectPoints(*input, points)) {
            EchoWarning(name().asChar(), "Input point data is not finite.");
            return MS::kInvalidParameter;
        }
        if (!dommy::geometry::regularizePointSet(
                points, retainPercentage, neighborRadius,
                static_cast<unsigned int>(rawIterations), uniform,
                regularized,
                dommy::generation::kMaxGeneratedPointCount,
                regularizationBudget,
                kRegularizerNeighborLimit)) {
            EchoWarning(
                name().asChar(),
                "Point regularization failed or exceeded its safety budget.");
            return MS::kFailure;
        }

        // Validate every narrowing conversion before any output data is copied
        // or changed. The double-valued regularizer may exceed float range.
        if (!regularizationBudget.consumeProduct(regularized.points.size(), 3U)) return MS::kFailure;
        stagedPoints.resize(regularized.points.size());
        for (std::size_t index = 0U; index < regularized.points.size(); ++index) {
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                if (!dommy::numeric::checkedFloatCast(
                        regularized.points[index][axis], stagedPoints[index][axis])) {
                    return MS::kInvalidParameter;
                }
            }
        }
    }

    MObject outputObject;
    DPData* output = nullptr;
    status = dommy::maya::createPluginDataObject<DPData>(
        DPData::id, outputObject, output);
    if (!status) return status;
    if (!output) return MS::kFailure;
    output->setDPData(*input);
    if (!regularize) return publishOutput(data, plug, outputObject);

    DPIndexList selection(
        regularized.sourceIndices.begin(),
        regularized.sourceIndices.end());
    if (!output->remapPointFromIds(selection) ||
        output->numberOfPoints() != regularized.points.size()) {
        EchoWarning(name().asChar(),
                    "Failed to preserve point attributes during reduction.");
        return MS::kFailure;
    }

    for (std::size_t index = 0U; index < stagedPoints.size(); ++index) {
        output->setPosition(stagedPoints[index], index);
    }
    output->updateBBox();

    return publishOutput(data, plug, outputObject);
}
DOMMY_COMPUTE_CATCH("DPAdjustSpace")

MStatus DPAdjustSpace::initialize()
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inDP = typedAttribute.create(
        "inDP", "idp", DPData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aNeighborRadius = numericAttribute.create(
        "NeighborRadius", "nrad", MFnNumericData::kDouble,
        0.5, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNeighborRadius));

    aIteration = numericAttribute.create(
        "Iteration", "itr", MFnNumericData::kInt, 20, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIteration));

    aIsUniform = numericAttribute.create(
        "IsUniform", "uni", MFnNumericData::kBoolean,
        true, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsUniform));

    aRetainPercentage = numericAttribute.create(
        "RetainPercentage", "rtnper", MFnNumericData::kDouble,
        100.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(100.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(100.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRetainPercentage));

    outDP = typedAttribute.create(
        "outDP", "odp", DPData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aNeighborRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIteration, outDP));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsUniform, outDP));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aRetainPercentage, outDP));

    return MS::kSuccess;
}
