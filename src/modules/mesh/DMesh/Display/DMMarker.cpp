//
//  DMMarker.cpp
//  dommyTool
//
//  Created by yuzo on 2018/01/15.
//

#include "DMMarker.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DViewportAttributeConversion.h"
#include "common/utility/DMayaMeshTraversal.h"

#include <cmath>
#include <sstream>
#include <algorithm>

#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"
#include "DNodeOperation.h"

#include "DColorOperation.h"
#include "DStringOp.h"

MTypeId DMMarker::typeId(ID_DM_MARKER);
const MString DMMarker::typeName("DMMarker");
MString DMMarker::classification("drawdb/geometry/DMMarker");
MString DMMarker::registrantId("DMMarkerPlugin");

MObject DMMarker::inDM;

// index
MObject DMMarker::aShowIndex;
MObject DMMarker::aIndexType;

// Display Option
MObject DMMarker::aPointSize;
MObject DMMarker::aLineWidth;
MObject DMMarker::aSolidColor;

// attribute info
MObject DMMarker::aAllAttributeNames;
MObject DMMarker::aIsColor;
MObject DMMarker::aIsValue;
MObject DMMarker::aIsVector;

MObject DMMarker::aVisibleAttributeName;

MObject DMMarker::aIsBlackTransparency;
MObject DMMarker::aIsDrawHeads;
MObject DMMarker::aVelocityScale;

// group
MObject DMMarker::aAllGroupNames;
MObject DMMarker::aIsGroup;
MObject DMMarker::aGroupName;
MObject DMMarker::aGroupColor;
MObject DMMarker::aGroupTransparency;

MObject DMMarker::aNodeInfo;

void* DMMarker::creator() {
    return ::dommy::maya::createObject("DMMarker", [&]() {
        return new DMMarker();
    });
}

MStatus DMMarker::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug == aNodeInfo || plug == aAllAttributeNames || plug == aAllGroupNames) {
        MPlug dmPlug = MPlug(thisMObject(), DMMarker::inDM);

        MStatus outputStatus;
        const bool connected = dmPlug.isConnected(&outputStatus);
        if (!outputStatus) return outputStatus;
        const DMeshData* dMeshes = NULL;
        if (connected) {
            outputStatus = getValidInputDM(inDM, data, dMeshes);
            if (!outputStatus) return outputStatus;
        }
        if (plug == aAllAttributeNames || plug == aAllGroupNames) {
            DStringList names;
            if (dMeshes) {
                names = plug == aAllAttributeNames
                    ? dMeshes->getAllAttributeList() : dMeshes->getAllGroupList();
            }
            const MString text(mergeStringList(names).c_str());
            MDataHandle output = data.outputValue(
                plug == aAllAttributeNames ? aAllAttributeNames : aAllGroupNames,
                &outputStatus);
            if (!outputStatus) return outputStatus;
            output.set(text);
            return data.setClean(plug);
        }

        std::stringstream infoStr;

        infoStr << "DMesh Info\n";

        if (connected) {



            if (dMeshes) {

                size_t totalVtx;
                dMeshes->getTotalVertex(totalVtx);

                // position

                infoStr << "    DMesh count: " << dMeshes->numberOfMesh() << "\n";
                infoStr << "    total vtx count: " << totalVtx << "\n";

                infoStr << "    uv map: " << dMeshes->hasUVs() << "\n";

                if (dMeshes->numberOfMesh()) {
                    // attribute
                    size_t attrCount = dMeshes->numberOfAttributes();
                    if (attrCount) {
                        infoStr << "    mesh attribute count: " << attrCount << "\n";
                        for (auto itr = dMeshes->beginAttr(); itr != dMeshes->endAttr(); ++itr) {
                            infoStr << "        @" << itr->first << " : " << itr->second->typeName()
                                    << "\n";
                        }
                    }

                    // point attribute
                    size_t attrPtCount = dMeshes->numberOfPointAttribute(false);
                    if (attrPtCount) {
                        infoStr << "    point attribute count: " << attrPtCount << "\n";
                        for (auto itr : dMeshes->getPointAttributeInfo()) {
                            infoStr << "        " << itr << "\n";
                        }
                    }

                    // face attribute
                    size_t faceAttrCount = dMeshes->numberOfCompAttr(D_COMP_FACE);
                    if (faceAttrCount) {
                        infoStr << "    face attribute count: " << faceAttrCount << "\n";
                        for (auto itr : dMeshes->getCompAttrInfo(D_COMP_FACE)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }

                    // per vertex attribute
                    size_t pVtxCount = dMeshes->numberOfCompAttr(D_COMP_PER_VTX);
                    if (pVtxCount) {
                        infoStr << "    per vtx attribute count: " << pVtxCount << "\n";
                        for (auto itr : dMeshes->getCompAttrInfo(D_COMP_PER_VTX)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }

                    // primitive group
                    size_t grpCount = dMeshes->numberOfGroups();
                    if (grpCount) {
                        infoStr << "    primitive group count: " << grpCount << "\n";
                        for (auto itr = dMeshes->beginGrp(); itr != dMeshes->endGrp(); ++itr) {
                            infoStr << "        *" << itr->first << "\n";
                        }
                    }

                    // point group
                    size_t grpPtCount = dMeshes->numberOfCompGroup(D_COMP_POINT);
                    if (grpPtCount) {
                        infoStr << "    point group count: " << grpPtCount << "\n";
                        for (auto itr : dMeshes->getCompGroupInfo(D_COMP_POINT)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }

                    // face group
                    size_t grpFaceCount = dMeshes->numberOfCompGroup(D_COMP_FACE);
                    if (grpFaceCount) {
                        infoStr << "    face group count: " << grpFaceCount << "\n";
                        for (auto itr : dMeshes->getCompGroupInfo(D_COMP_FACE)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }

                    // edge group
                    size_t grpEdgeCount = dMeshes->numberOfCompGroup(D_COMP_EDGE);
                    if (grpEdgeCount) {
                        infoStr << "    edge group count: " << grpEdgeCount << "\n";
                        for (auto itr : dMeshes->getCompGroupInfo(D_COMP_EDGE)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }
                }

            } else {

                infoStr << "DMesh data is empty.";
            }
        } else {

            infoStr << "inDM plug is not connected.";
        }

        MString str = dommy::text::checkedStreamText(infoStr, "DMMarker info formatting failed").c_str();
        MDataHandle strHandle = data.outputValue(aNodeInfo, &outputStatus);
        if (!outputStatus) return outputStatus;
        strHandle.set(str);

        return data.setClean(plug);
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DMMarker")

bool DMMarker::isBounded() const {
    return true;
}

MBoundingBox DMMarker::boundingBox() const try {

    MPoint corner1(-0.5, -0.5, -0.5);
    MPoint corner2(0.5, 0.5, 0.5);

    MPlug dmPlug = MPlug(thisMObject(), DMMarker::inDM);

    MStatus connectionStatus;
    const bool connected = dmPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DMMarker: unable to query input connection for bounds.");
    } else if (connected) {

        MObject obj;
        const DMeshData* dMeshes = getInputDMfromPlug(dmPlug, obj);

        if (dMeshes && dMeshes->numberOfMesh()) {

            MBoundingBox mBBox = dMeshes->getBoundingBox();
            if (!dommy::maya::isFiniteBoundingBox(mBBox) || mBBox.width() > 1000000.0 ||
                mBBox.height() > 1000000.0 || mBBox.depth() > 1000000.0) {
                mBBox = MBoundingBox(MPoint(-100000.0, -100000.0, -100000.0),
                                     MPoint(100000.0, 100000.0, 100000.0));
            }

            return mBBox;
        }
    }

    return MBoundingBox(corner1, corner2);
}

DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("DMMarker")

void DMMarker::draw(M3dView&, const MDagPath&, M3dView::DisplayStyle, M3dView::DisplayStatus) {}

MStatus DMMarker::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnGenericAttribute gAttr;
    MFnStringData fnStringData;

    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

#pragma mark Index
    aShowIndex = nAttr.create("ShowIndex", "sinx", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aShowIndex));

    MFnEnumAttribute idxEnumFn;
    aIndexType = idxEnumFn.create("IndexType", "idxty", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(idxEnumFn.addField("Primitive", 0));
    DOMMY_RETURN_IF_FAILURE(idxEnumFn.addField("Point", 1));
    DOMMY_RETURN_IF_FAILURE(idxEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIndexType));

#pragma mark display option
    aPointSize = nAttr.create("PointSize", "ptsz", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointSize));

    aLineWidth = nAttr.create("LineWidth", "lnw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLineWidth));

    aSolidColor = nAttr.create("SolidColor", "scol", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 0.0f, 1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolidColor));

#pragma mark mesh attribute

    const MObject aAllAttributeNamesDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAllAttributeNames = tAttr.create("AllAttributeNames", "allattrs", MFnData::kString,
                                      aAllAttributeNamesDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAllAttributeNames));
    aIsColor = nAttr.create("IsColor", "iscol", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsColor));

    aIsValue = nAttr.create("IsValue", "isval", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsValue));

    aIsVector = nAttr.create("IsVector", "isvec", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVector));

    MObject visAttrName = fnStringData.create("Cd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVisibleAttributeName =
        tAttr.create("VisibleAttributeName", "visatn", MFnData::kString, visAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVisibleAttributeName));

    aIsBlackTransparency = nAttr.create("IsBlackTransparency", "ibtrans", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsBlackTransparency));

    aIsDrawHeads = nAttr.create("IsDrawHeads", "idh", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDrawHeads));

    aVelocityScale = nAttr.create("VelocityScale", "vels", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityScale));

    // Group
    const MObject aAllGroupNamesDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAllGroupNames =
        tAttr.create("AllGroupNames", "allgrps", MFnData::kString, aAllGroupNamesDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAllGroupNames));

    aIsGroup = nAttr.create("IsGroup", "isgrp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsGroup));

    const MObject aGroupNameDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGroupName =
        tAttr.create("GroupName", "grpnm", MFnData::kString, aGroupNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));

    aGroupColor = nAttr.create("GroupColor", "grpcol", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f, 0.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupColor));

    aGroupTransparency = nAttr.create("GroupTransparency", "grptrans", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupTransparency));

    MObject emptyStr = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, emptyStr, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setAffectsAppearance(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNodeInfo));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aAllGroupNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aAllAttributeNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aNodeInfo));
//    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSelectAttributeNames, aNodeInfo));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMMarker", "initialize", std::current_exception());
}

#pragma mark DMMarkerOverride

DMMarkerOverride::DMMarkerOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mIndexUIShader(NULL)
, mColorMeshShader(NULL)
, mVectorUIShader(NULL)
, mValueUIShader(NULL)
, mValue2UIShader(NULL)
, mValue3UIShader(NULL)
, mGroupPolyShader(NULL)
, mGroupLineShader(NULL)
, mGroupPointShader(NULL)
, mDGNode(obj)
, mDataChanged(true)
{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DMMarkerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dIntegerNumericShader, mIndexUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVSolidShader, mColorMeshShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloatNumericShader, mValueUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat2NumericShader, mValue2UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat3NumericShader, mValue3UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dSolidShader, mGroupPolyShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mGroupLineShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mGroupPointShader);
}



