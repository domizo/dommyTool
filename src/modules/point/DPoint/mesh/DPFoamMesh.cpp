//
//  DPFoamMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/23.
//
//

#include "DPFoamMesh.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "modules/generators/icosahedron/DIcosahedronTopology.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DMeshDataHeaders.h"

#include "DPolygonUtil.h"
#include "dommyMath.h"
#include <cmath>
#include <cstddef>
#include <limits>

const MTypeId DPFoamMesh::typeId( ID_DP_FOAM_MESH );


MObject DPFoamMesh::inDP;

MObject DPFoamMesh::aRadius;
MObject DPFoamMesh::aRadiusAttribute;

//for ball
MObject DPFoamMesh::aFrequency;
MObject DPFoamMesh::aRadiusOffset;
MObject DPFoamMesh::aRadiusResultOffset;

//for deform
MObject DPFoamMesh::aSelfDeform;

MObject DPFoamMesh::outDM;


void *DPFoamMesh::creator() {
    return ::dommy::maya::createObject("DPFoamMesh", [&]() {
        return new DPFoamMesh();
    });
}
MStatus DPFoamMesh::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus stat;
    const MDataHandle inputHandle = data.inputValue(inDP, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const DPData* inputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
        inputHandle, DPData::id, inputDPData));
    if (!inputDPData || !inputDPData->hasValidPointTopology())
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DMeshData* outDMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
        pluginData, DMeshData::id, outDMeshes));
    if (!outDMeshes) return MS::kFailure;

    const std::size_t pointCount = inputDPData->numberOfPoints();
    if (pointCount > dommy::generation::kMaxGeneratedPointCount) {
        MGlobal::displayError("DPFoamMesh: input point count exceeds the safety budget.");
        return MS::kFailure;
    }

    // Empty input replaces an earlier non-empty result without sampling.
    if (pointCount == 0U) {
        MDataHandle output = data.outputValue(outDM, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::publishTypedPluginDataObject<DMeshData>(
                output, pluginData.object(), DMeshData::id));
        return data.setClean(plug);
    }

    MDataHandle handle = data.inputValue(aFrequency, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const int frequency = handle.asInt();
    handle = data.inputValue(aRadiusOffset, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double radiusOffset = handle.asDouble();
    handle = data.inputValue(aRadiusResultOffset, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double radiusResultOffset = handle.asDouble();
    handle = data.inputValue(aSelfDeform, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const bool selfDeformVal = handle.asBool();
    handle = data.inputValue(aRadius, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double radius = handle.asDouble();
    handle = data.inputValue(aRadiusAttribute, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const std::string radiusAttrName = handle.asString().asChar();
    if (frequency < 1 || !std::isfinite(radius) || radius < 0.0 ||
        !std::isfinite(radiusOffset) || !std::isfinite(radiusResultOffset))
        return MS::kInvalidParameter;

    DScalarDataList radiusArray(pointCount, radius);
    if (inputDPData->isExistAttr(radiusAttrName))
        getDScalarArrayFromAttribute(*inputDPData, radiusAttrName, radiusArray);
    if (radiusArray.size() != pointCount) return MS::kFailure;
    const DPointList* ptsPtr = inputDPData->getPositionPtrConst();
    if (!ptsPtr || ptsPtr->size() != pointCount) return MS::kFailure;

    dommy::geometry::IcosahedronTopology topology;
    if (!dommy::geometry::buildHistoricalIcosahedron(frequency, topology)) {
        MGlobal::displayError("DPFoamMesh: sphere subdivision exceeds the topology budget.");
        return MS::kFailure;
    }
    const std::size_t baseVertexCount = topology.points.size();
    const std::size_t basePolygonCount = topology.triangles.size();
    std::size_t totalVertexNum = 0U, totalPolygonNum = 0U, totalConnectCount = 0U;
    if (!dommy::generation::checkedProduct(baseVertexCount, pointCount,
            dommy::generation::kMaxGeneratedPointCount, totalVertexNum) ||
        !dommy::generation::checkedProduct(basePolygonCount, pointCount,
            dommy::generation::kMaxGeneratedFaceCount, totalPolygonNum) ||
        !dommy::generation::checkedProduct(totalPolygonNum, 3U,
            dommy::generation::kMaxWorkItemCount, totalConnectCount) ||
        totalVertexNum > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        totalPolygonNum > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        MGlobal::displayError("DPFoamMesh: generated mesh exceeds the safety budget.");
        return MS::kFailure;
    }
    if (selfDeformVal) {
        std::size_t pairWork = 0U, deformationWork = 0U;
        if (!dommy::generation::checkedProduct(pointCount, pointCount,
                dommy::generation::kMaxWorkItemCount, pairWork) ||
            !dommy::generation::checkedProduct(pairWork, baseVertexCount,
                dommy::generation::kMaxWorkItemCount, deformationWork)) {
            MGlobal::displayError("DPFoamMesh: self-deformation work exceeds the safety budget.");
            return MS::kFailure;
        }
    }

    MFloatPointArray baseVertexArray;
    MIntArray basePolyCountArray, basePolyConnectArray;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        baseVertexCount, [&](unsigned int index, MFloatPoint& value) -> MStatus {
            const auto& point = topology.points[index];
            value = MFloatPoint(point.x, point.y, point.z);
            return MS::kSuccess;
        }, baseVertexArray));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        basePolygonCount, [](unsigned int, int& value) -> MStatus {
            value = 3;
            return MS::kSuccess;
        }, basePolyCountArray));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        basePolygonCount * 3U, [&](unsigned int index, int& value) -> MStatus {
            const auto& triangle = topology.triangles[index / 3U];
            value = index % 3U == 0U ? triangle.first :
                index % 3U == 1U ? triangle.second : triangle.third;
            return MS::kSuccess;
        }, basePolyConnectArray));

    MFloatPointArray finVertectPointArray;
    MIntArray finPolyCountArray, finPolyConnectArray;
    for (std::size_t i = 0U; i < pointCount; ++i) {
        const MVector offsetPos((*ptsPtr)[i][0], (*ptsPtr)[i][1], (*ptsPtr)[i][2]);
        const double radiusVal = radiusArray[i] + radiusOffset;
        const double outputRadius = radiusVal + radiusResultOffset;
        if (!dommy::maya::isFiniteVector(offsetPos) || !std::isfinite(radiusVal) ||
            radiusVal < 0.0 || !std::isfinite(outputRadius) || outputRadius < 0.0 ||
            (selfDeformVal && radiusVal <= std::numeric_limits<double>::epsilon()))
            return MS::kFailure;

        MFloatPointArray newDeformedVtxArray;
        if (selfDeformVal) {
            DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(
                baseVertexArray, newDeformedVtxArray));
            // Preserve the historical intersection/deformation formula and
            // float intermediate precision; native shape quality is a host gate.
            for (std::size_t x = 0U; x < pointCount; ++x) {
                if (x == i) continue;
                const MVector targetObjPos((*ptsPtr)[x][0], (*ptsPtr)[x][1], (*ptsPtr)[x][2]);
                const double targetRadiusVal = radiusArray[x] + radiusOffset;
                if (!dommy::maya::isFiniteVector(targetObjPos) ||
                    !std::isfinite(targetRadiusVal) || targetRadiusVal < 0.0)
                    return MS::kFailure;
                const double radiusSam = targetRadiusVal + radiusVal;
                const float dist = dommy::math::distance(offsetPos, targetObjPos);
                if (dist > std::numeric_limits<float>::epsilon() && dist < radiusSam) {
                    const float c1 = (std::pow(radiusVal, 2.0) - std::pow(targetRadiusVal, 2.0) +
                        std::pow(dist, 2.0)) / (2.0 * dist);
                    const MVector planeNormalVec((targetObjPos.x - offsetPos.x) / dist,
                        (targetObjPos.y - offsetPos.y) / dist, (targetObjPos.z - offsetPos.z) / dist);
                    const MVector planePosVec(c1 * planeNormalVec.x,
                        c1 * planeNormalVec.y, c1 * planeNormalVec.z);
                    for (unsigned int v = 0U; v < newDeformedVtxArray.length(); ++v) {
                        MFloatPoint newPointVec(newDeformedVtxArray[v]);
                        newPointVec = newPointVec * radiusVal;
                        const double sn = -(planeNormalVec * (MVector(newPointVec) - planePosVec));
                        const double sd = planeNormalVec * planeNormalVec;
                        const double sb = sn / sd;
                        const MVector B = newPointVec + sb * planeNormalVec;
                        const MVector C = MVector(newPointVec) - B;
                        const float dots = planePosVec * C;
                        const float distFromPlane = std::sqrt(C * C);
                        if (c1 < 0) {
                            if (dots < 0) {
                                double innerBias = 1.0 - ((-c1) / radiusVal);
                                if (innerBias < 0.0) innerBias = 0.0;
                                const MVector newDotPosVec =
                                    -(planeNormalVec.normal() * C.length()) * innerBias;
                                newDeformedVtxArray[v] += newDotPosVec;
                            }
                        } else if (dots > 0) {
                            MVector newDotPosVec = -planeNormalVec.normal() * distFromPlane;
                            newDotPosVec /= 2.0;
                            newDeformedVtxArray[v] += newDotPosVec;
                        }
                    }
                }
            }
        }
        for (unsigned int j = 0U; j < baseVertexArray.length(); ++j) {
            MFloatPoint newPoint = baseVertexArray[j];
            if (selfDeformVal) {
                newPoint = newDeformedVtxArray[j];
                newPoint *= outputRadius;
                newPoint += offsetPos;
            } else {
                newPoint = newPoint * outputRadius + offsetPos;
            }
            if (!dommy::maya::isFloatRepresentablePoint(
                    MPoint(newPoint.x, newPoint.y, newPoint.z, newPoint.w)))
                return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(finVertectPointArray.append(newPoint));
        }
        for (unsigned int j = 0U; j < basePolyCountArray.length(); ++j)
            DOMMY_RETURN_IF_FAILURE(finPolyCountArray.append(basePolyCountArray[j]));
        const std::size_t vertexOffset = baseVertexCount * i;
        for (unsigned int j = 0U; j < basePolyConnectArray.length(); ++j) {
            const int baseIndex = basePolyConnectArray[j];
            if (baseIndex < 0 || static_cast<std::size_t>(baseIndex) >= baseVertexCount ||
                vertexOffset + static_cast<std::size_t>(baseIndex) >= totalVertexNum)
                return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(finPolyConnectArray.append(
                static_cast<int>(vertexOffset + static_cast<std::size_t>(baseIndex))));
        }
    }
    if (finVertectPointArray.length() != totalVertexNum ||
        finPolyCountArray.length() != totalPolygonNum ||
        finPolyConnectArray.length() != totalConnectCount) return MS::kFailure;

    MFnMeshData meshDataFn;
    const MObject newOutMeshData = meshDataFn.create(&stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    if (newOutMeshData.isNull()) return MS::kFailure;
    MFnMesh newMeshFn;
    const MObject generatedMesh = newMeshFn.create(
        static_cast<int>(totalVertexNum), static_cast<int>(totalPolygonNum),
        finVertectPointArray, finPolyCountArray, finPolyConnectArray, newOutMeshData, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    if (generatedMesh.isNull()) return MS::kFailure;
    DMItem convertedMesh;
    DOMMY_RETURN_IF_FAILURE(DMeshInternalData::createFromMayaObject(newOutMeshData, convertedMesh));
    if (!outDMeshes->insert(convertedMesh)) return MS::kFailure;
    outDMeshes->updateBBox();
    MDataHandle output = data.outputValue(outDM, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPFoamMesh")

MStatus DPFoamMesh::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    const MObject aRadiusAttributeDefaultData = fnStringData.create("pscale", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAttribute = tAttr.create("RadiusAttribute", "rattr", MFnData::kString, aRadiusAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));
    
    aRadius = nAttr.create( "Radius", "rad", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    
    
    
    aFrequency = nAttr.create( "Frequency", "fq", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFrequency));
    
    
    aRadiusOffset = nAttr.create( "RadiusOffset", "ro", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusOffset));
    
    aRadiusResultOffset = nAttr.create( "RadiusResultOffset", "rro", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusResultOffset));
    
    //for deform
    aSelfDeform = nAttr.create( "SelfDeform", "sd", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfDeform));

    
    
    
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadius, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFrequency, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusOffset, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusResultOffset, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSelfDeform, outDM ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPFoamMesh", "initialize", std::current_exception());
}
