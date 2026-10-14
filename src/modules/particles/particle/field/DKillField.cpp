#include "common/utility/DMayaEntryPointGuard.h"
#include "DKillField.h"

#include "DKillFieldMath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MBoundingBox.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MPlugArray.h>
#include <maya/MPointArray.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace {

const double kSurvivalLifespan = 100000.0;
const std::size_t kMaximumAttributeNameLength = 256U;
const std::size_t kMaximumExpressionBytes = 65536U;
const std::size_t kMaximumExpressionIntervals = 4096U;

static_assert(static_cast<int>(dommy::kill_field::kEqual) ==
                  static_cast<int>(D_CONDITION_EQUAL),
              "kill condition serialization changed");
static_assert(static_cast<int>(dommy::kill_field::kNotEqual) ==
                  static_cast<int>(D_CONDITION_NOT_EQUAL),
              "kill condition serialization changed");
static_assert(static_cast<int>(dommy::kill_field::kGreaterThan) ==
                  static_cast<int>(D_CONDITION_GREATER_THAN),
              "kill condition serialization changed");
static_assert(static_cast<int>(dommy::kill_field::kGreaterOrEqual) ==
                  static_cast<int>(D_CONDITION_GREATER_OR_EQUAL),
              "kill condition serialization changed");
static_assert(static_cast<int>(dommy::kill_field::kLessThan) ==
                  static_cast<int>(D_CONDITION_LESS_THAN),
              "kill condition serialization changed");
static_assert(static_cast<int>(dommy::kill_field::kLessOrEqual) ==
                  static_cast<int>(D_CONDITION_LESS_OR_EQUAL),
              "kill condition serialization changed");

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

dommy::kill_field::Point3d toCheckedPoint(const MVector& value)
{
    return dommy::kill_field::Point3d(value.x, value.y, value.z);
}

MStatus reportFailure(const MString& message)
{
    MGlobal::displayError(MString("DKillField: ") + message);
    return MS::kFailure;
}

MStatus findConnectedParticleSystem(const MPlug& outputPlug,
                                    MFnParticleSystem& particleSystem,
                                    bool& found)
{
    found = false;
    MStatus status;
    MPlugArray destinations;
    outputPlug.connectedTo(destinations, false, true, &status);
    if (!status) return status;

    MObject selectedParticle;
    for (unsigned int index = 0U; index < destinations.length(); ++index) {
        MFnParticleSystem candidate(destinations[index].node(), &status);
        if (!status) {
            status = MS::kSuccess;
            continue;
        }
        if (found && candidate.object() != selectedParticle) {
            return reportFailure(
                "one output element cannot target multiple particle systems.");
        }
        selectedParticle = candidate.object();
        found = true;
    }
    if (!found) return MS::kSuccess;
    status = particleSystem.setObject(selectedParticle);
    if (!status) return status;
    return MS::kSuccess;
}

MStatus readWorldToObjectMatrix(MDataBlock& data, MMatrix& worldToObject)
{
    MStatus status;
    MDataHandle matrixHandle = data.inputValue(
        DKillField::transformMatrix, &status);
    if (!status) return status;
    const MMatrix objectToWorld = matrixHandle.asMatrix();
    if (!dommy::maya::isFiniteMatrix(objectToWorld) ||
        objectToWorld.isSingular()) {
        return reportFailure(
            "transformMatrix must be finite and invertible.");
    }
    worldToObject = objectToWorld.inverse();
    if (!dommy::maya::isFiniteMatrix(worldToObject)) {
        return reportFailure("transformMatrix produced an invalid inverse.");
    }
    return MS::kSuccess;
}

bool convertWorldToObject(const MVector& worldPosition,
                          const MMatrix& worldToObject,
                          MPoint& objectPosition)
{
    if (!isFiniteVector(worldPosition)) return false;
    objectPosition = MPoint(worldPosition) * worldToObject;
    if (!dommy::maya::isFinitePoint(objectPosition) ||
        std::fabs(objectPosition.w) <= 1.0e-12) {
        return false;
    }
    objectPosition.cartesianize();
    return dommy::maya::isFinitePoint(objectPosition);
}

