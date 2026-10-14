//
//  DPLightGen.hpp
//  dommyTool
//
//  Created by yuzo on 11/16/16.
//
//

#pragma once

#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxLocatorNode.h>
#include <maya/MGlobal.h>
#include <maya/MArgList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MPointArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MVectorArray.h>
#include <maya/MDoubleArray.h>

// Viewport 2.0 includes
#include <maya/MDrawRegistry.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MObjectHandle.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MEventMessage.h>



#include <algorithm>
#include <vector>
#include <string>

#include "DPData.h"
#include "DPLightTransform.h"

class DPLightGen final : public MPxLocatorNode
{
public:
	DPLightGen();
	~DPLightGen() override;
	
	MStatus compute( const MPlug& plug, MDataBlock& data ) override;
	SchedulingType schedulingType() const override { return kGloballySerialize; }
	
	void draw( M3dView & view, const MDagPath & path,
					  M3dView::DisplayStyle style,
					  M3dView::DisplayStatus status ) override;
	
	bool isBounded() const override;
	MBoundingBox boundingBox() const override;
	
	static void *creator();
	static MStatus initialize();
	
	static MTypeId typeId;
	static const MString typeName;
	static	MString classification;
	static	MString registrantId;
	
	
	
	MStatus exportArnoldLight(
		MDataBlock& data,
		const DPData* inputDPData,
		const std::string& filename);
	
	
    
    //Attribute
	
	static MObject inDP;
	
	static MObject aNodeInfo;
	
	
	static MObject aFilePath;
	static MObject aExportMode;
	static MObject aExportLog;
    
    static MObject aLightVisType;
	static MObject aColorVis;
	static MObject aIntensityVis;
    static MObject aIntensityNormalize;
    static MObject aPointSize;
    
    //type
    static MObject aLightType;
    
    
    
    //arnold
    //diatant
    static MObject aDistantAngle;
    
    //spot
    static MObject aLensRadius;
    static MObject aConeAngle;
    static MObject aPenumbraAngle;
    static MObject aCosinePower;
    static MObject aAspectRatio;
	
	static MObject aUseGobo;
	static MObject aGoboTexture;
	static MObject aGoboFilter;
	static MObject aGoboDensity;
	static MObject aGoboOffset;
	static MObject aGoboScale;
    static MObject aGoboWrap;
    
    //area quad cylinder disk
    static MObject aResolution;
    static MObject aSpread; //disk
    static MObject aLightTexture;
	static MObject aRepeatTex;
    static MObject aOffsetTex;
	
    //photometric
    static MObject aPhotometricFileName;
    
    
    //common
    static MObject aIntensity;
    static MObject aExposure;
    static MObject aColor;
    
    
    static MObject aDecayType;
    static MObject aSamples;
    static MObject aLightRadius;//point spot photometric disk cylinder
    static MObject aNormalize;
    
    
    static MObject aCastShadow;
    static MObject aShadowColor;
    static MObject aShadowDensity;
    
    static MObject aAffectVolumetrics;
    static MObject aCastVolumeShadow;
    static MObject aVolumeSamples;
    
    static MObject aAffectDiffuse;
    static MObject aAffectSpecular;
    static MObject aDiffuse;
    static MObject aSpecular;
    static MObject aSss;
    static MObject aIndirect;
    static MObject aVolume;
    
    static MObject aMaxBounces;
    
    static MObject aAOVLightGroup;
    
    
    //direction
    static MObject aDirectionType;
    static MObject aBaseDirection;
    static MObject aDirectionAttribute;
    
    //attribute inherit
    static MObject aIntensityAttribute;
    static MObject aColorAttribute;
    static MObject aSpotAngleAttribute;
    static MObject aScaleAttribute;
    static MObject aAnimMatrixAttribute;
    static MObject aApplyAnimMatType;
	static MObject aAnimLookPos;
	
    //transform
    static MObject aGlobalScale;
    
};




// Draw override class for drawing manip in VP2.0
//
class DPLightGenData final : public MUserData
{
public:
	DPLightGenData() : MUserData(false) {} // don't delete after draw
	~DPLightGenData() override = default;
	
    int lightType = -1;
    MMatrixArray matrixArray;
    MDoubleArray intensityArray;
    MDoubleArray spotAngleArray;
    MVectorArray colorArray;
	
	bool useConeAngleVis = false;
	
	int lightVisType = 2;
	bool colorVis = false;
    bool intensityVis = false;
    double intensityNormalizeVal = 1.0;
    float pointSize = 3.0f;
};

class DPLightGenOverride final : public MHWRender::MPxDrawOverride
{
public:
    static MHWRender::MPxDrawOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DPLightGenOverride", [&]() {
            return new DPLightGenOverride(obj);
        });
    }
	
	~DPLightGenOverride() override;
	
	MHWRender::DrawAPI supportedDrawAPIs() const override;
	
	bool isBounded(
						   const MDagPath& objPath,
						   const MDagPath& cameraPath) const override;
	
	MBoundingBox boundingBox(
									 const MDagPath& objPath,
									 const MDagPath& cameraPath) const override;
	
	MUserData* prepareForDraw(
									  const MDagPath& objPath,
									  const MDagPath& cameraPath,
									  const MHWRender::MFrameContext& frameContext,
									  MUserData* oldData) override;
	
	bool hasUIDrawables() const override { return true; }
	
	void addUIDrawables(
								const MDagPath& objPath,
								MHWRender::MUIDrawManager& drawManager,
								const MHWRender::MFrameContext& frameContext,
								const MUserData* data) override;
	
	bool traceCallSequence() const override
	{
		// Return true if internal tracing is desired.
		return false;
	}
	void handleTraceMessage( const MString &message ) const override
	{
		MGlobal::displayInfo("DPLightGenOverride: " + message);
		
	}
	
	
private:
	DPLightGenOverride(const MObject& obj);
//	float getMultiplier(const MDagPath& objPath) const;
	
	static void OnModelEditorChanged(void *clientData);
	
	MObjectHandle mNodeHandle;
	MCallbackId fModelEditorChangedCbId;
};
