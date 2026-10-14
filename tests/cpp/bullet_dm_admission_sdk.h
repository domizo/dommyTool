#pragma once
// Real Maya API and project data-reader/entry-guard templates. Native data
// topology/copy implementations and Bullet worlds are declarations only.
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnPluginData.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPxData.h>
#include <maya/MTime.h>
#include <maya/MUuid.h>
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DGeometryBatchBudget.h"
#include "modules/dynamics/DBullet/Solver/DBulletEvaluation.h"
#include <vector>
struct DCurveData:MPxData {
    static MTypeId id;
    bool hasValidCurveTopology()const;
    MStatus copyFrom(const DCurveData&);
    std::size_t numberOfCurves()const;
};
struct DMeshData:MPxData {
    static MTypeId id;
    bool hasValidMeshTopology()const;
    MStatus copyFrom(const DMeshData&);
    std::size_t numberOfMesh()const;
};
using DCurveDataPtrList=std::vector<DCurveData*>;
namespace dommy {namespace bullet {
bool admitCurveInput(const DCurveData&,geometry::GeometryBatchBudget&,bool retain=true);
bool admitMeshInput(const DMeshData&,geometry::GeometryBatchBudget&,bool retain=true);
}}
using DMeshDataPtrList=std::vector<DMeshData*>;
using UuidList=std::vector<MUuid>;
using DPIdList=std::vector<int>;
struct DMbtSoft {static MTypeId typeId;};
struct DCbtRope {static MTypeId typeId;};
struct AdmissionBase {
    static MObject inDC,inDM,inSoftDM,inRopeDC,inKinematicDCs,inKinematicDMs,inConst;
    static MObject outDC,outDM,outSoftDM,outRopeDC,outContactDP,aIsCheckAndDeletePanicSoftBody;
    bool m_requiresStartFrame=true;
    MTime m_prevTime;
    MObject thisMObject()const;
};
struct DCBullet2DSolver:AdmissionBase{MStatus compute(const MPlug&,MDataBlock&);};
struct DMBulletTest:AdmissionBase{MStatus compute(const MPlug&,MDataBlock&);};
#define DM_BULLET_2D_DEBUG 0
#define DOMMY_MAYA_DEBUG(...) ((void)0)
