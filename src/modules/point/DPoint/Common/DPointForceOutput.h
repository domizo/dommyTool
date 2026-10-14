#pragma once

#include "DPData.h"
#include "DCurveData.h"
#include "DMeshData.h"
#include "DPAttributeType.h"
#include "common/utility/DPointForceBudget.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include <string>
#include <utility>
#include <vector>

namespace dommy {
namespace point {

inline bool validPointForceTargetBudget(std::size_t points, std::size_t partitions,
                                        short type, std::size_t iterations,
                                        const DPointList* targetPoints,
                                        const DCurveData* targetCurves)
{
    if (type < 0 || type > 3) return false;
    std::size_t targets = 1U, construction = 0U;
    if (type == 2) {
        if (!targetPoints) return false;
        targets = targetPoints->size();
    } else if (type == 3) {
        if (!targetCurves) return false;
        targets = targetCurves->numberOfCurves();
        if (targets > generation::kMaxGeneratedCurveCount) return false;
        for (auto it = targetCurves->beginCurves(); it != targetCurves->endCurves(); ++it) {
            if (!*it || !generation::checkedSum(construction, (*it)->numberOfPoints(),
                    generation::kMaxWorkItemCount, construction)) return false;
        }
    }
    return validPointForceBudget(points, partitions, targets, iterations, construction);
}

// Read-only, evaluation-local bindings. The MDataBlock owns the input data.
struct PointForceInput {
    const DPData* points = nullptr;
    const DCurveData* curves = nullptr;
    const DMeshData* meshes = nullptr;
    std::size_t pointCount = 0U;
    std::size_t partitionCount = 0U;

