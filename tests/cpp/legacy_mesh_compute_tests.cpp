#include "legacy_mesh_doubles.h"
#include "legacy_mesh_compute.inc"
#include "maya_point_blend_cases.h"

namespace {
template<class Node> MStatus evaluate(Node& node, MDataBlock& block, const MObject& output) {
    return node.compute(MPlug({"node"}, output), block);
}
const MPointArray& result(const MObject& output) { return meshes.at(outputs.at(output.name).name).points; }
void near(double a, double b) { assert(std::abs(a-b) < 1e-10); }

unsigned meshBlendValues = 0;
void blendOracle(const MPoint& actual, MPoint source, MPoint target, double weight,
                 MSpace::Space space = MSpace::kObject) {
    // This oracle deliberately does not call the production interpolation or
    // the fake MPoint operators. World-space translation is the fixture only.
    if (space == MSpace::kWorld) { source.y += 10; target.y += 10; }
    const double first[] = {source.x/source.w, source.y/source.w, source.z/source.w};
    const double second[] = {target.x/target.w, target.y/target.w, target.z/target.w};
    MPoint expected;
    double* axes[] = {&expected.x, &expected.y, &expected.z};
    for (unsigned axis = 0; axis < 3; ++axis) {
        *axes[axis] = weight == 0 ? first[axis] : weight == 1 ? second[axis] :
            first[axis] + (second[axis]-first[axis])*weight;
        assert(std::isfinite(*axes[axis]));
    }
    if (space == MSpace::kWorld) expected.y -= 10;
    assert(samePointBits(actual, expected));
    ++meshBlendValues;
}

void meshBlendCases() {
    DBSMirrorMesh mirror; DBSUpDownMesh updown; DBSFlipMesh flip; DPolySymmetry symmetry;
    const double maximum = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    const std::pair<double, double> endpoints[] = {
        {1e20, 1}, {1e20, -1}, {-1e20, 1}, {-1e20, -1},
        {maximum, 1}, {-maximum, -1}, {1e20, tiny}, {-1e20, -tiny}
    };
    const auto setup = [](double source, double target) {
        auto block = resetMeshRun();
        block.controls["aDistance"] = 4; // Both mirror paths visit all vertices.
        for (auto& point : meshes["base"].points) { point.y = source; point.z = -source; }
        for (auto& point : meshes["target"].points) { point.y = target; point.z = -target; }
        return block;
    };
    for (const auto& pair : endpoints) for (double weight : {0.0, 1.0}) {
        auto block = setup(pair.first, pair.second);
        constantRamp = true; rampValue = float(weight);
        assert(evaluate(mirror, block, DBSMirrorMesh::outLeftMesh));
        for (unsigned i = 0; i < 5; ++i) {
            blendOracle(result(DBSMirrorMesh::outLeftMesh)[i], meshes["base"].points[i],
                        meshes["target"].points[i], weight);
            blendOracle(result(DBSMirrorMesh::outRightMesh)[i], meshes["base"].points[i],
                        meshes["target"].points[i], weight);
        }
        for (int space = 0; space < 2; ++space) for (int invert = 0; invert < 2; ++invert) {
            block = setup(pair.first, pair.second);
            block.controls["aSpace"] = space; block.controls["aInvert"] = invert;
            for (auto& color : meshes["target"].colors) color.r = float(weight);
            assert(evaluate(updown, block, DBSUpDownMesh::outDownMesh));
            for (unsigned i = 0; i < 5; ++i) {
                const double upWeight = invert ? 1-weight : weight;
                blendOracle(result(DBSUpDownMesh::outUpMesh)[i], meshes["base"].points[i],
                            meshes["target"].points[i], upWeight, MSpace::Space(space));
                blendOracle(result(DBSUpDownMesh::outDownMesh)[i], meshes["base"].points[i],
                            meshes["target"].points[i], 1-upWeight, MSpace::Space(space));
            }
        }
        for (int invert = 0; invert < 2; ++invert) {
            block = setup(pair.first, pair.second);
            block.controls["aInvert"] = invert;
            for (auto& color : meshes["mask"].colors) color.r = float(weight);
            assert(evaluate(flip, block, DBSFlipMesh::outMesh));
            for (unsigned i = 0; i < 5; ++i) {
                const auto& matched = meshes["target"].points[4-i];
                blendOracle(result(DBSFlipMesh::outMesh)[i], meshes["base"].points[i],
                            MPoint(-matched.x, matched.y, matched.z), invert ? weight : 1-weight);
            }
        }
        for (int type = 0; type < 2; ++type) {
            block = setup(pair.first, pair.second); block.controls["aType"] = type;
            for (auto& color : meshes["target"].colors) color.r = float(weight);
            assert(evaluate(symmetry, block, DPolySymmetry::outMesh));
            for (unsigned i = 0; i < 5; ++i) {
                const auto& source = meshes["base"].points[i];
                const auto& matched = meshes["target"].points[4-i];
                const bool selected = type == 0 ? source.x >= 0 : source.x <= 0;
                blendOracle(result(DPolySymmetry::outMesh)[i], source,
                            MPoint(-matched.x, matched.y, matched.z), selected ? weight : 0);
            }
        }
    }

    // Exact ordinary double rounding, including unclamped Ramp/color
    // extrapolation and the existing clamped Flip/Symmetry masks.
    for (float weight : {-0.5f, 0.3f, 0.625f, 1.5f}) {
        auto block = setup(1.23456789012345, -2.34567890123456);
        constantRamp = true; rampValue = weight;
        assert(evaluate(mirror, block, DBSMirrorMesh::outRightMesh));
        for (unsigned i = 0; i < 5; ++i) for (auto output :
                {DBSMirrorMesh::outLeftMesh, DBSMirrorMesh::outRightMesh})
            blendOracle(result(output)[i], meshes["base"].points[i], meshes["target"].points[i], weight);
        for (int invert = 0; invert < 2; ++invert) {
            block = setup(1.23456789012345, -2.34567890123456);
            block.controls["aInvert"] = invert;
            for (auto& color : meshes["target"].colors) color.r = weight;
            assert(evaluate(updown, block, DBSUpDownMesh::outUpMesh));
            for (unsigned i = 0; i < 5; ++i) {
                const double upWeight = invert ? 1.0-double(weight) : double(weight);
                blendOracle(result(DBSUpDownMesh::outUpMesh)[i], meshes["base"].points[i],
                            meshes["target"].points[i], upWeight);
                blendOracle(result(DBSUpDownMesh::outDownMesh)[i], meshes["base"].points[i],
                            meshes["target"].points[i], 1.0-upWeight);
            }
            block = setup(1.23456789012345, -2.34567890123456);
            block.controls["aInvert"] = invert;
            for (auto& color : meshes["mask"].colors) color.r = weight;
            const float mask = invert ? 1.0f-weight : weight;
            const double blend = 1.0-std::max(0.0, std::min(1.0, double(mask)));
            assert(evaluate(flip, block, DBSFlipMesh::outMesh));
            for (unsigned i = 0; i < 5; ++i) {
                const auto& matched = meshes["target"].points[4-i];
                blendOracle(result(DBSFlipMesh::outMesh)[i], meshes["base"].points[i],
                            MPoint(-matched.x, matched.y, matched.z), blend);
            }
        }
        block = setup(1.23456789012345, -2.34567890123456);
        for (auto& color : meshes["target"].colors) color.r = weight;
        assert(evaluate(symmetry, block, DPolySymmetry::outMesh));
        for (unsigned i = 0; i < 5; ++i) {
            const auto& source = meshes["base"].points[i];
            const auto& matched = meshes["target"].points[4-i];
            blendOracle(result(DPolySymmetry::outMesh)[i], source,
                        MPoint(-matched.x, matched.y, matched.z), source.x >= 0 ?
                        std::max(0.0, std::min(1.0, double(weight))) : 0);
        }
    }

    // The mathematically finite midpoint must survive displacement overflow.
    auto block = setup(-maximum, maximum);
    constantRamp = true; rampValue = 0.5f;
    assert(evaluate(mirror, block, DBSMirrorMesh::outLeftMesh));
    for (auto output : {DBSMirrorMesh::outLeftMesh, DBSMirrorMesh::outRightMesh})
        for (const auto& point : result(output)) assert(point.y == 0 && point.z == 0);
    block = setup(-maximum, maximum);
    for (auto& color : meshes["target"].colors) color.r = 0.5f;
    assert(evaluate(updown, block, DBSUpDownMesh::outUpMesh));
    for (auto output : {DBSUpDownMesh::outUpMesh, DBSUpDownMesh::outDownMesh})
        for (const auto& point : result(output)) assert(point.y == 0 && point.z == 0);
    block = setup(-maximum, maximum);
    for (auto& color : meshes["mask"].colors) color.r = 0.5f;
    assert(evaluate(flip, block, DBSFlipMesh::outMesh));
    for (const auto& point : result(DBSFlipMesh::outMesh)) assert(point.y == 0 && point.z == 0);
    block = setup(-maximum, maximum);
    for (auto& color : meshes["target"].colors) color.r = 0.5f;
    assert(evaluate(symmetry, block, DPolySymmetry::outMesh));
    for (unsigned i = 2; i < 5; ++i) {
        const auto& point = result(DPolySymmetry::outMesh)[i];
        assert(point.y == 0 && point.z == 0);
    }
}

template<class Node> void blendFailureRetryCases(const MObject& output) {
    Node node;
    for (unsigned index : {0U, 2U, 4U}) for (double w :
            {0.0, std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::denorm_min()}) {
        auto block = resetMeshRun();
        constantRamp = true; rampValue = 0.5f;
        block.controls["aUseMask"] = block.controls["aUseColor"] = 0;
        for (auto& point : meshes["base"].points) point = MPoint();
        for (auto& point : meshes["target"].points) point = MPoint(0, 2, 0);
        for (auto& color : meshes["target"].colors) color.r = 0.5f;
        assert(evaluate(node, block, output));
        const auto retainedOutputs = outputs;
        std::map<std::string, MPointArray> retainedPoints;
        for (const auto& entry : retainedOutputs) retainedPoints[entry.first] = meshes.at(entry.second.name).points;
        const auto validPoint = meshes["base"].points[index];
        meshes["base"].points[index] = MPoint(0, 0, 1);
        meshes["base"].points[index].w = w;
        const auto base = meshes["base"].points, target = meshes["target"].points;
        resetCalls(); published = cleaned = 0; outputEvents.clear();
        assert(evaluate(node, block, output) == MS::kInvalidParameter);
        assert(outputs.size() == retainedOutputs.size());
        for (const auto& entry : retainedOutputs) {
            assert(outputs.at(entry.first).name == entry.second.name);
            assert(meshes.at(entry.second.name).points == retainedPoints.at(entry.first));
        }
        assert(meshes["base"].points == base && meshes["target"].points == target);
        assert(published == 0 && cleaned == 0 && outputEvents.empty());
        meshes["base"].points[index] = validPoint;
        resetCalls();
        assert(evaluate(node, block, output));
        assert(result(output) == retainedPoints.at(output.name) && published > 0 && cleaned > 0);
    }
}

template<class Node> void failureCases(const char* name, const MObject& output, int variant = 0) {
    Node node;
    auto setup = [&]() {
        auto block = resetMeshRun();
        if (variant == 1) block.controls["aSpace"] = 1;
        if (variant == 2) { block.controls["aUseColor"] = 0; block.controls["aUseMask"] = 0; }
        if (variant == 3) { block.controls["aInvert"] = 1; block.controls["aType"] = 1; }
        if (variant == 4) existingColorChannels = true;
        return block;
    };
    auto block = setup();
    assert(evaluate(node, block, output));
    const auto baseline = operations;
    const int count = callCount;
    for (MS::MStatusCode code : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = code;
        for (int at = 0; at < count; ++at) {
            block = setup(); failAt = at;
            const auto base = meshes.at("base").points, target = meshes.at("target").points;
            assert(evaluate(node, block, output) == code);
            assert(callCount == at+1);
            assert(meshes.at("base").points == base && meshes.at("target").points == target);
            if (baseline[at].find("output.set.") != 0 && baseline[at] != "data.clean")
                assert(published == 0 && cleaned == 0);
        }
    }
    for (int at = 0; at < count; ++at) {
        block = setup(); throwAt = at;
        assert(evaluate(node, block, output) == MS::kFailure && callCount == at+1);
    }
    for (int at = 0; at < count; ++at) {
        if (baseline[at] != "mesh.points" && baseline[at] != "mesh.colors") continue;
        block = setup(); shortAt = at;
        assert(!evaluate(node, block, output) && published == 0);
    }
    block = setup();
    assert(evaluate(node, block, {"unrelated"}) == MS::kUnknownParameter && callCount == 0);
    block = setup(); meshes["base"].reportedPoints = meshes["target"].reportedPoints = 10000000;
    assert(evaluate(node, block, output) == MS::kInvalidParameter);
    assert(std::find(operations.begin(), operations.end(), "mesh.points") == operations.end());
    assert(std::find(operations.begin(), operations.end(), "mesh.copy") == operations.end());
    block = setup(); meshes["target"].points.pop_back();
    assert(!evaluate(node, block, output) && published == 0);
    block = setup(); meshes["base"].reportedPoints = -2;
    assert(!evaluate(node, block, output) && published == 0);
    block = setup(); meshes["target"].points[0].x = NAN;
    assert(!evaluate(node, block, output) && published == 0);
    block = setup(); nullCreated = true;
    assert(!evaluate(node, block, output) && published == 0);
    block = setup(); nullCopied = true;
    assert(!evaluate(node, block, output) && published == 0);
    block = setup(); disconnected = true;
    assert(!evaluate(node, block, output) && published == 0);
    std::cout << name << " variant " << variant << ": " << count*2
              << " status failures, " << count << " exceptions passed\n";
}

template<class Node> void iteratorCases(const MObject& output) {
    Node node;
    for (int which = 0; which < 3; ++which) {
        auto block = resetMeshRun();
        badIndex = which == 0; stuckIterator = which == 1; earlyIterator = which == 2;
        assert(!evaluate(node, block, output) && published == 0 && cleaned == 0);
        assert(callCount < 200);
    }
}

void geometryCases() {
    DBSMirrorMesh mirror; DBSUpDownMesh updown; DBSFlipMesh flip;
    DPolySymmetry symmetry; DPolyResetVtx reset;
    auto block = resetMeshRun();
    assert(evaluate(mirror, block, DBSMirrorMesh::outLeftMesh));
    const auto left = result(DBSMirrorMesh::outLeftMesh), right = result(DBSMirrorMesh::outRightMesh);
    const double leftWeight[] = {0,0.625,0.75,0.875,1};
    for (unsigned i = 0; i < 5; ++i) { near(left[i].y,2*leftWeight[i]); near(right[i].y,2*leftWeight[4-i]); }
    for (unsigned i = 0; i < 5; ++i) {
        const double x = meshes["base"].points[i].x, dx = meshes["target"].points[i].x-x;
        near(left[i].x,x+dx*leftWeight[i]); near(right[i].x,x+dx*leftWeight[4-i]);
    }
    assert(outputEvents[0] == "handle.outLeftMesh" && outputEvents[1] == "handle.outRightMesh");
    block = resetMeshRun(); block.controls["aDistance"] = 0;
    assert(evaluate(mirror, block, DBSMirrorMesh::outRightMesh));
    near(result(DBSMirrorMesh::outLeftMesh)[2].y,2);
    block = resetMeshRun(); invalidRamp = true;
    assert(!evaluate(mirror, block, DBSMirrorMesh::outLeftMesh) && published == 0);
    for (int space = 0; space < 2; ++space) for (int invert = 0; invert < 2; ++invert) {
        block = resetMeshRun(); block.controls["aSpace"] = space; block.controls["aInvert"] = invert;
        assert(evaluate(updown, block, DBSUpDownMesh::outUpMesh));
        for (unsigned i = 0; i < 5; ++i) {
            const double w = invert ? 1-i*0.25 : i*0.25;
            near(result(DBSUpDownMesh::outUpMesh)[i].y,2*w);
            near(result(DBSUpDownMesh::outDownMesh)[i].y,2*(1-w));
        }
        for (auto s : positionSpaces) assert(int(s) == space);
        assert(outputEvents[0] == "handle.outUpMesh" && outputEvents[1] == "handle.outDownMesh");
    }
    for (int mask = 0; mask < 2; ++mask) for (int invert = 0; invert < 2; ++invert) {
        block = resetMeshRun(); block.controls["aUseMask"] = mask; block.controls["aInvert"] = invert;
        assert(evaluate(flip, block, DBSFlipMesh::outMesh));
        for (unsigned i = 0; i < 5; ++i) near(result(DBSFlipMesh::outMesh)[i].y,
            2*(mask ? (invert ? i*0.25 : 1-i*0.25) : 1));
    }
    for (int type = 0; type < 2; ++type) for (int color = 0; color < 2; ++color) {
        block = resetMeshRun(); block.controls["aType"] = type; block.controls["aUseColor"] = color;
        assert(evaluate(symmetry, block, DPolySymmetry::outMesh));
        for (unsigned i = 0; i < 5; ++i) near(result(DPolySymmetry::outMesh)[i].y,
            ((type == 0 && i >= 2) || (type == 1 && i <= 2)) ? 2*(color ? i*0.25 : 1) : 0);
    }
    block = resetMeshRun(); block.controls["aUseMask"] = 0;
    for (unsigned i = 0; i < 5; ++i) meshes["target"].points[i] = MPoint(i+10, i+2, i+3);
    assert(evaluate(flip, block, DBSFlipMesh::outMesh));
    for (unsigned i = 0; i < 5; ++i) {
        near(result(DBSFlipMesh::outMesh)[i].x,-double(14-i));
        near(result(DBSFlipMesh::outMesh)[i].y,6-i);
        near(result(DBSFlipMesh::outMesh)[i].z,7-i);
    }
    block = resetMeshRun(); assert(evaluate(reset, block, DPolyResetVtx::outMesh));
    for (unsigned i = 0; i < 5; ++i) near(result(DPolyResetVtx::outMesh)[i].y,i*0.25);
    block = resetMeshRun(); meshes["mask"].colors.clear();
    assert(evaluate(reset, block, DPolyResetVtx::outMesh));
    for (const auto& p : result(DPolyResetVtx::outMesh)) near(p.y,1);
    block = resetMeshRun(); block.controls["aWeight"] = -1; meshes["mask"].colors.clear();
    assert(evaluate(reset, block, DPolyResetVtx::outMesh));
    for (const auto& p : result(DPolyResetVtx::outMesh)) near(p.y,-2);
    block = resetMeshRun(); block.controls["aWeight"] = 1e308;
    meshes["target"].points = meshes["base"].points;
    for (auto& color : meshes["mask"].colors) color.r = 2.0f;
    assert(evaluate(reset, block, DPolyResetVtx::outMesh));
    assert(result(DPolyResetVtx::outMesh) == meshes["base"].points);
    block = resetMeshRun(); block.controls["aWeight"] = 1e308;
    for (auto& p : meshes["base"].points) p = MPoint();
    for (auto& p : meshes["target"].points) p = MPoint(1e-308, 0, 0);
    for (auto& color : meshes["mask"].colors) color.r = 2.0f;
    assert(evaluate(reset, block, DPolyResetVtx::outMesh));
    for (const auto& p : result(DPolyResetVtx::outMesh)) near(p.x, 2.0);
}

void numericalSearchCases() {
    using namespace dommy::mesh;
    std::vector<std::size_t> matches;
    for (double value : {1e200, 1e-200, std::numeric_limits<double>::denorm_min(),
                         std::numeric_limits<double>::min(), std::numeric_limits<double>::max()}) {
        const std::vector<Point3d> point{{value,0,0}};
        assert(findMirroredPointMatches(point,value,1,matches));
        assert(matches[0] == kNoPointMatch);
        assert(findMirroredPointMatches(point,0,1,matches));
        assert(matches[0] == kNoPointMatch);
        if (value <= std::numeric_limits<double>::max()/2) {
            assert(findMirroredPointMatches(point,value*2,1,matches));
            assert(matches[0] == 0); // Inclusive boundary, including subnormals.
        }
    }
    const double maximum = std::numeric_limits<double>::max();
    assert(findMirroredPointMatches({{maximum,0,0},{-maximum,0,0}},0,2,matches));
    assert((matches == std::vector<std::size_t>{1,0}));

    // Integer-coordinate oracle at ordinary scale, then the same geometry at
    // powers of two. The oracle does not call the production distance helper.
    std::vector<Point3d> base{{-1,0,0},{1,0,0},{1,0,0},{0,2,0},{7,4,0}};
    for (int i=0;i<40;++i) base.push_back(Point3d((i*7)%17-8,(i*11)%13-6,(i*3)%11-5));
    for (double tolerance : {0.0,1.0,4.0,16.0}) {
        std::vector<std::size_t> expected;
        for (const auto& query : base) {
            long double best=tolerance*tolerance;std::size_t id=kNoPointMatch;
            for (std::size_t j=0;j<base.size();++j) {
                const long double x=-query.x-base[j].x,y=query.y-base[j].y,z=query.z-base[j].z;
                const long double distance=x*x+y*y+z*z;
                if (distance<best || (distance==best && j<id)) {best=distance;id=j;}
            }
            expected.push_back(id);
        }
        for (int exponent : {-1060,-700,-10,0,700,1018}) {
            auto scaled=base;
            for (auto& p : scaled) {p.x=std::ldexp(p.x,exponent);p.y=std::ldexp(p.y,exponent);p.z=std::ldexp(p.z,exponent);}
            assert(findMirroredPointMatches(scaled,std::ldexp(tolerance,exponent),100,matches));
            assert(matches==expected);
        }
    }
}

void budgetCases() {
    using dommy::mesh::validMeshPairStorage;
    const std::size_t limit = 256U*1024U*1024U;
    assert(validMeshPairStorage(0));
    assert(validMeshPairStorage(limit/256));
    assert(!validMeshPairStorage(limit/256+1));
    assert(!validMeshPairStorage(limit/256,1));
    assert(validMeshPairStorage(limit/256-1,8));
    assert(!validMeshPairStorage(limit/256-1,9));
    assert(!validMeshPairStorage(std::numeric_limits<std::size_t>::max()));
    assert(!validMeshPairStorage(0,std::numeric_limits<std::size_t>::max()));

    using dommy::mesh::Point3d;
    using dommy::mesh::findMirroredPointMatches;
    std::vector<Point3d> points(100, {0,0,0});
    std::vector<std::size_t> matches{99};
    assert(!findMirroredPointMatches(points,0,1000,matches,1000));
    assert(matches == std::vector<std::size_t>{99});
    assert(findMirroredPointMatches(points,0,1000,matches,100000));
    for (auto match : matches) assert(match == 0);
    matches = {99};
    assert(!findMirroredPointMatches(points,0,1000,matches,0));
    assert(matches == std::vector<std::size_t>{99});
    // Compare exact ties and inclusive radius boundaries against a brute-force oracle.
    points = {{-1,0,0},{1,0,0},{1,0,0},{0,2,0},{7,4,0}};
    for (double tolerance : {0.0,1.0,100.0}) {
        assert(findMirroredPointMatches(points,tolerance,1000,matches));
        for (std::size_t i = 0; i < points.size(); ++i) {
            long double best = static_cast<long double>(tolerance)*tolerance;
            std::size_t id = dommy::mesh::kNoPointMatch;
            for (std::size_t j = 0; j < points.size(); ++j) {
                const long double x = -points[i].x-points[j].x, y = points[i].y-points[j].y, z = points[i].z-points[j].z;
                const long double d = x*x+y*y+z*z;
                if (d < best || (d == best && j < id)) { best = d; id = j; }
            }
            assert(matches[i] == id);
        }
    }
}
}