DMMarkerOverride::~DMMarkerOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DMMarkerOverride");
    shaderRelease.release(mIndexUIShader);
    shaderRelease.release(mColorMeshShader);
    shaderRelease.release(mVectorUIShader);
    shaderRelease.release(mValueUIShader);
    shaderRelease.release(mValue2UIShader);
    shaderRelease.release(mValue3UIShader);
    shaderRelease.release(mGroupPolyShader);
    shaderRelease.release(mGroupLineShader);
    shaderRelease.release(mGroupPointShader);
}


MHWRender::DrawAPI DMMarkerOverride::supportedDrawAPIs() const
{
    // this plugin supports both GL and DX
    return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark primitive attribute method
template <typename T>
inline void setMeshSingleValueColor(const DMeshData &inputData, const std::string &name, const bool &isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::ScalarColor, isBlackTrans);
    finValues.swap(candidate);
}

template <typename T>
inline void setMesh1DVector(const DMeshData& inputData, const std::string& name,
                            std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::ScalarVector);
    finValues.swap(candidate);
}

template <typename T>
inline void setMeshSimpleVectorValue(const DMeshData& inputData, const std::string& name,
                                     std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::Vector3);
    finValues.swap(candidate);
}

template <typename T>
inline void setMeshTwoVectorValue(const DMeshData& inputData, const std::string& name,
                                  std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::Vector2);
    finValues.swap(candidate);
}

template <typename T>
inline void setMesh2To3DVectorValue(const DMeshData& inputData, const std::string& name,
                                    std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::Vector2XZ);
    finValues.swap(candidate);
}

template <typename T>
inline void setMeshTwoVecColor(const DMeshData& inputData, const std::string& name,
                               bool isBlackTrans, std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::Vector2Color, isBlackTrans);
    finValues.swap(candidate);
}

template <typename T>
inline void setMeshThreeVecColor(const DMeshData& inputData, const std::string& name,
                                 bool isBlackTrans, std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::Vector3Color, isBlackTrans);
    finValues.swap(candidate);
}

template <typename T>
inline void setMeshSimpleSingleValue(const DMeshData& inputData, const std::string& name,
                                     std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfMesh(),
        inputData.beginMeshes(), inputData.endMeshes(), dommy::viewport::AttributeDisplayMode::Scalar);
    finValues.swap(candidate);
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark point attribute method
template <typename T>
inline void setMeshPointSingleValueColor(const DMeshData& inputData, const std::string& name,
                                         bool isBlackTrans, std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::ScalarLuminanceColor, isBlackTrans);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPoint1DVector(const DMeshData& inputData, const std::string& name,
                                 std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::ScalarVector);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPointSimpleVectorValue(const DMeshData& inputData, const std::string& name,
                                          std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::Vector3);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPointTwoVectorValue(const DMeshData& inputData, const std::string& name,
                                       std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::Vector2);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPoint2To3DVectorValue(const DMeshData& inputData, const std::string& name,
                                         std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::Vector2XZ);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPointTwoVecColor(const DMeshData& inputData, const std::string& name,
                                    bool isBlackTrans, std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::Vector2Color, isBlackTrans);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPointThreeVecColor(const DMeshData& inputData, const std::string& name,
                                      bool isBlackTrans, std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::Vector3Color, isBlackTrans);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setMeshPointSimpleSingleValue(const DMeshData& inputData, const std::string& name,
                                          std::vector<float>& finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginMeshes(), inputData.endMeshes(), name,
        dommy::viewport::AttributeDisplayMode::Scalar);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

/////////////////////////////////////////////////////////////////////////////////////////////

// Index
static const MString DMeshIndexItemName_ = "DMeshIndexItem";
// Color
static const MString DMeshColorItemName_ = "DMeshColorItem";
// Vector
static const MString DMeshVecItemName_ = "DMeshVecItem";
// Value
static const MString DMeshVal1ItemName_ = "DMeshVal1Item";
static const MString DMeshVal2ItemName_ = "DMeshVal2Item";
static const MString DMeshVal3ItemName_ = "DMeshVal3Item";
// Group
static const MString DMGroupPolyItemName_ = "DMGroupPolyItem";
static const MString DMGroupLineItemName_ = "DMGroupLineItem";
static const MString DMGroupPointItemName_ = "DMGroupPointItem";

namespace {

void clearMeshMarkerGroup(DMMarkerOverride::DrawData& data) noexcept {
    data.grpPtBuffer.clear();
    data.grpIndexBuffer.clear();
    data.totalGroupPointCount = 0U;
    data.isGroup = false;
}

void clearMeshMarkerGeometry(DMMarkerOverride::DrawData& data) noexcept {
    data.mPointBuffer.clear();
    data.indexBuffer.clear();
    data.meshFirstIndex.clear();
    data.colBuffer.clear();
    data.vecBuffer.clear();
    data.val3Buffer.clear();
    clearMeshMarkerGroup(data);
    data.pointCount = 0U;
    data.primCount = 0U;
    data.valType = DP_VAL_FLOAT;
}

} // namespace

