#pragma once
#include <cstring>
// Declaration-only custom payload/processing boundaries for a native Maya API
// syntax check. No custom data implementation or plug-in link is implied.
#include <maya/MPxData.h>
#include <maya/MVector.h>
#include <maya/MStringArray.h>
#include <maya/MRampAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnMesh.h>
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "modules/point/DPoint/math/DMayaUnaryMath.h"
#include "modules/point/DPoint/math/DMayaMathSettings.h"
#include "math/DRemapMath.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DUvParameter.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DAttributePublication.h"
#include "modules/point/DPoint/dpoint_ids.h"
#include "modules/curve/DCurve/dcurve_ids.h"
#include "modules/mesh/DMesh/dmesh_ids.h"
#include <memory>
#include <utility>

struct ApiGroup {const std::vector<bool>& getGroupIds()const;};
using ApiGroups=std::map<std::string,std::shared_ptr<ApiGroup>>;

class ApiAttributeOwner {
public:
    DPAttrIter beginAttr();
    DPAttrIter endAttr();
    ConstDPAttrIter beginAttr()const;
    ConstDPAttrIter endAttr()const;
    ApiGroups::const_iterator beginGrp()const;
    ApiGroups::const_iterator endGrp()const;
    DPAttributeData::ConstPtr getAttr(const std::string&)const;
    bool isExistAttr(const std::string&)const;
    void addAttr(const std::string&,const DPAttributeData&);
    void removeAttr(const std::string&);
    template<class T>std::shared_ptr<TypedDPAttributeData<T>> getValidTypedAttributedata(const std::string&);
    template<class T>void addAttrWithValues(const std::string&,const std::vector<T>&);
};
class DPData:public MPxData,public ApiAttributeOwner {
public:
    static const MTypeId id;
    bool hasValidPointTopology()const;
    void setDPData(const DPData&);
    std::size_t numberOfPoints()const;
    const DPointList* getPositionPtrConst()const;
};
class ApiPrimitive:public ApiAttributeOwner {
public:
    std::size_t numberOfPoints()const;
    const DPointList* getPositionPtrConst()const;
    const MMatrix& getTransformMatrix()const;
    int numFace()const;
    std::size_t numberOfConnections()const;
    ConstDPAttrIter beginFaceAttr()const;
    ConstDPAttrIter endFaceAttr()const;
    ConstDPAttrIter beginPVtxAttr()const;
    ConstDPAttrIter endPVtxAttr()const;
    ApiGroups::const_iterator beginFaceGrp()const;
    ApiGroups::const_iterator endFaceGrp()const;
    ApiGroups::const_iterator beginEdgeGrp()const;
    ApiGroups::const_iterator endEdgeGrp()const;
};
using ApiPrimitives=std::vector<std::shared_ptr<ApiPrimitive>>;
class DCurveData:public MPxData,public ApiAttributeOwner {
public:
    static const MTypeId id;
    bool hasValidCurveTopology()const;
    MStatus copyFrom(const DCurveData&);
    std::size_t numberOfCurves()const;
    bool isExistPointAttr(const std::string&)const;
    ApiPrimitives::const_iterator beginCurves()const;
    ApiPrimitives::const_iterator endCurves()const;
    ApiPrimitives::const_iterator beginPrims()const;
    ApiPrimitives::const_iterator endPrims()const;
};
class DMeshData:public MPxData,public ApiAttributeOwner {
public:
    static const MTypeId id;
    bool hasValidMeshTopology()const;
    MStatus copyFrom(const DMeshData&);
    std::size_t numberOfMesh()const;
    bool isExistPointAttr(const std::string&)const;
    ApiPrimitives::const_iterator beginMeshes()const;
    ApiPrimitives::const_iterator endMeshes()const;
    ApiPrimitives::const_iterator beginPrims()const;
    ApiPrimitives::const_iterator endPrims()const;
};
class dupMesh {
public:
    explicit dupMesh(const MObject&);
    explicit dupMesh(const std::shared_ptr<ApiPrimitive>&);
    bool isValid()const;
};
const DPData* getInputDP(const MObject&,MDataBlock&);
const DCurveData* getInputDC(const MObject&,MDataBlock&);
const DMeshData* getInputDM(const MObject&,MDataBlock&);
void EchoWarning(const char*,const char*);
void EchoWarning(const std::string&,const char*);
namespace shared_math_body {
template<class... Args>MStatus processDPointMathFncSingle(Args&&...);
template<class... Args>MStatus processDCurveMathFncSingle(Args&&...);
template<class... Args>MStatus processDMeshMathFncSingle(Args&&...);
template<class... Args>MStatus processDPointMathFnc(Args&&...);
template<class... Args>MStatus processDCurveMathFnc(Args&&...);
template<class... Args>MStatus processDMeshMathFnc(Args&&...);
}
MStatus processDDataMathOutput(const MObject&,const MObject&,const char*,const std::string&,
    const std::string&,bool,bool,const MObject&,const MObject&,const MObject&,
    MDataBlock&,const MPlug&,int);
MStatus processDDataMathOutputSingle(const MObject&,const MObject&,const char*,const std::string&,
    const std::string&,bool,bool,MDataBlock&,const MPlug&,int);
MStatus processMayaNumericMathFunc(const MObject&,const char*,const MObject&,const MObject&,
    const MObject&,const MObject&,MDataBlock&,const MPlug&,int);
MStatus processMayaNumericMathFuncSingle(const MObject&,const char*,const MObject&,const MObject&,
    MDataBlock&,const MPlug&,int);
