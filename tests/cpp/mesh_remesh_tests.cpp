#include "mesh_remesh_doubles.h"
#include "remesh_regression_fixture.h"
#include "mesh_remesh.inc"

static void fixture() {
    reset();
    injectedStatus = MS::kFailure;
    inputs.clear();
    emitterMeshes.clear();
    badNativeCounts = false;
    reportedTriangles = -1;
    triangleReadFault = 0;
    oversizedNativePoints = false;
    remeshInput = RemeshData();
    remeshInput.items.push_back(std::make_shared<RemeshItem>());
    remeshOutput = RemeshData();
    outputData = DCurveData();
    inputs["MaxRange"].number = .3;
    inputs["AspectBound"].number = .125;
    inputs["Iterate"].number = 1;
    inputs["PlanePosition"].vec[1] = .1f;
    inputs["PlaneNormal"].vec[1] = 1;
    inputs["CutCount"].number = 1;
    inputs["Step"].number = 1;
    inputs["Degree"].number = 0;
    inputs["IsOpen"].number = 1;
}

static MStatus compute(int mode) {
    MDataBlock data;
    if (mode == 3)
        return DMSlicer().compute(MPlug({}, DMSlicer::outDC), data);
    if (mode == 2)
        return DMDebrisGen().compute(MPlug({}, DMDebrisGen::outDM), data);
    inputs["MethodType"].number = mode;
    return DMRemesh().compute(MPlug({}, DMRemesh::outDM), data);
}
static bool sameMatrix(const MMatrix& a, const MMatrix& b) {
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
            if (a[r][c] != b[r][c])
                return false;
    return true;
}
static bool samePoints(const DPointList& a, const DPointList& b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        for (unsigned c = 0; c < 3; ++c)
            if (a[i][c] != b[i][c])
                return false;
    return true;
}
static void prepareBranchingSlice(std::size_t count) {
    fixture();
    auto& item = *remeshInput.items[0];
    item.points = {{0, 0, 0}};
    item.faces.clear();
    item.connections.clear();
    for (std::size_t i = 0U; i < count; ++i) {
        const double angle = double(i) * 6.28318530717958647692 / double(count);
        const double x = static_cast<float>(std::cos(angle));
        const double y = static_cast<float>(std::sin(angle));
        item.points.push_back(MPoint(x, y, 1));
        item.points.push_back(MPoint(x, y, -1));
        item.faces.push_back(3);
        item.connections.push_back(0);
        item.connections.push_back(static_cast<int>(2U*i + 1U));
        item.connections.push_back(static_cast<int>(2U*i + 2U));
    }
    inputs["PlanePosition"].vec[1] = 0;
    inputs["PlaneNormal"].vec[1] = 0;
    inputs["PlaneNormal"].vec[2] = 1;
}

static void branchingSliceBatch() {
    prepareBranchingSlice(1000U);
    const auto before = remeshInput.items[0]->points;
    assert(compute(3) && published == 1 && outputData.numberOfCurves() == 1000U);
    assert(std::count(operations.begin(), operations.end(), "batch.insert") == 1);
    assert(samePoints(before, remeshInput.items[0]->points));
    for (std::size_t i = 0U; i < 1000U; ++i) {
        const auto& curve = outputData.curves[i];
        assert(curve->points.size() == 2U && curve->points[0].x == 0 && curve->points[0].y == 0);
        assert(curve->points[1].x == before[2U*i + 1U].x);
        assert(curve->points[1].y == before[2U*i + 1U].y && curve->points[1].z == 0);
    }
    std::cout << "Branching slicer output keeps contour order and uses one transactional curve batch\n";
}

