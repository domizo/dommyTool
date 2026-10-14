//
//  DCMarker.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/10.
//

#include "DCMarker.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"




#include <sstream>
#include <algorithm>
#include <cmath>

#include "DMayaHeaders.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DNodeOperation.h"

#include "DColorOperation.h"
#include "DStringOp.h"
#include "DDataOp.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DViewportAttributeConversion.h"


#include "dcurve_ids.h"





#pragma mark DCMarker

MTypeId DCMarker::typeId( ID_DC_MARKER );
const MString DCMarker::typeName("DCMarker");
MString    DCMarker::classification("drawdb/geometry/DCMarker");
MString    DCMarker::registrantId("DCMarkerPlugin");


MObject DCMarker::inDC;

MObject DCMarker::aIsCenterVisible;

MObject DCMarker::aShowIndex;
MObject DCMarker::aIndexType;
MObject DCMarker::aLineWidth;
MObject DCMarker::aSolidColor;

MObject DCMarker::aAllAttributeNames;
MObject DCMarker::aIsColor;
MObject DCMarker::aIsValue;
MObject DCMarker::aIsVector;
MObject DCMarker::aVisibleAttributeName;

MObject DCMarker::aIsBlackTransparency;
MObject DCMarker::aIsDrawHeads;
MObject DCMarker::aVelocityScale;

//Group
MObject DCMarker::aAllGroupNames;
MObject DCMarker::aIsGroup;
MObject DCMarker::aGroupName;
MObject DCMarker::aGroupColor;
MObject DCMarker::aGroupTransparency;
MObject DCMarker::aPointSize;

MObject DCMarker::aNodeInfo;


DCMarker::DCMarker() {}

DCMarker::~DCMarker() {}

void* DCMarker::creator()
{
    return ::dommy::maya::createObject("DCMarker", [&]() {
        return new DCMarker();
    });
}
MStatus DCMarker::compute( const MPlug& plug, MDataBlock& data ) try
{
    if(plug == aNodeInfo || plug == aAllAttributeNames || plug == aAllGroupNames) {
        
        
        MPlug dcPlug = MPlug(thisMObject(), DCMarker::inDC);
        
        MStatus outputStatus;
        const bool connected = dcPlug.isConnected(&outputStatus);
        if (!outputStatus) return outputStatus;
        const DCurveData* dCurves = NULL;
        if (connected) {
            outputStatus = getValidInputDC(inDC, data, dCurves);
            if (!outputStatus) return outputStatus;
            if (!dCurves) return MS::kFailure;
        }
        if (plug == aAllAttributeNames || plug == aAllGroupNames) {
            DStringList names;
            if (dCurves) {
                names = plug == aAllAttributeNames
                    ? dCurves->getAllAttributeList() : dCurves->getAllGroupList();
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
        
        infoStr << "DCurve Info\n";
        
        if (connected) {

            
            
            if (dCurves) {
                
                
                
                
                //position
                
                infoStr << "    DCurve count: " << dCurves->numberOfCurves() << "\n";
                infoStr << "    total cv count: " << dCurves->getTotalCvs() << "\n";
                
                if (dCurves->numberOfCurves()) {
                    //primitive attribute
                    size_t attrCount = dCurves->numberOfAttributes();
                    infoStr << "    curve attribute count: " << attrCount << "\n";
                    
                    for(auto itr = dCurves->beginAttr(); itr != dCurves->endAttr(); ++itr) {
                        infoStr << "        @" << itr->first << " : " << itr->second->typeName() <<  "\n";
                    }
                    
                    //point attribute
                    size_t attrPtCount = dCurves->numberOfPointAttribute(false);
                    std::vector<std::string> attrInfos = dCurves->getPointAttributeInfo();
                    infoStr << "    point attribute count: " << attrPtCount << "\n";
                    for(auto itr = attrInfos.begin(); itr != attrInfos.end(); ++itr) {
                        infoStr << "        " << *itr << "\n";
                    }
                    
                    
                    
                    //primitive group
                    size_t grpCount = dCurves->numberOfGroups();
                    if(grpCount) {
                        infoStr << "    primitive group count: " << grpCount << "\n";
                        for(auto itr = dCurves->beginGrp(); itr != dCurves->endGrp(); ++itr) {
                            infoStr << "        *" << itr->first <<  "\n";
                        }
                    }
                    
                    
                    //point group
                    size_t grpPtCount = dCurves->numberOfCompGroup(D_COMP_POINT);
                    if (grpPtCount) {
                        infoStr << "    point group count: " << grpPtCount << "\n";
                        std::vector<std::string> grpPtInfos = dCurves->getCompGroupInfo(D_COMP_POINT);
                        for(auto itr = grpPtInfos.begin(); itr != grpPtInfos.end(); ++itr) {
                            infoStr << "        " << *itr << "\n";
                        }
                    }
                    
                    
                    
                }
                
                
                
            } else {
                
                infoStr << "DCurve data is empty.";
            }
        } else {
            
            infoStr << "inDC plug is not connected.";
        }
        
        
        MString str = dommy::text::checkedStreamText(infoStr, "DCMarker info formatting failed").c_str();
        MDataHandle strHandle = data.outputValue(aNodeInfo, &outputStatus);
        if (!outputStatus) return outputStatus;
        strHandle.set(str);
        
        return data.setClean(plug);
        
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCMarker")

bool DCMarker::isBounded() const {
    return true;
}

MBoundingBox DCMarker::boundingBox() const try {

    MPoint corner1(-0.5, -0.5, -0.5);
    MPoint corner2(0.5, 0.5, 0.5);

    MPlug dcPlug = MPlug(thisMObject(), DCMarker::inDC);

    MStatus connectionStatus;
    const bool connected = dcPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DCMarker: unable to query input connection for bounds.");
    } else if (connected) {

        dommy::maya::ScopedPlugDataHandle dCrvHndOwner;
        MDataHandle& dCrvHnd = dCrvHndOwner.handle();
        if (dCrvHndOwner.read(dcPlug) != MS::kSuccess) {
            return MBoundingBox(corner1, corner2);
        }

        const DCurveData* const dCurves(getInputDCfromHandle(dCrvHnd));

        if (dCurves && dCurves->numberOfCurves()) {
            const MBoundingBox bounds = dCurves->getBoundingBox();
            if (dommy::maya::isFiniteBoundingBox(bounds)) {
                return bounds;
            }
        }
    }

    return MBoundingBox(corner1, corner2);
}

DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("DCMarker")

void DCMarker::draw(M3dView& /*view*/, const MDagPath& /*path*/, M3dView::DisplayStyle /*style*/,
                    M3dView::DisplayStatus /*status*/) {}

MStatus DCMarker::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnGenericAttribute gAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aIsCenterVisible = nAttr.create("IsCenterVisible", "icntv", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCenterVisible));

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

    aLineWidth = nAttr.create("LineWidth", "lnw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLineWidth));
    
    
    
    aSolidColor = nAttr.create("SolidColor", "scol", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 1.0f, 1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolidColor));
    
    
#pragma mark curve attribute
    
    const MObject aAllAttributeNamesDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAllAttributeNames = tAttr.create("AllAttributeNames", "allattrs", MFnData::kString, aAllAttributeNamesDefaultData, &stat);
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
    aVisibleAttributeName = tAttr.create("VisibleAttributeName", "visatn", MFnData::kString, visAttrName, &stat);
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
    
    
    
    //group
    const MObject aAllGroupNamesDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAllGroupNames = tAttr.create("AllGroupNames", "allgrps", MFnData::kString, aAllGroupNamesDefaultData, &stat);
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
    aGroupName = tAttr.create("GroupName", "grpnm", MFnData::kString, aGroupNameDefaultData, &stat);
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
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.7f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupTransparency));
    
    
    aPointSize = nAttr.create("PointSize", "ptsz", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointSize));
    
    
    
    
    
    MObject emptyStr = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, emptyStr, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setAffectsAppearance(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNodeInfo));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, aAllGroupNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, aAllAttributeNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, aNodeInfo));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCMarker", "initialize", std::current_exception());
}

#pragma mark DCMarkerOverride

DCMarkerOverride::DCMarkerOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mCenterUIShader(NULL)
, mIndexUIShader(NULL)
, mColorLineShader(NULL)
, mVectorUIShader(NULL)
, mValueUIShader(NULL)
, mValue2UIShader(NULL)
, mValue3UIShader(NULL)
, mGroupLineShader(NULL)
, mGroupPointShader(NULL)
, mDGNode(obj)
, mDataChanged(true)

