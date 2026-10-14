//
//  DNodeOperation.cpp
//  dommyTool
//
//  Created by yuzo on 5/9/16.
//
//

#include "DMayaHeaders.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include "DNodeOperation.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaMeshInput.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "math/DBounds.h"
#include "math/DFrameAlignment.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DTimeSamplingBudget.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DAttributeArrayConversion.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#pragma mark dag node method

MStatus getChildNodeFromNode(const MFnDagNode& dagNode, MObjectArray& childArray,
                            std::size_t maxChildren) {
    MStatus status;
    const unsigned int childCount = dagNode.childCount(&status);
    if (!status)
        return status;
    if (childCount > maxChildren || childCount > dommy::generation::kMaxConnectedGeometryCount)
        return MS::kInvalidParameter;

    for (unsigned int index = 0U; index < childCount; ++index) {
        const MObject child = dagNode.child(index, &status);
        if (!status)
            return status;
        status = childArray.append(child);
        if (!status)
            return status;
    }
    return MS::kSuccess;
}

MStatus extractDagNodeFromTypeName(const MString& typeName, const MObjectArray& objectArray,
                                   MObjectArray& extractedArray, std::size_t* remainingVisits) {
    std::size_t localRemaining = dommy::generation::kMaxConnectedGeometryCount;
    std::size_t& remaining = remainingVisits ? *remainingVisits : localRemaining;
    if (remaining > dommy::generation::kMaxConnectedGeometryCount || objectArray.length() > remaining)
        return MS::kInvalidParameter;
    remaining -= objectArray.length();
    std::vector<MObject> pending;
    pending.reserve(objectArray.length());
    for (unsigned int index = 0U; index < objectArray.length(); ++index) {
        pending.push_back(objectArray[index]);
    }

    while (!pending.empty()) {
        const MObject object = pending.back();
        pending.pop_back();

        MStatus status;
        MFnDagNode node(object, &status);
        if (!status)
            return status;

        const MString nodeType = node.typeName(&status);
        if (!status) return status;
        if (nodeType == typeName) {
            const bool intermediate = node.isIntermediateObject(&status);
            if (!status) return status;
            if (!intermediate) {
                if (extractedArray.length() >= dommy::generation::kMaxGeneratedCurveCount)
                    return MS::kInvalidParameter;
                status = extractedArray.append(object);
                if (!status) return status;
            }
        }

        const unsigned int childCount = node.childCount(&status);
        if (!status)
            return status;
        if (childCount > remaining) return MS::kInvalidParameter;
        remaining -= childCount;
        for (unsigned int index = childCount; index > 0U; --index) {
            const MObject child = node.child(index - 1U, &status);
            if (!status)
                return status;
            pending.push_back(child);
        }
    }
    return MS::kSuccess;
}

MStatus getNurbsCurveNodeFromHierarchy(const MFnDagNode& parentNode, MObjectArray& curveNodes,
                                      std::size_t* remainingVisits) {
    MObjectArray childArray;
    MStatus status = getChildNodeFromNode(parentNode, childArray,
        remainingVisits ? *remainingVisits : dommy::generation::kMaxConnectedGeometryCount);
    if (!status)
        return status;
    return extractDagNodeFromTypeName(MString("nurbsCurve"), childArray, curveNodes, remainingVisits);
}

MStatus DGetMayaCurveFromPlug(const MObject& inputPlugObject, MDataBlock& data, MObject& dataObj) {
    MStatus status;
    const MDataHandle handle = data.inputValue(inputPlugObject, &status);
    if (!status) return status;
    if (handle.type() != MFnData::kNurbsCurve ||
        !dommy::maya::isFiniteMatrix(handle.geometryTransformMatrix()))
        return MS::kInvalidParameter;

    // Bake a supplied geometry transform once. Callers must only read the
    // returned data and query it in kObject space (it has no DAG path).
    const MObject candidate = handle.asNurbsCurveTransformed();
    if (candidate.isNull()) return MS::kFailure;
    dataObj = candidate;
    return MS::kSuccess;
}

bool DGetMayaSurfaceFromPlug(const MObject& inputPlugObject, MDataBlock& data, MObject& dataObj) {
    MStatus status;
    MDataHandle dataHnd = data.inputValue(inputPlugObject, &status);
    if (!status) return false;

    if (dataHnd.type() != MFnData::kNurbsSurface) {
        return false;
    }

    if (!dommy::maya::isFiniteMatrix(dataHnd.geometryTransformMatrix())) return false;
    // Both callers are readers. Bake a supplied geometry transform exactly
    // once, then query this read-only surface with MSpace::kObject.
    const MObject candidate = dataHnd.asNurbsSurfaceTransformed();
    if (candidate.isNull()) {
        return false;
    }
    dataObj = candidate;
    return true;
}

bool DGetMayaMeshFromPlug(const MObject& inputPlugObject, MDataBlock& data, MObject& dataObj) {
    return dommy::maya::getMeshObject(inputPlugObject, data, dataObj) == MS::kSuccess;
}

#pragma mark plug method

namespace {

template <typename Value> MStatus readScalarPlugValue(const MPlug& plug, Value& value) {
    if (plug.isNull()) {
        return MS::kFailure;
    }

    Value candidate = Value();
    const MStatus status = plug.getValue(candidate);
    if (status != MS::kSuccess) {
        return status;
    }

    value = candidate;
    return MS::kSuccess;
}

template <typename Value>
MStatus readNodePlugValue(const MObject& node, const MObject& attribute, Value& value) {
    if (node.isNull() || attribute.isNull()) {
        return MS::kFailure;
    }
    return readPlugValue(MPlug(node, attribute), value);
}

} // namespace