// Run unchanged production compute/helper bodies with only WorkBudget's
// default reduced to 2048 in a temporary header; admission policy stays intact.
static void aggregateSliceWorkBudget() {
    assert(dommy::generation::WorkBudget().remaining() == 2048U);
    fixture();
    assert(compute(3) && published == 1 && outputData.numberOfCurves() == 1U);
    const auto previous = outputData;
    for (int scenario : {0, 1, 2}) {
        fixture();
        if (scenario == 0) {
            const auto source = remeshInput.items[0];
            remeshInput.items.assign(8, source);
        }
        if (scenario == 1) {
            inputs["CutCount"].number = 8;
            inputs["Step"].number = 0;
        }
        if (scenario == 2) prepareBranchingSlice(64U);
        outputData = previous;
        const auto points = remeshInput.items[0]->points;
        assert(!compute(3) && published == 0 && cleaned == 0);
        assert(outputData.numberOfCurves() == previous.numberOfCurves());
        assert(samePoints(outputData.curves[0]->points, previous.curves[0]->points));
        assert(samePoints(remeshInput.items[0]->points, points));
        assert(std::count(operations.begin(), operations.end(), "batch.insert") == 0);
        fixture();
        assert(compute(3) && published == 1);
    }
    fixture();
    inputs["PlanePosition"].vec[1] = 100;
    inputs["CutCount"].number = 100;
    assert(!compute(3) && published == 0 && cleaned == 0);
    std::cout << "Slice work stays shared across meshes/cuts; failure keeps cached output and permits retry\n";
}
static void smallPolygonRegression() {
    for (int mode : {1, 2}) {
        for (float edge : {1.0e-9f, 1.0e-6f, 1.0e-4f}) {
            fixture();
            remeshInput.items[0]->points = {{0, 0, 0}, {edge, 0, 0},
                                          {edge, edge, 0}, {0, edge, 0}};
            remeshInput.items[0]->matrix[3][0] = 17;
            const auto before = remeshInput.items[0]->points;
            assert(compute(mode) && published == 1 && remeshOutput.items.size() == 1U);
            const auto& output = *remeshOutput.items[0];
            assert(output.points.size() == 4U && output.faces.size() == 2U);
            assert(output.connections.size() == 6U && output.matrix[3][0] == 17);
            assert(samePoints(before, remeshInput.items[0]->points));
            for (std::size_t triangle = 0; triangle < 2; ++triangle) {
                const auto& a = output.points[output.connections[triangle*3]];
                const auto& b = output.points[output.connections[triangle*3+1]];
                const auto& c = output.points[output.connections[triangle*3+2]];
                assert((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x) > 0);
            }
        }
    }
    std::cout << "Remesh/debris keep small nondegenerate faces and their primitive transforms\n";
}

static void simultaneousFaceSmoothingRegression() {
    for (int iterations : {1, 4}) {
        fixture();
        auto& source = *remeshInput.items[0];
        source.points.clear(); source.faces = {10};
        source.connections = {0,1,2,3,4,5,6,7,8,9};
        for (const auto& p : faceSmoothingPolygonFixture())
            source.points.push_back(MPoint(p.x, p.y, 0));
        source.matrix[3][2] = 17;
        const auto before = source.points;
        inputs["MaxRange"].number = .4f;
        inputs["AspectBound"].number = .5f;
        inputs["Iterate"].number = iterations;
        assert(compute(1) && published == 1 && remeshOutput.items.size() == 1U);
        const auto& output = *remeshOutput.items[0];
        assert(output.points.size() == 85U && output.faces.size() == 128U);
        assert(samePoints(before, source.points) && sameMatrix(source.matrix, output.matrix));
        for (std::size_t i = 0U; i < output.connections.size(); i += 3U) {
            const auto& a = output.points[output.connections[i]];
            const auto& b = output.points[output.connections[i+1]];
            const auto& c = output.points[output.connections[i+2]];
            assert((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x) > 0.0);
        }
    }
    std::cout << "Complete Face Triangled node keeps all 128 faces oriented after simultaneous smoothing\n";
}

static void nonPlanarFaceRejection() {
    for (int mode : {1,2}) {
        fixture();
        assert(compute(mode) && published == 1);
        const auto previous = remeshOutput.items[0];
        for (bool secondMesh : {false,true}) {
            fixture();
            auto warped = std::make_shared<RemeshItem>();
            warped->points = {{0,0,0},{1,0,0},{1,1,1},{0,1,0}};
            if (secondMesh) remeshInput.items.push_back(warped);
            else remeshInput.items[0] = warped;
            const auto before = warped->points;
            remeshOutput.items = {previous};
            assert(!compute(mode) && published == 0 && cleaned == 0);
            assert(remeshOutput.items.size() == 1U && remeshOutput.items[0] == previous);
            assert(samePoints(before, warped->points));
            fixture();
            assert(compute(mode) && published == 1);
        }
        fixture();
        auto& source = *remeshInput.items[0];
        source.points = {{0,0,0},{double(.2f),0,double(.2f)},
            {double(.2f),double(.1f),double(.3f)},{0,double(.1f),double(.1f)}};
        const auto before = source.points;
        inputs["MaxRange"].number = 10;
        assert(compute(mode) && published == 1);
        const auto& output = *remeshOutput.items[0];
        assert(output.points.size() >= before.size());
        for (std::size_t i = 0; i < before.size(); ++i) {
            const auto& a = before[i]; const auto& b = output.points[i];
            assert(a.x == b.x && a.y == b.y && a.z == b.z);
        }
        assert(samePoints(before, source.points));
    }
    std::cout << "Face nodes reject nonplanar geometry without replacing output, and keep accepted boundary coordinates exact\n";
}