{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DCMarkerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mCenterUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dIntegerNumericShader, mIndexUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVThickLineShader, mColorLineShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloatNumericShader, mValueUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat2NumericShader, mValue2UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat3NumericShader, mValue3UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mGroupLineShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mGroupPointShader);
}



DCMarkerOverride::~DCMarkerOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DCMarkerOverride");
    shaderRelease.release(mCenterUIShader);
    shaderRelease.release(mIndexUIShader);
    shaderRelease.release(mColorLineShader);
    shaderRelease.release(mVectorUIShader);
    shaderRelease.release(mValueUIShader);
    shaderRelease.release(mValue2UIShader);
    shaderRelease.release(mValue3UIShader);
    shaderRelease.release(mGroupLineShader);
    shaderRelease.release(mGroupPointShader);
}


MHWRender::DrawAPI DCMarkerOverride::supportedDrawAPIs() const
{
    // this plugin supports both GL and DX
    return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark primitive attribute method
template <typename T>
inline void setPrimSingleValueColor(const DCurveData &inputData, const std::string &name, bool isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::ScalarColor, isBlackTrans);
    finValues.swap(candidate);
}


template <typename T>
inline void setPrim1DVector(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::ScalarVector);
    finValues.swap(candidate);
}


template <typename T>
inline void setPrimSimpleVectorValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::Vector3);
    finValues.swap(candidate);
}

template <typename T>
inline void setPrimTwoVectorValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::Vector2);
    finValues.swap(candidate);
}

template <typename T>
inline void setCurve2To3DVectorValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::Vector2XZ);
    finValues.swap(candidate);
}


template <typename T>
inline void setPrimTwoVecColor(const DCurveData &inputData, const std::string &name, bool isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::Vector2Color, isBlackTrans);
    finValues.swap(candidate);
}

template <typename T>
inline void setPrimThreeVecColor(const DCurveData &inputData, const std::string &name, bool isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::Vector3Color, isBlackTrans);
    finValues.swap(candidate);
}


template <typename T>
inline void setPrimSimpleSingleValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePrimitiveAttributeValues(
        inputData.attrListValue<T>(name), inputData.numberOfCurves(),
        inputData.beginCurves(), inputData.endCurves(), dommy::viewport::AttributeDisplayMode::Scalar);
    finValues.swap(candidate);
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark point attribute method
template <typename T>
inline void setPointSingleValueColor(const DCurveData &inputData, const std::string &name, bool isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::ScalarLuminanceColor, isBlackTrans);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}


template <typename T>
inline void setPoint1DVector(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::ScalarVector);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}


template <typename T>
inline void setPointSimpleVectorValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::Vector3);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}



template <typename T>
inline void setCurvePoint2To3DVectorValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::Vector2XZ);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}


template <typename T>
inline void setPointTwoVectorValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::Vector2);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setPointTwoVecColor(const DCurveData &inputData, const std::string &name, bool isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::Vector2Color, isBlackTrans);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

template <typename T>
inline void setPointThreeVecColor(const DCurveData &inputData, const std::string &name, bool isBlackTrans, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::Vector3Color, isBlackTrans);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}


template <typename T>
inline void setPointSimpleSingleValue(const DCurveData &inputData, const std::string &name, std::vector<float> &finValues)
{
    auto candidate = dommy::viewport::makePointAttributeValues<T>(
        inputData.beginCurves(), inputData.endCurves(), name,
        dommy::viewport::AttributeDisplayMode::Scalar);
    dommy::viewport::appendAttributeValues(finValues, candidate);
}

/////////////////////////////////////////////////////////////////////////////////////////////

static const MString DCMarkCenterItemName_          = "DCMarkCenterItem";
static const MString DCMarkIndexItemName_          = "DCMarkIndexItem";
static const MString DCMarkColorItemName_       = "DCMarkColorItem";
static const MString DCMarkVecItemName_               = "DCMarkVecItem";
static const MString DCMarkVal1ItemName_               = "DCMarktVal1Item";
static const MString DCMarkVal2ItemName_               = "DCMarkVal2Item";
static const MString DCMarkVal3ItemName_               = "DCMarkVal3Item";

static const MString DCMarkGroupLineItemName_               = "DCMarkGroupLineItem";
static const MString DCMarkGroupPointItemName_              = "DCMarkGroupPointItem";

namespace {

void clearMarkerGroup(DCMarkerOverride::DrawData& data) noexcept {
    data.grpPtBuffer.clear();
    data.grpIndexBuffer.clear();
    data.totalGroupPointCount = 0U;
    data.isGroup = false;
}

void clearMarkerGeometry(DCMarkerOverride::DrawData& data) noexcept {
    data.centerPtBuffer.clear();
    data.cvsBuffer.clear();
    data.indexBuffer.clear();
    data.crvFirstIndex.clear();
    data.colBuffer.clear();
    data.vecBuffer.clear();
    data.val3Buffer.clear();
    clearMarkerGroup(data);
    data.pointCount = 0U;
    data.primCount = 0U;
    data.valType = DP_VAL_FLOAT;
}

} // namespace

