//
//  DMultiCurves.cpp
//  dommyTool
//
//  Created by yuzo on 4/30/16.
//
//

#include "DCurveDisplay.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportGeometryBudget.h"


#include <cmath>
#include <sstream>

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"


#include "DNodeOperation.h"
#include "DColorOperation.h"
#include "DDataOp.h"
#include "common/utility/DGeometryLayout.h"

#include <random>

#include <tbb/tick_count.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <tbb/combinable.h>




#pragma mark DMultiCurvesDisplay

MTypeId DCurveDisplay::typeId( ID_DC_DISPLAY );
const MString DCurveDisplay::typeName("DCurveDisplay");
MString	DCurveDisplay::classification("drawdb/geometry/DCurveDisplay");
MString	DCurveDisplay::registrantId("DCurveDisplayPlugin");


MObject DCurveDisplay::inDC;

MObject DCurveDisplay::aIsRender;
MObject DCurveDisplay::aiOpaque;
MObject DCurveDisplay::aiSelfShadows;
MObject DCurveDisplay::aiMatte;

MObject DCurveDisplay::aLineWidth;
MObject DCurveDisplay::aColorType;
MObject DCurveDisplay::aSolidColor;

MObject DCurveDisplay::aNodeInfo;

MObject DCurveDisplay::aIsStaticObject;
MObject DCurveDisplay::aIsMultiEndKnot;

MObject DCurveDisplay::aCurveWidthType;
MObject DCurveDisplay::aConstantWidth;
MObject DCurveDisplay::aWidthAttributeName;

MObject DCurveDisplay::aExportCurveAttribute;
MObject DCurveDisplay::aExportPointAttribute;


//for arnold
MObject DCurveDisplay::aRenderMode;
MObject DCurveDisplay::aBasisMode;
MObject DCurveDisplay::aMinPixelWidth;

MObject DCurveDisplay::aOrientType;
MObject DCurveDisplay::aOrientAttributeName;

MObject DCurveDisplay::aCurveShader;

void* DCurveDisplay::creator()
{
    return ::dommy::maya::createObject("DCurveDisplay", [&]() {
        return new DCurveDisplay();
    });
}


