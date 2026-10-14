//
//  DOpenVDBDisplay.cpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#include "DOpenVDBDisplayVP2.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include <openvdb/tools/Clip.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "DMayaHeaders.h"



#include "DNodeOperation.h"
#include "DStringOp.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DViewportCallbackGuard.h"

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"
#include "../Edit/DOpenVDBSpatialPolicy.h"





MTypeId DOpenVDBDisplayVP2::typeId( ID_VDB_DOPENVDB_DISPLAY_VP2 );
const MString DOpenVDBDisplayVP2::typeName("DVDBDisplayVP2");
MString DOpenVDBDisplayVP2::classification("drawdb/geometry/DVDBDisplayVP2");
MString	DOpenVDBDisplayVP2::registrantId("DVDBDisplayVP2Plugin");

MObject DOpenVDBDisplayVP2::aVdbInput;
MObject DOpenVDBDisplayVP2::aVdbAllGridNames;
MObject DOpenVDBDisplayVP2::aVdbSelectedGridNames;

MObject DOpenVDBDisplayVP2::aVisualizeBBox;
MObject DOpenVDBDisplayVP2::aVisualizeInternalNodes;
MObject DOpenVDBDisplayVP2::aVisualizeLeafNodes;
MObject DOpenVDBDisplayVP2::aVisualizeActiveTiles;
MObject DOpenVDBDisplayVP2::aVisualizeActiveVoxels;
MObject DOpenVDBDisplayVP2::aVisualizeSurface;
MObject DOpenVDBDisplayVP2::aIsovalue;

MObject DOpenVDBDisplayVP2::aIsClip;
MObject DOpenVDBDisplayVP2::aClipAxis;
MObject DOpenVDBDisplayVP2::aClipPosition;
MObject DOpenVDBDisplayVP2::aClipWidth;

MObject DOpenVDBDisplayVP2::aLowDetail;
MObject DOpenVDBDisplayVP2::aPointSize;
MObject DOpenVDBDisplayVP2::aColorType;
MObject DOpenVDBDisplayVP2::aInvertSurface;

MObject DOpenVDBDisplayVP2::aDisplayVector;
MObject DOpenVDBDisplayVP2::aVectorScale;
MObject DOpenVDBDisplayVP2::aIsDrawHeads;
MObject DOpenVDBDisplayVP2::aVectorColor;

MObject DOpenVDBDisplayVP2::aNodeInfo;

MObject DOpenVDBDisplayVP2::aiOpaque;
MObject DOpenVDBDisplayVP2::aiMatte;
MObject DOpenVDBDisplayVP2::aiSelfShadows;
MObject DOpenVDBDisplayVP2::aiReceiveShadows;
MObject DOpenVDBDisplayVP2::aSidedness;
MObject DOpenVDBDisplayVP2::aInvertNormals;
MObject DOpenVDBDisplayVP2::aRayBias;

MObject DOpenVDBDisplayVP2::aVolumeMode;

//volume_implicit
MObject DOpenVDBDisplayVP2::aMinBox;
MObject DOpenVDBDisplayVP2::aMaxBox;
MObject DOpenVDBDisplayVP2::aFieldChannel;
MObject DOpenVDBDisplayVP2::aSamples;
MObject DOpenVDBDisplayVP2::aSolver;

//volume
MObject DOpenVDBDisplayVP2::aStepSize;
MObject DOpenVDBDisplayVP2::aStepScale;
MObject DOpenVDBDisplayVP2::aVolumePadding;
MObject DOpenVDBDisplayVP2::aIsCompress;

MObject DOpenVDBDisplayVP2::aExportGrid;
MObject DOpenVDBDisplayVP2::aExportVelocityGrid;
MObject DOpenVDBDisplayVP2::aVelocityScale;
MObject DOpenVDBDisplayVP2::aVelocityFps;
MObject DOpenVDBDisplayVP2::aVelocityShutterStart;
MObject DOpenVDBDisplayVP2::aVelocityShutterEnd;
MObject DOpenVDBDisplayVP2::aVelocityThreshold;

MObject DOpenVDBDisplayVP2::aVdbShader;

namespace {

struct VdbViewportSettings {
    bool bbox = true, node = false, leaf = true, tile = true;
    bool surface = false, point = false, vector = false, clip = false;
    bool lowDetail = false, invertSurface = false;
    short colorType = 0, clipAxis = 1;
    float isovalue = 0.0f, clipPosition = 0.5f, clipWidth = 0.2f;
    float pointSize = 2.0f, vectorScale = 1.0f;
    bool drawHeads = true;
    MColor vectorColor = MColor(1.0f, 1.0f, 0.0f, 1.0f);
};

MStatus readVdbViewportSettings(const MObject& node, VdbViewportSettings& output)
{
    VdbViewportSettings settings;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVisualizeBBox), settings.bbox));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVisualizeInternalNodes), settings.node));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVisualizeLeafNodes), settings.leaf));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVisualizeActiveTiles), settings.tile));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVisualizeSurface), settings.surface));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVisualizeActiveVoxels), settings.point));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aDisplayVector), settings.vector));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aIsClip), settings.clip));
    if (settings.point || settings.vector) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aLowDetail), settings.lowDetail));
    }
    if (settings.point) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aPointSize), settings.pointSize));
        if (!std::isfinite(settings.pointSize)) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aColorType), settings.colorType));
        if (settings.colorType < 0 || settings.colorType > 1) return MS::kInvalidParameter;
    }
    if (settings.vector) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVectorColor), settings.vectorColor));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aIsDrawHeads), settings.drawHeads));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aVectorScale), settings.vectorScale));
        if (!dommy::maya::isFiniteColor(settings.vectorColor) ||
            !std::isfinite(settings.vectorScale)) return MS::kInvalidParameter;
    }
    if (settings.surface) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aIsovalue), settings.isovalue));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aInvertSurface), settings.invertSurface));
        if (!std::isfinite(settings.isovalue)) return MS::kInvalidParameter;
    }
    if (settings.clip) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aClipPosition), settings.clipPosition));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aClipWidth), settings.clipWidth));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(node, DOpenVDBDisplayVP2::aClipAxis), settings.clipAxis));
        if (settings.clipAxis < 0 || settings.clipAxis > 2 ||
            !std::isfinite(settings.clipPosition) || settings.clipPosition < 0.0f || settings.clipPosition > 1.0f ||
            !std::isfinite(settings.clipWidth) || settings.clipWidth < 0.01f) return MS::kInvalidParameter;
    }
    output = settings;
    return MS::kSuccess;
}

MString buildVdbInfo(const DOpenVDBData& inputVdb, const std::string& selection) {
    std::stringstream info;
    GridCPtrVec grids;
    getGrids(grids, inputVdb, selection);

    info << "DOpenVDB Info\n";
    if (grids.empty()) {
        info << "\tGrid is empty.";
        return MString(dommy::text::checkedStreamText(info, "VDB info formatting failed").c_str());
    }

    info << "\tgrid count: " << inputVdb.numberOfGrids() << "\n";
    for (GridCPtrVec::const_iterator it = grids.begin(); it != grids.end(); ++it) {
        if (!*it) {
            continue;
        }
        const openvdb::GridBase& grid = **it;
        const openvdb::Coord dimensions = grid.evalActiveVoxelDim();
        info << "\t\t-" << grid.getName() << "\n"
             << "\t\t\tType: " << grid.type() << "\n"
             << "\t\t\tValue type: " << grid.valueType() << "\n"
             << "\t\t\tActive voxel count: " << grid.activeVoxelCount() << "\n"
             << "\t\t\tMemory usage: " << formattedFileSize(grid.memUsage()) << "\n"
             << "\t\t\tVoxel size: " << grid.voxelSize()[0] << "\n"
             << "\t\t\tDimensions: " << dimensions[0] << "x" << dimensions[1] << "x"
             << dimensions[2] << "\n";
    }
    return MString(dommy::text::checkedStreamText(info, "VDB info formatting failed").c_str());
}

MBoundingBox defaultVdbBoundingBox() {
    return MBoundingBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
}

