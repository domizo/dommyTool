//
//  DOpenVDBGeoUtil.cpp
//  dommyTool
//
//  Created by yuzo on 12/17/16.
//
//

#include "DOpenVDBGeoUtil.h"

#include "common/utility/DCheckedSize.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"

#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MPointArray.h>

#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/Morphology.h>
#include <openvdb/tools/VolumeToMesh.h>

#include <cmath>
#include <exception>

namespace {

MStatus meshObjectFromDataBlock(const MObject& meshAttribute,
                                MDataBlock& data,
                                MObject& meshObject)
{
    MStatus status;
    MDataHandle handle = data.inputValue(meshAttribute, &status);
    if (status != MS::kSuccess || handle.type() != MFnData::kMesh) {
        return status == MS::kSuccess ? MS::kFailure : status;
    }

    const MObject candidate = handle.data();
    if (candidate.isNull()) {
        return MS::kFailure;
    }

    MFnMesh mesh(candidate, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    meshObject = candidate;
    return MS::kSuccess;
}

bool validVertexIndex(int index, std::size_t vertexCount)
{
    return index >= 0 &&
           dommy::memory::indexInRange(index, vertexCount);
}

bool appendPrimitive(int first,
                     int second,
                     int third,
                     bool hasFourth,
                     int fourth,
                     std::size_t vertexCount,
                     std::vector<openvdb::Vec4I>& primitives)
{
    if (!validVertexIndex(first, vertexCount) ||
        !validVertexIndex(second, vertexCount) ||
        !validVertexIndex(third, vertexCount) ||
        (hasFourth && !validVertexIndex(fourth, vertexCount)) ||
        primitives.size() >= dommy::generation::kMaxGeneratedFaceCount) {
        return false;
    }

    const openvdb::Index32 fourthIndex =
        hasFourth
            ? static_cast<openvdb::Index32>(fourth)
            : openvdb::util::INVALID_IDX;
    primitives.push_back(openvdb::Vec4I(
        static_cast<openvdb::Index32>(first),
        static_cast<openvdb::Index32>(second),
        static_cast<openvdb::Index32>(third),
        fourthIndex));
    return true;
}

} // namespace

MStatus DOpenVDBMakePointList(const MObject& meshObject,
                              const openvdb::math::Transform& transform,
                              std::vector<openvdb::Vec3s>& pointList)
{
    if (meshObject.isNull()) {
        return MS::kFailure;
    }

    MStatus status;
    MFnMesh mesh(meshObject, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    std::size_t vertexCount = 0U;
    if (!dommy::generation::checkedCount(
            mesh.numVertices(&status),
            dommy::generation::kMaxGeneratedPointCount,
            vertexCount) ||
        status != MS::kSuccess) {
        return status == MS::kSuccess ? MS::kFailure : status;
    }

    MFloatPointArray vertices;
    status = mesh.getPoints(vertices, MSpace::kObject);
    if (status != MS::kSuccess ||
        static_cast<std::size_t>(vertices.length()) != vertexCount) {
        return status == MS::kSuccess ? MS::kFailure : status;
    }

    std::vector<openvdb::Vec3s> candidate;
    candidate.reserve(vertexCount);
    for (unsigned int index = 0U; index < vertices.length(); ++index) {
        const MFloatPoint& vertex = vertices[index];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
            !std::isfinite(vertex.z)) {
            return MS::kFailure;
        }

        openvdb::Vec3d transformed;
        try {
            transformed = transform.worldToIndex(openvdb::Vec3d(
                static_cast<double>(vertex.x),
                static_cast<double>(vertex.y),
                static_cast<double>(vertex.z)));
        } catch (const std::exception&) {
            return MS::kFailure;
        }

        if (!dommy::numeric::canRepresentAsFloat(transformed[0]) ||
            !dommy::numeric::canRepresentAsFloat(transformed[1]) ||
            !dommy::numeric::canRepresentAsFloat(transformed[2])) {
            return MS::kFailure;
        }

        candidate.push_back(openvdb::Vec3s(
            static_cast<float>(transformed[0]),
            static_cast<float>(transformed[1]),
            static_cast<float>(transformed[2])));
    }

    pointList.swap(candidate);
    return MS::kSuccess;
}

MStatus DOpenVDBMakePointList(MObject meshAttribute,
                              MDataBlock& data,
                              const openvdb::math::Transform& transform,
                              std::vector<openvdb::Vec3s>& pointList)
{
    MObject meshObject;
    const MStatus status =
        meshObjectFromDataBlock(meshAttribute, data, meshObject);
    return status == MS::kSuccess
               ? DOpenVDBMakePointList(meshObject, transform, pointList)
               : status;
}

MStatus DOpenVDBMakePrimList(const MObject& meshObject,
                             std::vector<openvdb::Vec4I>& primList,
                             std::vector<int>* sourceFaces)
{
    if (meshObject.isNull()) {
        return MS::kFailure;
    }

    MStatus status;
    MFnMesh mesh(meshObject, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    std::size_t vertexCount = 0U;
    std::size_t polygonCount = 0U;
    const int rawVertexCount = mesh.numVertices(&status);
    if (status != MS::kSuccess ||
        !dommy::generation::checkedCount(
            rawVertexCount,
            dommy::generation::kMaxGeneratedPointCount,
            vertexCount)) {
        return status == MS::kSuccess ? MS::kFailure : status;
    }
    const int rawPolygonCount = mesh.numPolygons(&status);
    if (status != MS::kSuccess ||
        !dommy::generation::checkedCount(
            rawPolygonCount,
            dommy::generation::kMaxGeneratedFaceCount,
            polygonCount)) {
        return status == MS::kSuccess ? MS::kFailure : status;
    }

    MItMeshPolygon polygon(meshObject, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    std::vector<openvdb::Vec4I> candidate;
    std::vector<int> candidateFaces;
    candidate.reserve(polygonCount);
    std::size_t visitedPolygonCount = 0U;

    while (true) {
        const bool isDone = polygon.isDone(&status);
        if (status != MS::kSuccess) {
            return status;
        }
        if (isDone) {
            break;
        }
        if (visitedPolygonCount >= polygonCount) {
            return MS::kFailure;
        }
        ++visitedPolygonCount;

        const int faceIndex = polygon.index(&status);
        if (!status || faceIndex < 0 || static_cast<std::size_t>(faceIndex) >= polygonCount)
            return status ? MStatus(MS::kFailure) : status;
        const std::size_t primitiveStart = candidate.size();

        MIntArray vertices;
        status = polygon.getVertices(vertices);
        if (status != MS::kSuccess) {
            return status;
        }

        if (vertices.length() < 3U) {
            MGlobal::displayWarning(
                "Skipped unsupported single-point or line primitive.");
        } else if (vertices.length() <= 4U) {
            const bool hasFourth = vertices.length() == 4U;
            if (!appendPrimitive(vertices[0],
                                 vertices[1],
                                 vertices[2],
                                 hasFourth,
                                 hasFourth ? vertices[3] : 0,
                                 vertexCount,
                                 candidate)) {
                return MS::kFailure;
            }
        } else {
            MPointArray trianglePoints;
            MIntArray triangleVertices;
            status = polygon.getTriangles(
                trianglePoints, triangleVertices, MSpace::kObject);
            if (status != MS::kSuccess ||
                triangleVertices.length() == 0U ||
                triangleVertices.length() % 3U != 0U) {
                return status == MS::kSuccess ? MS::kFailure : status;
            }

            for (unsigned int index = 0U;
                 index < triangleVertices.length();
                 index += 3U) {
                if (!appendPrimitive(triangleVertices[index],
                                     triangleVertices[index + 1U],
                                     triangleVertices[index + 2U],
                                     false,
                                     0,
                                     vertexCount,
                                     candidate)) {
                    return MS::kFailure;
                }
            }
        }

        if (sourceFaces)
            candidateFaces.insert(candidateFaces.end(), candidate.size() - primitiveStart, faceIndex);
        status = polygon.next();
        if (status != MS::kSuccess) {
            return status;
        }
    }

    if (status != MS::kSuccess || visitedPolygonCount != polygonCount) {
        return status == MS::kSuccess ? MS::kFailure : status;
    }

    primList.swap(candidate);
    if (sourceFaces) sourceFaces->swap(candidateFaces);
    return MS::kSuccess;
}

MStatus DOpenVDBMakePrimList(MObject meshAttribute,
                             MDataBlock& data,
                             std::vector<openvdb::Vec4I>& primList)
{
    MObject meshObject;
    const MStatus status =
        meshObjectFromDataBlock(meshAttribute, data, meshObject);
    return status == MS::kSuccess
               ? DOpenVDBMakePrimList(meshObject, primList)
               : status;
}
