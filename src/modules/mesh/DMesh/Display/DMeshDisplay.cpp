//
//  DMeshDisplay.cpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//

#include "DMeshDisplay.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DPointTransform.h"

#include <cmath>
#include <sstream>

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"
#include "DColorOperation.h"

#include <random>

#pragma mark DMeshDisplay

MTypeId DMeshDisplay::typeId(ID_DM_DISPLAY);
const MString DMeshDisplay::typeName("DMeshDisplay");
MString DMeshDisplay::classification("drawdb/geometry/DMeshDisplay");
MString DMeshDisplay::registrantId("DMeshDisplayPlugin");

MObject DMeshDisplay::inDM;

MObject DMeshDisplay::aIsWireFrame;
MObject DMeshDisplay::aIsVertex;
MObject DMeshDisplay::aIsShade;
MObject DMeshDisplay::aDisplayMode;

MObject DMeshDisplay::aPointSize;
MObject DMeshDisplay::aLineWidth;
MObject DMeshDisplay::aColorType;
MObject DMeshDisplay::aSolidColor;

// Render
MObject DMeshDisplay::aIsRender;
MObject DMeshDisplay::aiOpaque;
MObject DMeshDisplay::aiSelfShadows;
MObject DMeshDisplay::aiMatte;
MObject DMeshDisplay::aIsStaticObject;

// Mesh Option
MObject DMeshDisplay::aSmoothing;

MObject DMeshDisplay::aCreaseEdgeGroup;
MObject DMeshDisplay::aExportUVs;

MObject DMeshDisplay::aSubdivType;
MObject DMeshDisplay::aSubdivIterations;
MObject DMeshDisplay::aSubdivAdaptiveError;
MObject DMeshDisplay::aSubdivDicingCamera;
MObject DMeshDisplay::aSubdivAdaptiveMetric;
MObject DMeshDisplay::aSubdivUVSmoothing;
MObject DMeshDisplay::aSubdivAdaptiveSpace;
MObject DMeshDisplay::aSubdivSmoothDerivs;

MObject DMeshDisplay::aDispMap;
MObject DMeshDisplay::aDispHeight;
MObject DMeshDisplay::aDispZeroValue;
MObject DMeshDisplay::aDispPadding;
MObject DMeshDisplay::aDispAutobump;

MObject DMeshDisplay::aSidedness;
MObject DMeshDisplay::aInvertNormals;

MObject DMeshDisplay::aExportMeshAttribute;
MObject DMeshDisplay::aExportPointAttribute;

MObject DMeshDisplay::aGroupMatte;
MObject DMeshDisplay::aGroupMatteEnable;
MObject DMeshDisplay::aGroupMatteName;
MObject DMeshDisplay::aMatteGroups;

MObject DMeshDisplay::aMeshShader;
MObject DMeshDisplay::aDisplacementShader;


MObject DMeshDisplay::aNodeInfo;


void* DMeshDisplay::creator()
{
    return ::dommy::maya::createObject("DMeshDisplay", [&]() {
        return new DMeshDisplay();
    });
}



