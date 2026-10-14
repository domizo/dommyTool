//
//  DPointDisplayNode.cpp
//  dommyTool
//
//  Created by yuzo on 2015/03/15.
//
//

#include "DPointDisplay.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DNodeOperation.h"
#include "DColorOperation.h"
#include "DStringOp.h"
#include "common/utility/DCheckedSize.h"

#include <limits>
#include <memory>


MTypeId DPointDisplay::typeId( ID_DP_DISPLAY_VP2 );
const MString DPointDisplay::typeName("DPointDisplay");
MString DPointDisplay::classification("drawdb/geometry/DPointDisplay");
MString	DPointDisplay::registrantId("DPointDisplayPlugin");


MObject DPointDisplay::inDP;

MObject DPointDisplay::aPointSize;
MObject DPointDisplay::aSolidColor;

MObject DPointDisplay::aNodeInfo;
MObject DPointDisplay::aCachedDObject;


//for render
MObject DPointDisplay::aIsRender;

MObject DPointDisplay::aIsStaticObject;

MObject DPointDisplay::aiOpaque;
MObject DPointDisplay::aiMatte;
MObject DPointDisplay::aiSelfShadows;
MObject DPointDisplay::aiReceiveShadows;

MObject DPointDisplay::aPointWidthType;
MObject DPointDisplay::aConstantWidth;
MObject DPointDisplay::aWidthAttributeName;
MObject DPointDisplay::aVelocityAttributeName;

MObject DPointDisplay::aExportPointAttribute;
MObject DPointDisplay::aExportAnimPointAttribute;

//for arnold

MObject DPointDisplay::aRenderMode;
MObject DPointDisplay::aPointShader;




DPointDisplay::DPointDisplay() {}
DPointDisplay::~DPointDisplay() {}

void* DPointDisplay::creator()
{
    return ::dommy::maya::createObject("DPointDisplay", [&]() {
        return new DPointDisplay();
    });
}

MSelectionMask DPointDisplay::getShapeSelectionMask() const
{
	return MSelectionMask("DPointDisplaySelection");
}

MStatus DPointDisplay::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != aNodeInfo && plug != aCachedDObject)
        return MS::kUnknownParameter;

    MStatus status;
    const bool connected = MPlug(thisMObject(), inDP).isConnected(&status);
    if (!status) return status;
    const DPData* points = NULL;
    if (connected) {
        status = getValidInputDP(inDP, data, points);
        if (!status) return status;
        if (!points) return MS::kFailure;
    }

    if (plug == aCachedDObject) {
        MDataHandle output = data.outputValue(aCachedDObject, &status);
        if (!status) return status;
        // Historical evaluation notification, not a "contains points" flag.
        output.set(true);
        return data.setClean(plug);
    }

    std::stringstream info;
    info << "DPoint Info\n";
    if (points) {
        info << "    point count: " << points->numberOfPoints() << "\n";
        info << "    attribute count: " << points->numberOfAttributes() << "\n";
        for (auto iter = points->beginAttr(), end = points->endAttr(); iter != end; ++iter)
            info << "        @" << iter->first << " : " << iter->second->typeName() << "\n";
        info << "    group count: " << points->numberOfGroups() << "\n";
        for (auto iter = points->beginGrp(), end = points->endGrp(); iter != end; ++iter)
            info << "        *" << iter->first << " : " << iter->second->numberOfIds() << "\n";
    } else {
        info << "DPoint is not connected.";
        info << "DPoint Data isEmpty.\n";
    }

    const MString text(dommy::text::checkedStreamText(info, "DPointDisplay info formatting failed").c_str());
    MDataHandle output = data.outputValue(aNodeInfo, &status);
    if (!status) return status;
    output.set(text);
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPointDisplay")



void DPointDisplay::draw( M3dView & view, const MDagPath & path,
                             M3dView::DisplayStyle style,
                             M3dView::DisplayStatus status )
{
	(void)view;
	(void)path;
	(void)style;
	(void)status;
}