void DMMarkerOverride::updateDG() try {
    mDataChanged = true;
    clearMeshMarkerGeometry(dData);

    MStatus stat;

    // Color
    if (readPlugValue(mDGNode, DMMarker::aSolidColor, dData.solidColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.solidColor)) {
        dData.solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (readPlugValue(mDGNode, DMMarker::aGroupColor, dData.groupColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.groupColor)) {
        dData.groupColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    double groupAlpha = 1.0;
    if (readPlugValue(mDGNode, DMMarker::aGroupTransparency, groupAlpha) != MS::kSuccess ||
        !std::isfinite(groupAlpha)) {
        groupAlpha = 1.0;
    }
    dData.groupAlpha = static_cast<float>(std::max(0.0, std::min(1.0, groupAlpha)));

    // index
    if (readPlugValue(mDGNode, DMMarker::aShowIndex, dData.showIndex) != MS::kSuccess ||
        readPlugValue(MPlug(mDGNode, DMMarker::aIndexType), dData.indexType) != MS::kSuccess) return;

    double pointSize = 3.0;
    if (readPlugValue(mDGNode, DMMarker::aPointSize, pointSize) != MS::kSuccess ||
        !std::isfinite(pointSize) || pointSize < 0.0) {
        pointSize = 3.0;
    }
    dData.pointSize = static_cast<float>(pointSize);

    double lineWidth = 1.0;
    if (readPlugValue(mDGNode, DMMarker::aLineWidth, lineWidth) != MS::kSuccess ||
        !std::isfinite(lineWidth) || lineWidth < 0.0) {
        lineWidth = 1.0;
    }
    dData.lineWidth = static_cast<float>(lineWidth);

    // attribte info
    if (readPlugValue(mDGNode, DMMarker::aIsVector, dData.isVector) != MS::kSuccess ||
        readPlugValue(mDGNode, DMMarker::aIsColor, dData.isColor) != MS::kSuccess ||
        readPlugValue(mDGNode, DMMarker::aIsValue, dData.isValue) != MS::kSuccess) return;

    if (readPlugValue(mDGNode, DMMarker::aIsDrawHeads, dData.isDrawHeads) != MS::kSuccess ||
        readPlugValue(mDGNode, DMMarker::aIsBlackTransparency, dData.isBlackTransparency) != MS::kSuccess) return;
    double velocityScale = 1.0;
    if (readPlugValue(mDGNode, DMMarker::aVelocityScale, velocityScale) != MS::kSuccess ||
        !std::isfinite(velocityScale) || velocityScale < 0.0) {
        velocityScale = 1.0;
    }
    dData.velocityScale = static_cast<float>(velocityScale);

    dData.infoClass = 0;
    bool isShowAttr = dData.isVector || dData.isColor || dData.isValue;
    MString visibleAttribute;
    if (readPlugValue(mDGNode, DMMarker::aVisibleAttributeName, visibleAttribute) != MS::kSuccess) return;
    const std::string visAttrStr(visibleAttribute.asChar());

    // group
    if (readPlugValue(mDGNode, DMMarker::aIsGroup, dData.isGroup) != MS::kSuccess) return;
    bool isShowGrp = dData.isGroup;
    MString groupName;
    if (readPlugValue(mDGNode, DMMarker::aGroupName, groupName) != MS::kSuccess) return;
    const std::string grpNameStr(groupName.asChar());

    MPlug dmPlug(mDGNode, DMMarker::inDM);
    if (dmPlug.isNull()) return;
    MStatus connectionStatus;
    const bool connected = dmPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DMMarker: unable to query input connection for drawing.");
        return;
    }
    if (connected) {

        // get dmesh
        MObject obj;
        const DMeshData* dMeshes = NULL;
        const MStatus dataStatus =
            dommy::maya::getPluginDataFromPlug(dmPlug, DMeshData::id, obj, dMeshes);
        if (!dataStatus) {
            MGlobal::displayError("DMMarker: connected input is not valid DMeshData.");
            return;
        }

        // check dmesh
        if (dMeshes && dMeshes->numberOfMesh()) {

#pragma mark attribute check
            if (isShowAttr && !visAttrStr.empty()) {
                DComponentType compType = dMeshes->getAttributeComponentType(visAttrStr);
                if (compType == D_COMP_POINT) {
                    dData.infoClass = 1;
                } else if (compType == D_COMP_PRIM) {
                    dData.infoClass = 0;
                } else {
                    dData.isColor = dData.isValue = dData.isVector = false;
                }
            } // check visible attribute
#pragma mark group check
            if (isShowGrp && !grpNameStr.empty()) {
                DComponentType compType = dMeshes->getGroupComponentType(grpNameStr);
                if (compType == D_COMP_PRIM) {
                    dData.grpType = D_COMP_PRIM;
                } else if (compType == D_COMP_POINT) {
                    dData.grpType = D_COMP_POINT;
                } else if (compType == D_COMP_EDGE) {
                    dData.grpType = D_COMP_EDGE;
                } else if (compType == D_COMP_FACE) {
                    dData.grpType = D_COMP_FACE;
                } else {
                    dData.isGroup = false;
                }
            } // check visible group

            // set base point count
            const std::size_t meshCount = dMeshes->numberOfMesh();
            std::size_t totalVtx = 0U;
            if (!dMeshes->getCheckedTotalVertex(totalVtx) ||
                totalVtx > dommy::viewport::kMaxViewportVertexCount) {
                MGlobal::displayError("DMMarker: mesh points exceed the viewport budget.");
                return;
            }

#pragma mark set point count
            std::size_t pointScalarCount = 0;
            if (!dommy::geometry::checkedTupleScalarCount(totalVtx, 3, pointScalarCount) ||
                pointScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                MGlobal::displayError("DMMarker: point buffer exceeds the viewport budget.");
                return;
            }
            dData.mPointBuffer.resize(pointScalarCount);

            size_t vIndex = 0;
            size_t vOffset = 0;
            std::size_t processedMeshCount = 0U;

            for (auto it = dMeshes->beginMeshes(), emIt = dMeshes->endMeshes(); it != emIt; ++it) {
                if (!*it || (*it)->numberOfPoints() == 0U) {
                    MGlobal::displayError(
                        "DMMarker: empty or invalid mesh cannot provide a display anchor.");
                    return;
                }
                dData.meshFirstIndex.push_back(vOffset);

                // mesh dataの生成
                MObject mObj;
                stat = generateMMeshFromDmesh(*it->get(), mObj);
                if (!stat) {
                    MGlobal::displayError("DMMarker: unable to create Maya mesh data.");
                    return;
                }

                MFnMesh meshFn(mObj, &stat);
                if (!stat) {
                    MGlobal::displayError("DMMarker: unable to inspect Maya mesh data.");
                    return;
                }

                // 三角形データ
                MIntArray triCnts, trVerts;
                stat = meshFn.getTriangles(triCnts, trVerts);
                if (!stat) {
                    MGlobal::displayError("DMMarker: unable to read triangle topology.");
                    return;
                }

                // addPoint
                const DPointList* const sourcePoints = (*it)->getPositionPtrConst();
                if (!sourcePoints) {
                    MGlobal::displayError("DMMarker: mesh has no point storage.");
                    return;
                }
                const DPointList pts =
                    (*it)->isIdentity() ? *sourcePoints : (*it)->getPointsInWorld();

                size_t count = pts.size();
                if (count != (*it)->numberOfPoints()) {
                    MGlobal::displayError("DMMarker: mesh point storage is inconsistent.");
                    return;
                }

                tbb::blocked_range<size_t> range =
                    tbb::blocked_range<size_t>(0, count, DGetGlobalProcessNum(count));
                tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& r) {
                    for (size_t i = r.begin(); i != r.end(); i++) {
                        size_t index = i * 3;
                        dData.mPointBuffer[index + vIndex] = pts[i][0];
                        dData.mPointBuffer[index + 1 + vIndex] = pts[i][1];
                        dData.mPointBuffer[index + 2 + vIndex] = pts[i][2];
                    }
                });
                vIndex += count * 3;

#pragma mark flat shade

                if (dData.isColor) {
                    std::size_t nextIndexCount = 0U;
                    if (!dommy::memory::checkedAdd(dData.indexBuffer.size(),
                                                   static_cast<std::size_t>(trVerts.length()),
                                                   nextIndexCount) ||
                        nextIndexCount > dommy::viewport::kMaxViewportIndexCount) {
                        MGlobal::displayError(
                            "DMMarker: triangle indices exceed the viewport budget.");
                        return;
                    }
                    dData.indexBuffer.reserve(nextIndexCount);
                    for (size_t i = 0, tfin = trVerts.length(); i < tfin; i++) {
                        if (trVerts[i] < 0 || static_cast<std::size_t>(trVerts[i]) >= count) {
                            MGlobal::displayError("DMMarker: triangle index is out of range.");
                            return;
                        }
                        dData.indexBuffer.push_back(static_cast<unsigned int>(
                            static_cast<std::size_t>(trVerts[i]) + vOffset));
                    }
                }

                vOffset += (*it)->numberOfPoints();
                ++processedMeshCount;
            }

            if (processedMeshCount != meshCount || vOffset != totalVtx ||
                vIndex != pointScalarCount) {
                MGlobal::displayError("DMMarker: generated viewport layout is inconsistent.");
                return;
            }
            dData.pointCount = totalVtx;
            dData.primCount = meshCount;

#pragma mark visible group
            if (dData.isGroup) {
                if (dData.grpType == D_COMP_PRIM) {
#pragma mark primitive group
                    if (dMeshes->isExistGrp(grpNameStr)) {
                        DPGroupData::ConstPtr groupData = dMeshes->getGrp(grpNameStr);
                        if (!groupData) {
                            MGlobal::displayError("DMMarker: primitive group data is unavailable.");
                            clearMeshMarkerGroup(dData);
                            return;
                        }
                        const DPIdList idsList = groupData->getGroupIds();
                        if (idsList.size() == meshCount) {
                            std::size_t meshIndex = 0U;
                            std::size_t totalPoint = 0U;

                            for (auto mesh = dMeshes->beginMeshes(), endMesh = dMeshes->endMeshes();
                                 mesh != endMesh; ++mesh, ++meshIndex) {
                                if (!idsList[meshIndex]) {
                                    continue;
                                }

                                MObject mObj;
                                if (generateMMeshFromDmesh(*mesh->get(), mObj) != MS::kSuccess) {
                                    MGlobal::displayError(
                                        "DMMarker: unable to create primitive group mesh data.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }

                                MFnMesh meshFn(mObj, &stat);
                                MIntArray triangleCounts;
                                MIntArray triangleVertices;
                                if (!stat ||
                                    meshFn.getTriangles(triangleCounts, triangleVertices) !=
                                        MS::kSuccess) {
                                    MGlobal::displayError(
                                        "DMMarker: unable to read primitive group topology.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }

                                const DPointList* const sourcePoints =
                                    (*mesh)->getPositionPtrConst();
                                if (!sourcePoints) {
                                    MGlobal::displayError(
                                        "DMMarker: primitive group has no point storage.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }
                                const DPointList points = (*mesh)->isIdentity()
                                                              ? *sourcePoints
                                                              : (*mesh)->getPointsInWorld();
                                const std::size_t pointCount = points.size();
                                std::size_t nextTotalPoint = 0U;
                                std::size_t groupScalarCount = 0U;
                                std::size_t nextIndexCount = 0U;
                                if (pointCount != (*mesh)->numberOfPoints() ||
                                    !dommy::memory::checkedAdd(totalPoint, pointCount,
                                                               nextTotalPoint) ||
                                    nextTotalPoint > dommy::viewport::kMaxViewportVertexCount ||
                                    !dommy::geometry::checkedTupleScalarCount(nextTotalPoint, 3U,
                                                                              groupScalarCount) ||
                                    groupScalarCount > dommy::viewport::kMaxViewportScalarCount ||
                                    !dommy::memory::checkedAdd(
                                        dData.grpIndexBuffer.size(),
                                        static_cast<std::size_t>(triangleVertices.length()),
                                        nextIndexCount) ||
                                    nextIndexCount > dommy::viewport::kMaxViewportIndexCount) {
                                    MGlobal::displayError(
                                        "DMMarker: primitive group exceeds the viewport budget.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }

                                dData.grpPtBuffer.reserve(groupScalarCount);
                                for (DPointList::const_iterator point = points.begin();
                                     point != points.end(); ++point) {
                                    dData.grpPtBuffer.push_back((*point)[0]);
                                    dData.grpPtBuffer.push_back((*point)[1]);
                                    dData.grpPtBuffer.push_back((*point)[2]);
                                }

                                dData.grpIndexBuffer.reserve(nextIndexCount);
                                for (unsigned int triangleIndex = 0U;
                                     triangleIndex < triangleVertices.length(); ++triangleIndex) {
                                    const int localIndex = triangleVertices[triangleIndex];
                                    if (localIndex < 0 ||
                                        static_cast<std::size_t>(localIndex) >= pointCount) {
                                        MGlobal::displayError(
                                            "DMMarker: primitive group index is out of range.");
                                        clearMeshMarkerGroup(dData);
                                        return;
                                    }
                                    dData.grpIndexBuffer.push_back(static_cast<unsigned int>(
                                        totalPoint + static_cast<std::size_t>(localIndex)));
                                }
                                totalPoint = nextTotalPoint;
                            }
                            dData.totalGroupPointCount = totalPoint;
                        } else {
                            clearMeshMarkerGroup(dData);
                        }
                    } else {
                        clearMeshMarkerGroup(dData);
                    }
                } else if (dData.grpType == D_COMP_POINT) {
#pragma mark point group
                    std::size_t totalPoint = 0U;
                    for (auto mesh = dMeshes->beginMeshes(), endMesh = dMeshes->endMeshes();
                         mesh != endMesh; ++mesh) {
                        if ((*mesh)->isExistGrp(grpNameStr)) {
                            DPGroupData::ConstPtr groupData = (*mesh)->getGrp(grpNameStr);
                            if (!groupData) {
                                MGlobal::displayError("DMMarker: point group data is unavailable.");
                                clearMeshMarkerGroup(dData);
                                return;
                            }
                            const DPIdList idsList = groupData->getGroupIds();
                            const DPointList worldPoints = (*mesh)->getPointsInWorld();
                            if (idsList.size() == (*mesh)->numberOfPoints() &&
                                worldPoints.size() == idsList.size()) {
                                std::size_t pointIndex = 0U;
                                for (DPointList::const_iterator point = worldPoints.begin();
                                     point != worldPoints.end(); ++point, ++pointIndex) {
                                    if (!idsList[pointIndex]) {
                                        continue;
                                    }

                                    dData.grpPtBuffer.push_back((*point)[0]);
                                    dData.grpPtBuffer.push_back((*point)[1]);
                                    dData.grpPtBuffer.push_back((*point)[2]);
                                    ++totalPoint;
                                }
                            }
                        }
                    }

                    dData.grpIndexBuffer.reserve(totalPoint);
                    for (std::size_t pointIndex = 0U; pointIndex < totalPoint; ++pointIndex) {
                        dData.grpIndexBuffer.push_back(static_cast<unsigned int>(pointIndex));
                    }
                    dData.totalGroupPointCount = totalPoint;
                } else if (dData.grpType == D_COMP_FACE) {
#pragma mark face group
                    std::size_t totalPoint = 0U;
                    for (auto mesh = dMeshes->beginMeshes(), endMesh = dMeshes->endMeshes();
                         mesh != endMesh; ++mesh) {
                        if ((*mesh)->isExistCompGrp(D_COMP_FACE, grpNameStr)) {
                            DPGroupData::ConstPtr groupData =
                                (*mesh)->getCompGrp(D_COMP_FACE, grpNameStr);
                            if (!groupData) {
                                MGlobal::displayError("DMMarker: face group data is unavailable.");
                                clearMeshMarkerGroup(dData);
                                return;
                            }
                            const DPIdList idsList = groupData->getGroupIds();
                            const int faceCount = (*mesh)->numFace();
                            if (faceCount >= 0 &&
                                idsList.size() == static_cast<std::size_t>(faceCount)) {
                                MObject mObj;
                                if (generateMMeshFromDmesh(*mesh->get(), mObj) != MS::kSuccess) {
                                    MGlobal::displayError(
                                        "DMMarker: unable to create face group mesh data.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }

                                MItMeshPolygon face(mObj, &stat);
                                if (!stat) {
                                    MGlobal::displayError(
                                        "DMMarker: unable to iterate face group mesh.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }
                                const MMatrix transform = (*mesh)->getTransformMatrix();
                                const bool applyTransform = !(*mesh)->isIdentity();

                                stat = dommy::maya::visitMeshPolygons(
                                    face, static_cast<std::size_t>(faceCount),
                                    [&](MItMeshPolygon& face) -> MStatus {
                                    MStatus indexStatus;
                                    const int faceIndex = face.index(&indexStatus);
                                    if (!indexStatus) return indexStatus;
                                    if (faceIndex < 0 ||
                                        static_cast<std::size_t>(faceIndex) >= idsList.size())
                                        return MS::kInvalidParameter;
                                    if (!idsList[faceIndex]) return MS::kSuccess;

                                    MPointArray pts;
                                    MIntArray vertexList;
                                    if (face.getTriangles(pts, vertexList) != MS::kSuccess ||
                                        pts.length() != vertexList.length()) {
                                        MGlobal::displayError(
                                            "DMMarker: unable to triangulate face group.");
                                        return MS::kFailure;
                                    }

                                    std::size_t nextTotalPoint = 0U;
                                    std::size_t groupScalarCount = 0U;
                                    std::size_t nextIndexCount = 0U;
                                    if (!dommy::memory::checkedAdd(
                                            totalPoint, static_cast<std::size_t>(pts.length()),
                                            nextTotalPoint) ||
                                        nextTotalPoint > dommy::viewport::kMaxViewportVertexCount ||
                                        !dommy::geometry::checkedTupleScalarCount(
                                            nextTotalPoint, 3U, groupScalarCount) ||
                                        groupScalarCount >
                                            dommy::viewport::kMaxViewportScalarCount ||
                                        !dommy::memory::checkedAdd(
                                            dData.grpIndexBuffer.size(),
                                            static_cast<std::size_t>(vertexList.length()),
                                            nextIndexCount) ||
                                        nextIndexCount > dommy::viewport::kMaxViewportIndexCount) {
                                        MGlobal::displayError(
                                            "DMMarker: face group exceeds the viewport budget.");
                                        return MS::kInvalidParameter;
                                    }

                                    dData.grpPtBuffer.reserve(groupScalarCount);
                                    dData.grpIndexBuffer.reserve(nextIndexCount);
                                    for (unsigned int pointIndex = 0U; pointIndex < pts.length();
                                         ++pointIndex) {
                                        MPoint point = pts[pointIndex];
                                        if (applyTransform) {
                                            point = point * transform;
                                        }
                                        dData.grpPtBuffer.push_back(static_cast<float>(point[0]));
                                        dData.grpPtBuffer.push_back(static_cast<float>(point[1]));
                                        dData.grpPtBuffer.push_back(static_cast<float>(point[2]));
                                        dData.grpIndexBuffer.push_back(
                                            static_cast<unsigned int>(totalPoint + pointIndex));
                                    }
                                    totalPoint = nextTotalPoint;
                                    return MS::kSuccess;
                                });
                                if (!stat) {
                                    MGlobal::displayError("DMMarker: invalid or incomplete face traversal.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }
                            }
                        }
                    }
                    dData.totalGroupPointCount = totalPoint;
                } else if (dData.grpType == D_COMP_EDGE) {
#pragma mark edge group
                    std::size_t totalPoint = 0U;
                    for (auto mesh = dMeshes->beginMeshes(), endMesh = dMeshes->endMeshes();
                         mesh != endMesh; ++mesh) {
                        if ((*mesh)->isExistCompGrp(D_COMP_EDGE, grpNameStr)) {
                            DPGroupData::ConstPtr groupData =
                                (*mesh)->getCompGrp(D_COMP_EDGE, grpNameStr);
                            if (!groupData) {
                                MGlobal::displayError("DMMarker: edge group data is unavailable.");
                                clearMeshMarkerGroup(dData);
                                return;
                            }
                            const DPIdList idsList = groupData->getGroupIds();

                            MObject mObj;
                            if (generateMMeshFromDmesh(*mesh->get(), mObj) != MS::kSuccess) {
                                MGlobal::displayError(
                                    "DMMarker: unable to create edge group mesh data.");
                                clearMeshMarkerGroup(dData);
                                return;
                            }

                            MFnMesh meshFn(mObj, &stat);
                            const int edgeCount = stat ? meshFn.numEdges(&stat) : -1;
                            if (!stat || edgeCount < 0) {
                                MGlobal::displayError("DMMarker: failed to query mesh edge count.");
                                clearMeshMarkerGroup(dData);
                                return;
                            }
                            if (idsList.size() == static_cast<std::size_t>(edgeCount)) {
                                MItMeshEdge edge(mObj, &stat);
                                if (!stat) {
                                    MGlobal::displayError(
                                        "DMMarker: unable to iterate edge group mesh.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }
                                const MMatrix transform = (*mesh)->getTransformMatrix();
                                const bool applyTransform = !(*mesh)->isIdentity();

                                stat = dommy::maya::visitMeshElements(
                                    edge, static_cast<std::size_t>(edgeCount),
                                    [&](MItMeshEdge& edge) -> MStatus {
                                    MStatus indexStatus;
                                    const int edgeIndex = edge.index(&indexStatus);
                                    if (!indexStatus) return indexStatus;
                                    if (edgeIndex < 0 ||
                                        static_cast<std::size_t>(edgeIndex) >= idsList.size())
                                        return MS::kInvalidParameter;
                                    if (!idsList[edgeIndex]) return MS::kSuccess;

                                    MPoint first = edge.point(0, MSpace::kObject, &stat);
                                    if (!stat) {
                                        MGlobal::displayError(
                                            "DMMarker: unable to read edge group point.");
                                        return stat;
                                    }
                                    MPoint second = edge.point(1, MSpace::kObject, &stat);
                                    if (!stat) {
                                        MGlobal::displayError(
                                            "DMMarker: unable to read edge group point.");
                                        return stat;
                                    }
                                    if (applyTransform) {
                                        first = first * transform;
                                        second = second * transform;
                                    }

                                    std::size_t nextTotalPoint = 0U;
                                    std::size_t groupScalarCount = 0U;
                                    std::size_t nextIndexCount = 0U;
                                    if (!dommy::memory::checkedAdd(totalPoint, 2U,
                                                                   nextTotalPoint) ||
                                        nextTotalPoint > dommy::viewport::kMaxViewportVertexCount ||
                                        !dommy::geometry::checkedTupleScalarCount(
                                            nextTotalPoint, 3U, groupScalarCount) ||
                                        groupScalarCount >
                                            dommy::viewport::kMaxViewportScalarCount ||
                                        !dommy::memory::checkedAdd(dData.grpIndexBuffer.size(), 2U,
                                                                   nextIndexCount) ||
                                        nextIndexCount > dommy::viewport::kMaxViewportIndexCount) {
                                        MGlobal::displayError(
                                            "DMMarker: edge group exceeds the viewport budget.");
                                        return MS::kInvalidParameter;
                                    }

                                    dData.grpPtBuffer.reserve(groupScalarCount);
                                    dData.grpIndexBuffer.reserve(nextIndexCount);
                                    const MPoint points[2] = {first, second};
                                    for (std::size_t pointIndex = 0U; pointIndex < 2U;
                                         ++pointIndex) {
                                        dData.grpPtBuffer.push_back(
                                            static_cast<float>(points[pointIndex][0]));
                                        dData.grpPtBuffer.push_back(
                                            static_cast<float>(points[pointIndex][1]));
                                        dData.grpPtBuffer.push_back(
                                            static_cast<float>(points[pointIndex][2]));
                                        dData.grpIndexBuffer.push_back(
                                            static_cast<unsigned int>(totalPoint + pointIndex));
                                    }
                                    totalPoint = nextTotalPoint;
                                    return MS::kSuccess;
                                });
                                if (!stat) {
                                    MGlobal::displayError("DMMarker: invalid or incomplete edge traversal.");
                                    clearMeshMarkerGroup(dData);
                                    return;
                                }
                            }
                        }
                    }
                    dData.totalGroupPointCount = totalPoint;
                }

                std::size_t groupScalarCount = 0U;
                bool groupLayoutIsValid =
                    dommy::geometry::checkedTupleScalarCount(dData.totalGroupPointCount, 3U,
                                                             groupScalarCount) &&
                    groupScalarCount == dData.grpPtBuffer.size() &&
                    groupScalarCount <= dommy::viewport::kMaxViewportScalarCount &&
                    dData.grpIndexBuffer.size() <= dommy::viewport::kMaxViewportIndexCount;
                if (groupLayoutIsValid) {
                    for (std::vector<unsigned int>::const_iterator index =
                             dData.grpIndexBuffer.begin();
                         index != dData.grpIndexBuffer.end(); ++index) {
                        if (*index >= dData.totalGroupPointCount) {
                            groupLayoutIsValid = false;
                            break;
                        }
                    }
                }
                if (!groupLayoutIsValid) {
                    MGlobal::displayError("DMMarker: group viewport layout is inconsistent.");
                    clearMeshMarkerGroup(dData);
                }
            } // Group Process

            // display info
            bool isNotExistPrimAttribute = false;
            bool isNotExisPointAttribute = false;
#pragma mark Display attribute in Color
            if (dData.isColor) {
                if (dData.infoClass == 0) { // primitive attribute
                    if (dMeshes->isExistAttr(visAttrStr)) {

                        DP_ATTR_DATA_TYPE attrType = dMeshes->getAttr(visAttrStr)->getAttrType();

                        if (isSingleValueAttrType(attrType)) {

                            dData.valType = DP_VAL_INT;
                            if (attrType == DP_ATTR_INT) {
                                setMeshSingleValueColor<int32_t>(*dMeshes, visAttrStr,
                                                                 dData.isBlackTransparency,
                                                                 dData.colBuffer);
                            } else if (attrType == DP_ATTR_BOOL) {
                                setMeshSingleValueColor<bool>(*dMeshes, visAttrStr,
                                                              dData.isBlackTransparency,
                                                              dData.colBuffer);
                            } else if (attrType == DP_ATTR_UINT32) {
                                setMeshSingleValueColor<uint32_t>(*dMeshes, visAttrStr,
                                                                  dData.isBlackTransparency,
                                                                  dData.colBuffer);
                            } else if (attrType == DP_ATTR_INT64) {
                                setMeshSingleValueColor<int64_t>(*dMeshes, visAttrStr,
                                                                 dData.isBlackTransparency,
                                                                 dData.colBuffer);
                            } else if (attrType == DP_ATTR_FLOAT) {
                                dData.valType = DP_VAL_FLOAT;
                                setMeshSingleValueColor<float>(*dMeshes, visAttrStr,
                                                               dData.isBlackTransparency,
                                                               dData.colBuffer);
                            } else if (attrType == DP_ATTR_DOUBLE) {
                                dData.valType = DP_VAL_FLOAT;
                                setMeshSingleValueColor<double>(*dMeshes, visAttrStr,
                                                                dData.isBlackTransparency,
                                                                dData.colBuffer);
                            }

                        } else if (isVector2DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC2I) {
                                dData.valType = DP_VAL_INT2;
                                setMeshTwoVecColor<Imath::V2i>(*dMeshes, visAttrStr,
                                                               dData.isBlackTransparency,
                                                               dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC2F) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshTwoVecColor<Imath::V2f>(*dMeshes, visAttrStr,
                                                               dData.isBlackTransparency,
                                                               dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC2D) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshTwoVecColor<Imath::V2d>(*dMeshes, visAttrStr,
                                                               dData.isBlackTransparency,
                                                               dData.colBuffer);
                            }

                        } else if (isVector3DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC3I) {
                                dData.valType = DP_VAL_INT3;
                                setMeshThreeVecColor<Imath::V3i>(*dMeshes, visAttrStr,
                                                                 dData.isBlackTransparency,
                                                                 dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC3F) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshThreeVecColor<Imath::V3f>(*dMeshes, visAttrStr,
                                                                 dData.isBlackTransparency,
                                                                 dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC3D) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshThreeVecColor<Imath::V3d>(*dMeshes, visAttrStr,
                                                                 dData.isBlackTransparency,
                                                                 dData.colBuffer);
                            }
                        }

                    } else { // check prim exist
                        isNotExistPrimAttribute = true;
                    }
                } else { // point attribute

                    // Check point attribute exist
                    if (dMeshes->isExistPointAttr(visAttrStr)) {

                        DP_ATTR_DATA_TYPE attrType = dMeshes->getPointAttributeType(visAttrStr);

                        if (isSingleValueAttrType(attrType)) {

                            dData.valType = DP_VAL_INT;
                            if (attrType == DP_ATTR_INT) {
                                setMeshPointSingleValueColor<int32_t>(*dMeshes, visAttrStr,
                                                                      dData.isBlackTransparency,
                                                                      dData.colBuffer);
                            } else if (attrType == DP_ATTR_BOOL) {
                                setMeshPointSingleValueColor<bool>(*dMeshes, visAttrStr,
                                                                   dData.isBlackTransparency,
                                                                   dData.colBuffer);
                            } else if (attrType == DP_ATTR_UINT32) {
                                setMeshPointSingleValueColor<uint32_t>(*dMeshes, visAttrStr,
                                                                       dData.isBlackTransparency,
                                                                       dData.colBuffer);
                            } else if (attrType == DP_ATTR_INT64) {
                                setMeshPointSingleValueColor<int64_t>(*dMeshes, visAttrStr,
                                                                      dData.isBlackTransparency,
                                                                      dData.colBuffer);
                            } else if (attrType == DP_ATTR_FLOAT) {
                                dData.valType = DP_VAL_FLOAT;
                                setMeshPointSingleValueColor<float>(*dMeshes, visAttrStr,
                                                                    dData.isBlackTransparency,
                                                                    dData.colBuffer);
                            } else if (attrType == DP_ATTR_DOUBLE) {
                                dData.valType = DP_VAL_FLOAT;
                                setMeshPointSingleValueColor<double>(*dMeshes, visAttrStr,
                                                                     dData.isBlackTransparency,
                                                                     dData.colBuffer);
                            }

                        } else if (isVector2DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC2I) {
                                dData.valType = DP_VAL_INT2;
                                setMeshPointTwoVecColor<Imath::V2i>(*dMeshes, visAttrStr,
                                                                    dData.isBlackTransparency,
                                                                    dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC2F) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshPointTwoVecColor<Imath::V2f>(*dMeshes, visAttrStr,
                                                                    dData.isBlackTransparency,
                                                                    dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC2D) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshPointTwoVecColor<Imath::V2d>(*dMeshes, visAttrStr,
                                                                    dData.isBlackTransparency,
                                                                    dData.colBuffer);
                            }

                        } else if (isVector3DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC3I) {
                                dData.valType = DP_VAL_INT3;
                                setMeshPointThreeVecColor<Imath::V3i>(*dMeshes, visAttrStr,
                                                                      dData.isBlackTransparency,
                                                                      dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC3F) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshPointThreeVecColor<Imath::V3f>(*dMeshes, visAttrStr,
                                                                      dData.isBlackTransparency,
                                                                      dData.colBuffer);
                            } else if (attrType == DP_ATTR_VEC3D) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshPointThreeVecColor<Imath::V3d>(*dMeshes, visAttrStr,
                                                                      dData.isBlackTransparency,
                                                                      dData.colBuffer);
                            }
                        }

                    } else {
                        isNotExisPointAttribute = true;
                    }
                }

            } // color attribute display

#pragma mark Display attribute in Vector
            if (dData.isVector) {
                if (dData.infoClass == 0) { // primitive attribute

                    if (dMeshes->isExistAttr(visAttrStr)) {
                        DP_ATTR_DATA_TYPE attrType = dMeshes->getAttr(visAttrStr)->getAttrType();

                        if (isSingleValueAttrType(attrType)) {
                            dData.valType = DP_VAL_INT;
                            if (attrType == DP_ATTR_INT) {
                                setMesh1DVector<int32_t>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_BOOL) {
                                setMesh1DVector<bool>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_UINT32) {
                                setMesh1DVector<uint32_t>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_INT64) {
                                setMesh1DVector<int64_t>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_FLOAT) {
                                dData.valType = DP_VAL_FLOAT;
                                setMesh1DVector<float>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_DOUBLE) {
                                dData.valType = DP_VAL_FLOAT;
                                setMesh1DVector<double>(*dMeshes, visAttrStr, dData.vecBuffer);
                            }

                        } else if (isVector2DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC2I) {
                                dData.valType = DP_VAL_INT2;
                                setMesh2To3DVectorValue<Imath::V2i>(*dMeshes, visAttrStr,
                                                                    dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC2F) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMesh2To3DVectorValue<Imath::V2f>(*dMeshes, visAttrStr,
                                                                    dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC2D) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMesh2To3DVectorValue<Imath::V2d>(*dMeshes, visAttrStr,
                                                                    dData.vecBuffer);
                            }

                        } else if (isVector3DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC3I) {
                                dData.valType = DP_VAL_INT3;
                                setMeshSimpleVectorValue<Imath::V3i>(*dMeshes, visAttrStr,
                                                                     dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC3F) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshSimpleVectorValue<Imath::V3f>(*dMeshes, visAttrStr,
                                                                     dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC3D) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshSimpleVectorValue<Imath::V3d>(*dMeshes, visAttrStr,
                                                                     dData.vecBuffer);
                            }
                        }

                    } else { // check prim exist
                        isNotExistPrimAttribute = true;
                    }

                } else { // point attribute

                    if (dMeshes->isExistPointAttr(visAttrStr)) {
                        DP_ATTR_DATA_TYPE attrType = dMeshes->getPointAttributeType(visAttrStr);

                        if (isSingleValueAttrType(attrType)) {
                            dData.valType = DP_VAL_INT;
                            if (attrType == DP_ATTR_INT) {
                                setMeshPoint1DVector<int32_t>(*dMeshes, visAttrStr,
                                                              dData.vecBuffer);
                            } else if (attrType == DP_ATTR_BOOL) {
                                setMeshPoint1DVector<bool>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_UINT32) {
                                setMeshPoint1DVector<uint32_t>(*dMeshes, visAttrStr,
                                                               dData.vecBuffer);
                            } else if (attrType == DP_ATTR_INT64) {
                                setMeshPoint1DVector<int64_t>(*dMeshes, visAttrStr,
                                                              dData.vecBuffer);
                            } else if (attrType == DP_ATTR_FLOAT) {
                                dData.valType = DP_VAL_FLOAT;
                                setMeshPoint1DVector<float>(*dMeshes, visAttrStr, dData.vecBuffer);
                            } else if (attrType == DP_ATTR_DOUBLE) {
                                dData.valType = DP_VAL_FLOAT;
                                setMeshPoint1DVector<double>(*dMeshes, visAttrStr, dData.vecBuffer);
                            }

                        } else if (isVector2DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC2I) {
                                dData.valType = DP_VAL_INT2;
                                setMeshPoint2To3DVectorValue<Imath::V2i>(*dMeshes, visAttrStr,
                                                                         dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC2F) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshPoint2To3DVectorValue<Imath::V2f>(*dMeshes, visAttrStr,
                                                                         dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC2D) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshPoint2To3DVectorValue<Imath::V2d>(*dMeshes, visAttrStr,
                                                                         dData.vecBuffer);
                            }

                        } else if (isVector3DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC3I) {
                                dData.valType = DP_VAL_INT3;
                                setMeshPointSimpleVectorValue<Imath::V3i>(*dMeshes, visAttrStr,
                                                                          dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC3F) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshPointSimpleVectorValue<Imath::V3f>(*dMeshes, visAttrStr,
                                                                          dData.vecBuffer);
                            } else if (attrType == DP_ATTR_VEC3D) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshPointSimpleVectorValue<Imath::V3d>(*dMeshes, visAttrStr,
                                                                          dData.vecBuffer);
                            }
                        }

                    } else { // check exist
                        isNotExisPointAttribute = true;
                    }
                } // check attribute class
            } // vector attribute display

#pragma mark Display attribute in Value
            if (dData.isValue) {
                if (dData.infoClass == 0) { // primitive attribute
                    if (dMeshes->isExistAttr(visAttrStr)) {

                        DP_ATTR_DATA_TYPE attrType = dMeshes->getAttr(visAttrStr)->getAttrType();

                        if (isSingleValueAttrType(attrType)) {

                            dData.valType = DP_VAL_INT;

                            if (attrType == DP_ATTR_INT) {
                                setMeshSimpleSingleValue<int32_t>(*dMeshes, visAttrStr,
                                                                  dData.val3Buffer);
                            } else if (attrType == DP_ATTR_BOOL) {
                                setMeshSimpleSingleValue<bool>(*dMeshes, visAttrStr,
                                                               dData.val3Buffer);
                            } else if (attrType == DP_ATTR_UINT32) {
                                setMeshSimpleSingleValue<uint32_t>(*dMeshes, visAttrStr,
                                                                   dData.val3Buffer);
                            } else if (attrType == DP_ATTR_INT64) {
                                setMeshSimpleSingleValue<int64_t>(*dMeshes, visAttrStr,
                                                                  dData.val3Buffer);

                            } else if (attrType == DP_ATTR_FLOAT) {

                                dData.valType = DP_VAL_FLOAT;
                                setMeshSimpleSingleValue<float>(*dMeshes, visAttrStr,
                                                                dData.val3Buffer);

                            } else if (attrType == DP_ATTR_DOUBLE) {

                                dData.valType = DP_VAL_FLOAT;
                                setMeshSimpleSingleValue<double>(*dMeshes, visAttrStr,
                                                                 dData.val3Buffer);
                            }

                        } else if (isVector2DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC2I) {
                                dData.valType = DP_VAL_INT2;
                                setMeshTwoVectorValue<Imath::V2i>(*dMeshes, visAttrStr,
                                                                  dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC2F) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshTwoVectorValue<Imath::V2f>(*dMeshes, visAttrStr,
                                                                  dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC2D) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshTwoVectorValue<Imath::V2d>(*dMeshes, visAttrStr,
                                                                  dData.val3Buffer);
                            }

                        } else if (isVector3DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC3I) {
                                dData.valType = DP_VAL_INT3;
                                setMeshSimpleVectorValue<Imath::V3i>(*dMeshes, visAttrStr,
                                                                     dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC3F) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshSimpleVectorValue<Imath::V3f>(*dMeshes, visAttrStr,
                                                                     dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC3D) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshSimpleVectorValue<Imath::V3d>(*dMeshes, visAttrStr,
                                                                     dData.val3Buffer);
                            }
                        }

                    } else { // check prim exist
                        isNotExistPrimAttribute = true;
                    }

                } else { // point attribute

                    if (dMeshes->isExistPointAttr(visAttrStr)) {

                        DP_ATTR_DATA_TYPE attrType = dMeshes->getPointAttributeType(visAttrStr);

                        if (isSingleValueAttrType(attrType)) {

                            dData.valType = DP_VAL_INT;

                            if (attrType == DP_ATTR_INT) {
                                setMeshPointSimpleSingleValue<int32_t>(*dMeshes, visAttrStr,
                                                                       dData.val3Buffer);
                            } else if (attrType == DP_ATTR_BOOL) {
                                setMeshPointSimpleSingleValue<bool>(*dMeshes, visAttrStr,
                                                                    dData.val3Buffer);
                            } else if (attrType == DP_ATTR_UINT32) {
                                setMeshPointSimpleSingleValue<uint32_t>(*dMeshes, visAttrStr,
                                                                        dData.val3Buffer);
                            } else if (attrType == DP_ATTR_INT64) {
                                setMeshPointSimpleSingleValue<int64_t>(*dMeshes, visAttrStr,
                                                                       dData.val3Buffer);

                            } else if (attrType == DP_ATTR_FLOAT) {

                                dData.valType = DP_VAL_FLOAT;
                                setMeshPointSimpleSingleValue<float>(*dMeshes, visAttrStr,
                                                                     dData.val3Buffer);

                            } else if (attrType == DP_ATTR_DOUBLE) {

                                dData.valType = DP_VAL_FLOAT;
                                setMeshPointSimpleSingleValue<double>(*dMeshes, visAttrStr,
                                                                      dData.val3Buffer);
                            }

                        } else if (isVector2DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC2I) {
                                dData.valType = DP_VAL_INT2;
                                setMeshPointTwoVectorValue<Imath::V2i>(*dMeshes, visAttrStr,
                                                                       dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC2F) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshPointTwoVectorValue<Imath::V2f>(*dMeshes, visAttrStr,
                                                                       dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC2D) {
                                dData.valType = DP_VAL_FLOAT2;
                                setMeshPointTwoVectorValue<Imath::V2d>(*dMeshes, visAttrStr,
                                                                       dData.val3Buffer);
                            }

                        } else if (isVector3DAttrType(attrType)) {

                            if (attrType == DP_ATTR_VEC3I) {
                                dData.valType = DP_VAL_INT3;
                                setMeshPointSimpleVectorValue<Imath::V3i>(*dMeshes, visAttrStr,
                                                                          dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC3F) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshPointSimpleVectorValue<Imath::V3f>(*dMeshes, visAttrStr,
                                                                          dData.val3Buffer);
                            } else if (attrType == DP_ATTR_VEC3D) {
                                dData.valType = DP_VAL_FLOAT3;
                                setMeshPointSimpleVectorValue<Imath::V3d>(*dMeshes, visAttrStr,
                                                                          dData.val3Buffer);
                            }
                        }

                    } else { // check exist
                        isNotExisPointAttribute = true;
                    }
                }

            } // value attribute display

            // attribute error check
            if (isNotExistPrimAttribute) {
                MGlobal::displayWarning("Not exist primitive attribute!");
                dData.isColor = false;
                dData.isValue = false;
                dData.isVector = false;
            }
            if (isNotExisPointAttribute) {
                MGlobal::displayWarning("Not exist point attribute!");
                dData.isColor = false;
                dData.isValue = false;
                dData.isVector = false;
            }

            if (dData.isColor) {
                std::size_t expectedColorCount = 0U;
                std::size_t expandedPositionCount = 0U;
                std::size_t expandedColorCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(totalVtx, 4U, expectedColorCount) ||
                    expectedColorCount != dData.colBuffer.size() ||
                    expectedColorCount > dommy::viewport::kMaxViewportScalarCount ||
                    !dommy::geometry::checkedTupleScalarCount(dData.indexBuffer.size(), 3U,
                                                              expandedPositionCount) ||
                    expandedPositionCount > dommy::viewport::kMaxViewportScalarCount ||
                    !dommy::geometry::checkedTupleScalarCount(dData.indexBuffer.size(), 4U,
                                                              expandedColorCount) ||
                    expandedColorCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DMMarker: color viewport layout is inconsistent.");
                    dData.colBuffer.clear();
                    dData.isColor = false;
                }
            }

            const std::size_t attributeElementCount = dData.infoClass == 0 ? meshCount : totalVtx;
            if (dData.isVector) {
                std::size_t expectedVectorCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(attributeElementCount, 3U,
                                                              expectedVectorCount) ||
                    expectedVectorCount != dData.vecBuffer.size() ||
                    expectedVectorCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DMMarker: vector attribute layout is inconsistent.");
                    dData.vecBuffer.clear();
                    dData.isVector = false;
                }
            }

            if (dData.isValue) {
                std::size_t valueTupleSize = 0U;
                switch (dData.valType) {
                case DP_VAL_FLOAT:
                case DP_VAL_INT:
                    valueTupleSize = 1U;
                    break;
                case DP_VAL_FLOAT2:
                case DP_VAL_INT2:
                    valueTupleSize = 2U;
                    break;
                case DP_VAL_FLOAT3:
                case DP_VAL_INT3:
                    valueTupleSize = 3U;
                    break;
                default:
                    break;
                }

                std::size_t expectedValueCount = 0U;
                if (valueTupleSize == 0U ||
                    !dommy::geometry::checkedTupleScalarCount(attributeElementCount, valueTupleSize,
                                                              expectedValueCount) ||
                    expectedValueCount != dData.val3Buffer.size() ||
                    expectedValueCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DMMarker: value attribute layout is inconsistent.");
                    dData.val3Buffer.clear();
                    dData.isValue = false;
                }
            }
        }
    }
} catch (...) {
    clearMeshMarkerGeometry(dData);
    mDataChanged = true;
    dommy::maya::reportViewportCallbackException("DMMarkerOverride", "updateDG",
                                                 std::current_exception());
}

void DMMarkerOverride::updateRenderItems(const MDagPath& /*path*/,
                                         MHWRender::MRenderItemList& list) try {
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DMMarkerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dIntegerNumericShader, mIndexUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVSolidShader, mColorMeshShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloatNumericShader, mValueUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat2NumericShader, mValue2UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat3NumericShader, mValue3UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dSolidShader, mGroupPolyShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mGroupLineShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mGroupPointShader);

    // get attribute
    const float solidColor[4] = {dData.solidColor.r, dData.solidColor.g, dData.solidColor.b,
                                 1.0f}; // main color
    const float2 tpos = {0, -0.5};      // value text position

#pragma mark mesh index shader
    MHWRender::MRenderItem* DMeshIndexItem = NULL;

    int index = list.indexOf(DMeshIndexItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshIndexItemName_,
                                                        MHWRender::MRenderItem::DecorationItem,
                                                        MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sActiveLineDepthPriority);
        if (!list.append(pendingItem.get())) return;
        DMeshIndexItem = pendingItem.release();
    } else {
        DMeshIndexItem = list.itemAt(index);
    }

    if (DMeshIndexItem) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DMeshIndexItem, mShaderUpdateFailed);
        if (mIndexUIShader) {
            shaderUpdate.setParameter(mIndexUIShader, "solidColor", solidColor);
            shaderUpdate.setShader(mIndexUIShader, &DMeshIndexItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.showIndex);
    }