MStatus findConnectedBounds(const MObject& node, MBoundingBox& bounds, bool& found) {
    found = false;
    MStatus status;
    MPlug matrixPlug(node, DKillField::transformMatrix);
    MPlugArray sources;
    matrixPlug.connectedTo(sources, true, false, &status);
    if (!status)
        return status;

    for (unsigned int index = 0U; index < sources.length(); ++index) {
        MFnDagNode dagNode(sources[index].node(), &status);
        if (!status) {
            status = MS::kSuccess;
            continue;
        }
        bounds = dagNode.boundingBox(&status);
        if (!status)
            return status;
        if (!dommy::maya::isFiniteBoundingBox(bounds)) {
            return reportFailure("the transformMatrix source has an invalid bounding box.");
        }
        found = true;
        return MS::kSuccess;
    }
    return MS::kSuccess;
}

MStatus markBoundingBoxKills(MDataBlock& data, const MObject& node, const MVectorArray& positions,
                             MDoubleArray& lifespans) {
    MMatrix worldToObject;
    MStatus status = readWorldToObjectMatrix(data, worldToObject);
    if (!status)
        return status;

    MBoundingBox bounds;
    bool foundBounds = false;
    status = findConnectedBounds(node, bounds, foundBounds);
    if (!status)
        return status;
    if (!foundBounds) {
        return reportFailure("BBox mode requires transformMatrix to be connected to a DAG node.");
    }

    for (unsigned int index = 0U; index < positions.length(); ++index) {
        MPoint objectPosition;
        if (!convertWorldToObject(positions[index], worldToObject, objectPosition)) {
            return reportFailure("a particle position is not finite.");
        }
        if (bounds.contains(objectPosition))
            lifespans[index] = 0.0;
    }
    return MS::kSuccess;
}

MStatus buildMeshBounds(MFnMesh& mesh, MBoundingBox& bounds) {
    MStatus status;
    const int vertexCount = mesh.numVertices(&status);
    if (!status)
        return status;
    const int polygonCount = mesh.numPolygons(&status);
    if (!status)
        return status;
    if (vertexCount <= 0 || polygonCount <= 0 ||
        static_cast<std::size_t>(vertexCount) > dommy::generation::kMaxGeneratedPointCount ||
        static_cast<std::size_t>(polygonCount) > dommy::generation::kMaxGeneratedFaceCount) {
        return reportFailure("the input mesh is empty or exceeds safety limits.");
    }

    MPointArray points;
    status = mesh.getPoints(points, MSpace::kObject);
    if (!status)
        return status;
    if (points.length() != static_cast<unsigned int>(vertexCount)) {
        return reportFailure("the input mesh returned inconsistent vertices.");
    }
    bounds.clear();
    for (unsigned int index = 0U; index < points.length(); ++index) {
        if (!dommy::maya::isFinitePoint(points[index])) {
            return reportFailure("the input mesh contains a non-finite point.");
        }
        bounds.expand(points[index]);
    }
    if (!dommy::maya::isFiniteBoundingBox(bounds)) {
        return reportFailure("the input mesh has invalid bounds.");
    }
    return MS::kSuccess;
}

MStatus markMeshKills(MDataBlock& data, const MVectorArray& positions, MDoubleArray& lifespans) {
    MStatus status;
    MDataHandle meshHandle = data.inputValue(DKillField::inMeshObject, &status);
    if (!status)
        return status;
    if (meshHandle.type() != MFnData::kMesh || meshHandle.data().isNull()) {
        return reportFailure("Mesh mode requires a valid input mesh.");
    }
    MFnMesh mesh(meshHandle.data(), &status);
    if (!status)
        return status;

    MBoundingBox bounds;
    status = buildMeshBounds(mesh, bounds);
    if (!status) return status;

    MMatrix worldToObject;
    status = readWorldToObjectMatrix(data, worldToObject);
    if (!status) return status;

    for (unsigned int index = 0U; index < positions.length(); ++index) {
        MPoint objectPosition;
        if (!convertWorldToObject(
                positions[index], worldToObject, objectPosition)) {
            return reportFailure("a particle position is not finite.");
        }
        if (!bounds.contains(objectPosition)) continue;

        MPoint closestPoint;
        MVector normal;
        status = mesh.getClosestPointAndNormal(
            objectPosition, closestPoint, normal, MSpace::kObject);
        if (!status) return status;
        if (!dommy::maya::isFinitePoint(closestPoint) ||
            !isFiniteVector(normal)) {
            return reportFailure("the input mesh returned invalid surface data.");
        }
        const long double offsetX =
            static_cast<long double>(closestPoint.x) - objectPosition.x;
        const long double offsetY =
            static_cast<long double>(closestPoint.y) - objectPosition.y;
        const long double offsetZ =
            static_cast<long double>(closestPoint.z) - objectPosition.z;
        const long double dot =
            static_cast<long double>(normal.x) * offsetX +
            static_cast<long double>(normal.y) * offsetY +
            static_cast<long double>(normal.z) * offsetZ;
        if (!std::isfinite(dot)) {
            return reportFailure("the mesh inside test overflowed.");
        }
        if (dot > 0.0L) lifespans[index] = 0.0;
    }
    return MS::kSuccess;
}