static void translatedGeometry() {
    for (double x : {0.0, double(1e12f), double(-1e12f), double(1e30f)}) {
        for (int mode : {0, 3}) {
            fixture();
            auto& source = *remeshInput.items[0];
            source.points = {{x, 0, 0}, {x, 1, 0}, {x, 0, 1}};
            source.faces = {3}; source.connections = {0, 1, 2};
            source.matrix[3][2] = 17;
            inputs["MaxRange"].number = 2;
            inputs["PlanePosition"].vec[1] = .5f;
            const auto before = source.points;
            assert(compute(mode) && published == 1);
            assert(samePoints(source.points, before));
            if (mode == 0) {
                assert(remeshOutput.items.size() == 1U);
                const auto& output = *remeshOutput.items[0];
                assert(output.faces.size() == 1U && samePoints(output.points, before));
                assert(sameMatrix(output.matrix, source.matrix));
            } else {
                assert(outputData.numberOfCurves() == 1U && outputData.curves[0]->points.size() == 2U);
                for (const auto& point : outputData.curves[0]->points)
                    assert(point.x == x && point.y == .5);
            }
        }
    }
    std::cout << "Complete remesh/slicer bodies preserve translated local geometry and source transforms\n";
}

static void unusedGeometryVertices() {
    dommy::geometry::TriangleMesh input;
    input.points = {{0,0,0}, {1,0,0}, {0,1,0}};
    input.triangles = {{0,1,2}};
    for (int mode : {0, 3}) {
        DPointList baseline;
        for (std::size_t position : {0U, 1U, 3U, 4U}) {
            const auto padded = position == 4U ? input :
                withUnusedVertex(input, position, {double(1e30f), double(1e30f), double(1e30f)});
            fixture();
            auto& source = *remeshInput.items[0];
            source.points = {};
            source.faces = {3};
            source.connections = {};
            for (const auto& point : padded.points)
                source.points.push_back(MPoint(point.x, point.y, point.z));
            for (std::size_t corner = 0U; corner < 3U; ++corner)
                source.connections.push_back(int(padded.triangles[0][corner]));
            source.matrix[3][0] = 17;
            const auto before = source.points;
            inputs["MaxRange"].number = 2;
            inputs["PlanePosition"].vec[1] = .5f;
            assert(compute(mode) && published == 1);
            assert(samePoints(source.points, before));
            const auto points = mode == 0 ? remeshOutput.items.at(0)->points :
                outputData.curves.at(0)->points;
            if (baseline.empty()) baseline = points;
            else assert(samePoints(points, baseline));
            if (mode == 0) assert(sameMatrix(remeshOutput.items.at(0)->matrix, source.matrix));
            else assert(outputData.numberOfCurves() == 1U);
        }
    }
    std::cout << "Complete remesh/slicer bodies ignore finite unused outliers without changing input\n";
}

// Runs with only the polygon budget's default reduced to 256 in a
// disposable header. Complete production compute/helper bodies are unchanged.
static void aggregatePolygonBudget() {
    assert(dommy::geometry::PolygonWorkBudget().remaining() == 256U);
    for (int mode : {1, 2}) {
        fixture();
        assert(compute(mode) && published == 1);
        fixture();
        const auto source = remeshInput.items[0];
        const auto before = source->points;
        remeshInput.items.assign(3, source);
        remeshOutput.items.push_back(std::make_shared<RemeshItem>());
        remeshOutput.items[0]->matrix[3][0] = 99;
        assert(!compute(mode) && published == 0 && cleaned == 0);
        assert(remeshOutput.items.size() == 1U && remeshOutput.items[0]->matrix[3][0] == 99);
        assert(samePoints(source->points, before));
        fixture();
        remeshInput.items[0]->faces = {4, 4, 4};
        remeshInput.items[0]->connections = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
        assert(!compute(mode) && published == 0 && cleaned == 0);
        fixture();
        const auto item = remeshInput.items[0];
        item->points.clear();
        item->connections.clear();
        for (int i = 0; i < 32; ++i) {
            item->points.push_back(MPoint(double(i), 0, 0));
            item->connections.push_back(i);
        }
        item->points.push_back(MPoint(0, 1, 0));
        item->connections.push_back(32);
        item->faces = {33};
        assert(!compute(mode) && published == 0 && cleaned == 0);
        fixture();
        remeshInput.items[0]->points = {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}};
        assert(!compute(mode) && published == 0 && cleaned == 0);
        fixture();
        assert(compute(mode) && published == 1);
    }
    fixture();
    inputs["Iterate"].number = 2;
    assert(!compute(1) && published == 0 && cleaned == 0);
    std::cout << "Polygon work is shared across faces/meshes; exhaustion never publishes partial output\n";
}