#pragma mark mesh color shader
    MHWRender::MRenderItem* colorDMesh = NULL;

    index = list.indexOf(DMeshColorItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshColorItemName_,
                                                    MHWRender::MRenderItem::DecorationItem,
                                                    MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sDormantWireDepthPriority);
        if (!list.append(pendingItem.get())) return;
        colorDMesh = pendingItem.release();
    } else {
        colorDMesh = list.itemAt(index);
    }

    if (colorDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*colorDMesh, mShaderUpdateFailed);
        if (mColorMeshShader) {
            shaderUpdate.setShader(mColorMeshShader, &DMeshColorItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.isColor && dData.colBuffer.size());
    }

#pragma mark Vector Shader
    MHWRender::MRenderItem* DCurveVectorItem = NULL;

    index = list.indexOf(DMeshVecItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshVecItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sActiveLineDepthPriority);
        if (!list.append(pendingItem.get())) return;
        DCurveVectorItem = pendingItem.release();
    } else {
        DCurveVectorItem = list.itemAt(index);
    }

    if (DCurveVectorItem) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveVectorItem, mShaderUpdateFailed);
        if (mVectorUIShader) {
            shaderUpdate.setParameter(mVectorUIShader, "drawHeads", dData.isDrawHeads);
            shaderUpdate.setParameter(mVectorUIShader, "velocityScale", dData.velocityScale);
            shaderUpdate.setParameter(mVectorUIShader, "solidColor", solidColor);
            shaderUpdate.setShader(mVectorUIShader, &DMeshVecItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.isVector);
    }