void DCMarkerOverride::updateDG() try {
    mDataChanged = true;
    clearMarkerGeometry(dData);

    if (readPlugValue(mDGNode, DCMarker::aSolidColor, dData.solidColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.solidColor)) {
        dData.solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (readPlugValue(mDGNode, DCMarker::aIsCenterVisible, dData.isCenterVis) != MS::kSuccess ||
        readPlugValue(mDGNode, DCMarker::aShowIndex, dData.showIndex) != MS::kSuccess ||
        readPlugValue(MPlug(mDGNode, DCMarker::aIndexType), dData.indexType) != MS::kSuccess) return;
    double lineWidth = 2.0;
    if (readPlugValue(mDGNode, DCMarker::aLineWidth, lineWidth) != MS::kSuccess ||
        !std::isfinite(lineWidth) || lineWidth < 0.0) {
        lineWidth = 2.0;
    }
    dData.lineWidth = static_cast<float>(lineWidth);

    if (readPlugValue(mDGNode, DCMarker::aIsDrawHeads, dData.isDrawHeads) != MS::kSuccess ||
        readPlugValue(mDGNode, DCMarker::aIsBlackTransparency, dData.isBlackTransparency) != MS::kSuccess) return;
    double velocityScale = 1.0;
    if (readPlugValue(mDGNode, DCMarker::aVelocityScale, velocityScale) != MS::kSuccess ||
        !std::isfinite(velocityScale) || velocityScale < 0.0) {
        velocityScale = 1.0;
    }
    dData.velocityScale = static_cast<float>(velocityScale);

    if (readPlugValue(mDGNode, DCMarker::aIsColor, dData.isColor) != MS::kSuccess ||
        readPlugValue(mDGNode, DCMarker::aIsValue, dData.isValue) != MS::kSuccess ||
        readPlugValue(mDGNode, DCMarker::aIsVector, dData.isVector) != MS::kSuccess) return;

    dData.infoClass = 0;
    bool isShowAttr = (dData.isVector || dData.isColor || dData.isValue) ? true : false;
    MString visibleAttribute;
    if (readPlugValue(mDGNode, DCMarker::aVisibleAttributeName, visibleAttribute) != MS::kSuccess) return;
    const std::string visAttrStr(visibleAttribute.asChar());

    // group
    if (readPlugValue(mDGNode, DCMarker::aIsGroup, dData.isGroup) != MS::kSuccess) return;
    bool isShowGrp = dData.isGroup;

    MString groupName;
    if (readPlugValue(mDGNode, DCMarker::aGroupName, groupName) != MS::kSuccess) return;
    const std::string grpNameStr(groupName.asChar());

    if (readPlugValue(mDGNode, DCMarker::aGroupColor, dData.groupColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.groupColor)) {
        dData.groupColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    double groupAlpha = 0.7;
    if (readPlugValue(mDGNode, DCMarker::aGroupTransparency, groupAlpha) != MS::kSuccess ||
        !std::isfinite(groupAlpha)) {
        groupAlpha = 0.7;
    }
    dData.groupAlpha = static_cast<float>(std::max(0.0, std::min(1.0, groupAlpha)));
    double pointSize = 5.0;
    if (readPlugValue(mDGNode, DCMarker::aPointSize, pointSize) != MS::kSuccess ||
        !std::isfinite(pointSize) || pointSize < 0.0) {
        pointSize = 5.0;
    }
    dData.pointSize = static_cast<float>(pointSize);

    MPlug dcPlug(mDGNode, DCMarker::inDC);
    if (dcPlug.isNull()) return;
    MStatus connectionStatus;
    const bool connected = dcPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DCMarker: unable to query input connection for drawing.");
        return;
    }
    if (connected) {
        dommy::maya::ScopedPlugDataHandle dCurveHndOwner;
        MDataHandle& dCurveHnd = dCurveHndOwner.handle();
        if (dCurveHndOwner.read(dcPlug) != MS::kSuccess) {
            MGlobal::displayError("DCMarker: unable to evaluate curve input data.");
            return;
        }
        const DCurveData* const dCurves(getInputDCfromHandle(dCurveHnd));
        if (!dCurves) {
            MGlobal::displayError("DCMarker: unable to read curve input data.");
            return;
        }
        size_t curveCount = dCurves->numberOfCurves();

        if (dCurves && curveCount) {

#pragma mark attribute check
            if (isShowAttr && !visAttrStr.empty()) {
                DComponentType compType = dCurves->getAttributeComponentType(visAttrStr);
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
                DComponentType compType = dCurves->getGroupComponentType(grpNameStr);
                if (compType == D_COMP_PRIM) {
                    dData.grpType = D_COMP_PRIM;
                } else if (compType == D_COMP_POINT) {
                    dData.grpType = D_COMP_POINT;
                } else {
                    dData.isGroup = false;
                }
            } // check visible group

            size_t totalCurve = 0;
            size_t totalCvs = 0;
            if (!dCurves->getCheckedTotalCvs(totalCvs) ||
                totalCvs > dommy::viewport::kMaxViewportVertexCount) {
                MGlobal::displayError("DCMarker: curve points exceed the viewport budget.");
                return;
            }
            std::size_t scalarCount = 0;
            std::size_t lineIndexCount = 0;
            if (!dommy::geometry::checkedTupleScalarCount(totalCvs, 3, scalarCount) ||
                scalarCount > dommy::viewport::kMaxViewportScalarCount) {
                MGlobal::displayError("DCMarker: point buffer exceeds the viewport budget.");
                return;
            }
            for (auto cIt = dCurves->beginCurves(), eIt = dCurves->endCurves(); cIt != eIt; ++cIt) {
                if (!(*cIt) || (*cIt)->numberOfPoints() == 0U ||
                    !dommy::geometry::checkedAccumulatePolylineIndices((*cIt)->numberOfPoints(),
                                                                       lineIndexCount) ||
                    lineIndexCount > dommy::viewport::kMaxViewportIndexCount) {
                    MGlobal::displayError(
                        "DCMarker: invalid curve or viewport index budget exceeded.");
                    return;
                }
            }

#pragma mark center data
            if (dData.isCenterVis) {
                std::size_t centerScalarCount = 0;
                if (!dommy::geometry::checkedTupleScalarCount(curveCount, 3, centerScalarCount) ||
                    centerScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DCMarker: center point buffer size overflow.");
                    return;
                }
                dData.centerPtBuffer.resize(centerScalarCount);
            } // set center data

#pragma mark set curve geometry data

            dData.cvsBuffer.resize(scalarCount);
            dData.indexBuffer.resize(lineIndexCount);
            dData.crvFirstIndex.resize(curveCount);
            size_t cvOffset = 0;
            size_t idOffset = 0;
            size_t vIndex = 0;

            for (auto cIt = dCurves->beginCurves(), ecIt = dCurves->endCurves(); cIt != ecIt;
                 ++cIt) {
                dData.crvFirstIndex[totalCurve] = vIndex;

                const DPointList* const sourcePoints = (*cIt)->getPositionPtrConst();
                if (!sourcePoints) {
                    MGlobal::displayError("DCMarker: curve has no point storage.");
                    return;
                }
                const DPointList pts =
                    (*cIt)->isIdentity() ? *sourcePoints : (*cIt)->getPointsInWorld();

                size_t count = pts.size();

                if (dData.isCenterVis) {
                    MBoundingBox curveBounds;
                    for (DPointList::const_iterator point = pts.begin(); point != pts.end();
                         ++point) {
                        curveBounds.expand(DPToMP(*point));
                    }
                    const MPoint center = curveBounds.center();
                    const std::size_t centerOffset = totalCurve * 3U;
                    dData.centerPtBuffer[centerOffset] = static_cast<float>(center.x);
                    dData.centerPtBuffer[centerOffset + 1U] = static_cast<float>(center.y);
                    dData.centerPtBuffer[centerOffset + 2U] = static_cast<float>(center.z);
                }

                tbb::blocked_range<size_t> range =
                    tbb::blocked_range<size_t>(0, count, DGetGlobalProcessNum(count));
                tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& r) {
                    for (size_t i = r.begin(); i != r.end(); i++) {
                        size_t index = i * 3;
                        dData.cvsBuffer[index + vIndex] = pts[i][0];
                        dData.cvsBuffer[index + vIndex + 1] = pts[i][1];
                        dData.cvsBuffer[index + vIndex + 2] = pts[i][2];
                    }
                });
                vIndex += count * 3;

                // add index
                size_t idx = 0;
                size_t eidx = count - 1;
                while (idx < eidx) {

                    dData.indexBuffer[idOffset++] = static_cast<unsigned int>(cvOffset + idx);
                    dData.indexBuffer[idOffset++] = static_cast<unsigned int>(cvOffset + idx + 1U);

                    idx++;
                }

                cvOffset += (*cIt)->numberOfPoints();
                totalCurve++;

            } // loop curve

            if (totalCurve != curveCount || cvOffset != totalCvs || idOffset != lineIndexCount ||
                vIndex != scalarCount) {
                MGlobal::displayError("DCMarker: generated viewport layout is inconsistent.");
                return;
            }
            dData.pointCount = totalCvs;
            dData.primCount = curveCount;

            //display info
            
            if(isShowAttr && curveCount) {
                if (dData.infoClass==0) {//primitive attribute
#pragma mark get primitive attribute
                    //Check primitive attribute exist
                    if (dCurves->isExistAttr(visAttrStr)) {
                        
                        DP_ATTR_DATA_TYPE type = dCurves->getAttr(visAttrStr)->getAttrType();
                        
                        
                        
                        //color
                        if (dData.isColor) {
                            if (isSingleValueAttrType(type)) {
                                
                                dData.valType = DP_VAL_INT;
                                if (type == DP_ATTR_INT) {
                                    setPrimSingleValueColor<int32_t>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_BOOL) {
                                    setPrimSingleValueColor<bool>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_UINT32) {
                                    setPrimSingleValueColor<uint32_t>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_INT64) {
                                    setPrimSingleValueColor<int64_t>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_FLOAT) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPrimSingleValueColor<float>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_DOUBLE) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPrimSingleValueColor<double>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }
                                
                            }  else if (isVector2DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC2I) {
                                    dData.valType = DP_VAL_INT2;
                                    setPrimTwoVecColor<Imath::V2i>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_VEC2F) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPrimTwoVecColor<Imath::V2f>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }else if (type == DP_ATTR_VEC2D) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPrimTwoVecColor<Imath::V2d>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }
                                
                            } else if (isVector3DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC3I) {
                                    dData.valType = DP_VAL_INT3;
                                    setPrimThreeVecColor<Imath::V3i>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_VEC3F) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPrimThreeVecColor<Imath::V3f>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }else if (type == DP_ATTR_VEC3D) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPrimThreeVecColor<Imath::V3d>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }
                                
                            }
                            
                            
                            
                            
                        }//isColor
                        
                        if (dData.isVector) {
                            
                            //Vector
                            if (isSingleValueAttrType(type)) {
                                dData.valType = DP_VAL_INT;
                                if (type == DP_ATTR_INT) {
                                    setPrim1DVector<int32_t>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_BOOL) {
                                    setPrim1DVector<bool>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_UINT32) {
                                    setPrim1DVector<uint32_t>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_INT64) {
                                    setPrim1DVector<int64_t>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_FLOAT) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPrim1DVector<float>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_DOUBLE) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPrim1DVector<double>(*dCurves, visAttrStr, dData.vecBuffer);
                                }
                                
                                
                            } else if (isVector2DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC2I) {
                                    dData.valType = DP_VAL_INT2;
                                    setCurve2To3DVectorValue<Imath::V2i>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_VEC2F) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setCurve2To3DVectorValue<Imath::V2f>(*dCurves, visAttrStr, dData.vecBuffer);
                                }else if (type == DP_ATTR_VEC2D) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setCurve2To3DVectorValue<Imath::V2d>(*dCurves, visAttrStr, dData.vecBuffer);
                                }
                                
                            }  else if (isVector3DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC3I) {
                                    dData.valType = DP_VAL_INT3;
                                    setPrimSimpleVectorValue<Imath::V3i>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_VEC3F) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPrimSimpleVectorValue<Imath::V3f>(*dCurves, visAttrStr, dData.vecBuffer);
                                }else if (type == DP_ATTR_VEC3D) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPrimSimpleVectorValue<Imath::V3d>(*dCurves, visAttrStr, dData.vecBuffer);
                                }
                            }
                            
                            
                            
                        }//isVector
                        
                        
                        if (dData.isValue) {
                            
                            //Values
                            if (isSingleValueAttrType(type)) {
                                
                                dData.valType = DP_VAL_INT;
                                
                                if (type == DP_ATTR_INT) {
                                    setPrimSimpleSingleValue<int32_t>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_BOOL) {
                                    setPrimSimpleSingleValue<bool>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_UINT32) {
                                    setPrimSimpleSingleValue<uint32_t>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_INT64) {
                                    setPrimSimpleSingleValue<int64_t>(*dCurves, visAttrStr, dData.val3Buffer);
                                    
                                } else if (type == DP_ATTR_FLOAT) {
                                    
                                    dData.valType = DP_VAL_FLOAT;
                                    setPrimSimpleSingleValue<float>(*dCurves, visAttrStr, dData.val3Buffer);
                                    
                                } else if (type == DP_ATTR_DOUBLE) {
                                    
                                    dData.valType = DP_VAL_FLOAT;
                                    setPrimSimpleSingleValue<double>(*dCurves, visAttrStr, dData.val3Buffer);
                                    
                                }
                                
                            } else if (isVector2DAttrType(type)) {
                                
                                
                                if (type == DP_ATTR_VEC2I) {
                                    dData.valType = DP_VAL_INT2;
                                    setPrimTwoVectorValue<Imath::V2i>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_VEC2F) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPrimTwoVectorValue<Imath::V2f>(*dCurves, visAttrStr, dData.val3Buffer);
                                }else if (type == DP_ATTR_VEC2D) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPrimTwoVectorValue<Imath::V2d>(*dCurves, visAttrStr, dData.val3Buffer);
                                }
                                
                            } else if (isVector3DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC3I) {
                                    dData.valType = DP_VAL_INT3;
                                    setPrimSimpleVectorValue<Imath::V3i>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_VEC3F) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPrimSimpleVectorValue<Imath::V3f>(*dCurves, visAttrStr, dData.val3Buffer);
                                }else if (type == DP_ATTR_VEC3D) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPrimSimpleVectorValue<Imath::V3d>(*dCurves, visAttrStr, dData.val3Buffer);
                                }
                                
                            }
                            
                            
                        }//isValue
                        
                    } else {
                        MGlobal::displayWarning("Not exist primitive attribute!");
                        dData.isColor = dData.isVector = dData.isValue = false;
                    }
                    
                } else {//point attribute
#pragma mark get point attribute
                    
                    
                    //Check point attribute exist
                    if (dCurves->isExistPointAttr(visAttrStr)) {
                        
                        DP_ATTR_DATA_TYPE type = dCurves->getPointAttributeType(visAttrStr);
                        
                        
                        
                        
                        //color
                        if (dData.isColor) {
                            if (isSingleValueAttrType(type)) {
                                
                                dData.valType = DP_VAL_INT;
                                if (type == DP_ATTR_INT) {
                                    setPointSingleValueColor<int32_t>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_BOOL) {
                                    setPointSingleValueColor<bool>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_UINT32) {
                                    setPointSingleValueColor<uint32_t>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_INT64) {
                                    setPointSingleValueColor<int64_t>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_FLOAT) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPointSingleValueColor<float>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_DOUBLE) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPointSingleValueColor<double>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }
                                
                            }  else if (isVector2DAttrType(type)) {
                                
                                
                                if (type == DP_ATTR_VEC2I) {
                                    dData.valType = DP_VAL_INT2;
                                    setPointTwoVecColor<Imath::V2i>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_VEC2F) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPointTwoVecColor<Imath::V2f>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }else if (type == DP_ATTR_VEC2D) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPointTwoVecColor<Imath::V2d>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }
                                
                            } else if (isVector3DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC3I) {
                                    dData.valType = DP_VAL_INT3;
                                    setPointThreeVecColor<Imath::V3i>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                } else if (type == DP_ATTR_VEC3F) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPointThreeVecColor<Imath::V3f>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }else if (type == DP_ATTR_VEC3D) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPointThreeVecColor<Imath::V3d>(*dCurves, visAttrStr, dData.isBlackTransparency, dData.colBuffer);
                                }
                                
                            }
                            
                            
                            
                            
                        }//isColor
                        
                        
                        if (dData.isVector) {
                            
                            //Vector
                            if (isSingleValueAttrType(type)) {
                                dData.valType = DP_VAL_INT;
                                if (type == DP_ATTR_INT) {
                                    setPoint1DVector<int32_t>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_BOOL) {
                                    setPoint1DVector<bool>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_UINT32) {
                                    setPoint1DVector<uint32_t>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_INT64) {
                                    setPoint1DVector<int64_t>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_FLOAT) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPoint1DVector<float>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_DOUBLE) {
                                    dData.valType = DP_VAL_FLOAT;
                                    setPoint1DVector<double>(*dCurves, visAttrStr, dData.vecBuffer);
                                }
                                
                                
                            } else if (isVector2DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC2I) {
                                    dData.valType = DP_VAL_INT2;
                                    setCurvePoint2To3DVectorValue<Imath::V2i>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_VEC2F) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setCurvePoint2To3DVectorValue<Imath::V2f>(*dCurves, visAttrStr, dData.vecBuffer);
                                }else if (type == DP_ATTR_VEC2D) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setCurvePoint2To3DVectorValue<Imath::V2d>(*dCurves, visAttrStr, dData.vecBuffer);
                                }
                                
                            } else if (isVector3DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC3I) {
                                    dData.valType = DP_VAL_INT3;
                                    setPointSimpleVectorValue<Imath::V3i>(*dCurves, visAttrStr, dData.vecBuffer);
                                } else if (type == DP_ATTR_VEC3F) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPointSimpleVectorValue<Imath::V3f>(*dCurves, visAttrStr, dData.vecBuffer);
                                }else if (type == DP_ATTR_VEC3D) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPointSimpleVectorValue<Imath::V3d>(*dCurves, visAttrStr, dData.vecBuffer);
                                }
                            }
                            
                            
                            
                        }//isVector
                        
                        if (dData.isValue) {
                            
                            //Values
                            if (isSingleValueAttrType(type)) {
                                
                                dData.valType = DP_VAL_INT;
                                
                                if (type == DP_ATTR_INT) {
                                    setPointSimpleSingleValue<int32_t>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_BOOL) {
                                    setPointSimpleSingleValue<bool>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_UINT32) {
                                    setPointSimpleSingleValue<uint32_t>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_INT64) {
                                    setPointSimpleSingleValue<int64_t>(*dCurves, visAttrStr, dData.val3Buffer);
                                    
                                } else if (type == DP_ATTR_FLOAT) {
                                    
                                    dData.valType = DP_VAL_FLOAT;
                                    setPointSimpleSingleValue<float>(*dCurves, visAttrStr, dData.val3Buffer);
                                    
                                } else if (type == DP_ATTR_DOUBLE) {
                                    
                                    dData.valType = DP_VAL_FLOAT;
                                    setPointSimpleSingleValue<double>(*dCurves, visAttrStr, dData.val3Buffer);
                                    
                                }
                                
                            } else if (isVector2DAttrType(type)) {
                                
                                
                                if (type == DP_ATTR_VEC2I) {
                                    dData.valType = DP_VAL_INT2;
                                    setPointTwoVectorValue<Imath::V2i>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_VEC2F) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPointTwoVectorValue<Imath::V2f>(*dCurves, visAttrStr, dData.val3Buffer);
                                }else if (type == DP_ATTR_VEC2D) {
                                    dData.valType = DP_VAL_FLOAT2;
                                    setPointTwoVectorValue<Imath::V2d>(*dCurves, visAttrStr, dData.val3Buffer);
                                }
                                
                            } else if (isVector3DAttrType(type)) {
                                
                                if (type == DP_ATTR_VEC3I) {
                                    dData.valType = DP_VAL_INT3;
                                    setPointSimpleVectorValue<Imath::V3i>(*dCurves, visAttrStr, dData.val3Buffer);
                                } else if (type == DP_ATTR_VEC3F) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPointSimpleVectorValue<Imath::V3f>(*dCurves, visAttrStr, dData.val3Buffer);
                                }else if (type == DP_ATTR_VEC3D) {
                                    dData.valType = DP_VAL_FLOAT3;
                                    setPointSimpleVectorValue<Imath::V3d>(*dCurves, visAttrStr, dData.val3Buffer);
                                }
                                
                            }
                            
                            
                        }//isValue

                    } else {
                        MGlobal::displayWarning("Not exist point attribute!");
                        dData.isColor = dData.isVector = dData.isValue = false;
                    }

                } // proint or primitive attribute check

            } // check display attribute

            if (dData.isColor) {
                std::size_t expectedColorCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(totalCvs, 4U, expectedColorCount) ||
                    expectedColorCount != dData.colBuffer.size() ||
                    expectedColorCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DCMarker: color attribute layout is inconsistent.");
                    dData.colBuffer.clear();
                    dData.isColor = false;
                }
            }

            const std::size_t attributeElementCount = dData.infoClass == 0 ? curveCount : totalCvs;
            if (dData.isVector) {
                std::size_t expectedVectorCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(attributeElementCount, 3U,
                                                              expectedVectorCount) ||
                    expectedVectorCount != dData.vecBuffer.size() ||
                    expectedVectorCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DCMarker: vector attribute layout is inconsistent.");
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
                    MGlobal::displayError("DCMarker: value attribute layout is inconsistent.");
                    dData.val3Buffer.clear();
                    dData.isValue = false;
                }
            }

