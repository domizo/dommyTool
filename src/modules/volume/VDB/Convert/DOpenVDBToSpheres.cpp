//
//  DOpenVDBToSpheres.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/08.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBToSpheres.h"
#include "../Common/DVDBComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "../Edit/DOpenVDBPointGenerationPolicy.h"
#include "../Edit/DOpenVDBFilterPolicy.h"

#include <openvdb/tools/VolumeToSpheres.h>
#include <openvdb/util/NullInterrupter.h>


#include <boost/algorithm/string/join.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <limits>


//vdb

#include "DOpenVDBCommon.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBGeoUtil.h"

#include "DMayaHeaders.h"

#include "DPolygonUtil.h"
#include "common/utility/DGenerationLimits.h"


#include "dvdb_ids.h"


const MTypeId DOpenVDBToSpheres::typeId( ID_VDB_DOPENVDB_TO_SPHERES );


MObject DOpenVDBToSpheres::aVdbInput;
MObject DOpenVDBToSpheres::aVdbSelectedGridNames;

MObject DOpenVDBToSpheres::aIsovalue;
MObject DOpenVDBToSpheres::aUseWorldSpaceUnits;
MObject DOpenVDBToSpheres::aMinRadiusInVoxels;
MObject DOpenVDBToSpheres::aMaxRadiusInVoxels;
MObject DOpenVDBToSpheres::aMinRadiusInWorld;
MObject DOpenVDBToSpheres::aMaxRadiusInWorld;

MObject DOpenVDBToSpheres::aMaxSpheres;
MObject DOpenVDBToSpheres::aScatterPoints;
MObject DOpenVDBToSpheres::aOverlapping;
MObject DOpenVDBToSpheres::aPreserveAttributesAndGroups;
MObject DOpenVDBToSpheres::aAddIDAttribute;
MObject DOpenVDBToSpheres::aAddPScaleAttribute;

MObject DOpenVDBToSpheres::aMeshDetail;
MObject DOpenVDBToSpheres::outMesh;

