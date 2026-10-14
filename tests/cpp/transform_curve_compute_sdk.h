#pragma once
// Real Maya declarations and production admission/math/publication helpers.
// Custom payload ownership, cloning and NURBS transport are declaration-only
// seams. This proves API/type compatibility, not a plug-in link or host run.
#include <maya/MPxData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MRampAttribute.h>
#include "DDataUtility/Data/DPAttributeData.h"
#include "DDataUtility/Data/DPGroupData.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DMayaCurveKnots.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DFurGenerationPlan.h"
#include "common/utility/DAhoGenerationPlan.h"
#include "common/utility/DRandomValueGeneration.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include "math/DRandomGenerator.h"
#include "math/dommyMath.h"
#include "modules/point/DPoint/dpoint_ids.h"
#include "modules/curve/DCurve/dcurve_ids.h"
#include "modules/mesh/DMesh/dmesh_ids.h"
#include "Debug/DMayaDebug.h"
#include <functional>

class GeneratorApiAttributeOwner {
public:
    ConstDPAttrIter beginAttr()const;
    ConstDPAttrIter endAttr()const;
    ConstDPGrpIter beginGrp()const;
    ConstDPGrpIter endGrp()const;
    bool isExistAttr(const std::string&)const;
    DPAttributeData::ConstPtr getAttr(const std::string&)const;
    void addAttr(const std::string&,const DPAttributeData&);
    template<class T>const std::vector<T>& attrListValue(const std::string&)const;
    template<class T>T attrDefaultValue(const std::string&)const;
};
class DPData:public MPxData,public GeneratorApiAttributeOwner {
public:
    static const MTypeId id;
    bool hasValidPointTopology()const;
    std::size_t numberOfPoints()const;
    const DPointList* getPositionPtrConst()const;
    ConstDPointIter beginPt()const;
    ConstDPointIter endPt()const;
    using PointCopyOperation=std::function<MStatus(DPoint&,std::size_t,std::size_t)>;
    MStatus copyRepeatedFrom(const DPData&,std::size_t,const PointCopyOperation&);
};
class GeneratorApiPrimitive:public GeneratorApiAttributeOwner {
public:
    GeneratorApiPrimitive(const MPointArray&,const MDoubleArray&,int,bool=true);
    bool hasValidCurveTopology()const;
    std::size_t numberOfPoints()const;
    DPointList getPositionConst()const;
    bool replacePositions(const DPointList&);
};
using DCurveInternalData=GeneratorApiPrimitive;
using DMeshInternalData=GeneratorApiPrimitive;
using DCItem=std::shared_ptr<GeneratorApiPrimitive>;
class DCurveData:public MPxData,public GeneratorApiAttributeOwner {
public:
    static const MTypeId id;
    using DCurveList=std::vector<DCItem>;
    using ConstDCurveIter=DCurveList::const_iterator;
    ConstDCurveIter beginCurves()const;
    ConstDCurveIter endCurves()const;
    bool hasValidCurveTopology()const;
    std::size_t numberOfCurves()const;
    std::size_t getTotalCvs()const;
    bool insert(const DCurveList&);
    MStatus copyFrom(const DCurveData&);
    void updateBBox();
};
class DMeshData:public MPxData,public GeneratorApiAttributeOwner {
public:
    static const MTypeId id;
    using Meshes=std::vector<std::shared_ptr<GeneratorApiPrimitive>>;
    Meshes::const_iterator beginMeshes()const;
    Meshes::const_iterator endMeshes()const;
    bool hasValidMeshTopology()const;
    std::size_t numberOfMesh()const;
    MStatus copyFrom(const DMeshData&);
    void updateBBox();
};
MStatus generateMCurveFromDcurve(const DCItem&,MObject&);