bool DPointDisplay::isBounded() const
{
	return true;
}

MBoundingBox DPointDisplay::boundingBox() const try
{
	
	
	MPlug dpPlug(thisMObject(), inDP);
    
    MStatus connectionStatus;
    const bool connected = dpPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DPointDisplay: unable to query input connection for bounds.");
    } else if (connected) {

        MObject obj;
        const DPData* dp =  getInputDPfromPlug(dpPlug, obj);
        
		if(dp) {
            MBoundingBox bbox = dp->getBoundingBox();
            
            return bbox;
		}
	}
	
	MPoint corner1( -1.0f, -1.0f, -1.0f );
	MPoint corner2( 1.0f, 1.0f, 1.0f );
	
	return MBoundingBox( corner1, corner2 );
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH_WITH_HALF_EXTENT("DPointDisplay", 1.0)


MStatus DPointDisplay::initialize()
try {
    MStatus				stat;
    MFnNumericAttribute	nAttr;
    MFnTypedAttribute	tAttr;
    MFnEnumAttribute eAttr;
	
	
    MFnStringData fnStringData;
	
	
    // ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
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
    
    aCachedDObject = nAttr.create("CachedDObject", "cdobj", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCachedDObject));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aCachedDObject));
    
    
#pragma mark render attribute
	
	aIsRender = nAttr.create("IsRender", "isren", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRender));
	
	
	
#pragma mark render and export common atribute
	
	aIsStaticObject = nAttr.create("IsStaticObject", "ismstobj", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsStaticObject));
    
    
    
    
    aiOpaque = nAttr.create("aiOpaque", "aiopq", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiOpaque));
    
    
    
    aiMatte = nAttr.create("aiMatte", "aimat", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiMatte));
    
    aiSelfShadows = nAttr.create("aiSelfShadows", "aisshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiSelfShadows));
    
    aiReceiveShadows = nAttr.create("aiReceiveShadows", "airshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiReceiveShadows));
    
    
	
	
	MFnEnumAttribute widthEnum;
	aPointWidthType = widthEnum.create("PointWidthType", "ptwty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(widthEnum.addField("Constant", 0));
	DOMMY_RETURN_IF_FAILURE(widthEnum.addField("From Attribute", 1));
	DOMMY_RETURN_IF_FAILURE(widthEnum.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aPointWidthType));
	
	
	aConstantWidth = nAttr.create( "ConstantWidth", "cw", MFnNumericData::kFloat , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aConstantWidth));
	
	const MObject aWidthAttributeNameDefaultData = fnStringData.create("width", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aWidthAttributeName = tAttr.create("WidthAttributeName", "wattr", MFnData::kString, aWidthAttributeNameDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthAttributeName));
	
    const MObject aVelocityAttributeNameDefaultData = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityAttributeName = tAttr.create("VelocityAttributeName", "velattr", MFnData::kString, aVelocityAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityAttributeName));
	
	
	const MObject aExportPointAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aExportPointAttribute = tAttr.create("ExportPointAttribute", "exptattr", MFnData::kString, aExportPointAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPointAttribute));
	
	
	const MObject aExportAnimPointAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aExportAnimPointAttribute = tAttr.create("ExportAnimPointAttribute", "exanptattr", MFnData::kString, aExportAnimPointAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportAnimPointAttribute));
	
	
#pragma mark arnold attribute
	
	MFnEnumAttribute modeEnumFn;
	aRenderMode = modeEnumFn.create("RenderMode", "renmd", 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("sphere", 0));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("disk", 1));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("quad", 2));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRenderMode));
	
	
	
	aPointShader = nAttr.createColor("PointShader", "ptsh", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aPointShader));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aNodeInfo));
    
    
    
	
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPointDisplay", "initialize", std::current_exception());
}