bool buildVdbViewportClipBox(const GridCPtrVec& grids, int axis,
                             float position, float width,
                             openvdb::BBoxd& output, bool& hasActiveBounds)
{
    if (grids.size() > dommy::vdb::kMaxVdbGridCount) return false;
    dommy::math::Bounds3d bounds;
    for (GridCPtrVecCIter grid = grids.begin(); grid != grids.end(); ++grid) {
        if (!*grid) return false;
        if ((*grid)->empty()) continue;
        openvdb::BBoxd world;
        if (!getActiveVoxelWorldBounds(**grid, world, false) ||
            !dommy::vdb::validGridBox(world) ||
            !bounds.add(world.min()[0], world.min()[1], world.min()[2]) ||
            !bounds.add(world.max()[0], world.max()[1], world.max()[2]))
            return false;
    }
    if (bounds.empty()) {
        hasActiveBounds = false;
        return true;
    }

    std::array<double, 3> low, high;
    const std::array<double, 3> minimum = {{bounds.minimum(0), bounds.minimum(1), bounds.minimum(2)}};
    const std::array<double, 3> maximum = {{bounds.maximum(0), bounds.maximum(1), bounds.maximum(2)}};
    if (!dommy::vdb::viewportClipBounds(minimum, maximum, axis, position, width, low, high))
        return false;
    const openvdb::BBoxd candidate(openvdb::Vec3d(low[0], low[1], low[2]),
                                  openvdb::Vec3d(high[0], high[1], high[2]));
    // Match Clip's world-to-index conversion and floor rounding before its
    // native kernel can narrow coordinates or step through a tree region.
    for (GridCPtrVecCIter grid = grids.begin(); grid != grids.end(); ++grid) {
        if ((*grid)->empty()) continue;
        openvdb::Vec3d indexMinimum, indexMaximum;
        openvdb::math::calculateBounds((*grid)->transform(), candidate.min(), candidate.max(),
                                       indexMinimum, indexMaximum);
        openvdb::CoordBBox region;
        if (!dommy::vdb::indexGridBox(openvdb::BBoxd(indexMinimum, indexMaximum), region))
            return false;
    }
    output = candidate;
    hasActiveBounds = true;
    return true;
}

bool getValueBufferLayout(const std::vector<valueBufferObjectVP2>& buffers,
                          size_t& pointCount,
                          size_t& componentCount)
{
    pointCount = 0;
    componentCount = 0;
    for (std::vector<valueBufferObjectVP2>::const_iterator it = buffers.begin();
         it != buffers.end(); ++it) {
        size_t expectedComponentCount = 0;
        if (!dommy::memory::checkedMultiply(
                it->indexArray.size(), 3U, expectedComponentCount) ||
            it->points.size() != expectedComponentCount ||
            it->normals.size() != expectedComponentCount ||
            it->colors.size() != expectedComponentCount ||
            !dommy::memory::checkedAdd(
                pointCount, it->indexArray.size(), pointCount) ||
            !dommy::memory::checkedAdd(
                componentCount, it->points.size(), componentCount)) {
            return false;
        }
        for (size_t component = 0; component < expectedComponentCount; ++component) {
            if (!std::isfinite(it->points[component]) ||
                !std::isfinite(it->normals[component]) ||
                !std::isfinite(it->colors[component])) {
                return false;
            }
        }
    }
    return true;
}

struct GeometryBufferLayout {
    size_t vertexCount;
    size_t componentCount;
    size_t indexCount;

    GeometryBufferLayout() : vertexCount(0), componentCount(0), indexCount(0) {}
};

bool getLineBufferLayout(const std::vector<lineBufferObjectVP2>& buffers,
                         GeometryBufferLayout& output)
{
    GeometryBufferLayout layout;
    for (std::vector<lineBufferObjectVP2>::const_iterator it = buffers.begin();
         it != buffers.end(); ++it) {
        if (it->points.size() % 3U != 0) {
            return false;
        }
        for (size_t component = 0; component < it->points.size(); ++component) {
            if (!std::isfinite(it->points[component])) return false;
        }
        const size_t localVertexCount = it->points.size() / 3U;
        for (size_t index = 0; index < it->indexArray.size(); ++index) {
            if (it->indexArray[index] >= localVertexCount) {
                return false;
            }
        }
        if (!dommy::memory::checkedAdd(
                layout.vertexCount, localVertexCount, layout.vertexCount) ||
            !dommy::memory::checkedAdd(
                layout.componentCount, it->points.size(), layout.componentCount) ||
            !dommy::memory::checkedAdd(
                layout.indexCount, it->indexArray.size(), layout.indexCount)) {
            return false;
        }
    }
    if (layout.vertexCount > std::numeric_limits<unsigned int>::max() ||
        layout.indexCount > std::numeric_limits<unsigned int>::max()) {
        return false;
    }
    output = layout;
    return true;
}

bool getSurfaceBufferLayout(const std::vector<BufferObjectVP2>& buffers,
                            GeometryBufferLayout& output)
{
    GeometryBufferLayout layout;
    for (std::vector<BufferObjectVP2>::const_iterator it = buffers.begin();
         it != buffers.end(); ++it) {
        if (it->normals.size() != it->points.size()) {
            return false;
        }
        for (size_t vertex = 0; vertex < it->points.size(); ++vertex) {
            if (!dommy::maya::isFloatRepresentablePoint(it->points[vertex]) ||
                !dommy::maya::isFloatRepresentablePoint(it->normals[vertex])) return false;
        }
        for (size_t index = 0; index < it->indexArray.size(); ++index) {
            if (it->indexArray[index] >= it->points.size()) {
                return false;
            }
        }
        size_t localComponentCount = 0;
        if (!dommy::memory::checkedMultiply(
                it->points.size(), 3U, localComponentCount) ||
            !dommy::memory::checkedAdd(
                layout.vertexCount, it->points.size(), layout.vertexCount) ||
            !dommy::memory::checkedAdd(
                layout.componentCount, localComponentCount, layout.componentCount) ||
            !dommy::memory::checkedAdd(
                layout.indexCount, it->indexArray.size(), layout.indexCount)) {
            return false;
        }
    }
    if (layout.vertexCount > dommy::viewport::kMaxVdbViewportSurfaceVertexCount ||
        layout.indexCount > dommy::viewport::kMaxVdbViewportSurfaceIndexCount ||
        layout.vertexCount > std::numeric_limits<unsigned int>::max() ||
        layout.indexCount > std::numeric_limits<unsigned int>::max()) {
        return false;
    }
    output = layout;
    return true;
}

void copyLineComponents(const std::vector<lineBufferObjectVP2>& buffers, float* destination)
{
    size_t offset = 0;
    for (std::vector<lineBufferObjectVP2>::const_iterator it = buffers.begin();
         it != buffers.end(); ++it) {
        std::copy(it->points.begin(), it->points.end(), destination + offset);
        offset += it->points.size();
    }
}

void copyLineIndices(const std::vector<lineBufferObjectVP2>& buffers,
                     unsigned int* destination)
{
    size_t indexOffset = 0;
    size_t vertexOffset = 0;
    for (std::vector<lineBufferObjectVP2>::const_iterator it = buffers.begin();
         it != buffers.end(); ++it) {
        for (size_t index = 0; index < it->indexArray.size(); ++index) {
            destination[indexOffset++] = static_cast<unsigned int>(
                vertexOffset + it->indexArray[index]);
        }
        vertexOffset += it->points.size() / 3U;
    }
}

} // namespace

void* DOpenVDBDisplayVP2::creator() {
    return ::dommy::maya::createObject("DOpenVDBDisplayVP2", [&]() {
        return new DOpenVDBDisplayVP2();
    });
}

bool DOpenVDBDisplayVP2::isBounded() const {
    return true;
}

