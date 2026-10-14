//
//  DPMarker.cpp
//  dommyTool
//
//  Created by yuzo on 2018/01/18.
//

#include "DPMarker.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DViewportGeometryBudget.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DNodeOperation.h"
#include "DColorOperation.h"
#include "DStringOp.h"

#include <cmath>
#include <numeric>

MTypeId DPMarker::typeId( ID_DP_MARKER );
const MString DPMarker::typeName("DPMarker");
MString DPMarker::classification("drawdb/geometry/DPMarker");
MString DPMarker::registrantId("DPMarkerPlugin");





MObject DPMarker::inDP;


MObject DPMarker::aShowIndex;

MObject DPMarker::aPointSize;
MObject DPMarker::aSolidColor;

MObject DPMarker::aNodeInfo;



//attribute
MObject DPMarker::aAllAttributeNames;
MObject DPMarker::aIsColor;
MObject DPMarker::aIsValue;
MObject DPMarker::aIsVector;

MObject DPMarker::aVisibleAttributeName;

MObject DPMarker::aIsBlackTransparency;
MObject DPMarker::aIsDrawHeads;
MObject DPMarker::aVelocityScale;

MObject DPMarker::aAllGroupNames;
MObject DPMarker::aIsGroup;
MObject DPMarker::aGroupName;
MObject DPMarker::aGroupColor;
MObject DPMarker::aGroupTransparency;

DPMarker::DPMarker() {}
DPMarker::~DPMarker() {}

void* DPMarker::creator()
{
    return ::dommy::maya::createObject("DPMarker", [&]() {
        return new DPMarker();
    });
}



MStatus DPMarker::compute( const MPlug& plug, MDataBlock& data ) try
{

    
    
    if(plug == aNodeInfo || plug == aAllAttributeNames || plug == aAllGroupNames) {

        MPlug dpPlug(thisMObject(), inDP);

        MStatus outputStatus;
        const bool connected = dpPlug.isConnected(&outputStatus);
        if (!outputStatus) return outputStatus;
        const DPData* dp = NULL;
        if (connected) {
            outputStatus = getValidInputDP(inDP, data, dp);
            if (!outputStatus) return outputStatus;
            if (!dp) return MS::kFailure;
        }
        if (plug == aAllAttributeNames || plug == aAllGroupNames) {
            DStringList names;
            if (dp) {
                names = plug == aAllAttributeNames
                    ? dp->getAllAttributeList() : dp->getAllGroupList();
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

        if (connected) {
            if (dp) {

                size_t count = dp->numberOfPoints();

                // position
                infoStr << "DPoint Info\n";

                infoStr << "    point count: " << count << "\n";
                
                
                //attribute
                infoStr << "    attribute count: " << dp->numberOfAttributes() << "\n";
                
                for(auto aIt = dp->beginAttr(), eaIt=dp->endAttr(); aIt!=eaIt; ++aIt) {
                    infoStr << "        @" << aIt->first << " : " << aIt->second->typeName() <<  "\n";
                }
                
                //group
                infoStr << "    group count: " << dp->numberOfGroups() << "\n";
                
                for(auto gIt = dp->beginGrp(), egIt=dp->endGrp(); gIt!=egIt; ++gIt) {
                    infoStr << "        *" << gIt->first << " : " << gIt->second->numberOfIds() <<  "\n";
                }
            }

        } else {
            infoStr << "DPoint Info\n";
            infoStr << "DPoint is not connected.";
        }

#pragma mark Create output
        MString str = dommy::text::checkedStreamText(infoStr, "DPMarker info formatting failed").c_str();
        MDataHandle strHandle = data.outputValue(aNodeInfo, &outputStatus);
        if (!outputStatus) return outputStatus;
        strHandle.set(str);
        
        return data.setClean(plug);
        
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPMarker")



void DPMarker::draw( M3dView & view, const MDagPath & path,
                         M3dView::DisplayStyle style,
                         M3dView::DisplayStatus status )
{
    
}



bool DPMarker::isBounded() const
{
    return true;
}

MBoundingBox DPMarker::boundingBox() const try
{
    

    MPlug dpPlug(thisMObject(), inDP);
    
    MStatus connectionStatus;
    const bool connected = dpPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DPMarker: unable to query input connection for bounds.");
    } else if (connected) {
        
        MObject obj;
        const DPData* dp =  getInputDPfromPlug(dpPlug, obj);
        
        if(dp) {
            const MBoundingBox bbox = dp->getBoundingBox();
            if (dommy::maya::isFiniteBoundingBox(bbox)) {
                return bbox;
            }
        }
    }

    MPoint corner1( -1.0f, -1.0f, -1.0f );
    MPoint corner2( 1.0f, 1.0f, 1.0f );
    
    return MBoundingBox( corner1, corner2 );
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH_WITH_HALF_EXTENT("DPMarker", 1.0)



MStatus DPMarker::initialize()
try {
    MStatus                stat;
    MFnNumericAttribute    nAttr;
    MFnTypedAttribute    tAttr;
    MFnEnumAttribute eAttr;
    
    
    MFnStringData fnStringData;
    
    
    
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    aShowIndex = nAttr.create("ShowIndex", "sinx", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aShowIndex));
    
  
    
    aPointSize = nAttr.create("PointSize", "ptsz", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointSize));
    
    
    aSolidColor = nAttr.create("SolidColor", "scol", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 1.0f, 1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolidColor));
    
    
    const MObject aNodeInfoDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, aNodeInfoDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setAffectsAppearance(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNodeInfo));
    
    
    
#pragma mark attribute
    
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
    
    const MObject aVisibleAttributeNameDefaultData = fnStringData.create("Cd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVisibleAttributeName = tAttr.create("VisibleAttributeName", "visatn", MFnData::kString, aVisibleAttributeNameDefaultData, &stat);
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
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupColor));
    
    aGroupTransparency = nAttr.create("GroupTransparency", "grptrans", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupTransparency));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aAllGroupNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aAllAttributeNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aNodeInfo));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPMarker", "initialize", std::current_exception());
}