#pragma mark visible group
            if (dData.isGroup) {

                if (dData.grpType == D_COMP_PRIM) {

#pragma mark primitive group

                    if (dCurves->isExistGrp(grpNameStr)) {

                        DPGroupData::ConstPtr grpdata = dCurves->getGrp(grpNameStr);
                        if (!grpdata) {
                            MGlobal::displayError("DCMarker: primitive group data is unavailable.");
                            clearMarkerGroup(dData);
                            return;
                        }
                        DPIdList idsList = grpdata->getGroupIds();

                        if (idsList.size() == dCurves->numberOfCurves()) {

                            size_t i = 0;
                            size_t totalPoint = 0;

                            for (auto cIt = dCurves->beginCurves(), ecIt = dCurves->endCurves();
                                 cIt != ecIt; ++cIt) {

                                if (!idsList[i]) {
                                    i++;
                                    continue;
                                }
                                const DPointList* const sourcePoints =
                                    (*cIt)->getPositionPtrConst();
                                if (!sourcePoints) {
                                    MGlobal::displayError(
                                        "DCMarker: group curve has no point storage.");
                                    return;
                                }
                                const DPointList points = (*cIt)->isIdentity()
                                                              ? *sourcePoints
                                                              : (*cIt)->getPointsInWorld();
                                for (DPointList::const_iterator point = points.begin();
                                     point != points.end(); ++point) {
                                    dData.grpPtBuffer.push_back((*point)[0]);
                                    dData.grpPtBuffer.push_back((*point)[1]);
                                    dData.grpPtBuffer.push_back((*point)[2]);
                                }

                                // add index
                                size_t idx = 0;
                                const size_t pointCount = (*cIt)->numberOfPoints();
                                const size_t fin = pointCount > 0U ? pointCount - 1U : 0U;
                                while (idx < fin) {

                                    dData.grpIndexBuffer.push_back(
                                        static_cast<unsigned int>(idx + totalPoint));
                                    dData.grpIndexBuffer.push_back(
                                        static_cast<unsigned int>(idx + 1U + totalPoint));
                                    idx++;
                                }

                                totalPoint += (*cIt)->numberOfPoints();
                                i++;
                            }

                            dData.totalGroupPointCount = totalPoint;

                        } // check count

                    } // check group exist

                } else if (dData.grpType == D_COMP_POINT) {

#pragma mark point group
                    size_t totalPoint = 0;

                    for (auto cIt = dCurves->beginCurves(), ecIt = dCurves->endCurves();
                         cIt != ecIt; ++cIt) {

                        if ((*cIt)->isExistGrp(grpNameStr)) {
                            DPGroupData::ConstPtr grpdata = (*cIt)->getGrp(grpNameStr);
                            if (!grpdata) {
                                MGlobal::displayError("DCMarker: point group data is unavailable.");
                                clearMarkerGroup(dData);
                                return;
                            }
                            DPIdList idsList = grpdata->getGroupIds();

                            if (idsList.size() == (*cIt)->numberOfPoints()) {
                                const DPointList* const sourcePoints =
                                    (*cIt)->getPositionPtrConst();
                                if (!sourcePoints) {
                                    MGlobal::displayError(
                                        "DCMarker: point group curve has no point storage.");
                                    return;
                                }
                                const DPointList points = (*cIt)->isIdentity()
                                                              ? *sourcePoints
                                                              : (*cIt)->getPointsInWorld();
                                size_t i = 0;
                                for (DPointList::const_iterator point = points.begin();
                                     point != points.end(); ++point) {
                                    if (!idsList[i]) {
                                        i++;
                                        continue;
                                    }

                                    dData.grpPtBuffer.push_back((*point)[0]);
                                    dData.grpPtBuffer.push_back((*point)[1]);
                                    dData.grpPtBuffer.push_back((*point)[2]);

                                    totalPoint++;
                                    i++;
                                } // loop point
                            } // check count
                        } // check group
                    } // loop curve

                    for (size_t f = 0; f < totalPoint; f++) {
                        dData.grpIndexBuffer.push_back(static_cast<unsigned int>(f));
                    }

                    dData.totalGroupPointCount = totalPoint;

                } // check display type

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
                    MGlobal::displayError("DCMarker: group viewport layout is inconsistent.");
                    clearMarkerGroup(dData);
                }

            } // check group display

        } // DCurve check

    } // plug check
} catch (...) {
    clearMarkerGeometry(dData);
    mDataChanged = true;
    dommy::maya::reportViewportCallbackException("DCMarkerOverride", "updateDG",
                                                 std::current_exception());
}

