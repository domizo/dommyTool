//
//  DPUVGenFromSurf.cpp
//  dommyTool
//
//  Created by yuzo on 3/1/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPUVGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DUvParameter.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DAttributePublication.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dpoint_ids.h"
#include "dcurve_ids.h"
#include "dmesh_ids.h"

#include <cmath>
#include <array>
#include <memory>
#include <string>
#include <vector>

// Utilities

namespace {

// Borrowed local points stay owned by the input/output data. Only the
// temporary Maya query point is transformed; stored geometry is unchanged.
struct UvQueryPartition {
    const DPointList *points;
    MMatrix toWorld;
};

bool validUvPartition(const UvQueryPartition &partition, std::size_t count) {
    return partition.points && partition.points->size() == count &&
           dommy::numeric::isFiniteAffineTransform(partition.toWorld);
}

MStatus uvQueryPoint(const UvQueryPartition &partition, std::size_t index, MPoint &point) {
    std::array<double, 3> world;
    if (!dommy::numeric::checkedTransformPoint((*partition.points)[index], partition.toWorld,
                                               world))
        return MS::kInvalidParameter;
    point = MPoint(world[0], world[1], world[2]);
    return MS::kSuccess;
}

template <typename Owner>
MStatus preflightUvPrimitives(const Owner &input, std::size_t count, const std::string &name,
                              std::size_t valueBytes, dommy::point::PointPayloadBudget &workspace) {
    dommy::point::preflightPrimitivePointPayload(input, count, sizeof(DPoint) + valueBytes, name,
                                                 valueBytes, workspace);
    workspace.add(count,
                  sizeof(UvQueryPartition) + sizeof(DScalarDataList) + sizeof(DVec2DataList));
    for (auto primitive = input.beginPrims(); primitive != input.endPrims(); ++primitive)
        if (!dommy::numeric::isFiniteAffineTransform((*primitive)->getTransformMatrix()))
            return MS::kInvalidParameter;
    return MS::kSuccess;
}

template <typename Owner, typename Value>
MStatus publishUvAttribute(Owner &output, const std::string &name,
                           const std::vector<Value> &values) {
    DPAttributeData::Ptr candidate = std::make_shared<TypedDPAttributeData<Value>>(values);
    return dommy::attribute::commitStagedAttribute(output, name, candidate, output.numberOfPoints())
               ? MStatus(MS::kSuccess)
               : MStatus(MS::kFailure);
}

MStatus generateCurveUvs(const MObject &object, const std::vector<UvQueryPartition> &pointLists,
                         std::vector<DScalarDataList> &valueLists) {
    if (pointLists.size() != valueLists.size()) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnNurbsCurve curveFn(object, &status);
    if (!status)
        return status;

    double domainStart = 0.0;
    double domainEnd = 0.0;
    status = curveFn.getKnotDomain(domainStart, domainEnd);
    if (!status)
        return status;

    for (std::size_t listIndex = 0; listIndex < pointLists.size(); ++listIndex) {
        if (!validUvPartition(pointLists[listIndex], valueLists[listIndex].size())) {
            return MS::kInvalidParameter;
        }

        for (std::size_t pointIndex = 0; pointIndex < pointLists[listIndex].points->size();
             ++pointIndex) {
            MPoint point;
            DOMMY_RETURN_IF_FAILURE(uvQueryPoint(pointLists[listIndex], pointIndex, point));

            double parameter = 0.0;
            curveFn.closestPoint(point, &parameter, kMFnNurbsEpsilon, MSpace::kWorld, &status);
            if (!status)
                return status;

            float normalizedParameter = 0.0f;
            if (!dommy::uv::normalizeParameter(parameter, domainStart, domainEnd,
                                               normalizedParameter)) {
                return MS::kInvalidParameter;
            }
            valueLists[listIndex][pointIndex] = normalizedParameter;
        }
    }
    return MS::kSuccess;
}

MStatus generateSurfaceUvs(const MObject &object, const std::vector<UvQueryPartition> &pointLists,
                           std::vector<DVec2DataList> &valueLists) {
    if (pointLists.size() != valueLists.size()) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnNurbsSurface surfaceFn(object, &status);
    if (!status)
        return status;

    for (std::size_t listIndex = 0; listIndex < pointLists.size(); ++listIndex) {
        if (!validUvPartition(pointLists[listIndex], valueLists[listIndex].size())) {
            return MS::kInvalidParameter;
        }

        for (std::size_t pointIndex = 0; pointIndex < pointLists[listIndex].points->size();
             ++pointIndex) {
            MPoint point;
            DOMMY_RETURN_IF_FAILURE(uvQueryPoint(pointLists[listIndex], pointIndex, point));

            double u = 0.0;
            double v = 0.0;
            status = surfaceFn.getParamAtPoint(point, u, v, MSpace::kWorld);
            if (!status ||
                !dommy::numeric::checkedFloatCast(u, valueLists[listIndex][pointIndex][0]) ||
                !dommy::numeric::checkedFloatCast(v, valueLists[listIndex][pointIndex][1])) {
                return status ? MS::kInvalidParameter : status;
            }
        }
    }
    return MS::kSuccess;
}

MStatus generateMeshUvs(const MObject &object, const std::vector<UvQueryPartition> &pointLists,
                        std::vector<DVec2DataList> &valueLists) {
    if (pointLists.size() != valueLists.size()) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnMesh meshFn(object, &status);
    if (!status)
        return status;

    for (std::size_t listIndex = 0; listIndex < pointLists.size(); ++listIndex) {
        if (!validUvPartition(pointLists[listIndex], valueLists[listIndex].size())) {
            return MS::kInvalidParameter;
        }

        for (std::size_t pointIndex = 0; pointIndex < pointLists[listIndex].points->size();
             ++pointIndex) {
            MPoint point;
            DOMMY_RETURN_IF_FAILURE(uvQueryPoint(pointLists[listIndex], pointIndex, point));

            float2 uv = {0.0f, 0.0f};
            status = meshFn.getUVAtPoint(point, uv, MSpace::kWorld);
            if (!status || !std::isfinite(uv[0]) || !std::isfinite(uv[1])) {
                return status ? MS::kInvalidParameter : status;
            }
            valueLists[listIndex][pointIndex][0] = uv[0];
            valueLists[listIndex][pointIndex][1] = uv[1];
        }
    }
    return MS::kSuccess;
}

} // namespace