DPMarkerData::DPMarkerData() : MUserData(false)
, showIndex(false)
, fColor(1.0f, 0.0f, 1.0f, 1.0f)
, fLineWidth(3.0f)
, fPointSize(2.0f)
, isShowVal(false)
, isShowCol(false)
, isShowVec(false)
, useGrp(false)
{
}


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------




DPMarkerOverride::DPMarkerOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mIndexUIShader(NULL)
, mColorUIShader(NULL)
, mVectorUIShader(NULL)
, mValueUIShader(NULL)
, mValue2UIShader(NULL)
, mValue3UIShader(NULL)
, mGroupPointShader(NULL)
, mDGNode(obj)
, mMultiplierChanged(true)
{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DPMarkerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVFatPointShader, mColorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dIntegerNumericShader, mIndexUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloatNumericShader, mValueUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat2NumericShader, mValue2UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat3NumericShader, mValue3UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mGroupPointShader);
}


DPMarkerOverride::~DPMarkerOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DPMarkerOverride");
    shaderRelease.release(mIndexUIShader);
    shaderRelease.release(mColorUIShader);
    shaderRelease.release(mVectorUIShader);
    shaderRelease.release(mValueUIShader);
    shaderRelease.release(mValue2UIShader);
    shaderRelease.release(mValue3UIShader);
    shaderRelease.release(mGroupPointShader);
}


MHWRender::DrawAPI DPMarkerOverride::supportedDrawAPIs() const
{
    return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}


static const MString DPMarkItemName_               = "DPMarkItem";
static const MString DPMarkIndexItemName_          = "DPMarkIndexItem";

static const MString DPMarkColItemName_               = "DPMarkColItem";
static const MString DPMarkVecItemName_               = "DPMarkVecItem";
static const MString DPMarkVal1ItemName_               = "DPMarkVal1Item";
static const MString DPMarkVal2ItemName_               = "DPMarkVal2Item";
static const MString DPMarkVal3ItemName_               = "DPMarkVal3Item";

static const MString DPMarkGroupPointItemName_              = "DPMarkGroupPointItem";

template <typename T>
inline void setSingleValueColor(const DPData& inputData, const std::string& name,
                                const bool& isBlackTrans, std::vector<float>& finValues) {
    SingleToColorFloat(inputData.attrListValue<T>(name), isBlackTrans, finValues);
}

template <typename T>
inline void set1DVector(const DPData& inputData, const std::string& name,
                        std::vector<float>& finValues) {
    SingleToVectorFloat(inputData.attrListValue<T>(name), finValues);
}

template <typename T>
inline void setSimpleVectorValue(const DPData& inputData, const std::string& name,
                                 std::vector<float>& finValues) {
    VectorToVectorFloat(inputData.attrListValue<T>(name), finValues);
}

template <typename T>
inline void setTwoVectorValue(const DPData& inputData, const std::string& name,
                              std::vector<float>& finValues) {
    TwoVectorToValueFloat(inputData.attrListValue<T>(name), finValues);
}

template <typename T>
inline void set2To3DVectorValue(const DPData& inputData, const std::string& name,
                                std::vector<float>& finValues) {
    TwoVectorTo3DVectorFloat(inputData.attrListValue<T>(name), finValues);
}

template <typename T>
inline void setTwoVecColor(const DPData& inputData, const std::string& name, bool isBlackTrans,
                           std::vector<float>& finValues) {
    TwoVectorToColorFloat(inputData.attrListValue<T>(name), isBlackTrans, finValues);
}

template <typename T>
inline void setThreeVecColor(const DPData& inputData, const std::string& name, bool isBlackTrans,
                             std::vector<float>& finValues) {
    VectorToColorFloat(inputData.attrListValue<T>(name), isBlackTrans, finValues);
}

template <typename T>
inline void setSimpleSingleValue(const DPData& inputData, const std::string& name,
                                 std::vector<float>& finValues) {
    ValueToValueFloat(inputData.attrListValue<T>(name), finValues);
}

namespace {

void clearPointMarkerGeometry(DPMarkerOverride::DrawData& data) noexcept {
    data.ptBuffer.clear();
    data.colBuffer.clear();
    data.vecBuffer.clear();
    data.val3Buffer.clear();
    data.grpPtBuffer.clear();
    data.pointCount = 0U;
    data.totalGroupPointCount = 0U;
    data.valType = DP_VAL_FLOAT;
}

} // namespace