DPointDisplayData::DPointDisplayData() : MUserData(false)
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


#pragma mark DPointDisplayNodeOverride method

DPointDisplayOverride::DPointDisplayOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mSolidUIShader(nullptr)
, mDGNode(obj)
, mMultiplierChanged(true)
{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DPointDisplayOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mSolidUIShader);
}


DPointDisplayOverride::~DPointDisplayOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DPointDisplayOverride");
    shaderRelease.release(mSolidUIShader);
}

MHWRender::DrawAPI DPointDisplayOverride::supportedDrawAPIs() const
{
	// this plugin supports both GL and DX
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}

const MString DPointItemName_               = "DPointItem";



void DPointDisplayOverride::updateDG() try
{
    mMultiplierChanged = true;
    dData.ptBuffer.clear();
    dData.pointCount = 0U;

    if (readPlugValue(mDGNode, DPointDisplay::aSolidColor, dData.solidColor) != MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.solidColor)) {
        dData.solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    double pointSize = 3.0;
    if (readPlugValue(mDGNode, DPointDisplay::aPointSize, pointSize) != MS::kSuccess ||
        pointSize < 0.0 || !dommy::numeric::checkedFloatCast(pointSize, dData.pointSize)) {
        dData.pointSize = 3.0f;
    }

    const MPlug dpPlug(mDGNode, DPointDisplay::inDP);
    MStatus connectionStatus;
    const bool connected = dpPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DPointDisplay: unable to query input connection for drawing.");
        return;
    }
    if (!connected) return;

    MObject inputObject;
    const DPData* inputDPData = NULL;
    if (!dommy::maya::getPluginDataFromPlug(dpPlug, DPData::id, inputObject, inputDPData)) {
        MGlobal::displayError("DPointDisplay: connected input is not valid DPData.");
        return;
    }

    const std::size_t count = inputDPData->numberOfPoints();
    std::size_t scalarCount = 0U;
    if (count > dommy::viewport::kMaxViewportVertexCount ||
        !dommy::geometry::checkedTupleScalarCount(count, 3U, scalarCount) ||
        scalarCount > dommy::viewport::kMaxViewportScalarCount) {
        MGlobal::displayError("DPointDisplay: point buffer exceeds the viewport budget.");
        return;
    }
    const DPointList* points = inputDPData->getPositionPtrConst();
    if (!points || points->size() != count) {
        MGlobal::displayError("DPointDisplay: invalid point storage.");
        return;
    }
    for (std::size_t i = 0U; i < count; ++i) {
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::canRepresentAsFloat((*points)[i][axis])) {
                MGlobal::displayError("DPointDisplay: invalid point position.");
                return;
            }
        }
    }
    if (count == 0U) return;

    std::vector<float> positions;
    positions.resize(scalarCount);
    const tbb::blocked_range<std::size_t> range(0U, count, DGetGlobalProcessNum(count));
    tbb::parallel_for(range, [&](const tbb::blocked_range<std::size_t>& r) {
        for (std::size_t i = r.begin(); i != r.end(); ++i) {
            const std::size_t offset = i * 3U;
            positions[offset] = (*points)[i][0];
            positions[offset + 1U] = (*points)[i][1];
            positions[offset + 2U] = (*points)[i][2];
        }
    });

    dData.ptBuffer.swap(positions);
    dData.pointCount = static_cast<unsigned int>(count);
}
catch (...) {
    dData.ptBuffer.clear();
    dData.pointCount = 0U;
    mMultiplierChanged = true;
    dommy::maya::reportViewportCallbackException(
        "DPointDisplayOverride", "updateDG", std::current_exception());
}

