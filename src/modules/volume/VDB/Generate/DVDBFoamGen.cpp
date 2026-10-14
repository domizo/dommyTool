//
//  DVDBFoamGen.cpp
//  dommyTool
//
//  Created by yuzo on 2/7/17.
//
//

#include "DVDBFoamGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "../Common/DVDBComputeGuard.h"

#include <cmath>
#include <cstdint>

#include <openvdb/Types.h>
#include <openvdb/Grid.h>
#include <openvdb/math/Math.h>
#include <openvdb/math/Transform.h>
#include <openvdb/util/NullInterrupter.h>
#include <openvdb/tools/ParticlesToLevelSet.h>

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DOpenVDBDataHeaders.h"

#include "DNodeOperation.h"
#include "../Common/DVdbParticleRasterPlan.h"
#include "../Edit/DOpenVDBParticleConversionPolicy.h"
#include "common/utility/DMayaEntryPointGuard.h"

const MTypeId DVDBFoamGen::typeId(ID_VDB_DOPENVDB_FOAM_GEN);

MObject DVDBFoamGen::inDP;

MObject DVDBFoamGen::aRadius;
MObject DVDBFoamGen::aRadiusOffset;

MObject DVDBFoamGen::aVoxelSize;
MObject DVDBFoamGen::aBandWidth;

MObject DVDBFoamGen::aVdbOut;

void* DVDBFoamGen::creator() {
    return ::dommy::maya::createObject("DVDBFoamGen", [&]() {
        return new DVDBFoamGen();
    });
}