void DPMarkerOverride::updateDG() try {
    mMultiplierChanged = true;
    clearPointMarkerGeometry(dData);

    // common attributes
    // retrieve color
    if (readPlugValue(mDGNode, DPMarker::aSolidColor, dData.solidColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.solidColor)) {
        dData.solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (readPlugValue(mDGNode, DPMarker::aShowIndex, dData.showIndex) != MS::kSuccess) return;
    double pointSize = 3.0;
    if (readPlugValue(mDGNode, DPMarker::aPointSize, pointSize) != MS::kSuccess ||
        !std::isfinite(pointSize) || pointSize < 0.0) {
        pointSize = 3.0;
    }
    dData.pointSize = static_cast<float>(pointSize);

    if (readPlugValue(mDGNode, DPMarker::aIsDrawHeads, dData.isDrawHeads) != MS::kSuccess ||
        readPlugValue(mDGNode, DPMarker::aIsBlackTransparency, dData.isBlackTransparency) != MS::kSuccess) return;
    double velocityScale = 1.0;
    if (readPlugValue(mDGNode, DPMarker::aVelocityScale, velocityScale) != MS::kSuccess ||
        !std::isfinite(velocityScale) || velocityScale < 0.0) {
        velocityScale = 1.0;
    }
    dData.velocityScale = static_cast<float>(velocityScale);

    if (readPlugValue(mDGNode, DPMarker::aIsColor, dData.isColor) != MS::kSuccess ||
        readPlugValue(mDGNode, DPMarker::aIsValue, dData.isValue) != MS::kSuccess ||
        readPlugValue(mDGNode, DPMarker::aIsVector, dData.isVector) != MS::kSuccess) return;

    bool isShowAttr = dData.isVector || dData.isColor || dData.isValue;
    MString visibleAttribute;
    if (readPlugValue(mDGNode, DPMarker::aVisibleAttributeName, visibleAttribute) != MS::kSuccess) return;
    const std::string visAttrStr(visibleAttribute.asChar());

    // group
    if (readPlugValue(mDGNode, DPMarker::aIsGroup, dData.isGroup) != MS::kSuccess) return;

    MString groupName;
    if (readPlugValue(mDGNode, DPMarker::aGroupName, groupName) != MS::kSuccess) return;
    const std::string grpNameStr(groupName.asChar());
    if (readPlugValue(mDGNode, DPMarker::aGroupColor, dData.groupColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.groupColor)) {
        dData.groupColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    double groupAlpha = 1.0;
    if (readPlugValue(mDGNode, DPMarker::aGroupTransparency, groupAlpha) != MS::kSuccess ||
        !std::isfinite(groupAlpha)) {
        groupAlpha = 1.0;
    }
    dData.groupAlpha = static_cast<float>(std::max(0.0, std::min(1.0, groupAlpha)));

    MPlug dpPlug(mDGNode, DPMarker::inDP);
    MStatus connectionStatus;
    const bool connected = dpPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DPMarker: unable to query input connection for drawing.");
        return;
    }
    if (connected) {

        MObject obj;
        const DPData* inputDPData = NULL;
        const MStatus dataStatus =
            dommy::maya::getPluginDataFromPlug(dpPlug, DPData::id, obj, inputDPData);
        if (!dataStatus) {
            MGlobal::displayError("DPMarker: connected input is not valid DPData.");
            return;
        }

        if (inputDPData) {

#pragma mark attribute check
            if (isShowAttr) {
                if (!inputDPData->isExistAttr(visAttrStr)) {
                    isShowAttr = dData.isColor = dData.isValue = dData.isVector = false;
                }
            } // check visible attribute
#pragma mark group check
            if (dData.isGroup) {
                if (!inputDPData->isExistGrp(grpNameStr)) {
                    dData.isGroup = false;
                }
            } // check visible group

            // get point count
            size_t count = inputDPData->numberOfPoints();

#pragma mark ポイントデータの作成
            if (count) {
                size_t scalarCount = 0;
                if (count > dommy::viewport::kMaxViewportVertexCount ||
                    !dommy::geometry::checkedTupleScalarCount(count, 3, scalarCount) ||
                    scalarCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DPMarker: point buffer exceeds the viewport budget.");
                    return;
                }
                dData.ptBuffer.resize(scalarCount);

                const DPointList* pts = inputDPData->getPositionPtrConst();
                if (!pts || pts->size() != count) {
                    MGlobal::displayError("DPMarker: invalid point storage.");
                    return;
                }

                tbb::blocked_range<size_t> range =
                    tbb::blocked_range<size_t>(0, count, DGetGlobalProcessNum(count));
                tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& r) {
                    for (size_t i = r.begin(); i != r.end(); i++) {
                        size_t index = i * 3;
                        dData.ptBuffer[index] = (*pts)[i][0];
                        dData.ptBuffer[index + 1] = (*pts)[i][1];
                        dData.ptBuffer[index + 2] = (*pts)[i][2];
                    }
                });
                dData.pointCount = count;
            }

#pragma mark Attribute Dataの作成
            // Check point attribute exist
            if (isShowAttr) {
                const DPAttributeData::ConstPtr visibleAttribute = inputDPData->getAttr(visAttrStr);
                if (!visibleAttribute || visibleAttribute->attrSize() != count) {
                    MGlobal::displayError(
                        "DPMarker: visible attribute count does not match the point count.");
                    dData.isColor = false;
                    dData.isValue = false;
                    dData.isVector = false;
                    isShowAttr = false;
                }
            }

            if (isShowAttr) {
                const DP_ATTR_DATA_TYPE type = inputDPData->getAttr(visAttrStr)->getAttrType();

                if (dData.isColor) {

                    if (isSingleValueAttrType(type)) {

                        dData.valType = DP_VAL_INT;
                        if (type == DP_ATTR_INT) {
                            setSingleValueColor<int32_t>(*inputDPData, visAttrStr,
                                                         dData.isBlackTransparency,
                                                         dData.colBuffer);
                        } else if (type == DP_ATTR_BOOL) {
                            setSingleValueColor<bool>(*inputDPData, visAttrStr,
                                                      dData.isBlackTransparency, dData.colBuffer);
                        } else if (type == DP_ATTR_UINT32) {
                            setSingleValueColor<uint32_t>(*inputDPData, visAttrStr,
                                                          dData.isBlackTransparency,
                                                          dData.colBuffer);
                        } else if (type == DP_ATTR_INT64) {
                            setSingleValueColor<int64_t>(*inputDPData, visAttrStr,
                                                         dData.isBlackTransparency,
                                                         dData.colBuffer);
                        } else if (type == DP_ATTR_FLOAT) {
                            dData.valType = DP_VAL_FLOAT;
                            setSingleValueColor<float>(*inputDPData, visAttrStr,
                                                       dData.isBlackTransparency, dData.colBuffer);
                        } else if (type == DP_ATTR_DOUBLE) {
                            dData.valType = DP_VAL_FLOAT;
                            setSingleValueColor<double>(*inputDPData, visAttrStr,
                                                        dData.isBlackTransparency, dData.colBuffer);
                        }

                    } else if (isVector2DAttrType(type)) {

                        if (type == DP_ATTR_VEC2I) {
                            dData.valType = DP_VAL_INT2;
                            setTwoVecColor<Imath::V2i>(*inputDPData, visAttrStr,
                                                       dData.isBlackTransparency, dData.colBuffer);
                        } else if (type == DP_ATTR_VEC2F) {
                            dData.valType = DP_VAL_FLOAT2;
                            setTwoVecColor<Imath::V2f>(*inputDPData, visAttrStr,
                                                       dData.isBlackTransparency, dData.colBuffer);
                        } else if (type == DP_ATTR_VEC2D) {
                            dData.valType = DP_VAL_FLOAT2;
                            setTwoVecColor<Imath::V2d>(*inputDPData, visAttrStr,
                                                       dData.isBlackTransparency, dData.colBuffer);
                        }

                    } else if (isVector3DAttrType(type)) {

                        if (type == DP_ATTR_VEC3I) {
                            dData.valType = DP_VAL_INT3;
                            setThreeVecColor<Imath::V3i>(*inputDPData, visAttrStr,
                                                         dData.isBlackTransparency,
                                                         dData.colBuffer);
                        } else if (type == DP_ATTR_VEC3F) {
                            dData.valType = DP_VAL_FLOAT3;
                            setThreeVecColor<Imath::V3f>(*inputDPData, visAttrStr,
                                                         dData.isBlackTransparency,
                                                         dData.colBuffer);
                        } else if (type == DP_ATTR_VEC3D) {
                            dData.valType = DP_VAL_FLOAT3;
                            setThreeVecColor<Imath::V3d>(*inputDPData, visAttrStr,
                                                         dData.isBlackTransparency,
                                                         dData.colBuffer);
                        }
                    }

                } // color attribute

                if (dData.isVector) {

                    // Vector
                    if (isSingleValueAttrType(type)) {
                        dData.valType = DP_VAL_INT;
                        if (type == DP_ATTR_INT) {
                            set1DVector<int32_t>(*inputDPData, visAttrStr, dData.vecBuffer);
                        } else if (type == DP_ATTR_BOOL) {
                            set1DVector<bool>(*inputDPData, visAttrStr, dData.vecBuffer);
                        } else if (type == DP_ATTR_UINT32) {
                            set1DVector<uint32_t>(*inputDPData, visAttrStr, dData.vecBuffer);
                        } else if (type == DP_ATTR_INT64) {
                            set1DVector<int64_t>(*inputDPData, visAttrStr, dData.vecBuffer);
                        } else if (type == DP_ATTR_FLOAT) {
                            dData.valType = DP_VAL_FLOAT;
                            set1DVector<float>(*inputDPData, visAttrStr, dData.vecBuffer);
                        } else if (type == DP_ATTR_DOUBLE) {
                            dData.valType = DP_VAL_FLOAT;
                            set1DVector<double>(*inputDPData, visAttrStr, dData.vecBuffer);
                        }

                    } else if (isVector2DAttrType(type)) {

                        if (type == DP_ATTR_VEC2I) {
                            dData.valType = DP_VAL_INT2;
                            set2To3DVectorValue<Imath::V2i>(*inputDPData, visAttrStr,
                                                            dData.vecBuffer);
                        } else if (type == DP_ATTR_VEC2F) {
                            dData.valType = DP_VAL_FLOAT2;
                            set2To3DVectorValue<Imath::V2f>(*inputDPData, visAttrStr,
                                                            dData.vecBuffer);
                        } else if (type == DP_ATTR_VEC2D) {
                            dData.valType = DP_VAL_FLOAT2;
                            set2To3DVectorValue<Imath::V2d>(*inputDPData, visAttrStr,
                                                            dData.vecBuffer);
                        }

                    } else if (isVector3DAttrType(type)) {

                        if (type == DP_ATTR_VEC3I) {
                            dData.valType = DP_VAL_INT3;
                            setSimpleVectorValue<Imath::V3i>(*inputDPData, visAttrStr,
                                                             dData.vecBuffer);
                        } else if (type == DP_ATTR_VEC3F) {
                            dData.valType = DP_VAL_FLOAT3;
                            setSimpleVectorValue<Imath::V3f>(*inputDPData, visAttrStr,
                                                             dData.vecBuffer);
                        } else if (type == DP_ATTR_VEC3D) {
                            dData.valType = DP_VAL_FLOAT3;
                            setSimpleVectorValue<Imath::V3d>(*inputDPData, visAttrStr,
                                                             dData.vecBuffer);
                        }
                    }

                } // vector attrbute

                if (dData.isValue) {

                    // Values
                    if (isSingleValueAttrType(type)) {

                        dData.valType = DP_VAL_INT;

                        if (type == DP_ATTR_INT) {
                            setSimpleSingleValue<int32_t>(*inputDPData, visAttrStr,
                                                          dData.val3Buffer);
                        } else if (type == DP_ATTR_BOOL) {
                            setSimpleSingleValue<bool>(*inputDPData, visAttrStr, dData.val3Buffer);
                        } else if (type == DP_ATTR_UINT32) {
                            setSimpleSingleValue<uint32_t>(*inputDPData, visAttrStr,
                                                           dData.val3Buffer);
                        } else if (type == DP_ATTR_INT64) {
                            setSimpleSingleValue<int64_t>(*inputDPData, visAttrStr,
                                                          dData.val3Buffer);

                        } else if (type == DP_ATTR_FLOAT) {

                            dData.valType = DP_VAL_FLOAT;
                            setSimpleSingleValue<float>(*inputDPData, visAttrStr, dData.val3Buffer);

                        } else if (type == DP_ATTR_DOUBLE) {

                            dData.valType = DP_VAL_FLOAT;
                            setSimpleSingleValue<double>(*inputDPData, visAttrStr,
                                                         dData.val3Buffer);
                        }

                    } else if (isVector2DAttrType(type)) {

                        if (type == DP_ATTR_VEC2I) {
                            dData.valType = DP_VAL_INT2;
                            setTwoVectorValue<Imath::V2i>(*inputDPData, visAttrStr,
                                                          dData.val3Buffer);
                        } else if (type == DP_ATTR_VEC2F) {
                            dData.valType = DP_VAL_FLOAT2;
                            setTwoVectorValue<Imath::V2f>(*inputDPData, visAttrStr,
                                                          dData.val3Buffer);
                        } else if (type == DP_ATTR_VEC2D) {
                            dData.valType = DP_VAL_FLOAT2;
                            setTwoVectorValue<Imath::V2d>(*inputDPData, visAttrStr,
                                                          dData.val3Buffer);
                        }

                    } else if (isVector3DAttrType(type)) {

                        if (type == DP_ATTR_VEC3I) {
                            dData.valType = DP_VAL_INT3;
                            setSimpleVectorValue<Imath::V3i>(*inputDPData, visAttrStr,
                                                             dData.val3Buffer);
                        } else if (type == DP_ATTR_VEC3F) {
                            dData.valType = DP_VAL_FLOAT3;
                            setSimpleVectorValue<Imath::V3f>(*inputDPData, visAttrStr,
                                                             dData.val3Buffer);
                        } else if (type == DP_ATTR_VEC3D) {
                            dData.valType = DP_VAL_FLOAT3;
                            setSimpleVectorValue<Imath::V3d>(*inputDPData, visAttrStr,
                                                             dData.val3Buffer);
                        }
                    }

                } // value attribute

            } // check attribute

            if (dData.isColor) {
                std::size_t expectedColorCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(count, 4U, expectedColorCount) ||
                    expectedColorCount != dData.colBuffer.size() ||
                    expectedColorCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DPMarker: color attribute layout is inconsistent.");
                    dData.colBuffer.clear();
                    dData.isColor = false;
                }
            }

            if (dData.isVector) {
                std::size_t expectedVectorCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(count, 3U, expectedVectorCount) ||
                    expectedVectorCount != dData.vecBuffer.size() ||
                    expectedVectorCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DPMarker: vector attribute layout is inconsistent.");
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
                    !dommy::geometry::checkedTupleScalarCount(count, valueTupleSize,
                                                              expectedValueCount) ||
                    expectedValueCount != dData.val3Buffer.size() ||
                    expectedValueCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DPMarker: value attribute layout is inconsistent.");
                    dData.val3Buffer.clear();
                    dData.isValue = false;
                }
            }

