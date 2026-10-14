//
//  DCMeshCopy.cpp
//  dommyTool
//
//  Created by yuzo on 3/7/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCMeshCopy.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"

#include "DMayaHeaders.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "DNodeOperation.h"
#include "DCurveCopyMath.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DDeformerMath.h"

const MTypeId DCMeshCopy::typeId(ID_DC_MESH_COPY);

MObject DCMeshCopy::inCopyObject;

MObject DCMeshCopy::inMesh;

MObject DCMeshCopy::inReferenceObject;

MObject DCMeshCopy::aCount;

MObject DCMeshCopy::aPadding;
MObject DCMeshCopy::aRandomPadding;

MObject DCMeshCopy::aHeightScale;
MObject DCMeshCopy::aRandomHeight;

MObject DCMeshCopy::aDirectionMode;

MObject DCMeshCopy::aSeed;

MObject DCMeshCopy::outDC;

void* DCMeshCopy::creator() {
    return ::dommy::maya::createObject("DCMeshCopy", [&]() {
        return new DCMeshCopy();
    });
}

namespace {

// Traverse a bounded, immutable mesh twice: plan first, then emit. Preserve the
// legacy policy that only UV-bearing triangles/quads participate, even in Simple.
template <typename Visit>
MStatus visitCopyFaces(MItMeshPolygon& faces, std::size_t expectedFaces, Visit visit) {
    DOMMY_RETURN_IF_FAILURE(faces.reset());
    MStatus status;
    for (std::size_t visited = 0U;; ++visited) {
        const bool done = faces.isDone(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (done)
            return visited == expectedFaces ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
        if (visited >= expectedFaces)
            return MS::kFailure;
        const unsigned int vertices = faces.polygonVertexCount(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (vertices == 3U || vertices == 4U) {
            const bool hasUvs = faces.hasUVs(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (hasUvs)
                DOMMY_RETURN_IF_FAILURE(visit(vertices));
        }
        DOMMY_RETURN_IF_FAILURE(faces.next());
    }
}

} // namespace

MStatus DCMeshCopy::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDC))
        return MS::kUnknownParameter;
    MStatus status;
    const bool copyConnected = MPlug(thisMObject(), inCopyObject).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!copyConnected) return MS::kFailure;
    MObject copyObject;
    DOMMY_RETURN_IF_FAILURE(DGetMayaCurveFromPlug(inCopyObject, data, copyObject));
    MFnNurbsCurve copyCurve(copyObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawCvCount = copyCurve.numCVs(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t copyCvCount = 0U;
    if (!dommy::generation::checkedCount(rawCvCount, dommy::generation::kMaxGeneratedPointCount,
                                         copyCvCount) ||
        copyCvCount == 0U)
        return MS::kInvalidParameter;
    const int copyDegree = copyCurve.degree(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    MPointArray copyCvs;
    MDoubleArray copyKnots;
    DOMMY_RETURN_IF_FAILURE(copyCurve.getCVs(copyCvs, MSpace::kObject));
    DOMMY_RETURN_IF_FAILURE(copyCurve.getKnots(copyKnots));
    if (copyCvs.length() != copyCvCount ||
        !DCurveInternalData(copyCvs, copyKnots, copyDegree).hasValidCurveTopology())
        return MS::kInvalidParameter;
    std::vector<std::array<double, 2>> copyCoordinates;
    if (!dommy::curve::normalizedCopyCoordinates(copyCvs, copyCoordinates))
        return MS::kInvalidParameter;

    const MDataHandle perCountHandle = data.inputValue(aCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int perCount = perCountHandle.asInt();
    const MDataHandle heightScaleHandle = data.inputValue(aHeightScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float heightScale = heightScaleHandle.asFloat();
    const MDataHandle randomHeightHandle = data.inputValue(aRandomHeight, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float randomHeight = randomHeightHandle.asFloat();
    const MDataHandle directionModeHandle = data.inputValue(aDirectionMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short directionMode = directionModeHandle.asShort();
    const MDataHandle paddingHandle = data.inputValue(aPadding, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float2& padding = paddingHandle.asFloat2();
    const MDataHandle randomPaddingHandle = data.inputValue(aRandomPadding, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float2& randomPadding = randomPaddingHandle.asFloat2();
    const MDataHandle rawSeedHandle = data.inputValue(aSeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSeed = rawSeedHandle.asInt();

    if (perCount < 0 || (directionMode != 0 && directionMode != 1) || !std::isfinite(heightScale) ||
        !std::isfinite(randomHeight))
        return MS::kInvalidParameter;
    for (unsigned int axis = 0U; axis < 2U; ++axis)
        if (!std::isfinite(padding[axis]) || !std::isfinite(randomPadding[axis]))
            return MS::kInvalidParameter;
    const int seed = dommy::random::requireOffsetNonnegativeSeed(rawSeed, 0U);
    const int seedY = dommy::random::requireOffsetNonnegativeSeed(seed, 1U);
    const int seedZ = dommy::random::requireOffsetNonnegativeSeed(seed, 2U);
    DCurveOutputBatch generatedCurves;

    // Count historically acts as an enable switch, not a multiplier. Keep the
    // dormant referenceObject attribute for existing scenes; do not invent a use.
    const bool meshConnected = MPlug(thisMObject(), inMesh).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (meshConnected && perCount != 0) {
        const MDataHandle handle = data.inputValue(inMesh, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (handle.type() != MFnData::kMesh ||
            !dommy::maya::isFiniteMatrix(handle.geometryTransformMatrix()))
            return MS::kInvalidParameter;
        // Bake a supplied geometry transform once, then read without a DAG path.
        const MObject meshObject = handle.asMeshTransformed();
        if (meshObject.isNull())
            return MS::kFailure;
        MFnMesh mesh(meshObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int rawPointCount = mesh.numVertices(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int rawFaceCount = mesh.numPolygons(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t pointCount = 0U, faceCount = 0U;
        if (!dommy::generation::checkedCount(
                rawPointCount, dommy::generation::kMaxGeneratedPointCount, pointCount) ||
            !dommy::generation::checkedCount(rawFaceCount,
                                             dommy::generation::kMaxGeneratedFaceCount, faceCount))
            return MS::kInvalidParameter;
        MItMeshPolygon faces(meshObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t eligibleCount = 0U;
        DOMMY_RETURN_IF_FAILURE(visitCopyFaces(faces, faceCount, [&](unsigned int) -> MStatus {
            return dommy::generation::checkedSum(
                       eligibleCount, 1U, dommy::generation::kMaxGeneratedCurveCount, eligibleCount)
                       ? MStatus(MS::kSuccess)
                       : MStatus(MS::kInvalidParameter);
        }));
        std::size_t outputCvCount = 0U, workCount = 0U;
        if (!dommy::generation::checkedProduct(eligibleCount, copyCvCount,
                                               dommy::generation::kMaxGeneratedPointCount,
                                               outputCvCount) ||
            !dommy::generation::checkedProduct(outputCvCount, 4U,
                                               dommy::generation::kMaxWorkItemCount, workCount))
            return MS::kInvalidParameter;

        if (eligibleCount != 0U) {
            MPointArray points;
            DOMMY_RETURN_IF_FAILURE(mesh.getPoints(points, MSpace::kObject));
            if (points.length() != pointCount)
                return MS::kFailure;
            for (unsigned int i = 0U; i < points.length(); ++i)
                if (!dommy::maya::isFinitePoint(points[i]))
                    return MS::kInvalidParameter;
            std::vector<float> randomU, randomV, randomH;
            // The original selected-face sequence consumed the prefix of these
            // inclusive distributions; skipped faces must not advance the seed index.
            dommy::random::generateValues(static_cast<std::uint32_t>(seed), -randomPadding[0],
                                          randomPadding[0], eligibleCount, randomU, true);
            dommy::random::generateValues(static_cast<std::uint32_t>(seedY), -randomPadding[1],
                                          randomPadding[1], eligibleCount, randomV, true);
            dommy::random::generateValues(static_cast<std::uint32_t>(seedZ), -randomHeight,
                                          randomHeight, eligibleCount, randomH, true);
            std::size_t offset = 0U;
            DOMMY_RETURN_IF_FAILURE(
                visitCopyFaces(faces, faceCount, [&](unsigned int corners) -> MStatus {
                    if (offset >= eligibleCount)
                        return MS::kFailure;
                    MIntArray ids;
                    DOMMY_RETURN_IF_FAILURE(faces.getVertices(ids));
                    if (ids.length() != corners)
                        return MS::kInvalidParameter;
                    for (unsigned int i = 0U; i < corners; ++i)
                        if (ids[i] < 0 || static_cast<std::size_t>(ids[i]) >= pointCount)
                            return MS::kInvalidParameter;
                    unsigned int top = corners == 3U ? 2U : 1U;
                    if (directionMode == 1) {
                        MFloatArray u, v;
                        DOMMY_RETURN_IF_FAILURE(faces.getUVs(u, v));
                        if (u.length() != corners ||
                            !dommy::curve::checkedCopyUvTopIndex(u, v, top))
                            return MS::kInvalidParameter;
                    }
                    const unsigned int firstBase = corners == 3U ? top : (top + 3U) % 4U;
                    const unsigned int secondBase =
                        corners == 3U ? (top + 1U) % 3U : (top + 2U) % 4U;
                    const unsigned int secondEnd =
                        corners == 3U ? (top + 2U) % 3U : (top + 1U) % 4U;
                    MVector normal;
                    DOMMY_RETURN_IF_FAILURE(faces.getNormal(normal, MSpace::kObject));
                    if (!dommy::maya::isFiniteVector(normal))
                        return MS::kInvalidParameter;
                    const double padU = double(padding[0]) + randomU[offset];
                    const double padV = double(padding[1]) + randomV[offset];
                    const double height = double(heightScale) + randomH[offset];
                    MPointArray cvs;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
                        copyCvCount,
                        [&](unsigned int index, MPoint& result) -> MStatus {
                            const double u = padU * 0.5 + (1.0 - padU) * copyCoordinates[index][0];
                            const double v = padV * 0.5 + (1.0 - padV) * copyCoordinates[index][1];
                            MPoint first, second, surface;
                            if (!dommy::deformer::blendPoint(points[ids[firstBase]],
                                                             points[ids[top]], u, first) ||
                                !dommy::deformer::blendPoint(points[ids[secondBase]],
                                                             points[ids[secondEnd]], u, second) ||
                                !dommy::deformer::blendPoint(first, second, v, surface) ||
                                !dommy::deformer::offsetPoint(surface, normal, copyCvs[index].y,
                                                              height, 1.0, 1.0, 1.0, result) ||
                                !dommy::maya::isFloatRepresentablePoint(result))
                                return MS::kInvalidParameter;
                            return MS::kSuccess;
                        },
                        cvs));
                    if (!generatedCurves.append(
                            DCItem(new DCurveInternalData(cvs, copyKnots, copyDegree))))
                        return MS::kFailure;
                    ++offset;
                    return MS::kSuccess;
                }));
            if (offset != eligibleCount)
                return MS::kFailure;
        }
    }
    MFnPluginData pluginData;
    DCurveData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output || !generatedCurves.publishTo(*output))
        return MS::kFailure;
    output->updateBBox();
    MDataHandle handle = data.outputValue(outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        handle, pluginData.object(), DCurveData::id));
    handle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCMeshCopy")

MStatus DCMeshCopy::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inCopyObject = gAttr.create("inCopyObject", "ico", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCopyObject));

    inMesh = gAttr.create("inMesh", "im", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));

    inReferenceObject = gAttr.create("referenceObject", "refo", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inReferenceObject));

    aCount = nAttr.create("Count", "cnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(8));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));

    aHeightScale = nAttr.create("HeightScale", "hscl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aHeightScale));

    aRandomHeight = nAttr.create("RandomHeight", "rndht", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomHeight));

    MFnEnumAttribute dirEnum;
    aDirectionMode = dirEnum.create("DirectionMode", "dirm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dirEnum.addField("Simple", 0));
    DOMMY_RETURN_IF_FAILURE(dirEnum.addField("UV Up Direction", 1));
    DOMMY_RETURN_IF_FAILURE(dirEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDirectionMode));

    aPadding = nAttr.create("Padding", "pad", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPadding));

    aRandomPadding = nAttr.create("RandomPadding", "rndpad", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomPadding));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inCopyObject, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inReferenceObject, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCount, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPadding, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomPadding, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aHeightScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomHeight, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDirectionMode, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));

    return MS::kSuccess;
}
