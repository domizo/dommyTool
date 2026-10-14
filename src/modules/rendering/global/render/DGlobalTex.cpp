//
//  DGlobalTex.cpp
//  dommyTool
//
//  Created by yuzo on 3/2/16.
//

#include "DGlobalTex.h"
#include "common/utility/DBatchRange.h"

#include <maya/MDynamicsUtil.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MRenderUtil.h>
#include <maya/MString.h>

#include <cmath>
#include <cstddef>
#include <limits>

namespace {

MStatus connectedTexturePlug(
    const MObject& node,
    const MObject& attribute,
    MPlug& sourcePlug) {
    MStatus status;
    MPlug texturePlug(node, attribute);
    MPlugArray sourcePlugs;
    const bool connected =
        texturePlug.connectedTo(sourcePlugs, true, false, &status);
    if (!status) return status;
    if (!connected || sourcePlugs.length() == 0) {
        return MS::kInvalidParameter;
    }
    sourcePlug = sourcePlugs[0];
    return MS::kSuccess;
}

bool finiteFloatValue(const double value) {
    return std::isfinite(value) &&
           value >= -std::numeric_limits<float>::max() &&
           value <= std::numeric_limits<float>::max();
}

bool finitePoint(const MFloatPoint& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z) && std::isfinite(point.w);
}

bool finiteVector(const MFloatVector& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) &&
           std::isfinite(vector.z);
}

bool finiteVector(const MVector& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) &&
           std::isfinite(vector.z);
}

bool finiteMatrix(const MFloatMatrix& matrix) {
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            if (!std::isfinite(matrix(row, column))) {
                return false;
            }
        }
    }
    return true;
}

template <typename ArrayType>
bool matchesSampleCount(const ArrayType* array, unsigned int sampleCount) {
    return array == nullptr || array->length() == sampleCount;
}

