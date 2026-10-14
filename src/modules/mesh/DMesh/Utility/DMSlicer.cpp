//
//  DMSlicer.cpp
//  dommyTool
//
//  Created by yuzo on 4/23/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMSlicer.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DSliceBudget.h"
#include "geometry/DTrianglePlaneSlicer.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DMeshDataHeaders.h"

#include <cmath>
#include <vector>

const MTypeId DMSlicer::typeId(ID_DM_SLICER);

MObject DMSlicer::inDM;

MObject DMSlicer::aPlanePosition;
MObject DMSlicer::aPlaneNormal;

MObject DMSlicer::aCutCount;
MObject DMSlicer::aStep;

// curve charactor
MObject DMSlicer::aDegree;
MObject DMSlicer::aIsOpen;
MObject DMSlicer::aIsMultiEndKnot;

MObject DMSlicer::outDC;

void* DMSlicer::creator() {
    return ::dommy::maya::createObject("DMSlicer", [&]() {
        return new DMSlicer();
    });
}

MStatus DMSlicer::compute(const MPlug& plug, MDataBlock& data) try {

    MStatus stat;

    if (plug == outDC) {

#pragma mark get input dmesh
        const DMeshData* inputDMData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputDMData));
        if (!inputDMData || !inputDMData->hasValidMeshTopology()) return MS::kInvalidParameter;

        if (inputDMData) {

            // create output data
            MFnPluginData pluginData;
            pluginData.create(DCurveData::id, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);

            DCurveData* const outDCrvs(
                dommy::maya::pluginDataPointer<DCurveData>(pluginData, &stat));
            if (stat != MS::kSuccess || !outDCrvs) {
                return MS::kFailure;
            }

            size_t meshCount = inputDMData->numberOfMesh();
            DCurveOutputBatch generatedCurves;

            if (meshCount > 0) {
                MDataHandle inputHandle = data.inputValue(aPlanePosition, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const float3& planePos = inputHandle.asFloat3();
                const MVector planeBasePos(planePos[0], planePos[1], planePos[2]);

                inputHandle = data.inputValue(aPlaneNormal, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const float3& planeNormal = inputHandle.asFloat3();
                MVector planeDirection(planeNormal[0], planeNormal[1], planeNormal[2]);

                inputHandle = data.inputValue(aCutCount, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const int rawCutCount = inputHandle.asInt();
                std::size_t cutCount = 0U;
                std::size_t sliceWork = 0U;
                if (!dommy::generation::checkedCount(
                        rawCutCount, dommy::generation::kMaxGeneratedCurveCount, cutCount) ||
                    cutCount == 0U ||
                    !dommy::generation::checkedProduct(
                        meshCount, cutCount, dommy::generation::kMaxWorkItemCount, sliceWork) ||
                    sliceWork == 0U) {
                    MGlobal::displayError("DMSlicer cut count exceeds the generation budget.");
                    return MS::kFailure;
                }

                inputHandle = data.inputValue(aStep, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const float step = inputHandle.asFloat();

                inputHandle = data.inputValue(aDegree, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const short rawDegree = inputHandle.asShort();
                if (rawDegree != 0 && rawDegree != 1) return MS::kInvalidParameter;
                const short degree = rawDegree == 0 ? 1 : 3;
                inputHandle = data.inputValue(aIsOpen, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const short rawIsOpen = inputHandle.asShort();
                if (rawIsOpen != 0 && rawIsOpen != 1) return MS::kInvalidParameter;
                const bool isOpen = rawIsOpen == 1;
                inputHandle = data.inputValue(aIsMultiEndKnot, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                const bool isMultiEndKnot = inputHandle.asBool();

                const double normalLength = planeDirection.length();
                if (!std::isfinite(planeBasePos.x) || !std::isfinite(planeBasePos.y) ||
                    !std::isfinite(planeBasePos.z) || !std::isfinite(normalLength) ||
                    normalLength <= 1.0e-12 || !std::isfinite(step)) {
                    MGlobal::displayError("DMSlicer requires a finite position, step, "
                                          "and non-zero normal.");
                    return MS::kFailure;
                }
                planeDirection /= normalLength;
                std::size_t generatedPointCount = 0U;

                // Bound all input staging before native conversion. The estimate
                // does not include Maya's internal mesh/triangulation caches.
                dommy::mesh::MeshMergePlan inputStorage;
                for (auto mesh = inputDMData->beginMeshes(); mesh != inputDMData->endMeshes(); ++mesh) {
                    if (!*mesh || (*mesh)->numFace() < 0 ||
                        !dommy::mesh::appendMeshMergePlan(
                            (*mesh)->numberOfPoints(), static_cast<std::size_t>((*mesh)->numFace()),
                            (*mesh)->numberOfConnections(), (*mesh)->numberOfUVs(), inputStorage) ||
                        !dommy::mesh::validMeshOperationStorage(inputStorage)) return MS::kInvalidParameter;
                }
                struct PreparedMesh {
                    MObject object;
                    std::size_t points, faces, triangles;
                };
                std::vector<PreparedMesh> prepared;
                prepared.reserve(meshCount);
                dommy::mesh::SliceBudget budget;
                dommy::generation::WorkBudget sliceWorkBudget;
                for (auto mIt = inputDMData->beginMeshes(); mIt != inputDMData->endMeshes();
                     ++mIt) {
                    MObject newObj;
                    stat = generateMMeshFromDmeshWithUV(*mIt->get(), newObj);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    if (newObj.isNull()) {
                        return MS::kFailure;
                    }
                    MFnMesh meshFn(newObj, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    const int rawPoints = meshFn.numVertices(&stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    std::size_t pointCount = 0U, faceCount = 0U, triangleCount = 0U;
                    if (!dommy::generation::checkedCount(rawPoints,
                            dommy::generation::kMaxGeneratedPointCount, pointCount))
                        return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshTriangles(
                        meshFn, newObj, faceCount, triangleCount));
                    if (!dommy::mesh::appendSliceBudget(pointCount, faceCount, triangleCount,
                                                       cutCount, budget)) {
                        MGlobal::displayError("DMSlicer total point/triangle traversal exceeds the budget.");
                        return MS::kInvalidParameter;
                    }
                    prepared.push_back(PreparedMesh{newObj, pointCount, faceCount, triangleCount});
                }

                // No slicing or bulk read until every primitive passes preflight.
                for (const PreparedMesh& mesh : prepared) {
                    if (!sliceWorkBudget.consume(1U)) return MS::kFailure;
                    MFnMesh meshFn(mesh.object, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);

                    MPointArray pts;
                    DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(pts, MSpace::kObject));

                    MIntArray triangleCounts, triangleVertices;
                    DOMMY_RETURN_IF_FAILURE(
                        meshFn.getTriangles(triangleCounts, triangleVertices));

                    if (!dommy::maya::validMeshTriangleArrays(pts, triangleCounts,
                            triangleVertices, mesh.points, mesh.faces, mesh.triangles)) {
                        return MS::kInvalidParameter;
                    }

                    std::vector<dommy::geometry::Point3> geometryPoints;
                    if (!sliceWorkBudget.consume(mesh.points) ||
                        !sliceWorkBudget.consumeProduct(mesh.triangles, 2U)) return MS::kFailure;
                    geometryPoints.reserve(pts.length());
                    for (unsigned int pointIndex = 0U;
                         pointIndex < pts.length(); ++pointIndex) {
                        geometryPoints.push_back(dommy::geometry::Point3(
                            pts[pointIndex].x,
                            pts[pointIndex].y,
                            pts[pointIndex].z));
                    }
                    std::vector<dommy::geometry::Triangle>
                        geometryTriangles;
                    geometryTriangles.reserve(
                        triangleVertices.length() / 3U);
                    for (unsigned int triangleIndex = 0U;
                         triangleIndex < triangleVertices.length();
                         triangleIndex += 3U) {
                        const int first = triangleVertices[triangleIndex];
                        const int second =
                            triangleVertices[triangleIndex + 1U];
                        const int third =
                            triangleVertices[triangleIndex + 2U];
                        if (first < 0 || second < 0 || third < 0 ||
                            static_cast<std::size_t>(first) >= mesh.points ||
                            static_cast<std::size_t>(second) >= mesh.points ||
                            static_cast<std::size_t>(third) >= mesh.points) {
                            return MS::kInvalidParameter;
                        }
                        geometryTriangles.push_back(
                            dommy::geometry::Triangle(
                                static_cast<std::size_t>(first),
                                static_cast<std::size_t>(second),
                                static_cast<std::size_t>(third)));
                    }

                    for (size_t i = 0; i < cutCount; ++i) {
                        if (!sliceWorkBudget.consume(1U)) return MS::kFailure;
                        const MVector finPos =
                            planeBasePos + planeDirection * step * static_cast<double>(i);
                        dommy::geometry::Plane3 plane;
                        if (!dommy::geometry::makePlane(
                                dommy::geometry::Point3(
                                    finPos[0], finPos[1], finPos[2]),
                                dommy::geometry::Point3(
                                    planeDirection[0],
                                    planeDirection[1],
                                    planeDirection[2]),
                                plane)) {
                            return MS::kInvalidParameter;
                        }

                        std::vector<dommy::geometry::Polyline3> polylines;
                        const std::size_t remainingPointBudget =
                            dommy::generation::kMaxGeneratedPointCount -
                            generatedPointCount;
                        if (remainingPointBudget < 2U ||
                            !dommy::geometry::sliceTriangleMesh(
                                geometryPoints, geometryTriangles, plane,
                                polylines,
                                dommy::generation::kMaxGeneratedFaceCount,
                                remainingPointBudget, sliceWorkBudget)) {
                            MGlobal::displayError(
                                "DMSlicer could not construct a bounded "
                                "slice contour.");
                            return MS::kFailure;
                        }

                        for (std::vector<dommy::geometry::Polyline3>::
                                 const_iterator curveIt = polylines.begin();
                             curveIt != polylines.end(); ++curveIt) {
                            if (!sliceWorkBudget.consume(1U)) return MS::kFailure;
                            const std::size_t pointCount = curveIt->size();
                            if (pointCount < static_cast<std::size_t>(degree + 1)) {
                                continue;
                            }

                            std::size_t nextPointCount = 0U;
                            if (!dommy::generation::checkedSum(
                                    generatedPointCount, pointCount,
                                    dommy::generation::kMaxGeneratedPointCount, nextPointCount)) {
                                MGlobal::displayError("DMSlicer output exceeds the "
                                                      "generation budget.");
                                return MS::kFailure;
                            }

                            // CV/knots construction and topology validation are
                            // linear; append the complete batch only once below.
                            if (!sliceWorkBudget.consumeProduct(pointCount, 12U)) return MS::kFailure;
                            MPointArrayPtr cvs(new MPointArray());
                            DOMMY_RETURN_IF_FAILURE(cvs->setLength(static_cast<unsigned int>(pointCount)));
                            if (cvs->length() != pointCount) return MS::kFailure;
                            std::size_t offset = 0U;
                            for (dommy::geometry::Polyline3::const_iterator
                                     pointIt = curveIt->begin();
                                 pointIt != curveIt->end(); ++pointIt, ++offset) {
                                if (!dommy::geometry::isFinite(*pointIt)) {
                                    return MS::kFailure;
                                }
                                (*cvs)[static_cast<unsigned int>(offset)] =
                                    MPoint(
                                        pointIt->x,
                                        pointIt->y,
                                        pointIt->z);
                            }

                            MDoubleArrayPtr knots(new MDoubleArray());
                            GenerateDCurveKnots(pointCount, degree, *knots, isMultiEndKnot);
                            DCItem curve(new DCurveInternalData(*cvs, *knots, degree, isOpen));
                            if (!curve->hasValidCurveTopology() || !generatedCurves.append(curve)) {
                                return MS::kFailure;
                            }
                            generatedPointCount = nextPointCount;
                        }
                    }
                }
                if (!sliceWorkBudget.consumeProduct(generatedPointCount, 6U)) return MS::kFailure;
            }

            if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;
            outDCrvs->updateBBox();

            MDataHandle output = data.outputValue(outDC, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishTypedPluginDataObject<DCurveData>(
                    output, pluginData.object(), DCurveData::id));
            output.setClean();

            return data.setClean(plug);
        }
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DMSlicer")

MStatus DMSlicer::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aPlanePosition = nAttr.create("PlanePosition", "ppos", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlanePosition));

    aPlaneNormal = nAttr.create("PlaneNormal", "pnorm", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlaneNormal));

    aCutCount = nAttr.create("CutCount", "ccnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCutCount));

    aStep = nAttr.create("Step", "stp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStep));

    MFnEnumAttribute degEnumFn;
    aDegree = degEnumFn.create("Degree", "deg", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(degEnumFn.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(degEnumFn.addField("Cubic", 1));
    DOMMY_RETURN_IF_FAILURE(degEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));

    MFnEnumAttribute openEnumFn;
    aIsOpen = openEnumFn.create("IsOpen", "isop", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(openEnumFn.addField("Close", 0));
    DOMMY_RETURN_IF_FAILURE(openEnumFn.addField("Open", 1));
    DOMMY_RETURN_IF_FAILURE(openEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOpen));

    aIsMultiEndKnot = nAttr.create("IsMultiEndKnot", "imek", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMultiEndKnot));

    // create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPlanePosition, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPlaneNormal, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCutCount, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStep, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDegree, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsOpen, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsMultiEndKnot, outDC));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMSlicer", "initialize", std::current_exception());
}