#pragma mark Value3 Shader
    MHWRender::MRenderItem* DValue3Item = NULL;

    index = list.indexOf(DMeshVal3ItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshVal3ItemName_,
                                                     MHWRender::MRenderItem::DecorationItem,
                                                     MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sActiveLineDepthPriority);
        if (!list.append(pendingItem.get())) return;
        DValue3Item = pendingItem.release();
    } else {
        DValue3Item = list.itemAt(index);
    }

    if (DValue3Item) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DValue3Item, mShaderUpdateFailed);
        if (mValue3UIShader) {
            shaderUpdate.setParameter(mValue3UIShader, "solidColor", solidColor);
            shaderUpdate.setParameter(mValue3UIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT3) {
                shaderUpdate.setParameter(mValue3UIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValue3UIShader, "isFloatValue", false);
            }

            shaderUpdate.setShader(mValue3UIShader, &DMeshVal3ItemName_);
        }
        if (dData.primCount &&
            (dData.isValue && (dData.valType == DP_VAL_FLOAT3 || dData.valType == DP_VAL_INT3))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }

    // Value2 Shader
    MHWRender::MRenderItem* DValue2Item = NULL;

    index = list.indexOf(DMeshVal2ItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshVal2ItemName_,
                                                     MHWRender::MRenderItem::DecorationItem,
                                                     MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sActiveLineDepthPriority);
        if (!list.append(pendingItem.get())) return;
        DValue2Item = pendingItem.release();
    } else {
        DValue2Item = list.itemAt(index);
    }

    if (DValue2Item) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DValue2Item, mShaderUpdateFailed);
        if (mValue2UIShader) {
            shaderUpdate.setParameter(mValue2UIShader, "solidColor", solidColor);
            shaderUpdate.setParameter(mValue2UIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT2) {
                shaderUpdate.setParameter(mValue2UIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValue2UIShader, "isFloatValue", false);
            }

            shaderUpdate.setShader(mValue2UIShader, &DMeshVal2ItemName_);
        }
        if (dData.primCount &&
            (dData.isValue && (dData.valType == DP_VAL_FLOAT2 || dData.valType == DP_VAL_INT2))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }

    // Value1 Shader
    MHWRender::MRenderItem* DValue1Item = NULL;

    index = list.indexOf(DMeshVal1ItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshVal1ItemName_,
                                                     MHWRender::MRenderItem::DecorationItem,
                                                     MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sActiveLineDepthPriority);
        if (!list.append(pendingItem.get())) return;
        DValue1Item = pendingItem.release();
    } else {
        DValue1Item = list.itemAt(index);
    }

    if (DValue1Item) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DValue1Item, mShaderUpdateFailed);
        if (mValueUIShader) {
            shaderUpdate.setParameter(mValueUIShader, "solidColor", solidColor);
            shaderUpdate.setParameter(mValueUIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT) {
                shaderUpdate.setParameter(mValueUIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValueUIShader, "isFloatValue", false);
            }

            shaderUpdate.setShader(mValueUIShader, &DMeshVal1ItemName_);
        }
        if (dData.primCount &&
            (dData.isValue && (dData.valType == DP_VAL_FLOAT || dData.valType == DP_VAL_INT))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }

