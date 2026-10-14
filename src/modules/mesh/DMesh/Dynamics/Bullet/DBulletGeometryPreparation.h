#pragma once

#include "commonDBullet.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "Solver/DPassiveTransformMath.h"

// Geometry preparation is driven by the solver, not by an output plug's
// evaluation time. No native body is changed until the candidate is complete.
inline MStatus ReadBulletGeometryTransform(const MMatrix& matrix,
    btTransform& transform, MVector& scale)
{
    if (!dommy::maya::isFiniteMatrix(matrix)) return MS::kInvalidParameter;
    MTransformationMatrix decomposition(matrix);
    MStatus status;
    const MVector position = decomposition.getTranslation(MSpace::kTransform, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    double x, y, z, w, values[3];
    DOMMY_RETURN_IF_FAILURE(decomposition.getRotationQuaternion(x, y, z, w));
    DOMMY_RETURN_IF_FAILURE(decomposition.getScale(values, MSpace::kTransform));
    float rotation[4];
    const double components[] = {w, x, y, z};
    for (int axis = 0; axis < 4; ++axis)
        if (!dommy::numeric::checkedFloatCast(components[axis], rotation[axis]))
            return MS::kInvalidParameter;
    quatf normalized;
    if (!dommy::bullet::normalizeQuaternion(
            quatf(rotation[0], rotation[1], rotation[2], rotation[3]), normalized))
        return MS::kInvalidParameter;
    for (int axis = 0; axis < 3; ++axis)
        if (!dommy::bullet::isSafeCollisionCoordinate(position[axis]) ||
            !dommy::bullet::isSafeCollisionCoordinate(values[axis]) ||
            values[axis] == 0 ||
            !dommy::bullet::isSafeInverseOperand(std::abs(values[axis])))
            return MS::kInvalidParameter;
    transform = btTransform(btQuaternion(normalized[1], normalized[2],
        normalized[3], normalized[0]), btVector3(position[0], position[1], position[2]));
    scale = MVector(values[0], values[1], values[2]);
    return MS::kSuccess;
}

struct DBulletGeometryCandidate
{
    std::vector<std::unique_ptr<db_collision_shape_base>> single, compound;
    std::vector<btTransform> transforms, compoundTransforms;
    std::vector<MVector> scales;
    std::vector<DPIndexList> groups;
    std::vector<MMatrix> currentTransforms;
};

// Reserve all destinations before releasing any candidate owner. A failed
// allocation leaves the existing geometry and native bodies untouched.
inline void ReserveBulletGeometry(const DBulletGeometryCandidate& candidate,
    bool replace, std::vector<db_collision_shape_base*>& shapes,
    std::vector<btTransform>& transforms, std::vector<MVector>& scales)
{
    shapes.reserve((replace ? 0 : shapes.size()) + candidate.single.size());
    transforms.reserve((replace ? 0 : transforms.size()) + candidate.transforms.size());
    scales.reserve((replace ? 0 : scales.size()) + candidate.scales.size());
}

inline void AppendBulletGeometry(DBulletGeometryCandidate& candidate,
    std::vector<db_collision_shape_base*>& shapes,
    std::vector<btTransform>& transforms, std::vector<MVector>& scales)
{
    transforms.insert(transforms.end(), candidate.transforms.begin(), candidate.transforms.end());
    scales.insert(scales.end(), candidate.scales.begin(), candidate.scales.end());
    for (auto& shape : candidate.single) shapes.push_back(shape.release());
}

template <class GetItem, class CreateShape>
MStatus PrepareBulletGeometryCandidate(std::size_t count, std::size_t firstNew,
    const std::vector<DPIndexList>& groups, GetItem getItem, CreateShape createShape,
    const MUuid& nodeUuid, const std::string& nodeName, DBulletGeometryCandidate& candidate)
{
    if (firstNew > count || count > dommy::generation::kMaxGeneratedTransformCount)
        return MS::kInvalidParameter;
    std::vector<btTransform> transforms(count);
    std::vector<MVector> scales(count);
    candidate.currentTransforms.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto item = getItem(index);
        if (!item) return MS::kInvalidParameter;
        const MMatrix matrix = item->getTransformMatrix();
        DOMMY_RETURN_IF_FAILURE(ReadBulletGeometryTransform(matrix, transforms[index], scales[index]));
        candidate.currentTransforms.push_back(matrix);
    }
    std::vector<bool> grouped(count, false);
    for (const DPIndexList& group : groups) {
        if (group.empty()) continue;
        std::vector<std::unique_ptr<db_collision_shape_base>> children;
        std::vector<btTransform> childTransforms;
        for (const auto id : group) {
            if (id >= count || grouped[id]) return MS::kInvalidParameter;
            grouped[id] = true;
            const auto item = getItem(id);
            std::unique_ptr<db_collision_shape_base> shape(createShape(item));
            if (!shape || !shape->shape()) return MS::kFailure;
            std::unique_ptr<customDBData> metadata(new customDBData);
            metadata->nodeUuid = nodeUuid;
            metadata->prefixName = nodeName;
            metadata->index = static_cast<int>(id);
            shape->set_user_data(metadata.release());
            children.push_back(std::move(shape));
            childTransforms.push_back(transforms[id]);
        }
        std::unique_ptr<db_collision_shape_base> shape(
            new db_composite_shape_t(std::move(children), childTransforms));
        btTransform transform;
        shape->getCenterOfMassTransformShift(transform);
        candidate.groups.push_back(group);
        candidate.compoundTransforms.push_back(transform);
        candidate.compound.push_back(std::move(shape));
    }
    for (std::size_t index = firstNew; index < count; ++index) {
        if (grouped[index]) continue;
        const auto item = getItem(index);
        std::unique_ptr<db_collision_shape_base> shape(createShape(item));
        if (!shape || !shape->shape()) return MS::kFailure;
        shape->set_itemIndex(index);
        candidate.transforms.push_back(transforms[index]);
        candidate.scales.push_back(scales[index]);
        candidate.single.push_back(std::move(shape));
    }
    return MS::kSuccess;
}
