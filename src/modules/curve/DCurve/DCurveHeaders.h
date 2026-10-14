//
//  Header.h
//  dommyTool
//
//  Created by yuzo on 5/1/16.
//
//

#pragma once

#include "PluginFeatures.h"

/////////////////  Data  /////////////////
#include "DCurveData.h"

/////////////////  Sub Data  /////////////////

#include "DCAttributeCopy.h"
#include "DCAttributeTransfer.h"


/////////////////  Cmd  /////////////////
#if DOMMY_WITH_ALEMBIC
#include "DExportAbcCmd.h"
#endif


/////////////////  Display  /////////////////
#include "DCurveDisplay.h"
#include "DCurveShape.h"
#include "DCMarker.h"

/////////////////  Group  /////////////////
#include "DCGroup.h"
#include "DCGroupDelete.h"
#include "DCGroupFromDP.h"

/////////////////  Generate  /////////////////
#include "DCurveFromMesh.h"
#include "DCurveFromCurve.h"
#include "DCurveFromSurface.h"
#include "DCurveLinkGen.h"
#include "DCurveFromDP.h"
#include "DCAhoGen.h"
#include "DCurveFromPfx.h"
#include "DCFurGen.h"
#include "DCurveCircleGen.h"
#if DOMMY_WITH_FREETYPE_TEXT
#include "DCText.h"
#endif
#include "DCEyelashGen.h"


///////////////// cloth  /////////////////
#include "DCurveClothFromSurface.h"


/////////////////  Convert  /////////////////
#include "DCConvertFromCurves.h"
#include "DCToCurves.h"

/////////////////  IO  /////////////////

#if DOMMY_WITH_ARNOLD
#include "DWriteDCurveToAi.h"
#endif


/////////////////  Utility  /////////////////
#include "DCReform.h"
#include "DCDivide.h"
#include "DCCharactor.h"
#include "DCCopy.h"
#include "DCUVMap.h"
#include "DCDelete.h"
#include "DCColor.h"
#include "DCMotionPath.h"
#include "DCNoise.h"
#include "DCResample.h"
#include "DCWeightMap.h"
#include "DCNormal.h"
#include "DCFitCopy.h"
#include "DCReplicator.h"
#include "DCMeshCopy.h"
#include "DCConnect.h"
#include "DCExtend.h"
#include "DCOffset.h"
#include "DCBend.h"
#include "DCStitch.h"
#include "DCCurl.h"
#include "DCCutObject.h"
#include "DCFillBetween.h"
#include "DCConvex2D.h"
#include "DCReplace.h"
#include "DCPartition.h"
#include "DCExplodeView.h"
#include "DCClumping.h"
#include "DCReflect.h"


/////////////////  Solver  /////////////////
#include "DCSolver.h"


/////////////////  Deform  /////////////////

#include "DCVectorDeform.h"