#pragma mark group poly shader

    const float grpColor[4] = {dData.groupColor.r, dData.groupColor.g, dData.groupColor.b,
                               dData.groupAlpha};

    MHWRender::MRenderItem* grpPolyDMesh = NULL;
    index = list.indexOf(DMGroupPolyItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMGroupPolyItemName_,
                                                      MHWRender::MRenderItem::DecorationItem,
                                                      MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        grpPolyDMesh = pendingItem.release();
    } else {
        grpPolyDMesh = list.itemAt(index);
    }

    if (grpPolyDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*grpPolyDMesh, mShaderUpdateFailed);
        if (mGroupPolyShader) {
            shaderUpdate.setParameter(mGroupPolyShader, "solidColor", grpColor);
            shaderUpdate.setShader(mGroupPolyShader, &DMGroupPolyItemName_);
        }
        shaderUpdate.enable(dData.isGroup &&
                             (dData.grpType == D_COMP_PRIM || dData.grpType == D_COMP_FACE) &&
                             dData.totalGroupPointCount > 0);
    }

#pragma mark group line shader
    MHWRender::MRenderItem* grpLineDMesh = NULL;
    index = list.indexOf(DMGroupLineItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMGroupLineItemName_,
                                                      MHWRender::MRenderItem::DecorationItem,
                                                      MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        grpLineDMesh = pendingItem.release();
    } else {
        grpLineDMesh = list.itemAt(index);
    }

    if (grpLineDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*grpLineDMesh, mShaderUpdateFailed);
        if (mGroupLineShader) {
            const float glineWidth[2] = {dData.lineWidth + 2, dData.lineWidth + 2};
            shaderUpdate.setParameter(mGroupLineShader, "lineWidth", glineWidth);
            shaderUpdate.setParameter(mGroupLineShader, "solidColor", grpColor);
            shaderUpdate.setShader(mGroupLineShader, &DMGroupLineItemName_);
        }
        shaderUpdate.enable(dData.isGroup && dData.grpType == D_COMP_EDGE &&
                             dData.totalGroupPointCount > 0);
    }