MBoundingBox DOpenVDBDisplayVP2::boundingBox() const try
{
    const MPlug vdbPlug(thisMObject(), aVdbInput);
    MStatus connectionStatus;
    const bool connected = vdbPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DOpenVDBDisplayVP2: unable to query input connection for bounds.");
        return defaultVdbBoundingBox();
    }
    if (!connected) return defaultVdbBoundingBox();
    MObject inputObject;
    const DOpenVDBData* inputVdb = nullptr;
    if (!dommy::maya::getPluginDataFromPlug(vdbPlug, DOpenVDBData::id, inputObject, inputVdb) ||
        !inputVdb || !inputVdb->hasValidData()) return defaultVdbBoundingBox();

    MString selectedNames;
    if (readPlugValue(MPlug(thisMObject(), aVdbSelectedGridNames), selectedNames) != MS::kSuccess)
        return defaultVdbBoundingBox();
    GridCPtrVec grids;
    getGrids(grids, *inputVdb, selectedNames.asChar());
    dommy::math::Bounds3d bounds;
    for (GridCPtrVecCIter grid = grids.begin(); grid != grids.end(); ++grid) {
        if (!*grid) return defaultVdbBoundingBox();
        openvdb::BBoxd world;
        if (!getActiveVoxelWorldBounds(**grid, world)) continue;
        if (!dommy::vdb::validGridBox(world)) return defaultVdbBoundingBox();
        float minimum[3], maximum[3];
        for (int axis = 0; axis < 3; ++axis) {
            if (!dommy::numeric::checkedFloatCast(world.min()[axis], minimum[axis]) ||
                !dommy::numeric::checkedFloatCast(world.max()[axis], maximum[axis]))
                return defaultVdbBoundingBox();
        }
        // Match the float-backed wire geometry without allocating it.
        if (!bounds.add(minimum[0], minimum[1], minimum[2]) ||
            !bounds.add(maximum[0], maximum[1], maximum[2]))
            return defaultVdbBoundingBox();
    }
    if (bounds.empty()) return defaultVdbBoundingBox();
    return MBoundingBox(MPoint(bounds.minimum(0), bounds.minimum(1), bounds.minimum(2)),
                        MPoint(bounds.maximum(0), bounds.maximum(1), bounds.maximum(2)));
}
catch (...) {
    dommy::maya::reportViewportCallbackException(
        "DOpenVDBDisplayVP2", "boundingBox", std::current_exception());
    return defaultVdbBoundingBox();
}

////////////////////////////////////////

MStatus DOpenVDBDisplayVP2::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aNodeInfo && plug != aVdbAllGridNames) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const DOpenVDBData* inputVdb = nullptr;
    const bool connected = MPlug(thisMObject(), aVdbInput).isConnected(&status);
    if (!status) return status;
    if (connected) {
        status = getValidInputVDB(aVdbInput, data, inputVdb);
        if (status != MS::kSuccess) return status;
    }
    if (plug == aVdbAllGridNames) {
        MDataHandle output = data.outputValue(aVdbAllGridNames, &status);
        if (status != MS::kSuccess) {
            return status;
        }
        output.set(inputVdb != nullptr ? MString(getGridNames(*inputVdb).c_str())
                                       : MString(""));
        return data.setClean(plug);
    }

    MString nodeInfo("DOpenVDB Info\n\tDOpenVDB is not connected.");
    if (inputVdb != nullptr) {

        MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &status);
        if (status != MS::kSuccess) {
            return status;
        }
        nodeInfo = buildVdbInfo(*inputVdb, selectionHandle.asString().asChar());
    }

    MDataHandle output = data.outputValue(aNodeInfo, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    output.set(nodeInfo);
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBDisplayVP2")

////////////////////////////////////////

void DOpenVDBDisplayVP2::draw(M3dView& /*view*/, const MDagPath& /*path*/,
                              M3dView::DisplayStyle /*style*/,
                              M3dView::DisplayStatus /*status*/) {}

////////////////////////////////////////

MStatus DOpenVDBDisplayVP2::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // Setup input / output attributes

    aVdbInput = tAttr.create("VDBInput", "input", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));
	
	
	// Setup UI attributes
	
	aVisualizeBBox = nAttr.create("ActiveValueBoundingBox", "bbox", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVisualizeBBox));
	
	
	aVisualizeInternalNodes = nAttr.create("InternalNodes", "inodes", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVisualizeInternalNodes));
	
	
	aVisualizeLeafNodes = nAttr.create("LeafNodes", "lnodes", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVisualizeLeafNodes));
	
	aVisualizeActiveTiles = nAttr.create("ActiveTiles", "tiles", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVisualizeActiveTiles));
	
	
	aVisualizeActiveVoxels = nAttr.create("ActiveVoxels", "voxels", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVisualizeActiveVoxels));
	
	aVisualizeSurface = nAttr.create("Surface", "surface", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVisualizeSurface));
	
	
	aIsovalue = nAttr.create("Isovalue", "iso", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsovalue));
    
    aIsClip = nAttr.create("IsClip", "isclp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsClip));
    
    MFnEnumAttribute clpaxAttr;
    aClipAxis = clpaxAttr.create("ClipAxis", "clax", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(clpaxAttr.addField("X-Axis",0));
    DOMMY_RETURN_IF_FAILURE(clpaxAttr.addField("Y-Axis",1));
    DOMMY_RETURN_IF_FAILURE(clpaxAttr.addField("Z-Axis",2));
    DOMMY_RETURN_IF_FAILURE(clpaxAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClipAxis));
    
    
    aClipPosition = nAttr.create("ClipPosition", "clpos", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aClipPosition));
    
    aClipWidth = nAttr.create("ClipWidth", "clw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.01));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.2));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aClipWidth));
    
    
    aLowDetail = nAttr.create("LowDetail", "ldtl", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLowDetail));
	
	aInvertSurface = nAttr.create("InvertSurface", "invsurf", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInvertSurface));
	
	aPointSize = nAttr.create("PointSize", "pts", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aPointSize));
	
	
	MFnEnumAttribute coltyAttr;
	aColorType = coltyAttr.create("ColorType", "colty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(coltyAttr.addField("Value",0));
	DOMMY_RETURN_IF_FAILURE(coltyAttr.addField("Normal",1));
	DOMMY_RETURN_IF_FAILURE(coltyAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aColorType));
	
	
	
    aDisplayVector = nAttr.create("DisplayVector", "dpyvec", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplayVector));
    
    aVectorScale = nAttr.create("VectorScale", "vecscl", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorScale));
    
    aIsDrawHeads = nAttr.create("IsDrawHeads", "isdwh", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDrawHeads));
    
    
    aVectorColor = nAttr.create("VectorColor", "vcol", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f, 1.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorColor));
	
	MFnStringData fnStringData;
	MObject defaultStringData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, defaultStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aNodeInfo));

	aVdbAllGridNames = tAttr.create("VdbAllGridNames", "allgrids", MFnData::kString, defaultStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbAllGridNames));
	
	
	aVdbSelectedGridNames = tAttr.create("VdbSelectedGridNames", "selectedgrids", MFnData::kString, defaultStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));
	
	// Setup dependencies
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbAllGridNames));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aNodeInfo));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aNodeInfo));

#pragma mark render attribute

    //common
    aiOpaque = nAttr.create("aiOpaque", "aiopq", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiOpaque));
    
    aiMatte = nAttr.create("aiMatte", "aimat", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiMatte));
    
    aiSelfShadows = nAttr.create("aiSelfShadows", "aisshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiSelfShadows));
    
    aiReceiveShadows = nAttr.create("aiReceiveShadows", "airshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiReceiveShadows));
    
    aSidedness = nAttr.create("Sidedness", "sdns", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSidedness));
    
    aInvertNormals = nAttr.create("InvertNormals", "invn", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvertNormals));
    
    aRayBias = nAttr.create("RayBias", "raibias", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1e-06));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRayBias));
    
    
    
    MFnEnumAttribute volModeAttr;
    aVolumeMode = volModeAttr.create("VolumeMode", "volmd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(volModeAttr.addField("Volume",0));
    DOMMY_RETURN_IF_FAILURE(volModeAttr.addField("Implicit Surface",1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolumeMode));
    
    
    
    //Volume
    aStepSize = nAttr.create("StepSize", "stsz", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStepSize));
    
    aStepScale = nAttr.create("StepScale", "stscl", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStepScale));
    
    aVolumePadding = nAttr.create("VolumePadding", "volpad", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolumePadding));
    
    aIsCompress = nAttr.create("IsCompress", "iscomp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCompress));
    
    
    
    //Implicit
    aMinBox = nAttr.create("MinBox", "minb", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f,0.0f,0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinBox));
    
    aMaxBox = nAttr.create("MaxBox", "mqxb", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f,0.0f,0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxBox));
    
    const MObject aFieldChannelDefaultData = fnStringData.create("surface", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFieldChannel = tAttr.create("FieldChannel", "fldchnl", MFnData::kString, aFieldChannelDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFieldChannel));
    
    
    
    //    MObject DOpenVDBDisplayVP2::aField;
    aSamples = nAttr.create("Samples", "smpls", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSamples));
    
    MFnEnumAttribute solverAttr;
    aSolver = solverAttr.create("Solver", "slvr", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(solverAttr.addField("Uniform",0));
    DOMMY_RETURN_IF_FAILURE(solverAttr.addField("Level Set",1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolver));
    
    
    
    
    
    //Grid
    
    
    const MObject aExportGridDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportGrid = tAttr.create("ExportGrid", "exgrd", MFnData::kString, aExportGridDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportGrid));
    
    const MObject aExportVelocityGridDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportVelocityGrid = tAttr.create("ExportVelocityGrid", "exvelgrd", MFnData::kString, aExportVelocityGridDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportVelocityGrid));
    
    aVelocityScale = nAttr.create("VelocityScale", "velscl", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityScale));
    
    aVelocityFps = nAttr.create("VelocityFps", "velfps", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(24.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityFps));
    
    aVelocityShutterStart = nAttr.create("VelocityShutterStart", "velshtst", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(-0.250f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityShutterStart));
    
    aVelocityShutterEnd = nAttr.create("VelocityShutterEnd", "velshted", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.250f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityShutterEnd));
    
    aVelocityThreshold = nAttr.create("VelocityThreshold", "velthld", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityThreshold));
    
    aVdbShader = nAttr.createColor("VdbShader", "vdbsh", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbShader));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DOpenVDBDisplayVP2", "initialize", std::current_exception());
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