    MStatus bind(const MDataHandle& handle)
    {
        *this = PointForceInput();
        const MTypeId type = handle.typeId();
        if (type == DPData::id) {
            DOMMY_RETURN_IF_FAILURE(maya::getPluginDataFromHandle(handle, DPData::id, points));
            if (!points || !points->hasValidPointTopology()) return MS::kInvalidParameter;
            pointCount = points->numberOfPoints();
            partitionCount = pointCount == 0U ? 0U : 1U;
        } else if (type == DCurveData::id) {
            DOMMY_RETURN_IF_FAILURE(maya::getPluginDataFromHandle(handle, DCurveData::id, curves));
            if (!curves || !curves->hasValidCurveTopology()) return MS::kInvalidParameter;
            partitionCount = curves->numberOfCurves();
            for (auto it = curves->beginCurves(); it != curves->endCurves(); ++it) {
                if (!*it || !generation::checkedSum(pointCount, (*it)->numberOfPoints(),
                        generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
            }
        } else if (type == DMeshData::id) {
            DOMMY_RETURN_IF_FAILURE(maya::getPluginDataFromHandle(handle, DMeshData::id, meshes));
            if (!meshes || !meshes->hasValidMeshTopology()) return MS::kInvalidParameter;
            partitionCount = meshes->numberOfMesh();
            for (auto it = meshes->beginMeshes(); it != meshes->endMeshes(); ++it) {
                if (!*it || !generation::checkedSum(pointCount, (*it)->numberOfPoints(),
                        generation::kMaxGeneratedPointCount, pointCount)) return MS::kInvalidParameter;
            }
        } else {
            return MS::kInvalidParameter;
        }
        return MS::kSuccess;
    }
};

// Retain the historical fallback for missing/non-scalar weight/radius fields.
// Present supported scalar fields must have exactly one finite float per item.
inline bool readPointForceScalars(const DPAttributeData::ConstPtr& field,
                                  std::size_t count, float fallback,
                                  DScalarDataList& values)
{
    if (!attribute::validArrayConversionBudget(count, sizeof(float))) return false;
    if (field && isSingleRealAttrType(field->getAttrType()))
        return attribute::convertScalarAttribute(field, count, values);
    values.assign(count, fallback);
    return true;
}

template <typename Geometry>
bool appendPointForcePartition(const Geometry& geometry,
                                const std::string& weightName, float weight,
                                std::vector<DPointList>& positions,
                                std::vector<DScalarDataList>& weights)
{
    const DPointList* source = geometry.getPositionPtrConst();
    if (!source || source->size() != geometry.numberOfPoints()) return false;
    DScalarDataList values;
    if (!readPointForceScalars(geometry.getAttr(weightName), source->size(), weight, values))
        return false;
    positions.push_back(*source);
    weights.push_back(std::move(values));
    return true;
}

// The caller preflights the complete workload before reaching this allocation
// boundary. Evaluate detached local-space positions before creating/copying an
// output. Maya function sets are still invoked serially by the supplied kernel.
template <typename Evaluate>
MStatus evaluatePointForceOutput(const PointForceInput& input,
                                 MDataBlock& data, const MObject& outputAttribute,
                                 const std::string& weightName, float weight,
                                 Evaluate evaluate)
{
    // Charge the complete detached point/weight workspace and retained
    // metadata across partitions, not just each array in isolation. Native
    // Maya query workspaces and mesh topology are outside this point estimate.
    PointPayloadBudget workspace;
    workspace.add(input.pointCount, sizeof(DPoint) + sizeof(float));
    workspace.add(input.partitionCount,
        sizeof(DPointList) + sizeof(DScalarDataList) + sizeof(DPointList*));
    if (input.points) {
        preflightMetadataPayload(*input.points, input.pointCount, workspace);
    } else if (input.curves) {
        preflightMetadataPayload(*input.curves, input.partitionCount, workspace);
        for (auto it = input.curves->beginCurves(); it != input.curves->endCurves(); ++it)
            preflightMetadataPayload(**it, (*it)->numberOfPoints(), workspace);
    } else if (input.meshes) {
        preflightMetadataPayload(*input.meshes, input.partitionCount, workspace);
        for (auto it = input.meshes->beginMeshes(); it != input.meshes->endMeshes(); ++it)
            preflightMetadataPayload(**it, (*it)->numberOfPoints(), workspace);
    } else {
        return MS::kInvalidParameter;
    }
    std::vector<DPointList> positions;
    std::vector<DScalarDataList> weights;
    positions.reserve(input.partitionCount);
    weights.reserve(input.partitionCount);
    if (input.points && input.partitionCount != 0U) {
        if (!appendPointForcePartition(*input.points, weightName, weight, positions, weights))
            return MS::kInvalidParameter;
    } else if (input.curves) {
        for (auto it = input.curves->beginCurves(); it != input.curves->endCurves(); ++it)
            if (!appendPointForcePartition(**it, weightName, weight, positions, weights))
                return MS::kInvalidParameter;
    } else if (input.meshes) {
        for (auto it = input.meshes->beginMeshes(); it != input.meshes->endMeshes(); ++it)
            if (!appendPointForcePartition(**it, weightName, weight, positions, weights))
                return MS::kInvalidParameter;
    }
    std::vector<DPointList*> pointers;
    pointers.reserve(positions.size());
    for (DPointList& partition : positions) pointers.push_back(&partition);
    if (!pointers.empty()) DOMMY_RETURN_IF_FAILURE(evaluate(pointers, weights));

    if (input.points) {
        // A coordinate kernel must not resize the detached source partition.
        // Check before indexed placement and before creating a Maya output.
        if (positions.size() != input.partitionCount ||
            (!positions.empty() && positions[0].size() != input.pointCount))
            return MS::kInvalidParameter;
        return maya::setPluginDataOutput<DPData>(data, outputAttribute, DPData::id,
            [&](DPData& output) {
                return output.copyRepeatedFrom(*input.points, 1U,
                    [&](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                        point = positions[0][index];
                        return MS::kSuccess;
                    });
            });
    }
    if (input.curves) {
        return maya::setPluginDataOutput<DCurveData>(data, outputAttribute, DCurveData::id,
            [&](DCurveData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input.curves));
                std::size_t index = 0U;
                for (auto it = output.beginCurves(); it != output.endCurves(); ++it, ++index)
                    if (!(*it)->replacePositions(positions[index])) return MStatus(MS::kFailure);
                if (!positions.empty()) output.updateBBox();
                return MStatus(MS::kSuccess);
            });
    }
    if (!input.meshes) return MS::kInvalidParameter;
    return maya::setPluginDataOutput<DMeshData>(data, outputAttribute, DMeshData::id,
        [&](DMeshData& output) {
            DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input.meshes));
            std::size_t index = 0U;
            for (auto it = output.beginMeshes(); it != output.endMeshes(); ++it, ++index)
                if (!(*it)->replacePositions(positions[index])) return MStatus(MS::kFailure);
            return MStatus(MS::kSuccess);
        });
}

} // namespace point
} // namespace dommy
