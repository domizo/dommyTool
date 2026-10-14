//
//  DPointHeaders.h
//  dommyTool
//
//  Created by yuzo on 2016/04/30.
//
//

#pragma once

#include "PluginFeatures.h"

//DPoint Data
#include "DPData.h"

//Group
#include "DPGroup.h"
#include "DPGroupDelete.h"

//Attribute
#include "DPAttributeCreate.h"
#include "DPAttributeDelete.h"
#include "DPAttributeBlend.h"
#if DOMMY_WITH_OPENVDB
#include "DPAttrSmooth.h"
#endif
#include "DPAttributeTransfer.h"
#include "DPAttributeSeExpr.h"
#include "DPAttributeCast.h"
#if DOMMY_WITH_OPENVDB
#include "DPAttributeVDB.h"
#endif
#include "DPAttributeToDataArray.h"

//IO
#include "DPointDataExportCmd.h"
#include "ReadDPointDataNode.h"

#if DOMMY_WITH_ARNOLD
#include "DPWriteToAi.h"
#endif




//Render
#if DOMMY_WITH_ARNOLD
#include "DPLightGen.h"
#endif


//Display
#include "DPointDisplay.h"
#include "DPMarker.h"

//emitter
#include "DPEmitter.h"
#include "DPFluidEmitter.h"

//field
#include "DPGoalField.h"
#include "DPFluidVectorField.h"
#include "DPFluidNoiseField.h"

//nobject
#include "DPnParticle.h"
#include "DPnPopSolver.h"


//Deform
#include "DPConvertWeights.h"
#include "DPVectorDeform.h"
#include "DPPointDeform.h"

//Modify
#include "DPEditDouble.h"


//Utility
#include "DPDelete.h"
#include "DPTransform.h"
#include "DPMerge.h"
#include "DPFillSpace.h"
#include "DPVecCompare.h"
#include "DPArraySize.h"
#include "DPDistance.h"
#include "DPPointToFloat.h"
#include "DPDirection.h"
#include "DPColor.h"
#include "DPSort.h"
#include "DPTimeShift.h"
#include "DPRay.h"
#include "DPBlend.h"
#include "DPNoise.h"
#include "DPDFNoise.h"
#include "DPSmooth.h"
#include "DPAdjustSpace.h"
#include "DPAffect.h"
#include "DPPyroNoise.h"
#include "DPMagnet.h"
#include "DPSolver.h"
#if DOMMY_WITH_OPENVDB
#include "DPSelfCollision.h"
#endif
#include "DPGoal.h"
#include "DPParticleAttribute.h"
#include "DPDuplicator.h"
#include "DPNull.h"
#if DOMMY_WITH_OPENVDB
#include "DPCache.h"
#endif
#if DOMMY_WITH_ARNOLD
#include "DPInstancer.h"
#endif
#include "DPAvoid.h"
#include "DPSpiralField.h"
#if DOMMY_WITH_FASTNOISE
#include "DPFastNoise.h"
#endif
#include "DPSearchPoint.h"
#if DOMMY_WITH_OPENVDB
#include "DPClosestPointVDB.h"
#endif
#include "DPCopyTransform.h"


//Generate
#if DOMMY_WITH_OPENVDB
#include "DPointPlaneEmit.h"
#endif
#include "DPointMeshEmit.h"
#include "DPointCurveEmit.h"
#include "DPointSurfaceEmit.h"
#include "DPointCircleEmit.h"
#if DOMMY_WITH_OPENVDB
#include "DPointVolumeEmit.h"
#include "DPScatter.h"
#endif
#include "DPAddPoint.h"
#include "DPointFromFluid.h"


#include "DPAnimationEmit.h"

//generate Data
#include "DPIntArrayGen.h"
#include "DPDoubleArrayGen.h"
#include "DPVectorArrayGen.h"
#include "DPStringArrayGen.h"
#include "DPSignal.h"
#include "DPSignal3D.h"
#include "DPArrayToCompound.h"


//DP Texture
#include "DPUVGen.h"

//DPMesh
#include "DPMeshDuplicator.h"
#include "DPVoronoi.h"
#include "DPToMesh.h"
#include "DPFoamMesh.h"

//DPAnim
#include "DPAnimMatrix.h"
#include "DPMatrix.h"
#include "DPMotionCreate.h"
#include "DPRotation.h"


//DPRender
//Texture


//Math
#include "DPMath.h"
#include "DPAdd.h"
#include "DPSubtract.h"
#include "DPMultiply.h"
#include "DPDivide.h"
#include "DPModulo.h"
#include "DPPow.h"
#include "DPMin.h"
#include "DPMax.h"

#include "DPMathFunc.h"
#include "DPNegative.h"
#include "DPSqrt.h"
#include "DPFloor.h"
#include "DPRound.h"
#include "DPCeil.h"
#include "DPExponential.h"
#include "DPAbsolute.h"


#include "DPDotProduct.h"
#include "DPVecAngle.h"
#include "DPVecLength.h"
#include "DPVecMinComponent.h"
#include "DPVecMaxComponent.h"
#include "DPVecAverageComponent.h"
#include "DPNormalize.h"
#include "DPCrossProduct.h"


#include "DPFitRange.h"
#include "DPRemap.h"