#pragma mark group

            if (dData.isGroup) {
                if (inputDPData->isExistGrp(grpNameStr)) {

                    DPGroupData::ConstPtr grpdata = inputDPData->getGrp(grpNameStr);
                    if (!grpdata) {
                        MGlobal::displayError("DPMarker: group data is unavailable.");
                        dData.isGroup = false;
                        return;
                    }
                    DPIdList grpIdsList = grpdata->getGroupIds();

                    if (grpIdsList.size() == count) {

                        const std::size_t num_grouped = static_cast<std::size_t>(
                            std::count(grpIdsList.begin(), grpIdsList.end(), 1));
                        std::size_t groupedScalarCount = 0;
                        if (!dommy::geometry::checkedTupleScalarCount(num_grouped, 3,
                                                                      groupedScalarCount) ||
                            groupedScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                            MGlobal::displayError(
                                "DPMarker: group buffer exceeds the viewport budget.");
                            return;
                        }
                        dData.grpPtBuffer.resize(groupedScalarCount);

                        size_t i = 0;
                        size_t pIdx = 0;
                        size_t offset = 0;
                        for (auto pIt = inputDPData->beginPt(), epIt = inputDPData->endPt();
                             pIt != epIt; ++pIt) {
                            if (grpIdsList[offset]) {
                                dData.grpPtBuffer[i++] = (*pIt)[0];
                                dData.grpPtBuffer[i++] = (*pIt)[1];
                                dData.grpPtBuffer[i++] = (*pIt)[2];
                                pIdx++;
                            }
                            offset++;
                        }

                        dData.totalGroupPointCount = pIdx;
                    }
                }
            } // check group

            if (dData.isGroup) {
                std::size_t expectedGroupScalarCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(dData.totalGroupPointCount, 3U,
                                                              expectedGroupScalarCount) ||
                    expectedGroupScalarCount != dData.grpPtBuffer.size() ||
                    expectedGroupScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DPMarker: group viewport layout is inconsistent.");
                    dData.grpPtBuffer.clear();
                    dData.totalGroupPointCount = 0U;
                    dData.isGroup = false;
                }
            }
        }
    }
} catch (...) {
    clearPointMarkerGeometry(dData);
    mMultiplierChanged = true;
    dommy::maya::reportViewportCallbackException("DPMarkerOverride", "updateDG",
                                                 std::current_exception());
}

