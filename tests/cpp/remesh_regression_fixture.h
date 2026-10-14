#pragma once

#include "geometry/DMeshRemesher.h"

// A simple concave polygon whose refined 2D mesh used to invert under the
// simultaneous Face Triangled relaxation at 0.75, after three iterations.
inline std::vector<dommy::geometry::Point2> faceSmoothingPolygonFixture()
{
    return {{1.6003568172454834,0},
        {0.13582871854305267,0.09868534654378891},
        {0.48542031645774841,1.4939700365066528},
        {-0.46584427356719971,1.4337213039398193},
        {-0.41131901741027832,0.29884076118469238},
        {-0.99811726808547974,1.222341070084064e-16},
        {-0.18442980945110321,-0.13399609923362732},
        {-0.28593549132347107,-0.88001900911331177},
        {0.17241035401821136,-0.53062450885772705},
        {0.65513724088668823,-0.47598505020141602}};
}

// Frozen float-representable planar input. The old simultaneous smoothing
// inverted one face after splitting/collapsing/flipping at target length 0.5.
inline dommy::geometry::TriangleMesh simultaneousSmoothingFixture()
{
    using namespace dommy::geometry;
    TriangleMesh mesh;
    mesh.points = {
        {0,0,0}, {1,0,0}, {2,0,0}, {3,0,0}, {4,0,0},
        {0,1,0}, {0.076857373118400574,0.10961054265499115,0},
        {1.5917148590087891,1.2735857963562012,0},
        {2.7037672996520996,0.68654912710189819,0}, {4,1,0},
        {0,2,0}, {1.6516118049621582,1.9368780851364136,0},
        {3.5761187076568604,2.8812167644500732,0},
        {3.8692841529846191,2.8887085914611816,0}, {4,2,0},
        {0,3,0}, {2.3992021083831787,2.4032113552093506,0},
        {2.4899227619171143,2.4365546703338623,0},
        {3.494837760925293,3.9582357406616211,0}, {4,3,0},
        {0,4,0}, {1,4,0}, {2,4,0}, {3,4,0}, {4,4,0}
    };
    for (std::size_t y = 0U; y < 4U; ++y)
        for (std::size_t x = 0U; x < 4U; ++x) {
            const std::size_t a = y * 5U + x;
            mesh.triangles.push_back(Triangle(a, a + 1U, a + 6U));
            mesh.triangles.push_back(Triangle(a, a + 6U, a + 5U));
        }
    return mesh;
}

inline dommy::geometry::TriangleMesh withUnusedVertex(
    const dommy::geometry::TriangleMesh& input, std::size_t position,
    const dommy::geometry::Point3& unused)
{
    dommy::geometry::TriangleMesh result = input;
    result.points.insert(result.points.begin() + position, unused);
    for (auto& triangle : result.triangles)
        for (std::size_t corner = 0U; corner < 3U; ++corner)
            if (triangle[corner] >= position) ++triangle[corner];
    return result;
}

inline dommy::geometry::TriangleMesh separatedTrianglesFixture(double separation,
                                                               double secondScale = 1.0)
{
    using namespace dommy::geometry;
    TriangleMesh result;
    result.points = {{0,0,0}, {0,1,0}, {0,0,1},
                     {separation,0,0}, {separation,secondScale,0}, {separation,0,secondScale}};
    result.triangles = {{0,1,2}, {3,4,5}};
    return result;
}
