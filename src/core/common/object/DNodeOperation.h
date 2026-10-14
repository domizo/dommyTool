//
//  DNodeOperation.hpp
//  dommyTool
//
//  Created by yuzo on 5/9/16.
//
//

#ifndef DNodeOperation_h
#define DNodeOperation_h

#include <maya/MBoundingBox.h>
#include <maya/MColor.h>
#include <maya/MDGContext.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnData.h>
#include <maya/MFnMesh.h>
#include <maya/MMatrix.h>
#include <maya/MMatrixArray.h>
#include <maya/MObject.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include "DPData.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DGenerationLimits.h"

#include <vector>

namespace dommy {
namespace sampling {
class TimeSamplingBudget;
}
}

// DAG traversal
MStatus getChildNodeFromNode(const MFnDagNode& dagNode, MObjectArray& childArray,
    std::size_t maxChildren = dommy::generation::kMaxConnectedGeometryCount);
// Scratch arrays may be partially filled on failure. A shared quota bounds
// path visits across separate child groups without changing instance/order semantics.
MStatus extractDagNodeFromTypeName(const MString& typeName, const MObjectArray& objectArray,
                                   MObjectArray& extractedArray, std::size_t* remainingVisits = nullptr);
MStatus getNurbsCurveNodeFromHierarchy(const MFnDagNode& parentNode, MObjectArray& curveNodes,
                                      std::size_t* remainingVisits = nullptr);

// MayaObject data
// Read-only transformed geometry, queried in kObject space; output unchanged on failure.
MStatus DGetMayaCurveFromPlug(const MObject& inputPlugObject, MDataBlock& data, MObject& dataObj);
// Returns read-only geometry with any incoming worldSpace transform applied.
// Query it in kObject space; do not modify it or apply another world transform.
bool DGetMayaSurfaceFromPlug(const MObject& inputPlugObject, MDataBlock& data, MObject& dataObj);
bool DGetMayaMeshFromPlug(const MObject& inputPlugObject, MDataBlock& data, MObject& dataObj);

// Plug values are published to the output only after Maya reports a
// successful read. This prevents callers from observing partially populated
// compound values.
MStatus readPlugValue(const MPlug& plug, bool& value);
MStatus readPlugValue(const MPlug& plug, short& value);
MStatus readPlugValue(const MPlug& plug, int& value);
MStatus readPlugValue(const MPlug& plug, float& value);
MStatus readPlugValue(const MPlug& plug, double& value);
// Raw plug handles require explicit ownership; use ScopedPlugDataHandle.
MStatus readPlugValue(const MPlug& plug, MDataHandle& value) = delete;
MStatus readPlugValue(const MPlug& plug, MObject& value);
MStatus readPlugValue(const MPlug& plug, MString& value);
MStatus readPlugValue(const MPlug& plug, MVector& value);
MStatus readPlugValue(const MPlug& plug, MColor& value);

MStatus readPlugValue(const MObject& node, const MObject& attribute, bool& value);
MStatus readPlugValue(const MObject& node, const MObject& attribute, int& value);
MStatus readPlugValue(const MObject& node, const MObject& attribute, double& value);
MStatus readPlugValue(const MObject& node, const MObject& attribute, MString& value);
MStatus readPlugValue(const MObject& node, const MObject& attribute, MVector& value);
MStatus readPlugValue(const MObject& node, const MObject& attribute, MColor& value);

MStatus getTransformNodeFromPlug(const MObject& shapeObject, const MObject& sourceAttribute,
                                 MFnDagNode& transformFn);

bool isInputPlugDataForScalar(MFnData::Type type);
bool isInputPlugDataForVector(MFnData::Type type);
bool isInputPlugDataForString(MFnData::Type type);
bool isInputPlugDataForMatrix(MFnData::Type type);

MStatus getMDoubleArrayFromPlug(MDataHandle dataHnd, MDoubleArray& values);
MStatus getDScalarArrayFromPlug(MDataHandle dataHnd, DScalarDataList& values);

MStatus getMVectorArrayFromPlug(MDataHandle dataHnd, MVectorArray& values);
MStatus getDVec3DataArrayFromPlug(MDataHandle dataHnd, DVec3DataList& values);