// Only the WorkBudget default changes in the disposable test header.
static void aggregateIsotropicWorkBudget() {
    assert(dommy::generation::WorkBudget().remaining() == 4096U);
    fixture();
    assert(compute(0) && published == 1);
    fixture();
    const auto source = remeshInput.items[0];
    const auto before = source->points;
    remeshInput.items.assign(8U, source);
    remeshOutput.items.push_back(std::make_shared<RemeshItem>());
    remeshOutput.items[0]->matrix[3][0] = 99;
    assert(!compute(0) && published == 0 && cleaned == 0);
    assert(remeshOutput.items.size() == 1U && remeshOutput.items[0]->matrix[3][0] == 99);
    assert(samePoints(source->points, before));
    fixture();
    inputs["Iterate"].number = 20;
    assert(!compute(0) && published == 0 && cleaned == 0);
    fixture();
    remeshInput.items[0]->points.resize(5000U);
    assert(!compute(0) && published == 0 && cleaned == 0);
    assert(std::find(operations.begin(), operations.end(), "mesh.convertUV") == operations.end());
    fixture();
    assert(compute(0) && published == 1);
    std::cout << "Isotropic work stays shared across meshes/iterations; exhaustion preserves cached output and retry\n";
}

static void transforms() {
    MMatrix translation, rotation, scale, shear;
    translation[3][0] = 4;
    translation[3][1] = -3;
    translation[3][2] = 7;
    rotation[0][0] = 0;
    rotation[0][1] = 1;
    rotation[1][0] = -1;
    rotation[1][1] = 0;
    scale[0][0] = 2;
    scale[1][1] = 3;
    scale[2][2] = .5;
    shear[0][1] = .4;
    shear[1][2] = -.2;
    for (int mode = 0; mode < 3; ++mode) {
        fixture();
        assert(compute(mode));
        const DPointList local = remeshOutput.items.at(0)->points;
        assert(!local.empty());
        fixture();
        remeshInput.items.clear();
        for (const MMatrix& matrix : {MMatrix(), translation, rotation, scale, shear}) {
            auto item = std::make_shared<RemeshItem>();
            item->matrix = matrix;
            remeshInput.items.push_back(item);
        }
        const auto original = remeshInput.items.front()->points;
        assert(compute(mode));
        assert(remeshOutput.items.size() == 5 && published == 1 && cleaned == 2);
        for (std::size_t i = 0; i < 5; ++i) {
            assert(sameMatrix(remeshInput.items[i]->matrix, remeshOutput.items[i]->matrix));
            assert(samePoints(local, remeshOutput.items[i]->points));
            assert(samePoints(original, remeshInput.items[i]->points));
            assert(remeshOutput.items[i] != remeshInput.items[i]);
        }
        for (double invalid : {NAN, INFINITY}) {
            fixture();
            remeshInput.items[0]->matrix[3][0] = invalid;
            assert(!compute(mode) && published == 0 && cleaned == 0);
        }
        fixture();
        remeshInput.items[0]->matrix[0][3] = 1;
        assert(!compute(mode) && published == 0);
    }
    std::cout << "Remesh/debris preserve independent primitive transforms and local geometry\n";
}

static void transactionalMeshBatches() {
    for (int mode : {0,1,2}) {
        fixture();
        inputs["MaxRange"].number = 2;
        assert(compute(mode));
        const auto local = remeshOutput.items[0]->points;
        for (std::size_t count : {100U,200U,1000U}) {
            fixture();
            inputs["MaxRange"].number = 2;
            remeshInput.items.clear();
            for (std::size_t i = 0U; i < count; ++i) {
                auto item = std::make_shared<RemeshItem>();
                item->matrix[3][0] = double(i);
                remeshInput.items.push_back(item);
            }
            assert(compute(mode) && published == 1 && remeshOutput.items.size() == count);
            assert(std::count(operations.begin(), operations.end(), "mesh.batch.insert") == 1);
            assert(std::count(operations.begin(), operations.end(), "mesh.insert") == 0);
            assert(std::count(operations.begin(), operations.end(), "mesh.bounds") == 1);
            for (std::size_t i = 0U; i < count; ++i) {
                assert(sameMatrix(remeshInput.items[i]->matrix, remeshOutput.items[i]->matrix));
                assert(samePoints(local, remeshOutput.items[i]->points));
                assert(remeshInput.items[i] != remeshOutput.items[i]);
            }
        }
        fixture();
        remeshInput.items.push_back(std::make_shared<RemeshItem>());
        assert(compute(mode));
        const auto previous = remeshOutput.items;
        const auto trace = operations;
        for (const std::string& boundary : {std::string("mesh.batch.insert"), std::string("mesh.bounds")}) {
            const auto position = std::find(trace.begin(), trace.end(), boundary);
            assert(position != trace.end());
            for (bool throws : {false,true}) {
                fixture();
                remeshInput.items.push_back(std::make_shared<RemeshItem>());
                remeshOutput.items = previous;
                if (throws) throwAt = int(position - trace.begin());
                else failAt = int(position - trace.begin());
                assert(!compute(mode) && published == 0 && cleaned == 0);
                assert(remeshOutput.items == previous);
                fixture();
                assert(compute(mode) && published == 1);
            }
        }
        fixture();
        auto invalid = std::make_shared<RemeshItem>();
        invalid->matrix[3][0] = NAN;
        remeshInput.items.push_back(invalid);
        remeshOutput.items = previous;
        assert(!compute(mode) && published == 0 && cleaned == 0 && remeshOutput.items == previous);
        assert(std::count(operations.begin(), operations.end(), "mesh.batch.insert") == 0);
        fixture();
        remeshInput.items.clear();
        assert(compute(mode) && published == 1 && remeshOutput.items.empty());
        assert(std::count(operations.begin(), operations.end(), "mesh.bounds") == 1);
    }
    std::cout << "Remesh/debris use one mesh/bounds transaction for 100/200/1000 primitives; late failures preserve cached output\n";
}

