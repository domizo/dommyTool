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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_maya/maya/OpenVDBFromMayaFluidNode.cc

//
//  DOpenVDBFromFluid.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/17.
//
//

#include "DOpenVDBFromFluid.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"



#include <openvdb/tools/Dense.h>
#include <openvdb/math/Transform.h>

#include <cmath>
#include <cstddef>
#include <string>

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DOpenVDBDataHeaders.h"
#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DMayaValueValidation.h"
#include "../Common/DVDBComputeGuard.h"




MObject DOpenVDBFromFluid::aInFluid;
MObject DOpenVDBFromFluid::aVdbOutput;

MObject DOpenVDBFromFluid::aDensity;
MObject DOpenVDBFromFluid::aDensityName;
MObject DOpenVDBFromFluid::aTemperature;
MObject DOpenVDBFromFluid::aTemperatureName;
MObject DOpenVDBFromFluid::aPressure;
MObject DOpenVDBFromFluid::aPressureName;
MObject DOpenVDBFromFluid::aFuel;
MObject DOpenVDBFromFluid::aFuelName;
MObject DOpenVDBFromFluid::aFalloff;
MObject DOpenVDBFromFluid::aFalloffName;
MObject DOpenVDBFromFluid::aVelocity;
MObject DOpenVDBFromFluid::aVelocityName;
MObject DOpenVDBFromFluid::aColors;
MObject DOpenVDBFromFluid::aColorsName;
MObject DOpenVDBFromFluid::aCoordinates;
MObject DOpenVDBFromFluid::aCoordinatesName;
MObject DOpenVDBFromFluid::aTime;



const MTypeId DOpenVDBFromFluid::typeId( ID_VDB_DOPENVDB_FROM_FLUID );

void* DOpenVDBFromFluid::creator()
{
    return ::dommy::maya::createObject("DOpenVDBFromFluid", [&]() {
        return new DOpenVDBFromFluid();
    });
}


namespace {

openvdb::math::Transform::Ptr createTransform(
    const dommy::fluid::GridShape& shape,
    const double cellSize[3],
    const MMatrix& objectToWorld)
{
    const openvdb::Mat4R localToObject(
        cellSize[0], 0.0,         0.0,         0.0,
        0.0,         cellSize[1], 0.0,         0.0,
        0.0,         0.0,         cellSize[2], 0.0,
        shape.origin[0], shape.origin[1], shape.origin[2], 1.0);
    const openvdb::Mat4R objectToWorldMatrix(
        objectToWorld(0, 0), objectToWorld(0, 1),
        objectToWorld(0, 2), objectToWorld(0, 3),
        objectToWorld(1, 0), objectToWorld(1, 1),
        objectToWorld(1, 2), objectToWorld(1, 3),
        objectToWorld(2, 0), objectToWorld(2, 1),
        objectToWorld(2, 2), objectToWorld(2, 3),
        objectToWorld(3, 0), objectToWorld(3, 1),
        objectToWorld(3, 2), objectToWorld(3, 3));
    return openvdb::math::Transform::createLinearTransform(
        localToObject * objectToWorldMatrix);
}

openvdb::CoordBBox createBounds(const dommy::fluid::GridShape& shape)
{
    return openvdb::CoordBBox(
        openvdb::Coord(0),
        openvdb::Coord(
            static_cast<int>(shape.dimensions[0] - 1U),
            static_cast<int>(shape.dimensions[1] - 1U),
            static_cast<int>(shape.dimensions[2] - 1U)));
}

template<typename ValueType>
bool copyGrid(DOpenVDBData& vdb,
              const std::string& name,
              const openvdb::math::Transform& transform,
              const ValueType* values,
              const dommy::fluid::GridShape& shape,
              ValueType background,
              ValueType tolerance)
{
    if (values == NULL) return false;

    openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(background);
    const openvdb::CoordBBox bounds = createBounds(shape);
    openvdb::tools::Dense<const ValueType, openvdb::tools::LayoutXYZ> dense(
        bounds, values);
    openvdb::tools::copyFromDense(dense, grid->tree(), tolerance);
    grid->setName(name);
    grid->setTransform(transform.copy());
    vdb.insert(grid);
    return true;
}

bool readBoolean(MDataBlock& data, const MObject& attribute, bool& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) return false;
    value = handle.asBool();
    return true;
}

