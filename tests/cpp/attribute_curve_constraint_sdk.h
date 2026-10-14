#pragma once
// Native Maya API headers, declaration-only procedural payload owners. This
// checks complete entry bodies, not payload implementations or plug-in linking.
#include <maya/MPxData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MStringArray.h>
#include <maya/MMatrixArray.h>
#include "common/utility/DAttributeCreateNumeric.h"
#include "common/utility/DAttributeCreatePlan.h"
#include <maya/MArrayDataHandle.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFloatVector.h>
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <memory>
#include <vector>

using DStringList = std::vector<std::string>;
using DPoint = Imath::V3f;
class CreateApiGroup { public: const std::vector<bool>& getGroupIds() const; };
using CreateApiGroups = std::map<std::string, std::shared_ptr<CreateApiGroup>>;
class CreateApiOwner {
public:
    DPAttrIter beginAttr();
    DPAttrIter endAttr();
    ConstDPAttrIter beginAttr() const;
    ConstDPAttrIter endAttr() const;
    CreateApiGroups::const_iterator beginGrp() const;
    CreateApiGroups::const_iterator endGrp() const;
    DPAttributeData::ConstPtr getAttr(const std::string&) const;
    bool isExistAttr(const std::string&) const;
    void removeAttr(const std::string&);
    void addAttr(const std::string&, const DPAttributeData&);
};
class DPData : public MPxData, public CreateApiOwner {
public:
    static const MTypeId id;
    std::size_t numberOfPoints() const;
    void setDPData(const DPData&);
    bool hasValidPointTopology() const;
};
class CreateApiPrimitive : public CreateApiOwner {
public:
    std::size_t numberOfPoints() const;
    std::size_t numberOfKnots() const;
    int numFace() const;
    std::size_t numberOfConnections() const;
    std::size_t numberOfUVs() const;
    ConstDPAttrIter beginFaceAttr() const;
    ConstDPAttrIter endFaceAttr() const;
    ConstDPAttrIter beginPVtxAttr() const;
    ConstDPAttrIter endPVtxAttr() const;
    CreateApiGroups::const_iterator beginFaceGrp() const;
    CreateApiGroups::const_iterator endFaceGrp() const;
    CreateApiGroups::const_iterator beginEdgeGrp() const;
    CreateApiGroups::const_iterator endEdgeGrp() const;
};
using CreateApiPrimitives = std::vector<std::shared_ptr<CreateApiPrimitive>>;
class DCurveData : public MPxData, public CreateApiOwner {
public:
    static const MTypeId id;
    MStatus copyFrom(const DCurveData&);
    bool hasValidCurveTopology() const;
    std::size_t numberOfCurves() const;
    std::size_t getTotalCvs() const;
    CreateApiPrimitives::const_iterator beginCurves() const;
    CreateApiPrimitives::const_iterator endCurves() const;
    CreateApiPrimitives::const_iterator beginPrims() const;
    CreateApiPrimitives::const_iterator endPrims() const;
};
class DMeshData : public MPxData, public CreateApiOwner {
public:
    static const MTypeId id;
    MStatus copyFrom(const DMeshData&);
    bool hasValidMeshTopology() const;
    std::size_t numberOfMesh() const;
    void getTotalVertex(std::size_t&) const;
    CreateApiPrimitives::const_iterator beginMeshes() const;
    CreateApiPrimitives::const_iterator endMeshes() const;
    CreateApiPrimitives::const_iterator beginPrims() const;
    CreateApiPrimitives::const_iterator endPrims() const;
};
const DPData* getInputDP(const MObject&, MDataBlock&);
const DCurveData* getInputDC(const MObject&, MDataBlock&);
const DMeshData* getInputDM(const MObject&, MDataBlock&);
void EchoWarning(const char*, const char*);
template <typename Array, typename T>
void DScalarMayaArrayToVectorList(const Array&, std::vector<T>&);
struct SeExprParameters;
class DPointSeExpr {
public:
    explicit DPointSeExpr(const std::string&);
    bool isValid() const;
};
template <typename Owner>
MStatus evaluateAttribute(Owner&, const std::string&, int, const MTime&,
                          const SeExprParameters&, DPData* = nullptr);