MStatus markSpaceRangeKills(MDataBlock& data,
                            const MVectorArray& positions,
                            MDoubleArray& lifespans)
{
    MStatus status;
    MDataHandle minimumHandle = data.inputValue(
        DKillField::minKillSpaceRange, &status);
    if (!status) return status;
    const double3& rawMinimum = minimumHandle.asDouble3();
    const dommy::kill_field::Point3d minimum(
        rawMinimum[0], rawMinimum[1], rawMinimum[2]);

    MDataHandle maximumHandle = data.inputValue(
        DKillField::maxKillSpaceRange, &status);
    if (!status) return status;
    const double3& rawMaximum = maximumHandle.asDouble3();
    const dommy::kill_field::Point3d maximum(
        rawMaximum[0], rawMaximum[1], rawMaximum[2]);
    if (!dommy::kill_field::isFinite(minimum) ||
        !dommy::kill_field::isFinite(maximum) ||
        minimum.x > maximum.x || minimum.y > maximum.y ||
        minimum.z > maximum.z) {
        return reportFailure("the space range is invalid.");
    }

    for (unsigned int index = 0U; index < positions.length(); ++index) {
        const dommy::kill_field::Point3d point =
            toCheckedPoint(positions[index]);
        if (!dommy::kill_field::isFinite(point)) {
            return reportFailure("a particle position is not finite.");
        }
        if (point.x < minimum.x || point.y < minimum.y ||
            point.z < minimum.z || point.x > maximum.x ||
            point.y > maximum.y || point.z > maximum.z) {
            lifespans[index] = 0.0;
        }
    }
    return MS::kSuccess;
}

MStatus markAttributeKills(MDataBlock& data,
                           const MFnParticleSystem& particleSystem,
                           unsigned int receptorCount,
                           MDoubleArray& lifespans)
{
    MStatus status;
    MDataHandle nameHandle = data.inputValue(
        DKillField::killAttributeName, &status);
    if (!status) return status;
    const MString attributeName = nameHandle.asString();
    if (attributeName.length() == 0U ||
        attributeName.length() > kMaximumAttributeNameLength) {
        return reportFailure("the per-particle attribute name is invalid.");
    }

    MDataHandle typeHandle = data.inputValue(
        DKillField::killAttributeType, &status);
    if (!status) return status;
    const int attributeType = typeHandle.asShort();
    MDataHandle conditionHandle = data.inputValue(
        DKillField::killCondition, &status);
    if (!status) return status;
    const int condition = conditionHandle.asShort();
    if (attributeType < 0 || attributeType >= D_ATTR_TYPE_COUNT ||
        condition < 0 || condition >= D_CONDITION_TYPE_COUNT) {
        return reportFailure("the attribute comparison mode is invalid.");
    }

    MDataHandle targetHandle = data.inputValue(
        DKillField::killAttributeValue, &status);
    if (!status) return status;
    const double3& rawTarget = targetHandle.asDouble3();
    const dommy::kill_field::Point3d target(
        rawTarget[0], rawTarget[1], rawTarget[2]);
    if (!dommy::kill_field::isFinite(target)) {
        return reportFailure("the attribute comparison value is not finite.");
    }

    if (attributeType == D_ATTR_1D) {
        MDoubleArray values;
        const unsigned int valueCount = particleSystem.getPerParticleAttribute(
            attributeName, values, &status);
        if (!status || valueCount != receptorCount ||
            values.length() != receptorCount) {
            return reportFailure(
                "the scalar per-particle attribute has an invalid length.");
        }
        for (unsigned int index = 0U; index < receptorCount; ++index) {
            if (!std::isfinite(values[index])) {
                return reportFailure(
                    "the scalar per-particle attribute contains a "
                    "non-finite value.");
            }
            if (dommy::kill_field::compareScalar(
                    condition, values[index], target.x)) {
                lifespans[index] = 0.0;
            }
        }
        return MS::kSuccess;
    }

    MVectorArray values;
    const unsigned int valueCount = particleSystem.getPerParticleAttribute(
        attributeName, values, &status);
    if (!status || valueCount != receptorCount ||
        values.length() != receptorCount) {
        return reportFailure(
            "the vector per-particle attribute has an invalid length.");
    }

    double angleThreshold = 0.0;
    if (attributeType == D_ATTR_VEC_ANGLE) {
        MDataHandle angleHandle = data.inputValue(
            DKillField::killAngleValue, &status);
        if (!status) return status;
        angleThreshold = angleHandle.asDouble();
        if (!std::isfinite(angleThreshold)) {
            return reportFailure("the angle threshold is not finite.");
        }
    }

    for (unsigned int index = 0U; index < receptorCount; ++index) {
        const dommy::kill_field::Point3d value = toCheckedPoint(values[index]);
        if (!dommy::kill_field::isFinite(value)) {
            return reportFailure(
                "the vector per-particle attribute contains a "
                "non-finite value.");
        }
        bool shouldKill = false;
        switch (attributeType) {
            case D_ATTR_2D:
            case D_ATTR_3D:
                shouldKill = dommy::kill_field::compareComponents(
                    condition, value, target);
                break;
            case D_ATTR_VEC_MAGNITUDE:
                shouldKill = dommy::kill_field::compareMagnitude(
                    condition, value, target);
                break;
            case D_ATTR_VEC_ANGLE:
                shouldKill = dommy::kill_field::compareAngle(
                    condition, value, target, angleThreshold);
                break;
            default:
                return reportFailure("the vector comparison mode is invalid.");
        }
        if (shouldKill) lifespans[index] = 0.0;
    }
    return MS::kSuccess;
}

