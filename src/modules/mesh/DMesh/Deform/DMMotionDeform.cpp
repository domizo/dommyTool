//
//  DMMotionDeform.cpp
//  dommyTool
//
//  Created by yuzo on 2017/06/22.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMMotionDeform.h"
#include "DMeshUtil.h"
#include "dmesh_ids.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "dommyMath.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DTimeSamplingBudget.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>



const MTypeId DMMotionDeform::typeId( ID_DM_MOTION_DEFORM );


MObject DMMotionDeform::inDM;

MObject DMMotionDeform::aTimeRange;
MObject DMMotionDeform::aSampling;

MObject DMMotionDeform::aAmplitude;
MObject DMMotionDeform::aAmplitudeRamp;

MObject DMMotionDeform::aWeightAttributeName;
MObject DMMotionDeform::aWeight;

MObject DMMotionDeform::aIsExportMotionPower;
MObject DMMotionDeform::aMotionPowerName;
MObject DMMotionDeform::aIsExportMotionVector;
MObject DMMotionDeform::aMotionVectorName;

MObject DMMotionDeform::outDM;

void *DMMotionDeform::creator()
{
    return ::dommy::maya::createObject("DMMotionDeform", [&]() {
        return new DMMotionDeform();
    });
}

MStatus DMMotionDeform::getDMeshFromTime(std::vector<std::vector<DPointList>>& ptsArray,
                                        const std::vector<std::size_t>& expectedPointCounts,
                                        std::size_t totalPointCount,
                                        long sampling,
                                        double timeRange,
                                        const MTime& currentTime)
{
    if (sampling < 2 ||
        !dommy::deformer::validMotionSampleRequest(
            sampling, timeRange, totalPointCount) ||
        !dommy::sampling::validTimeHistoryStorage(
            totalPointCount, expectedPointCounts.size(), sampling)) {
        return MS::kFailure;
    }

    const double stepTime =
        timeRange / static_cast<double>(sampling - 1);
    MTime endpoint;
    if (!dommy::maya::historicalSampleTime(currentTime, 0.0, -stepTime, 0.0,
            static_cast<std::size_t>(sampling - 1), endpoint)) return MS::kInvalidParameter;
    std::size_t expectedTotal = 0U;
    for (std::size_t count : expectedPointCounts)
        if (!dommy::generation::checkedSum(expectedTotal, count, totalPointCount, expectedTotal))
            return MS::kInvalidParameter;
    if (expectedTotal != totalPointCount) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    MPlug dmPlug(thisMObject(), inDM);

    std::vector<std::vector<DPointList>> sampledPoints;
    sampledPoints.reserve(static_cast<std::size_t>(sampling));
	for (long sampleIndex = 0; sampleIndex < sampling; ++sampleIndex) {
        MTime sampleTime;
        if (!dommy::maya::historicalSampleTime(currentTime, 0.0, -stepTime, 0.0,
                static_cast<std::size_t>(sampleIndex), sampleTime)) return MS::kInvalidParameter;
        if (!budget.readPoints(totalPointCount) ||
            !budget.addPoints(totalPointCount, expectedPointCounts.size() + 1U))
            return MS::kInvalidParameter;
		const MDGContext context(sampleTime);
        const MDGContextGuard contextGuard(context);
        dommy::maya::ScopedPlugDataHandle dmHndOwner;
        MDataHandle& dmHnd = dmHndOwner.handle();
        const MStatus sampleStatus = dmHndOwner.read(dmPlug);
        if (!sampleStatus) return sampleStatus;

        const DMeshData* dmesh = nullptr;
        const MStatus bindingStatus = dommy::maya::getPluginDataFromHandle(
            dmHnd, DMeshData::id, dmesh);
        if (!bindingStatus) return bindingStatus;
		if (!dmesh || dmesh->numberOfMesh() != expectedPointCounts.size() ||
            !dmesh->hasValidMeshTopology()) {
			return MS::kFailure;
		}

        std::vector<std::size_t> sampledPointCounts;
        sampledPointCounts.reserve(dmesh->numberOfMesh());
		for (auto mesh = dmesh->beginMeshes(); mesh != dmesh->endMeshes();
             ++mesh) {
            if (!*mesh || !(*mesh)->hasValidMeshData()) {
                return MS::kFailure;
            }
            const DPointList* positions = (*mesh)->getPositionPtrConst();
            if (!positions ||
                positions->size() != (*mesh)->numberOfPoints()) {
                return MS::kFailure;
            }
            sampledPointCounts.push_back(positions->size());
		}

        if (!dommy::deformer::hasMatchingSampleLayout(
                expectedPointCounts, sampledPointCounts)) {
            MGlobal::displayWarning(
                "DMMotionDeform skipped evaluation because sampled topology changed.");
            return MS::kFailure;
        }
        // Validate every group before copying any historical positions.
        std::vector<DPointList> sampleMeshes;
        sampleMeshes.reserve(expectedPointCounts.size());
        for (auto mesh = dmesh->beginMeshes(); mesh != dmesh->endMeshes(); ++mesh)
            sampleMeshes.push_back(*(*mesh)->getPositionPtrConst());
        sampledPoints.push_back(std::move(sampleMeshes));
	}

    ptsArray.swap(sampledPoints);
	return MS::kSuccess;
}