MStatus DMeshDisplay::compute( const MPlug& plug, MDataBlock& data ) try
{
    
    
    
	if(plug == aNodeInfo) {
        
        
        
        
		MStatus stat;
		
		MPlug dmPlug = MPlug(thisMObject(), DMeshDisplay::inDM);
		
		
		std::stringstream infoStr;
		
		infoStr << "DMesh Info\n";
		
        
        
        
		const bool connected = dmPlug.isConnected(&stat);
		if (!stat) return stat;
		if (connected) {
            const DMeshData* dMeshes = NULL;
            stat = getValidInputDM(inDM, data, dMeshes);
            if (!stat) return stat;
			
			
			
			if (dMeshes) {
				
				size_t totalVtx;
				dMeshes->getTotalVertex(totalVtx);
				
				//position
				
				infoStr << "	DMesh count: " << dMeshes->numberOfMesh() << "\n";
				infoStr << "	total vtx count: " << totalVtx << "\n";
				
				infoStr << "	uv map: " << dMeshes->hasUVs() << "\n";
				
				
				
				if (dMeshes->numberOfMesh()) {
					//attribute
					size_t attrCount = dMeshes->numberOfAttributes();
                    if(attrCount) {
                        infoStr << "	mesh attribute count: " << attrCount << "\n";
                        for(auto itr = dMeshes->beginAttr(); itr != dMeshes->endAttr(); ++itr) {
                            infoStr << "		@" << itr->first << " : " << itr->second->typeName() <<  "\n";
                        }
                    }
										
					//point attribute
					size_t attrPtCount = dMeshes->numberOfPointAttribute(false);
                    if(attrPtCount) {
                        infoStr << "	point attribute count: " << attrPtCount << "\n";
                        for(auto itr : dMeshes->getPointAttributeInfo()) {
                            infoStr << "		" << itr << "\n";
                        }
                    }
                    
                    //face attribute
                    size_t faceAttrCount = dMeshes->numberOfCompAttr(D_COMP_FACE);
                    if(faceAttrCount) {
                        infoStr << "    face attribute count: " << faceAttrCount << "\n";
                        for(auto itr : dMeshes->getCompAttrInfo(D_COMP_FACE)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }
                    
                    //per vertex attribute
                    size_t pVtxCount = dMeshes->numberOfCompAttr(D_COMP_PER_VTX);
                    if(pVtxCount) {
                        infoStr << "    per vtx attribute count: " << pVtxCount << "\n";
                        for(auto itr : dMeshes->getCompAttrInfo(D_COMP_PER_VTX)) {
                            infoStr << "        " << itr << "\n";
                        }
                    }
                    
                    
                    
					
                    
                    //primitive group
                    size_t grpCount = dMeshes->numberOfGroups();
                    if(grpCount) {
                        infoStr << "	primitive group count: " << grpCount << "\n";
                        for(auto itr = dMeshes->beginGrp(); itr != dMeshes->endGrp(); ++itr) {
                            infoStr << "		*" << itr->first <<  "\n";
                        }
                    }
                    
                    
                    //point group
                    size_t grpPtCount = dMeshes->numberOfCompGroup(D_COMP_POINT);
                    if (grpPtCount) {
                        infoStr << "	point group count: " << grpPtCount << "\n";
                        for(auto itr : dMeshes->getCompGroupInfo(D_COMP_POINT)) {
                            infoStr << "		" << itr << "\n";
                        }
                    }
                    
                    
                    //face group
                    size_t grpFaceCount = dMeshes->numberOfCompGroup(D_COMP_FACE);
                    if(grpFaceCount) {
                        infoStr << "	face group count: " << grpFaceCount << "\n";
                        for(auto itr : dMeshes->getCompGroupInfo(D_COMP_FACE)) {
                            infoStr << "		" << itr << "\n";
                        }
                    }
                    
                    //edge group
                    size_t grpEdgeCount = dMeshes->numberOfCompGroup(D_COMP_EDGE);
                    if(grpEdgeCount) {
                        infoStr << "	edge group count: " << grpEdgeCount << "\n";
                        for(auto itr : dMeshes->getCompGroupInfo(D_COMP_EDGE)) {
                            infoStr << "		" << itr << "\n";
                        }
                    }
                    
				}
				
			} else {
				
                
                
				infoStr << "DMesh data is empty.";
			}
		} else {
            
            
			
			infoStr << "inDM plug is not connected.";
		}
		
		
		MString str = dommy::text::checkedStreamText(infoStr, "DMeshDisplay info formatting failed").c_str();
		MDataHandle strHandle = data.outputValue(aNodeInfo, &stat);
		if (!stat) return stat;
		strHandle.set(str);
        
        
        
		return data.setClean(plug);
		
	}
    
    
    
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DMeshDisplay")

bool DMeshDisplay::isBounded() const
{
	return true;
}

MBoundingBox DMeshDisplay::boundingBox() const try
{
	
	MPoint corner1( -0.5, -0.5, -0.5 );
	MPoint corner2( 0.5, 0.5, 0.5 );
	
    MStatus stat;
    
	MPlug dmPlug = MPlug(thisMObject(), DMeshDisplay::inDM);
	
    const bool connected = dmPlug.isConnected(&stat);
    if (!stat) {
        MGlobal::displayError("DMeshDisplay: unable to query input connection for bounds.");
    } else if (connected) {
		
        MObject obj;
        const DMeshData* dMeshes =  getInputDMfromPlug(dmPlug, obj);
        
        
		if (dMeshes && dMeshes->numberOfMesh()) {
			
            
            MBoundingBox mBBox = dMeshes->getBoundingBox();
            
            if (mBBox.width() > 1000000.0 ||
                mBBox.height() > 1000000.0 ||
                mBBox.depth() > 1000000.0) {
                mBBox = MBoundingBox(MPoint(-100000,-100000,-100000),MPoint(100000,100000,100000));
            }
            
			return mBBox;
		}
	}
	
	return MBoundingBox( corner1, corner2 );
	
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("DMeshDisplay")

void DMeshDisplay::draw(
    M3dView&, const MDagPath&, M3dView::DisplayStyle,
    M3dView::DisplayStatus)
{
}

MStatus DMeshDisplay::initialize()
try {
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
	
	
	aIsWireFrame = nAttr.create("IsWireFrame", "iswr", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsWireFrame));

    aIsVertex = nAttr.create("IsVertex", "isvtx", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVertex));

    aIsShade = nAttr.create("IsShade", "issh", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsShade));
    MFnEnumAttribute dpyModeEnumFn;
    aDisplayMode = dpyModeEnumFn.create("DisplayMode", "dpyty", DM_DPY_HARD, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dpyModeEnumFn.addField("Flat", DM_DPY_FLAT));
    DOMMY_RETURN_IF_FAILURE(dpyModeEnumFn.addField("Hard", DM_DPY_HARD));
    DOMMY_RETURN_IF_FAILURE(dpyModeEnumFn.addField("Smooth", DM_DPY_SMOOTH));
    DOMMY_RETURN_IF_FAILURE(dpyModeEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplayMode));

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

    MFnEnumAttribute enumFn;
    aColorType = enumFn.create("ColorType", "colty", DM_DISPLAY_COLOR_SOLID, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Solid", DM_DISPLAY_COLOR_SOLID));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Random", DM_DISPLAY_COLOR_RANDOM));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColorType));

    aSolidColor = nAttr.create("SolidColor", "scol", MFnNumericData::k3Float, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f, 1.0f, 1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setUsedAsColor(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolidColor));