namespace {

const int kMaxMeshDetail = 5;

MStatus readFloatInput(
    MDataBlock& data, const MObject& attribute, float& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    value = handle.asFloat();
    return MS::kSuccess;
}

MStatus readIntInput(
    MDataBlock& data, const MObject& attribute, int& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    value = handle.asInt();
    return MS::kSuccess;
}

MStatus readBoolInput(
    MDataBlock& data, const MObject& attribute, bool& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    value = handle.asBool();
    return MS::kSuccess;
}

bool isFiniteSphere(const openvdb::Vec4s& sphere)
{
    return std::isfinite(sphere.x()) &&
           std::isfinite(sphere.y()) &&
           std::isfinite(sphere.z()) &&
           std::isfinite(sphere[3]) &&
           sphere[3] >= 0.0f && sphere[3] <= 4e12f &&
           std::fabs(sphere.x()) <= 1e12f && std::fabs(sphere.y()) <= 1e12f &&
           std::fabs(sphere.z()) <= 1e12f;
}

MStatus publishMeshData(
    MDataBlock& data, const MPlug& plug, const MObject& meshData)
{
    MStatus status;
    MDataHandle output = data.outputValue(DOpenVDBToSpheres::outMesh, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    status = output.set(meshData);
    if (status != MS::kSuccess) {
        return status;
    }
    output.setClean();
    return data.setClean(plug);
}


struct SphereGridPlan {
    GridCPtr grid;
    float minimum = 0, maximum = 0;
};
struct SpherePackingPreflight {
    dommy::vdb::TreeEditBudget& budget;
    bool world, valid = false;
    float minimum, maximum;
    std::size_t spheres, candidates;
    SpherePackingPreflight(dommy::vdb::TreeEditBudget& b, bool w, float low, float high,
                           std::size_t count, std::size_t samples)
        : budget(b), world(w), minimum(low), maximum(high), spheres(count), candidates(samples) {}
    template<typename GridT> void operator()(const GridT& grid) {
        dommy::math::Bounds3d bounds;
        if (!dommy::vdb::pointGenerationDomain(grid, true, budget, bounds) || bounds.empty() ||
            !grid.hasUniformVoxels()) return;
        if (grid.getGridClass() == openvdb::GRID_LEVEL_SET && !dommy::vdb::validLevelSetScale(grid))
            return;
        // Native surface interpolation and squared float distances need headroom.
        if (std::fabs(static_cast<long double>(grid.background())) > 1e12) return;
        for (auto it = grid.cbeginValueAll(); it; ++it)
            if (std::fabs(static_cast<long double>(*it)) > 1e12) return;
        const double dx = grid.voxelSize()[0];
        const double minWorld = world ? double(minimum) : double(minimum) * dx;
        const double maxWorld = world ? double(maximum) : double(maximum) * dx;
        if (!std::isfinite(minWorld) || minWorld > 4e12 || !std::isfinite(maxWorld) ||
            !dommy::numeric::checkedNumericCast(minWorld / dx, minimum) ||
            !dommy::numeric::checkedNumericCast(std::min(maxWorld, 4e12) / dx, maximum)) return;
        // Preserve the historical effectively-unbounded FLT_MAX maximum without
        // overflowing its conversion. The finite cap exceeds the admitted domain.
        maximum = std::max(maximum, minimum + 1e-5f);
        std::size_t active = 0;
        if (!dommy::numeric::checkedNumericCast(grid.activeVoxelCount(), active) || active == 0)
            return;
        auto box = dommy::vdb::gridBounds(bounds);
        box.expand(1);
        auto transform = grid.transform().copy();
        if (!transform) return;
        if (active < 10000) {
            // Native fillWithSpheres may rebuild small inputs at this finer scale.
            const double scale = 1.0 / std::cbrt(20000.0 / double(active));
            transform->preScale(openvdb::Vec3d(scale));
            openvdb::BBoxd mapped;
            if (!dommy::vdb::validRelativeGridMap(grid.transform(), *transform) ||
                !dommy::vdb::mapGridBox(box, grid.transform(), *transform, mapped)) return;
            openvdb::CoordBBox original;
            if (!dommy::vdb::pointGenerationBox(box, grid.transform(), original) ||
                !dommy::vdb::chargeDenseTreeRegion<GridT>(original, budget, 1, 64, 64)) return;
            box = mapped;
        }
        box.expand(8); // Rebuild half-band, leaf boundaries and surface neighbors.
        openvdb::CoordBBox region;
        if (!dommy::vdb::pointGenerationBox(box, *transform, region) ||
            !budget.addBytes(1, grid.memUsage()) ||
            !dommy::vdb::chargeDenseTreeRegion<GridT>(region, budget, 2, 256, 128) ||
            !budget.addBytes(candidates, 96) || !budget.addBytes(spheres, 32) ||
            !budget.addWork(candidates, spheres + 128 + dommy::vdb::pointSortPasses(candidates))) return;
        valid = true;
    }
};
bool reserveSphereMesh(const meshPrimData& base, std::size_t count,
                       dommy::vdb::TreeEditBudget& budget) {
    using namespace dommy::generation;
    std::size_t points = 0, faces = 0, connects = 0;
    if (base.m_pts.length() == 0 || base.m_faces.length() == 0 || base.m_connects.length() == 0 ||
        !checkedProduct(count, base.m_pts.length(), kMaxGeneratedPointCount, points) ||
        !checkedProduct(count, base.m_faces.length(), kMaxGeneratedFaceCount, faces) ||
        !checkedProduct(count, base.m_connects.length(), kMaxWorkItemCount, connects) ||
        !budget.addBytes(points, 96) || !budget.addBytes(faces, 64) || !budget.addBytes(connects, 8) ||
        !budget.addWork(points, 16) || !budget.addWork(connects, 4)) return false;
    std::size_t expected = 0;
    for (unsigned int i = 0; i < base.m_faces.length(); ++i) {
        if (base.m_faces[i] < 3 || !checkedSum(expected, static_cast<std::size_t>(base.m_faces[i]),
                                             kMaxWorkItemCount, expected)) return false;
    }
    if (expected != base.m_connects.length()) return false;
    for (unsigned int i = 0; i < base.m_connects.length(); ++i)
        if (base.m_connects[i] < 0 || static_cast<unsigned int>(base.m_connects[i]) >= base.m_pts.length())
            return false;
    for (unsigned int i = 0; i < base.m_pts.length(); ++i) {
        const auto& p = base.m_pts[i];
        if (!dommy::numeric::canRepresentAsFloat(p.x) || !dommy::numeric::canRepresentAsFloat(p.y) ||
            !dommy::numeric::canRepresentAsFloat(p.z) || !std::isfinite(p.w)) return false;
    }
    return true;
}

} // namespace


void* DOpenVDBToSpheres::creator()
{
    return ::dommy::maya::createObject("DOpenVDBToSpheres", [&]() {
        return new DOpenVDBToSpheres();
    });
}



MStatus DOpenVDBToSpheres::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) {
        return MS::kUnknownParameter;
    }

    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputVDB(aVdbInput, data, inputVdb));

    MStatus status;
    const MDataHandle selectionHandle =
        data.inputValue(aVdbSelectedGridNames, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    GridCPtrVec grids;
    const std::string selection = selectionHandle.asString().asChar();
    if (!getSelectedGrids(grids, selection, *inputVdb) || grids.empty()) {
        MGlobal::displayError("DVDBToSpheres found no selected VDB grids.");
        return MS::kInvalidParameter;
    }

    float isovalue = 0.0f;
    float minRadiusVoxel = 0.0f;
    float maxRadiusVoxel = 0.0f;
    float minRadiusWorld = 0.0f;
    float maxRadiusWorld = 0.0f;
    bool worldUnits = false;
    bool overlapping = false;
    int rawSphereCount = 0;
    int rawScatterCount = 0;
    int meshDetail = 0;

    status = readFloatInput(data, aIsovalue, isovalue);
    if (status != MS::kSuccess) return status;
    status = readBoolInput(data, aUseWorldSpaceUnits, worldUnits);
    if (status != MS::kSuccess) return status;
    if (worldUnits) {
        DOMMY_RETURN_IF_FAILURE(readFloatInput(data, aMinRadiusInWorld, minRadiusWorld));
        DOMMY_RETURN_IF_FAILURE(readFloatInput(data, aMaxRadiusInWorld, maxRadiusWorld));
    } else {
        DOMMY_RETURN_IF_FAILURE(readFloatInput(data, aMinRadiusInVoxels, minRadiusVoxel));
        DOMMY_RETURN_IF_FAILURE(readFloatInput(data, aMaxRadiusInVoxels, maxRadiusVoxel));
    }
    status = readBoolInput(data, aOverlapping, overlapping);
    if (status != MS::kSuccess) return status;
    status = readIntInput(data, aMaxSpheres, rawSphereCount);
    if (status != MS::kSuccess) return status;
    status = readIntInput(data, aScatterPoints, rawScatterCount);
    if (status != MS::kSuccess) return status;
    status = readIntInput(data, aMeshDetail, meshDetail);
    if (status != MS::kSuccess) return status;

    const bool finiteRadii =
        std::isfinite(minRadiusVoxel) && std::isfinite(maxRadiusVoxel) &&
        std::isfinite(minRadiusWorld) && std::isfinite(maxRadiusWorld);
    if (!std::isfinite(isovalue) || std::fabs(isovalue) > 1e12f || !finiteRadii ||
        minRadiusVoxel < 0.0f || maxRadiusVoxel < 0.0f ||
        minRadiusWorld < 0.0f || maxRadiusWorld < 0.0f ||
        meshDetail < 1 || meshDetail > kMaxMeshDetail) {
        MGlobal::displayError(
            "DVDBToSpheres requires finite non-negative radii and "
            "MeshDetail in [1, 5].");
        return MS::kInvalidParameter;
    }

    std::size_t sphereCount = 0U;
    std::size_t scatterCount = 0U;
    std::size_t maximumSelectedSphereCount = 0U;
    if (!dommy::generation::checkedCount(
            rawSphereCount,
            dommy::generation::kMaxGeneratedTransformCount,
            sphereCount) ||
        sphereCount == 0U ||
        !dommy::generation::checkedCount(
            rawScatterCount,
            dommy::generation::kMaxGeneratedPointCount,
            scatterCount) ||
        scatterCount == 0U ||
        !dommy::generation::checkedProduct(
            grids.size(), sphereCount,
            dommy::generation::kMaxGeneratedTransformCount,
            maximumSelectedSphereCount)) {
        MGlobal::displayError(
            "DVDBToSpheres input exceeds the generation safety budget.");
        return MS::kInvalidParameter;
    }

    dommy::vdb::TreeEditBudget budget;
    std::vector<SphereGridPlan> plans;
    std::vector<std::string> skippedGrids;
    maximumSelectedSphereCount = 0;
    for (const auto& grid : grids) {
        const auto type = getVdbGridType(*grid);
        if (type != DVdbGridType::kFloat && type != DVdbGridType::kDouble) {
            skippedGrids.push_back(grid->getName());
            continue;
        }
        if (grid->empty() || grid->activeVoxelCount() == 0) continue;
        SpherePackingPreflight plan(budget, worldUnits,
            worldUnits ? minRadiusWorld : minRadiusVoxel,
            worldUnits ? maxRadiusWorld : maxRadiusVoxel,
            sphereCount, std::max(sphereCount, scatterCount));
        if (!processVdbRealGrid(type, *grid, plan) || !plan.valid ||
            !dommy::generation::checkedSum(maximumSelectedSphereCount, sphereCount,
                dommy::generation::kMaxGeneratedTransformCount, maximumSelectedSphereCount))
            return MS::kInvalidParameter;
        SphereGridPlan entry;
        entry.grid = grid; entry.minimum = plan.minimum; entry.maximum = plan.maximum;
        plans.push_back(entry);
    }
    // Read and validate the exact historical base mesh before the native packing
    // kernel; no duplicated detail/count formula or changed subdivision convention.
    meshPrimData sphereMeshData;
    if (!plans.empty()) {
        DMeshSphere baseSphere(meshDetail);
        if (baseSphere.meshObj.isNull()) return MS::kFailure;
        MFnMesh baseMesh(baseSphere.meshObj, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(getPrimDataFromMesh(baseMesh, sphereMeshData));
        sphereMeshData.m_obj = baseSphere.meshObj;
        if (!reserveSphereMesh(sphereMeshData, maximumSelectedSphereCount, budget))
            return MS::kInvalidParameter;
    }
    openvdb::util::NullInterrupter interrupter;
    MPointArray spherePositions;
    MFloatArray sphereRadii;
    for (const auto& plan : plans) {
        std::vector<openvdb::Vec4s> spheres;
        const auto copy = plan.grid->copyGrid();
        if (!copy || copy.get() == plan.grid.get()) return MS::kFailure;
        const auto gridType = getVdbGridType(*plan.grid);
        if (gridType == DVdbGridType::kFloat) {
            const auto grid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(copy);
            if (!grid) return MS::kFailure;
            openvdb::tools::fillWithSpheres(*grid, spheres, rawSphereCount, overlapping,
                plan.minimum, plan.maximum, isovalue, rawScatterCount, &interrupter);
        } else {
            const auto grid = openvdb::gridConstPtrCast<openvdb::DoubleGrid>(copy);
            if (!grid) return MS::kFailure;
            openvdb::tools::fillWithSpheres(*grid, spheres, rawSphereCount, overlapping,
                plan.minimum, plan.maximum, isovalue, rawScatterCount, &interrupter);
        }
        if (interrupter.wasInterrupted() || spheres.size() > sphereCount) return MS::kFailure;

        std::size_t aggregateCount = 0U;
        if (!dommy::generation::checkedSum(
                spherePositions.length(), spheres.size(),
                dommy::generation::kMaxGeneratedTransformCount,
                aggregateCount)) {
            MGlobal::displayError(
                "DVDBToSpheres generated too many sphere transforms.");
            return MS::kFailure;
        }

        for (std::size_t index = 0U; index < spheres.size(); ++index) {
            const openvdb::Vec4s& sphere = spheres[index];
            if (!isFiniteSphere(sphere)) {
                MGlobal::displayError(
                    "DVDBToSpheres received invalid generated sphere data.");
                return MS::kFailure;
            }
            status = spherePositions.append(
                MPoint(sphere.x(), sphere.y(), sphere.z()));
            if (status != MS::kSuccess) return status;
            status = sphereRadii.append(sphere[3]);
            if (status != MS::kSuccess) return status;
        }
    }

    if (!skippedGrids.empty()) {
        const std::string warning =
            "Only scalar float/double grids are supported; skipped: '" +
            boost::algorithm::join(skippedGrids, ", ") + "'.";
        MGlobal::displayWarning(warning.c_str());
    }
    if (spherePositions.length() != sphereRadii.length()) {
        return MS::kFailure;
    }

    MFnMeshData meshDataFunction;
    MObject outputMeshData = meshDataFunction.create(&status);
    if (status != MS::kSuccess) {
        return status;
    }
    if (outputMeshData.isNull()) return MS::kFailure;
    if (spherePositions.length() == 0U) {
        return publishMeshData(data, plug, outputMeshData);
    }

    const std::size_t generatedSphereCount = spherePositions.length();
    std::size_t totalPointCount = 0U;
    std::size_t totalFaceCount = 0U;
    std::size_t totalConnectCount = 0U;
    if (!dommy::generation::checkedProduct(
            generatedSphereCount, sphereMeshData.m_pts.length(),
            dommy::generation::kMaxGeneratedPointCount, totalPointCount) ||
        !dommy::generation::checkedProduct(
            generatedSphereCount, sphereMeshData.m_faces.length(),
            dommy::generation::kMaxGeneratedFaceCount, totalFaceCount) ||
        !dommy::generation::checkedProduct(
            generatedSphereCount, sphereMeshData.m_connects.length(),
            dommy::generation::kMaxWorkItemCount, totalConnectCount)) {
        MGlobal::displayError(
            "DVDBToSpheres output mesh exceeds the topology safety budget.");
        return MS::kInvalidParameter;
    }

    MFloatPointArray totalPoints;
    MIntArray totalFaceCounts;
    MIntArray totalConnectIndices;
    std::size_t pointOffset = 0U;
    for (std::size_t index = 0U;
         index < generatedSphereCount; ++index) {
        MTransformationMatrix transform;
        const double radius = sphereRadii[static_cast<unsigned int>(index)];
        const double scale[3] = {radius, radius, radius};
        status = transform.addScale(scale, MSpace::kObject);
        if (status != MS::kSuccess) return status;
        status = transform.addTranslation(
            MVector(spherePositions[static_cast<unsigned int>(index)]),
            MSpace::kObject);
        if (status != MS::kSuccess) return status;

        meshPrimData transformedSphere;
        DOMMY_RETURN_IF_FAILURE(sphereMeshData.duplicateTransform(transform, transformedSphere));
        if (transformedSphere.m_pts.length() != sphereMeshData.m_pts.length() ||
            transformedSphere.m_faces.length() != sphereMeshData.m_faces.length() ||
            transformedSphere.m_connects.length() != sphereMeshData.m_connects.length()) return MS::kFailure;
        for (unsigned int point = 0U;
             point < transformedSphere.m_pts.length(); ++point) {
            const auto& p = transformedSphere.m_pts[point];
            if (!dommy::numeric::canRepresentAsFloat(p.x) || !dommy::numeric::canRepresentAsFloat(p.y) ||
                !dommy::numeric::canRepresentAsFloat(p.z) || !dommy::numeric::canRepresentAsFloat(p.w))
                return MS::kFailure;
            status = totalPoints.append(MFloatPoint(p));
            if (status != MS::kSuccess) return status;
        }
        for (unsigned int face = 0U;
             face < transformedSphere.m_faces.length(); ++face) {
            status = totalFaceCounts.append(transformedSphere.m_faces[face]);
            if (status != MS::kSuccess) return status;
        }
        for (unsigned int connect = 0U;
             connect < transformedSphere.m_connects.length(); ++connect) {
            status = totalConnectIndices.append(
                transformedSphere.m_connects[connect] +
                static_cast<int>(pointOffset));
            if (status != MS::kSuccess) return status;
        }
        pointOffset += transformedSphere.m_pts.length();
    }

    if (pointOffset != totalPointCount ||
        totalFaceCounts.length() != totalFaceCount ||
        totalConnectIndices.length() != totalConnectCount) {
        return MS::kFailure;
    }

    MFnMesh outputMeshFunction;
    const MObject createdMesh = outputMeshFunction.create(
        totalPoints.length(), totalFaceCounts.length(), totalPoints,
        totalFaceCounts, totalConnectIndices, outputMeshData, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    if (createdMesh.isNull()) return MS::kFailure;
    return publishMeshData(data, plug, outputMeshData);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBToSpheres")



MStatus DOpenVDBToSpheres::initialize() try
{
    MStatus stat;
    
    // attributes
    
    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;
    
    // ----------------------- INPUTS --------------------------
    aVdbInput = tAttr.create( "vdbInput", "input", DOpenVDBData::id, MObject::kNullObj, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));
    
    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("vdbSelectedGridNames", "selectedgrids",
                                         MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));
    
    
    aIsovalue = nAttr.create("isovalue", "iso", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsovalue));
    
    
    aUseWorldSpaceUnits = nAttr.create("UseWorldSpaceUnits", "uwsu", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseWorldSpaceUnits));
    
    
    
    aMinRadiusInVoxels = nAttr.create("MinRadiusInVoxels", "minriv", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(std::numeric_limits<float>::max()));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinRadiusInVoxels));
    
    
    aMaxRadiusInVoxels = nAttr.create("MaxRadiusInVoxels", "maxriv", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(std::numeric_limits<float>::max()));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 100.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(std::numeric_limits<float>::max()));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxRadiusInVoxels));
	
	aMinRadiusInWorld = nAttr.create("MinRadiusInWorld", "minriw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 2.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(std::numeric_limits<float>::max()));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMinRadiusInWorld));
	
	
	aMaxRadiusInWorld = nAttr.create("MaxRadiusInWorld", "maxriw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(std::numeric_limits<float>::max()));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 100.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(std::numeric_limits<float>::max()));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxRadiusInWorld));
	
	
	
    aMaxSpheres = nAttr.create("MaxSpheres", "maxsph", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(
        dommy::generation::kMaxGeneratedTransformCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxSpheres));
    
    aScatterPoints = nAttr.create("ScatterPoints", "scpt", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(static_cast<int>(
        dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScatterPoints));
    
    aOverlapping = nAttr.create("Overlapping", "ovl", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOverlapping));

    // Retain these historical no-op attributes for saved-scene compatibility,
    // but keep them hidden and outside the dirty-propagation graph.
    aPreserveAttributesAndGroups = nAttr.create("PreserveAttributesAndGroups", "patangrp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPreserveAttributesAndGroups));
    
    aAddIDAttribute = nAttr.create("AddIDAttribute", "aidattr", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddIDAttribute));
    
    aAddPScaleAttribute = nAttr.create("AddPScaleAttribute", "apsattr", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddPScaleAttribute));
	
	
	aMeshDetail = nAttr.create("MeshDetail", "mdtl", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(kMaxMeshDetail));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMeshDetail));
	
	
    outMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));
    
    
    
    // Setup dependencies
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, outMesh));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsovalue, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseWorldSpaceUnits, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinRadiusInVoxels, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxRadiusInVoxels, outMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinRadiusInWorld, outMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxRadiusInWorld, outMesh));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxSpheres, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScatterPoints, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOverlapping, outMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aMeshDetail, outMesh));
	
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBToSpheres", "initialize", std::current_exception());
}
