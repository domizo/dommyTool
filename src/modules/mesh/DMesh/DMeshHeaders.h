//
//  DMeshHeaders.h
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//

#pragma once

#include "PluginFeatures.h"

/////////////////  Data  /////////////////
#include "DMeshData.h"



/////////////////  Display  /////////////////
#include "DMeshDisplay.h"
#include "DMMarker.h"


/////////////////  Sub Data  /////////////////

#include "DMGroup.h"
#include "DMGroupDelete.h"
#include "DMAttributeCopy.h"
#include "DMAttributeTransfer.h"
#include "DMGroupFromDP.h"
#include "DMGroupToIndex.h"


/////////////////  Convert  /////////////////
#include "DMConvertFromMeshes.h"
#include "DMToMesh.h"

/////////////////  Generate  /////////////////
#include "DMWireGen.h"
#include "DMTimeShapeGen.h"


/////////////////  Utility  /////////////////
#include "DMColor.h"
#include "DMRemesh.h"
#include "DMSlicer.h"
#include "DMBoolean.h"
#include "DMCharactor.h"
#include "DMExplodeView.h"
#include "DMFaceInOut.h"
#include "DMDelete.h"
#include "DMTransform.h"
#include "DMSolver.h"
#include "DMVectorDeform.h"
#include "DMNoise.h"
#include "DMSubDivide.h"
#include "DMCopyUV.h"
#if DOMMY_WITH_FASTNOISE
#include "DMFastNoise.h"
#endif
#include "DMNormal.h"
#include "DMMerge.h"
#include "DMMotionDeform.h"
#include "DMDebrisGen.h"
#include "DMEdgeDisplace.h"
#include "DMExtrude.h"
#include "DMConnetedPieces.h"
#include "DMConnectTargets.h"
#include "DMCopyTransform.h"
#include "DMMakePiece.h"
#include "DMEdgeDisplaceDir.h"
#include "DMInfo.h"



/////////////////  Dynamics  /////////////////
#include "DMnRigidGen.h"
#include "DMRest.h"
#include "DCRest.h"
#if DOMMY_WITH_BULLET
#include "DMbtRigid.h"
#include "DCbtRigid2D.h"
#include "DMbtSoft.h"
#include "DCbtRope.h"
#include "DBulletFractureSolver.h"
#include "DMbtFracture.h"
#include "DBPointConstraint.h"
#include "DBLinkConstraint.h"


#include "BTP2PConstraint.h"
#include "BTFixConstraint.h"
#include "BTConeTwistConstraint.h"
#include "BT6DofConstraint.h"
#endif
