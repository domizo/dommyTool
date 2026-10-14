#pragma once

// Real Maya/DType/DPData/attribute/geometry declarations. DPData's compiled
// implementation is not linked in this method-level SDK check.
// Imath vectors use the host test stub when archived headers are unavailable.
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlug.h>
#include "src/modules/point/DPoint/Utility/DPAdjustSpace.h"
#include "DType.h"
#include "DDataUtility/Data/DPData.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DPointPayloadBudget.h"
#include "geometry/DPointSetRegularizer.h"

const DPData* getInputDP(const MObject&, MDataBlock&);
MStatus getValidInputDP(const MObject&, MDataBlock&, const DPData*&);