const MTypeId DPUVGen::typeId(ID_DP_UV_GEN);

// Attribute
MObject DPUVGen::inDD;

MObject DPUVGen::inObject;

MObject DPUVGen::uvName;

MObject DPUVGen::outDD;

void *DPUVGen::creator() {
    return ::dommy::maya::createObject("DPUVGen", [&]() { return new DPUVGen(); });
}

MStatus DPUVGen::compute(const MPlug &plug, MDataBlock &data) try {
    if (plug != outDD)
        return MS::kUnknownParameter;
    MStatus stat;

    if (plug == outDD) {

#pragma mark get object オブジェクトの取得
        MDataHandle objHnd = data.inputValue(inObject, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (objHnd.data().isNull()) {
            return MS::kInvalidParameter;
        }
        MFnData::Type objType = objHnd.type();
        if (objType != MFnData::kNurbsCurve && objType != MFnData::kNurbsSurface &&
            objType != MFnData::kMesh) {
            EchoWarning(name().asChar(), "Not Support input object type.");
            return MS::kFailure;
        }

#pragma mark inDDの取得

        MDataHandle inputDataHandle = data.inputValue(inDD, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MObject dataObj = inputDataHandle.data();
        const unsigned int inputTypeId = inputDataHandle.typeId().id();

        if (dataObj.isNull()) {
            EchoWarning(name().asChar(), "input data is null.");
            return MS::kFailure;
        }

        // get uv attribute name
        const MDataHandle uvNameHandle = data.inputValue(uvName, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MString requestedName = uvNameHandle.asString();
        if (requestedName.length() > dommy::generation::kMaxGeneratedStringByteCount)
            return MS::kInvalidParameter;
        const std::string uvAttrName = requestedName.asChar();
        if (uvAttrName.empty()) {
            return MS::kInvalidParameter;
        }
        const std::size_t valueBytes =
            objType == MFnData::kNurbsCurve ? sizeof(float) : sizeof(DVec2Data);
        dommy::point::PointPayloadBudget workspace;
        workspace.add(uvAttrName.size(), sizeof(char));

        if (inputTypeId == ID_DP_DATA) {

#pragma mark DPoint

            const DPData *inputDPData = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDD, data, inputDPData));
            if (!inputDPData)
                return MS::kFailure;
            dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), workspace);
            const std::size_t inputCount = inputDPData->numberOfPoints();
            if (inputCount != 0U) {
                workspace.add(inputCount, valueBytes);
                workspace.add(1U, sizeof(UvQueryPartition) + sizeof(DScalarDataList) +
                                      sizeof(DVec2DataList));
                dommy::point::preflightGeneratedPointAttribute(inputCount, uvAttrName, valueBytes,
                                                               workspace);
            }
            {

                MFnPluginData pluginData;
                pluginData.create(DPData::id, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);

                DPData *const outputDPData(
                    dommy::maya::pluginDataPointer<DPData>(pluginData, &stat));
                CHECK_MSTATUS_AND_RETURN_IT(stat);

                if (outputDPData) {

                    outputDPData->setDPData(*inputDPData);

                    // get position

                    size_t pointCount = outputDPData->numberOfPoints();

                    if (pointCount) {

                        if (objType == MFnData::kNurbsCurve) {

                            // get position and attribute size
                            std::vector<UvQueryPartition> pointsList;
                            pointsList.push_back({outputDPData->getPositionPtrConst(), MMatrix()});

                            std::vector<DScalarDataList> attrValueList;
                            DScalarDataList values(pointCount, 0.0);
                            attrValueList.push_back(values);

                            // get uv
                            stat = generateCurveUvs(objHnd.data(), pointsList, attrValueList);
                            CHECK_MSTATUS_AND_RETURN_IT(stat);

                            // create attribute

                            DOMMY_RETURN_IF_FAILURE(
                                publishUvAttribute(*outputDPData, uvAttrName, attrValueList[0]));

                        } else if (objType == MFnData::kNurbsSurface || objType == MFnData::kMesh) {

                            // get position and attribute size
                            std::vector<UvQueryPartition> pointsList;
                            pointsList.push_back({outputDPData->getPositionPtrConst(), MMatrix()});

                            std::vector<DVec2DataList> attrValueList;
                            DVec2DataList values(pointCount, DVec2Data(0.0, 0.0));
                            attrValueList.push_back(values);

                            // get uv
                            if (objType == MFnData::kNurbsSurface) {
                                stat = generateSurfaceUvs(objHnd.data(), pointsList, attrValueList);
                                CHECK_MSTATUS_AND_RETURN_IT(stat);
                            } else {
                                stat = generateMeshUvs(objHnd.data(), pointsList, attrValueList);
                                CHECK_MSTATUS_AND_RETURN_IT(stat);
                            }

                            // create attribute

                            DOMMY_RETURN_IF_FAILURE(
                                publishUvAttribute(*outputDPData, uvAttrName, attrValueList[0]));
                        }
                    }

#pragma mark Create DC Output

                    MDataHandle output = data.outputValue(outDD, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DPData>(
                        output, pluginData.object(), DPData::id));
                    output.setClean();
                    return data.setClean(plug);
                }
            }

        } else if (inputTypeId == ID_DC_DATA) {

#pragma mark DCurve

            const DCurveData *inputDCData = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDD, data, inputDCData));
            if (!inputDCData)
                return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(preflightUvPrimitives(
                *inputDCData, inputDCData->numberOfCurves(), uvAttrName, valueBytes, workspace));
            {
                MFnPluginData pluginData;
                pluginData.create(DCurveData::id, &stat);

                if (stat != MS::kSuccess) {
                    EchoWarning(name().asChar(), "Failed to create a new DCurveData object");
                    return stat;
                }

                DCurveData *outDCrvs =
                    dommy::maya::pluginDataPointer<DCurveData>(pluginData, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);

                if (!outDCrvs) {
                    EchoWarning(name().asChar(), "Failed to create a Output DCurveData object");
                    return MS::kFailure;
                }

                if (outDCrvs) {
                    DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));

                    if (outDCrvs->numberOfCurves()) {

                        if (objType == MFnData::kNurbsCurve) {

                            // data
                            std::vector<UvQueryPartition> pointsList;
                            std::vector<DScalarDataList> attrValueList;

                            // get position and attribute size
                            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves();
                                 ++cIt) {

                                pointsList.push_back(
                                    {(*cIt)->getPositionPtrConst(), (*cIt)->getTransformMatrix()});

                                DScalarDataList values((*cIt)->numberOfPoints(), 0.0);

                                attrValueList.push_back(values);
                            }

                            // get uv
                            stat = generateCurveUvs(objHnd.data(), pointsList, attrValueList);
                            CHECK_MSTATUS_AND_RETURN_IT(stat);

                            // create attribute
                            size_t itemOffset = 0;
                            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves();
                                 ++cIt) {

                                if ((*cIt)->numberOfPoints() == attrValueList[itemOffset].size()) {

                                    DOMMY_RETURN_IF_FAILURE(publishUvAttribute(
                                        **cIt, uvAttrName, attrValueList[itemOffset]));
                                }

                                itemOffset++;
                            }

                        } else if (objType == MFnData::kNurbsSurface || objType == MFnData::kMesh) {

                            // data
                            std::vector<UvQueryPartition> pointsList;
                            std::vector<DVec2DataList> attrValueList;

                            // get position and attribute size
                            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves();
                                 ++cIt) {

                                pointsList.push_back(
                                    {(*cIt)->getPositionPtrConst(), (*cIt)->getTransformMatrix()});

                                DVec2DataList values((*cIt)->numberOfPoints(),
                                                     DVec2Data(0.0f, 0.0f));

                                attrValueList.push_back(values);
                            }

                            // get uv
                            if (objType == MFnData::kNurbsSurface) {
                                stat = generateSurfaceUvs(objHnd.data(), pointsList, attrValueList);
                                CHECK_MSTATUS_AND_RETURN_IT(stat);
                            } else {
                                stat = generateMeshUvs(objHnd.data(), pointsList, attrValueList);
                                CHECK_MSTATUS_AND_RETURN_IT(stat);
                            }

                            // create attribute
                            size_t itemOffset = 0;
                            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves();
                                 ++cIt) {

                                if ((*cIt)->numberOfPoints() == attrValueList[itemOffset].size()) {
                                    DOMMY_RETURN_IF_FAILURE(publishUvAttribute(
                                        **cIt, uvAttrName, attrValueList[itemOffset]));
                                }

                                itemOffset++;
                            }
                        }
                    }