MStatus DMMotionDeform::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* inputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputMeshes));

    const MDataHandle sampleCountHandle = data.inputValue(aSampling, &status);
    if (!status) return status;
    const int sampleCount = sampleCountHandle.asInt();
    const MDataHandle timeRangeHandle = data.inputValue(aTimeRange, &status);
    if (!status) return status;
    const double timeRange = timeRangeHandle.asDouble();
    const MDataHandle amplitudeHandle = data.inputValue(aAmplitude, &status);
    if (!status) return status;
    const double amplitude = amplitudeHandle.asDouble();
    const MDataHandle defaultWeightHandle = data.inputValue(aWeight, &status);
    if (!status) return status;
    const double defaultWeight = defaultWeightHandle.asDouble();
    const MDataHandle weightAttributeHandle = data.inputValue(aWeightAttributeName, &status);
    if (!status) return status;
    const std::string weightAttribute = weightAttributeHandle.asString().asChar();
    const MDataHandle exportMotionPowerHandle = data.inputValue(aIsExportMotionPower, &status);
    if (!status) return status;
    const bool exportMotionPower = exportMotionPowerHandle.asBool();
    const MDataHandle motionPowerAttributeHandle = data.inputValue(aMotionPowerName, &status);
    if (!status) return status;
    const std::string motionPowerAttribute = motionPowerAttributeHandle.asString().asChar();
    const MDataHandle exportMotionVectorHandle = data.inputValue(aIsExportMotionVector, &status);
    if (!status) return status;
    const bool exportMotionVector = exportMotionVectorHandle.asBool();
    const MDataHandle motionVectorAttributeHandle = data.inputValue(aMotionVectorName, &status);
    if (!status) return status;
    const std::string motionVectorAttribute = motionVectorAttributeHandle.asString().asChar();
    if (sampleCount < 0) return MS::kFailure;
    const bool writeMotionPower = exportMotionPower && !motionPowerAttribute.empty();
    const bool writeMotionVector = exportMotionVector && !motionVectorAttribute.empty();

    const std::size_t meshCount = inputMeshes->numberOfMesh();
    std::size_t totalPointCount = 0U;
    std::vector<std::size_t> expectedPointCounts;
    float defaultWeightValue = 0.0f;
    if (meshCount != 0U && sampleCount > 1) {
        // Empty names retain the historical opt-out. Enabled outputs must be
        // distinct: one stored field cannot hold both scalar and Vec3 data.
        if ((writeMotionPower &&
             (!dommy::attribute::validAttributeName(motionPowerAttribute) ||
              motionPowerAttribute.size() > dommy::generation::kMaxGeneratedStringByteCount)) ||
            (writeMotionVector &&
             (!dommy::attribute::validAttributeName(motionVectorAttribute) ||
              motionVectorAttribute.size() > dommy::generation::kMaxGeneratedStringByteCount)) ||
            (writeMotionPower && writeMotionVector &&
             motionPowerAttribute == motionVectorAttribute)) {
            return MS::kInvalidParameter;
        }
        if (!inputMeshes->getCheckedTotalVertex(totalPointCount) ||
            !dommy::deformer::validMotionSampleRequest(
                sampleCount, timeRange, totalPointCount) ||
            !dommy::sampling::validTimeHistoryStorage(totalPointCount, meshCount, sampleCount) ||
            !std::isfinite(amplitude) ||
            !dommy::numeric::checkedFloatCast(defaultWeight, defaultWeightValue)) {
            MGlobal::displayWarning(
                "DMMotionDeform rejected invalid or excessive sampling input.");
            return MS::kFailure;
        }

        expectedPointCounts.reserve(meshCount);
        for (auto mesh = inputMeshes->beginMeshes();
             mesh != inputMeshes->endMeshes(); ++mesh) {
            if (!*mesh || !(*mesh)->hasValidMeshData()) {
                return MS::kFailure;
            }
            expectedPointCounts.push_back((*mesh)->numberOfPoints());
        }

    }

    MFnPluginData pluginData;
    pluginData.create(DMeshData::id, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DMeshData* const outputMeshes =
        dommy::maya::pluginDataPointer<DMeshData>(pluginData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputMeshes) return MS::kFailure;

    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));
    if (!outputMeshes->hasValidMeshTopology()) return MS::kFailure;

    if (meshCount != 0U && sampleCount > 1) {
        MRampAttribute amplitudeRamp(
            MPlug(thisMObject(), aAmplitudeRamp), &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        MTime evaluationTime;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getEvaluationTime(data, evaluationTime));
        std::vector<std::vector<DPointList>> sampledPoints;
        DOMMY_RETURN_IF_FAILURE(getDMeshFromTime(
            sampledPoints, expectedPointCounts, totalPointCount, sampleCount,
            timeRange, evaluationTime));

        std::size_t meshIndex = 0U;
        auto inputMesh = inputMeshes->beginMeshes();
        for (auto mesh = outputMeshes->beginMeshes();
             mesh != outputMeshes->endMeshes(); ++mesh, ++inputMesh, ++meshIndex) {
            if (inputMesh == inputMeshes->endMeshes() || !*inputMesh ||
                (*inputMesh)->numberOfPoints() != (*mesh)->numberOfPoints()) {
                return MS::kInvalidParameter;
            }
            // Borrow the original field until conversion is complete, even
            // when either output deliberately replaces the weight's name/type.
            DScalarDataList weights((*inputMesh)->numberOfPoints(), defaultWeightValue);
            const auto weight = (*inputMesh)->getAttr(weightAttribute);
            if (weight && !dommy::attribute::convertScalarAttribute(
                    weight, (*inputMesh)->numberOfPoints(), weights)) {
                return MS::kInvalidParameter;
            }
            MObject mayaMesh;
            DOMMY_RETURN_IF_FAILURE(
                generateMMeshFromDmesh(*mesh->get(), mayaMesh));

            if (weights.size() != (*mesh)->numberOfPoints()) {
                return MS::kFailure;
            }
            for (std::size_t pointIndex = 0U; pointIndex < weights.size();
                 ++pointIndex) {
                if (!std::isfinite(static_cast<double>(weights[pointIndex]))) {
                    return MS::kFailure;
                }
            }

            dommy::geometry::StagedPositionEdits<
                DMeshInternalData> positionEdit;
            if (!positionEdit.add(**mesh)) {
                return MS::kFailure;
            }
            DPointList* positions = &positionEdit.positions(0U);

            MItMeshVertex vertexIterator(mayaMesh, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            DScalarDataList motionPowers(
                (*mesh)->numberOfPoints(), 0.0);
            DVec3DataList motionVectors(
                (*mesh)->numberOfPoints(), DVec3Data(0.0, 0.0, 0.0));

            for (std::size_t pointIndex = 0U;
                 pointIndex < positions->size(); ++pointIndex) {
                int previousIndex = 0;
                DOMMY_RETURN_IF_FAILURE(vertexIterator.setIndex(
                    static_cast<int>(pointIndex), previousIndex));

                MVector normal;
                DOMMY_RETURN_IF_FAILURE(vertexIterator.getNormal(normal));
                MVector normalizedNormal;
                if (!dommy::maya::normalizedVector(
                        normal, normalizedNormal)) {
                    continue;
                }

                const DPoint& currentSample =
                    sampledPoints[0U][meshIndex][pointIndex];
                const DPoint& previousSample =
                    sampledPoints[1U][meshIndex][pointIndex];
                const MPoint sampledCurrent(
                    currentSample[0], currentSample[1], currentSample[2]);
                const MPoint sampledPrevious(
                    previousSample[0], previousSample[1], previousSample[2]);

                MVector movementDirection;
                if (!dommy::maya::normalizedVector(
                        sampledCurrent - sampledPrevious,
                        movementDirection)) {
                    continue;
                }
                motionVectors[pointIndex] = DVec3Data(
                    movementDirection.x, movementDirection.y,
                    movementDirection.z);

                const double angleDegrees =
                    dommy::math::radiansToDegrees(movementDirection.angle(normalizedNormal));
                if (!std::isfinite(angleDegrees)) return MS::kFailure;

                float rampValue = 0.0f;
                amplitudeRamp.getValueAtPosition(
                    static_cast<float>(angleDegrees / 180.0),
                    rampValue, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!std::isfinite(rampValue)) return MS::kFailure;
                rampValue = std::max(0.0f, std::min(1.0f, rampValue));
                motionPowers[pointIndex] = rampValue;

                dommy::deformer::MotionSampleBlend blend;
                if (!dommy::deformer::motionSampleBlend(
                        static_cast<double>(rampValue),
                        sampledPoints.size(), blend)) {
                    return MS::kFailure;
                }

                const DPoint& lower =
                    sampledPoints[blend.lowerIndex][meshIndex][pointIndex];
                const DPoint& upper =
                    sampledPoints[blend.upperIndex][meshIndex][pointIndex];
                const MPoint lowerPoint(lower[0], lower[1], lower[2]);
                const MPoint upperPoint(upper[0], upper[1], upper[2]);

                const MPoint sourcePoint(
                    (*positions)[pointIndex][0],
                    (*positions)[pointIndex][1],
                    (*positions)[pointIndex][2]);
                const MVector displacement =
                    lowerPoint +
                    (upperPoint - lowerPoint) * blend.fraction -
                    sampledCurrent;
                const MPoint result =
                    sourcePoint +
                    displacement *
                        (amplitude *
                         static_cast<double>(weights[pointIndex]));
                if (!dommy::maya::isFloatRepresentablePoint(result)) {
                    return MS::kFailure;
                }
                (*positions)[pointIndex] =
                    DPoint(result.x, result.y, result.z);
            }

            if (!positionEdit.commit()) return MS::kFailure;

            if (writeMotionPower) {
                const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(motionPowers);
                if (!dommy::attribute::commitStagedAttribute(
                        **mesh, motionPowerAttribute, candidate, (*mesh)->numberOfPoints()))
                    return MS::kFailure;
            }
            if (writeMotionVector) {
                const DPAttributeData::Ptr candidate = std::make_shared<DPVec3fAttr>(motionVectors);
                if (!dommy::attribute::commitStagedAttribute(
                        **mesh, motionVectorAttribute, candidate, (*mesh)->numberOfPoints()))
                    return MS::kFailure;
            }
        }
    }

    outputMeshes->updateBBox();
    MDataHandle output = data.outputValue(outDM, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            output, pluginData.object(), DMeshData::id));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMMotionDeform")




