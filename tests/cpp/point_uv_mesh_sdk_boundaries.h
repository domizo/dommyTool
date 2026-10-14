#pragma once

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include "src/modules/point/DPoint/mesh/DPToMesh.h"
#include "src/modules/point/DPoint/nurbs/DPUVGen.h"
#include "DDataUtility/Data/DPData.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshDataOutput.h"
#include "common/utility/DMayaPluginData.h"

// The UV initializer only needs these custom-data IDs. Their unchanged values
// and registration are checked elsewhere; no custom ownership is emulated.
struct DCurveData { static const MTypeId id; };
struct DMeshData { static const MTypeId id; };
const DPData* getInputDP(const MObject&, MDataBlock&);