void DCMarkerOverride::updateRenderItems(
    const MDagPath& /*path*/,
    MHWRender::MRenderItemList& list) try
{
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DCMarkerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mCenterUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dIntegerNumericShader, mIndexUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVThickLineShader, mColorLineShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloatNumericShader, mValueUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat2NumericShader, mValue2UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat3NumericShader, mValue3UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mGroupLineShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mGroupPointShader);
    
    
    
    //get attribute
    const float  wireframeColor[4] = { dData.solidColor.r, dData.solidColor.g, dData.solidColor.b, 1.0f };//main color
    const float lineWidth[2] = { dData.lineWidth, dData.lineWidth };
    const float2 tpos = {0,-0.5};//value text position
    const float2 ipos = {0,0.2};//index text position
    
    unsigned int priority = MHWRender::MRenderItem::sActiveLineDepthPriority;
    
#pragma mark curve index shader
    MHWRender::MRenderItem* DCurveCenterItem = NULL;
    
    int index = list.indexOf(DCMarkCenterItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create( DCMarkCenterItemName_,
                                                         MHWRender::MRenderItem::DecorationItem,
                                                         MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        DCurveCenterItem = pendingItem.release();
    }
    else
    {
        DCurveCenterItem = list.itemAt(index);
    }
    
    if(DCurveCenterItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveCenterItem, mShaderUpdateFailed);
        if (mCenterUIShader)
        {
            const float  centerColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            float2 cpSize = {3.0,3.0};//point size
            shaderUpdate.setParameter(mCenterUIShader, "pointSize", cpSize);
            shaderUpdate.setParameter(mCenterUIShader, "solidColor", centerColor);
            shaderUpdate.setShader(mCenterUIShader, &DCMarkCenterItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.isCenterVis);
    }
    
#pragma mark curve index shader
    MHWRender::MRenderItem* DCurveIndexItem = NULL;
    
    index = list.indexOf(DCMarkIndexItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create( DCMarkIndexItemName_,
                                                         MHWRender::MRenderItem::DecorationItem,
                                                         MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        DCurveIndexItem = pendingItem.release();
    }
    else
    {
        DCurveIndexItem = list.itemAt(index);
    }
    
    if(DCurveIndexItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveIndexItem, mShaderUpdateFailed);
        if (mIndexUIShader)
        {
            
            shaderUpdate.setParameter(mIndexUIShader, "pointPlacement", ipos);
            shaderUpdate.setParameter(mIndexUIShader, "solidColor", wireframeColor);
            shaderUpdate.setShader(mIndexUIShader, &DCMarkIndexItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.showIndex);
    }
    
    
#pragma mark curve color shader
    MHWRender::MRenderItem* colorDCurve = NULL;
    
    index = list.indexOf(DCMarkColorItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkColorItemName_,
                                                     MHWRender::MRenderItem::DecorationItem,
                                                     MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        colorDCurve = pendingItem.release();
    }
    else
    {
        colorDCurve = list.itemAt(index);
    }
    
    if(colorDCurve)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*colorDCurve, mShaderUpdateFailed);
        if (mColorLineShader)
        {
            shaderUpdate.setParameter(mColorLineShader, "lineWidth", lineWidth);
            shaderUpdate.setShader(mColorLineShader, &DCMarkColorItemName_);
            
        }
        shaderUpdate.enable( dData.primCount && dData.isColor);
    }
    