MStatus DVDBFoamGen::compute(const MPlug& plug, MDataBlock& data) try {

    MStatus stat;
    MDataHandle valueHandle;

    if (plug == aVdbOut) {

#pragma mark inDPの取得

        const DPData* inputDPData = nullptr;
        valueHandle = data.inputValue(inDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(valueHandle, DPData::id, inputDPData));
        if (!inputDPData || !inputDPData->hasValidPointTopology() || !inputDPData->numberOfPoints())
            return MS::kInvalidParameter;

        if (inputDPData && inputDPData->numberOfPoints()) {
            valueHandle = data.inputValue(aVoxelSize, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float voxelSize = valueHandle.asFloat();
            valueHandle = data.inputValue(aBandWidth, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float bandWidth = valueHandle.asFloat();
            valueHandle = data.inputValue(aRadius, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const float radius = valueHandle.asFloat();

            if (!std::isfinite(voxelSize) || voxelSize <= 0.0f) {
                EchoWarning(name().asChar(), "voxel size must be finite and positive");
                return MS::kFailure;
            }
            if (!std::isfinite(bandWidth) || bandWidth <= 1.0f) {
                EchoWarning(name().asChar(), "half-width must be finite and larger than one");
                return MS::kFailure;
            }
            if (!std::isfinite(radius) || radius <= 0.0f) {
                EchoWarning(name().asChar(), "radius must be finite and positive");
                return MS::kFailure;
            }

            const DPointList& points = *inputDPData->getPositionPtrConst();
            if (points.size() > (dommy::vdb::kMaxVdbPayloadBytes / 4U) / 160U)
                return MS::kInvalidParameter;
            dommy::vdb::ParticleInputExtrema extrema;
            for (std::size_t index = 0; index < points.size(); ++index) {
                if (!extrema.addPosition(points[index])) {
                    MGlobal::displayError("Foam input positions must contain only finite values.");
                    return MS::kFailure;
                }
            }
            extrema.addRadius(radius);

            dommy::vdb::ParticleRasterParameters rasterParameters;
            rasterParameters.voxelSize = voxelSize;
            rasterParameters.halfBandVoxels = bandWidth;
            rasterParameters.radiusScale = 1.0;
            rasterParameters.minimumRadiusVoxels = 0.0;
            rasterParameters.maskWidth = 0.0;
            rasterParameters.velocityMultiplier = 0.0;
            rasterParameters.velocitySpacing = 1.0;
            dommy::vdb::TreeEditBudget rasterBudget;
            float background = 0;
            if (!dommy::vdb::planParticleConversion(points.size(), extrema, rasterParameters,
                    1, 0,
                    rasterBudget, background)) {
                MGlobal::displayError("Foam input is invalid or exceeds the raster work budget.");
                return MS::kFailure;
            }

#pragma mark get output vdb
            MFnPluginData pluginData;
            pluginData.create(DOpenVDBData::id, &stat);
            if (stat != MS::kSuccess)
                return stat;

            DOpenVDBData* const vdb(
                dommy::maya::pluginDataPointer<DOpenVDBData>(pluginData, &stat));
            if (stat != MS::kSuccess || !vdb)
                return MS::kFailure;

            if (vdb) {
                

                openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(background);
                if (!grid) return MS::kFailure;

                openvdb::math::Transform::Ptr transform =
                    openvdb::math::Transform::createLinearTransform(voxelSize);
            if (!transform) return MS::kFailure;

                grid->setGridClass(openvdb::GRID_LEVEL_SET);
                grid->setTransform(transform);
                grid->setName("foam_ls");

                openvdb::FloatGrid::Accessor acc = grid->getAccessor();

                for (auto pIt = inputDPData->beginPt(), epIt = inputDPData->endPt(); pIt != epIt;
                     ++pIt) {
                    const float r0 = radius / voxelSize;
                    const float rmax = r0 + bandWidth;
                    openvdb::Vec3R pos((*pIt)[0], (*pIt)[1], (*pIt)[2]);
                    pos = transform->worldToIndex(openvdb::Vec3R((*pIt)[0], (*pIt)[1], (*pIt)[2]));

                    if (!dommy::vdb::isValidIndexCoordinate(pos[0] - rmax) ||
                        !dommy::vdb::isValidIndexCoordinate(pos[0] + rmax) ||
                        !dommy::vdb::isValidIndexCoordinate(pos[1] - rmax) ||
                        !dommy::vdb::isValidIndexCoordinate(pos[1] + rmax) ||
                        !dommy::vdb::isValidIndexCoordinate(pos[2] - rmax) ||
                        !dommy::vdb::isValidIndexCoordinate(pos[2] + rmax)) {
                        MGlobal::displayError(
                            "Foam positions exceed the supported VDB index range.");
                        return MS::kFailure;
                    }

                    openvdb::Coord ijk;
                    // Wider counters remain safe even at an Int32 endpoint.
                    const int imin = openvdb::math::Floor(pos[0] - rmax),
                              imax = openvdb::math::Ceil(pos[0] + rmax);
                    const int jmin = openvdb::math::Floor(pos[1] - rmax),
                              jmax = openvdb::math::Ceil(pos[1] + rmax);
                    const int kmin = openvdb::math::Floor(pos[2] - rmax),
                              kmax = openvdb::math::Ceil(pos[2] + rmax);

                    for (std::int64_t i = imin; i <= imax; ++i) {
                        ijk[0] = static_cast<int>(i);
                        const float x2 = openvdb::math::Pow2(i - pos[0]);
                        for (std::int64_t j = jmin; j <= jmax; ++j) {
                        ijk[1] = static_cast<int>(j);
                            const float x2y2 = openvdb::math::Pow2(j - pos[1]) + x2;
                            for (std::int64_t k = kmin; k <= kmax; ++k) {
                        ijk[2] = static_cast<int>(k);
                                // Distance in voxel units to the sphere.
                                const float v =
                                    openvdb::math::Sqrt(x2y2 + openvdb::math::Pow2(k - pos[2])) -
                                    r0;
                                const float d = openvdb::math::Abs(v);
                                if (d < bandWidth) { // active narrow band
                                    const float distance = voxelSize * v;
                                    if (distance < acc.getValue(ijk)) {
                                        acc.setValue(ijk, distance);
                                    }
                                } else if (v <= -bandWidth) {
                                    acc.setValueOff(ijk, -background);
                                }
                            } // end leapfrog over k
                        } // end loop over j
                    } // end loop over i
                }
                openvdb::tools::signedFloodFill(grid->tree());
                openvdb::tools::pruneLevelSet(grid->tree());

                vdb->insert(grid);

                MDataHandle outHandle = data.outputValue(aVdbOut, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
                        outHandle, pluginData.object(), DOpenVDBData::id));

                return data.setClean(plug);
            }
        }
    }

    return MS::kUnknownParameter;
}
DOMMY_VDB_COMPUTE_CATCH("DVDBFoamGen")

MStatus DVDBFoamGen::initialize() try {
    MStatus stat;

    // attributes

    MFnTypedAttribute tAttr;
    MFnStringData strData;
    MFnNumericAttribute nAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aRadius = nAttr.create("Radius", "rad", MFnNumericData::kFloat, 0, &stat);

    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aRadiusOffset = nAttr.create("RadiusOffset", "roff", MFnNumericData::kFloat, 0, &stat);

    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusOffset));

    aVoxelSize = nAttr.create("VoxelSize", "vs", MFnNumericData::kFloat, 0, &stat);

    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.02));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVoxelSize));

    aBandWidth = nAttr.create("BandWidth", "bw", MFnNumericData::kFloat, 0, &stat);

    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBandWidth));

    // Setup the output vdb
    aVdbOut = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOut));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aVdbOut));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, aVdbOut));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusOffset, aVdbOut));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, aVdbOut));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBandWidth, aVdbOut));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DVDBFoamGen", "initialize", std::current_exception());
}