MStatus DCurveDisplay::compute( const MPlug& plug, MDataBlock& data ) try
{
	if(plug == aNodeInfo ) {
		MStatus stat;
		MPlug dcPlug(thisMObject(), DCurveDisplay::inDC);
		
		
		std::stringstream infoStr;
		
		infoStr << "DCurve Info\n";
		
		const bool connected = dcPlug.isConnected(&stat);
		if (!stat) return stat;
		if (connected) {
			
			
			const DCurveData* dCurves = NULL;
			stat = getValidInputDC(inDC, data, dCurves);
			if (!stat) return stat;
			
			if (dCurves) {
                
				
				//position
				
				infoStr << "	DCurve count: " << dCurves->numberOfCurves() << "\n";
				infoStr << "	total cv count: " << dCurves->getTotalCvs() << "\n";
				
                if (dCurves->numberOfCurves()) {
                    //attribute
                    size_t attrCount = dCurves->numberOfAttributes();
                    infoStr << "	curve attribute count: " << attrCount << "\n";
                    
                    for(auto itr = dCurves->beginAttr(); itr != dCurves->endAttr(); ++itr) {
                        infoStr << "		@" << itr->first << " : " << itr->second->typeName() <<  "\n";
                    }
                    
                    //attribute
                    size_t attrPtCount = dCurves->numberOfPointAttribute(false);
                    std::vector<std::string> attrInfos = dCurves->getPointAttributeInfo();
                    infoStr << "	point attribute count: " << attrPtCount << "\n";
                    for(auto itr = attrInfos.begin(); itr != attrInfos.end(); ++itr) {
                        infoStr << "		" << *itr << "\n";
                    }
                }
				
			} else {
				
				infoStr << "DCurve data is empty.";
			}
		} else {
			
			infoStr << "inDC plug is not connected.";
		}
		
		
			MString str = dommy::text::checkedStreamText(infoStr, "DCurveDisplay info formatting failed").c_str();
			MDataHandle strHandle = data.outputValue(aNodeInfo, &stat);
			if (!stat) return stat;
			strHandle.set(str);
			
			return data.setClean(plug);
		
	}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCurveDisplay")

bool DCurveDisplay::isBounded() const
{
	return true;
}

MBoundingBox DCurveDisplay::boundingBox() const try
{
	MPoint corner1( -0.5, -0.5, -0.5 );
	MPoint corner2( 0.5, 0.5, 0.5 );
	
	MPlug crvPlug(thisMObject(), DCurveDisplay::inDC);
	
    MStatus connectionStatus;
    const bool connected = crvPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DCurveDisplay: unable to query input connection for bounds.");
    } else if (connected) {
		
        dommy::maya::ScopedPlugDataHandle dCurveHndOwner;
        MDataHandle& dCurveHnd = dCurveHndOwner.handle();
        if (!dCurveHndOwner.read(crvPlug)) {
            return MBoundingBox(corner1, corner2);
        }
		
        const DCurveData* const dCurves(getInputDCfromHandle(dCurveHnd));
        
        if (dCurves && dCurves->numberOfCurves()) {
			
            return dCurves->getBoundingBox();
        }
	}
	
	return MBoundingBox( corner1, corner2 );
	
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("DCurveDisplay")

void DCurveDisplay::draw( M3dView& /*view*/, const MDagPath& /*path*/,
                        M3dView::DisplayStyle /*style*/,
                        M3dView::DisplayStatus /*status*/ )
{
}

MStatus DCurveDisplay::initialize()
try {
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
    
    aIsRender = nAttr.create("IsRender", "isren", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRender));
    
    aiOpaque = nAttr.create("aiOpaque", "aiopq", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiOpaque));
    
    aiSelfShadows = nAttr.create("aiSelfShadows", "aisshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiSelfShadows));
    
    aiMatte = nAttr.create("aiMatte", "aimat", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiMatte));
    
    
	
	
	
	aLineWidth = nAttr.create("LineWidth", "lnw", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0f));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLineWidth));
	
	
	MFnEnumAttribute enumFn;
	aColorType = enumFn.create( "ColorType", "colty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Solid Color", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Random Color", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aColorType));

	
	aSolidColor = nAttr.create("SolidColor", "scol", MFnNumericData::k3Float, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 1.0f, 1.0f));
	DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSolidColor));
	
	
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
#pragma mark render and export common atribute
	
    aIsStaticObject = nAttr.create("IsStaticObject", "ismstobj", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsStaticObject));
	
	aIsMultiEndKnot = nAttr.create("IsMultiEndKnot", "ismltknt", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMultiEndKnot));
	
	
	MFnEnumAttribute widthEnum;
	aCurveWidthType = widthEnum.create("CurveWidthType", "crvwty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(widthEnum.addField("Constant", 0));
	DOMMY_RETURN_IF_FAILURE(widthEnum.addField("From Attribute", 1));
	DOMMY_RETURN_IF_FAILURE(widthEnum.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveWidthType));
	
	
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
	
	
	const MObject aExportCurveAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aExportCurveAttribute = tAttr.create("ExportCurveAttribute", "excrvattr", MFnData::kString, aExportCurveAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportCurveAttribute));
	
	const MObject aExportPointAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aExportPointAttribute = tAttr.create("ExportPointAttribute", "exptattr", MFnData::kString, aExportPointAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPointAttribute));
	
	
	
#pragma mark arnold attribute
	
	MFnEnumAttribute modeEnumFn;
	aRenderMode = modeEnumFn.create("RenderMode", "renmd", 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("ribbon", 0));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("thick", 1));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("oriented", 2));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRenderMode));
	
	MFnEnumAttribute basisEnumFn;
	aBasisMode = basisEnumFn.create("BasisMode", "bmode", 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(basisEnumFn.addField("Linear", 0));
	DOMMY_RETURN_IF_FAILURE(basisEnumFn.addField("B-Spline", 1));
	DOMMY_RETURN_IF_FAILURE(basisEnumFn.addField("Catmull-Rom", 2));
	DOMMY_RETURN_IF_FAILURE(basisEnumFn.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aBasisMode));
	
	
	aMinPixelWidth= nAttr.create( "MinPixelWidth", "mpxw", MFnNumericData::kFloat , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMinPixelWidth));
	
	
	MFnEnumAttribute oriEnumFn;
	aOrientType = oriEnumFn.create("OrientType", "ority", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("None", 0));
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("Curve Attribute", 1));
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("Point Attribute", 2));
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOrientType));
	
	const MObject aOrientAttributeNameDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOrientAttributeName = tAttr.create("OrientAttributeName", "orattr", MFnData::kString, aOrientAttributeNameDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOrientAttributeName));

	
	
	aCurveShader = nAttr.createColor("CurveShader", "crvsh", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveShader));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, aNodeInfo));
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveDisplay", "initialize", std::current_exception());
}