static void failures() {
    int failures = 0;
    for (int mode = 0; mode < 4; ++mode) {
        fixture();
        assert(compute(mode));
        const auto trace = operations;
        const auto publication = std::find(trace.begin(), trace.end(), "publish") - trace.begin();
        assert(publication < static_cast<std::ptrdiff_t>(trace.size()));
        if (mode == 3)
            assert(outputData.numberOfCurves() == 1);
        for (std::size_t i = 0; i < trace.size(); ++i) {
            if(mode!=3&&(trace[i]=="mesh.input"||trace[i]=="plugin.pointer")) {
                for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                    fixture();failAt=i;injectedStatus=code;
                    assert(compute(mode)==code&&published==0&&cleaned==0&&callCount==int(i)+1);
                    fixture();assert(compute(mode)&&published==1);
                }
            }
            for (int kind = 0; kind < 2; ++kind) {
                fixture();
                if (kind == 0)
                    failAt = i;
                else
                    throwAt = i;
                assert(!compute(mode));
                if (static_cast<std::ptrdiff_t>(i) <= publication)
                    assert(published == 0 && cleaned == 0);
                assert(callCount == static_cast<int>(i) + 1);
                assert(samePoints(remeshInput.items[0]->points, RemeshItem().points));
                ++failures;
            }
            if (mode == 3 && trace[i] == "array.resize" &&
                std::find(trace.begin(), trace.end(), "array.resize") == trace.begin() + i) {
                fixture();
                failAt = i;
                injectedStatus = MS::kInvalidParameter;
                assert(compute(mode) == MS::kInvalidParameter && published == 0 &&
                       callCount == int(i) + 1);
            }
            if ((mode == 0 || mode == 3) &&
                (trace[i] == "mesh.points" || trace[i] == "mesh.triangles" ||
                 trace[i] == "array.resize")) {
                fixture();
                shortAt = i;
                assert(!compute(mode) && published == 0 && cleaned == 0 && shortApplied);
            }
        }
        fixture();
        remeshInput.items.clear();
        assert(compute(mode) && published == 1);
        fixture();
        remeshInput.valid = false;
        assert(!compute(mode) && published == 0);
        fixture();
        remeshInput.items[0].reset();
        assert(!compute(mode) && published == 0);
    }
    std::cout << "Remesh/debris/slicer: " << failures << " status/exception injections passed\n";
}

template <typename T> struct ShortAppendArray : Array<T> {
    MStatus append(const T& value) {
        const int index = callCount;
        const auto s = api("array.append");
        if (s && index != shortAt)
            this->push_back(value);
        return s;
    }
};
static void appendBoundaries() {
    const std::vector<dommy::geometry::Triangle> triangles{{0, 1, 2}, {0, 2, 3}};
    const auto point = [](std::size_t i) { return dommy::geometry::Point3(double(i), 0, 0); };
    for (int i = 0; i < 12; ++i) {
        reset();
        shortAt = i;
        ShortAppendArray<MPoint> points;
        ShortAppendArray<int> faces, connections;
        assert(!dommy::maya::appendTriangleMeshChecked(4, point, triangles, points, faces,
                                                       connections));
        reset();
        failAt = i;
        assert(!dommy::maya::appendTriangleMeshChecked(4, point, triangles, points, faces,
                                                       connections));
        assert(callCount == i + 1);
    }
    reset();
    MPointArray points;
    MIntArray faces, connections;
    assert(dommy::maya::appendTriangleMeshChecked(4, point, triangles, points, faces, connections));
    assert(dommy::maya::appendTriangleMeshChecked(4, point, triangles, points, faces, connections));
    assert(points.size() == 8 && faces.size() == 4 && connections[6] == 4 && connections[8] == 6);
    assert(
        !dommy::maya::appendTriangleMeshChecked(3, point, triangles, points, faces, connections));
    assert(!dommy::maya::appendTriangleMeshChecked(
        4, [](std::size_t) { return dommy::geometry::Point3(NAN, 0, 0); }, triangles, points, faces,
        connections));
    assert(!dommy::maya::appendTriangleMeshChecked(std::numeric_limits<std::size_t>::max(), point,
                                                   triangles, points, faces, connections));
    const std::vector<dommy::geometry::Triangle> one{{0,1,2}};
    const std::size_t maximum = dommy::mesh::kMaxMeshOperationBytes;
    reset();
    points.clear(); faces.clear(); connections.clear();
    reset(); // Clearing the host-double arrays is itself an API operation.
    unsigned reads = 0U;
    const auto countedPoint = [&](std::size_t i) { ++reads; return point(i); };
    assert(!dommy::maya::appendTriangleMeshChecked(3U, countedPoint, one, points, faces,
                                                   connections, maximum - 640U + 1U));
    assert(reads == 0U && callCount == 0 && points.empty() && faces.empty() && connections.empty());
    assert(dommy::maya::appendTriangleMeshChecked(3U, countedPoint, one, points, faces,
                                                  connections, maximum - 640U));
    assert(reads == 3U && points.size() == 3U && faces.size() == 1U && connections.size() == 3U);
}