void DPMarkerOverride::updateRenderItems(const MDagPath& /*path*/,
                                         MHWRender::MRenderItemList& list) try {
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DPMarkerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVFatPointShader, mColorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dIntegerNumericShader, mIndexUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dPointVectorShader, mVectorUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloatNumericShader, mValueUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat2NumericShader, mValue2UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFloat3NumericShader, mValue3UIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mGroupPointShader);

    // commonValue
    float solidColor[4] = {dData.solidColor.r, dData.solidColor.g, dData.solidColor.b,
                           1.0f};                      // main color
    float2 pSize = {dData.pointSize, dData.pointSize}; // point size
    float2 tpos = {0, -0.5};                           // value text position

#pragma mark index shader
    MHWRender::MRenderItem* DPointIndexItem = NULL;

    int index = list.indexOf(DPMarkIndexItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkIndexItemName_,
                                                         MHWRender::MRenderItem::DecorationItem,
                                                         MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sDormantPointDepthPriority);
        if (!list.append(pendingItem.get())) return;
        DPointIndexItem = pendingItem.release();
    } else {
        DPointIndexItem = list.itemAt(index);
    }

    if (DPointIndexItem) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointIndexItem, mShaderUpdateFailed);
        if (mIndexUIShader) {
            shaderUpdate.setParameter(mIndexUIShader, "solidColor", solidColor);
            shaderUpdate.setShader(mIndexUIShader, &DPMarkIndexItemName_);
        }
        shaderUpdate.enable(dData.pointCount && dData.showIndex);
    }