MStatus markDistanceKills(MDataBlock& data,
                          const MVectorArray& positions,
                          MDoubleArray& lifespans)
{
    MStatus status;
    MDataHandle distanceHandle = data.inputValue(
        DKillField::killDistance, &status);
    if (!status) return status;
    const double distance = distanceHandle.asDouble();

    std::vector<dommy::kill_field::Point3d> points;
    points.reserve(positions.length());
    for (unsigned int index = 0U; index < positions.length(); ++index) {
        points.push_back(toCheckedPoint(positions[index]));
    }
    std::vector<unsigned char> hasNeighbor;
    if (!dommy::kill_field::findNeighborPresence(
            points, distance, dommy::generation::kMaxWorkItemCount,
            hasNeighbor) || hasNeighbor.size() != points.size()) {
        return reportFailure(
            "the point-distance test is invalid or exceeded its work budget.");
    }
    for (unsigned int index = 0U; index < positions.length(); ++index) {
        if (hasNeighbor[index] == 0U) lifespans[index] = 0.0;
    }
    return MS::kSuccess;
}

MStatus markExpressionKills(MDataBlock& data,
                            const MFnParticleSystem& particleSystem,
                            unsigned int receptorCount,
                            MDoubleArray& lifespans)
{
    MStatus status;
    MDataHandle expressionHandle = data.inputValue(
        DKillField::killExpression, &status);
    if (!status) return status;
    const MString rawExpression = expressionHandle.asString();
    const char* const expressionCharacters = rawExpression.asChar();
    const std::string expression = expressionCharacters
        ? std::string(expressionCharacters) : std::string();

    std::vector<dommy::kill_field::IdInterval> intervals;
    if (!dommy::kill_field::parseIdExpression(
            expression, kMaximumExpressionBytes,
            kMaximumExpressionIntervals, intervals)) {
        return reportFailure(
            "killExpression must use non-negative IDs or end-exclusive "
            "start-end ranges separated by semicolons.");
    }

    MIntArray particleIds;
    particleSystem.particleIds(particleIds);
    if (particleIds.length() != receptorCount) {
        return reportFailure("the particle ID array has an invalid length.");
    }
    for (unsigned int index = 0U; index < receptorCount; ++index) {
        if (dommy::kill_field::containsId(intervals, particleIds[index])) {
            lifespans[index] = 0.0;
        }
    }
    return MS::kSuccess;
}

} // namespace

