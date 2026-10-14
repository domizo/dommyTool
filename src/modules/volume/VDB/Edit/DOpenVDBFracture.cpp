//
//  DOpenVDBFracture.cpp
//  dommyTool
//
//  Created by yuzo on 12/15/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBFracture.h"
#include "../Common/DVDBComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginData.h"
#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/LevelSetFracture.h>
#include <set>
#include <list>


#include <openvdb/util/NullInterrupter.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/random.hpp>
#include <boost/math/constants/constants.hpp>

#include <cmath>
#include <locale>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>



//vdb

#include "DOpenVDBCommon.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBGeoUtil.h"

//dp
#include "DPData.h"
#include "DPointCommon.h"




#include "DNodeOperation.h"

#include "DMayaHeaders.h"

#include "dvdb_ids.h"


const MTypeId DOpenVDBFracture::typeId( ID_VDB_DOPENVDB_FRACTURE );

MObject DOpenVDBFracture::aVdbInput;
MObject DOpenVDBFracture::aVdbSelectedGridNames;

MObject DOpenVDBFracture::aInObject;
MObject DOpenVDBFracture::inDP;

MObject DOpenVDBFracture::aSeparateCutters;
MObject DOpenVDBFracture::aCutterOverlap;
MObject DOpenVDBFracture::aCenterCutter;
MObject DOpenVDBFracture::aRandomizeRotation;
MObject DOpenVDBFracture::aSeed;
MObject DOpenVDBFracture::aSegmentFragments;
MObject DOpenVDBFracture::aFragmentgroup;

MObject DOpenVDBFracture::aVdbOutput;



void* DOpenVDBFracture::creator()
{
    return ::dommy::maya::createObject("DOpenVDBFracture", [&]() {
        return new DOpenVDBFracture();
    });
}