MStatus readPlugValue(const MPlug& plug, bool& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, short& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, int& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, float& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, double& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, MObject& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, MString& value) {
    return readScalarPlugValue(plug, value);
}

MStatus readPlugValue(const MPlug& plug, MVector& value) {
    MStatus status;
    if (plug.isNull() || plug.numChildren(&status) != 3 || status != MS::kSuccess) {
        return MS::kFailure;
    }

    double components[3] = {0.0, 0.0, 0.0};
    for (unsigned int index = 0; index < 3; ++index) {
        const MPlug child = plug.child(index, &status);
        if (status != MS::kSuccess || child.isNull()) {
            return status == MS::kSuccess ? MS::kFailure : status;
        }
        status = child.getValue(components[index]);
        if (status != MS::kSuccess) {
            return status;
        }
    }

    value = MVector(components[0], components[1], components[2]);
    return MS::kSuccess;
}

MStatus readPlugValue(const MPlug& plug, MColor& value) {
    MVector candidate;
    const MStatus status = readPlugValue(plug, candidate);
    if (status != MS::kSuccess) {
        return status;
    }

    value = MColor(MColor::kRGB, static_cast<float>(candidate.x), static_cast<float>(candidate.y),
                   static_cast<float>(candidate.z));
    return MS::kSuccess;
}

MStatus readPlugValue(const MObject& node, const MObject& attribute, bool& value) {
    return readNodePlugValue(node, attribute, value);
}

MStatus readPlugValue(const MObject& node, const MObject& attribute, int& value) {
    return readNodePlugValue(node, attribute, value);
}

MStatus readPlugValue(const MObject& node, const MObject& attribute, double& value) {
    return readNodePlugValue(node, attribute, value);
}

MStatus readPlugValue(const MObject& node, const MObject& attribute, MString& value) {
    return readNodePlugValue(node, attribute, value);
}

MStatus readPlugValue(const MObject& node, const MObject& attribute, MVector& value) {
    return readNodePlugValue(node, attribute, value);
}

MStatus readPlugValue(const MObject& node, const MObject& attribute, MColor& value) {
    return readNodePlugValue(node, attribute, value);
}

namespace {

// Keep the public typed-array wrappers ABI-compatible and share one checked
// construction/publication path. A failed creation must never clean an output.
template <typename FunctionSet, typename Array>
MStatus outputMayaArray(MDataBlock& data, const MObject& dirtyPlug,
                       const MObject& output, const Array& values)
{
    MStatus status;
    FunctionSet creator;
    const MObject candidate = creator.create(values, &status);
    if (!status) return status;
    if (candidate.isNull()) return MS::kFailure;

    MDataHandle handle = data.outputValue(output, &status);
    if (!status) return status;
    status = handle.set(candidate);
    if (!status) return status;
    handle.setClean();
    return data.setClean(dirtyPlug);
}

} // namespace

// outputMayaData
MStatus DOutputDataMayaIntArray(MDataBlock& data, const MObject& dirtPlug, const MObject& output,
                                const MIntArray& dataArray) {
    return outputMayaArray<MFnIntArrayData>(data, dirtPlug, output, dataArray);
}

MStatus DOutputDataMayaFloatArray(MDataBlock& data, const MObject& dirtPlug, const MObject& output,
                                  const MFloatArray& dataArray) {
    return outputMayaArray<MFnFloatArrayData>(data, dirtPlug, output, dataArray);
}

MStatus DOutputDataMayaDoubleArray(MDataBlock& data, const MObject& dirtPlug, const MObject& output,
                                   const MDoubleArray& dataArray) {
    return outputMayaArray<MFnDoubleArrayData>(data, dirtPlug, output, dataArray);
}

MStatus DOutputDataMayaVectorArray(MDataBlock& data, const MObject& dirtPlug, const MObject& output,
                                   const MVectorArray& dataArray) {
    return outputMayaArray<MFnVectorArrayData>(data, dirtPlug, output, dataArray);
}

MStatus DOutputDataMayaPointArray(MDataBlock& data, const MObject& dirtPlug, const MObject& output,
                                  const MPointArray& dataArray) {
    return outputMayaArray<MFnPointArrayData>(data, dirtPlug, output, dataArray);
}

MStatus getTransformNodeFromPlug(const MObject& shapeObject, const MObject& sourceAttribute,
                                 MFnDagNode& transformFn) {
    if (shapeObject.isNull() || sourceAttribute.isNull()) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnDependencyNode nodeFn(shapeObject, &status);
    if (!status)
        return status;
    const MPlug matrixPlug = nodeFn.findPlug(sourceAttribute, true, &status);
    if (!status)
        return status;
    MPlugArray connectedPlugs;
    const bool connected = matrixPlug.connectedTo(connectedPlugs, true, false, &status);
    if (!status)
        return status;
    if (!connected || connectedPlugs.length() == 0U)
        return MS::kFailure;

    MFnDagNode shapeFn(connectedPlugs[0].node(), &status);
    if (!status)
        return status;
    const unsigned int parentCount = shapeFn.parentCount(&status);
    if (!status)
        return status;
    if (parentCount == 0U)
        return MS::kFailure;

    const MObject parent = shapeFn.parent(0U, &status);
    if (!status)
        return status;
    return transformFn.setObject(parent);
}

bool isInputPlugDataForScalar(MFnData::Type type) {

    if (type == MFnData::kNumeric || type == MFnData::kDoubleArray ||
        type == MFnData::kFloatArray || type == MFnData::kIntArray) {

        return true;

    } else {

        return false;
    }
}