#pragma mark render attribute

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

    aIsStaticObject = nAttr.create("IsStaticObject", "ismstobj", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsStaticObject));

    // Mesh Option
    aSmoothing = nAttr.create("Smoothing", "smth", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSmoothing));

    const MObject aCreaseEdgeGroupDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aCreaseEdgeGroup =
        tAttr.create("CreaseEdgeGroup", "cregrp", MFnData::kString, aCreaseEdgeGroupDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCreaseEdgeGroup));

    aExportUVs = nAttr.create("ExportUVs", "exuv", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportUVs));

    // Subdiv
    MFnEnumAttribute subdEnumFn;
    aSubdivType = subdEnumFn.create("SubdivType", "sbdv", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(subdEnumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(subdEnumFn.addField("Linear", 1));
    DOMMY_RETURN_IF_FAILURE(subdEnumFn.addField("Catclark", 2));
    DOMMY_RETURN_IF_FAILURE(subdEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivType));

    aSubdivIterations = nAttr.create("SubdivIterations", "sbdvitr", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivIterations));

    MFnEnumAttribute subdAdmtEnumFn;
    aSubdivAdaptiveMetric = subdAdmtEnumFn.create("SubdivAdaptiveMetric", "sbdvadmt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(subdAdmtEnumFn.addField("Auto", 0));
    DOMMY_RETURN_IF_FAILURE(subdAdmtEnumFn.addField("Flatness", 1));
    DOMMY_RETURN_IF_FAILURE(subdAdmtEnumFn.addField("Edge Length", 2));
    DOMMY_RETURN_IF_FAILURE(subdAdmtEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivAdaptiveMetric));

    aSubdivAdaptiveError = nAttr.create("SubdivAdaptiveError", "sbdverr", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivAdaptiveError));

    MFnEnumAttribute subdAdSpEnumFn;
    aSubdivAdaptiveSpace = subdAdSpEnumFn.create("SubdivAdaptiveSpace", "sbdvadsp", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(subdAdSpEnumFn.addField("Raster", 0));
    DOMMY_RETURN_IF_FAILURE(subdAdSpEnumFn.addField("Object", 1));
    DOMMY_RETURN_IF_FAILURE(subdAdSpEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivAdaptiveSpace));

    MFnEnumAttribute subduvsmEnumFn;
    aSubdivUVSmoothing = subduvsmEnumFn.create("SubdivUVSmoothing", "sbdvuvsm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(subduvsmEnumFn.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(subduvsmEnumFn.addField("Pin Borders", 1));
    DOMMY_RETURN_IF_FAILURE(subduvsmEnumFn.addField("Pin Corners", 2));
    DOMMY_RETURN_IF_FAILURE(subduvsmEnumFn.addField("Smooth (Pin None)", 3));
    DOMMY_RETURN_IF_FAILURE(subduvsmEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivUVSmoothing));

    aSubdivSmoothDerivs = nAttr.create("SubdivSmoothDerivs", "sbdvdrv", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivSmoothDerivs));
    // Displacement
    aDispHeight = nAttr.create("DispHeight", "dsph", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDispHeight));

    aDispZeroValue = nAttr.create("DispZeroValue", "dspzv", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDispZeroValue));

    aDispPadding = nAttr.create("DispPadding", "dsppad", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDispPadding));

    aDispAutobump = nAttr.create("DispAutobump", "dspatbmp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDispAutobump));

    // Other
    aSidedness = nAttr.create("Sidedness", "sdns", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSidedness));

    aInvertNormals = nAttr.create("InvertNormals", "invn", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvertNormals));

    const MObject aExportMeshAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportMeshAttribute = tAttr.create("ExportMeshAttribute", "exmshattr", MFnData::kString,
                                        aExportMeshAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportMeshAttribute));

    const MObject aExportPointAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportPointAttribute = tAttr.create("ExportPointAttribute", "exptattr", MFnData::kString,
                                         aExportPointAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPointAttribute));

    aGroupMatteEnable = nAttr.create("GroupMatteEnable", "grpebl", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupMatteEnable));

    const MObject aGroupMatteNameDefaultData = fnStringData.create("grpmask", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGroupMatteName = tAttr.create("GroupMatteName", "grpmatnm", MFnData::kString,
                                   aGroupMatteNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupMatteName));

    const MObject aMatteGroupsDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMatteGroups =
        tAttr.create("MatteGroups", "matgrps", MFnData::kString, aMatteGroupsDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMatteGroups));

    MFnCompoundAttribute cmpAttr;
	aGroupMatte = cmpAttr.create("GroupMatte", "grpmat", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aGroupMatteEnable));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aGroupMatteName));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aMatteGroups));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray( true ));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setUsesArrayDataBuilder(true));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupMatte));
	
	
	
	aMeshShader = nAttr.createColor("MeshShader", "mshsh", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMeshShader));
	
	aDisplacementShader = nAttr.createColor("DisplacementShader", "dspsh", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplacementShader));
	
	
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
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, aNodeInfo));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMeshDisplay", "initialize", std::current_exception());
}



#pragma mark ---------------DMeshDisplayOverride---------------


DMeshDisplayOverride::DMeshDisplayOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mSolidUIShader(NULL)
, mPointUIShader(NULL)
, mFlatUIShader(NULL)
, mWireUIShader(NULL)
, mColorMeshShader(NULL)
, mMeshDisplayNode(obj)
, mDataChanged(true)
{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DMeshDisplayOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dDefaultMaterialShader, mSolidUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mPointUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dSolidShader, mFlatUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mWireUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVSolidShader, mColorMeshShader);
}



DMeshDisplayOverride::~DMeshDisplayOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DMeshDisplayOverride");
    shaderRelease.release(mSolidUIShader);
    shaderRelease.release(mPointUIShader);
    shaderRelease.release(mFlatUIShader);
    shaderRelease.release(mWireUIShader);
    shaderRelease.release(mColorMeshShader);
}

MHWRender::DrawAPI DMeshDisplayOverride::supportedDrawAPIs() const {
    // this plugin supports both GL and DX
    return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}

/////////////////////////////////////////////////////////////////////////////////////////////

static const MString DMeshItemName_ = "DMeshItem";
static const MString DMeshPointItemName_ = "DMeshPointItem";
static const MString DMeshFlatItemName_ = "DMeshFlatItem";
static const MString DMeshWireItemName_ = "DMeshWireItem";
static const MString DMeshColorItemName_ = "DMeshColorItem";

namespace {

void clearMeshDisplayGeometry(DMeshDisplayOverride::DrawData& data) noexcept {
    data.mPointBuffer.clear();
    data.mFaceIndexList.clear();
    data.mWireIndexList.clear();
    data.colBuffer.clear();
    data.normBuffer.clear();
    data.pointCount = 0U;
    data.primCount = 0U;
}

bool transformDisplayNormal(const MVector& source, const MMatrix& transform, MVector& result) {
    // Zero is an automatic-normal marker in stored data, but not a drawable
    // shading direction. Share inverse-transpose math, not that marker policy.
    if (source.x == 0.0 && source.y == 0.0 && source.z == 0.0) return false;
    std::array<double, 3> normal;
    if (!dommy::numeric::checkedTransformNormal(source, transform, normal)) return false;
    result = MVector(normal[0], normal[1], normal[2]);
    return true;
}

} // namespace