// Complete production bodies with only the byte policy reduced in a temporary
// header. Exercise aggregate retained source/output without large allocations.
static void faceStorageAdmission() {
    assert(dommy::mesh::kMaxMeshOperationBytes == 8192U);
    for (int mode : {1,2}) {
        fixture();
        assert(compute(mode) && published == 1);
        const auto previous = remeshOutput.items[0];
        for (int scenario : {0,1,2}) {
            fixture();
            if (scenario == 0) {
                // Native output arrays must not be appended if refinement's
                // active workspace plus its copies exceeds the byte budget.
                const float extent = 16.0f;
                remeshInput.items[0]->points = {{0,0,0},{extent,0,0},
                                              {extent,extent,0},{0,extent,0}};
            } else if (scenario == 1) {
                auto& source = *remeshInput.items[0];
                source.faces.clear(); source.connections.clear();
                for (int i = 0; i < 10; ++i) {
                    source.faces.push_back(4);
                    for (int c : {0,1,2,3}) source.connections.push_back(c);
                }
            } else {
                const auto source = remeshInput.items[0];
                remeshInput.items.assign(4, source);
            }
            const auto points = remeshInput.items[0]->points;
            remeshOutput.items = {previous};
            assert(!compute(mode) && published == 0 && cleaned == 0);
            assert(remeshOutput.items.size() == 1U && remeshOutput.items[0] == previous);
            assert(samePoints(points, remeshInput.items[0]->points));
            if (scenario == 0)
                assert(std::find(operations.begin(), operations.end(), "array.append") == operations.end());
            fixture();
            assert(compute(mode) && published == 1);
        }
    }
    std::cout << "Face nodes reject refinement/aggregate byte excess before publication and permit retry\n";
}

static void sliceBudgets() {
    using namespace dommy;
    mesh::SliceBudget budget;
    assert(!mesh::appendSliceBudget(4, 1, 1000, 1000000, budget));
    assert(budget.workItems == 0 && budget.storage.meshes == 0);
    assert(mesh::appendSliceBudget(4, 1, 50, 1000000, budget));
    const auto work = budget.workItems;
    assert(!mesh::appendSliceBudget(4, 1, 50, 1000000, budget) && budget.workItems == work);
    assert(!mesh::appendSliceBudget(std::numeric_limits<std::size_t>::max(), 1, 2, 1, budget));
    assert(!mesh::appendSliceBudget(4, 1, 2, 0, budget));

    fixture();
    inputs["CutCount"].number = 1000000;
    reportedTriangles = 1000;
    assert(!compute(3) && published == 0);
    assert(std::find(operations.begin(), operations.end(), "mesh.points") == operations.end());
    fixture();
    inputs["CutCount"].number = 1000000;
    reportedTriangles = 50;
    remeshInput.items.push_back(std::make_shared<RemeshItem>());
    assert(!compute(3) && published == 0);
    assert(std::find(operations.begin(), operations.end(), "mesh.points") == operations.end());
    fixture();
    remeshInput.items[0]->reportedPoints = generation::kMaxGeneratedPointCount;
    assert(!compute(3) && published == 0);
    assert(std::find(operations.begin(), operations.end(), "mesh.convertUV") == operations.end());
    fixture();
    inputs["PlanePosition"].vec[1] = 100;
    inputs["CutCount"].number = 3;
    assert(compute(3) && outputData.numberOfCurves() == 0 && published == 1);
    fixture();
    inputs["PlaneNormal"].vec[1] = 0;
    assert(!compute(3) && published == 0);
    fixture();
    badNativeCounts = true;
    assert(!compute(3) && published == 0);
    fixture();
    remeshInput.items[0]->points[0].x = NAN;
    assert(!compute(3) && published == 0);
    fixture();
    remeshInput.items.push_back(std::make_shared<RemeshItem>());
    remeshInput.items[1]->matrix[3][0] = 100;
    inputs["Step"].number = -.02;
    inputs["CutCount"].number = 3;
    assert(compute(3) && outputData.numberOfCurves() == 6);
    assert(samePoints(outputData.curves[0]->points, outputData.curves[3]->points));
    std::cout << "Slice preflight rejects aggregate work/storage before bulk reads, including "
                 "off-mesh cuts\n";
}