namespace {
const std::size_t kMaxFractureInstanceCount = 10000U;
struct FractureSettings {
    bool overlap = false, segment = false, center = false, random = false;
    int seed = 1;
    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::math::Quats> rotations;
};
MStatus readFractureSettings(const MObject& node, MDataBlock& data, FractureSettings& settings) {
    MStatus status;
    const auto segment = data.inputValue(DOpenVDBFracture::aSegmentFragments, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.segment = segment.asBool();
    const bool connected = MPlug(node, DOpenVDBFracture::inDP).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) return MS::kSuccess;
    const auto handle = data.inputValue(DOpenVDBFracture::inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* points = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, points));
    if (!points || !points->hasValidPointTopology()) return MS::kInvalidParameter;
    const auto count = points->numberOfPoints();
    if (count > kMaxFractureInstanceCount) return MS::kInvalidParameter;
    if (count == 0) return MS::kSuccess;
    settings.points.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& p = points->positionAt(i);
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::numeric::checkedNumericCast(p[axis], settings.points[i][axis]))
                return MS::kInvalidParameter;
    }
    const auto overlap = data.inputValue(DOpenVDBFracture::aCutterOverlap, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.overlap = overlap.asBool();
    const auto center = data.inputValue(DOpenVDBFracture::aCenterCutter, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.center = center.asBool();
    const auto random = data.inputValue(DOpenVDBFracture::aRandomizeRotation, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.random = random.asBool();
    if (settings.random) {
        const auto seed = data.inputValue(DOpenVDBFracture::aSeed, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.seed = seed.asInt();
        if (settings.seed < 0) return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}
void buildFractureRotations(FractureSettings& settings) {
    if (!settings.random) return;
    settings.rotations.resize(settings.points.size());
    typedef boost::mt19937 RandGen;
    RandGen rng(static_cast<RandGen::result_type>(settings.seed));
    boost::uniform_01<RandGen, float> uniform01(rng);
    const float two_pi = 2.0f * boost::math::constants::pi<float>();
    for (auto& rotation : settings.rotations) {
        // Preserve the historical seeded Shoemake sequence.
        const float u = uniform01(), c1 = std::sqrt(1.0f - u), c2 = std::sqrt(u);
        const float s1 = two_pi * uniform01(), s2 = two_pi * uniform01();
        rotation.init(c1 * std::sin(s1), c1 * std::cos(s1),
                      c2 * std::sin(s2), c2 * std::cos(s2));
    }
}
bool fractureBox(const openvdb::BBoxd& box, openvdb::CoordBBox& result) {
    if (!dommy::vdb::indexGridBox(box, result, true)) return false;
    for (int axis = 0; axis < 3; ++axis)
        if (std::fabs(box.min()[axis]) > 1e6 || std::fabs(box.max()[axis]) > 1e6) return false;
    return true;
}
struct FractureMesh {
    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec4I> primitives;
    openvdb::BBoxd bounds;
};
MStatus readFractureMesh(MDataBlock& data, const openvdb::math::Transform& transform,
                        bool center, FractureMesh& mesh) {
    MObject object;
    MBoundingBox meshBBox;
    DOMMY_RETURN_IF_FAILURE(getMeshInformation(DOpenVDBFracture::aInObject, data, meshBBox, object));
    if (object.isNull()) return MS::kInvalidParameter;
    MStatus status;
    MFnMesh meshFn(object, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawVertices = meshFn.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawPolygons = meshFn.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t vertices = 0, polygons = 0;
    if (!dommy::generation::checkedCount(rawVertices, dommy::generation::kMaxGeneratedPointCount, vertices) ||
        !dommy::generation::checkedCount(rawPolygons, dommy::generation::kMaxGeneratedFaceCount, polygons) ||
        vertices == 0 || polygons == 0) return MS::kInvalidParameter;
    MPointArray vertexArray;
    DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(vertexArray, MSpace::kObject));
    if (vertexArray.length() != vertices) return MS::kFailure;
    const MPoint cutterCenter = center ? meshBBox.center() : MPoint(0.0, 0.0, 0.0);
    dommy::math::Bounds3d bounds;
    mesh.points.resize(vertices);
    for (unsigned int i = 0; i < vertexArray.length(); ++i) {
        const openvdb::Vec3d p(vertexArray[i].x - cutterCenter.x,
                              vertexArray[i].y - cutterCenter.y,
                              vertexArray[i].z - cutterCenter.z);
        const auto index = transform.worldToIndex(p);
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::numeric::checkedNumericCast(index[axis], mesh.points[i][axis]))
                return MS::kInvalidParameter;
        // Validate the rounded float coordinates actually sent to meshToVolume.
        if (!bounds.add(mesh.points[i][0], mesh.points[i][1], mesh.points[i][2]))
            return MS::kInvalidParameter;
    }
    mesh.bounds = dommy::vdb::gridBounds(bounds);
    openvdb::CoordBBox checked;
    if (!fractureBox(mesh.bounds, checked)) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(DOpenVDBMakePrimList(object, mesh.primitives));
    return mesh.primitives.empty() ? MS::kInvalidParameter : MS::kSuccess;
}
template<typename GridT>
bool inspectFractureGrid(const GridT& grid, dommy::math::Bounds3d& bounds,
                         dommy::vdb::TreeEditBudget& budget, float& width) {
    if (!dommy::vdb::validLevelSetScale(grid) ||
        !dommy::vdb::validDerivativeGridMap(grid.transform()) ||
        !dommy::numeric::checkedNumericCast(
            static_cast<long double>(grid.background()) / grid.voxelSize()[0], width) || width <= 0)
        return false;
    dommy::vdb::ValuePreflight values(budget.work, 2);
    values(grid);
    if (!values.valid) return false;
    const long double limit = static_cast<long double>(grid.background()) * 64;
    for (auto it = grid.cbeginValueAll(); it; ++it) {
        if (std::fabs(static_cast<long double>(*it)) > limit) return false;
        if (it.isTileValue() && !it.isValueOn() && *it == grid.background()) continue;
        openvdb::CoordBBox box;
        it.getBoundingBox(box);
        if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
            !bounds.add(box.max()[0], box.max()[1], box.max()[2])) return false;
    }
    return true;
}
template<typename GridT>
bool checkFracturePieces(const std::list<typename GridT::Ptr>& residuals,
                         const std::list<typename GridT::Ptr>& fragments,
                         const GridCPtrVec& sources, const GridT& first,
                         std::size_t available, dommy::vdb::TreeEditBudget& budget) {
    std::size_t count = 0;
    if (!dommy::generation::checkedSum(residuals.size(), fragments.size(), available, count))
        return false;
    std::set<const Grid*> seen;
    dommy::vdb::TreeEditBudget resident;
    for (const auto* list : {&residuals, &fragments}) for (const auto& grid : *list) {
        if (!grid || !seen.insert(grid.get()).second || grid->transform() != first.transform())
            return false;
        for (const auto& source : sources) if (source.get() == grid.get()) return false;
        dommy::math::Bounds3d bounds;
        float width = 0;
        if (!inspectFractureGrid(*grid, bounds, budget, width) ||
            !resident.addBytes(1, grid->memUsage())) return false;
        if (!bounds.empty()) {
            openvdb::CoordBBox checked;
            if (!fractureBox(dommy::vdb::gridBounds(bounds), checked)) return false;
        }
    }
    return true;
}
template<typename GridT>
MStatus fractureGrids(const MObject& node, const GridCPtrVec& sources,
                      MDataBlock& data, DOpenVDBData& output) {
    using List = std::list<typename GridT::Ptr>;
    const auto first = openvdb::gridConstPtrCast<GridT>(sources.front());
    if (!first) return MS::kFailure;
    FractureSettings settings;
    DOMMY_RETURN_IF_FAILURE(readFractureSettings(node, data, settings));
    buildFractureRotations(settings);
    dommy::vdb::TreeEditBudget budget;
    dommy::math::Bounds3d domain;
    GridCPtrVec matched;
    float width = 0;
    for (const auto& source : sources) {
        const auto grid = openvdb::gridConstPtrCast<GridT>(source);
        if (!grid || grid->transform() != first->transform() ||
            !openvdb::math::isApproxEqual(grid->background(), first->background())) {
            output.insert(*source);
            continue;
        }
        float candidateWidth = 0;
        if (!inspectFractureGrid(*grid, domain, budget, candidateWidth) ||
            !budget.addBytes(2, grid->memUsage())) return MS::kInvalidParameter;
        if (matched.empty()) width = candidateWidth;
        matched.push_back(source);
    }
    if (matched.empty() || domain.empty()) return MS::kInvalidParameter;
    const std::size_t available = dommy::vdb::kMaxVdbGridCount - output.numberOfGrids();
    if (matched.size() > available) return MS::kInvalidParameter;
    // Mesh conversion and all transformed cutter bounds are planned before any tree copy.
    FractureMesh mesh;
    DOMMY_RETURN_IF_FAILURE(readFractureMesh(data, first->transform(), settings.center, mesh));
    auto cutterBounds = mesh.bounds;
    cutterBounds.expand(width + 8);
    auto bounds = dommy::vdb::gridBounds(domain);
    bounds.expand(width + 8);
    openvdb::CoordBBox checked, cutterRegion;
    if (!fractureBox(bounds, checked) || !fractureBox(cutterBounds, cutterRegion))
        return MS::kInvalidParameter;
    bounds.expand(cutterBounds);
    for (std::size_t i = 0; i < settings.points.size(); ++i) {
        auto transform = first->transform().copy();
        if (!transform) return MS::kFailure;
        if (settings.random) {
            const auto rotation = settings.rotations[i].eulerAngles(openvdb::math::XYZ_ROTATION);
            transform->preRotate(rotation[0], openvdb::math::X_AXIS);
            transform->preRotate(rotation[1], openvdb::math::Y_AXIS);
            transform->preRotate(rotation[2], openvdb::math::Z_AXIS);
        }
        transform->postTranslate(settings.points[i]);
        openvdb::BBoxd placed;
        if (!dommy::vdb::validRelativeGridMap(*transform, first->transform()) ||
            !dommy::vdb::validRelativeGridMap(first->transform(), *transform) ||
            !dommy::vdb::mapGridBox(cutterBounds, *transform, first->transform(), placed))
            return MS::kInvalidParameter;
        placed.expand(3);
        if (!fractureBox(placed, checked)) return MS::kInvalidParameter;
        bounds.expand(placed);
    }
    if (!fractureBox(bounds, checked) ||
        !dommy::vdb::chargeDenseTreeRegion<GridT>(cutterRegion, budget, 1, 512, 256))
        return MS::kInvalidParameter;
    std::size_t voxels = 0;
    if (!dommy::vdb::treeBlockCount(cutterRegion, 1, voxels) ||
        !budget.addWork(voxels, mesh.primitives.size())) return MS::kInvalidParameter;
    // Reserve initial tree copies and the first worst-case split before conversion.
    if (!dommy::vdb::chargeDenseTreeRegion<GridT>(checked, budget, matched.size(), 64, 128))
        return MS::kInvalidParameter;
    std::size_t next = 0;
    if (!dommy::generation::checkedSum(matched.size(), matched.size(), available, next) ||
        !dommy::vdb::chargeDenseTreeRegion<GridT>(checked, budget, next, 64, 128))
        return MS::kInvalidParameter;
    List residuals;
    for (const auto& source : matched) {
        const auto copy = source->deepCopyGrid();
        if (!copy || copy.get() == source.get()) return MS::kFailure;
        auto typed = openvdb::gridPtrCast<GridT>(copy);
        if (!typed) return MS::kFailure;
        residuals.push_back(typed);
    }
    const List noFragments;
    if (!checkFracturePieces<GridT>(residuals, noFragments, sources, *first, available, budget))
        return MS::kFailure;
    openvdb::util::NullInterrupter interrupt;
    openvdb::tools::QuadAndTriangleDataAdapter<openvdb::Vec3s, openvdb::Vec4I>
        adapter(mesh.points, mesh.primitives);
    auto cutter = openvdb::tools::meshToVolume<GridT>(
        interrupt, adapter, first->transform(), width, width);
    if (!cutter || cutter->activeVoxelCount() == 0 || cutter->transform() != first->transform())
        return MS::kFailure;
    for (const auto& source : sources) if (source.get() == cutter.get()) return MS::kFailure;
    {
        dommy::math::Bounds3d cutterData;
        float cutterWidth = 0;
        openvdb::CoordBBox generatedRegion;
        if (!inspectFractureGrid(*cutter, cutterData, budget, cutterWidth) || cutterData.empty() ||
            !fractureBox(dommy::vdb::gridBounds(cutterData), generatedRegion)) return MS::kFailure;
    }
    openvdb::tools::LevelSetFracture<GridT, openvdb::util::NullInterrupter> tool(&interrupt);
    const std::size_t passes = std::max(std::size_t(1), settings.points.size());
    for (std::size_t i = 0; i < passes; ++i) {
        std::size_t current = 0;
        if (!dommy::generation::checkedSum(residuals.size(), tool.fragments().size(), available, current) ||
            !dommy::generation::checkedSum(current, settings.overlap ? current : residuals.size(), available, next) ||
            (i != 0 && !dommy::vdb::chargeDenseTreeRegion<GridT>(
                checked, budget, std::max(std::size_t(1), next), 64, 128)))
            return MS::kInvalidParameter;
        std::vector<openvdb::Vec3s> point;
        std::vector<openvdb::math::Quats> rotation;
        if (!settings.points.empty()) point.push_back(settings.points[i]);
        if (settings.random) rotation.push_back(settings.rotations[i]);
        // Retain one tool so previously generated fragments are recut in the same order.
        tool.fracture(residuals, *cutter, false, point.empty() ? nullptr : &point,
                      rotation.empty() ? nullptr : &rotation, settings.overlap);
        if (interrupt.wasInterrupted() ||
            !checkFracturePieces<GridT>(residuals, tool.fragments(), sources, *first, available, budget))
            return MS::kFailure;
    }
    if (settings.segment) {
        std::size_t segmentLimit = 0;
        // Each connected component needs at least one active voxel. Reserve
        // worst-case component trees before the native segmenter allocates them.
        for (const auto* list : {&tool.fragments(), &residuals}) for (const auto& grid : *list) {
            std::size_t active = 0;
            if (!dommy::numeric::checkedNumericCast(grid->activeVoxelCount(), active))
                return MS::kInvalidParameter;
            active = std::max(std::size_t(1), active);
            if (!dommy::generation::checkedSum(segmentLimit, active, available, segmentLimit) ||
                !dommy::vdb::chargeDenseTreeRegion<GridT>(checked, budget, active, 64, 128))
                return MS::kInvalidParameter;
        }
        for (auto* list : {&tool.fragments(), &residuals}) {
            List segmented;
            for (const auto& grid : *list) {
                std::vector<typename GridT::Ptr> pieces;
                openvdb::tools::segmentSDF(*grid, pieces);
                if (pieces.size() > available - segmented.size()) return MS::kFailure;
                segmented.insert(segmented.end(), pieces.begin(), pieces.end());
            }
            list->swap(segmented);
        }
        if (!checkFracturePieces<GridT>(residuals, tool.fragments(), sources, *first, available, budget))
            return MS::kFailure;
    }
    std::size_t index = 0;
    for (const auto* list : {&residuals, &tool.fragments()}) for (const auto& grid : *list) {
        std::ostringstream name;
        name.imbue(std::locale::classic());
        name << grid->getName() << "_" << index++;
        if (!name) throw std::runtime_error("VDB fracture name formatting failed");
        grid->setName(name.str());
        output.insert(grid);
    }
    return MS::kSuccess;
}
} // namespace

MStatus DOpenVDBFracture::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput) return MS::kUnknownParameter;
    const DOpenVDBData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, input));
    MStatus status;
    const auto handle = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selection = handle.asString().asChar();
    MFnPluginData pluginData;
    DOpenVDBData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, output));
    if (!output) return MS::kFailure;
    GridCPtrVec selectedGrids, eligible;
    getSelectedGrids(selectedGrids, selection, *input, *output);
    for (const auto& grid : selectedGrids) {
        if (grid->getGridClass() != openvdb::GRID_LEVEL_SET ||
            !grid->transform().isLinear() || grid->empty()) output->insert(*grid);
        else eligible.push_back(grid);
    }
    if (!eligible.empty()) {
        if (eligible.front()->isType<openvdb::FloatGrid>())
            DOMMY_RETURN_IF_FAILURE(fractureGrids<openvdb::FloatGrid>(thisMObject(), eligible, data, *output));
        else if (eligible.front()->isType<openvdb::DoubleGrid>())
            DOMMY_RETURN_IF_FAILURE(fractureGrids<openvdb::DoubleGrid>(thisMObject(), eligible, data, *output));
        else for (const auto& grid : eligible) output->insert(*grid);
    }
    if (!output->hasValidData()) return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFracture")