#pragma mark Color Shader
    MHWRender::MRenderItem* DPointColorItem = NULL;

    index = list.indexOf(DPMarkColItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkColItemName_,
                                                         MHWRender::MRenderItem::DecorationItem,
                                                         MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DPointColorItem = pendingItem.release();
    } else {
        DPointColorItem = list.itemAt(index);
    }

    if (DPointColorItem) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointColorItem, mShaderUpdateFailed);
        if (mColorUIShader) {
            shaderUpdate.setParameter(mColorUIShader, "pointSize", pSize);
            shaderUpdate.setParameter(mColorUIShader, "alphaDiscardThreshold", 1.0f);
            shaderUpdate.setParameter(mColorUIShader, "isCulled", false);
            shaderUpdate.setShader(mColorUIShader, &DPMarkColItemName_);
        }
        shaderUpdate.enable(dData.pointCount && dData.isColor);
    }

#pragma mark Vector Shader
    MHWRender::MRenderItem* DPointVectorItem = NULL;
    
    index = list.indexOf(DPMarkVecItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkVecItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DPointVectorItem = pendingItem.release();
    }
    else
    {
        DPointVectorItem = list.itemAt(index);
    }
    
    if(DPointVectorItem)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointVectorItem, mShaderUpdateFailed);
        if (mVectorUIShader)
        {
            shaderUpdate.setParameter(mVectorUIShader, "drawHeads", dData.isDrawHeads);
            shaderUpdate.setParameter(mVectorUIShader, "velocityScale", dData.velocityScale);
            shaderUpdate.setParameter(mVectorUIShader, "solidColor", solidColor);
            shaderUpdate.setShader(mVectorUIShader, &DPMarkVecItemName_);
        }
        shaderUpdate.enable(dData.pointCount && dData.isVector);
    }
    
    
#pragma mark Value3 Shader
    MHWRender::MRenderItem* DPointValue3Item = NULL;
    
    index = list.indexOf(DPMarkVal3ItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkVal3ItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DPointValue3Item = pendingItem.release();
    }
    else
    {
        DPointValue3Item = list.itemAt(index);
    }
    
    if(DPointValue3Item)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointValue3Item, mShaderUpdateFailed);
        if (mValue3UIShader)
        {
            shaderUpdate.setParameter(mValue3UIShader, "solidColor", solidColor);
            shaderUpdate.setParameter(mValue3UIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT3) {
                shaderUpdate.setParameter(mValue3UIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValue3UIShader, "isFloatValue", false);
            }
            
            shaderUpdate.setShader(mValue3UIShader, &DPMarkVal3ItemName_);
        }
        if (dData.pointCount && (dData.isValue && (dData.valType==DP_VAL_FLOAT3 || dData.valType==DP_VAL_INT3))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
        
    }
    
#pragma mark Value2 Shader
    MHWRender::MRenderItem* DPointValue2Item = NULL;
    
    index = list.indexOf(DPMarkVal2ItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkVal2ItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DPointValue2Item = pendingItem.release();
    }
    else
    {
        DPointValue2Item = list.itemAt(index);
    }
    
    if(DPointValue2Item)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointValue2Item, mShaderUpdateFailed);
        if (mValue2UIShader)
        {
            shaderUpdate.setParameter(mValue2UIShader, "solidColor", solidColor);
            shaderUpdate.setParameter(mValue2UIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT2) {
                shaderUpdate.setParameter(mValue2UIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValue2UIShader, "isFloatValue", false);
            }
            
            shaderUpdate.setShader(mValue2UIShader, &DPMarkVal2ItemName_);
        }
        if (dData.pointCount && (dData.isValue && (dData.valType==DP_VAL_FLOAT2 || dData.valType==DP_VAL_INT2))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }
    
    
#pragma mark Value1 Shader
    MHWRender::MRenderItem* DPointValue1Item = NULL;
    
    index = list.indexOf(DPMarkVal1ItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkVal1ItemName_,
                                                          MHWRender::MRenderItem::DecorationItem,
                                                          MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DPointValue1Item = pendingItem.release();
    }
    else
    {
        DPointValue1Item = list.itemAt(index);
    }
    
    if(DPointValue1Item)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointValue1Item, mShaderUpdateFailed);
        if (mValueUIShader)
        {
            shaderUpdate.setParameter(mValueUIShader, "solidColor", solidColor);
            shaderUpdate.setParameter(mValueUIShader, "pointPlacement", tpos);
            if (dData.valType == DP_VAL_FLOAT) {
                shaderUpdate.setParameter(mValueUIShader, "isFloatValue", true);
            } else {
                shaderUpdate.setParameter(mValueUIShader, "isFloatValue", false);
            }
            
            shaderUpdate.setShader(mValueUIShader, &DPMarkVal1ItemName_);
        }
        if (dData.pointCount && (dData.isValue && (dData.valType==DP_VAL_FLOAT || dData.valType==DP_VAL_INT))) {
            shaderUpdate.enable(true);
        } else {
            shaderUpdate.enable(false);
        }
    }
    
    
#pragma mark group point shader
    
    const float  grpColor[4] = { dData.groupColor.r, dData.groupColor.g, dData.groupColor.b, dData.groupAlpha };
    
    MHWRender::MRenderItem* grpPoint = NULL;
    index = list.indexOf(DPMarkGroupPointItemName_);
    
    if (index < 0)
    {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPMarkGroupPointItemName_,
                                                  MHWRender::MRenderItem::DecorationItem,
                                                  MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPMarkerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        grpPoint = pendingItem.release();
    }
    else
    {
        grpPoint = list.itemAt(index);
    }
    
    if(grpPoint)
    {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*grpPoint, mShaderUpdateFailed);
        if (mGroupPointShader)
        {
            float2 gpSize = {dData.pointSize+2,dData.pointSize+2};//point size
            shaderUpdate.setParameter(mGroupPointShader, "pointSize", gpSize);
            shaderUpdate.setParameter(mGroupPointShader, "solidColor", grpColor);
            shaderUpdate.setShader(mGroupPointShader, &DPMarkGroupPointItemName_);
        }
        shaderUpdate.enable(dData.isGroup && dData.totalGroupPointCount>0);
    }
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH(
    "DPMarkerOverride", "updateRenderItems")





void DPMarkerOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                                             const MHWRender::MRenderItemList& renderItems,
                                             MHWRender::MGeometry& data) try
{
    mMultiplierChanged = true;
    if (dData.ptBuffer.empty() && dData.grpPtBuffer.empty()) {
        mMultiplierChanged = false;
        return;
    }
    
    
    
    
    
    MHWRender::MVertexBuffer* verticesBuffer       = NULL;
    float* vertices = NULL;

    MHWRender::MVertexBuffer* indexPositionBuffer = NULL;
    float* indexPositions = NULL;
    MHWRender::MVertexBuffer* indexNumericBuffer = NULL;//texture
    float* indexNumerics = NULL;
    
    
    //color
    MHWRender::MVertexBuffer* colBuffer       = NULL;
    float* colorvalues = NULL;
    //vector
    MHWRender::MVertexBuffer* vecBuffer       = NULL;
    float* vectorvalues = NULL;
    //value
    MHWRender::MVertexBuffer* val3Buffer       = NULL;
    float* val3values = NULL;
    
    
    //value or vec points
    MHWRender::MVertexBuffer* vPosBuffer = NULL;
    float* vPosPoints = NULL;
    MHWRender::MVertexBuffer* vecPosBuffer = NULL;
    float* vecPosPoints = NULL;
    
    //tmp numeric
    MHWRender::MVertexBuffer* tmpNumBuffer = NULL;
    float* tmpnumvalues = NULL;
    
    
    //group buffer
    MHWRender::MVertexBuffer* grpVerticesBuffer = NULL;
    float* grpVertices = NULL;
    
    
    
    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic())
            {
                case MHWRender::MGeometry::kPosition:
                {

                    //Index
                    if (vertexBufferDescriptor.name() == DPMarkIndexItemName_ )
                    {
                        if (!indexPositionBuffer)
                        {
                            indexPositionBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!indexPositionBuffer) return false;
                            if (indexPositionBuffer)
                            {
                                indexPositions = dommy::viewport::acquireFloatBuffer(
                                    indexPositionBuffer, vertexBufferDescriptor, dData.ptBuffer.size(), 3);
                                if (!indexPositions) return false;
                            }
                        }
                    }
                    //Color
                    else if (vertexBufferDescriptor.name() == DPMarkColItemName_)
                    {
                        if (!verticesBuffer)
                        {
                            verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!verticesBuffer) return false;
                            if (verticesBuffer)
                            {
                                vertices = dommy::viewport::acquireFloatBuffer(
                                    verticesBuffer, vertexBufferDescriptor, dData.ptBuffer.size(), 3);
                                if (!vertices) return false;
                            }
                        }
                    }
                    //Vector
                    else if (vertexBufferDescriptor.name() == DPMarkVecItemName_)
                    {
                        if (!vecPosBuffer)
                        {
                            vecPosBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vecPosBuffer) return false;
                            if (vecPosBuffer)
                            {
                                vecPosPoints = dommy::viewport::acquireFloatBuffer(
                                    vecPosBuffer, vertexBufferDescriptor, dData.ptBuffer.size(), 3);
                                if (!vecPosPoints) return false;
                            }
                        }
                    }
                    //Value Pos
                    else if (vertexBufferDescriptor.name() == DPMarkVal3ItemName_ ||
                               vertexBufferDescriptor.name() == DPMarkVal2ItemName_ ||
                               vertexBufferDescriptor.name() == DPMarkVal1ItemName_
                               )
                    {
                        if (!vPosBuffer)
                        {
                            vPosBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vPosBuffer) return false;
                            if (vPosBuffer)
                            {
                                if(!vPosPoints){
                                    vPosPoints = dommy::viewport::acquireFloatBuffer(
                                        vPosBuffer, vertexBufferDescriptor, dData.ptBuffer.size(), 3);
                                    if (!vPosPoints) return false;
                                }
                            }
                        }
                    }
                    //Group
                    else if (vertexBufferDescriptor.name() == DPMarkGroupPointItemName_ )
                    {
                        if (!grpVerticesBuffer)
                        {
                            grpVerticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!grpVerticesBuffer) return false;
                            if (grpVerticesBuffer)
                            {

                                grpVertices = dommy::viewport::acquireFloatBuffer(
                                    grpVerticesBuffer, vertexBufferDescriptor, dData.grpPtBuffer.size(), 3);
                                if (!grpVertices) return false;

                            }
                        }
                    }
                }
                    break;
                case MHWRender::MGeometry::kNormal:
                {

                }
                    break;
                case MHWRender::MGeometry::kTexture:
                {

                    static const MString numericValue("numericvalue");
                    static const MString numeric2Value("numeric2value");
                    static const MString numeric3Value("numeric3value");

                    // Fill in single numeric field
                    if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                        (vertexBufferDescriptor.name() == DPMarkIndexItemName_ ))
                    {

                        if (!indexNumericBuffer)
                        {
                            indexNumericBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!indexNumericBuffer) return false;
                            if (indexNumericBuffer)
                            {
                                indexNumerics = dommy::viewport::acquireFloatBuffer(
                                    indexNumericBuffer, vertexBufferDescriptor, dData.pointCount, 1);
                                if (!indexNumerics) return false;
                            }
                        }
                    }
                    //Vector
                    else if ((vertexBufferDescriptor.name() == DPMarkVecItemName_)) {
                        if (!vecBuffer)
                        {
                            vecBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!vecBuffer) return false;
                            if (vecBuffer)
                            {
                                vectorvalues = dommy::viewport::acquireFloatBuffer(
                                    vecBuffer, vertexBufferDescriptor, dData.vecBuffer.size(), 3);
                                if (!vectorvalues) return false;
                            }
                        }
                    }
                    //Value 3
                    else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numeric3Value) &&
                             (vertexBufferDescriptor.name() == DPMarkVal3ItemName_ ||
                              vertexBufferDescriptor.name() == DPMarkVal2ItemName_ ||
                              vertexBufferDescriptor.name() == DPMarkVal1ItemName_))
                    {

                                 if (!val3Buffer)
                                 {
                                     val3Buffer = data.createVertexBuffer(vertexBufferDescriptor);
                                     if (!val3Buffer) return false;
                                     if (val3Buffer) {
                                         val3values = dommy::viewport::acquireFloatBuffer(
                                             val3Buffer, vertexBufferDescriptor,
                                             dData.val3Buffer.size(), 3);
                                         if (!val3values) return false;
                                     }
                                 }

                    }
                    // Value3 or Value2
                    // numericなぜ必要なのかなぞとりあえず何かしら値を与えると正常に動作するのでバッファを生成
                    else if ((vertexBufferDescriptor.semanticName().toLowerCase() == numericValue) &&
                             (vertexBufferDescriptor.name() == DPMarkVal3ItemName_ ||
                              vertexBufferDescriptor.name() == DPMarkVal2ItemName_)) {
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
                             (vertexBufferDescriptor.name() == DPMarkVal2ItemName_)) {
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
                             (vertexBufferDescriptor.name() == DPMarkVal1ItemName_)) {
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

                    if ((vertexBufferDescriptor.name() == DPMarkColItemName_) ||
                        vertexBufferDescriptor.name() == DPMarkItemName_) {
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

    if ((verticesBuffer && !vertices) || (indexPositionBuffer && !indexPositions) ||
        (indexNumericBuffer && !indexNumerics) || (colBuffer && !colorvalues) ||
        (vecBuffer && !vectorvalues) || (val3Buffer && !val3values) ||
        (vPosBuffer && !vPosPoints) || (vecPosBuffer && !vecPosPoints) ||
        (tmpNumBuffer && !tmpnumvalues) || (grpVerticesBuffer && !grpVertices)) {
        return;
    }

#pragma mark commit point array
    if (dData.ptBuffer.size()) {
        if (vertices) {
            size_t i = 0;
            for (auto& it : dData.ptBuffer) {
                vertices[i++] = (it);
            }
        }

        if (verticesBuffer && vertices) {
            verticesBuffer->commit(vertices);
        }

        if (vPosPoints) {
            size_t i = 0;
            for (auto it = dData.ptBuffer.begin(), eIt = dData.ptBuffer.end(); it != eIt; ++it) {
                vPosPoints[i++] = (*it);
            }
        }

        if (vPosBuffer && vPosPoints) {
            vPosBuffer->commit(vPosPoints);
        }

        if (vecPosPoints) {
            size_t i = 0;
            for (auto it = dData.ptBuffer.begin(), eIt = dData.ptBuffer.end(); it != eIt; ++it) {
                vecPosPoints[i++] = (*it);
            }
        }

        if (vecPosBuffer && vecPosPoints) {
            vecPosBuffer->commit(vecPosPoints);
        }
    }

#pragma mark commit index

    if (dData.showIndex && dData.ptBuffer.size()) {
        if (indexPositionBuffer && indexPositions) {
            size_t i = 0;
            for (auto it = dData.ptBuffer.begin(), eIt = dData.ptBuffer.end(); it != eIt; ++it) {
                indexPositions[i++] = (*it);
            }
        }

        if (indexPositionBuffer && indexPositions) {
            indexPositionBuffer->commit(indexPositions);
        }
#pragma mark numeric index texture

        if (indexNumericBuffer && indexNumerics) {
            for (size_t i = 0; i < dData.pointCount; i++) {
                indexNumerics[i] = (float)(i);
            }
        }

        if (indexNumericBuffer && indexNumerics) {
            indexNumericBuffer->commit(indexNumerics);
        }
    }

#pragma mark color shder commit

    if (dData.colBuffer.size()) {
        if (colorvalues) {
            size_t i = 0;
            for (auto it = dData.colBuffer.begin(), eIt = dData.colBuffer.end(); it != eIt; ++it) {
                colorvalues[i++] = (*it);
            }
        }
        if (colBuffer && colorvalues) {
            colBuffer->commit(colorvalues);
        }
    }

#pragma mark vector shder commit

    if (dData.vecBuffer.size()) {
        if (vectorvalues) {

            size_t i = 0;
            for (auto it = dData.vecBuffer.begin(), eIt = dData.vecBuffer.end(); it != eIt; ++it) {
                vectorvalues[i++] = (*it);
            }
        }
        if (vecBuffer && vectorvalues) {
            vecBuffer->commit(vectorvalues);
        }
    }

#pragma mark value3 shder commit

    if (dData.val3Buffer.size()) {
        if (val3values) {
            size_t i = 0;
            for (auto it = dData.val3Buffer.begin(), eIt = dData.val3Buffer.end(); it != eIt;
                 ++it) {
                val3values[i++] = (*it);
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
        for (auto& g : dData.grpPtBuffer) {
            grpVertices[indexOffset++] = g;
        }

        grpVerticesBuffer->commit(grpVertices);
    }

    for (int idx = 0, eidx = renderItems.length(); idx < eidx; ++idx) {
        const MHWRender::MRenderItem* item = renderItems.itemAt(idx);

        if (!item)
            continue;

        std::size_t numIndex = 0U;
        if (item->name() == DPMarkIndexItemName_ || item->name() == DPMarkColItemName_ ||
            item->name() == DPMarkVecItemName_ || item->name() == DPMarkVal3ItemName_ ||
            item->name() == DPMarkVal2ItemName_ || item->name() == DPMarkVal1ItemName_) {
            numIndex = dData.pointCount;

            if (numIndex) {
                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* indexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!indexBuffer)
                    return;

                for (size_t c = 0; c < numIndex; c++) {
                    indices[c] = static_cast<unsigned int>(c);
                }

                indexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(indexBuffer)) return;
            }

        } else if (item->name() == DPMarkGroupPointItemName_) // group
        {
            if (dData.isGroup && dData.totalGroupPointCount) {

                numIndex = dData.totalGroupPointCount;

                unsigned int* indices = NULL;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                for (size_t c = 0; c < numIndex; c++)
                    indices[c] = static_cast<unsigned int>(c);

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        }
    }

    mMultiplierChanged = false;
}
DOMMY_VIEWPORT_VOID_CATCH("DPMarkerOverride", "populateGeometry")