DOpenVDBDisplayVP2Override::DOpenVDBDisplayVP2Override(const MObject& obj)
    : MHWRender::MPxGeometryOverride(obj), mSolidUIShader(nullptr), mVolumeUIShader(nullptr),
      mVectorUIShader(nullptr), bboxUIShader(nullptr), nodeUIShader(nullptr), leafUIShader(nullptr),
      tileUIShader(nullptr), mLocatorNode(obj), mMultiplierChanged(true), visBBox(false),
      visNode(false), visLeaf(false), visTile(false), visSurf(false), visPoint(false),
      visVector(false) {
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DOpenVDBDisplayVP2Override");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dDefaultMaterialShader, mSolidUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVFatPointShader, mVolumeUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, bboxUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, nodeUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, leafUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, tileUIShader);
}

DOpenVDBDisplayVP2Override::~DOpenVDBDisplayVP2Override() {
    dommy::viewport::StockShaderRelease shaderRelease("DOpenVDBDisplayVP2Override");
    shaderRelease.release(mSolidUIShader);
    shaderRelease.release(mVolumeUIShader);
    shaderRelease.release(mVectorUIShader);
    shaderRelease.release(bboxUIShader);
    shaderRelease.release(nodeUIShader);
    shaderRelease.release(leafUIShader);
    shaderRelease.release(tileUIShader);
    clearGeometryBuffers();
}

void DOpenVDBDisplayVP2Override::clearGeometryBuffers() {
    mBBoxBuffersVP2.clear();
    mNodeBuffersVP2.clear();
    mLeafBuffersVP2.clear();
    mTileBuffersVP2.clear();
    mPointBuffersVP2.clear();
    mVectorBuffersVP2.clear();
    mSurfaceBuffersVP2.clear();
}

void DOpenVDBDisplayVP2Override::clearViewportState() {
    clearGeometryBuffers();
    visBBox = visNode = visLeaf = visTile = visSurf = visPoint = visVector = false;
}

MHWRender::DrawAPI DOpenVDBDisplayVP2Override::supportedDrawAPIs() const {
    // this plugin supports both GL and DX
    return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}

const MString colorParameterName_ = "solidColor";
const MString pointSizeParameterName_ = "pointSize";
const MString volumeSurfaceItemName_ = "volumeSurface";
const MString volumeVoxelItemName_ = "volumeVoxel";

const MString bboxShaderItemName_ = "bboxShader";
const MString nodeShaderItemName_ = "inNodeShader";
const MString leafShaderItemName_ = "leafShader";
const MString tileShaderItemName_ = "tileShader";

