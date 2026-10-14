// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/convex_decomposition_hacd.cpp

// HACD collision construction. All intermediate ownership is scoped.
#include "convex_decomposition_hacd.h"

#include "HACD/hacdHACD.h"
#include "btBulletDynamicsCommon.h"
#include "common/utility/DGenerationLimits.h"

#include <maya/MProgressWindow.h>
#include <maya/MGlobal.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace {
class DOwningCompoundShape : public btCompoundShape
{
public:
    ~DOwningCompoundShape() override = default;

    void addOwnedChild(const btTransform& transform,
                       std::unique_ptr<btConvexHullShape> child)
    {
        // Own first: even a partially completed Bullet insertion cannot orphan
        // the hull. On failure the entire unpublished compound is discarded.
        m_children.push_back(std::move(child));
        addChildShape(transform, m_children.back().get());
    }

private:
    std::vector<std::unique_ptr<btConvexHullShape> > m_children;
};

class ProgressWindowGuard
{
public:
    ProgressWindowGuard() : m_active(MProgressWindow::reserve())
    {
        if (!m_active) return;
        MProgressWindow::setProgressRange(0, 1000);
        MProgressWindow::setTitle("HACD convex decomposition");
        MProgressWindow::setInterruptable(true);
        MProgressWindow::setProgress(0);
        if (MProgressWindow::startProgress() != MS::kSuccess) {
            MProgressWindow::endProgress();
            m_active = false;
        }
    }

    ~ProgressWindowGuard()
    {
        if (m_active) MProgressWindow::endProgress();
    }

    bool active() const { return m_active; }

private:
    bool m_active;
};

bool mayaCallback(const char* message, double current, double total, size_t)
{
    const double fraction =
        (std::isfinite(current) && std::isfinite(total) && total > 0.0)
        ? std::max(0.0, std::min(1.0, current / total)) : 0.0;
    CHECK_MSTATUS(MProgressWindow::setProgress(static_cast<int>(fraction * 1000.0)));
    CHECK_MSTATUS(MProgressWindow::setProgressStatus(MString(message ? message : "HACD")));
    if (MProgressWindow::isCancelled()) {
        MGlobal::displayInfo("Progress interrupted!");
        return true;
    }
    return false;
}
} // namespace