MStatus getMStringArrayFromPlug(MDataHandle dataHnd, MStringArray& values);
MStatus getMMatrixArrayFromPlug(MDataHandle dataHnd, MMatrixArray& values);

// Ids
void mergeUnionIdListOp(const DPIdList* inIdList, DPIdList& outIdList);
void mergeSubstructIdListOp(const DPIdList* inIdList, DPIdList& outIdList);
void mergeIntersectIdListOp(const DPIdList* inIdList, DPIdList& outIdList);

// check minMax
void CheckDouble3MinMaxRange(const double3* min, const double3* max, double3* finMin,
                             double3* finMax);

// check data method
MStatus isMeshData(MDataBlock data, MObject plug, MFnMesh& meshFn);

// curve
MStatus getMayaNurbsCurve(const MObject& obj, const MObject& plugObj, MDataBlock& data,
                          MObject& crvData);
// mesh
MStatus getMayaMesh(const MObject& obj, const MObject& plugObj, MDataBlock& data,
                    MObject& meshData);
MStatus getMeshInformation(const MObject& plugObject, MDataBlock& data, MBoundingBox& boundingBox,
                           MObject& meshObject);

// animation
MStatus DGetPlugTimeDP(const MTime& currentTime, std::vector<MPointArray>& array,
                       const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                       DScalarDataList* timeAttrArray, DAUIntList* pIdArray);
MStatus DGetPlugTimeCurve(const MTime& currentTime, std::vector<MPointArray>& array,
                          const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                          DScalarDataList* timeAttrArray, DAUIntList* pIdArray);
MStatus DGetPlugTimeNurbs(const MTime& currentTime, std::vector<MPointArray>& array,
                          const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                          DScalarDataList* timeAttrArray, DAUIntList* pIdArray);
MStatus DGetPlugTimeMesh(const MTime& currentTime, std::vector<MPointArray>& array,
                         const MPlug& srcPlug, const DScalarDataList& timeValueArray,
                         DScalarDataList* timeAttrArray, DAUIntList* pIdArray);
MStatus DGetPlugTimeMatrix(const MTime& currentTime, std::vector<MMatrix>& array,
                           const MPlug& srcPlug, const DScalarDataList& timeValueArray);

// time range common function
MStatus getTimeOffsetAndStretch(const DPData& inputDPData, MDataBlock& data,
                                const MObject& thisObject, const MObject& offsetPlug,
                                const MObject& stretchPlug, const MObject& offsetArrayPlug,
                                const MObject& stretchArrayPlug, const int& offsetType,
                                const int& stretchType, MDoubleArray& offsetArray,
                                MDoubleArray& stretchArray);

MDGContext getTimeDGContext(float timeVal, const float& offset, const float& stretch,
                            const float& globalOffset, const MTime::Unit& fps);

// Matrix
// Maps the space frame to the point frame; leaves output unchanged on failure.
MStatus getAlignSpaceMatrix(const MVector& forwardVec, const MVector& upVec,
                            const MVector& spaceForwardAxis, const MVector& spaceUpAxis,
                            MMatrix& output);

// Scratch outputs may be partially filled on failure; callers must discard them.
// Optional sampled offsets are appended only alongside an actually retained point.
MStatus DGetPlugTimeDPPP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                         const DScalarDataList& timeValueArray, uint ptIndex,
                         DScalarDataList* sampledOffsets = nullptr,
                         dommy::sampling::TimeSamplingBudget* sharedBudget = nullptr);
MStatus DGetPlugTimeCurvePP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                            const DScalarDataList& timeValueArray, uint ptIndex,
                            DScalarDataList* sampledOffsets = nullptr,
                            dommy::sampling::TimeSamplingBudget* sharedBudget = nullptr);
MStatus DGetPlugTimeNurbsPP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                            const DScalarDataList& timeValueArray, uint ptIndex,
                            DScalarDataList* sampledOffsets = nullptr,
                            dommy::sampling::TimeSamplingBudget* sharedBudget = nullptr);
MStatus DGetPlugTimeMeshPP(const MTime& currentTime, MPointArray& array, const MPlug& srcPlug,
                           const DScalarDataList& timeValueArray, uint ptIndex,
                           DScalarDataList* sampledOffsets = nullptr,
                           dommy::sampling::TimeSamplingBudget* sharedBudget = nullptr);

#endif /* DNodeOperation_h */