void DOpenVDBDisplayVP2Override::updateDG() try
{
    mMultiplierChanged = true;
    clearViewportState();

    const MPlug plug(mLocatorNode, DOpenVDBDisplayVP2::aVdbInput);
    MObject obj; // Keep Maya's data object alive while using the non-owning view.
    const DOpenVDBData* inputVdb = nullptr;
    if (!dommy::maya::getPluginDataFromPlug(plug, DOpenVDBData::id, obj, inputVdb) ||
        !inputVdb || !inputVdb->hasValidData()) return;

    MString selectedNames;
    if (readPlugValue(MPlug(mLocatorNode, DOpenVDBDisplayVP2::aVdbSelectedGridNames),
        selectedNames) != MS::kSuccess) return;
    GridCPtrVec grids;
    getGrids(grids, *inputVdb, selectedNames.asChar());
    if (grids.empty()) return;

    VdbViewportSettings settings;
    if (readVdbViewportSettings(mLocatorNode, settings) != MS::kSuccess) {
        MGlobal::displayError("DVDBDisplayVP2: unable to read valid viewport settings.");
        return;
    }
    visBBox = settings.bbox;
    visNode = settings.node;
    visLeaf = settings.leaf;
    visTile = settings.tile;
    visSurf = settings.surface;
    visPoint = settings.point;
    visVector = settings.vector;
    mPointSize = settings.pointSize;
    mVectorScale = settings.vectorScale;
    mDrawHeads = settings.drawHeads;
    mVectorColor = settings.vectorColor;
    bool isClip = settings.clip;
    if (visBBox) {
        mBBoxBuffersVP2.clear();
        const size_t bboxGridCount = std::min<size_t>(
            grids.size(), dommy::viewport::kMaxVdbViewportWireBoxCount);
        mBBoxBuffersVP2.resize(bboxGridCount);

        for (size_t n = 0; n < bboxGridCount; ++n) {
            BoundingBoxGeoVP2 drawBBox(mBBoxBuffersVP2[n]);
            drawBBox(grids[n]);
        }

        mMultiplierChanged = true;
    } else {
        mBBoxBuffersVP2.clear();
    }

    if (visNode) {
        mNodeBuffersVP2.clear();
        mNodeBuffersVP2.resize(grids.size());
        dommy::viewport::GeometryBudget boxBudget(
            dommy::viewport::kMaxVdbViewportWireBoxCount);

        for (size_t n = 0, N = grids.size();
            n < N && boxBudget.remaining() != 0; ++n) {
            InternalNodesGeoVP2 drawNodes(
                mNodeBuffersVP2[n], boxBudget.remaining());
            if (processTypedGrid(grids[n], drawNodes)) {
                boxBudget.take(drawNodes.generatedBoxCount());
            }
        }
        mMultiplierChanged = true;
    } else {
        mNodeBuffersVP2.clear();
    }

    if (visLeaf) {
        mLeafBuffersVP2.clear();
        mLeafBuffersVP2.resize(grids.size());
        dommy::viewport::GeometryBudget boxBudget(
            dommy::viewport::kMaxVdbViewportWireBoxCount);

        for (size_t n = 0, N = grids.size();
            n < N && boxBudget.remaining() != 0; ++n) {
            LeafNodesGeoVP2 drawLeafs(
                mLeafBuffersVP2[n], boxBudget.remaining());
            if (processTypedGrid(grids[n], drawLeafs)) {
                boxBudget.take(drawLeafs.generatedBoxCount());
            }
        }

        mMultiplierChanged = true;
    } else {
        mLeafBuffersVP2.clear();
    }

    if (visTile) {
        mTileBuffersVP2.clear();
        mTileBuffersVP2.resize(grids.size());
        dommy::viewport::GeometryBudget boxBudget(
            dommy::viewport::kMaxVdbViewportWireBoxCount);

        for (size_t n = 0, N = grids.size();
            n < N && boxBudget.remaining() != 0; ++n) {
            ActiveTileGeoVP2 drawTiles(
                mTileBuffersVP2[n], boxBudget.remaining());
            if (processTypedGrid(grids[n], drawTiles)) {
                boxBudget.take(drawTiles.generatedBoxCount());
            }
        }

        mMultiplierChanged = true;
    } else {
        mTileBuffersVP2.clear();
    }

#pragma mark clip bbox
    openvdb::BBoxd bbox;
    if (isClip) {
        const float position = settings.clipPosition;
        const float width = settings.clipWidth;
        const short axis = settings.clipAxis;
        bool hasActiveBounds = false;
        if (!buildVdbViewportClipBox(grids, axis, position, width, bbox, hasActiveBounds)) {
            MGlobal::displayError("DVDBDisplayVP2: invalid viewport clip bounds or settings.");
            clearViewportState();
            mMultiplierChanged = true;
            return;
        }
        isClip = hasActiveBounds;
    }

#pragma mark mPointBuffersVP2
    if (visPoint || visVector) {

        mPointBuffersVP2.clear();
        mPointBuffersVP2.resize(grids.size());
        mVectorBuffersVP2.clear();
        mVectorBuffersVP2.resize(grids.size());

        const bool lowDetail = settings.lowDetail;
        const short colorType = settings.colorType;

        size_t remainingPointBudget = dommy::viewport::kMaxVdbViewportPointCount;
        size_t remainingSupportedGrids = 0;
        for (GridCPtrVecCIter it = grids.begin(); it != grids.end(); ++it) {
            if (!*it) {
                continue;
            }
            const DVdbGridType gridType = getVdbGridType(**it);
            if ((visPoint && gridType == DVdbGridType::kFloat) ||
                (visVector && (gridType == DVdbGridType::kVec3f || gridType == DVdbGridType::kVec3d))) {
                ++remainingSupportedGrids;
            }
        }

        size_t pIndex = 0;

        for (GridCPtrVecIter it = grids.begin(); it != grids.end(); ++it) {

            if (!*it) {
                ++pIndex;
                continue;
            }
            const openvdb::GridBase& gridRef = **it;
            const DVdbGridType gridType = getVdbGridType(gridRef);
            const bool isScalarGrid = gridType == DVdbGridType::kFloat;
            const bool isVectorGrid =
            gridType == DVdbGridType::kVec3f || gridType == DVdbGridType::kVec3d;
            if ((!isScalarGrid || !visPoint) && (!isVectorGrid || !visVector)) {
                ++pIndex;
                continue;
            }

            const size_t gridPointBudget = remainingSupportedGrids == 0
            ? 0
            : remainingPointBudget / remainingSupportedGrids +
            (remainingPointBudget % remainingSupportedGrids != 0 ? 1U : 0U);
            if (gridPointBudget == 0) {
                --remainingSupportedGrids;
                ++pIndex;
                continue;
            }
            size_t generatedPointCount = 0;

            if (isClip) {
                GridPtr outGrid;
                BBoxClipOp op(bbox, true);
                bool clipSucceeded = false;
                if (isScalarGrid) {

                    clipSucceeded = processVdbScalarGrid(gridType, gridRef, op);
                } else if (isVectorGrid) {

                    clipSucceeded = processVdbVectorGrid(gridType, gridRef, op);
                }

                outGrid = op.outputGrid;
                if (!clipSucceeded || !outGrid) {
                    --remainingSupportedGrids;
                    ++pIndex;
                    continue;
                }

                if (isScalarGrid) {
                    ActiveVoxelGeoVP2 drawVoxels(
                        mPointBuffersVP2[pIndex], lowDetail, colorType, gridPointBudget);
                    if (processTypedScalarGrid(outGrid, drawVoxels)) {
                        generatedPointCount = drawVoxels.generatedPointCount();
                    }
                } else if (isVectorGrid) {
                    ActiveVoxelVecVP2 vecVoxels(
                        mVectorBuffersVP2[pIndex], lowDetail, gridPointBudget);
                    if (processTypedVectorGrid(outGrid, vecVoxels)) {
                        generatedPointCount = vecVoxels.generatedPointCount();
                    }
                }

            } else {
                if (isScalarGrid) {
                    ActiveVoxelGeoVP2 drawVoxels(
                        mPointBuffersVP2[pIndex], lowDetail, colorType, gridPointBudget);
                    if (processTypedScalarGrid(grids[pIndex], drawVoxels)) {
                        generatedPointCount = drawVoxels.generatedPointCount();
                    }
                } else if (isVectorGrid) {
                    ActiveVoxelVecVP2 vecVoxels(
                        mVectorBuffersVP2[pIndex], lowDetail, gridPointBudget);
                    if (processTypedVectorGrid(grids[pIndex], vecVoxels)) {
                        generatedPointCount = vecVoxels.generatedPointCount();
                    }
                }
            }

            remainingPointBudget -= std::min(remainingPointBudget, generatedPointCount);
            --remainingSupportedGrids;
            ++pIndex;
        }

        mMultiplierChanged = true;
    }

#pragma mark DOpenVDBDisplayVP2
    if (visSurf) {

        const float iso = settings.isovalue;
        const bool inv = settings.invertSurface;

        mSurfaceBuffersVP2.clear();
        mSurfaceBuffersVP2.resize(grids.size());
        dommy::viewport::GeometryBudget vertexBudget(
            dommy::viewport::kMaxVdbViewportSurfaceVertexCount);
        dommy::viewport::GeometryBudget indexBudget(
            dommy::viewport::kMaxVdbViewportSurfaceIndexCount);

        for (size_t gridIndex = 0;
            gridIndex < grids.size() && vertexBudget.remaining() != 0 &&
            indexBudget.remaining() != 0;
            ++gridIndex) {
            if (!grids[gridIndex]) {
                continue;
            }
            const openvdb::GridBase& gridRef = *grids[gridIndex];
            const DVdbGridType gridType = getVdbGridType(gridRef);
            if (gridType != DVdbGridType::kFloat) {
                if (gridType == DVdbGridType::kVec3f || gridType == DVdbGridType::kVec3d) {
                    MGlobal::displayWarning("Surface display does not support vector grids.");
                }
                continue;
            }

            GridCPtr surfaceGrid = grids[gridIndex];
            GridPtr clippedGrid;
            if (isClip) {
                BBoxClipOp op(bbox, true);
                if (!processVdbScalarGrid(gridType, gridRef, op) || !op.outputGrid) {
                    continue;
                }
                clippedGrid = op.outputGrid;
                surfaceGrid = clippedGrid;
            }

            SurfaceGeoVP2 drawSurface(
                mSurfaceBuffersVP2[gridIndex], iso, inv,
                vertexBudget.remaining(), indexBudget.remaining());
            if (processTypedScalarGrid(surfaceGrid, drawSurface) && drawSurface.succeeded()) {
                vertexBudget.take(drawSurface.generatedVertexCount());
                indexBudget.take(drawSurface.generatedIndexCount());
            }
        }

        mMultiplierChanged = true;
    } else {
        mSurfaceBuffersVP2.clear();
    }
}
catch (...) {
    clearViewportState();
    mMultiplierChanged = true;
    dommy::maya::reportViewportCallbackException(
        "DOpenVDBDisplayVP2Override", "updateDG", std::current_exception());
}

const MString DVDBVecItemName_ = "DVDBVecItem";