btCompoundShape* convex_decomposition_hacd::ConvexDecomp(
    int numVertices, const float* vertices,
    int numIndices, const unsigned int* indices)
{
    using dommy::generation::kMaxGeneratedPointCount;
    using dommy::generation::kMaxGeneratedFaceCount;
    if (!vertices || !indices || numVertices < 3 || numIndices < 3 ||
        numIndices % 3 != 0 ||
        static_cast<size_t>(numVertices) > kMaxGeneratedPointCount ||
        static_cast<size_t>(numIndices / 3) > kMaxGeneratedFaceCount) {
        return nullptr;
    }
    for (int i = 0; i < numIndices; i += 3) {
        const unsigned int a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= static_cast<unsigned int>(numVertices) ||
            b >= static_cast<unsigned int>(numVertices) ||
            c >= static_cast<unsigned int>(numVertices) || a == b || b == c || c == a) {
            return nullptr;
        }
    }

    std::vector<HACD::Vec3<HACD::Real> > points;
    points.reserve(static_cast<size_t>(numVertices));
    for (int i = 0; i < numVertices; ++i) {
        const size_t offset = static_cast<size_t>(i) * 3U;
        const float x = vertices[offset], y = vertices[offset + 1], z = vertices[offset + 2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return nullptr;
        points.push_back(HACD::Vec3<HACD::Real>(x, y, z));
    }
    std::vector<HACD::Vec3<long> > triangles;
    triangles.reserve(static_cast<size_t>(numIndices / 3));
    for (int i = 0; i < numIndices; i += 3) {
        triangles.push_back(HACD::Vec3<long>(indices[i], indices[i + 1], indices[i + 2]));
    }

    HACD::HACD decomposition;
    decomposition.SetPoints(points.data());
    decomposition.SetNPoints(points.size());
    decomposition.SetTriangles(triangles.data());
    decomposition.SetNTriangles(triangles.size());
    decomposition.SetCompacityWeight(0.1);
    decomposition.SetVolumeWeight(0.0);
    decomposition.SetNClusters(2);
    decomposition.SetNVerticesPerCH(256);
    decomposition.SetConcavity(10);
    decomposition.SetAddExtraDistPoints(true);
    decomposition.SetAddNeighboursDistPoints(true);
    decomposition.SetAddFacesPoints(true);
    ProgressWindowGuard progress;
    if (progress.active()) decomposition.SetCallBack(mayaCallback);
    if (!decomposition.Compute()) return nullptr;

    const size_t clusterCount = decomposition.GetNClusters();
    if (clusterCount == 0 || clusterCount > kMaxGeneratedFaceCount) return nullptr;
    std::unique_ptr<DOwningCompoundShape> compound(new DOwningCompoundShape());
    size_t totalPoints = 0, totalTriangles = 0;
    for (size_t cluster = 0; cluster < clusterCount; ++cluster) {
        const size_t pointCount = decomposition.GetNPointsCH(cluster);
        const size_t triangleCount = decomposition.GetNTrianglesCH(cluster);
        if (pointCount == 0 || triangleCount == 0 ||
            pointCount > kMaxGeneratedPointCount - totalPoints ||
            triangleCount > kMaxGeneratedFaceCount - totalTriangles ||
            pointCount > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return nullptr;
        }
        totalPoints += pointCount;
        totalTriangles += triangleCount;
        std::vector<HACD::Vec3<HACD::Real> > hullPoints(pointCount);
        std::vector<HACD::Vec3<long> > hullTriangles(triangleCount);
        if (!decomposition.GetCH(cluster, hullPoints.data(), hullTriangles.data())) return nullptr;

        double center[3] = {0.0, 0.0, 0.0};
        for (size_t i = 0; i < pointCount; ++i) {
            const double coordinates[3] = {hullPoints[i].X(), hullPoints[i].Y(), hullPoints[i].Z()};
            for (int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(coordinates[axis]) ||
                    std::abs(coordinates[axis]) > std::numeric_limits<float>::max()) return nullptr;
                center[axis] += coordinates[axis] / static_cast<double>(pointCount);
            }
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(center[axis]) ||
                std::abs(center[axis]) > std::numeric_limits<btScalar>::max()) return nullptr;
        }
        for (size_t i = 0; i < triangleCount; ++i) {
            const long a = hullTriangles[i].X(), b = hullTriangles[i].Y(), c = hullTriangles[i].Z();
            if (a < 0 || b < 0 || c < 0 ||
                static_cast<size_t>(a) >= pointCount || static_cast<size_t>(b) >= pointCount ||
                static_cast<size_t>(c) >= pointCount || a == b || b == c || c == a) return nullptr;
        }
        std::vector<btVector3> localPoints;
        localPoints.reserve(pointCount);
        for (size_t i = 0; i < pointCount; ++i) {
            const double local[3] = {hullPoints[i].X() - center[0],
                                     hullPoints[i].Y() - center[1], hullPoints[i].Z() - center[2]};
            for (int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(local[axis]) ||
                    std::abs(local[axis]) > std::numeric_limits<btScalar>::max()) return nullptr;
            }
            localPoints.push_back(btVector3(local[0], local[1], local[2]));
        }
        std::unique_ptr<btConvexHullShape> hull(new btConvexHullShape(
            &localPoints[0].getX(), static_cast<int>(pointCount), sizeof(btVector3)));
        hull->setMargin(0.01f);
        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(btVector3(center[0], center[1], center[2]));
        compound->addOwnedChild(transform, std::move(hull));
    }
    return compound.release();
}