MStatus copyPointBatch(const MFloatPointArray& source,
                       const dommy::batch::Range& range,
                       MFloatPointArray& destination) {
    if (range.end() > static_cast<std::size_t>(source.length())) {
        return MS::kInvalidParameter;
    }

    MStatus status = destination.setLength(
        static_cast<unsigned int>(range.count));
    if (!status) return status;
    for (std::size_t index = 0U; index < range.count; ++index) {
        const MFloatPoint& point = source[
            static_cast<unsigned int>(range.begin + index)];
        if (!finitePoint(point)) {
            return MS::kInvalidParameter;
        }
        status = destination.set(
            point, static_cast<unsigned int>(index));
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus copyFloatBatch(const MFloatArray& source,
                       const dommy::batch::Range& range,
                       MFloatArray& destination) {
    if (range.end() > static_cast<std::size_t>(source.length())) {
        return MS::kInvalidParameter;
    }

    MStatus status = destination.setLength(
        static_cast<unsigned int>(range.count));
    if (!status) return status;
    for (std::size_t index = 0U; index < range.count; ++index) {
        const float value = source[
            static_cast<unsigned int>(range.begin + index)];
        if (!std::isfinite(value)) {
            return MS::kInvalidParameter;
        }
        status = destination.set(value, static_cast<unsigned int>(index));
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus copyDoubleBatch(const MDoubleArray& source,
                        const dommy::batch::Range& range,
                        MDoubleArray& destination) {
    if (range.end() > static_cast<std::size_t>(source.length())) {
        return MS::kInvalidParameter;
    }

    MStatus status = destination.setLength(
        static_cast<unsigned int>(range.count));
    if (!status) return status;
    for (std::size_t index = 0U; index < range.count; ++index) {
        const double value = source[
            static_cast<unsigned int>(range.begin + index)];
        if (!std::isfinite(value)) {
            return MS::kInvalidParameter;
        }
        status = destination.set(value, static_cast<unsigned int>(index));
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus copyVectorBatch(const MFloatVectorArray& source,
                        const dommy::batch::Range& range,
                        MFloatVectorArray& destination) {
    if (range.end() > static_cast<std::size_t>(source.length())) {
        return MS::kInvalidParameter;
    }

    MStatus status = destination.setLength(
        static_cast<unsigned int>(range.count));
    if (!status) return status;
    for (std::size_t index = 0U; index < range.count; ++index) {
        const MFloatVector& vector = source[
            static_cast<unsigned int>(range.begin + index)];
        if (!finiteVector(vector)) {
            return MS::kInvalidParameter;
        }
        status = destination.set(
            vector, static_cast<unsigned int>(index));
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus copyVectorBatchToResult(
    const MFloatVectorArray& source,
    const dommy::batch::Range& range,
    MFloatVectorArray& destination) {
    if (static_cast<std::size_t>(source.length()) != range.count ||
        range.end() > static_cast<std::size_t>(destination.length())) {
        return MS::kFailure;
    }

    for (std::size_t index = 0U; index < range.count; ++index) {
        const MFloatVector& vector = source[
            static_cast<unsigned int>(index)];
        if (!finiteVector(vector)) {
            return MS::kFailure;
        }
        const MStatus status = destination.set(
            vector, static_cast<unsigned int>(range.begin + index));
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus copyTextureBatchToResult(
    const MVectorArray& sourceColors,
    const MDoubleArray& sourceTransparencies,
    const dommy::batch::Range& range,
    MVectorArray& destinationColors,
    MDoubleArray& destinationTransparencies) {
    if (static_cast<std::size_t>(sourceColors.length()) != range.count ||
        static_cast<std::size_t>(sourceTransparencies.length()) !=
            range.count ||
        range.end() >
            static_cast<std::size_t>(destinationColors.length()) ||
        range.end() > static_cast<std::size_t>(
                          destinationTransparencies.length())) {
        return MS::kFailure;
    }

    for (std::size_t index = 0U; index < range.count; ++index) {
        const unsigned int sourceIndex = static_cast<unsigned int>(index);
        const MVector& color = sourceColors[sourceIndex];
        const double transparency = sourceTransparencies[sourceIndex];
        if (!finiteVector(color) || !std::isfinite(transparency)) {
            return MS::kFailure;
        }
        MStatus status = destinationColors.set(
            color, static_cast<unsigned int>(range.begin + index));
        if (!status) return status;
        status = destinationTransparencies.set(
            transparency,
            static_cast<unsigned int>(range.begin + index));
        if (!status) return status;
    }
    return MS::kSuccess;
}

enum class Texture2dEvaluator {
    kRender,
    kDynamics
};

MStatus evaluate2dTextureInBatchesImpl(
    const Texture2dEvaluator evaluator,
    const MObject& node,
    const MObject& textureAttribute,
    const MDoubleArray& uCoordinates,
    const MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies) {
    const unsigned int sampleCount = uCoordinates.length();
    if (node.isNull() ||
        (evaluator == Texture2dEvaluator::kDynamics &&
         textureAttribute.isNull()) ||
        vCoordinates.length() != sampleCount) {
        return MS::kInvalidParameter;
    }

    MVectorArray sampledColors;
    MDoubleArray sampledTransparencies;
    MStatus status = sampledColors.setLength(sampleCount);
    if (!status) return status;
    status = sampledTransparencies.setLength(sampleCount);
    if (!status) return status;

    const std::size_t totalCount = static_cast<std::size_t>(sampleCount);
    for (std::size_t begin = 0U; begin < totalCount;) {
        const dommy::batch::Range range = dommy::batch::rangeAt(
            totalCount, dommy::batch::kMaximumShadingSampleCount, begin);
        if (range.empty()) return MS::kFailure;

        MDoubleArray uBatch;
        MDoubleArray vBatch;
        status = copyDoubleBatch(uCoordinates, range, uBatch);
        if (!status) return status;
        status = copyDoubleBatch(vCoordinates, range, vBatch);
        if (!status) return status;

        MVectorArray colorBatch;
        MDoubleArray transparencyBatch;
        if (evaluator == Texture2dEvaluator::kRender) {
            status = MRenderUtil::eval2dTexture(
                node, uBatch, vBatch, &colorBatch, &transparencyBatch);
        } else {
            status = MDynamicsUtil::evalDynamics2dTexture(
                node, textureAttribute, uBatch, vBatch, &colorBatch,
                &transparencyBatch);
        }
        if (!status) return status;
        status = copyTextureBatchToResult(
            colorBatch, transparencyBatch, range, sampledColors,
            sampledTransparencies);
        if (!status) return status;
        begin = range.end();
    }

    colors = sampledColors;
    transparencies = sampledTransparencies;
    return MS::kSuccess;
}

MStatus sampleShadingNetworkInBatchesImpl(
    const MString& shadingNodeName,
    bool useShadowMaps,
    bool reuseMaps,
    const MFloatMatrix& cameraMatrix,
    const MFloatPointArray& points,
    const MFloatArray* uCoordinates,
    const MFloatArray* vCoordinates,
    const MFloatVectorArray* normals,
    const MFloatPointArray* referencePoints,
    const MFloatVectorArray* tangentUs,
    const MFloatVectorArray* tangentVs,
    const MFloatArray* filterSizes,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies) {
    const unsigned int sampleCount = points.length();
    if (shadingNodeName.length() == 0U || !finiteMatrix(cameraMatrix) ||
        (uCoordinates == nullptr) != (vCoordinates == nullptr) ||
        (tangentUs == nullptr) != (tangentVs == nullptr) ||
        !matchesSampleCount(uCoordinates, sampleCount) ||
        !matchesSampleCount(vCoordinates, sampleCount) ||
        !matchesSampleCount(normals, sampleCount) ||
        !matchesSampleCount(referencePoints, sampleCount) ||
        !matchesSampleCount(tangentUs, sampleCount) ||
        !matchesSampleCount(tangentVs, sampleCount) ||
        !matchesSampleCount(filterSizes, sampleCount)) {
        return MS::kInvalidParameter;
    }

    MFloatVectorArray sampledColors;
    MFloatVectorArray sampledTransparencies;
    MStatus status = sampledColors.setLength(sampleCount);
    if (!status) return status;
    status = sampledTransparencies.setLength(sampleCount);
    if (!status) return status;

    const std::size_t totalCount =
        static_cast<std::size_t>(sampleCount);
    for (std::size_t begin = 0U; begin < totalCount;) {
        const dommy::batch::Range range = dommy::batch::rangeAt(
            totalCount, dommy::batch::kMaximumShadingSampleCount, begin);
        if (range.empty()) {
            return MS::kFailure;
        }

        MFloatPointArray pointBatch;
        status = copyPointBatch(points, range, pointBatch);
        if (!status) return status;

        MFloatArray uBatch;
        MFloatArray vBatch;
        MFloatArray* uBatchPointer = nullptr;
        MFloatArray* vBatchPointer = nullptr;
        if (uCoordinates != nullptr) {
            status = copyFloatBatch(*uCoordinates, range, uBatch);
            if (!status) return status;
            status = copyFloatBatch(*vCoordinates, range, vBatch);
            if (!status) return status;
            uBatchPointer = &uBatch;
            vBatchPointer = &vBatch;
        }

        MFloatVectorArray normalBatch;
        MFloatVectorArray* normalBatchPointer = nullptr;
        if (normals != nullptr) {
            status = copyVectorBatch(*normals, range, normalBatch);
            if (!status) return status;
            normalBatchPointer = &normalBatch;
        }

        MFloatPointArray referencePointBatch;
        MFloatPointArray* referencePointBatchPointer = nullptr;
        if (referencePoints != nullptr) {
            status = copyPointBatch(
                *referencePoints, range, referencePointBatch);
            if (!status) return status;
            referencePointBatchPointer = &referencePointBatch;
        }

        MFloatVectorArray tangentUBatch;
        MFloatVectorArray tangentVBatch;
        MFloatVectorArray* tangentUBatchPointer = nullptr;
        MFloatVectorArray* tangentVBatchPointer = nullptr;
        if (tangentUs != nullptr) {
            status = copyVectorBatch(*tangentUs, range, tangentUBatch);
            if (!status) return status;
            status = copyVectorBatch(*tangentVs, range, tangentVBatch);
            if (!status) return status;
            tangentUBatchPointer = &tangentUBatch;
            tangentVBatchPointer = &tangentVBatch;
        }

        MFloatArray filterSizeBatch;
        MFloatArray* filterSizeBatchPointer = nullptr;
        if (filterSizes != nullptr) {
            status = copyFloatBatch(
                *filterSizes, range, filterSizeBatch);
            if (!status) return status;
            filterSizeBatchPointer = &filterSizeBatch;
        }

        MFloatVectorArray colorBatch;
        MFloatVectorArray transparencyBatch;
        MFloatMatrix batchCameraMatrix(cameraMatrix);
        status = MRenderUtil::sampleShadingNetwork(
            shadingNodeName, static_cast<int>(range.count), useShadowMaps,
            reuseMaps, batchCameraMatrix, &pointBatch, uBatchPointer,
            vBatchPointer, normalBatchPointer, referencePointBatchPointer,
            tangentUBatchPointer, tangentVBatchPointer,
            filterSizeBatchPointer,
            colorBatch, transparencyBatch);
        if (!status) return status;

        status = copyVectorBatchToResult(
            colorBatch, range, sampledColors);
        if (!status) return status;
        status = copyVectorBatchToResult(
            transparencyBatch, range, sampledTransparencies);
        if (!status) return status;
        begin = range.end();
    }

    colors = sampledColors;
    transparencies = sampledTransparencies;
    return MS::kSuccess;
}

MStatus copyTextureCoordinates(
    const MDoubleArray& uCoordinates,
    const MDoubleArray& vCoordinates,
    MFloatArray& outputU,
    MFloatArray& outputV) {
    if (uCoordinates.length() != vCoordinates.length()) {
        return MS::kInvalidParameter;
    }

    MStatus status = outputU.setLength(uCoordinates.length());
    if (!status) return status;
    status = outputV.setLength(vCoordinates.length());
    if (!status) return status;
    for (unsigned int index = 0; index < uCoordinates.length(); ++index) {
        if (!finiteFloatValue(uCoordinates[index]) ||
            !finiteFloatValue(vCoordinates[index])) {
            return MS::kInvalidParameter;
        }
        status = outputU.set(
            static_cast<float>(uCoordinates[index]), index);
        if (!status) return status;
        status = outputV.set(
            static_cast<float>(vCoordinates[index]), index);
        if (!status) return status;
    }
    return MS::kSuccess;
}

} // namespace

MStatus sampleShadingNetworkInBatches(
    const MString& shadingNodeName,
    const bool useShadowMaps,
    const bool reuseMaps,
    const MFloatMatrix& cameraMatrix,
    const MFloatPointArray& points,
    const MFloatArray* uCoordinates,
    const MFloatArray* vCoordinates,
    const MFloatVectorArray* normals,
    const MFloatPointArray* referencePoints,
    const MFloatVectorArray* tangentUs,
    const MFloatVectorArray* tangentVs,
    const MFloatArray* filterSizes,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies) {
    return sampleShadingNetworkInBatchesImpl(
        shadingNodeName, useShadowMaps, reuseMaps, cameraMatrix, points,
        uCoordinates, vCoordinates, normals, referencePoints, tangentUs,
        tangentVs, filterSizes, colors, transparencies);
}

MStatus evaluate2dTextureInBatches(
    const MObject& textureNode,
    const MDoubleArray& uCoordinates,
    const MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies) {
    return evaluate2dTextureInBatchesImpl(
        Texture2dEvaluator::kRender, textureNode, MObject::kNullObj,
        uCoordinates, vCoordinates, colors, transparencies);
}

MStatus evaluateDynamics2dTextureInBatches(
    const MObject& node,
    const MObject& textureAttribute,
    const MDoubleArray& uCoordinates,
    const MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies) {
    return evaluate2dTextureInBatchesImpl(
        Texture2dEvaluator::kDynamics, node, textureAttribute,
        uCoordinates, vCoordinates, colors, transparencies);
}

MStatus getColorFromTex(
    const MObject& node,
    const MObject& plug,
    MDoubleArray& uCoordinates,
    MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies) {
    if (uCoordinates.length() != vCoordinates.length()) {
        return MS::kInvalidParameter;
    }
    for (unsigned int index = 0; index < uCoordinates.length(); ++index) {
        if (!std::isfinite(uCoordinates[index]) ||
            !std::isfinite(vCoordinates[index])) {
            return MS::kInvalidParameter;
        }
    }

    MPlug sourcePlug;
    MStatus status = connectedTexturePlug(node, plug, sourcePlug);
    if (!status) return status;

    return evaluate2dTextureInBatches(
        sourcePlug.node(), uCoordinates, vCoordinates, colors,
        transparencies);
}

MStatus getColorFromShaderNetwork(
    const MObject& node,
    const MObject& plug,
    MFloatPointArray& points,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies) {
    MPlug sourcePlug;
    MStatus status = connectedTexturePlug(node, plug, sourcePlug);
    if (!status) return status;

    const MString sourcePlugName = sourcePlug.name(&status);
    if (!status) return status;
    const MFloatMatrix cameraMatrix;
    return sampleShadingNetworkInBatches(
        sourcePlugName, false, false, cameraMatrix, points, nullptr,
        nullptr, nullptr, &points, nullptr, nullptr, nullptr, colors,
        transparencies);
}

MStatus getColorFromShaderNetwork(
    const MObject& node,
    const MObject& plug,
    MFloatPointArray& points,
    MDoubleArray& uCoordinates,
    MDoubleArray& vCoordinates,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies) {
    if (points.length() != uCoordinates.length() ||
        points.length() != vCoordinates.length()) {
        return MS::kInvalidParameter;
    }

    MFloatArray uArray;
    MFloatArray vArray;
    MStatus status = copyTextureCoordinates(
        uCoordinates, vCoordinates, uArray, vArray);
    if (!status) return status;

    MPlug sourcePlug;
    status = connectedTexturePlug(node, plug, sourcePlug);
    if (!status) return status;

    const MString sourcePlugName = sourcePlug.name(&status);
    if (!status) return status;
    const MFloatMatrix cameraMatrix;
    return sampleShadingNetworkInBatches(
        sourcePlugName, false, false, cameraMatrix, points, &uArray,
        &vArray, nullptr, &points, nullptr, nullptr, nullptr, colors,
        transparencies);
}