#pragma mark group point shader

    MHWRender::MRenderItem* grpPointDMesh = NULL;
    index = list.indexOf(DMGroupPointItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMGroupPointItemName_,
                                                       MHWRender::MRenderItem::DecorationItem,
                                                       MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        grpPointDMesh = pendingItem.release();
    } else {
        grpPointDMesh = list.itemAt(index);
    }

    if (grpPointDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*grpPointDMesh, mShaderUpdateFailed);
        if (mGroupPointShader) {
            float2 gpSize = {dData.pointSize + 2, dData.pointSize + 2}; // point size
            shaderUpdate.setParameter(mGroupPointShader, "pointSize", gpSize);
            shaderUpdate.setParameter(mGroupPointShader, "solidColor", grpColor);
            shaderUpdate.setShader(mGroupPointShader, &DMGroupPointItemName_);
        }
        shaderUpdate.enable(dData.isGroup && dData.grpType == D_COMP_POINT &&
                              dData.totalGroupPointCount > 0);
    }
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH("DMMarkerOverride", "updateRenderItems")

void DMMarkerOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                                        const MHWRender::MRenderItemList& renderItems,
                                        MHWRender::MGeometry& data) try {
    mDataChanged = true;

    // vtx for Color
    MHWRender::MVertexBuffer* verticesBuffer = NULL;
    float* vertices = NULL;

    // index
    MHWRender::MVertexBuffer* indexPositionBuffer = NULL;
    float* indexPositions = NULL;
    MHWRender::MVertexBuffer* indexNumericBuffer = NULL; // texture
    float* indexNumerics = NULL;

    // color
    MHWRender::MVertexBuffer* colBuffer = NULL;
    float* colorvalues = NULL;

    // vector
    MHWRender::MVertexBuffer* vecBuffer = NULL;
    float* vectorvalues = NULL;

    // value
    MHWRender::MVertexBuffer* val3Buffer = NULL;
    float* val3values = NULL;

    MHWRender::MVertexBuffer* tmpNumBuffer = NULL;
    float* tmpnumvalues = NULL;

    // value or vec points
    MHWRender::MVertexBuffer* vPosBuffer = NULL;
    float* vPosPoints = NULL;
    MHWRender::MVertexBuffer* vecPosBuffer = NULL;
    float* vecPosPoints = NULL;

    // group buffer
    MHWRender::MVertexBuffer* grpVerticesBuffer = NULL;
    float* grpVertices = NULL;

    std::size_t primitivePositionScalarCount = 0U;
    std::size_t expandedPositionScalarCount = 0U;
    std::size_t expandedColorScalarCount = 0U;
    if (!dommy::geometry::checkedTupleScalarCount(dData.meshFirstIndex.size(), 3U,
                                                  primitivePositionScalarCount) ||
        primitivePositionScalarCount > dommy::viewport::kMaxViewportScalarCount ||
        (dData.isColor && (!dommy::geometry::checkedTupleScalarCount(dData.indexBuffer.size(), 3U,
                                                                     expandedPositionScalarCount) ||
                           expandedPositionScalarCount > dommy::viewport::kMaxViewportScalarCount ||
                           !dommy::geometry::checkedTupleScalarCount(dData.indexBuffer.size(), 4U,
                                                                     expandedColorScalarCount) ||
                           expandedColorScalarCount > dommy::viewport::kMaxViewportScalarCount))) {
        return;
    }

    const std::size_t pointPositionScalarCount = dData.mPointBuffer.size();
    static const MString numericValue("numericvalue");
    static const MString numeric2Value("numeric2value");
    static const MString numeric3Value("numeric3value");

    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic()) {
            case MHWRender::MGeometry::kPosition: {

                // Color
                if (vertexBufferDescriptor.name() == DMeshColorItemName_) {
                    if (dData.isColor && !dData.indexBuffer.empty() && !verticesBuffer) {
                        verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!verticesBuffer) return false;

                        if (verticesBuffer) {

                            vertices = dommy::viewport::acquireFloatBuffer(
                                verticesBuffer, vertexBufferDescriptor, expandedPositionScalarCount, 3);
                        }
                    }
                }
                // Index
                else if (vertexBufferDescriptor.name() == DMeshIndexItemName_) {
                    if (!indexPositionBuffer) {
                        indexPositionBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!indexPositionBuffer) return false;
                        if (indexPositionBuffer) {
                            if (dData.indexType == 0) { // primitive

                                indexPositions = dommy::viewport::acquireFloatBuffer(
                                    indexPositionBuffer, vertexBufferDescriptor,
                                    primitivePositionScalarCount, 3);
                            } else { // point

                                indexPositions = dommy::viewport::acquireFloatBuffer(
                                    indexPositionBuffer, vertexBufferDescriptor,
                                    pointPositionScalarCount, 3);
                            }
                        }
                    }
                }
                // Vector
                else if (vertexBufferDescriptor.name() == DMeshVecItemName_) // vec
                {
                    if (!vecPosBuffer) {
                        vecPosBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!vecPosBuffer) return false;
                        if (vecPosBuffer) {
                            if (dData.infoClass == 0) { // primitive

                                vecPosPoints = dommy::viewport::acquireFloatBuffer(
                                    vecPosBuffer, vertexBufferDescriptor, primitivePositionScalarCount, 3);
                            } else { // point

                                vecPosPoints = dommy::viewport::acquireFloatBuffer(
                                    vecPosBuffer, vertexBufferDescriptor, pointPositionScalarCount, 3);
                            }
                        }
                    }
                }
                // Value
                else if (vertexBufferDescriptor.name() == DMeshVal3ItemName_ ||
                         vertexBufferDescriptor.name() == DMeshVal2ItemName_ ||
                         vertexBufferDescriptor.name() == DMeshVal1ItemName_) // vec or val or point
                {
                    if (!vPosBuffer) {
                        vPosBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!vPosBuffer) return false;
                        if (vPosBuffer) {
                            if (dData.infoClass == 0) { // primitive

                                vPosPoints = dommy::viewport::acquireFloatBuffer(
                                    vPosBuffer, vertexBufferDescriptor, primitivePositionScalarCount, 3);
                            } else { // point

                                vPosPoints = dommy::viewport::acquireFloatBuffer(
                                    vPosBuffer, vertexBufferDescriptor, pointPositionScalarCount, 3);
                            }
                        }
                    }
                }
                // Group
                else if (vertexBufferDescriptor.name() == DMGroupPolyItemName_ ||
                         vertexBufferDescriptor.name() == DMGroupLineItemName_ ||
                         vertexBufferDescriptor.name() == DMGroupPointItemName_) // group
                {
                    if (!grpVerticesBuffer) {
                        grpVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!grpVerticesBuffer) return false;
                        if (grpVerticesBuffer) {

                            grpVertices = dommy::viewport::acquireFloatBuffer(
                                grpVerticesBuffer, vertexBufferDescriptor, dData.grpPtBuffer.size(), 3);
                        }
                    }
                }

            } // kPoint

            break;
            case MHWRender::MGeometry::kTexture: {

                if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                    (vertexBufferDescriptor.name() == DMeshIndexItemName_)) {

                    if (!indexNumericBuffer) {
                        indexNumericBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!indexNumericBuffer) return false;
                        if (indexNumericBuffer) {
                            if (dData.indexType == 0) { // primitive

                                indexNumerics = dommy::viewport::acquireFloatBuffer(
                                    indexNumericBuffer, vertexBufferDescriptor,
                                    dData.meshFirstIndex.size(), 1);
                            } else { // point

                                indexNumerics = dommy::viewport::acquireFloatBuffer(
                                    indexNumericBuffer, vertexBufferDescriptor, dData.pointCount, 1);
                            }
                        }
                    }
                }
                // Vector
                else if ((vertexBufferDescriptor.name() == DMeshVecItemName_)) {
                    if (!vecBuffer) {
                        vecBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!vecBuffer) return false;
                        if (vecBuffer) {

                            vectorvalues = dommy::viewport::acquireFloatBuffer(
                                vecBuffer, vertexBufferDescriptor, dData.vecBuffer.size(), 3);
                        }
                    }
                }
                // Value 3
                else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numeric3Value) &&
                         (vertexBufferDescriptor.name() == DMeshVal3ItemName_ ||
                          vertexBufferDescriptor.name() == DMeshVal2ItemName_ ||
                          vertexBufferDescriptor.name() == DMeshVal1ItemName_)) {

                    if (!val3Buffer) {
                        val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!val3Buffer) return false;
                        if (val3Buffer) {

                            val3values = dommy::viewport::acquireFloatBuffer(
                                val3Buffer, vertexBufferDescriptor, dData.val3Buffer.size(), 3);
                        }
                    }

                }
                // Value3 or Value2
                // numericなぜ必要なのかなぞとりあえず何かしら値を与えると正常に動作するのでバッファを生成
                else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                         (vertexBufferDescriptor.name() == DMeshVal3ItemName_ ||
                          vertexBufferDescriptor.name() == DMeshVal2ItemName_)) {
                    if (!tmpNumBuffer) {
                        tmpNumBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!tmpNumBuffer) return false;
                        if (tmpNumBuffer) {
                            size_t num = 1;

                            tmpnumvalues = dommy::viewport::acquireFloatBuffer(
                                tmpNumBuffer, vertexBufferDescriptor, num, 1);
                        }
                    }
                }
                // Value2
                else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numeric2Value) &&
                         (vertexBufferDescriptor.name() == DMeshVal2ItemName_)) {
                    if (dData.valType == DP_VAL_INT2 || dData.valType == DP_VAL_FLOAT2) {
                        if (!val3Buffer) {
                            val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!val3Buffer) return false;
                            if (val3Buffer) {

                                val3values = dommy::viewport::acquireFloatBuffer(
                                    val3Buffer, vertexBufferDescriptor, dData.val3Buffer.size(), 2);
                            }
                        }
                    }
                }
                // Value1
                else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                         (vertexBufferDescriptor.name() == DMeshVal1ItemName_)) {
                    if (dData.valType == DP_VAL_INT || dData.valType == DP_VAL_FLOAT) {
                        if (!val3Buffer) {
                            val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!val3Buffer) return false;
                            if (val3Buffer) {

                                val3values = dommy::viewport::acquireFloatBuffer(
                                    val3Buffer, vertexBufferDescriptor, dData.val3Buffer.size(), 1);
                            }
                        }
                    }
                }
            } break;
            case MHWRender::MGeometry::kColor: {

                if ((vertexBufferDescriptor.name() == DMeshColorItemName_)) {
                    if (!colBuffer && dData.isColor && !dData.indexBuffer.empty() &&
                        !dData.colBuffer.empty()) {
                        colBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!colBuffer) return false;
                        if (colBuffer) {

                            colorvalues = dommy::viewport::acquireFloatBuffer(
                                colBuffer, vertexBufferDescriptor, expandedColorScalarCount, 4);
                        }
                    }
                }
            } break;
            default:
                // This stream semantic has no project buffer.
                break;
            }
            return true;
        });
    if (!streamsReady) return;

    if ((verticesBuffer && !vertices) || (indexPositionBuffer && !indexPositions) ||
        (indexNumericBuffer && !indexNumerics) || (colBuffer && !colorvalues) ||
        (vecBuffer && !vectorvalues) || (val3Buffer && !val3values) ||
        (tmpNumBuffer && !tmpnumvalues) || (vPosBuffer && !vPosPoints) ||
        (vecPosBuffer && !vecPosPoints) || (grpVerticesBuffer && !grpVertices)) {
        return;
    }