#pragma mark DCurvesDisplayOverride

DCurveDisplayOverride::DCurveDisplayOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mSolidUIShader(NULL)
, mColorLineShader(NULL)
, mCurveDisplayNode(obj)
, mDataChanged(true)

{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DCurveDisplayOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mSolidUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVThickLineShader, mColorLineShader);
}



DCurveDisplayOverride::~DCurveDisplayOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DCurveDisplayOverride");
    shaderRelease.release(mSolidUIShader);
    shaderRelease.release(mColorLineShader);
}


MHWRender::DrawAPI DCurveDisplayOverride::supportedDrawAPIs() const
{
    // this plugin supports both GL and DX
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}

/////////////////////////////////////////////////////////////////////////////////////////////

static const MString DCurveItemName_       = "DCurveItem";
static const MString DCurveColorItemName_       = "DCurveColorItem";


void DCurveDisplayOverride::updateDG() try
{
    mDataChanged = true;
    dData.cvsBuffer.clear();
    dData.indexBuffer.clear();
    dData.colBuffer.clear();
    dData.pointCount = 0U;
    dData.primCount = 0U;

    if (readPlugValue(mCurveDisplayNode, DCurveDisplay::aSolidColor, dData.solidColor) !=
        MS::kSuccess || !dommy::maya::isFiniteColor(dData.solidColor)) {
        dData.solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    int colorType = 0;
    if (readPlugValue(mCurveDisplayNode, DCurveDisplay::aColorType, colorType) != MS::kSuccess ||
        (colorType != 0 && colorType != 1)) {
        colorType = 0;
    }
    dData.colorType = static_cast<short>(colorType);

    double lineWidth = 2.0;
    if (readPlugValue(mCurveDisplayNode, DCurveDisplay::aLineWidth, lineWidth) != MS::kSuccess ||
        !std::isfinite(lineWidth) || lineWidth < 0.0) {
        lineWidth = 2.0;
    }
    dData.lineWidth = static_cast<float>(lineWidth);

    MPlug curvePlug(mCurveDisplayNode, DCurveDisplay::inDC);
    if (curvePlug.isNull()) return;
    MStatus connectionStatus;
    const bool connected = curvePlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DCurveDisplay: unable to query input connection for drawing.");
        return;
    }
    if (!connected) return;

    dommy::maya::ScopedPlugDataHandle curveHandleOwner;
    MDataHandle& curveHandle = curveHandleOwner.handle();
    MStatus status = curveHandleOwner.read(curvePlug);
    if (!status) {
        MGlobal::displayError("DCurveDisplay: unable to evaluate curve input.");
        return;
    }
    const DCurveData* const curves = getInputDCfromHandle(curveHandle);
    if (!curves || curves->numberOfCurves() == 0U)
        return;

    std::size_t totalCvs = 0U;
    if (!curves->getCheckedTotalCvs(totalCvs) ||
        totalCvs > dommy::viewport::kMaxViewportVertexCount) {
        MGlobal::displayError("DCurveDisplay: curve points exceed the viewport budget.");
        return;
    }

    std::size_t positionScalarCount = 0U;
    std::size_t lineIndexCount = 0U;
    if (!dommy::geometry::checkedTupleScalarCount(totalCvs, 3U, positionScalarCount) ||
        positionScalarCount > dommy::viewport::kMaxViewportScalarCount) {
        MGlobal::displayError("DCurveDisplay: position buffer exceeds the viewport budget.");
        return;
    }
    for (DCurveData::ConstDCurveIter curve = curves->beginCurves(); curve != curves->endCurves();
         ++curve) {
        if (!*curve ||
            !dommy::geometry::checkedAccumulatePolylineIndices((*curve)->numberOfPoints(),
                                                               lineIndexCount) ||
            lineIndexCount > dommy::viewport::kMaxViewportIndexCount) {
            MGlobal::displayError("DCurveDisplay: line indices exceed the viewport budget.");
            return;
        }
    }

    std::size_t colorScalarCount = 0U;
    if (dData.colorType == 1 &&
        (!dommy::geometry::checkedTupleScalarCount(totalCvs, 4U, colorScalarCount) ||
         colorScalarCount > dommy::viewport::kMaxViewportScalarCount)) {
        MGlobal::displayError("DCurveDisplay: color buffer exceeds the viewport budget.");
        return;
    }

    std::vector<float> positions(positionScalarCount);
    std::vector<unsigned int> indices(lineIndexCount);
    std::vector<float> colors(colorScalarCount);
    std::size_t positionOffset = 0U;
    std::size_t pointOffset = 0U;
    std::size_t indexOffset = 0U;
    std::size_t colorOffset = 0U;
    std::mt19937 rng(1U);
    std::uniform_real_distribution<float> randomColor(0.3f, 0.9f);

    for (DCurveData::ConstDCurveIter curve = curves->beginCurves(); curve != curves->endCurves();
         ++curve) {
        const DPointList* const sourcePoints = (*curve)->getPositionPtrConst();
        if (!sourcePoints) {
            MGlobal::displayError("DCurveDisplay: curve has no point storage.");
            return;
        }
        const DPointList points =
            (*curve)->isIdentity() ? *sourcePoints : (*curve)->getPointsInWorld();
        const std::size_t count = points.size();
        if (count == 0U)
            continue;

        const std::size_t curvePositionOffset = positionOffset;
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0U, count, DGetGlobalProcessNum(count)),
                          [&](const tbb::blocked_range<std::size_t>& range) {
                              for (std::size_t index = range.begin(); index != range.end();
                                   ++index) {
                                  const std::size_t target = curvePositionOffset + index * 3U;
                                  positions[target] = points[index][0];
                                  positions[target + 1U] = points[index][1];
                                  positions[target + 2U] = points[index][2];
                              }
                          });
        positionOffset += count * 3U;

        if (dData.colorType == 1) {
            const MColor color(randomColor(rng), randomColor(rng), randomColor(rng), 1.0f);
            for (std::size_t index = 0U; index < count; ++index) {
                colors[colorOffset++] = color.r;
                colors[colorOffset++] = color.g;
                colors[colorOffset++] = color.b;
                colors[colorOffset++] = color.a;
            }
        }

        for (std::size_t index = 1U; index < count; ++index) {
            indices[indexOffset++] = static_cast<unsigned int>(pointOffset + index - 1U);
            indices[indexOffset++] = static_cast<unsigned int>(pointOffset + index);
        }
        pointOffset += count;
    }

    if (positionOffset != positions.size() || indexOffset != indices.size() ||
        colorOffset != colors.size() || pointOffset != totalCvs) {
        MGlobal::displayError("DCurveDisplay: generated viewport layout is inconsistent.");
        return;
    }

    dData.cvsBuffer.swap(positions);
    dData.indexBuffer.swap(indices);
    dData.colBuffer.swap(colors);
    dData.pointCount = totalCvs;
    dData.primCount = curves->numberOfCurves();
}
DOMMY_VIEWPORT_VOID_CATCH("DCurveDisplayOverride", "updateDG")