bool isInputPlugDataForVector(MFnData::Type type) {

    if (type == MFnData::kVectorArray || type == MFnData::kPointArray) {

        return true;

    } else {

        return false;
    }
}

bool isInputPlugDataForString(MFnData::Type type) {

    if (type == MFnData::kString || type == MFnData::kStringArray) {

        return true;

    } else {

        return false;
    }
}

bool isInputPlugDataForMatrix(MFnData::Type type) {

    if (type == MFnData::kMatrix || type == MFnData::kMatrixArray) {

        return true;

    } else {

        return false;
    }
}

MStatus getMDoubleArrayFromPlug(MDataHandle dataHnd, MDoubleArray& values) {
    std::vector<double> input;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readScalarInput(
        dataHnd, values.length() == 0U ? dommy::maya::kInferInputCount : values.length(), input));
    MDoubleArray candidate;
    DOMMY_RETURN_IF_FAILURE(candidate.setLength(static_cast<unsigned int>(input.size())));
    for (unsigned int i = 0U; i < candidate.length(); ++i) candidate[i] = input[i];
    return values.copy(candidate);
}

MStatus getDScalarArrayFromPlug(MDataHandle dataHnd, DScalarDataList& values) {
    return dommy::maya::readScalarInput(dataHnd,
        values.empty() ? dommy::maya::kInferInputCount : values.size(), values);
}

MStatus getMVectorArrayFromPlug(MDataHandle dataHnd, MVectorArray& values) {
    std::vector<MVector> input;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readVectorInput(
        dataHnd, values.length() == 0U ? dommy::maya::kInferInputCount : values.length(), input));
    MVectorArray candidate;
    DOMMY_RETURN_IF_FAILURE(candidate.setLength(static_cast<unsigned int>(input.size())));
    for (unsigned int i = 0U; i < candidate.length(); ++i) candidate[i] = input[i];
    return values.copy(candidate);
}

MStatus getDVec3DataArrayFromPlug(MDataHandle dataHnd, DVec3DataList& values) {
    return dommy::maya::readVectorInput(dataHnd,
        values.empty() ? dommy::maya::kInferInputCount : values.size(), values);
}

MStatus getMStringArrayFromPlug(MDataHandle dataHnd, MStringArray& values) {
    MStatus status;
    MStringArray candidate;
    if (dataHnd.type() == MFnData::kString) {
        if (values.length() > dommy::generation::kMaxGeneratedStringCount)
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(candidate.setLength(values.length()));
        const MString value = dataHnd.asString();
        for (unsigned int i = 0U; i < candidate.length(); ++i) candidate[i] = value;
    } else if (dataHnd.type() == MFnData::kStringArray) {
        MFnStringArrayData source(dataHnd.data(), &status);
        if (!status) return status;
        candidate = source.array(&status);
        if (!status) return status;
        if (candidate.length() > dommy::generation::kMaxGeneratedStringCount ||
            (values.length() != 0U && values.length() != candidate.length()))
            return MS::kInvalidParameter;
    } else {
        return MS::kInvalidParameter;
    }
    values = candidate; // MStringArray has no fallible copy() API.
    return values.length() == candidate.length() ? MS::kSuccess : MS::kFailure;
}

MStatus getMMatrixArrayFromPlug(MDataHandle dataHnd, MMatrixArray& values) {
    MStatus status;
    MMatrixArray candidate;
    if (dataHnd.type() == MFnData::kMatrix) {
        const MMatrix value = dataHnd.asMatrix();
        if (!dommy::maya::isFiniteMatrix(value) ||
            values.length() > dommy::generation::kMaxGeneratedTransformCount)
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(candidate.setLength(values.length()));
        for (unsigned int i = 0U; i < candidate.length(); ++i) candidate[i] = value;
    } else if (dataHnd.type() == MFnData::kMatrixArray) {
        MFnMatrixArrayData source(dataHnd.data(), &status);
        if (!status) return status;
        candidate = source.array(&status);
        if (!status) return status;
        if (candidate.length() > dommy::generation::kMaxGeneratedTransformCount ||
            (values.length() != 0U && values.length() != candidate.length()))
            return MS::kInvalidParameter;
        for (unsigned int i = 0U; i < candidate.length(); ++i)
            if (!dommy::maya::isFiniteMatrix(candidate[i])) return MS::kInvalidParameter;
    } else {
        return MS::kInvalidParameter;
    }
    return values.copy(candidate);
}

#pragma mark Id method

void mergeUnionIdListOp(const DPIdList* inIdList, DPIdList& outIdList) {

    if (inIdList->size() == outIdList.size()) {
        size_t n = 0;
        for (auto it = inIdList->begin(); it != inIdList->end(); ++it) {
            if ((*it)) {
                outIdList[n] = true;
            }
            n++;
        }
    }
}

void mergeSubstructIdListOp(const DPIdList* inIdList, DPIdList& outIdList) {

    if (inIdList->size() == outIdList.size()) {
        size_t n = 0;
        for (auto it = inIdList->begin(); it != inIdList->end(); ++it) {
            if ((*it)) {
                outIdList[n] = false;
            }
            n++;
        }
    }
}

void mergeIntersectIdListOp(const DPIdList* inIdList, DPIdList& outIdList) {
    if (inIdList->size() == outIdList.size()) {
        size_t n = 0;
        for (auto it = inIdList->begin(); it != inIdList->end(); ++it) {
            if ((*it) && outIdList[n]) {
                outIdList[n] = true;
            } else {
                outIdList[n] = false;
            }
            n++;
        }
    }
}