bool readString(MDataBlock& data,
                const MObject& attribute,
                std::string& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) return false;
    value = handle.asString().asChar();
    return true;
}

} // namespace


MStatus DOpenVDBFromFluid::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != aVdbOutput) return MS::kUnknownParameter;

    MStatus status;
    data.inputValue(aTime, &status);
    if (!status) return status;

    MPlug messagePlug(thisMObject(), aInFluid);
    MPlug fluidPlugSource = messagePlug.source(&status);
    if (!status || fluidPlugSource.isNull()) {
        MGlobal::displayError("DOpenVDBFromFluid requires a connected fluid.");
        return MS::kFailure;
    }
    MObject fluidNode = fluidPlugSource.node(&status);
    if (!status || fluidNode.isNull() || !fluidNode.hasFn(MFn::kFluid)) {
        MGlobal::displayError("DOpenVDBFromFluid input is not a valid fluid.");
        return MS::kFailure;
    }

    MFnFluid fluid(fluidNode, &status);
    if (!status || fluid.object().isNull()) return MS::kFailure;

    bool exportDensity = false;
    bool exportTemperature = false;
    bool exportPressure = false;
    bool exportFuel = false;
    bool exportFalloff = false;
    bool exportVelocity = false;
    bool exportColors = false;
    bool exportCoordinates = false;
    if (!readBoolean(data, aDensity, exportDensity) ||
        !readBoolean(data, aTemperature, exportTemperature) ||
        !readBoolean(data, aPressure, exportPressure) ||
        !readBoolean(data, aFuel, exportFuel) ||
        !readBoolean(data, aFalloff, exportFalloff) ||
        !readBoolean(data, aVelocity, exportVelocity) ||
        !readBoolean(data, aColors, exportColors) ||
        !readBoolean(data, aCoordinates, exportCoordinates)) {
        return MS::kFailure;
    }

    const std::size_t scalarGridCount =
        static_cast<std::size_t>(exportDensity) +
        static_cast<std::size_t>(exportTemperature) +
        static_cast<std::size_t>(exportPressure) +
        static_cast<std::size_t>(exportFuel) +
        static_cast<std::size_t>(exportFalloff) +
        (exportColors ? 3U : 0U) +
        (exportCoordinates ? 3U : 0U);

    unsigned int xResolution = 0U;
    unsigned int yResolution = 0U;
    unsigned int zResolution = 0U;
    status = fluid.getResolution(xResolution, yResolution, zResolution);
    if (!status) return status;

    double xDimension = 0.0;
    double yDimension = 0.0;
    double zDimension = 0.0;
    status = fluid.getDimensions(xDimension, yDimension, zDimension);
    if (!status) return status;

    const MBoundingBox fluidBounds = fluid.boundingBox(&status);
    if (!status) return status;

    dommy::fluid::GridPlan gridPlan;
    if (!dommy::fluid::buildGridPlan(
            xResolution, yResolution, zResolution,
            xDimension, yDimension, zDimension,
            fluidBounds.min().x, fluidBounds.min().y, fluidBounds.min().z,
            scalarGridCount, exportVelocity,
            dommy::generation::kMaxWorkItemCount, gridPlan)) {
        MGlobal::displayError(
            "Fluid dimensions, resolution, or selected channels exceed the VDB work budget.");
        return MS::kFailure;
    }

    const unsigned int scalarGridSize = fluid.gridSize(&status);
    if (!status || static_cast<std::size_t>(scalarGridSize) !=
                       gridPlan.scalar.voxelCount) {
        MGlobal::displayError("Fluid scalar-grid size does not match its resolution.");
        return MS::kFailure;
    }

    MDagPath fluidPath;
    status = fluid.getPath(fluidPath);
    if (!status || !fluidPath.isValid()) return MS::kFailure;
    const MMatrix objectToWorld = fluidPath.inclusiveMatrix(&status);
    const double transformDeterminant = objectToWorld.det4x4();
    if (!status || !dommy::maya::isFiniteMatrix(objectToWorld) ||
        !std::isfinite(transformDeterminant) || transformDeterminant == 0.0) {
        MGlobal::displayError("Fluid world transform must be finite and invertible.");
        return MS::kFailure;
    }

    const openvdb::math::Transform::Ptr scalarTransform = createTransform(
        gridPlan.scalar, gridPlan.cellSize, objectToWorld);
    openvdb::math::Transform::Ptr velocityTransforms[3];
    if (exportVelocity) {
        for (int component = 0; component < 3; ++component) {
            velocityTransforms[component] = createTransform(
                gridPlan.velocity[component], gridPlan.cellSize, objectToWorld);
        }
    }

    MFnPluginData pluginData;
    pluginData.create(DOpenVDBData::id, &status);
    if (!status) return status;
    DOpenVDBData* const vdb =
        dommy::maya::pluginDataPointer<DOpenVDBData>(pluginData, &status);
    if (!status || vdb == NULL) return MS::kFailure;

    std::string gridName;
    const float background = 0.0f;
    const float tolerance = 1.0e-7f;

    if (exportDensity) {
        const float* values = fluid.density(&status);
        if (!status || !readString(data, aDensityName, gridName) ||
            !copyGrid(*vdb, gridName, *scalarTransform, values,
                      gridPlan.scalar, background, tolerance)) {
            MGlobal::displayError("Unable to read the fluid density grid.");
            return MS::kFailure;
        }
    }
    if (exportTemperature) {
        const float* values = fluid.temperature(&status);
        if (!status || !readString(data, aTemperatureName, gridName) ||
            !copyGrid(*vdb, gridName, *scalarTransform, values,
                      gridPlan.scalar, background, tolerance)) {
            MGlobal::displayError("Unable to read the fluid temperature grid.");
            return MS::kFailure;
        }
    }
    if (exportPressure) {
        const float* values = fluid.pressure(&status);
        if (!status || !readString(data, aPressureName, gridName) ||
            !copyGrid(*vdb, gridName, *scalarTransform, values,
                      gridPlan.scalar, background, tolerance)) {
            MGlobal::displayError("Unable to read the fluid pressure grid.");
            return MS::kFailure;
        }
    }
    if (exportFuel) {
        const float* values = fluid.fuel(&status);
        if (!status || !readString(data, aFuelName, gridName) ||
            !copyGrid(*vdb, gridName, *scalarTransform, values,
                      gridPlan.scalar, background, tolerance)) {
            MGlobal::displayError("Unable to read the fluid fuel grid.");
            return MS::kFailure;
        }
    }
    if (exportFalloff) {
        const float* values = fluid.falloff(&status);
        if (!status || !readString(data, aFalloffName, gridName) ||
            !copyGrid(*vdb, gridName, *scalarTransform, values,
                      gridPlan.scalar, background, tolerance)) {
            MGlobal::displayError("Unable to read the fluid falloff grid.");
            return MS::kFailure;
        }
    }

    if (exportVelocity) {
        float* velocityValues[3] = {NULL, NULL, NULL};
        status = fluid.getVelocity(
            velocityValues[0], velocityValues[1], velocityValues[2]);
        int velocitySizes[3] = {0, 0, 0};
        if (!status ||
            fluid.velocityGridSizes(
                velocitySizes[0], velocitySizes[1], velocitySizes[2]) !=
                MS::kSuccess ||
            !readString(data, aVelocityName, gridName)) {
            MGlobal::displayError("Unable to read the fluid velocity grids.");
            return MS::kFailure;
        }

        const char* suffixes[3] = {".x", ".y", ".z"};
        for (int component = 0; component < 3; ++component) {
            if (velocitySizes[component] < 0 ||
                static_cast<std::size_t>(velocitySizes[component]) !=
                    gridPlan.velocity[component].voxelCount ||
                !copyGrid(*vdb, gridName + suffixes[component],
                          *velocityTransforms[component],
                          velocityValues[component],
                          gridPlan.velocity[component], background, tolerance)) {
                MGlobal::displayError(
                    "Fluid velocity-grid size does not match its staggered resolution.");
                return MS::kFailure;
            }
        }
    }

    if (exportColors) {
        float* colorValues[3] = {NULL, NULL, NULL};
        status = fluid.getColors(
            colorValues[0], colorValues[1], colorValues[2]);
        if (!status || !readString(data, aColorsName, gridName)) {
            MGlobal::displayError("Unable to read the fluid color grids.");
            return MS::kFailure;
        }
        const char* suffixes[3] = {".r", ".g", ".b"};
        for (int component = 0; component < 3; ++component) {
            if (!copyGrid(*vdb, gridName + suffixes[component],
                          *scalarTransform, colorValues[component],
                          gridPlan.scalar, background, tolerance)) {
                MGlobal::displayError("Unable to read a fluid color component.");
                return MS::kFailure;
            }
        }
    }

    if (exportCoordinates) {
        float* coordinateValues[3] = {NULL, NULL, NULL};
        status = fluid.getCoordinates(
            coordinateValues[0], coordinateValues[1], coordinateValues[2]);
        if (!status || !readString(data, aCoordinatesName, gridName)) {
            MGlobal::displayError("Unable to read the fluid coordinate grids.");
            return MS::kFailure;
        }
        const char* suffixes[3] = {".u", ".v", ".w"};
        for (int component = 0; component < 3; ++component) {
            if (!copyGrid(*vdb, gridName + suffixes[component],
                          *scalarTransform, coordinateValues[component],
                          gridPlan.scalar, background, tolerance)) {
                MGlobal::displayError(
                    "Unable to read a fluid coordinate component.");
                return MS::kFailure;
            }
        }
    }

    MDataHandle outHandle = data.outputValue(aVdbOutput, &status);
    if (!status) return status;
    status = dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
        outHandle, pluginData.object(), DOpenVDBData::id);
    if (!status) return status;
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFromFluid")