void DCurveDisplayOverride::updateRenderItems(const MDagPath& /*path*/,
                                              MHWRender::MRenderItemList& list) try {
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DCurveDisplayOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mSolidUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVThickLineShader, mColorLineShader);
    const float wireframeColor[4] = {dData.solidColor.r, dData.solidColor.g, dData.solidColor.b,
                                     1.0f}; // main color
    const float lineWidth[2] = {dData.lineWidth, dData.lineWidth};
#pragma mark curve base shader
    MHWRender::MRenderItem* solidDCurve = nullptr;

    int index = list.indexOf(DCurveItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(
            DCurveItemName_, MHWRender::MRenderItem::DecorationItem, MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DCurveDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        solidDCurve = pendingItem.release();
    } else {
        solidDCurve = list.itemAt(index);
    }

    if (solidDCurve) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*solidDCurve, mShaderUpdateFailed);
        if (mSolidUIShader) {
            shaderUpdate.setParameter(mSolidUIShader, "solidColor", wireframeColor);
            shaderUpdate.setParameter(mSolidUIShader, "lineWidth", lineWidth);
            shaderUpdate.setShader(mSolidUIShader, &DCurveItemName_);
        }
        shaderUpdate.enable(dData.primCount && !(dData.colorType == 1));
    }