#pragma mark Vector Shader
    MHWRender::MRenderItem* DCurveVectorItem = NULL;
    
    index = list.indexOf(DCMarkVecItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkVecItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        DCurveVectorItem = pendingItem.release();
    }
    else
    {
        DCurveVectorItem = list.itemAt(index);
    }
    
    if(DCurveVectorItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveVectorItem, mShaderUpdateFailed);
        if (mVectorUIShader)
        {
            shaderUpdate.setParameter(mVectorUIShader, "drawHeads", dData.isDrawHeads);
            shaderUpdate.setParameter(mVectorUIShader, "velocityScale", dData.velocityScale);
            shaderUpdate.setParameter(mVectorUIShader, "solidColor", wireframeColor);
            shaderUpdate.setShader(mVectorUIShader, &DCMarkVecItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.isVector);
    }
    
    
#pragma mark Value3 Shader
    MHWRender::MRenderItem* DCurveValue3Item = NULL;
    
    index = list.indexOf(DCMarkVal3ItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkVal3ItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        DCurveValue3Item = pendingItem.release();
    }
    else
    {
        DCurveValue3Item = list.itemAt(index);
    }
    
    if(DCurveValue3Item)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveValue3Item, mShaderUpdateFailed);
        if (mValue3UIShader)
        {
            shaderUpdate.setParameter(mValue3UIShader, "solidColor", wireframeColor);
            shaderUpdate.setParameter(mValue3UIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT3) {
                shaderUpdate.setParameter(mValue3UIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValue3UIShader, "isFloatValue", false);
            }
            
            shaderUpdate.setShader(mValue3UIShader, &DCMarkVal3ItemName_);
        }
        if (dData.primCount && (dData.isValue && (dData.valType==DP_VAL_FLOAT3 || dData.valType==DP_VAL_INT3))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
        
    }
    
    //Value2 Shader
    MHWRender::MRenderItem* DCurveValue2Item = NULL;
    
    index = list.indexOf(DCMarkVal2ItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkVal2ItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        DCurveValue2Item = pendingItem.release();
    }
    else
    {
        DCurveValue2Item = list.itemAt(index);
    }
    
    if(DCurveValue2Item)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveValue2Item, mShaderUpdateFailed);
        if (mValue2UIShader)
        {
            shaderUpdate.setParameter(mValue2UIShader, "solidColor", wireframeColor);
            shaderUpdate.setParameter(mValue2UIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT2) {
                shaderUpdate.setParameter(mValue2UIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValue2UIShader, "isFloatValue", false);
            }
            
            shaderUpdate.setShader(mValue2UIShader, &DCMarkVal2ItemName_);
        }
        if (dData.primCount && (dData.isValue && (dData.valType==DP_VAL_FLOAT2 || dData.valType==DP_VAL_INT2))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }
    
    //Value1 Shader
    MHWRender::MRenderItem* DCurveValue1Item = NULL;
    
    index = list.indexOf(DCMarkVal1ItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkVal1ItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(priority);
        if (!list.append(pendingItem.get())) return;
        DCurveValue1Item = pendingItem.release();
    }
    else
    {
        DCurveValue1Item = list.itemAt(index);
    }
    
    if(DCurveValue1Item)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DCurveValue1Item, mShaderUpdateFailed);
        if (mValueUIShader)
        {
            shaderUpdate.setParameter(mValueUIShader, "solidColor", wireframeColor);
            shaderUpdate.setParameter(mValueUIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT) {
                shaderUpdate.setParameter(mValueUIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValueUIShader, "isFloatValue", false);
            }
            
            shaderUpdate.setShader(mValueUIShader, &DCMarkVal1ItemName_);
        }
        if (dData.primCount && (dData.isValue && (dData.valType==DP_VAL_FLOAT || dData.valType==DP_VAL_INT))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }
    
#pragma mark group line shader
    const float  grpColor[4] = { dData.groupColor.r, dData.groupColor.g, dData.groupColor.b, dData.groupAlpha };
    
    MHWRender::MRenderItem* grpLineDMesh = NULL;
    index = list.indexOf(DCMarkGroupLineItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkGroupLineItemName_,
                                                      MHWRender::MRenderItem::DecorationItem,
                                                      MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        grpLineDMesh = pendingItem.release();
    }
    else
    {
        grpLineDMesh = list.itemAt(index);
    }
    
    if(grpLineDMesh)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*grpLineDMesh, mShaderUpdateFailed);
        if (mGroupLineShader)
        {
            const float glineWidth[2] = { dData.lineWidth+2, dData.lineWidth+2 };
            shaderUpdate.setParameter(mGroupLineShader, "lineWidth", glineWidth);
            shaderUpdate.setParameter(mGroupLineShader, "solidColor", grpColor);
            shaderUpdate.setShader(mGroupLineShader, &DCMarkGroupLineItemName_);
        }
        shaderUpdate.enable(dData.isGroup && dData.grpType==D_COMP_PRIM && dData.totalGroupPointCount>0);
    }
    
#pragma mark group point shader
    
    MHWRender::MRenderItem* grpPointDMesh = NULL;
    index = list.indexOf(DCMarkGroupPointItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCMarkGroupPointItemName_,
                                                       MHWRender::MRenderItem::DecorationItem,
                                                       MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DCMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        grpPointDMesh = pendingItem.release();
    }
    else
    {
        grpPointDMesh = list.itemAt(index);
    }
    
    if(grpPointDMesh)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*grpPointDMesh, mShaderUpdateFailed);
        if (mGroupPointShader)
        {
            float2 gpSize = {dData.pointSize,dData.pointSize};//point size
            shaderUpdate.setParameter(mGroupPointShader, "pointSize", gpSize);
            shaderUpdate.setParameter(mGroupPointShader, "solidColor", grpColor);
            shaderUpdate.setShader(mGroupPointShader, &DCMarkGroupPointItemName_);
        }
        shaderUpdate.enable(dData.isGroup && dData.grpType==D_COMP_POINT && dData.totalGroupPointCount>0);
    }
    
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH(
    "DCMarkerOverride", "updateRenderItems")

void DCMarkerOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                                             const MHWRender::MRenderItemList& renderItems,
                                             MHWRender::MGeometry& data) try
{
    mDataChanged = true;
    if (dData.cvsBuffer.empty() && dData.grpPtBuffer.empty()) {
        mDataChanged = false;
        return;
    }
    //cvs
    MHWRender::MVertexBuffer* verticesBuffer = NULL;
    float* vertices = NULL;
    
    //index
    MHWRender::MVertexBuffer* centerPositionBuffer = NULL;
    float* centerPositions = NULL;
    MHWRender::MVertexBuffer* indexPositionBuffer = NULL;
    float* indexPositions = NULL;
    MHWRender::MVertexBuffer* indexNumericBuffer = NULL;//texture
    float* indexNumerics = NULL;
    
    //color
    MHWRender::MVertexBuffer* colBuffer = NULL;
    float* colorvalues = NULL;
    
    //vector
    MHWRender::MVertexBuffer* vecBuffer = NULL;
    float* vectorvalues = NULL;
    
    //value
    MHWRender::MVertexBuffer* val3Buffer = NULL;
    float* val3values = NULL;
    
    //tmp numeric
    MHWRender::MVertexBuffer* tmpNumBuffer = NULL;
    float* tmpnumvalues = NULL;
    
    //value or vec points
    MHWRender::MVertexBuffer* vPosBuffer = NULL;
    float* vPosPoints = NULL;
    MHWRender::MVertexBuffer* vecPosBuffer = NULL;
    float* vecPosPoints = NULL;
    
    
    //group buffer
    MHWRender::MVertexBuffer* grpVerticesBuffer = NULL;
    float* grpVertices = NULL;
    
    
    
    static const MString numericValue("numericvalue");
    static const MString numeric2Value("numeric2value");
    static const MString numeric3Value("numeric3value");
    
    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic())
            {
                case MHWRender::MGeometry::kPosition:
                {

                    if (vertexBufferDescriptor.name() == DCMarkColorItemName_)
                    {
                        if (!verticesBuffer)
                        {
                            verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!verticesBuffer) return false;

                            if (verticesBuffer)
                            {
                                vertices = dommy::viewport::acquireFloatBuffer(
                                    verticesBuffer, vertexBufferDescriptor, dData.cvsBuffer.size(), 3);
                                if (!vertices) return false;

                            }
                        }
                    }
                    else if (vertexBufferDescriptor.name() == DCMarkCenterItemName_ )// Center
                    {
                        if (!centerPositionBuffer)
                        {
                            centerPositionBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!centerPositionBuffer) return false;
                            if (centerPositionBuffer)
                            {
                                centerPositions = dommy::viewport::acquireFloatBuffer(
                                    centerPositionBuffer, vertexBufferDescriptor, dData.primCount * 3, 3);
                                if (!centerPositions) return false;
                            }
                        }
                    }
                    else if (vertexBufferDescriptor.name() == DCMarkIndexItemName_ )// Index
                    {
                        if (!indexPositionBuffer)
                        {
                            indexPositionBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!indexPositionBuffer) return false;
                            if (indexPositionBuffer)
                            {
                                if (dData.indexType==0) {//primitive
                                    indexPositions = dommy::viewport::acquireFloatBuffer(
                                        indexPositionBuffer, vertexBufferDescriptor,
                                        dData.crvFirstIndex.size() * 3, 3);
                                    if (!indexPositions) return false;
                                } else {//point
                                    indexPositions = dommy::viewport::acquireFloatBuffer(
                                        indexPositionBuffer, vertexBufferDescriptor, dData.cvsBuffer.size(), 3);
                                    if (!indexPositions) return false;
                                }

                            }
                        }
                    }
                    else if (vertexBufferDescriptor.name() == DCMarkVecItemName_)// vec
                    {
                        if (!vecPosBuffer)
                        {
                            vecPosBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vecPosBuffer) return false;
                            if (vecPosBuffer)
                            {
                                if (dData.infoClass==0) {//primitive

                                    vecPosPoints = dommy::viewport::acquireFloatBuffer(
                                        vecPosBuffer, vertexBufferDescriptor,
                                        dData.crvFirstIndex.size() * 3, 3);
                                    if (!vecPosPoints) return false;
                                } else {//point

                                    vecPosPoints = dommy::viewport::acquireFloatBuffer(
                                        vecPosBuffer, vertexBufferDescriptor, dData.cvsBuffer.size(), 3);
                                    if (!vecPosPoints) return false;
                                }

                            }
                        }
                    } else if (vertexBufferDescriptor.name() == DCMarkVal3ItemName_ ||
                              vertexBufferDescriptor.name() == DCMarkVal2ItemName_ ||
                              vertexBufferDescriptor.name() == DCMarkVal1ItemName_
                              )// for val
                    {
                        if (!vPosBuffer)
                        {
                            vPosBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vPosBuffer) return false;
                            if (vPosBuffer)
                            {
                                if(!vPosPoints){
                                if (dData.infoClass==0) {//primitive

                                    vPosPoints = dommy::viewport::acquireFloatBuffer(
                                        vPosBuffer, vertexBufferDescriptor,
                                        dData.crvFirstIndex.size() * 3, 3);
                                    if (!vPosPoints) return false;
                                } else {//point

                                    vPosPoints = dommy::viewport::acquireFloatBuffer(
                                        vPosBuffer, vertexBufferDescriptor, dData.cvsBuffer.size(), 3);
                                    if (!vPosPoints) return false;
                                }
                                }
                            }
                        }
                    } else if (vertexBufferDescriptor.name() == DCMarkGroupLineItemName_ ||
                               vertexBufferDescriptor.name() == DCMarkGroupPointItemName_) // group
                    {
                        if (!grpVerticesBuffer) {
                            grpVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!grpVerticesBuffer) return false;
                            if (grpVerticesBuffer) {

                                grpVertices = dommy::viewport::acquireFloatBuffer(
                                    grpVerticesBuffer, vertexBufferDescriptor,
                                    dData.grpPtBuffer.size(), 3);
                                if (!grpVertices) return false;
                            }
                        }
                    }

                } break;
                case MHWRender::MGeometry::kTexture: {

                    // Index
                    if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                        (vertexBufferDescriptor.name() == DCMarkIndexItemName_)) {

                        if (!indexNumericBuffer) {
                            indexNumericBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!indexNumericBuffer) return false;
                            if (indexNumericBuffer) {
                                if (dData.indexType == 0) { // primitive
                                    indexNumerics = dommy::viewport::acquireFloatBuffer(
                                        indexNumericBuffer, vertexBufferDescriptor,
                                        dData.crvFirstIndex.size(), 1);
                                    if (!indexNumerics) return false;
                                } else { // point
                                    indexNumerics = dommy::viewport::acquireFloatBuffer(
                                        indexNumericBuffer, vertexBufferDescriptor, dData.pointCount, 1);
                                    if (!indexNumerics) return false;
                                }
                            }
                        }
                    }
                    // Vector
                    else if ((vertexBufferDescriptor.name() == DCMarkVecItemName_)) {
                        if (!vecBuffer) {
                            vecBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vecBuffer) return false;
                            if (vecBuffer) {
                                vectorvalues = dommy::viewport::acquireFloatBuffer(
                                    vecBuffer, vertexBufferDescriptor, dData.vecBuffer.size(), 3);
                                if (!vectorvalues) return false;
                            }
                        }
                    }
                    // Value 3
                    else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numeric3Value) &&
                             (vertexBufferDescriptor.name() == DCMarkVal3ItemName_ ||
                              vertexBufferDescriptor.name() == DCMarkVal2ItemName_ ||
                              vertexBufferDescriptor.name() == DCMarkVal1ItemName_)) {

                        if (!val3Buffer) {
                            val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!val3Buffer) return false;
                            if (val3Buffer) {
                                val3values = dommy::viewport::acquireFloatBuffer(
                                    val3Buffer, vertexBufferDescriptor, dData.val3Buffer.size(), 3);
                                if (!val3values) return false;
                            }
                        }

                    }
                    // Value3 or Value2
                    // numericなぜ必要なのかなぞとりあえず何かしら値を与えると正常に動作するのでバッファを生成
                    else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                             (vertexBufferDescriptor.name() == DCMarkVal3ItemName_ ||
                              vertexBufferDescriptor.name() == DCMarkVal2ItemName_)) {
                        if (!tmpNumBuffer) {
                            tmpNumBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!tmpNumBuffer) return false;
                            if (tmpNumBuffer) {

                                size_t num = 1;

                                tmpnumvalues = dommy::viewport::acquireFloatBuffer(
                                    tmpNumBuffer, vertexBufferDescriptor, num, 1);
                                if (!tmpnumvalues) return false;
                            }
                        }
                    }
                    // Value2
                    else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numeric2Value) &&
                             (vertexBufferDescriptor.name() == DCMarkVal2ItemName_)) {
                        if (dData.valType == DP_VAL_INT2 || dData.valType == DP_VAL_FLOAT2) {
                            if (!val3Buffer) {
                                val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                                if (!val3Buffer) return false;
                                if (val3Buffer) {
                                    val3values = dommy::viewport::acquireFloatBuffer(
                                        val3Buffer, vertexBufferDescriptor, dData.val3Buffer.size(), 2);
                                    if (!val3values) return false;
                                }
                            }
                        }
                    }
                    // Value1
                    else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                             (vertexBufferDescriptor.name() == DCMarkVal1ItemName_)) {
                        if (dData.valType == DP_VAL_INT || dData.valType == DP_VAL_FLOAT) {
                            if (!val3Buffer) {
                                val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                                if (!val3Buffer) return false;
                                if (val3Buffer) {
                                    val3values = dommy::viewport::acquireFloatBuffer(
                                        val3Buffer, vertexBufferDescriptor, dData.val3Buffer.size(), 1);
                                    if (!val3values) return false;
                                }
                            }
                        }
                    }
                } break;
                case MHWRender::MGeometry::kColor: {

                    if ((vertexBufferDescriptor.name() == DCMarkColorItemName_)) {
                        if (!colBuffer && dData.colBuffer.size()) {
                            colBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!colBuffer) return false;
                            if (colBuffer) {
                                colorvalues = dommy::viewport::acquireFloatBuffer(
                                    colBuffer, vertexBufferDescriptor, dData.colBuffer.size(), 4);
                                if (!colorvalues) return false;
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

    if ((verticesBuffer && !vertices) || (centerPositionBuffer && !centerPositions) ||
        (indexPositionBuffer && !indexPositions) || (indexNumericBuffer && !indexNumerics) ||
        (colBuffer && !colorvalues) || (vecBuffer && !vectorvalues) ||
        (val3Buffer && !val3values) || (tmpNumBuffer && !tmpnumvalues) ||
        (vPosBuffer && !vPosPoints) || (vecPosBuffer && !vecPosPoints) ||
        (grpVerticesBuffer && !grpVertices)) {
        return;
    }

#pragma mark commit position

    if (dData.cvsBuffer.size()) {

        if (vertices) {
            std::copy(dData.cvsBuffer.begin(), dData.cvsBuffer.end(), vertices);
        }

        if (verticesBuffer && vertices) {
            verticesBuffer->commit(vertices);
        }

        if (vPosPoints) {
            if (dData.infoClass == 0) { // primitive

                size_t indexOffset = 0;
                for (std::size_t i = 0; i < dData.crvFirstIndex.size(); i++) {
                    vPosPoints[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i]];
                    vPosPoints[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i] + 1];
                    vPosPoints[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i] + 2];
                }
            } else { // point

                std::copy(dData.cvsBuffer.begin(), dData.cvsBuffer.end(), vPosPoints);
            }
        }

        if (vPosBuffer && vPosPoints) {
            vPosBuffer->commit(vPosPoints);
        }

        if (vecPosPoints) {
            if (dData.infoClass == 0) { // primitive
                size_t indexOffset = 0;
                for (std::size_t i = 0; i < dData.crvFirstIndex.size(); i++) {
                    vecPosPoints[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i]];
                    vecPosPoints[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i] + 1];
                    vecPosPoints[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i] + 2];
                }
            } else { // point
                std::copy(dData.cvsBuffer.begin(), dData.cvsBuffer.end(), vecPosPoints);
            }
        }

        if (vecPosBuffer && vecPosPoints) {
            vecPosBuffer->commit(vecPosPoints);
        }
    }