static void remeshReadBoundaries() {
    for (int mode : {0, 3}) {
        for (int fault = 1; fault <= 6; ++fault) {
            fixture();
            triangleReadFault = fault;
            assert(!compute(mode) && published == 0 && cleaned == 0);
        }
        fixture();
        reportedTriangles = int(dommy::generation::kMaxGeneratedFaceCount + 1U);
        assert(!compute(mode) && published == 0);
        assert(std::find(operations.begin(), operations.end(), "mesh.points") == operations.end());
        fixture();
        oversizedNativePoints = true;
        assert(!compute(mode) && published == 0);
        assert(std::find(operations.begin(), operations.end(), "mesh.points") == operations.end());
    }
    std::cout << "Remesh/slicer reject incomplete triangles, mismatched counts and invalid IDs "
                 "before publication\n";
}

static void meshCollapseRegression() {
    fixture();
    RemeshItem& mesh = *remeshInput.items[0];
    mesh.points = {};
    mesh.faces = {};
    mesh.connections = {};
    const double coordinates[] = {0.0, 0.9, 1.0, 2.0};
    for (unsigned int y = 0U; y < 4U; ++y)
        for (unsigned int x = 0U; x < 4U; ++x)
            mesh.points.push_back(MPoint(coordinates[x], coordinates[y], 0.0));
    for (unsigned int y = 0U; y < 3U; ++y)
        for (unsigned int x = 0U; x < 3U; ++x) {
            const unsigned int a = y * 4U + x;
            mesh.faces.push_back(4);
            for (unsigned int vertex : {a, a + 1U, a + 5U, a + 4U})
                mesh.connections.push_back(int(vertex));
        }
    const DPointList original = mesh.points;
    inputs["MaxRange"].number = .2;
    assert(compute(0) && published == 1);
    assert(remeshOutput.items.at(0)->faces.size() == 50U);
    assert(samePoints(mesh.points, original));
    std::cout
        << "DMRemesh evaluates the formerly failing short-edge grid without changing its input\n";
}