void DMeshDisplayOverride::updateDG() try {
    mDataChanged = true;
    clearMeshDisplayGeometry(dData);

    MStatus stat;

    if (readPlugValue(mMeshDisplayNode, DMeshDisplay::aSolidColor, dData.solidColor) !=
            MS::kSuccess ||
        !dommy::maya::isFiniteColor(dData.solidColor)) {
        dData.solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (readPlugValue(mMeshDisplayNode, DMeshDisplay::aIsWireFrame, dData.isWireframe) != MS::kSuccess ||
        readPlugValue(mMeshDisplayNode, DMeshDisplay::aIsVertex, dData.isVertex) != MS::kSuccess ||
        readPlugValue(mMeshDisplayNode, DMeshDisplay::aIsShade, dData.isShade) != MS::kSuccess) return;

    short displayType = DM_DPY_HARD;
    if (readPlugValue(MPlug(mMeshDisplayNode, DMeshDisplay::aDisplayMode), displayType) != MS::kSuccess) return;
    dData.displayType = displayType >= DM_DPY_FLAT && displayType < DM_DISPLAY_TYPE_COUNT
                            ? static_cast<DM_DISPLAY_TYPE>(displayType)
                            : static_cast<DM_DISPLAY_TYPE>(DM_DPY_HARD);

    short colorType = DM_DISPLAY_COLOR_SOLID;
    if (readPlugValue(MPlug(mMeshDisplayNode, DMeshDisplay::aColorType), colorType) != MS::kSuccess) return;
    dData.colorType = colorType >= DM_DISPLAY_COLOR_SOLID && colorType < DM_DISPLAY_COLOR_TYPE_COUNT
                          ? colorType
                          : static_cast<short>(DM_DISPLAY_COLOR_SOLID);

    double pointSize = 3.0;
    if (readPlugValue(mMeshDisplayNode, DMeshDisplay::aPointSize, pointSize) != MS::kSuccess ||
        !std::isfinite(pointSize) || pointSize < 0.0) {
        pointSize = 3.0;
    }
    dData.pointSize = static_cast<float>(pointSize);

    double lineWidth = 1.0;
    if (readPlugValue(mMeshDisplayNode, DMeshDisplay::aLineWidth, lineWidth) != MS::kSuccess ||
        !std::isfinite(lineWidth) || lineWidth < 0.0) {
        lineWidth = 1.0;
    }
    dData.lineWidth = static_cast<float>(lineWidth);

    MPlug dmPlug(mMeshDisplayNode, DMeshDisplay::inDM);
    if (dmPlug.isNull()) return;
    MStatus connectionStatus;
    const bool connected = dmPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DMeshDisplay: unable to query input connection for drawing.");
        return;
    }
    if (connected) {

        MObject obj;
        const DMeshData* dMeshes = NULL;
        const MStatus dataStatus =
            dommy::maya::getPluginDataFromPlug(dmPlug, DMeshData::id, obj, dMeshes);
        if (!dataStatus) {
            MGlobal::displayError("DMeshDisplay: connected input is not valid DMeshData.");
            return;
        }

        if (dMeshes && dMeshes->numberOfMesh()) {
            const std::size_t meshCount = dMeshes->numberOfMesh();

            // set base point count
            size_t totalVtx = 0U;
            if (!dMeshes->getCheckedTotalVertex(totalVtx) ||
                totalVtx > dommy::viewport::kMaxViewportVertexCount) {
                MGlobal::displayError("DMeshDisplay: invalid mesh point count.");
                return;
            }

#pragma mark set point count
            std::size_t pointScalarCount = 0;
            if (!dommy::geometry::checkedTupleScalarCount(totalVtx, 3, pointScalarCount) ||
                pointScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                MGlobal::displayError("DMeshDisplay: point buffer exceeds the viewport budget.");
                return;
            }
            dData.mPointBuffer.resize(pointScalarCount);

            // set triangle point count
            std::size_t allTriTotalVtx = 0U;
            std::size_t expandedPositionScalarCount = 0U;
            if (dData.isShade) {
                if (!dMeshes->getCheckedTotalVertexForDisplay(allTriTotalVtx) ||
                    allTriTotalVtx > dommy::viewport::kMaxViewportIndexCount ||
                    !dommy::geometry::checkedTupleScalarCount(allTriTotalVtx, 3U,
                                                              expandedPositionScalarCount) ||
                    expandedPositionScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DMeshDisplay: invalid polygon topology.");
                    return;
                }

                dData.mFaceIndexList.reserve(allTriTotalVtx);
                if (dData.displayType == DM_DPY_HARD || dData.displayType == DM_DPY_SMOOTH) {
                    dData.normBuffer.resize(expandedPositionScalarCount);
                }
            }

            size_t vIndex = 0;
            size_t colIndex = 0;
            size_t vOffset = 0;
            size_t nOffset = 0;

            std::mt19937 rng(1U);
            std::uniform_real_distribution<float> randomColor(0.3f, 0.9f);

            // primitive colorのサイズを決定 attribute 表示の際はそちらを優先
            if (dData.isShade && dData.colorType == DM_DISPLAY_COLOR_RANDOM) {
                std::size_t colorScalarCount = 0;
                if (!dommy::geometry::checkedTupleScalarCount(totalVtx, 4, colorScalarCount) ||
                    colorScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError("DMeshDisplay: color buffer size overflow.");
                    return;
                }
                std::size_t expandedColorScalarCount = 0U;
                if (!dommy::geometry::checkedTupleScalarCount(allTriTotalVtx, 4U,
                                                              expandedColorScalarCount) ||
                    expandedColorScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                    MGlobal::displayError(
                        "DMeshDisplay: expanded color buffer exceeds the viewport budget.");
                    return;
                }
                dData.colBuffer.resize(colorScalarCount);
            }

            std::size_t processedMeshCount = 0U;
            for (auto it = dMeshes->beginMeshes(), emIt = dMeshes->endMeshes(); it != emIt; ++it) {
                if (!*it || (*it)->numberOfPoints() == 0U) {
                    MGlobal::displayError(
                        "DMeshDisplay: empty or invalid mesh cannot be displayed.");
                    return;
                }

                MObject mObj;
                stat = generateMMeshFromDmesh(*it->get(), mObj);
                if (!stat) {
                    MGlobal::displayError("DMeshDisplay: unable to create Maya mesh data.");
                    return;
                }

                MFnMesh meshFn(mObj, &stat);
                if (!stat) {
                    MGlobal::displayError("DMeshDisplay: unable to inspect Maya mesh data.");
                    return;
                }

                MIntArray triCnts, trVerts;
                if (dData.isShade && meshFn.getTriangles(triCnts, trVerts) != MS::kSuccess) {
                    MGlobal::displayError("DMeshDisplay: unable to read triangle topology.");
                    return;
                }

#pragma mark Color 表示の際のデータを生成
                if (dData.isShade && dData.colorType == DM_DISPLAY_COLOR_RANDOM) { // random

                    MColor peaceCol(randomColor(rng), randomColor(rng), randomColor(rng));

                    for (auto pIt = (*it)->beginPt(), epIt = (*it)->endPt(); pIt != epIt; ++pIt) {
                        dData.colBuffer[colIndex++] = peaceCol.r;
                        dData.colBuffer[colIndex++] = peaceCol.g;
                        dData.colBuffer[colIndex++] = peaceCol.b;
                        dData.colBuffer[colIndex++] = 1.0f; // alpha
                    }
                }
                // addPoint
                const DPointList* const sourcePoints = (*it)->getPositionPtrConst();
                if (!sourcePoints) {
                    MGlobal::displayError("DMeshDisplay: mesh has no point storage.");
                    return;
                }
                const DPointList pts =
                    (*it)->isIdentity() ? *sourcePoints : (*it)->getPointsInWorld();
                const size_t count = pts.size();
                if (count != (*it)->numberOfPoints()) {
                    MGlobal::displayError("DMeshDisplay: mesh point storage is inconsistent.");
                    return;
                }
                for (std::size_t pointIndex = 0U; pointIndex < count; ++pointIndex) {
                    if (!std::isfinite(pts[pointIndex][0]) || !std::isfinite(pts[pointIndex][1]) ||
                        !std::isfinite(pts[pointIndex][2])) {
                        MGlobal::displayError("DMeshDisplay: mesh contains a non-finite point.");
                        return;
                    }
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

#pragma mark wire data
                if (dData.isWireframe) {
                    const int edgeCount = meshFn.numEdges(&stat);
                    std::size_t additionalWireIndexCount = 0U;
                    std::size_t nextWireIndexCount = 0U;
                    std::size_t nextWireScalarCount = 0U;
                    if (!stat || edgeCount < 0 ||
                        !dommy::memory::checkedMultiply(static_cast<std::size_t>(edgeCount), 2U,
                                                        additionalWireIndexCount) ||
                        !dommy::memory::checkedAdd(dData.mWireIndexList.size(),
                                                   additionalWireIndexCount, nextWireIndexCount) ||
                        nextWireIndexCount > dommy::viewport::kMaxViewportIndexCount ||
                        !dommy::geometry::checkedTupleScalarCount(nextWireIndexCount, 3U,
                                                                  nextWireScalarCount) ||
                        nextWireScalarCount > dommy::viewport::kMaxViewportScalarCount) {
                        MGlobal::displayError(
                            "DMeshDisplay: wire indices exceed the viewport budget.");
                        return;
                    }
                    dData.mWireIndexList.reserve(nextWireIndexCount);

                    MItMeshEdge eIt(mObj, &stat);
                    if (!stat) {
                        MGlobal::displayError("DMeshDisplay: unable to iterate mesh edges.");
                        return;
                    }
                    stat = dommy::maya::visitMeshElements(
                        eIt, static_cast<std::size_t>(edgeCount),
                        [&](MItMeshEdge& edge) -> MStatus {
                        MStatus endpointStatus;
                        const int firstIndex = edge.index(0, &endpointStatus);
                        if (!endpointStatus) return endpointStatus;
                        const int secondIndex = edge.index(1, &endpointStatus);
                        if (!endpointStatus) return endpointStatus;
                        if (dData.mWireIndexList.size() > nextWireIndexCount ||
                            nextWireIndexCount - dData.mWireIndexList.size() < 2U ||
                            firstIndex < 0 || secondIndex < 0 ||
                            static_cast<std::size_t>(firstIndex) >= count ||
                            static_cast<std::size_t>(secondIndex) >= count) {
                            return MS::kInvalidParameter;
                        }
                        dData.mWireIndexList.push_back(static_cast<unsigned int>(
                            vOffset + static_cast<std::size_t>(firstIndex)));
                        dData.mWireIndexList.push_back(static_cast<unsigned int>(
                            vOffset + static_cast<std::size_t>(secondIndex)));
                        return MS::kSuccess;
                    });
                    if (!stat || dData.mWireIndexList.size() != nextWireIndexCount) {
                        MGlobal::displayError("DMeshDisplay: invalid or incomplete edge traversal.");
                        return;
                    }
                }

#pragma mark flat shade
                if (dData.isShade) {
                    std::size_t nextFaceIndexCount = 0U;
                    if (!dommy::memory::checkedAdd(dData.mFaceIndexList.size(),
                                                   static_cast<std::size_t>(trVerts.length()),
                                                   nextFaceIndexCount) ||
                        nextFaceIndexCount > allTriTotalVtx ||
                        nextFaceIndexCount > dommy::viewport::kMaxViewportIndexCount) {
                        MGlobal::displayError(
                            "DMeshDisplay: triangle indices exceed the viewport budget.");
                        return;
                    }
                    for (unsigned int triangleIndex = 0U; triangleIndex < trVerts.length();
                         ++triangleIndex) {
                        const int localIndex = trVerts[triangleIndex];
                        if (localIndex < 0 || static_cast<std::size_t>(localIndex) >= count) {
                            MGlobal::displayError("DMeshDisplay: triangle index is out of range.");
                            return;
                        }
                        dData.mFaceIndexList.push_back(static_cast<unsigned int>(
                            vOffset + static_cast<std::size_t>(localIndex)));
                    }

                    if (dData.displayType == DM_DPY_HARD || dData.displayType == DM_DPY_SMOOTH) {
                        const MMatrix normalTransform = (*it)->getTransformMatrix();
                        if (!dommy::numeric::isFiniteAffineTransform(normalTransform)) {
                            MGlobal::displayError(
                                "DMeshDisplay: mesh has an invalid normal transform.");
                            return;
                        }

                        MItMeshPolygon face(mObj, &stat);
                        if (!stat) {
                            MGlobal::displayError("DMeshDisplay: unable to iterate mesh faces.");
                            return;
                        }
                        const int faceCount = meshFn.numPolygons(&stat);
                        if (!stat || faceCount < 0) {
                            MGlobal::displayError("DMeshDisplay: unable to query face count.");
                            return;
                        }
                        stat = dommy::maya::visitMeshPolygons(
                            face, static_cast<std::size_t>(faceCount),
                            [&](MItMeshPolygon& face) -> MStatus {
                            int triangleCount = 0;
                            if (face.numTriangles(triangleCount) != MS::kSuccess ||
                                triangleCount < 0) {
                                MGlobal::displayError(
                                    "DMeshDisplay: unable to inspect face triangles.");
                                return MS::kFailure;
                            }

                            if (dData.displayType == DM_DPY_HARD) {
                                MVector faceNormal;
                                MVector displayNormal;
                                if (face.getNormal(faceNormal) != MS::kSuccess ||
                                    !transformDisplayNormal(faceNormal, normalTransform,
                                                            displayNormal)) {
                                    MGlobal::displayError("DMeshDisplay: face normal is invalid.");
                                    return MS::kFailure;
                                }
                                for (int triangle = 0; triangle < triangleCount; ++triangle) {
                                    for (int vertex = 0; vertex < 3; ++vertex) {
                                        if (nOffset + 3U > dData.normBuffer.size()) {
                                            MGlobal::displayError(
                                                "DMeshDisplay: hard-normal layout overflow.");
                                            return MS::kFailure;
                                        }
                                        dData.normBuffer[nOffset++] =
                                            static_cast<float>(displayNormal.x);
                                        dData.normBuffer[nOffset++] =
                                            static_cast<float>(displayNormal.y);
                                        dData.normBuffer[nOffset++] =
                                            static_cast<float>(displayNormal.z);
                                    }
                                }
                            } else {
                                for (int triangle = 0; triangle < triangleCount; ++triangle) {
                                    MPointArray trianglePoints;
                                    MIntArray vertexIds;
                                    if (face.getTriangle(triangle, trianglePoints, vertexIds) !=
                                            MS::kSuccess ||
                                        vertexIds.length() != 3U) {
                                        MGlobal::displayError("DMeshDisplay: unable to inspect "
                                                              "smooth-normal triangle.");
                                        return MS::kFailure;
                                    }
                                    for (unsigned int vertex = 0U; vertex < vertexIds.length();
                                         ++vertex) {
                                        MVector vertexNormal;
                                        MVector displayNormal;
                                        MStatus indexStatus;
                                        const int faceIndex = face.index(&indexStatus);
                                        if (!indexStatus || faceIndex < 0 ||
                                            meshFn.getFaceVertexNormal(
                                                faceIndex, vertexIds[vertex], vertexNormal) !=
                                                MS::kSuccess ||
                                            !transformDisplayNormal(vertexNormal, normalTransform,
                                                                    displayNormal) ||
                                            nOffset + 3U > dData.normBuffer.size()) {
                                            MGlobal::displayError(
                                                "DMeshDisplay: smooth normal is invalid.");
                                            return MS::kFailure;
                                        }
                                        dData.normBuffer[nOffset++] =
                                            static_cast<float>(displayNormal.x);
                                        dData.normBuffer[nOffset++] =
                                            static_cast<float>(displayNormal.y);
                                        dData.normBuffer[nOffset++] =
                                            static_cast<float>(displayNormal.z);
                                    }
                                }
                            }
                            return MS::kSuccess;
                        });
                        if (!stat) {
                            MGlobal::displayError("DMeshDisplay: invalid or incomplete face traversal.");
                            return;
                        }
                    }
                }

                vOffset += (*it)->numberOfPoints();
                ++processedMeshCount;
            }

            const bool expectsNormals = dData.isShade && (dData.displayType == DM_DPY_HARD ||
                                                          dData.displayType == DM_DPY_SMOOTH);
            std::size_t expectedColorScalarCount = 0U;
            std::size_t wirePositionScalarCount = 0U;
            if (processedMeshCount != meshCount || vOffset != totalVtx ||
                vIndex != pointScalarCount ||
                (dData.isShade && dData.mFaceIndexList.size() != allTriTotalVtx) ||
                (expectsNormals && nOffset != expandedPositionScalarCount) ||
                (!expectsNormals && !dData.normBuffer.empty()) ||
                !dommy::geometry::checkedTupleScalarCount(dData.mWireIndexList.size(), 3U,
                                                          wirePositionScalarCount) ||
                wirePositionScalarCount > dommy::viewport::kMaxViewportScalarCount ||
                (dData.colorType == DM_DISPLAY_COLOR_RANDOM && dData.isShade &&
                 (!dommy::geometry::checkedTupleScalarCount(totalVtx, 4U,
                                                            expectedColorScalarCount) ||
                  expectedColorScalarCount != dData.colBuffer.size() ||
                  colIndex != expectedColorScalarCount))) {
                MGlobal::displayError("DMeshDisplay: generated viewport layout is inconsistent.");
                return;
            }

            dData.pointCount = totalVtx;
            dData.primCount = meshCount;
        }
    }
} catch (...) {
    clearMeshDisplayGeometry(dData);
    mDataChanged = true;
    dommy::maya::reportViewportCallbackException("DMeshDisplayOverride", "updateDG",
                                                 std::current_exception());
}

void DMeshDisplayOverride::updateRenderItems(const MDagPath& path,
                                             MHWRender::MRenderItemList& list) try {
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DMeshDisplayOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dDefaultMaterialShader, mSolidUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mPointUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dSolidShader, mFlatUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mWireUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dCPVSolidShader, mColorMeshShader);

    // get attribute
    const float solidColor[4] = {dData.solidColor.r, dData.solidColor.g, dData.solidColor.b,
                                 1.0f}; // main color
    const float lineWidth[2] = {dData.lineWidth, dData.lineWidth};

    const bool isShaded =
        (dData.isShade && (dData.displayType == DM_DPY_HARD || dData.displayType == DM_DPY_SMOOTH))
            ? true
            : false;

    const bool isWired = dData.isWireframe;
    const bool isVertex = dData.isVertex;

    const bool isFlat = (dData.isShade && dData.displayType == DM_DPY_FLAT) ? true : false;

    const bool isColorRand = (dData.colorType == DM_DISPLAY_COLOR_RANDOM) ? true : false;

#pragma mark mesh base shader
    MHWRender::MRenderItem* solidDMesh = NULL;

    int index = list.indexOf(DMeshItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshItemName_, MHWRender::MRenderItem::DecorationItem,
                                           MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DMeshDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sDormantWireDepthPriority);
        if (!list.append(pendingItem.get())) return;
        solidDMesh = pendingItem.release();
    } else {
        solidDMesh = list.itemAt(index);
    }

    if (solidDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*solidDMesh, mShaderUpdateFailed);
        if (mSolidUIShader) {
            shaderUpdate.setParameter(mSolidUIShader, "diffuseColor", solidColor);
            shaderUpdate.setParameter(mSolidUIShader, "specularPower", 0.0f);
            shaderUpdate.setShader(mSolidUIShader, &DMeshItemName_);
        }
        shaderUpdate.enable(isShaded && dData.primCount && !isColorRand);
    }

