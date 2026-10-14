//
//  DVdbOp.hpp
//  dommyTool
//
//  Created by yuzo on 2018/01/21.
//

#pragma once

#include "DMayaHeaders.h"

#include <openvdb/openvdb.h>
#include <openvdb/Types.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/LevelSetUtil.h>

#include <openvdb/tools/PointScatter.h>
#include <openvdb/util/NullInterrupter.h>

namespace dommy { namespace vdb { struct TreeEditBudget; } }

MStatus preflightVdbMeshInput(const MObject& meshObject,
                              dommy::vdb::TreeEditBudget& budget,
                              std::size_t& vertexCount, std::size_t& triangleCount);
bool GenerateVdbMeshOp(short voxelType,
                       float voxelSize,
                       const MObject& meshObject,
                       float exteriorBandWidth,
                       float interiorBandWidth,
                       bool unsignedDistanceField,
                       bool fillInterior,
                       openvdb::FloatGrid::Ptr& grid,
                       dommy::vdb::TreeEditBudget* sharedBudget = nullptr);