int main() {
    const unsigned helperCases = checkMayaPointBlendCases();
    meshBlendCases();
    blendFailureRetryCases<DBSMirrorMesh>(DBSMirrorMesh::outLeftMesh);
    blendFailureRetryCases<DBSUpDownMesh>(DBSUpDownMesh::outDownMesh);
    blendFailureRetryCases<DBSFlipMesh>(DBSFlipMesh::outMesh);
    blendFailureRetryCases<DPolySymmetry>(DPolySymmetry::outMesh);
    std::cout << helperCases << " point helper cases, " << meshBlendValues
              << " mesh endpoint/golden values, 6 overflow-midpoint paths and 36 failure/retries passed\n";
    geometryCases(); budgetCases(); numericalSearchCases();
    failureCases<DBSMirrorMesh>("Mirror",DBSMirrorMesh::outLeftMesh);
    failureCases<DBSUpDownMesh>("UpDown",DBSUpDownMesh::outDownMesh);
    failureCases<DBSUpDownMesh>("UpDown",DBSUpDownMesh::outDownMesh,1);
    failureCases<DBSFlipMesh>("Flip",DBSFlipMesh::outMesh);
    failureCases<DBSFlipMesh>("Flip",DBSFlipMesh::outMesh,2);
    failureCases<DPolySymmetry>("Symmetry",DPolySymmetry::outMesh);
    failureCases<DPolySymmetry>("Symmetry",DPolySymmetry::outMesh,3);
    failureCases<DPolyResetVtx>("Reset",DPolyResetVtx::outMesh);
    failureCases<DSkinInfo>("Skin",DSkinInfo::outMesh);
    failureCases<DSkinInfo>("Skin",DSkinInfo::outMesh,4);
    iteratorCases<DBSMirrorMesh>(DBSMirrorMesh::outLeftMesh);
    iteratorCases<DBSUpDownMesh>(DBSUpDownMesh::outUpMesh);
    iteratorCases<DBSFlipMesh>(DBSFlipMesh::outMesh);
    iteratorCases<DPolySymmetry>(DPolySymmetry::outMesh);
    std::cout << "Legacy mesh compute/search/storage tests passed\n";
}