void DOpenVDBDisplayVP2Override::updateRenderItems(const MDagPath& /*path*/,
                                                   MHWRender::MRenderItemList& list) try
{
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DOpenVDBDisplayVP2Override");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dDefaultMaterialShader, mSolidUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVFatPointShader, mVolumeUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, bboxUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, nodeUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, leafUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, tileUIShader);
    


    bool hasPointData = false;
    for (std::vector<valueBufferObjectVP2>::const_iterator it = mPointBuffersVP2.begin();
         it != mPointBuffersVP2.end(); ++it) {
        if (!it->indexArray.empty()) {
            hasPointData = true;
            break;
        }
    }
    bool hasVectorData = false;
    for (std::vector<valueBufferObjectVP2>::const_iterator it = mVectorBuffersVP2.begin();
         it != mVectorBuffersVP2.end(); ++it) {
        if (!it->indexArray.empty()) {
            hasVectorData = true;
            break;
        }
    }

#pragma mark volume shader
    MHWRender::MRenderItem* volumeItem = nullptr;

    int index = list.indexOf(volumeSurfaceItemName_);

    if (index < 0) {
        
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(volumeSurfaceItemName_,
													MHWRender::MRenderItem::DecorationItem,
													MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        volumeItem = pendingItem.release();
    } else {
        
        volumeItem = list.itemAt(index);
    }

    if(volumeItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*volumeItem, mShaderUpdateFailed);
		
        if (mSolidUIShader)
        {
			float diffColor[4] = { 0.8, 0.8, 0.8, 1.0f };
			shaderUpdate.setParameter(mSolidUIShader, "diffuseColor", diffColor);
			shaderUpdate.setParameter(mSolidUIShader, "specularPower", 0.0f);
            shaderUpdate.setShader(mSolidUIShader, &volumeSurfaceItemName_);
        }
        shaderUpdate.enable(visSurf);
    }
    
	
#pragma mark voxel shader
    
    MHWRender::MRenderItem* voxelItem = nullptr;
    
    index = list.indexOf(volumeVoxelItemName_);
    
    if (index < 0)
    {
        
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create( volumeVoxelItemName_,
                                                    MHWRender::MRenderItem::DecorationItem,
                                                    MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        voxelItem = pendingItem.release();
    }
    else
    {
        
        voxelItem = list.itemAt(index);
    }
    
    if(voxelItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*voxelItem, mShaderUpdateFailed);
        
        if (mVolumeUIShader)
        {
            
			float2 pSize = {mPointSize, mPointSize};
			shaderUpdate.setParameter(mVolumeUIShader, pointSizeParameterName_, pSize);
            shaderUpdate.setShader(mVolumeUIShader, &volumeVoxelItemName_);
        }
        shaderUpdate.enable(visPoint && hasPointData);
    }
	
#pragma mark vector shader
	
	MHWRender::MRenderItem* DVDBVectorItem = nullptr;
	
	index = list.indexOf(DVDBVecItemName_);
	
	if (index < 0)
	{
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DVDBVecItemName_,
														  MHWRender::MRenderItem::DecorationItem,
														  MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
		pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DVDBVectorItem = pendingItem.release();
	}
	else
	{
		DVDBVectorItem = list.itemAt(index);
	}
	
	if(DVDBVectorItem)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DVDBVectorItem, mShaderUpdateFailed);
		if (mVectorUIShader)
		{
            
			float wireframeColor[4] = {mVectorColor.r, mVectorColor.g, mVectorColor.b, 1.0f};
			shaderUpdate.setParameter(mVectorUIShader, "drawHeads", mDrawHeads);
			shaderUpdate.setParameter(mVectorUIShader, "velocityScale", mVectorScale);
			shaderUpdate.setParameter(mVectorUIShader, colorParameterName_, wireframeColor);
			shaderUpdate.setShader(mVectorUIShader, &DVDBVecItemName_);
		}
		shaderUpdate.enable(visVector && hasVectorData);
	}
	
	
#pragma mark BBox shader
	
	
	MHWRender::MRenderItem* bboxItem = nullptr;
	
	index = list.indexOf(bboxShaderItemName_);
	
	if (index < 0)
	{
		
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(
												   bboxShaderItemName_,
												   MHWRender::MRenderItem::DecorationItem,
												   MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
		pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        bboxItem = pendingItem.release();
	}
	else
	{
		
		bboxItem = list.itemAt(index);
	}
	
	if(bboxItem)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*bboxItem, mShaderUpdateFailed);
		
		if (bboxUIShader)
		{
			
			float  bboxColor[4] = { 0.045, 0.045, 0.045, 1.0f };
			shaderUpdate.setParameter(bboxUIShader, colorParameterName_, bboxColor);
			float2 lw = {1.0,1.0};
			shaderUpdate.setParameter(bboxUIShader, "lineWidth", lw);
			shaderUpdate.setShader(bboxUIShader, &bboxShaderItemName_);
		}
		shaderUpdate.enable(visBBox);
	}
	
	
	
#pragma mark node shader
	MHWRender::MRenderItem* inNodeItem = nullptr;
	
	index = list.indexOf(nodeShaderItemName_);
	
	if (index < 0)
	{
		
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(
												  nodeShaderItemName_,
												  MHWRender::MRenderItem::DecorationItem,
												  MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
		pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        inNodeItem = pendingItem.release();
	}
	else
	{
		
		inNodeItem = list.itemAt(index);
	}
	
	if(inNodeItem)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*inNodeItem, mShaderUpdateFailed);
		
		if (nodeUIShader)
		{
			
            float  nodeColor[4] = { 0.871, 0.394, 0.01916, 1.0f };
            shaderUpdate.setParameter(nodeUIShader, colorParameterName_, nodeColor);
			float2 lw = {1.0,1.0};
			shaderUpdate.setParameter(nodeUIShader, "lineWidth", lw);
			shaderUpdate.setShader(nodeUIShader, &nodeShaderItemName_);
		}
		shaderUpdate.enable(visNode);
	}
	
	
#pragma mark leaf shader
	
	MHWRender::MRenderItem* leafItem = nullptr;
	
	index = list.indexOf(leafShaderItemName_);
	
	if (index < 0)
	{
		
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(leafShaderItemName_,
													MHWRender::MRenderItem::DecorationItem,
													MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
		pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        leafItem = pendingItem.release();
	}
	else
	{
		
		leafItem = list.itemAt(index);
	}
	
	if(leafItem)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*leafItem, mShaderUpdateFailed);
		
		if (leafUIShader)
		{
			
            float  leafColor[4] = { 0.00608299, 0.279541, 0.625, 1.0f };
            shaderUpdate.setParameter(leafUIShader, colorParameterName_, leafColor);
			float2 lw = {1.0,1.0};
			shaderUpdate.setParameter(leafUIShader, "lineWidth", lw);
			shaderUpdate.setShader(leafUIShader, &leafShaderItemName_);
		}
		shaderUpdate.enable(visLeaf);
	}
    
#pragma mark tile shader
	
    MHWRender::MRenderItem* tileItem = nullptr;
    
    index = list.indexOf(tileShaderItemName_);
    
    if (index < 0)
    {
        
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(tileShaderItemName_,
                                                  MHWRender::MRenderItem::DecorationItem,
                                                  MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DOpenVDBDisplayVP2Override"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        tileItem = pendingItem.release();
    }
    else
    {
        
        tileItem = list.itemAt(index);
    }
    
    if(tileItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*tileItem, mShaderUpdateFailed);
        
        if (tileUIShader)
        {
            
            float  tileColor[4] = { 0.9, 0.3, 0.3, 1.0f };
            shaderUpdate.setParameter(tileUIShader, colorParameterName_, tileColor);
            float2 lw = {1.0,1.0};
            shaderUpdate.setParameter(tileUIShader, "lineWidth", lw);
            shaderUpdate.setShader(tileUIShader, &tileShaderItemName_);
        }
        shaderUpdate.enable(visTile);
    }
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH("DOpenVDBDisplayVP2Override", "updateRenderItems")