MObject DKillField::killType;
MObject DKillField::transformMatrix;
MObject DKillField::inMeshObject;
MObject DKillField::minKillSpaceRange;
MObject DKillField::minKillSpaceRangeX;
MObject DKillField::minKillSpaceRangeY;
MObject DKillField::minKillSpaceRangeZ;
MObject DKillField::maxKillSpaceRange;
MObject DKillField::maxKillSpaceRangeX;
MObject DKillField::maxKillSpaceRangeY;
MObject DKillField::maxKillSpaceRangeZ;
MObject DKillField::killAttributeName;
MObject DKillField::killAttributeType;
MObject DKillField::killAttributeValue;
MObject DKillField::killAttributeValueX;
MObject DKillField::killAttributeValueY;
MObject DKillField::killAttributeValueZ;
MObject DKillField::killCondition;
MObject DKillField::killAngleValue;
MObject DKillField::killDistance;
MObject DKillField::killExpression;
MTypeId DKillField::id(0x0402A);

void* DKillField::creator()
{
    return ::dommy::maya::createObject("DKillField", [&]() {
        return new DKillField();
    });
}

MStatus DKillField::initialize()
{
    MStatus status;
    MFnEnumAttribute enumAttribute;
    killType = enumAttribute.create("killType", "kt", D_KILL_BBOX, &status);
    if (!status) return status;
    status = enumAttribute.addField("BBox", D_KILL_BBOX);
    if (!status) return status;
    status = enumAttribute.addField("Mesh", D_KILL_MESH);
    if (!status) return status;
    status = enumAttribute.addField("Space Range", D_KILL_SPACERANGE);
    if (!status) return status;
    status = enumAttribute.addField("Attribute", D_KILL_ATTRIBUTE);
    if (!status) return status;
    status = enumAttribute.addField("Point Distance", D_KILL_PP_DISTANCE);
    if (!status) return status;
    status = enumAttribute.addField("Expression", D_KILL_EXPRESSION);
    if (!status) return status;
    status = enumAttribute.setKeyable(true);
    if (!status) return status;
    status = enumAttribute.setStorable(true);
    if (!status) return status;

    MFnMatrixAttribute matrixAttribute;
    transformMatrix = matrixAttribute.create(
        "transformMatrix", "tm", MFnMatrixAttribute::kDouble, &status);
    if (!status) return status;
    status = matrixAttribute.setStorable(true);
    if (!status) return status;
    status = matrixAttribute.setKeyable(false);
    if (!status) return status;
    status = matrixAttribute.setReadable(true);
    if (!status) return status;
    status = matrixAttribute.setWritable(true);
    if (!status) return status;
    status = matrixAttribute.setCached(false);
    if (!status) return status;

    MFnTypedAttribute typedAttribute;
    inMeshObject = typedAttribute.create(
        "inMeshObject", "imo", MFnData::kMesh, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = typedAttribute.setHidden(true);
    if (!status) return status;

    MFnNumericAttribute numericAttribute;
    minKillSpaceRangeX = numericAttribute.create(
        "minKillSpaceRangeX", "mikx", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    minKillSpaceRangeY = numericAttribute.create(
        "minKillSpaceRangeY", "miky", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    minKillSpaceRangeZ = numericAttribute.create(
        "minKillSpaceRangeZ", "mikz", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    minKillSpaceRange = numericAttribute.create(
        "minKillSpaceRange", "miks", minKillSpaceRangeX,
        minKillSpaceRangeY, minKillSpaceRangeZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(-1000.0, -1000.0, -1000.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;

    maxKillSpaceRangeX = numericAttribute.create(
        "maxKillSpaceRangeX", "makx", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    maxKillSpaceRangeY = numericAttribute.create(
        "maxKillSpaceRangeY", "maky", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    maxKillSpaceRangeZ = numericAttribute.create(
        "maxKillSpaceRangeZ", "makz", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    maxKillSpaceRange = numericAttribute.create(
        "maxKillSpaceRange", "maks", maxKillSpaceRangeX,
        maxKillSpaceRangeY, maxKillSpaceRangeZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(1000.0, 1000.0, 1000.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;

    MFnStringData stringData;
    const MObject emptyString = stringData.create("", &status);
    if (!status) return status;
    killAttributeName = typedAttribute.create(
        "killAttributeName", "katn", MFnData::kString,
        emptyString, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(true);
    if (!status) return status;
    status = typedAttribute.setUsedAsFilename(false);
    if (!status) return status;

    killAttributeType = enumAttribute.create(
        "killAttributeType", "katt", D_ATTR_1D, &status);
    if (!status) return status;
    status = enumAttribute.addField("1D", D_ATTR_1D);
    if (!status) return status;
    status = enumAttribute.addField("2D", D_ATTR_2D);
    if (!status) return status;
    status = enumAttribute.addField("3D", D_ATTR_3D);
    if (!status) return status;
    status = enumAttribute.addField("Magnitude", D_ATTR_VEC_MAGNITUDE);
    if (!status) return status;
    status = enumAttribute.addField("Angle", D_ATTR_VEC_ANGLE);
    if (!status) return status;
    status = enumAttribute.setKeyable(true);
    if (!status) return status;
    status = enumAttribute.setStorable(true);
    if (!status) return status;

    killAttributeValueX = numericAttribute.create(
        "killAttributeValueX", "katx", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    killAttributeValueY = numericAttribute.create(
        "killAttributeValueY", "katy", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    killAttributeValueZ = numericAttribute.create(
        "killAttributeValueZ", "katz", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    killAttributeValue = numericAttribute.create(
        "killAttributeValue", "kav", killAttributeValueX,
        killAttributeValueY, killAttributeValueZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(0.0, 0.0, 0.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;

    killCondition = enumAttribute.create(
        "killCondition", "kco", D_CONDITION_EQUAL, &status);
    if (!status) return status;
    status = enumAttribute.addField("Equal", D_CONDITION_EQUAL);
    if (!status) return status;
    status = enumAttribute.addField("Not Equal", D_CONDITION_NOT_EQUAL);
    if (!status) return status;
    status = enumAttribute.addField(
        "Greater Than", D_CONDITION_GREATER_THAN);
    if (!status) return status;
    status = enumAttribute.addField(
        "Greater or Equal", D_CONDITION_GREATER_OR_EQUAL);
    if (!status) return status;
    status = enumAttribute.addField("Less Than", D_CONDITION_LESS_THAN);
    if (!status) return status;
    status = enumAttribute.addField(
        "Less or Equal", D_CONDITION_LESS_OR_EQUAL);
    if (!status) return status;
    status = enumAttribute.setKeyable(true);
    if (!status) return status;
    status = enumAttribute.setStorable(true);
    if (!status) return status;

    killAngleValue = numericAttribute.create(
        "killAngleValue", "kagv", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;

    killDistance = numericAttribute.create(
        "killDistance", "kdis", MFnNumericData::kDouble, 5.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;

    killExpression = typedAttribute.create(
        "killExpression", "kexp", MFnData::kString, emptyString, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(true);
    if (!status) return status;
    status = typedAttribute.setUsedAsFilename(false);
    if (!status) return status;

    const MObject attributes[] = {
        killType,
        transformMatrix,
        inMeshObject,
        minKillSpaceRange,
        maxKillSpaceRange,
        killAttributeName,
        killAttributeType,
        killAttributeValue,
        killCondition,
        killAngleValue,
        killDistance,
        killExpression,
    };
    for (std::size_t index = 0U;
         index < sizeof(attributes) / sizeof(attributes[0]); ++index) {
        status = addAttribute(attributes[index]);
        if (!status) return status;
    }

    const MObject affectingAttributes[] = {
        killType,
        transformMatrix,
        inMeshObject,
        minKillSpaceRange,
        minKillSpaceRangeX,
        minKillSpaceRangeY,
        minKillSpaceRangeZ,
        maxKillSpaceRange,
        maxKillSpaceRangeX,
        maxKillSpaceRangeY,
        maxKillSpaceRangeZ,
        killAttributeName,
        killAttributeType,
        killAttributeValue,
        killAttributeValueX,
        killAttributeValueY,
        killAttributeValueZ,
        killCondition,
        killAngleValue,
        killDistance,
        killExpression,
    };
    for (std::size_t index = 0U;
         index < sizeof(affectingAttributes) /
             sizeof(affectingAttributes[0]);
         ++index) {
        status = attributeAffects(
            affectingAttributes[index], mOutputForce);
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus DKillField::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutputForce) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        return reportFailure("received an invalid output index.");
    }
    const unsigned int logicalIndex =
        static_cast<unsigned int>(rawLogicalIndex);

    // outputArrayValue is required for field inputs: evaluating all logical
    // input elements can create a dependency-graph cycle.
    MArrayDataHandle inputArray = data.outputArrayValue(mInputData, &status);
    if (!status) return status;
    status = inputArray.jumpToElement(logicalIndex);
    if (!status) return status;
    MDataHandle compound = inputArray.inputValue(&status);
    if (!status) return status;
    MDataHandle positionHandle = compound.child(mInputPositions);
    MFnVectorArrayData positionData(positionHandle.data(), &status);
    if (!status) return status;
    const MVectorArray positions = positionData.array(&status);
    if (!status) return status;

    const unsigned int receptorCount = positions.length();
    if (static_cast<std::size_t>(receptorCount) >
        dommy::generation::kMaxGeneratedPointCount) {
        return reportFailure("the particle count exceeds the safety limit.");
    }
    MVectorArray forceArray;
    status = dommy::maya::buildCheckedArray(
        receptorCount,
        [](unsigned int, MVector& value) -> MStatus {
            value = MVector::zero;
            return MS::kSuccess;
        }, forceArray);
    if (!status) return status;
    for (unsigned int index = 0U; index < receptorCount; ++index) {
        if (!isFiniteVector(positions[index])) {
            return reportFailure("a particle position is not finite.");
        }
    }

    // Prepare the output before any lifespanPP mutation. The final host setter
    // and clean operation can still fail; this is not a native rollback promise.
    MArrayDataHandle outputArray = data.outputArrayValue(mOutputForce, &status);
    if (!status) return status;
    MArrayDataBuilder outputBuilder = outputArray.builder(&status);
    if (!status) return status;
    MDataHandle outputHandle = outputBuilder.addElement(logicalIndex, &status);
    if (!status) return status;
    MFnVectorArrayData outputData;
    const MObject outputObject = outputData.create(forceArray, &status);
    if (!status) return status;
    const auto finishOutput = [&]() -> MStatus {
        const MStatus finishStatus = outputHandle.set(outputObject);
        return finishStatus ? data.setClean(plug) : finishStatus;
    };

    MFnParticleSystem particleSystem;
    bool foundParticleSystem = false;
    status = findConnectedParticleSystem(
        plug, particleSystem, foundParticleSystem);
    if (!status) return status;
    if (!foundParticleSystem) {
        MGlobal::displayWarning(
            "DKillField has no connected particle system; zero force was output.");
        return finishOutput();
    }
    if (particleSystem.count() != receptorCount) {
        MGlobal::displayWarning(
            "DKillField particle counts do not match; no lifespan was changed.");
        return finishOutput();
    }
    if (receptorCount == 0U) {
        return finishOutput();
    }

    MDataHandle killTypeHandle = data.inputValue(killType, &status);
    if (!status) return status;
    const int selectedKillType = killTypeHandle.asShort();
    if (selectedKillType < 0 || selectedKillType >= D_KILL_TYPE_COUNT) {
        return reportFailure("killType is invalid.");
    }

    // Build the complete result before mutating the external particle node.
    // This prevents malformed input from leaving a partially updated lifespan.
    MDoubleArray lifespans;
    status = dommy::maya::buildCheckedArray(
        receptorCount,
        [](unsigned int, double& value) -> MStatus {
            value = kSurvivalLifespan;
            return MS::kSuccess;
        }, lifespans);
    if (!status) return status;
    switch (selectedKillType) {
        case D_KILL_BBOX:
            status = markBoundingBoxKills(
                data, thisMObject(), positions, lifespans);
            break;
        case D_KILL_MESH:
            status = markMeshKills(data, positions, lifespans);
            break;
        case D_KILL_SPACERANGE:
            status = markSpaceRangeKills(data, positions, lifespans);
            break;
        case D_KILL_ATTRIBUTE:
            status = markAttributeKills(
                data, particleSystem, receptorCount, lifespans);
            break;
        case D_KILL_PP_DISTANCE:
            status = markDistanceKills(data, positions, lifespans);
            break;
        case D_KILL_EXPRESSION:
            status = markExpressionKills(
                data, particleSystem, receptorCount, lifespans);
            break;
        default:
            status = MS::kFailure;
            break;
    }
    if (!status) return status;

    particleSystem.setPerParticleAttribute(
        MString("lifespanPP"), lifespans, &status);
    if (!status) return status;
    return finishOutput();
}
DOMMY_COMPUTE_CATCH("DKillField")