void CheckDouble3MinMaxRange(const double3* min, const double3* max, double3* finMin,
                             double3* finMax) {

    if ((*min)[0] > (*max)[0]) {
        (*finMin)[0] = (*max)[0];
        (*finMax)[0] = (*min)[0];
    } else {
        (*finMin)[0] = (*min)[0];
        (*finMax)[0] = (*max)[0];
    }

    if ((*min)[1] > (*max)[1]) {
        (*finMin)[1] = (*max)[1];
        (*finMax)[1] = (*min)[1];
    } else {
        (*finMin)[1] = (*min)[1];
        (*finMax)[1] = (*max)[1];
    }

    if ((*min)[2] > (*max)[2]) {
        (*finMin)[2] = (*max)[2];
        (*finMax)[2] = (*min)[2];
    } else {
        (*finMin)[2] = (*min)[2];
        (*finMax)[2] = (*max)[2];
    }
}

MStatus isMeshData(MDataBlock data, MObject plug, MFnMesh& meshFn) {
    MObject meshObject;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getMeshObject(plug, data, meshObject));
    return meshFn.setObject(meshObject);
}

#pragma mark nurbs curve

MStatus getMayaNurbsCurve(const MObject& obj, const MObject& plugObj, MDataBlock& data,
                          MObject& crvData) {

    MStatus stat;

    const bool connected = MPlug(obj, plugObj).isConnected(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!connected) {
        return MS::kFailure;
    }

    MDataHandle crvHnd = data.inputValue(plugObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    if (crvHnd.type() != MFnData::kNurbsCurve) {
        return MS::kFailure;
    }

    const MObject candidate = crvHnd.data();
    if (candidate.isNull()) {
        return MS::kFailure;
    }

    crvData = candidate;
    return MS::kSuccess;
}

#pragma mark mesh

MStatus getMayaMesh(const MObject& obj, const MObject& plugObj, MDataBlock& data,
                    MObject& meshData) {
    return dommy::maya::getConnectedMeshObject(obj, plugObj, data, meshData);
}

MStatus getMeshInformation(const MObject& plugObject, MDataBlock& data, MBoundingBox& boundingBox,
                           MObject& meshObject) {
    MStatus status;
    MDataHandle meshHandle = data.inputValue(plugObject, &status);
    if (!status)
        return status;
    if (meshHandle.type() != MFnData::kMesh)
        return MS::kInvalidParameter;

    const MObject candidateMesh = meshHandle.data();
    if (candidateMesh.isNull())
        return MS::kInvalidParameter;

    MFnMesh meshFunction(candidateMesh, &status);
    if (!status)
        return status;

    const int rawPointCount = meshFunction.numVertices(&status);
    if (!status) return status;
    std::size_t pointCount = 0U;
    if (!dommy::generation::checkedCount(
            rawPointCount, dommy::generation::kMaxGeneratedPointCount, pointCount) ||
        pointCount == 0U)
        return MS::kInvalidParameter;

    MFloatPointArray points;
    // MDataBlock mesh values are geometry-data objects. Their incoming
    // connection determines whether transforms are already baked.
    status = meshFunction.getPoints(points, MSpace::kObject);
    if (!status)
        return status;
    if (points.length() != pointCount) return MS::kFailure;

    dommy::math::Bounds3d bounds;
    for (unsigned int index = 0U; index < points.length(); ++index) {
        if (!bounds.add(points[index].x, points[index].y, points[index].z)) {
            return MS::kInvalidParameter;
        }
    }
    if (bounds.empty())
        return MS::kInvalidParameter;

    const MBoundingBox candidateBounds(
        MPoint(bounds.minimum(0), bounds.minimum(1), bounds.minimum(2)),
        MPoint(bounds.maximum(0), bounds.maximum(1), bounds.maximum(2)));

    meshObject = candidateMesh;
    boundingBox = candidateBounds;
    return MS::kSuccess;
}

#pragma mark animate

namespace {

bool validHistoricalTimes(const MTime& currentTime, const DScalarDataList& offsets)
{
    if (!std::isfinite(currentTime.value()) ||
        offsets.size() > dommy::sampling::kMaxTimeSamples) return false;
    for (float offset : offsets)
        if (!std::isfinite(offset) || !std::isfinite(currentTime.value() - offset)) return false;
    return true;
}

MStatus preflightHistoricalGroups(const std::vector<MPointArray>& groups,
                                  std::size_t sampleCount,
                                  dommy::sampling::TimeSamplingBudget& budget)
{
    if (!budget.addPoints(0U, sampleCount)) return MS::kInvalidParameter;
    for (const auto& group : groups)
        if (!budget.addPoints(group.length())) return MS::kInvalidParameter;
    return MS::kSuccess;
}

} // namespace

MStatus DGetPlugTimeDP(const MTime& currentTime, std::vector<MPointArray>& array,
                       const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                       DScalarDataList* timeAttrArray, DAUIntList* pIdArray) {
    if (!validHistoricalTimes(currentTime, timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    DOMMY_RETURN_IF_FAILURE(preflightHistoricalGroups(array, timeValueArray.size(), budget));

    array.clear();
    for (std::size_t i = 0; i < timeValueArray.size(); ++i) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;

        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));
        MDGContextGuard contextGuard(context);
        dommy::maya::ScopedPlugDataHandle objHndOwner;
        MDataHandle& objHnd = objHndOwner.handle();
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = objHndOwner.read(srcPlug);
        if (!status)
            return status;

        const DPData* dpData = getInputDPfromHandle(objHnd);
        if (!dpData)
            return MS::kFailure;

        const DPointList* ptsPtr = dpData->getPositionPtrConst();
        const std::size_t pointCount = ptsPtr->size();

        if (!budget.readPoints(pointCount) || !budget.addPoints(pointCount, 0U))
            return MS::kInvalidParameter;
        MPointArray pts;
        DOMMY_RETURN_IF_FAILURE(dommy::geometry::copyPointsToMayaArray(*ptsPtr, pts));
        array.emplace_back();
        DOMMY_RETURN_IF_FAILURE(array.back().copy(pts));
        if (array.back().length() != pointCount) return MS::kFailure;

        if (timeAttrArray) {
            DScalarDataList newtimeAttr(pointCount, timeValueArray[i]);
            timeAttrArray->insert(timeAttrArray->end(), newtimeAttr.begin(), newtimeAttr.end());
        }
        if (pIdArray) {
            DAUIntList newIds(pointCount);
            std::iota(std::begin(newIds), std::end(newIds), 0);
            pIdArray->insert(pIdArray->end(), newIds.begin(), newIds.end());
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeCurve(const MTime& currentTime, std::vector<MPointArray>& array,
                          const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                          DScalarDataList* timeAttrArray, DAUIntList* pIdArray) {
    if (!validHistoricalTimes(currentTime, timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    DOMMY_RETURN_IF_FAILURE(preflightHistoricalGroups(array, timeValueArray.size(), budget));


    for (uint i = 0, N = timeValueArray.size(); i < N; i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnNurbsCurve crvFn(obj, &status);
        if (!status)
            return status;
        const int rawCount = crvFn.numCVs(&status);
        if (!status) return status;
        std::size_t pointCount = 0U;
        if (!dommy::generation::checkedCount(rawCount,
                dommy::generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
        if (!dommy::sampling::validTimeStorage(pointCount, 1U) || !budget.readPoints(pointCount))
            return MS::kInvalidParameter;
        if (!budget.addPoints(pointCount, 0U)) return MS::kInvalidParameter;
        MPointArray pts;
        status = crvFn.getCVs(pts, MSpace::kWorld);
        if (!status)
            return status;
        if (pts.length() != pointCount) return MS::kFailure;
        for (unsigned int index = 0U; index < pts.length(); ++index)
            if (!dommy::maya::isFloatRepresentablePoint(pts[index])) return MS::kInvalidParameter;

        array.emplace_back();
        DOMMY_RETURN_IF_FAILURE(array.back().copy(pts));
        if (array.back().length() != pointCount) return MS::kFailure;
        if (timeAttrArray) {
            DScalarDataList newtimeAttr(pts.length(), timeValueArray[i]);
            timeAttrArray->insert(timeAttrArray->end(), newtimeAttr.begin(), newtimeAttr.end());
        }
        if (pIdArray) {
            DAUIntList newIds(pts.length());
            std::iota(std::begin(newIds), std::end(newIds), 0);
            pIdArray->insert(pIdArray->end(), newIds.begin(), newIds.end());
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeNurbs(const MTime& currentTime, std::vector<MPointArray>& array,
                          const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                          DScalarDataList* timeAttrArray, DAUIntList* pIdArray) {
    if (!validHistoricalTimes(currentTime, timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    DOMMY_RETURN_IF_FAILURE(preflightHistoricalGroups(array, timeValueArray.size(), budget));


    for (uint i = 0, N = timeValueArray.size(); i < N; i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnNurbsSurface srfFn(obj, &status);
        if (!status)
            return status;
        const int rawU = srfFn.numCVsInU(&status);
        if (!status) return status;
        const int rawV = srfFn.numCVsInV(&status);
        if (!status) return status;
        std::size_t pointCount = 0U;
        if (rawU < 0 || rawV < 0 || !dommy::generation::checkedProduct(
                static_cast<std::size_t>(rawU), static_cast<std::size_t>(rawV),
                dommy::generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
        if (!dommy::sampling::validTimeStorage(pointCount, 1U) || !budget.readPoints(pointCount))
            return MS::kInvalidParameter;
        if (!budget.addPoints(pointCount, 0U)) return MS::kInvalidParameter;
        MPointArray pts;
        status = srfFn.getCVs(pts, MSpace::kWorld);
        if (!status)
            return status;
        if (pts.length() != pointCount) return MS::kFailure;
        for (unsigned int index = 0U; index < pts.length(); ++index)
            if (!dommy::maya::isFloatRepresentablePoint(pts[index])) return MS::kInvalidParameter;

        array.emplace_back();
        DOMMY_RETURN_IF_FAILURE(array.back().copy(pts));
        if (array.back().length() != pointCount) return MS::kFailure;
        if (timeAttrArray) {
            DScalarDataList newtimeAttr(pts.length(), timeValueArray[i]);
            timeAttrArray->insert(timeAttrArray->end(), newtimeAttr.begin(), newtimeAttr.end());
        }
        if (pIdArray) {
            DAUIntList newIds(pts.length());
            std::iota(std::begin(newIds), std::end(newIds), 0);
            pIdArray->insert(pIdArray->end(), newIds.begin(), newIds.end());
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeMesh(const MTime& currentTime, std::vector<MPointArray>& array,
                         const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                         DScalarDataList* timeAttrArray, DAUIntList* pIdArray) {
    if (!validHistoricalTimes(currentTime, timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    DOMMY_RETURN_IF_FAILURE(preflightHistoricalGroups(array, timeValueArray.size(), budget));


    for (uint i = 0, N = timeValueArray.size(); i < N; i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnMesh meshFn(obj, &status);
        if (!status)
            return status;
        const int rawCount = meshFn.numVertices(&status);
        if (!status) return status;
        std::size_t pointCount = 0U;
        if (!dommy::generation::checkedCount(rawCount,
                dommy::generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
        if (!dommy::sampling::validTimeStorage(pointCount, 1U) || !budget.readPoints(pointCount))
            return MS::kInvalidParameter;
        if (!budget.addPoints(pointCount, 0U)) return MS::kInvalidParameter;
        MPointArray pts;
        status = meshFn.getPoints(pts, MSpace::kWorld);
        if (!status)
            return status;
        if (pts.length() != pointCount) return MS::kFailure;
        for (unsigned int index = 0U; index < pts.length(); ++index)
            if (!dommy::maya::isFloatRepresentablePoint(pts[index])) return MS::kInvalidParameter;

        array.emplace_back();
        DOMMY_RETURN_IF_FAILURE(array.back().copy(pts));
        if (array.back().length() != pointCount) return MS::kFailure;
        if (timeAttrArray) {
            DScalarDataList newtimeAttr(pts.length(), timeValueArray[i]);
            timeAttrArray->insert(timeAttrArray->end(), newtimeAttr.begin(), newtimeAttr.end());
        }
        if (pIdArray) {
            DAUIntList newIds(pts.length());
            std::iota(std::begin(newIds), std::end(newIds), 0);
            pIdArray->insert(pIdArray->end(), newIds.begin(), newIds.end());
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeMatrix(const MTime& currentTime, std::vector<MMatrix>& array,
                           const MPlug& srcPlug, const DScalarDataList& timeValueArray) {
    if (!validHistoricalTimes(currentTime, timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget budget;
    std::size_t count = 0U;
    if (!dommy::generation::checkedSum(array.size(), timeValueArray.size(),
            dommy::generation::kMaxGeneratedPointCount, count) ||
        !dommy::sampling::validTimeStorage(count, 0U))
        return MS::kInvalidParameter;


    for (uint i = 0, N = timeValueArray.size(); i < N; i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnMatrixData worldMatrixData(obj, &status);
        if (!status)
            return status;

        const MMatrix matrix = worldMatrixData.matrix(&status);
        if (!status)
            return status;
        if (!dommy::maya::isFiniteMatrix(matrix))
            return MS::kInvalidParameter;
        array.push_back(matrix);
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeDPPP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                         const DScalarDataList& timeValueArray, uint ptIndex,
                         DScalarDataList* sampledOffsets,
                         dommy::sampling::TimeSamplingBudget* sharedBudget) {
    if (!validHistoricalTimes(currentTime, timeValueArray) ||
        (sampledOffsets && sampledOffsets == &timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget localBudget;
    auto& budget = sharedBudget ? *sharedBudget : localBudget;
    if (!budget.addPoints(array.length(), 1U)) return MS::kInvalidParameter;


    for (uint i = 0; i < timeValueArray.size(); i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;

        const double lowerOffset = std::floor(timeValueArray[i]);
        const double fraction = timeValueArray[i] - lowerOffset;

        if (fraction == 0.0) {
            MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

            // for 2018
            MDGContextGuard contextGuard2(context);
            dommy::maya::ScopedPlugDataHandle objHndOwner;
            MDataHandle& objHnd = objHndOwner.handle();
            if (!budget.readPoints(0U)) return MS::kInvalidParameter;
            MStatus status = objHndOwner.read(srcPlug);
            if (!status)
                return status;

            const DPData* dpData = getInputDPfromHandle(objHnd);

            if (!dpData)
                return MS::kFailure;

            const DPointList* ptsPtr = dpData->getPositionPtrConst();
            if (!dommy::sampling::validTimeStorage(ptsPtr->size(), 1U) ||
                !budget.readPoints(ptsPtr->size())) return MS::kInvalidParameter;

            if (ptIndex < ptsPtr->size()) {
                const MPoint point((*ptsPtr)[ptIndex][0], (*ptsPtr)[ptIndex][1],
                                   (*ptsPtr)[ptIndex][2]);
                if (!dommy::maya::isFinitePoint(point))
                    return MS::kInvalidParameter;
                if (!budget.addPoints(1U, 0U)) return MS::kInvalidParameter;
                status = array.append(point);
                if (!status)
                    return status;
                if (sampledOffsets) sampledOffsets->push_back(timeValueArray[i]);
            }

        } else {

            // 時間が整数値じゃない場合前後のフレームを補完した位置を抽出

            MDGContext context(MTime(currentTime.value() - lowerOffset, currentTime.unit()));
            MDGContext context2(MTime(currentTime.value() - lowerOffset - 1.0, currentTime.unit()));

            // for 2018
            MDGContextGuard contextGuard(context);
            dommy::maya::ScopedPlugDataHandle firstOwner;
            if (!budget.readPoints(0U)) return MS::kInvalidParameter;
            MStatus status = firstOwner.read(srcPlug);
            if (!status)
                return status;

            const DPData* dpData1 = getInputDPfromHandle(firstOwner.handle());
            if (!dpData1)
                return MS::kFailure;

            const DPointList* ptsPtr1 = dpData1->getPositionPtrConst();
            if (!dommy::sampling::validTimeStorage(ptsPtr1->size(), 1U) ||
                !budget.readPoints(ptsPtr1->size())) return MS::kInvalidParameter;

            // for 2018
            MDGContextGuard contextGuard2(context2);
            dommy::maya::ScopedPlugDataHandle secondOwner;
            if (!budget.readPoints(0U)) return MS::kInvalidParameter;
            status = secondOwner.read(srcPlug);
            if (!status)
                return status;
            const DPData* dpData2 = getInputDPfromHandle(secondOwner.handle());
            if (!dpData2)
                return MS::kFailure;

            const DPointList* ptsPtr2 = dpData2->getPositionPtrConst();
            if (!dommy::sampling::validTimeStorage(ptsPtr2->size(), 1U) ||
                !budget.readPoints(ptsPtr2->size())) return MS::kInvalidParameter;

            if (ptIndex < ptsPtr1->size() && ptIndex < ptsPtr2->size()) {
                MPoint point;
                for (unsigned int axis = 0U; axis < 3U; ++axis) {
                    const double first = (*ptsPtr1)[ptIndex][axis];
                    const double second = (*ptsPtr2)[ptIndex][axis];
                    point[axis] = first + (second - first) * fraction;
                }
                if (!dommy::maya::isFinitePoint(point))
                    return MS::kInvalidParameter;
                if (!budget.addPoints(1U, 0U)) return MS::kInvalidParameter;
                status = array.append(point);
                if (!status)
                    return status;
                if (sampledOffsets) sampledOffsets->push_back(timeValueArray[i]);
            }
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeCurvePP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                            const DScalarDataList& timeValueArray, uint ptIndex,
                            DScalarDataList* sampledOffsets,
                            dommy::sampling::TimeSamplingBudget* sharedBudget) {
    if (!validHistoricalTimes(currentTime, timeValueArray) ||
        (sampledOffsets && sampledOffsets == &timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget localBudget;
    auto& budget = sharedBudget ? *sharedBudget : localBudget;
    if (!budget.addPoints(array.length(), 1U)) return MS::kInvalidParameter;


    for (uint i = 0; i < timeValueArray.size(); i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnNurbsCurve crvFn(obj, &status);
        if (!status)
            return status;
        const int rawCount = crvFn.numCVs(&status);
        if (!status) return status;
        std::size_t pointCount = 0U;
        if (!dommy::generation::checkedCount(rawCount,
                dommy::generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
        if (!dommy::sampling::validTimeStorage(pointCount, 1U) || !budget.readPoints(pointCount))
            return MS::kInvalidParameter;
        MPointArray pts;
        status = crvFn.getCVs(pts, MSpace::kWorld);
        if (!status)
            return status;
        if (pts.length() != pointCount) return MS::kFailure;
        for (unsigned int index = 0U; index < pts.length(); ++index)
            if (!dommy::maya::isFloatRepresentablePoint(pts[index])) return MS::kInvalidParameter;

        if (ptIndex < pts.length()) {
            if (!dommy::maya::isFinitePoint(pts[ptIndex]))
                return MS::kInvalidParameter;
            if (!budget.addPoints(1U, 0U)) return MS::kInvalidParameter;
            status = array.append(pts[ptIndex]);
            if (!status)
                return status;
            if (sampledOffsets) sampledOffsets->push_back(timeValueArray[i]);
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeNurbsPP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                            const DScalarDataList& timeValueArray, uint ptIndex,
                            DScalarDataList* sampledOffsets,
                            dommy::sampling::TimeSamplingBudget* sharedBudget) {
    if (!validHistoricalTimes(currentTime, timeValueArray) ||
        (sampledOffsets && sampledOffsets == &timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget localBudget;
    auto& budget = sharedBudget ? *sharedBudget : localBudget;
    if (!budget.addPoints(array.length(), 1U)) return MS::kInvalidParameter;


    for (uint i = 0; i < timeValueArray.size(); i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnNurbsSurface srfFn(obj, &status);
        if (!status)
            return status;
        const int rawU = srfFn.numCVsInU(&status);
        if (!status) return status;
        const int rawV = srfFn.numCVsInV(&status);
        if (!status) return status;
        std::size_t pointCount = 0U;
        if (rawU < 0 || rawV < 0 || !dommy::generation::checkedProduct(
                static_cast<std::size_t>(rawU), static_cast<std::size_t>(rawV),
                dommy::generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
        if (!dommy::sampling::validTimeStorage(pointCount, 1U) || !budget.readPoints(pointCount))
            return MS::kInvalidParameter;
        MPointArray pts;
        status = srfFn.getCVs(pts, MSpace::kWorld);
        if (!status)
            return status;
        if (pts.length() != pointCount) return MS::kFailure;
        for (unsigned int index = 0U; index < pts.length(); ++index)
            if (!dommy::maya::isFloatRepresentablePoint(pts[index])) return MS::kInvalidParameter;

        if (ptIndex < pts.length()) {
            if (!dommy::maya::isFinitePoint(pts[ptIndex]))
                return MS::kInvalidParameter;
            if (!budget.addPoints(1U, 0U)) return MS::kInvalidParameter;
            status = array.append(pts[ptIndex]);
            if (!status)
                return status;
            if (sampledOffsets) sampledOffsets->push_back(timeValueArray[i]);
        }
    }
    return MS::kSuccess;
}

MStatus DGetPlugTimeMeshPP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                           const DScalarDataList& timeValueArray, uint ptIndex,
                           DScalarDataList* sampledOffsets,
                           dommy::sampling::TimeSamplingBudget* sharedBudget) {
    if (!validHistoricalTimes(currentTime, timeValueArray) ||
        (sampledOffsets && sampledOffsets == &timeValueArray)) return MS::kInvalidParameter;
    dommy::sampling::TimeSamplingBudget localBudget;
    auto& budget = sharedBudget ? *sharedBudget : localBudget;
    if (!budget.addPoints(array.length(), 1U)) return MS::kInvalidParameter;


    for (uint i = 0; i < timeValueArray.size(); i++) {
        if (!std::isfinite(timeValueArray[i]))
            return MS::kInvalidParameter;
        MObject obj;
        MDGContext context(MTime(currentTime.value() - timeValueArray[i], currentTime.unit()));

        // for 2018
        MDGContextGuard contextGuard(context);
        if (!budget.readPoints(0U)) return MS::kInvalidParameter;
        MStatus status = srcPlug.getValue(obj);
        if (!status)
            return status;

        MFnMesh meshFn(obj, &status);
        if (!status)
            return status;
        const int rawCount = meshFn.numVertices(&status);
        if (!status) return status;
        std::size_t pointCount = 0U;
        if (!dommy::generation::checkedCount(rawCount,
                dommy::generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
        if (!dommy::sampling::validTimeStorage(pointCount, 1U) || !budget.readPoints(pointCount))
            return MS::kInvalidParameter;
        MPointArray pts;
        status = meshFn.getPoints(pts, MSpace::kWorld);
        if (!status)
            return status;
        if (pts.length() != pointCount) return MS::kFailure;
        for (unsigned int index = 0U; index < pts.length(); ++index)
            if (!dommy::maya::isFloatRepresentablePoint(pts[index])) return MS::kInvalidParameter;

        if (ptIndex < pts.length()) {
            if (!dommy::maya::isFinitePoint(pts[ptIndex]))
                return MS::kInvalidParameter;
            if (!budget.addPoints(1U, 0U)) return MS::kInvalidParameter;
            status = array.append(pts[ptIndex]);
            if (!status)
                return status;
            if (sampledOffsets) sampledOffsets->push_back(timeValueArray[i]);
        }
    }
    return MS::kSuccess;
}

#pragma mark time range common func

MStatus getTimeOffsetAndStretch(const DPData& inputDPData, MDataBlock& data,
                                const MObject& thisObject, const MObject& offsetPlug,
                                const MObject& stretchPlug, const MObject& offsetArrayPlug,
                                const MObject& stretchArrayPlug, const int& offsetType,
                                const int& stretchType, MDoubleArray& offsetArray,
                                MDoubleArray& stretchArray) {
    const std::size_t count = inputDPData.numberOfPoints();
    if (&offsetArray == &stretchArray || offsetType < 0 || offsetType > 2 || stretchType < 0 ||
        stretchType > 2 ||
        !dommy::attribute::validArrayConversionBudget(count, 2U * sizeof(double)))
        return MS::kInvalidParameter;

    // Caller-owned scratch arrays are discarded on failure. Do not index a
    // sized constructor result whose allocation status cannot be observed.
    MStatus status = offsetArray.setLength(static_cast<unsigned int>(count));
    if (!status)
        return status;
    if (offsetArray.length() != count)
        return MS::kFailure;
    status = stretchArray.setLength(static_cast<unsigned int>(count));
    if (!status)
        return status;
    if (stretchArray.length() != count)
        return MS::kFailure;
    for (unsigned int index = 0U; index < count; ++index) {
        offsetArray[index] = 0.0;
        stretchArray[index] = 1.0;
    }

    const auto readValues = [&](int type, const MObject& nameAttribute,
                                const MObject& arrayAttribute, MDoubleArray& output) -> MStatus {
        if (type == 1) {
            MStatus inputStatus;
            const MDataHandle handle = data.inputValue(nameAttribute, &inputStatus);
            if (!inputStatus)
                return inputStatus;
            const std::string name = handle.asString().asChar();
            // Preserve the legacy missing/non-scalar attribute default behavior.
            getMDoubleArrayFromAttribute(inputDPData, name, output);
        } else if (type == 2) {
            MStatus connectionStatus;
            const bool connected = MPlug(thisObject, arrayAttribute).isConnected(&connectionStatus);
            if (!connectionStatus)
                return connectionStatus;
            if (!connected)
                return MS::kSuccess;
            // DScalarDataList is float-backed; retain the historical rounding.
            DScalarDataList source;
            const MStatus inputStatus = getDoubleArrayFromData(data, arrayAttribute, source);
            if (!inputStatus)
                return inputStatus;
            dommy::memory::copyPrefix(source, source.size(), output,
                                      static_cast<std::size_t>(output.length()));
        }
        return output.length() == count ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
    };
    status = readValues(offsetType, offsetPlug, offsetArrayPlug, offsetArray);
    if (!status)
        return status;
    status = readValues(stretchType, stretchPlug, stretchArrayPlug, stretchArray);
    if (!status)
        return status;
    for (unsigned int index = 0U; index < count; ++index) {
        if (!std::isfinite(offsetArray[index]) || !std::isfinite(stretchArray[index]))
            return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

MDGContext getTimeDGContext(float timeVal, const float& offset, const float& stretch,
                            const float& globalOffset, const MTime::Unit& fps) {

    // stretch
    if (stretch != 0.0) {
        timeVal /= stretch;
    }

    // offsetarray
    timeVal -= offset;

    MTime animTime(timeVal, fps);
    animTime += globalOffset;

    MObject matObj;
    return MDGContext(animTime);
}

#pragma mark Matrix

MStatus getAlignSpaceMatrix(const MVector& forwardVec, const MVector& upVec,
                            const MVector& spaceForwardAxis, const MVector& spaceUpAxis,
                            MMatrix& output) {
    dommy::math::FrameRotation rotation;
    if (!dommy::math::makeFrameAlignment(
            {{spaceForwardAxis.x, spaceForwardAxis.y, spaceForwardAxis.z}},
            {{spaceUpAxis.x, spaceUpAxis.y, spaceUpAxis.z}},
            {{forwardVec.x, forwardVec.y, forwardVec.z}},
            {{upVec.x, upVec.y, upVec.z}}, rotation)) {
        return MS::kInvalidParameter;
    }
    MMatrix candidate;
    for (unsigned row = 0U; row < 3U; ++row) {
        for (unsigned column = 0U; column < 3U; ++column) {
            candidate[row][column] = rotation[row][column];
        }
    }
    output = candidate;
    return MS::kSuccess;
}