static void simultaneousSmoothingRegression() {
    const auto input = simultaneousSmoothingFixture();
    fixture();
    auto& source = *remeshInput.items[0];
    source.points = {};
    source.faces = {};
    source.connections = {};
    for (const auto& point : input.points)
        source.points.push_back(MPoint(point.x, point.y, point.z));
    for (const auto& triangle : input.triangles) {
        source.faces.push_back(3);
        for (std::size_t corner = 0U; corner < 3U; ++corner)
            source.connections.push_back(int(triangle[corner]));
    }
    const auto original = source.points;
    inputs["MaxRange"].number = 0.5;
    assert(compute(0) && published == 1);
    const auto& output = *remeshOutput.items.at(0);
    assert(output.faces.size() == 108U);
    for (std::size_t i = 0U; i < output.connections.size(); i += 3U) {
        const MPoint& a = output.points[output.connections[i]];
        const MPoint& b = output.points[output.connections[i + 1U]];
        const MPoint& c = output.points[output.connections[i + 2U]];
        assert((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0.0);
    }
    assert(samePoints(source.points, original));
    std::cout << "DMRemesh validates simultaneous smoothing without reversing any face\n";
}

static void closedMeshRegression() {
    for (double target : {1.0, 2.0, 3.0, 4.0, 5.0}) {
        fixture();
        auto& source = *remeshInput.items[0];
        source.points = {{1, 2, 0}, {-1, -1, -1}, {2, 0, 2}, {0, 2, -2}};
        source.faces = {3, 3, 3, 3};
        source.connections = {0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3};
        const auto original = source.points;
        inputs["MaxRange"].number = target;
        assert(compute(0) && published == 1);
        const auto& output = *remeshOutput.items.at(0);
        std::vector<dommy::geometry::Triangle> triangles;
        for (unsigned int i = 0; i < output.connections.size(); i += 3U)
            triangles.push_back({std::size_t(output.connections[i]),
                                 std::size_t(output.connections[i + 1U]),
                                 std::size_t(output.connections[i + 2U])});
        std::map<dommy::geometry::remesh_detail::EdgeKey, unsigned int> edges;
        assert(dommy::geometry::remesh_detail::buildEdgeCounts(triangles, edges));
        for (const auto& edge : edges) assert(edge.second == 2U);
        assert(output.points.size() + output.faces.size() == edges.size() + 2U);
        assert(samePoints(source.points, original));
    }
    std::cout << "DMRemesh preserves closed tetrahedron topology across five target lengths\n";
}

static void disconnectedComponentNodes() {
    const auto geometry = separatedTrianglesFixture(double(1e12f));
    for (int mode : {0, 3}) {
        for (bool combined : {false, true}) {
            fixture();
            remeshInput.items.clear();
            for (std::size_t item = 0U; item < (combined ? 1U : 2U); ++item) {
                auto source = std::make_shared<RemeshItem>();
                source->points.clear();
                source->faces.clear();
                source->connections.clear();
                const std::size_t begin = combined ? 0U : 3U * item;
                const std::size_t count = combined ? 6U : 3U;
                for (std::size_t i = begin; i < begin + count; ++i)
                    source->points.push_back(MPoint(geometry.points[i].x, geometry.points[i].y,
                                                   geometry.points[i].z));
                for (std::size_t i = 0U; i < count; i += 3U) {
                    source->faces.push_back(3);
                    source->connections.insert(source->connections.end(), {int(i), int(i+1U), int(i+2U)});
                }
                remeshInput.items.push_back(source);
            }
            inputs["MaxRange"].number = 2.0;
            inputs["PlanePosition"].vec[1] = .5;
            assert(compute(mode) && published == 1);
            if (mode == 0) {
                assert(remeshOutput.numberOfMesh() == (combined ? 1U : 2U));
                std::size_t faces = 0U;
                for (const auto& mesh : remeshOutput.items) faces += mesh->faces.size();
                assert(faces == 2U);
            } else {
                assert(outputData.numberOfCurves() == 2U);
                for (std::size_t i = 0U; i < 2U; ++i)
                    for (const auto& point : outputData.curves[i]->points)
                        assert(point.x == geometry.points[3U*i].x && point.y == .5);
            }
        }
    }
    std::cout << "Complete remesh/slicer nodes retain disconnected geometry in combined or separate primitives\n";
}

static void enumAndStorageAdmission() {
    fixture();
    assert(compute(3));
    const auto previous = outputData;
    for (const std::string& field : {std::string("Degree"), std::string("IsOpen")}) {
        for (int value : {-32768, -1, 2, 32767}) {
            fixture();
            inputs[field].number = value;
            outputData = previous;
            assert(compute(3) == MS::kInvalidParameter && published == 0 && cleaned == 0);
            assert(outputData.numberOfCurves() == previous.numberOfCurves());
            assert(samePoints(outputData.curves[0]->points, previous.curves[0]->points));
            fixture();
            assert(compute(3) && published == 1);
        }
    }
    for (int mode : {0, 1, 2}) {
        fixture();
        const auto previousMesh = std::make_shared<RemeshItem>();
        remeshOutput.items = {previousMesh};
        remeshInput.items[0]->reportedPoints = 9000000U;
        assert(compute(mode) == MS::kInvalidParameter && published == 0 && cleaned == 0);
        assert(remeshOutput.items.size() == 1U && remeshOutput.items[0] == previousMesh);
        assert(std::find(operations.begin(), operations.end(), "mesh.convertUV") == operations.end());
        assert(std::find(operations.begin(), operations.end(), "plugin.create") == operations.end());
        fixture();
        assert(compute(mode) && published == 1);
    }
    std::cout << "Slicer enum and remesh byte admission reject invalid input before publication and permit retry\n";
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--face-storage") {
        assert(DMRemesh::initialize() && DMDebrisGen::initialize());
        faceStorageAdmission();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--isotropic-work-budget") {
        assert(DMRemesh::initialize());
        aggregateIsotropicWorkBudget();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--slice-work-budget") {
        assert(DMSlicer::initialize());
        aggregateSliceWorkBudget();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--polygon-budget") {
        assert(DMRemesh::initialize() && DMDebrisGen::initialize());
        aggregatePolygonBudget();
        return 0;
    }
    assert(argc == 2);
    const auto trace = schemaTrace("DMSlicer", DMSlicer::initialize) +
                       schemaTrace("DMVectorDeform", DMVectorDeform::initialize);
    std::ifstream file(argv[1]);
    assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    const int count = testFailures("DMSlicer", DMSlicer::initialize, 56) +
                      testFailures("DMVectorDeform", DMVectorDeform::initialize, 47);
    reset();
    assert(DMRemesh::initialize());
    assert(DMDebrisGen::initialize());
    remeshReadBoundaries();
    meshCollapseRegression();
    simultaneousSmoothingRegression();
    closedMeshRegression();
    transforms();
    transactionalMeshBatches();
    smallPolygonRegression();
    simultaneousFaceSmoothingRegression();
    nonPlanarFaceRejection();
    translatedGeometry();
    unusedGeometryVertices();
    failures();
    appendBoundaries();
    sliceBudgets();
    branchingSliceBatch();
    disconnectedComponentNodes();
    enumAndStorageAdmission();
    std::cout << "Unchanged initializer schemas: " << count << " injected failures passed\n";
}