#pragma mark commit center

    if (dData.isCenterVis && dData.centerPtBuffer.size()) {
        if (centerPositionBuffer && centerPositions) {

            size_t indexOffset = 0;
            for (size_t i = 0; i < dData.primCount; i++) {
                const std::size_t sourceOffset = i * 3U;
                centerPositions[indexOffset++] = dData.centerPtBuffer[sourceOffset];
                centerPositions[indexOffset++] = dData.centerPtBuffer[sourceOffset + 1U];
                centerPositions[indexOffset++] = dData.centerPtBuffer[sourceOffset + 2U];
            }
        }

        if (centerPositionBuffer && centerPositions) {
            centerPositionBuffer->commit(centerPositions);
        }
    }

#pragma mark commit index

    if (dData.showIndex && dData.cvsBuffer.size()) {
        if (indexPositionBuffer && indexPositions) {
            if (dData.indexType == 0) { // primitive
                size_t indexOffset = 0;
                for (std::size_t i = 0; i < dData.crvFirstIndex.size(); i++) {
                    indexPositions[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i]];
                    indexPositions[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i] + 1];
                    indexPositions[indexOffset++] = dData.cvsBuffer[dData.crvFirstIndex[i] + 2];
                }
            } else { // point

                std::copy(dData.cvsBuffer.begin(), dData.cvsBuffer.end(), indexPositions);
            }
        }

        if (indexPositionBuffer && indexPositions) {
            indexPositionBuffer->commit(indexPositions);
        }

#pragma mark numeric index texture

        if (indexNumericBuffer && indexNumerics) {
            if (dData.indexType == 0) { // primitive
                for (std::size_t i = 0, e = dData.crvFirstIndex.size(); i < e; i++) {
                    indexNumerics[i] = static_cast<float>(i);
                }
            } else { // point
                for (std::size_t i = 0; i < dData.pointCount; i++) {
                    indexNumerics[i] = static_cast<float>(i);
                }
            }
        }

        if (indexNumericBuffer && indexNumerics) {
            indexNumericBuffer->commit(indexNumerics);
        }
    }

#pragma mark color shder commit

    if (dData.colBuffer.size()) {
        if (colorvalues) {
            std::copy(dData.colBuffer.begin(), dData.colBuffer.end(), colorvalues);
        }
        if (colBuffer && colorvalues) {
            colBuffer->commit(colorvalues);
        }
    }

#pragma mark vector shder commit

    if (dData.vecBuffer.size()) {
        if (vectorvalues) {
            std::copy(dData.vecBuffer.begin(), dData.vecBuffer.end(), vectorvalues);
        }
        if (vecBuffer && vectorvalues) {
            vecBuffer->commit(vectorvalues);
        }
    }

#pragma mark value3 shder commit

    if (dData.val3Buffer.size()) {
        if (val3values) {
            std::copy(dData.val3Buffer.begin(), dData.val3Buffer.end(), val3values);
        }
        if (val3Buffer && val3values) {
            val3Buffer->commit(val3values);
        }

        if (tmpnumvalues) {
            tmpnumvalues[0] = 0.0f;
        }
        if (tmpNumBuffer && tmpnumvalues) {
            tmpNumBuffer->commit(tmpnumvalues);
        }
    }

#pragma mark group shader vertex commit

    if (dData.isGroup && grpVertices && grpVerticesBuffer) {

        size_t indexOffset = 0;
        for (auto vIt = dData.grpPtBuffer.begin(), eIt = dData.grpPtBuffer.end(); vIt != eIt;
             ++vIt) {
            grpVertices[indexOffset++] = (*vIt);
        }

        grpVerticesBuffer->commit(grpVertices);
    }

    for (int idx = 0, edx = renderItems.length(); idx < edx; ++idx) {
        const MHWRender::MRenderItem* item = renderItems.itemAt(idx);

        if (!item)
            continue;

        std::size_t numIndex = 0U;

        if (item->name() == DCMarkColorItemName_) {
            if (dData.indexBuffer.size()) {
                numIndex = dData.indexBuffer.size();
                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                size_t c = 0;
                for (auto it = dData.indexBuffer.begin(), eIt = dData.indexBuffer.end(); it != eIt;
                     ++it) {
                    indices[c++] = (*it);
                }

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        } else if (item->name() == DCMarkCenterItemName_) {
            if (dData.isCenterVis && dData.centerPtBuffer.size()) {
                numIndex = dData.primCount;

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

        } else if (item->name() == DCMarkIndexItemName_) {
            if (dData.showIndex && dData.cvsBuffer.size()) {
                if (dData.indexType == 0) { // prim
                    numIndex = dData.crvFirstIndex.size();
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
        } else if (item->name() == DCMarkVecItemName_ || item->name() == DCMarkVal3ItemName_ ||
                   item->name() == DCMarkVal2ItemName_ || item->name() == DCMarkVal1ItemName_) {
            if (dData.pointCount || dData.crvFirstIndex.size()) {
                if (dData.infoClass == 0) { // prim
                    numIndex = dData.crvFirstIndex.size();
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
        } else if (item->name() == DCMarkGroupLineItemName_ ||
                   item->name() == DCMarkGroupPointItemName_) // group
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
DOMMY_VIEWPORT_VOID_CATCH("DCMarkerOverride", "populateGeometry")