void DPointDisplayOverride::updateRenderItems(
    const MDagPath& /*path*/, MHWRender::MRenderItemList& list) try
{
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DPointDisplayOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mSolidUIShader);
	//commonValue
	float  solidColor[4] = { dData.solidColor.r, dData.solidColor.g, dData.solidColor.b, 1.0f };
	float2 pSize = {dData.pointSize,dData.pointSize};//point size
	//my add
	MHWRender::MRenderItem* DPointBaseItem = nullptr;
	
	int index = list.indexOf(DPointItemName_);
	
	if (index < 0)
	{
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPointItemName_,
                MHWRender::MRenderItem::DecorationItem, MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPointDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        DPointBaseItem = pendingItem.release();
	}
	else
	{
		DPointBaseItem = list.itemAt(index);
	}
	
	if(DPointBaseItem)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*DPointBaseItem, mShaderUpdateFailed);
		if (mSolidUIShader)
		{
			shaderUpdate.setParameter(mSolidUIShader, "solidColor", solidColor);
			shaderUpdate.setParameter(mSolidUIShader, "pointSize", pSize);
			shaderUpdate.setShader(mSolidUIShader, &DPointItemName_);
		}
        
		shaderUpdate.enable(dData.pointCount);
	}
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH("DPointDisplayOverride", "updateRenderItems")

void DPointDisplayOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                                                 const MHWRender::MRenderItemList& renderItems,
                                                 MHWRender::MGeometry& data) try
{
    mMultiplierChanged = true;
    std::size_t scalarCount = 0U;
    if (dData.pointCount > dommy::viewport::kMaxViewportVertexCount ||
        !dommy::geometry::checkedTupleScalarCount(dData.pointCount, 3U, scalarCount) ||
        scalarCount != dData.ptBuffer.size() ||
        scalarCount > dommy::viewport::kMaxViewportScalarCount) return;
    if (dData.pointCount == 0U) {
        mMultiplierChanged = false;
        return;
    }
	
	
	
    MHWRender::MVertexBuffer* verticesBuffer = nullptr;
    float* vertices = nullptr;
	
	
    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic())
            {
                case MHWRender::MGeometry::kPosition:
                {

                    if (vertexBufferDescriptor.name() == DPointItemName_)
                    {
                        if (!verticesBuffer)
                        {
                            verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                            if (!verticesBuffer) return false;
                            if (verticesBuffer)
                            {
                                vertices = dommy::viewport::acquireFloatBuffer(
                                    verticesBuffer, vertexBufferDescriptor,
                                    dData.ptBuffer.size(), 3);
                                if (!vertices) return false;
                            }
                        }
                    }
                }
                    break;
                default:
                    break;
            }
            return true;
        });
    if (!streamsReady) return;
    
	
	
#pragma mark commit point array
	if (dData.pointCount) {
		
		if(vertices && dData.ptBuffer.size() ==
					   static_cast<std::size_t>(dData.pointCount) * 3)
        {
            memcpy(vertices, dData.ptBuffer.data(), sizeof(float)*dData.ptBuffer.size());
        }
        
        if(verticesBuffer && vertices)
        {
            verticesBuffer->commit(vertices);
        }
		
	}
    
	
	
    for (int idx=0, eidx=renderItems.length(); idx<eidx; ++idx)
    {
        const MHWRender::MRenderItem* item = renderItems.itemAt(idx);
		 
        if (!item) continue;
		
	        std::size_t numIndex = 0U;
		if (item->name() == DPointItemName_)
		{
			numIndex = dData.pointCount;
			
			if (numIndex)
			{
					unsigned int* indices = nullptr;
					MHWRender::MIndexBuffer* indexBuffer =
						dommy::viewport::createIndexBuffer(
							data, numIndex, indices);
					if (!indexBuffer) return;
                
					std::size_t c = 0U;
                while (c<numIndex) {
                    indices[c] = c;
                    c++;
                }

				indexBuffer->commit(indices);
				if (!item->associateWithIndexBuffer(indexBuffer)) return;
			}
		}
    }
    mMultiplierChanged = false;
}
DOMMY_VIEWPORT_VOID_CATCH("DPointDisplayOverride", "populateGeometry")