#pragma mark vertex shader

    MHWRender::MRenderItem* pointDMesh = NULL;

    index = list.indexOf(DMeshPointItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshPointItemName_,
                                                    MHWRender::MRenderItem::DecorationItem,
                                                    MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DMeshDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sActiveLineDepthPriority);
        if (!list.append(pendingItem.get())) return;
        pointDMesh = pendingItem.release();
    } else {
        pointDMesh = list.itemAt(index);
    }

    if (pointDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*pointDMesh, mShaderUpdateFailed);
        if (mPointUIShader) {
            MColor wireColor = MHWRender::MGeometryUtilities::wireframeColor(path);
            const float wirefloat[4] = {wireColor.r, wireColor.g, wireColor.b, 1.0f};
            shaderUpdate.setParameter(mPointUIShader, "solidColor", wirefloat);
            float2 pSize = {dData.pointSize, dData.pointSize}; // point size
            shaderUpdate.setParameter(mPointUIShader, "pointSize", pSize);
            shaderUpdate.setShader(mPointUIShader, &DMeshPointItemName_);
        }

        shaderUpdate.enable(isVertex && dData.primCount);
    }

#pragma mark mesh flat shader
    MHWRender::MRenderItem* flatDMesh = NULL;

    index = list.indexOf(DMeshFlatItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshFlatItemName_,
                                                   MHWRender::MRenderItem::DecorationItem,
                                                   MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DMeshDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sDormantWireDepthPriority);
        if (!list.append(pendingItem.get())) return;
        flatDMesh = pendingItem.release();
    } else {
        flatDMesh = list.itemAt(index);
    }

    if (flatDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*flatDMesh, mShaderUpdateFailed);
        if (mFlatUIShader) {
            shaderUpdate.setParameter(mFlatUIShader, "solidColor", solidColor);
            shaderUpdate.setShader(mFlatUIShader, &DMeshFlatItemName_);
        }
        shaderUpdate.enable(isFlat && dData.primCount && !isColorRand);
    }