MStatus DOpenVDBFracture::initialize() try
{
    MStatus stat;

    // attributes

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;
    MFnGenericAttribute gAttr;

    // ----------------------- INPUTS --------------------------
    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    const MObject stringDefault1 = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, stringDefault1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    aInObject = gAttr.create( "InObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));

    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    // ----------------------- Option --------------------------
    aSeparateCutters = nAttr.create("SeparateCutters", "sepcu", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    // Retained for scene compatibility; this unfinished component classifier
    // was never a valid Maya implementation.
    DOMMY_RETURN_IF_FAILURE(nAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeparateCutters));

    aCutterOverlap = nAttr.create("CutterOverlap", "cuov", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCutterOverlap));

    aCenterCutter = nAttr.create("CenterCutter", "cencu", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenterCutter));

    aRandomizeRotation = nAttr.create("RandomizeRotation", "ranrot", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomizeRotation));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aSegmentFragments = nAttr.create("SegmentFragments", "segfra", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSegmentFragments));

    const MObject stringDefault2 = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFragmentgroup = tAttr.create("Fragmentgroup", "fragrp", MFnData::kString, stringDefault2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    // Retained for scene compatibility; no Maya output group consumes it.
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFragmentgroup));

    //output
    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInObject, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeparateCutters, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCutterOverlap, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCenterCutter, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomizeRotation, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSegmentFragments, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFragmentgroup, aVdbOutput));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFracture", "initialize", std::current_exception());
}