MStatus DMMotionDeform::initialize() try {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aTimeRange = nAttr.create("TimeRange", "tmr", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeRange));

    aSampling = nAttr.create("Sampling", "sa", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(
        static_cast<int>(dommy::deformer::kMaxMotionSampleCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aAmplitude = nAttr.create("Amplitude", "amp", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitude));

    aAmplitudeRamp = MRampAttribute::createCurveRamp("AmplitudeRamp", "aramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitudeRamp));

    const MObject defaultWeightName = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttributeName = tAttr.create("WeightAttributeName", "weiattrn", MFnData::kString, defaultWeightName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttributeName));

    aWeight = nAttr.create("Weight", "wei", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));

    aIsExportMotionPower = nAttr.create("IsExportMotionPower", "exmp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportMotionPower));

    const MObject defaultMotionPowerName = fnStringData.create("motionpower", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMotionPowerName = tAttr.create("MotionPowerName", "mpattr", MFnData::kString, defaultMotionPowerName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMotionPowerName));

    aIsExportMotionVector = nAttr.create("IsExportMotionVector", "exmv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportMotionVector));

    const MObject defaultMotionVectorName = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMotionVectorName = tAttr.create("MotionVectorName", "mvattr", MFnData::kString, defaultMotionVectorName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMotionVectorName));

    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimeRange, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitude, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitudeRamp, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeightAttributeName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeight, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsExportMotionPower, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMotionPowerName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsExportMotionVector, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMotionVectorName, outDM));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMMotionDeform", "initialize", std::current_exception());
}