MStatus DOpenVDBFromFluid::initialize()
try {
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;
    MFnMessageAttribute msAttr;
    
    aInFluid = msAttr.create( "InFluid", "infld",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(msAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(msAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInFluid));
    
    
    aDensity = nAttr.create("Density", "d", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aDensity);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aDensityNameDefaultData = strData.create("density", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDensityName = tAttr.create("DensityName", "dname", MFnData::kString, aDensityNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aDensityName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aTemperature = nAttr.create("Temperature", "t", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aTemperature);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aTemperatureNameDefaultData = strData.create("temperature", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTemperatureName = tAttr.create("TemperatureName", "tname", MFnData::kString, aTemperatureNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aTemperatureName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aPressure = nAttr.create("Pressure", "p", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aPressure);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aPressureNameDefaultData = strData.create("pressure", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPressureName = tAttr.create("PressureName", "pname", MFnData::kString, aPressureNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aPressureName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aFuel = nAttr.create("Fuel", "f", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aFuel);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aFuelNameDefaultData = strData.create("fuel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFuelName = tAttr.create("FuelName", "fname", MFnData::kString, aFuelNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aFuelName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aFalloff = nAttr.create("Falloff", "falloff", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aFalloff);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aFalloffNameDefaultData = strData.create("falloff", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFalloffName = tAttr.create("FalloffName", "falloffname", MFnData::kString, aFalloffNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aFalloffName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aVelocity = nAttr.create("Velocity", "v", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aVelocity);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aVelocityNameDefaultData = strData.create("velocity", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityName = tAttr.create("VelocityName", "vname", MFnData::kString, aVelocityNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aVelocityName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aColors = nAttr.create("Color", "cd", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aColors);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aColorsNameDefaultData = strData.create("color", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aColorsName = tAttr.create("ColorsName", "cdname", MFnData::kString, aColorsNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aColorsName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    aCoordinates = nAttr.create("Coordinates", "uvw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    stat = addAttribute(aCoordinates);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    const MObject aCoordinatesNameDefaultData = strData.create("coordinates", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aCoordinatesName = tAttr.create("CoordinatesName", "uvwname", MFnData::kString, aCoordinatesNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aCoordinatesName);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    MFnUnitAttribute uniAttr;
    aTime = uniAttr.create("CurrentTime", "time", MFnUnitAttribute::kTime,  0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    stat = addAttribute(aTime);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    
    // Setup the output vdb
    aVdbOutput = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    stat = addAttribute(aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    // Set the attribute dependencies
    stat = attributeAffects(message, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aInFluid, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aDensity, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aDensityName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aTemperature, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aTemperatureName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aPressure, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aPressureName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aFuel, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aFuelName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aFalloff, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aFalloffName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aVelocity, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aVelocityName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aColors, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aColorsName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aCoordinates, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    stat = attributeAffects(aCoordinatesName, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    
    stat = attributeAffects(aTime, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (stat != MS::kSuccess) return stat;
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DOpenVDBFromFluid", "initialize", std::current_exception());
}
