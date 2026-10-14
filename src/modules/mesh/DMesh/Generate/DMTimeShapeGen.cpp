//
//  DMTimeShapeGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/08.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMTimeShapeGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DTimeSamplingBudget.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshData.h"
#include "dmesh_ids.h"

#include <algorithm>
#include <cmath>

const MTypeId DMTimeShapeGen::typeId( ID_DM_TIME_SHAPE_GEN );

MObject DMTimeShapeGen::inMesh;


MObject DMTimeShapeGen::aStartFrame;
MObject DMTimeShapeGen::aEndFrame;

MObject DMTimeShapeGen::aTimeSample;

MObject DMTimeShapeGen::outDM;

void *DMTimeShapeGen::creator() {
    return ::dommy::maya::createObject("DMTimeShapeGen", [&]() {
        return new DMTimeShapeGen();
    });
}

namespace {

MStatus sampleMeshPoints(
    std::vector<MPointArray>& pointSamples,
    const MPlug& meshPlug,
    const std::size_t sampling,
    const MTime& startTime,
    const double stepTime,
    const std::size_t expectedVertexCount)
{
    if (meshPlug.isNull() || !std::isfinite(stepTime) ||
        !dommy::sampling::validTimeHistoryStorage(expectedVertexCount, 1U, sampling)) {
        return MS::kInvalidParameter;
    }

    MTime endpoint;
    if (!dommy::maya::historicalSampleTime(startTime, 0.0, stepTime, 0.0,
            sampling - 1U, endpoint)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    std::vector<MPointArray> candidate;
    candidate.reserve(sampling);
    for (std::size_t index = 0U; index < sampling; ++index) {
        MTime sampleTime;
        if (!dommy::maya::historicalSampleTime(
                startTime, 0.0, stepTime, 0.0, index,
                sampleTime)) {
            return MS::kInvalidParameter;
        }

        const MDGContext context(sampleTime);
        if (!budget.readPoints(expectedVertexCount) || !budget.addPoints(expectedVertexCount))
            return MS::kInvalidParameter;
        const MDGContextGuard contextGuard(context);
        dommy::maya::ScopedPlugDataHandle meshHandleOwner;
        MDataHandle& meshHandle = meshHandleOwner.handle();
        MStatus status = meshHandleOwner.read(meshPlug);
        if (!status) return status;
        if (meshHandle.data().isNull() ||
            meshHandle.type() != MFnData::kMesh) {
            return MS::kFailure;
        }

        MFnMesh meshFn(meshHandle.asMesh(), &status);
        if (!status) return status;
        const int rawVertexCount = meshFn.numVertices(&status);
        if (!status || rawVertexCount < 0 ||
            static_cast<std::size_t>(rawVertexCount) !=
                expectedVertexCount) {
            return status ? MStatus(MS::kFailure) : status;
        }

        MPointArray points;
        status = meshFn.getPoints(points, MSpace::kWorld);
        if (!status) return status;
        if (points.length() != expectedVertexCount) return MS::kFailure;
        for (unsigned int pointIndex = 0U;
             pointIndex < points.length(); ++pointIndex) {
            if (!dommy::maya::isFloatRepresentablePoint(points[pointIndex])) {
                return MS::kInvalidParameter;
            }
        }
        candidate.emplace_back();
        DOMMY_RETURN_IF_FAILURE(candidate.back().copy(points));
        if (candidate.back().length() != expectedVertexCount) return MS::kFailure;
    }

    pointSamples.swap(candidate);
    return MS::kSuccess;
}

} // namespace


MStatus DMTimeShapeGen::compute(const MPlug& plug, MDataBlock& data) try
{
    MStatus stat;
    
    if (plug == outDM) {
        

        
#pragma mark input Object 入力オブジェクトの取得
        
        MPlug meshPlug(thisMObject(), inMesh);
        
        const bool meshConnected = meshPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!meshConnected) {
            EchoWarning(name().asChar(), "inMesh Plug isnot connected.");
            return MS::kFailure;
        }
        
        
        MDataHandle meshHnd = data.inputValue(inMesh, &stat);
        if (!stat) return stat;
        
        if (meshHnd.data().isNull() || MFnData::kMesh != meshHnd.type()) {
            EchoWarning(name().asChar(), "Connected node to inMesh Plug that not supported type.");
            return MS::kFailure;
        }
        
        MFnMesh meshFn;
        stat = meshFn.setObject(meshHnd.asMesh());
        if (!stat) return stat;
        
#pragma mark outDM の作成
        MObject outputDataObject;
        DMeshData* outDMeshes = NULL;
        stat = dommy::maya::createPluginDataObject<DMeshData>(
            DMeshData::id, outputDataObject, outDMeshes);
        if (!stat) return stat;
        
        
        if (outDMeshes) {
            
            
#pragma mark get Attribute
            
            const MDataHandle rawSamplingHandle = data.inputValue(aTimeSample, &stat);
            if (!stat) return stat;
            const int rawSampling = rawSamplingHandle.asInt();
            const MDataHandle rawStartTimeHandle = data.inputValue(aStartFrame, &stat);
            if (!stat) return stat;
            const float rawStartTime = rawStartTimeHandle.asFloat();
            const MDataHandle rawEndTimeHandle = data.inputValue(aEndFrame, &stat);
            if (!stat) return stat;
            const float rawEndTime = rawEndTimeHandle.asFloat();

            std::size_t sampling = 0U;
            if (!dommy::generation::checkedCount(
                    rawSampling,
                    dommy::generation::kMaxCurveSampleCount,
                    sampling) ||
                sampling < 2U ||
                !std::isfinite(rawStartTime) ||
                !std::isfinite(rawEndTime)) {
                return MS::kInvalidParameter;
            }

            const double startTime =
                std::min<double>(rawStartTime, rawEndTime);
            const double endTime =
                std::max<double>(rawStartTime, rawEndTime);
            
            
            
            
            
            
            // Scalar preflight precedes topology/UV extraction and historical reads.
            const int rawVertexCount = meshFn.numVertices(&stat);
            if (!stat) return stat;
            const int rawFaceCount = meshFn.numPolygons(&stat);
            if (!stat) return stat;
            const int rawConnectionCount = meshFn.numFaceVertices(&stat);
            if (!stat) return stat;
            const int rawUvCount = meshFn.numUVs(&stat);
            if (!stat) return stat;
            std::size_t meshVertexCount = 0U, meshFaceCount = 0U;
            std::size_t connectionCount = 0U, uvCount = 0U;
            if (!dommy::generation::checkedCount(rawVertexCount,
                    dommy::generation::kMaxGeneratedPointCount, meshVertexCount) ||
                !dommy::generation::checkedCount(rawFaceCount,
                    dommy::generation::kMaxGeneratedFaceCount, meshFaceCount) ||
                !dommy::generation::checkedCount(rawConnectionCount,
                    dommy::generation::kMaxWorkItemCount, connectionCount) ||
                !dommy::generation::checkedCount(rawUvCount,
                    dommy::generation::kMaxWorkItemCount, uvCount) ||
                !dommy::sampling::validTimeHistoryStorage(meshVertexCount, 1U, sampling))
                return MS::kInvalidParameter;
            const bool hasUVs = uvCount != 0U;
            std::size_t outputPointCount = 0U, outputFaceCount = 0U;
            std::size_t topologyItems = 0U, uvItems = 0U, totalTopologyItems = 0U;
            std::size_t totalWorkItemCount = 0U;
            const std::size_t workLimit = dommy::generation::kMaxWorkItemCount;
            if (!dommy::generation::checkedProduct(sampling, meshVertexCount,
                    dommy::generation::kMaxGeneratedPointCount, outputPointCount) ||
                !dommy::generation::checkedProduct(sampling, meshFaceCount,
                    dommy::generation::kMaxGeneratedFaceCount, outputFaceCount) ||
                !dommy::generation::checkedSum(meshFaceCount, connectionCount, workLimit, topologyItems))
                return MS::kInvalidParameter;
            if (hasUVs &&
                (!dommy::generation::checkedProduct(uvCount, 2U, workLimit, uvItems) ||
                 !dommy::generation::checkedSum(uvItems, topologyItems, workLimit, uvItems) ||
                 !dommy::generation::checkedSum(topologyItems, uvItems, workLimit, topologyItems)))
                return MS::kInvalidParameter;
            if (!dommy::generation::checkedProduct(topologyItems, sampling, workLimit, totalTopologyItems) ||
                !dommy::generation::checkedSum(totalTopologyItems, outputPointCount, workLimit, totalWorkItemCount))
                return MS::kInvalidParameter;
            // Include retained history, staged mesh copies, topology and group overhead.
            // This conservative estimate does not control Maya's upstream/cache memory.
            const std::size_t byteLimit = 256U * 1024U * 1024U;
            std::size_t pointBytes = 0U, topologyBytes = 0U, groupBytes = 0U, bytes = 0U;
            if (!dommy::generation::checkedProduct(outputPointCount, 128U, byteLimit, pointBytes) ||
                !dommy::generation::checkedProduct(totalTopologyItems, 32U, byteLimit, topologyBytes) ||
                !dommy::generation::checkedProduct(sampling, 128U, byteLimit, groupBytes) ||
                !dommy::generation::checkedSum(pointBytes, topologyBytes, byteLimit, bytes) ||
                !dommy::generation::checkedSum(bytes, groupBytes, byteLimit, bytes))
                return MS::kInvalidParameter;

            MIntArray faceCountArray, connectIdsArray;
            MFloatArray ucoords, vcoords;
            MIntArray uvcounts, uvconnects;
            stat = meshFn.getVertices(faceCountArray, connectIdsArray);
            if (!stat) return stat;
            if (faceCountArray.length() != meshFaceCount || connectIdsArray.length() != connectionCount)
                return MS::kFailure;
            if (hasUVs) {
                stat = meshFn.getUVs(ucoords, vcoords);
                if (!stat) return stat;
                if (ucoords.length() != uvCount || vcoords.length() != uvCount) return MS::kFailure;
                stat = meshFn.getAssignedUVs(uvcounts, uvconnects);
                if (!stat) return stat;
                if (uvcounts.length() != meshFaceCount || uvconnects.length() > connectionCount)
                    return MS::kFailure;
            }

            const double stepTime =
                (endTime - startTime) /
                static_cast<double>(sampling - 1U);
            std::vector<MPointArray> ptsArray;
            stat = sampleMeshPoints(
                ptsArray, meshPlug, sampling,
                MTime(startTime, MTime::uiUnit()), stepTime,
                meshVertexCount);
            if (!stat) return stat;
            
            

            DMeshData::DMeshList generatedMeshes;
            generatedMeshes.reserve(ptsArray.size());
            for (std::size_t i = 0U;
                 i < ptsArray.size(); ++i) {
                
                DMItem item = DMItem(new DMeshInternalData(
                    ptsArray[i], faceCountArray, connectIdsArray));
                if (hasUVs &&
                    !item->setUvData(ucoords, vcoords, uvcounts, uvconnects)) {
                    return MS::kFailure;
                }
                
                if (!item->hasValidMeshData()) return MS::kFailure;
                generatedMeshes.push_back(item);
                
            }//create mesh
            
            if (!outDMeshes->insert(generatedMeshes)) return MS::kFailure;
            outDMeshes->updateBBox();
            
            MDataHandle outHandle = data.outputValue(outDM, &stat);
            if (!stat) return stat;
            
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishPluginDataObject(
                    outHandle, outputDataObject, *outDMeshes));
            
            return data.setClean(plug);
            
            
        }//check output dmesh
        
        
        
        
        
    }
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DMTimeShapeGen")



MStatus DMTimeShapeGen::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------

    inMesh = gAttr.create("inMesh", "im",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));

    aStartFrame = nAttr.create("StartFrame", "sf", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(120.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartFrame));

    aEndFrame = nAttr.create("EndFrame", "ef", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(60.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(240.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEndFrame));

    aTimeSample = nAttr.create("TimeSample", "tsa", MFnNumericData::kLong, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(60));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(
        dommy::generation::kMaxCurveSampleCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeSample));

    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartFrame, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEndFrame, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimeSample, outDM));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMTimeShapeGen", "initialize", std::current_exception());
}