#pragma mark wireframe shader
    MHWRender::MRenderItem* wireDMesh = NULL;

    index = list.indexOf(DMeshWireItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshWireItemName_,
                                                   MHWRender::MRenderItem::DecorationItem,
                                                   MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DMeshDisplayOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        pendingItem->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);
        if (!list.append(pendingItem.get())) return;
        wireDMesh = pendingItem.release();
    } else {
        wireDMesh = list.itemAt(index);
    }

    if (wireDMesh) {
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*wireDMesh, mShaderUpdateFailed);
        if (mWireUIShader) {

            MColor wireColor = MHWRender::MGeometryUtilities::wireframeColor(path);
            const float wirefloat[4] = {wireColor.r, wireColor.g, wireColor.b, 1.0f};
            shaderUpdate.setParameter(mWireUIShader, "solidColor", wirefloat);
            shaderUpdate.setParameter(mWireUIShader, "lineWidth", lineWidth);

            shaderUpdate.setShader(mWireUIShader, &DMeshWireItemName_);
        }
        shaderUpdate.enable(isWired && dData.primCount);
    }

#pragma mark mesh color shader
    MHWRender::MRenderItem* colorDMesh = NULL;

    index = list.indexOf(DMeshColorItemName_);

    if (index < 0) {
        dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DMeshColorItemName_,
                                                    MHWRender::MRenderItem::DecorationItem,
                                                    MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DMeshDisplayOverride"});
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
        shaderUpdate.enable(dData.primCount != 0U && isColorRand && !dData.colBuffer.empty() &&
                           dData.isShade);
    }
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH("DMeshDisplayOverride", "updateRenderItems")

void DMeshDisplayOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                                            const MHWRender::MRenderItemList& renderItems,
                                            MHWRender::MGeometry& data) try {
    mDataChanged = true;
    // cvs
    MHWRender::MVertexBuffer* verticesBuffer = NULL;
    float* vertices = NULL;
    MHWRender::MVertexBuffer* normalBuffer = NULL;
    float* normals = NULL;

    // point
    MHWRender::MVertexBuffer* ptVtxBuffer = NULL;
    float* ptVertices = NULL;

    // wire
    MHWRender::MVertexBuffer* wireVtxBuffer = NULL;
    float* wireVertices = NULL;

    // color
    MHWRender::MVertexBuffer* colBuffer = NULL;
    float* colorvalues = NULL;
    std::size_t facePositionScalarCount = 0U;
    std::size_t wirePositionScalarCount = 0U;
    std::size_t expandedColorScalarCount = 0U;
    if (!dommy::geometry::checkedTupleScalarCount(dData.mFaceIndexList.size(), 3U,
                                                  facePositionScalarCount) ||
        facePositionScalarCount > dommy::viewport::kMaxViewportScalarCount ||
        !dommy::geometry::checkedTupleScalarCount(dData.mWireIndexList.size(), 3U,
                                                  wirePositionScalarCount) ||
        wirePositionScalarCount > dommy::viewport::kMaxViewportScalarCount ||
        !dommy::geometry::checkedTupleScalarCount(dData.mFaceIndexList.size(), 4U,
                                                  expandedColorScalarCount) ||
        expandedColorScalarCount > dommy::viewport::kMaxViewportScalarCount) {
        return;
    }

    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& vertexBufferDescriptor) -> bool {
            switch (vertexBufferDescriptor.semantic()) {
            case MHWRender::MGeometry::kPosition: {

                if (vertexBufferDescriptor.name() == DMeshItemName_ ||
                    vertexBufferDescriptor.name() == DMeshColorItemName_ ||
                    vertexBufferDescriptor.name() == DMeshFlatItemName_) // mesh position
                {
                    if (!verticesBuffer && !dData.mFaceIndexList.empty()) {
                        verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!verticesBuffer) return false;

                        if (verticesBuffer) {

                            vertices = dommy::viewport::acquireFloatBuffer(
                                verticesBuffer, vertexBufferDescriptor, facePositionScalarCount, 3);
                        }
                    }
                } else if (vertexBufferDescriptor.name() == DMeshWireItemName_) // wireframe position
                {
                    if (!wireVtxBuffer && !dData.mWireIndexList.empty()) {
                        wireVtxBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!wireVtxBuffer) return false;

                        if (wireVtxBuffer) {

                            wireVertices = dommy::viewport::acquireFloatBuffer(
                                wireVtxBuffer, vertexBufferDescriptor, wirePositionScalarCount, 3);
                        }
                    }
                } else if (vertexBufferDescriptor.name() == DMeshPointItemName_) // point
                {
                    if (!ptVtxBuffer && dData.pointCount != 0U) {
                        ptVtxBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!ptVtxBuffer) return false;
                        if (ptVtxBuffer) {

                            ptVertices = dommy::viewport::acquireFloatBuffer(
                                ptVtxBuffer, vertexBufferDescriptor, dData.mPointBuffer.size(), 3);
                        }
                    }
                }

            } // kPoint
            break;
            case MHWRender::MGeometry::kTexture: {

            } break;
            case MHWRender::MGeometry::kColor: {

                if ((vertexBufferDescriptor.name() == DMeshColorItemName_)) {
                    if (!colBuffer && !dData.colBuffer.empty() && !dData.mFaceIndexList.empty()) {
                        colBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!colBuffer) return false;
                        if (colBuffer) {

                            colorvalues = dommy::viewport::acquireFloatBuffer(
                                colBuffer, vertexBufferDescriptor, expandedColorScalarCount, 4);
                        }
                    }
                }
            } break;
            case MHWRender::MGeometry::kNormal: {

                if (vertexBufferDescriptor.name() == DMeshItemName_) {
                    if (!normalBuffer && !dData.normBuffer.empty()) {
                        normalBuffer = data.createVertexBuffer(vertexBufferDescriptor);
                        if (!normalBuffer) return false;
                        if (normalBuffer) {

                            normals = dommy::viewport::acquireFloatBuffer(
                                normalBuffer, vertexBufferDescriptor, dData.normBuffer.size(), 3);
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

    if ((verticesBuffer && !vertices) || (normalBuffer && !normals) ||
        (ptVtxBuffer && !ptVertices) || (wireVtxBuffer && !wireVertices) ||
        (colBuffer && !colorvalues)) {
        return;
    }

#pragma mark commit position

    if (!dData.mPointBuffer.empty()) {

        if (vertices) {
            size_t i = 0;
            for (auto fIdx : dData.mFaceIndexList) {
                const std::size_t idxOffset = static_cast<std::size_t>(fIdx) * 3U;
                vertices[i++] = dData.mPointBuffer[idxOffset];
                vertices[i++] = dData.mPointBuffer[idxOffset + 1];
                vertices[i++] = dData.mPointBuffer[idxOffset + 2];
            }
        }

        if (verticesBuffer && vertices) {
            verticesBuffer->commit(vertices);
        }
    }

    if (dData.normBuffer.size()) {
        if (normals) {
            size_t indexOffset = 0;
            for (auto it : dData.normBuffer) {
                normals[indexOffset++] = it;
            }
        }

        if (normalBuffer && normals) {
            normalBuffer->commit(normals);
        }
    }

#pragma mark point

    if (dData.isVertex) {
        if (ptVertices) {
            size_t indexOffset = 0;

            for (auto p : dData.mPointBuffer) {
                ptVertices[indexOffset++] = p;
            }
        }

        if (ptVtxBuffer && ptVertices) {
            ptVtxBuffer->commit(ptVertices);
        }
    }

#pragma mark wire
    if (dData.mPointBuffer.size() && dData.mWireIndexList.size()) {
        // wire normal
        if (wireVertices) {
            size_t i = 0;

            for (auto fIdx : dData.mWireIndexList) {
                const std::size_t idxOffset = static_cast<std::size_t>(fIdx) * 3U;
                wireVertices[i++] = dData.mPointBuffer[idxOffset];
                wireVertices[i++] = dData.mPointBuffer[idxOffset + 1];
                wireVertices[i++] = dData.mPointBuffer[idxOffset + 2];
            }
        }

        if (wireVtxBuffer && wireVertices) {
            wireVtxBuffer->commit(wireVertices);
        }
    }

#pragma mark color shder commit

    if (dData.colBuffer.size()) {
        if (colorvalues) {
            size_t i = 0;
            for (auto fIdx : dData.mFaceIndexList) {
                const std::size_t idxOffset = static_cast<std::size_t>(fIdx) * 4U;
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

#pragma mark index commit

    for (int idx = 0; idx < renderItems.length(); ++idx) {
        const MHWRender::MRenderItem* item = renderItems.itemAt(idx);

        if (!item)
            continue;

        std::size_t numIndex = 0U;
        if (item->name() == DMeshItemName_ || item->name() == DMeshColorItemName_ ||
            item->name() == DMeshFlatItemName_) // mesh
        {
            if (dData.mFaceIndexList.size()) {
                numIndex = dData.mFaceIndexList.size();

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
        } else if (item->name() == DMeshWireItemName_) // wire
        {
            if (dData.mWireIndexList.size()) {
                numIndex = dData.mWireIndexList.size();

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
        } else if (item->name() == DMeshPointItemName_) // point
        {
            if (dData.isVertex && dData.pointCount) {
                numIndex = dData.pointCount;

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
        }
    }
    mDataChanged = false;
}
DOMMY_VIEWPORT_VOID_CATCH("DMeshDisplayOverride", "populateGeometry")