#pragma mark Create DC Output

                    MDataHandle output = data.outputValue(outDD, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
                        output, pluginData.object(), DCurveData::id));
                    output.setClean();
                    return data.setClean(plug);
                }
            }

        } else if (inputTypeId == ID_DM_DATA) {

#pragma mark DMesh

            const DMeshData *inputDMData = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDD, data, inputDMData));
            if (!inputDMData)
                return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(preflightUvPrimitives(*inputDMData, inputDMData->numberOfMesh(),
                                                          uvAttrName, valueBytes, workspace));
            for (auto mesh = inputDMData->beginMeshes(); mesh != inputDMData->endMeshes(); ++mesh) {
                dommy::point::preflightAttributePayload(
                    (*mesh)->beginFaceAttr(), (*mesh)->endFaceAttr(),
                    static_cast<std::size_t>((*mesh)->numFace()), workspace);
                dommy::point::preflightAttributePayload((*mesh)->beginPVtxAttr(),
                                                        (*mesh)->endPVtxAttr(),
                                                        (*mesh)->numberOfConnections(), workspace);
                dommy::point::preflightGroupPayload((*mesh)->beginFaceGrp(), (*mesh)->endFaceGrp(),
                                                    workspace);
                dommy::point::preflightGroupPayload((*mesh)->beginEdgeGrp(), (*mesh)->endEdgeGrp(),
                                                    workspace);
            }
            {
                MFnPluginData pluginData;
                pluginData.create(DMeshData::id, &stat);

                if (stat != MS::kSuccess) {
                    EchoWarning(name().asChar(), "Failed to create a new DMeshData object");
                    return stat;
                }

                DMeshData *outDMeshes =
                    dommy::maya::pluginDataPointer<DMeshData>(pluginData, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);

                if (!outDMeshes) {
                    EchoWarning(name().asChar(), "Failed to create a Output DMeshData object");
                    return MS::kFailure;
                }

                if (outDMeshes) {
                    DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));

                    if (outDMeshes->numberOfMesh()) {

                        if (objType == MFnData::kNurbsCurve) {

                            // data
                            std::vector<UvQueryPartition> pointsList;
                            std::vector<DScalarDataList> attrValueList;

                            // get position and attribute size
                            for (auto cIt = outDMeshes->beginMeshes();
                                 cIt != outDMeshes->endMeshes(); ++cIt) {

                                pointsList.push_back(
                                    {(*cIt)->getPositionPtrConst(), (*cIt)->getTransformMatrix()});

                                DScalarDataList values((*cIt)->numberOfPoints(), 0.0);

                                attrValueList.push_back(values);
                            }

                            // get uv
                            stat = generateCurveUvs(objHnd.data(), pointsList, attrValueList);
                            CHECK_MSTATUS_AND_RETURN_IT(stat);

                            // create attribute
                            size_t itemOffset = 0;
                            for (auto cIt = outDMeshes->beginMeshes();
                                 cIt != outDMeshes->endMeshes(); ++cIt) {

                                if ((*cIt)->numberOfPoints() == attrValueList[itemOffset].size()) {
                                    DOMMY_RETURN_IF_FAILURE(publishUvAttribute(
                                        **cIt, uvAttrName, attrValueList[itemOffset]));
                                }

                                itemOffset++;
                            }

                        } else if (objType == MFnData::kNurbsSurface || objType == MFnData::kMesh) {

                            // data
                            std::vector<UvQueryPartition> pointsList;
                            std::vector<DVec2DataList> attrValueList;

                            // get position and attribute size
                            for (auto cIt = outDMeshes->beginMeshes();
                                 cIt != outDMeshes->endMeshes(); ++cIt) {

                                pointsList.push_back(
                                    {(*cIt)->getPositionPtrConst(), (*cIt)->getTransformMatrix()});

                                DVec2DataList values((*cIt)->numberOfPoints(),
                                                     DVec2Data(0.0f, 0.0f));

                                attrValueList.push_back(values);
                            }

                            // get uv
                            if (objType == MFnData::kNurbsSurface) {
                                stat = generateSurfaceUvs(objHnd.data(), pointsList, attrValueList);
                                CHECK_MSTATUS_AND_RETURN_IT(stat);
                            } else {
                                stat = generateMeshUvs(objHnd.data(), pointsList, attrValueList);
                                CHECK_MSTATUS_AND_RETURN_IT(stat);
                            }

                            // create attribute
                            size_t itemOffset = 0;
                            for (auto cIt = outDMeshes->beginMeshes();
                                 cIt != outDMeshes->endMeshes(); ++cIt) {

                                if ((*cIt)->numberOfPoints() == attrValueList[itemOffset].size()) {
                                    DOMMY_RETURN_IF_FAILURE(publishUvAttribute(
                                        **cIt, uvAttrName, attrValueList[itemOffset]));
                                }

                                itemOffset++;
                            }
                        }
                    }

#pragma mark Create DM Output

                    MDataHandle output = data.outputValue(outDD, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
                        output, pluginData.object(), DMeshData::id));
                    output.setClean();
                    return data.setClean(plug);
                }
            }
        }
    }

    // The requested output is recognized; the input payload is unsupported.
    return MS::kInvalidParameter;
}
DOMMY_COMPUTE_CATCH("DPUVGen")

MStatus DPUVGen::initialize() try {
    MStatus status;
    MFnGenericAttribute generic;
    MFnTypedAttribute typed;

    inDD = generic.create("inDD", "idd", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(generic.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(generic.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(generic.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(generic.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    inObject = generic.create("inObject", "ino", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(generic.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(generic.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    MFnStringData stringData;
    const MObject defaultName = stringData.create("uv", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    uvName = typed.create("uvName", "uvn", MFnData::kString, defaultName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typed.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(uvName));

    outDD = generic.create("outDD", "odd", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(generic.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(generic.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(generic.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(generic.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(uvName, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, outDD));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPUVGen", "initialize",
                                                  std::current_exception());
}
