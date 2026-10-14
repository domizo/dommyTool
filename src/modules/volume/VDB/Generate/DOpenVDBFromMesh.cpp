// DOMMY_THIRD_PARTY_NOTICE_BEGIN
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2018 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_maya/maya/OpenVDBFromPolygonsNode.cc

//
//  DOpenVDBFromMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBFromMesh.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DGenerationLimits.h"
#include "../Common/DVDBComputeGuard.h"
#include "../Common/DOpenVDBGeoUtil.h"
#include "../Edit/DOpenVDBMeshConversionPolicy.h"
#include "common/vdb/DVdbOp.h"
#include "common/utility/DStatusOperation.h"
#include "common/string/DStreamFormat.h"

#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/LevelSetUtil.h>

#include <maya/MFnTypedAttribute.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnPluginData.h>
#include <maya/MGlobal.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>

#include "DMeshDataHeaders.h"

#include <limits>

#include "dvdb_ids.h"

namespace {

MStatus appendOpenVDBMesh(
    const std::vector<openvdb::Vec3s>& sourcePoints,
    const std::vector<openvdb::Vec4I>& sourcePrimitives,
    std::vector<openvdb::Vec3s>& points,
    std::vector<openvdb::Vec4I>& primitives)
{
    std::size_t totalPointCount = 0U;
    std::size_t totalPrimitiveCount = 0U;
    if (!dommy::generation::checkedSum(
            points.size(),
            sourcePoints.size(),
            dommy::generation::kMaxGeneratedPointCount,
            totalPointCount) ||
        !dommy::generation::checkedSum(
            primitives.size(),
            sourcePrimitives.size(),
            dommy::generation::kMaxGeneratedFaceCount,
            totalPrimitiveCount)) {
        return MS::kFailure;
    }

    const std::size_t pointOffset = points.size();
    std::vector<openvdb::Vec4I> adjustedPrimitives;
    adjustedPrimitives.reserve(sourcePrimitives.size());
    for (const openvdb::Vec4I& source : sourcePrimitives) {
        openvdb::Vec4I adjusted(openvdb::util::INVALID_IDX);
        for (int component = 0; component < 4; ++component) {
            const openvdb::Index32 sourceIndex = source[component];
            if (sourceIndex == openvdb::util::INVALID_IDX) {
                if (component != 3) {
                    return MS::kFailure;
                }
                continue;
            }
            if (!dommy::memory::indexInRange(
                    sourceIndex, sourcePoints.size())) {
                return MS::kFailure;
            }

            std::size_t adjustedIndex = 0U;
            if (!dommy::memory::checkedAdd(
                    pointOffset,
                    static_cast<std::size_t>(sourceIndex),
                    adjustedIndex) ||
                adjustedIndex >
                    static_cast<std::size_t>(
                        std::numeric_limits<openvdb::Index32>::max())) {
                return MS::kFailure;
            }
            adjusted[component] =
                static_cast<openvdb::Index32>(adjustedIndex);
        }
        adjustedPrimitives.push_back(adjusted);
    }

    points.reserve(totalPointCount);
    primitives.reserve(totalPrimitiveCount);
    points.insert(points.end(), sourcePoints.begin(), sourcePoints.end());
    primitives.insert(primitives.end(),
                      adjustedPrimitives.begin(),
                      adjustedPrimitives.end());
    return MS::kSuccess;
}


struct FromMeshSettings {
    bool distance = false, density = false;
    std::string distanceName, densityName;
    dommy::vdb::MeshConversionSettings conversion;
};
MStatus readFromMeshSettings(MDataBlock& data, FromMeshSettings& settings) {
    MStatus status;
    const auto distance = data.inputValue(DOpenVDBFromMesh::aExportDistanceGrid, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.distance = distance.asBool();
    const auto density = data.inputValue(DOpenVDBFromMesh::aExportDensityGrid, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.density = density.asBool();
    if (!settings.distance && !settings.density) return MS::kSuccess;
    const auto voxel = data.inputValue(DOpenVDBFromMesh::aVoxelSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.conversion.voxelSize = voxel.asFloat();
    const auto exterior = data.inputValue(DOpenVDBFromMesh::aExteriorBandWidth, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.conversion.exterior = exterior.asFloat();
    const auto unsignedField = data.inputValue(DOpenVDBFromMesh::aUnsignedDistanceField, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    settings.conversion.unsignedDistance = unsignedField.asBool();
    if (!settings.conversion.unsignedDistance) {
        const auto fill = data.inputValue(DOpenVDBFromMesh::aFillInterior, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.conversion.fillInterior = fill.asBool();
        if (!settings.conversion.fillInterior) {
            const auto interior = data.inputValue(DOpenVDBFromMesh::aInteriorBandWidth, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            settings.conversion.interior = interior.asFloat();
        }
    }
    if (!dommy::vdb::validMeshConversionSettings(settings.conversion)) return MS::kInvalidParameter;
    if (settings.distance) {
        const auto name = data.inputValue(DOpenVDBFromMesh::aDistanceGridName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.distanceName = name.asString().asChar();
    }
    if (settings.density) {
        const auto name = data.inputValue(DOpenVDBFromMesh::aDensityGridName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        settings.densityName = name.asString().asChar();
    }
    return MS::kSuccess;
}
MStatus readFromMeshObject(MDataBlock& data, MObject& object) {
    MStatus status;
    const auto mesh = data.inputValue(DOpenVDBFromMesh::aMeshInput, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (mesh.asMesh().isNull()) return MS::kInvalidParameter;
    object = mesh.asMeshTransformed();
    return object.isNull() ? MS::kInvalidParameter : MS::kSuccess;
}
MStatus appendFromMeshObject(const MObject& object, const openvdb::math::Transform& transform,
                              dommy::vdb::TreeEditBudget& budget,
                              std::vector<openvdb::Vec3s>& points,
                              std::vector<openvdb::Vec4I>& primitives) {
    std::size_t vertices = 0, triangles = 0;
    DOMMY_RETURN_IF_FAILURE(preflightVdbMeshInput(object, budget, vertices, triangles));
    std::vector<openvdb::Vec3s> meshPoints;
    std::vector<openvdb::Vec4I> meshPrimitives;
    DOMMY_RETURN_IF_FAILURE(DOpenVDBMakePointList(object, transform, meshPoints));
    DOMMY_RETURN_IF_FAILURE(DOpenVDBMakePrimList(object, meshPrimitives));
    if (meshPoints.size() != vertices || meshPrimitives.size() > triangles) return MS::kFailure;
    return appendOpenVDBMesh(meshPoints, meshPrimitives, points, primitives);
}
MStatus collectFromMeshInputs(MDataBlock& data, const MObject& node,
                               const openvdb::math::Transform& transform,
                               dommy::vdb::TreeEditBudget& budget,
                               std::vector<openvdb::Vec3s>& points,
                               std::vector<openvdb::Vec4I>& primitives) {
    MStatus status;
    const bool meshConnected = MPlug(node, DOpenVDBFromMesh::aMeshInput).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (meshConnected) {
        MObject object;
        DOMMY_RETURN_IF_FAILURE(readFromMeshObject(data, object));
        return appendFromMeshObject(object, transform, budget, points, primitives);
    }
    const bool customConnected = MPlug(node, DOpenVDBFromMesh::inDM).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!customConnected) return MS::kInvalidParameter;
    const DMeshData* meshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(DOpenVDBFromMesh::inDM, data, meshes));
    // Reserve Maya materialization before converting any custom mesh.
    std::size_t vertices = 0, faces = 0, connects = 0;
    if (!meshes || !meshes->getCheckedTotalVertex(vertices) || !meshes->getCheckedTotalFace(faces) ||
        !meshes->getCheckedTotalConnect(connects) || !budget.addBytes(vertices, 64) ||
        !budget.addBytes(faces, 16) || !budget.addBytes(connects, 16)) return MS::kInvalidParameter;
    for (auto it = meshes->beginMeshes(); it != meshes->endMeshes(); ++it) {
        if (!*it) return MS::kInvalidParameter;
        MObject object;
        DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmeshWithUV(
            *it->get(), object, DMeshPointSpace::Transformed));
        DOMMY_RETURN_IF_FAILURE(appendFromMeshObject(object, transform, budget, points, primitives));
    }
    return MS::kSuccess;
}
MStatus estimateFromMeshResolution(MDataBlock& data, const MObject& node, const MPlug& plug) {
    MStatus status;
    const auto voxel = data.inputValue(DOpenVDBFromMesh::aVoxelSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float voxelSize = voxel.asFloat();
    if (!std::isfinite(voxelSize) || voxelSize <= 0) return MS::kInvalidParameter;
    dommy::math::Bounds3d bounds;
    dommy::vdb::TreeEditBudget budget;
    const bool meshConnected = MPlug(node, DOpenVDBFromMesh::aMeshInput).isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (meshConnected) {
        MObject object;
        DOMMY_RETURN_IF_FAILURE(readFromMeshObject(data, object));
        MFnMesh mesh(object, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int rawCount = mesh.numVertices(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t count = 0;
        if (!dommy::generation::checkedCount(rawCount, dommy::generation::kMaxGeneratedPointCount, count) ||
            !budget.addBytes(count, sizeof(MFloatPoint)) || !budget.addWork(count, 1)) return MS::kInvalidParameter;
        MFloatPointArray points;
        DOMMY_RETURN_IF_FAILURE(mesh.getPoints(points, MSpace::kObject));
        if (points.length() != count) return MS::kFailure;
        for (unsigned int i = 0; i < points.length(); ++i)
            if (!bounds.add(points[i].x, points[i].y, points[i].z)) return MS::kInvalidParameter;
    } else {
        const bool customConnected = MPlug(node, DOpenVDBFromMesh::inDM).isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (customConnected) {
            const DMeshData* meshes = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDM(DOpenVDBFromMesh::inDM, data, meshes));
            if (!meshes) return MS::kInvalidParameter;
            for (auto it = meshes->beginMeshes(); it != meshes->endMeshes(); ++it) {
                if (!*it) return MS::kInvalidParameter;
                const auto* points = (*it)->getPositionPtrConst();
                if (!points || !budget.addWork(points->size(), 1)) return MS::kInvalidParameter;
                // Preserve the historical raw custom-mesh coordinate convention
                // for this informational output; conversion uses Transformed.
                for (const auto& p : *points)
                    if (!bounds.add(p[0], p[1], p[2])) return MS::kInvalidParameter;
            }
        }
    }
    int resolution[3] = {0, 0, 0};
    if (!bounds.empty()) for (int axis = 0; axis < 3; ++axis) {
        const double cells = std::ceil((bounds.maximum(axis) - bounds.minimum(axis)) / voxelSize);
        if (!dommy::numeric::checkedNumericCast(cells, resolution[axis])) return MS::kInvalidParameter;
    }
    std::stringstream text;
    text << resolution[0] << " x " << resolution[1] << " x " << resolution[2] << " voxels";
    const MString resolutionText(dommy::text::checkedStreamText(text, "VDB resolution formatting failed").c_str());
    auto output = data.outputValue(DOpenVDBFromMesh::aEstimatedGridResolution, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    output.set(resolutionText); // Maya's string setter returns void.
    return data.setClean(plug);
}

} // namespace

const MTypeId DOpenVDBFromMesh::typeId( ID_VDB_DOPENVDB_FROM_MESH );

MObject DOpenVDBFromMesh::inDM;
MObject DOpenVDBFromMesh::aMeshInput;
MObject DOpenVDBFromMesh::aVdbOutput;
MObject DOpenVDBFromMesh::aExportDistanceGrid;
MObject DOpenVDBFromMesh::aDistanceGridName;
MObject DOpenVDBFromMesh::aExportDensityGrid;
MObject DOpenVDBFromMesh::aDensityGridName;
MObject DOpenVDBFromMesh::aVoxelSize;
MObject DOpenVDBFromMesh::aExteriorBandWidth;
MObject DOpenVDBFromMesh::aInteriorBandWidth;
MObject DOpenVDBFromMesh::aFillInterior;
MObject DOpenVDBFromMesh::aUnsignedDistanceField;
MObject DOpenVDBFromMesh::aEstimatedGridResolution;
MObject DOpenVDBFromMesh::aNodeInfo;


void* DOpenVDBFromMesh::creator()
{
    return ::dommy::maya::createObject("DOpenVDBFromMesh", [&]() {
        return new DOpenVDBFromMesh();
    });
}



MStatus DOpenVDBFromMesh::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug == aEstimatedGridResolution) return estimateFromMeshResolution(data, thisMObject(), plug);
    if (plug != aVdbOutput && plug != aNodeInfo) return MS::kUnknownParameter;
    Timer timer;
    FromMeshSettings settings;
    DOMMY_RETURN_IF_FAILURE(readFromMeshSettings(data, settings));
    dommy::vdb::TreeEditBudget budget;
    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec4I> primitives;
    openvdb::FloatGrid::Ptr grid, density;
    if (settings.distance || settings.density) {
        const auto transform = openvdb::math::Transform::createLinearTransform(settings.conversion.voxelSize);
        if (!transform) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(collectFromMeshInputs(data, thisMObject(), *transform, budget, points, primitives));
        if (!dommy::vdb::convertMeshVolume(points, primitives, *transform, settings.conversion,
                budget, settings.distance && settings.density ? 2 : 1, grid)) return MS::kInvalidParameter;
        if (settings.distance && !settings.distanceName.empty()) grid->setName(settings.distanceName);
        if (settings.density) {
            density = settings.distance ? grid->deepCopy() : grid;
            if (!density || (settings.distance && density.get() == grid.get()) ||
                !dommy::vdb::convertMeshVolumeToFog(density)) return MS::kFailure;
            if (!settings.densityName.empty()) density->setName(settings.densityName);
        }
    }
    MFnPluginData pluginData;
    DOpenVDBData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, output));
    if (!output) return MS::kFailure;
    if (settings.distance) output->insert(grid);
    if (settings.density) output->insert(density);
    if (!output->hasValidData()) return MS::kFailure;
    std::stringstream info;
    info << "Input Mesh\n  nr of points: " << points.size()
         << "\n  nr of primitives: " << primitives.size() << "\n";
    printGridInfo(info, *output);
    info << "Compute Time: " << timer.elapsedTime() << "\n";
    const MString infoText(dommy::text::checkedStreamText(info, "Mesh-to-VDB info formatting failed").c_str());
    MStatus status;
    auto outputHandle = data.outputValue(aVdbOutput, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    auto infoHandle = data.outputValue(aNodeInfo, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
        outputHandle, pluginData.object(), DOpenVDBData::id));
    // Maya does not provide an atomic two-output commit. If this void setter
    // throws or final cleaning fails, VDB can already contain the valid result.
    infoHandle.set(infoText);
    DOMMY_RETURN_IF_FAILURE(data.setClean(MPlug(thisMObject(), aNodeInfo)));
    return data.setClean(MPlug(thisMObject(), aVdbOutput));
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFromMesh")

MStatus DOpenVDBFromMesh::initialize() try
{
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    
    // Setup the input mesh attribute
    
    MFnMeshData meshCreator;
    MObject emptyMesh = meshCreator.create(&stat);
    if (stat != MS::kSuccess) return stat;
    
    
    MFnStringData fnStringData;
    MObject distName = fnStringData.create("surface", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    MObject densName = fnStringData.create("density", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    MObject emptyStr = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	
	
	
	
	// ----------------------- INPUTS --------------------------
	inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
	
	
    aMeshInput = tAttr.create("MeshInput", "mesh", MFnData::kMesh, emptyMesh, &stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = addAttribute(aMeshInput);
    if (stat != MS::kSuccess) return stat;
    
    
    // Conversion settings
    
    aExportDistanceGrid = nAttr.create("ExportDistanceVDB", "exportdistance", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    
    stat = addAttribute(aExportDistanceGrid);
    if (stat != MS::kSuccess) return stat;
    
    aDistanceGridName = tAttr.create("DistanceGridName", "distancename", MFnData::kString, distName, &stat);
    if (stat != MS::kSuccess) return stat;
    
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aDistanceGridName);
    if (stat != MS::kSuccess) return stat;
    
    
    aExportDensityGrid = nAttr.create(
                                      "ExportDensityVDB", "exportdensity", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    
    stat = addAttribute(aExportDensityGrid);
    if (stat != MS::kSuccess) return stat;
    
    aDensityGridName = tAttr.create(
                                    "DensityGridName", "densityname", MFnData::kString, densName, &stat);
    if (stat != MS::kSuccess) return stat;
    
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aDensityGridName);
    if (stat != MS::kSuccess) return stat;
    
    
    aVoxelSize = nAttr.create("VoxelSize", "voxelsize", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    
    stat = addAttribute(aVoxelSize);
    if (stat != MS::kSuccess) return stat;
    
    
    aExteriorBandWidth = nAttr.create(
                                      "ExteriorBandWidth", "exteriorbandwidth", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    
    stat = addAttribute(aExteriorBandWidth);
    if (stat != MS::kSuccess) return stat;
    
    aInteriorBandWidth = nAttr.create(
                                      "InteriorBandWidth", "interiorbandwidth", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    
    stat = addAttribute(aInteriorBandWidth);
    if (stat != MS::kSuccess) return stat;
    
    
    aFillInterior = nAttr.create("FillInterior", "fillinterior", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    
    stat = addAttribute(aFillInterior);
    if (stat != MS::kSuccess) return stat;
    
    
    aUnsignedDistanceField = nAttr.create(
                                          "UnsignedDistanceField", "udf", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    
    stat = addAttribute(aUnsignedDistanceField);
    if (stat != MS::kSuccess) return stat;
    
    
    // Setup the output attributes
    
    aVdbOutput = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    if (stat != MS::kSuccess) return stat;
    
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    stat = addAttribute(aVdbOutput);
    if (stat != MS::kSuccess) return stat;
    
    // VDB Info
    aEstimatedGridResolution = tAttr.create(
                                            "EstimatedGridResolution", "res", MFnData::kString, emptyStr, &stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    stat = addAttribute(aEstimatedGridResolution);
    if (stat != MS::kSuccess) return stat;
    
    aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, emptyStr, &stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    stat = addAttribute(aNodeInfo);
    if (stat != MS::kSuccess) return stat;
    
    // Set the attribute dependencies
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMeshInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportDistanceGrid, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistanceGridName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportDensityGrid, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDensityGridName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExteriorBandWidth, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInteriorBandWidth, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFillInterior, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUnsignedDistanceField, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aEstimatedGridResolution));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMeshInput, aEstimatedGridResolution));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, aEstimatedGridResolution));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMeshInput, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportDistanceGrid, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistanceGridName, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportDensityGrid, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDensityGridName, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExteriorBandWidth, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInteriorBandWidth, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFillInterior, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUnsignedDistanceField, aNodeInfo));
    
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFromMesh", "initialize", std::current_exception());
}