#pragma mark commit position

    if (dData.mPointBuffer.size()) {
        if (vertices) {
            size_t i = 0;
            for (auto fIdx : dData.indexBuffer) {
                size_t idxOffset = fIdx * 3;
                vertices[i++] = dData.mPointBuffer[idxOffset];
                vertices[i++] = dData.mPointBuffer[idxOffset + 1];
                vertices[i++] = dData.mPointBuffer[idxOffset + 2];
            }
        }

        if (verticesBuffer && vertices) {
            verticesBuffer->commit(vertices);
        }

        if (vPosPoints) {
            if (dData.infoClass == 0) { // primitive attribute

                size_t indexOffset = 0;
                for (std::size_t i = 0U, e = dData.meshFirstIndex.size(); i < e; ++i) {
                    const std::size_t idxOffset = dData.meshFirstIndex[i] * 3U;
                    vPosPoints[indexOffset++] = dData.mPointBuffer[idxOffset];
                    vPosPoints[indexOffset++] = dData.mPointBuffer[idxOffset + 1];
                    vPosPoints[indexOffset++] = dData.mPointBuffer[idxOffset + 2];
                }

            } else { // point attribute
                size_t indexOffset = 0;
                for (auto p : dData.mPointBuffer) {
                    vPosPoints[indexOffset++] = p;
                }
            }
        }

        if (vPosBuffer && vPosPoints) {
            vPosBuffer->commit(vPosPoints);
        }

        if (vecPosPoints) {

            if (dData.infoClass == 0) { // primitive
                size_t indexOffset = 0;
                for (uint i = 0; i < dData.meshFirstIndex.size(); i++) {
                    const std::size_t idxOffset = dData.meshFirstIndex[i] * 3U;
                    vecPosPoints[indexOffset++] = dData.mPointBuffer[idxOffset];
                    vecPosPoints[indexOffset++] = dData.mPointBuffer[idxOffset + 1U];
                    vecPosPoints[indexOffset++] = dData.mPointBuffer[idxOffset + 2U];
                }
            } else { // point
                size_t i = 0;
                for (auto it = dData.mPointBuffer.begin(), eIt = dData.mPointBuffer.end();
                     it != eIt; ++it) {
                    vecPosPoints[i++] = (*it);
                }
            }
        }

        if (vecPosBuffer && vecPosPoints) {
            vecPosBuffer->commit(vecPosPoints);
        }
    }

#pragma mark commit index

    if (dData.showIndex) {
        if (indexPositionBuffer && indexPositions) {
            if (dData.indexType == 0) { // primitive
                size_t indexOffset = 0;
                for (std::size_t i = 0U, e = dData.meshFirstIndex.size(); i < e; ++i) {
                    const std::size_t idxOffset = dData.meshFirstIndex[i] * 3U;
                    indexPositions[indexOffset++] = dData.mPointBuffer[idxOffset];
                    indexPositions[indexOffset++] = dData.mPointBuffer[idxOffset + 1];
                    indexPositions[indexOffset++] = dData.mPointBuffer[idxOffset + 2];
                }

            } else { // point
                size_t indexOffset = 0;
                for (auto p : dData.mPointBuffer) {
                    indexPositions[indexOffset++] = p;
                }
            }
        }

        if (indexPositionBuffer && indexPositions) {
            indexPositionBuffer->commit(indexPositions);
        }

#pragma mark numeric index texture

        if (indexNumericBuffer && indexNumerics) {
            if (dData.indexType == 0) { // primitive
                for (size_t i = 0, e = dData.meshFirstIndex.size(); i < e; i++) {
                    indexNumerics[i] = static_cast<float>(i);
                }
            } else { // point
                for (size_t i = 0; i < dData.pointCount; i++) {
                    indexNumerics[i] = static_cast<float>(i);
                }
            }
        }

        if (indexNumericBuffer && indexNumerics) {
            indexNumericBuffer->commit(indexNumerics);
        }
    }

#pragma mark color shader commit

    if (dData.isColor && dData.colBuffer.size()) {
        if (colorvalues) {
            size_t i = 0;
            for (auto fIdx : dData.indexBuffer) {
                size_t idxOffset = fIdx * 4;
                colorvalues[i++] = dData.colBuffer[idxOffset];
                colorvalues[i++] = dData.colBuffer[idxOffset + 1];
                colorvalues[i++] = dData.colBuffer[idxOffset + 2];
                colorvalues[i++] = dData.colBuffer[idxOffset + 3];
            }
        }
        if (colBuffer && colorvalues) {
            colBuffer->commit(colorvalues);
        }
    }

#pragma mark vector shader commit

    if (dData.isVector && dData.vecBuffer.size()) {
        if (vectorvalues) {
            size_t i = 0;
            for (auto v : dData.vecBuffer) {
                vectorvalues[i++] = v;
            }
        }
        if (vecBuffer && vectorvalues) {
            vecBuffer->commit(vectorvalues);
        }
    }

#pragma mark value3 shader commit

    if (dData.isValue && dData.val3Buffer.size()) {
        if (val3values) {
            size_t i = 0;
            for (auto v : dData.val3Buffer) {
                val3values[i++] = v;
            }
        }
        if (val3Buffer && val3values) {
            val3Buffer->commit(val3values);
        }

        if (tmpnumvalues) {
            size_t num = 1;
            for (size_t i = 0, e = num; i < e; i++)
                tmpnumvalues[i] = 0.0;
        }
        if (tmpNumBuffer && tmpnumvalues) {
            tmpNumBuffer->commit(tmpnumvalues);
        }
    }

#pragma mark group shader vertex commit

    if (dData.isGroup && grpVertices && grpVerticesBuffer) {

        size_t indexOffset = 0;
        for (auto vIt : dData.grpPtBuffer) {
            grpVertices[indexOffset++] = vIt;
        }

        grpVerticesBuffer->commit(grpVertices);
    }

    for (int idx = 0, eidx = renderItems.length(); idx < eidx; ++idx) {
        const MHWRender::MRenderItem* item = renderItems.itemAt(idx);

        if (!item)
            continue;

        std::size_t numIndex = 0U;
        if (item->name() == DMeshColorItemName_) // mesh
        {
            if (dData.indexBuffer.size()) {
                numIndex = dData.indexBuffer.size();

                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                for (size_t c = 0; c < numIndex; c++) {
                    indices[c] = static_cast<unsigned int>(c);
                }

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        } else if (item->name() == DMeshIndexItemName_) // index
        {
            if (dData.showIndex) {
                if (dData.indexType == 0) { // prim
                    numIndex = dData.meshFirstIndex.size();
                } else { // point index
                    numIndex = dData.pointCount;
                }

                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                for (size_t c = 0; c < numIndex; c++) {
                    indices[c] = static_cast<unsigned int>(c);
                }

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        } else if (item->name() == DMeshVecItemName_ || item->name() == DMeshVal3ItemName_ ||
                   item->name() == DMeshVal2ItemName_ || item->name() == DMeshVal1ItemName_) {
            if (dData.pointCount || dData.meshFirstIndex.size()) {
                if (dData.infoClass == 0) { // prim
                    numIndex = dData.meshFirstIndex.size();
                } else { // point index
                    numIndex = dData.pointCount;
                }

                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                for (size_t c = 0; c < numIndex; c++) {
                    indices[c] = static_cast<unsigned int>(c);
                }

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        } else if (item->name() == DMGroupPolyItemName_ || item->name() == DMGroupLineItemName_ ||
                   item->name() == DMGroupPointItemName_) // group
        {
            if (dData.isGroup && dData.totalGroupPointCount && dData.grpIndexBuffer.size()) {

                numIndex = dData.grpIndexBuffer.size();

                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                size_t i = 0;
                for (auto it = dData.grpIndexBuffer.begin(), eIt = dData.grpIndexBuffer.end();
                     it != eIt; ++it) {
                    indices[i] = (*it);
                    i++;
                }

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        }
    }
    mDataChanged = false;
}
DOMMY_VIEWPORT_VOID_CATCH("DMMarkerOverride", "populateGeometry")