void DOpenVDBDisplayVP2Override::populateGeometry(
                                                 const MHWRender::MGeometryRequirements& requirements,
                                                 const MHWRender::MRenderItemList& renderItems,
                                                 MHWRender::MGeometry& data) try
{
    mMultiplierChanged = true;
	size_t totalPointCount = 0;
	size_t totalPointComponentCount = 0;
	size_t totalVectorCount = 0;
	size_t totalVectorComponentCount = 0;
	if (!getValueBufferLayout(
			mPointBuffersVP2, totalPointCount, totalPointComponentCount) ||
		!getValueBufferLayout(
			mVectorBuffersVP2, totalVectorCount, totalVectorComponentCount)) {
		return;
	}
	size_t totalPointColorComponentCount = 0;
	if (!dommy::memory::checkedMultiply(
			totalPointCount, 4U, totalPointColorComponentCount)) {
		return;
	}
	GeometryBufferLayout surfaceLayout;
	GeometryBufferLayout bboxLayout;
	GeometryBufferLayout nodeLayout;
	GeometryBufferLayout leafLayout;
	GeometryBufferLayout tileLayout;
	if (!getSurfaceBufferLayout(mSurfaceBuffersVP2, surfaceLayout) ||
		!getLineBufferLayout(mBBoxBuffersVP2, bboxLayout) ||
		!getLineBufferLayout(mNodeBuffersVP2, nodeLayout) ||
		!getLineBufferLayout(mLeafBuffersVP2, leafLayout) ||
		!getLineBufferLayout(mTileBuffersVP2, tileLayout)) {
		return;
	}
	
    if (surfaceLayout.vertexCount == 0U && totalPointCount == 0U &&
        totalVectorCount == 0U && bboxLayout.vertexCount == 0U &&
        nodeLayout.vertexCount == 0U && leafLayout.vertexCount == 0U &&
        tileLayout.vertexCount == 0U) {
        mMultiplierChanged = false;
        return;
    }

	//surface
    MHWRender::MVertexBuffer* verticesBuffer = nullptr;
    MHWRender::MVertexBuffer* normalBuffer = nullptr;
    float* vertices = nullptr;
    float* normals = nullptr;
	
	//voxel
    MHWRender::MVertexBuffer* voxelVerticesBuffer = nullptr;
	MHWRender::MVertexBuffer* voxelColorBuffer = nullptr;
	MHWRender::MVertexBuffer* voxelNormalBuffer = nullptr;
    float* voxelVertices = nullptr;
	float* voxelColors = nullptr;
	float* voxelNormals = nullptr;
	
	//vector
	MHWRender::MVertexBuffer* vecPtBuffer = nullptr;
	MHWRender::MVertexBuffer* vecBuffer = nullptr;
	float* vecPts = nullptr;
	float* vectorvalues = nullptr;
	
	
	
	//bbox
	MHWRender::MVertexBuffer* bboxVerticesBuffer = nullptr;
	float* bboxVertices = nullptr;
	
	//node
	MHWRender::MVertexBuffer* nodeVerticesBuffer = nullptr;
	float* nodeVertices = nullptr;
	
	//leaf
	MHWRender::MVertexBuffer* leafVerticesBuffer = nullptr;
	float* leafVertices = nullptr;
    
    //tile
    MHWRender::MVertexBuffer* tileVerticesBuffer = nullptr;
    float* tileVertices = nullptr;
	
    if (requirements.vertexRequirements().length() == 0) {
        mMultiplierChanged = false;
        return;
    }

    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic())
            {
                case MHWRender::MGeometry::kPosition:
                {

                    if (vertexBufferDescriptor.name() == volumeSurfaceItemName_ && surfaceLayout.vertexCount != 0)
                    {
                        if (!verticesBuffer)
                        {

                            verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!verticesBuffer) return false;
                            if (verticesBuffer)
                            {

                                vertices = dommy::viewport::acquireFloatBuffer(
                                    verticesBuffer, vertexBufferDescriptor,
                                    surfaceLayout.componentCount, 3);
                                if (!vertices) return false;
                            }
                        }
                    } else if(vertexBufferDescriptor.name() == volumeVoxelItemName_ && totalPointCount != 0) {
                        if (!voxelVerticesBuffer)
                        {
                            voxelVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!voxelVerticesBuffer) return false;
                            if (voxelVerticesBuffer)
                            {

                                voxelVertices = dommy::viewport::acquireFloatBuffer(
                                    voxelVerticesBuffer, vertexBufferDescriptor,
                                    totalPointComponentCount, 3);
                                if (!voxelVertices) return false;
                            }
                        }
                    } else if(vertexBufferDescriptor.name() == bboxShaderItemName_ && bboxLayout.vertexCount != 0) {
                        if (!bboxVerticesBuffer)
                        {
                            bboxVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!bboxVerticesBuffer) return false;
                            if (bboxVerticesBuffer)
                            {

                                bboxVertices = dommy::viewport::acquireFloatBuffer(
                                    bboxVerticesBuffer, vertexBufferDescriptor,
                                    bboxLayout.componentCount, 3);
                                if (!bboxVertices) return false;
                            }
                        }
                    } else if(vertexBufferDescriptor.name() == nodeShaderItemName_ && nodeLayout.vertexCount != 0) {
                        if (!nodeVerticesBuffer)
                        {
                            nodeVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!nodeVerticesBuffer) return false;
                            if (nodeVerticesBuffer)
                            {

                                nodeVertices = dommy::viewport::acquireFloatBuffer(
                                    nodeVerticesBuffer, vertexBufferDescriptor,
                                    nodeLayout.componentCount, 3);
                                if (!nodeVertices) return false;
                            }
                        }
                    } else if(vertexBufferDescriptor.name() == leafShaderItemName_ && leafLayout.vertexCount != 0) {
                        if (!leafVerticesBuffer)
                        {
                            leafVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!leafVerticesBuffer) return false;
                            if (leafVerticesBuffer)
                            {

                                leafVertices = dommy::viewport::acquireFloatBuffer(
                                    leafVerticesBuffer, vertexBufferDescriptor,
                                    leafLayout.componentCount, 3);
                                if (!leafVertices) return false;
                            }
                        }
                    } else if(vertexBufferDescriptor.name() == tileShaderItemName_ && tileLayout.vertexCount != 0) {
                        if (!tileVerticesBuffer)
                        {
                            tileVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!tileVerticesBuffer) return false;
                            if (tileVerticesBuffer)
                            {

                                tileVertices = dommy::viewport::acquireFloatBuffer(
                                    tileVerticesBuffer, vertexBufferDescriptor,
                                    tileLayout.componentCount, 3);
                                if (!tileVertices) return false;
                            }
                        }
                    } else if (vertexBufferDescriptor.name() == DVDBVecItemName_ && totalVectorCount != 0) {
                        if (!vecPtBuffer)
                        {
                            vecPtBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vecPtBuffer) return false;
                            if (vecPtBuffer)
                            {
                                vecPts = dommy::viewport::acquireFloatBuffer(
                                    vecPtBuffer, vertexBufferDescriptor,
                                    totalVectorComponentCount, 3);
                                if (!vecPts) return false;
                            }
                        }
                    }

                }
                    break;
                case MHWRender::MGeometry::kTexture:

                    if (vertexBufferDescriptor.name() == DVDBVecItemName_ && totalVectorCount != 0) {

                        if (!vecBuffer)
                        {
                            vecBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vecBuffer) return false;
                            if (vecBuffer)
                            {
                                vectorvalues = dommy::viewport::acquireFloatBuffer(
                                    vecBuffer, vertexBufferDescriptor,
                                    totalVectorComponentCount, 3);
                                if (!vectorvalues) return false;
                            }
                        }
                    }
                    break;
                case MHWRender::MGeometry::kNormal:
                {

                    if (vertexBufferDescriptor.name() == volumeSurfaceItemName_ && surfaceLayout.vertexCount != 0)
                    {
                        if (!normalBuffer)
                        {
                            normalBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!normalBuffer) return false;
                            if (normalBuffer)
                            {

                                normals = dommy::viewport::acquireFloatBuffer(
                                    normalBuffer, vertexBufferDescriptor,
                                    surfaceLayout.componentCount, 3);
                                if (!normals) return false;
                            }
                        }
                    }
                    else if (vertexBufferDescriptor.name() == volumeVoxelItemName_ && totalPointCount != 0)
                    {
                        if (!voxelNormalBuffer)
                        {
                            voxelNormalBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!voxelNormalBuffer) return false;
                            if (voxelNormalBuffer)
                            {

                                voxelNormals = dommy::viewport::acquireFloatBuffer(
                                    voxelNormalBuffer, vertexBufferDescriptor,
                                    totalPointComponentCount, 3);
                                if (!voxelNormals) return false;
                            }
                        }
                    }

                }
                    break;
                case MHWRender::MGeometry::kColor:
                {

                    if (vertexBufferDescriptor.name() == volumeVoxelItemName_ && totalPointCount != 0)
                    {
                        if (!voxelColorBuffer)
                        {
                            voxelColorBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!voxelColorBuffer) return false;
                            if (voxelColorBuffer)
                            {

                                voxelColors = dommy::viewport::acquireFloatBuffer(
                                    voxelColorBuffer, vertexBufferDescriptor,
                                    totalPointColorComponentCount, 4);
                                if (!voxelColors) return false;
                            }
                        }
                    }
                }
                    break;
                default:
                    // This stream semantic has no project buffer.
                    break;
            }
            return true;
        });
    if (!streamsReady) return;
		
		
		
		if (mSurfaceBuffersVP2.size()) {
			
            size_t verticesPointerOffset = 0;
            size_t normalPointerOffset = 0;
            for (std::vector<BufferObjectVP2>::iterator it=mSurfaceBuffersVP2.begin(); it!=mSurfaceBuffersVP2.end(); ++it) {
                
                
                for (uint currentVertex = 0 ; currentVertex < it->points.size(); ++currentVertex)
                {
                    if(vertices)
                    {
                        vertices[verticesPointerOffset++] = it->points[currentVertex].x;
                        vertices[verticesPointerOffset++] = it->points[currentVertex].y;
                        vertices[verticesPointerOffset++] = it->points[currentVertex].z;
                    }
                    if (normals) {
                        normals[normalPointerOffset++] = it->normals[currentVertex].x;
                        normals[normalPointerOffset++] = it->normals[currentVertex].y;
                        normals[normalPointerOffset++] = it->normals[currentVertex].z;
                    }
                }
            }
            
			if(verticesBuffer && vertices)
			{
				verticesBuffer->commit(vertices);
			}
			
			if(normalBuffer && normals)
			{
				normalBuffer->commit(normals);
			}
		}
		
		
		
        //Voxel vertext data
        if (!mPointBuffersVP2.empty() || !mVectorBuffersVP2.empty()) {
			// Point positions and normals use packed three-component layouts.
			if (voxelVertices) {
				size_t componentOffset = 0;
				for (std::vector<valueBufferObjectVP2>::const_iterator it = mPointBuffersVP2.begin();
					 it != mPointBuffersVP2.end(); ++it) {
					for (size_t component = 0; component < it->points.size(); ++component) {
						voxelVertices[componentOffset++] = it->points[component];
					}
				}
				
				
				if(voxelVerticesBuffer && voxelVertices)
				{
					voxelVerticesBuffer->commit(voxelVertices);
				}
			}
			
			if(voxelNormals) {
				size_t componentOffset = 0;
				for (std::vector<valueBufferObjectVP2>::const_iterator it = mPointBuffersVP2.begin();
					 it != mPointBuffersVP2.end(); ++it) {
					for (size_t component = 0; component < it->normals.size(); ++component) {
						voxelNormals[componentOffset++] = it->normals[component];
					}
				}
				
				
				if(voxelNormalBuffer && voxelNormals)
				{
					voxelNormalBuffer->commit(voxelNormals);
				}
			}
			
			size_t colorPointerOffset = 0;
			if (voxelColors) {
				for (std::vector<valueBufferObjectVP2>::const_iterator it = mPointBuffersVP2.begin();
					 it != mPointBuffersVP2.end(); ++it) {
					for (size_t component = 0; component < it->colors.size();) {
						voxelColors[colorPointerOffset++] = it->colors[component++];
						voxelColors[colorPointerOffset++] = it->colors[component++];
						voxelColors[colorPointerOffset++] = it->colors[component++];
						voxelColors[colorPointerOffset++] = 1.0f;
					}
				}
				
				
				if(voxelColorBuffer && voxelColors)
				{
					voxelColorBuffer->commit(voxelColors);
				}
			}
			
			
			//vector
			if (vecPts) {
				size_t componentOffset = 0;
				for (std::vector<valueBufferObjectVP2>::const_iterator it = mVectorBuffersVP2.begin();
					 it != mVectorBuffersVP2.end(); ++it) {
					for (size_t component = 0; component < it->points.size(); ++component) {
						vecPts[componentOffset++] = it->points[component];
					}
				}
				
				
				if(vecPtBuffer && vecPts)
				{
					vecPtBuffer->commit(vecPts);
				}
			}
			
			if(vectorvalues) {
				size_t componentOffset = 0;
				for (std::vector<valueBufferObjectVP2>::const_iterator it = mVectorBuffersVP2.begin();
					 it != mVectorBuffersVP2.end(); ++it) {
					for (size_t component = 0; component < it->normals.size(); ++component) {
						vectorvalues[componentOffset++] = it->normals[component];
					}
				}
				
				
				if(vecBuffer && vectorvalues)
				{
					vecBuffer->commit(vectorvalues);
				}
			}
        }
		
		// Bounding-box and topology wire data are packed across every selected grid.
		if (bboxVertices) {
			copyLineComponents(mBBoxBuffersVP2, bboxVertices);
				
				if(bboxVerticesBuffer) {
					bboxVerticesBuffer->commit(bboxVertices);
				}
		}
		
		if (nodeVertices) {
			copyLineComponents(mNodeBuffersVP2, nodeVertices);
				
				if(nodeVerticesBuffer) {
					nodeVerticesBuffer->commit(nodeVertices);
				}
		}

		if (leafVertices) {
			copyLineComponents(mLeafBuffersVP2, leafVertices);
				
				if(leafVerticesBuffer) {
					leafVerticesBuffer->commit(leafVertices);
				}
		}
		
        if (tileVertices) {
            copyLineComponents(mTileBuffersVP2, tileVertices);
                
                if(tileVerticesBuffer) {
                    tileVerticesBuffer->commit(tileVertices);
                }
        }
		
		
		
        
        for (int i=0; i < renderItems.length(); ++i)
        {
            const MHWRender::MRenderItem* item = renderItems.itemAt(i);
            
            if (!item)
            {
                continue;
            }
            
			
            // Number of index to generate (for line strip, or triangle list)
            size_t numIndex = 0;
			
			
            if (item->name() == volumeSurfaceItemName_ && surfaceLayout.indexCount != 0)
            {
                
                numIndex = surfaceLayout.indexCount;
                
            } else if (item->name() == volumeVoxelItemName_ && mPointBuffersVP2.size()) {
				
					numIndex = totalPointCount;
			} else if (item->name() == DVDBVecItemName_ && mVectorBuffersVP2.size()) {//vector
				
					numIndex = totalVectorCount;
				
			} else if (item->name() == bboxShaderItemName_) {
				numIndex = bboxLayout.indexCount;
			} else if (item->name() == nodeShaderItemName_) {
				numIndex = nodeLayout.indexCount;
			} else if (item->name() == leafShaderItemName_) {
				numIndex = leafLayout.indexCount;
			} else if (item->name() == tileShaderItemName_) {
				numIndex = tileLayout.indexCount;
            }
			
			
            
            
            
            if (numIndex)
            {
	                unsigned int* indices = nullptr;
	                MHWRender::MIndexBuffer* indexBuffer =
	                    dommy::viewport::createIndexBuffer(
	                        data, numIndex, indices);
	                if (!indexBuffer) return;
                
                if (item->name() == volumeSurfaceItemName_ && surfaceLayout.indexCount != 0) {
                    size_t indexOffset = 0;
                    size_t totalOffset = 0;
                    for (std::vector<BufferObjectVP2>::iterator it=mSurfaceBuffersVP2.begin(); it!=mSurfaceBuffersVP2.end(); ++it) {
                        for(size_t j = 0; j < it->indexArray.size(); ++j) {
                            indices[totalOffset++] = static_cast<unsigned int>(
                                it->indexArray[j] + indexOffset);
                        }
                        indexOffset+=it->points.size();
                    }
                } else if (item->name() == volumeVoxelItemName_ && mPointBuffersVP2.size()) {
                    size_t pointOffset = 0;
                    size_t indexOffset = 0;
                    for (std::vector<valueBufferObjectVP2>::const_iterator it = mPointBuffersVP2.begin();
                         it != mPointBuffersVP2.end(); ++it) {
                        for (size_t index = 0; index < it->indexArray.size(); ++index) {
                            indices[indexOffset++] = static_cast<unsigned int>(
                                pointOffset + it->indexArray[index]);
                        }
                        pointOffset += it->indexArray.size();
                    }
                    
				} else if (item->name() == DVDBVecItemName_ && mVectorBuffersVP2.size()) {
					size_t pointOffset = 0;
					size_t indexOffset = 0;
					for (std::vector<valueBufferObjectVP2>::const_iterator it = mVectorBuffersVP2.begin();
						 it != mVectorBuffersVP2.end(); ++it) {
						for (size_t index = 0; index < it->indexArray.size(); ++index) {
							indices[indexOffset++] = static_cast<unsigned int>(
								pointOffset + it->indexArray[index]);
						}
						pointOffset += it->indexArray.size();
					}
					
				} else if (item->name() == bboxShaderItemName_) {
					copyLineIndices(mBBoxBuffersVP2, indices);
                } else if (item->name() == nodeShaderItemName_) {
                    copyLineIndices(mNodeBuffersVP2, indices);
                } else if (item->name() == leafShaderItemName_) {
                    copyLineIndices(mLeafBuffersVP2, indices);
                } else if (item->name() == tileShaderItemName_) {
                    copyLineIndices(mTileBuffersVP2, indices);
                }
                indexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(indexBuffer)) return;
            }
        }
    

    mMultiplierChanged = false;
    

}
DOMMY_VIEWPORT_VOID_CATCH("DOpenVDBDisplayVP2Override", "populateGeometry")

void DOpenVDBDisplayVP2Override::cleanUp() {
    clearGeometryBuffers();
}