#pragma mark curve color shader
    MHWRender::MRenderItem* colorDCurve = nullptr;

    index = list.indexOf(DCurveColorItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DCurveColorItemName_,
                                                     MHWRender::MRenderItem::DecorationItem,
                                                     MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DCurveDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(5);
        if (!list.append(pendingItem.get())) return;
        colorDCurve = pendingItem.release();
    } else {
        colorDCurve = list.itemAt(index);
    }

    if (colorDCurve) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*colorDCurve, mShaderUpdateFailed);
        if (mColorLineShader) {
            shaderUpdate.setParameter(mColorLineShader, "lineWidth", lineWidth);
            shaderUpdate.setShader(mColorLineShader, &DCurveColorItemName_);
        }
        shaderUpdate.enable(dData.primCount && dData.colorType == 1 && !dData.colBuffer.empty());
    }
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH("DCurveDisplayOverride", "updateRenderItems")

void DCurveDisplayOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                                             const MHWRender::MRenderItemList& renderItems,
                                             MHWRender::MGeometry& data) try {
    mDataChanged = true;
    if (dData.cvsBuffer.empty() && dData.indexBuffer.empty()) {
        mDataChanged = false;
        return;
    }
    // cvs
    MHWRender::MVertexBuffer* verticesBuffer = nullptr;
    float* vertices = nullptr;

    // color
    MHWRender::MVertexBuffer* colBuffer = nullptr;
    float* colorvalues = nullptr;

    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic()) {
            case MHWRender::MGeometry::kPosition: {

                if (vertexBufferDescriptor.name() == DCurveItemName_ ||
                    vertexBufferDescriptor.name() == DCurveColorItemName_) {
                    if (!verticesBuffer) {
                        verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!verticesBuffer) return false;

                        if (verticesBuffer) {
                            vertices = dommy::viewport::acquireFloatBuffer(
                                verticesBuffer, vertexBufferDescriptor, dData.cvsBuffer.size(), 3);
                            if (!vertices) return false;
                        }
                    }
                }
            } break;
            case MHWRender::MGeometry::kColor: {

                if ((vertexBufferDescriptor.name() == DCurveColorItemName_)) {
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

    const bool hasVertices = !dData.cvsBuffer.empty();
    const bool hasColors = !dData.colBuffer.empty();
    if (hasVertices && (!verticesBuffer || !vertices)) {
        return;
    }
    if (hasColors && (!colBuffer || !colorvalues)) {
        return;
    }

#pragma mark commit position

    if (hasVertices) {

        std::copy(dData.cvsBuffer.begin(), dData.cvsBuffer.end(), vertices);
        verticesBuffer->commit(vertices);
    }

#pragma mark color shder commit

    if (hasColors) {
        std::copy(dData.colBuffer.begin(), dData.colBuffer.end(), colorvalues);
        colBuffer->commit(colorvalues);
    }

    for (int idx = 0, eidx = renderItems.length(); idx < eidx; ++idx) {
        const MHWRender::MRenderItem* item = renderItems.itemAt(idx);

        if (!item)
            continue;

        if (item->name() == DCurveItemName_ || item->name() == DCurveColorItemName_) {
            if (dData.indexBuffer.size()) {
                const std::size_t numIndex = dData.indexBuffer.size();
                unsigned int* indices = nullptr;
                MHWRender::MIndexBuffer* outIndexBuffer =
                    dommy::viewport::createIndexBuffer(data, numIndex, indices);
                if (!outIndexBuffer)
                    return;

                std::copy(dData.indexBuffer.begin(), dData.indexBuffer.end(), indices);

                outIndexBuffer->commit(indices);
                if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
            }
        }
    }
    mDataChanged = false;
}
DOMMY_VIEWPORT_VOID_CATCH("DCurveDisplayOverride", "populateGeometry")
