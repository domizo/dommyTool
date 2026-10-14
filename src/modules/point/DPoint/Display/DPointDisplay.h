//
//  DPointDisplayNode.h
//  dommyTool
//
//  Created by yuzo on 2015/03/15.
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

// Viewport 2.0 includes
#include <maya/MDrawRegistry.h>
#include <maya/MDrawContext.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MUserData.h>
#include <maya/MShaderManager.h>
#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>
#include <algorithm>

#include <vector>
#include <string>

enum {
	DP_DISPLAY_NONE = 0,
	DP_DISPLAY_VALUE,
	DP_DISPLAY_COLOR,
	DP_DISPLAY_VECTOR,
	DP_DISPLAY_INFO_TYPE_COUNT
};typedef short DP_DISPLAY_INFO_TYPE;

enum {
	DP_VAL_FLOAT = 0,
	DP_VAL_FLOAT2,
	DP_VAL_FLOAT3,
	DP_VAL_INT,
	DP_VAL_INT2,
	DP_VAL_INT3,
	DP_DISPLAY_VALUE_TYPE_COUNT
};typedef short DP_DISPLAY_VALUE_TYPE;


class DPointDisplay final : public MPxLocatorNode
{
public:
    DPointDisplay();
    ~DPointDisplay() override;
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    
    void draw( M3dView & view, const MDagPath & path,
               M3dView::DisplayStyle style,
               M3dView::DisplayStatus status ) override;
    
	bool isBounded() const override;
	MBoundingBox boundingBox() const override;
	
	MSelectionMask getShapeSelectionMask() const override;
	
    static void *creator();
    static MStatus initialize();
    
    static MTypeId typeId;
	static const MString typeName;
	static	MString classification;
	static	MString registrantId;
	
	
	static MObject inDP;
	
	
	
    static MObject aPointSize;
    static MObject aSolidColor;
    
	static MObject aNodeInfo;
    
    static MObject aCachedDObject;
	
	
	//for render
    static MObject aIsRender;
	static MObject aIsStaticObject;
    
    static MObject aiOpaque;
    static MObject aiMatte;
    static MObject aiSelfShadows;
    static MObject aiReceiveShadows;
    
	//point
	static MObject aPointWidthType;
	static MObject aConstantWidth;
	static MObject aWidthAttributeName;
	
    
    static MObject aVelocityAttributeName;//for alembic etc
    
    
	
	static MObject aExportPointAttribute;
	static MObject aExportAnimPointAttribute;
	
	//for arnold
	
	static MObject aRenderMode;
	
	static MObject aPointShader;
	
};

class DPointDisplayData : public MUserData
{
public:
    DPointDisplayData();
    virtual ~DPointDisplayData() {};
    
    
    bool showIndex;
    MColor fColor;
    
    float fLineWidth;
    
    // point
    float fPointSize;
    
    bool isShowVal;
    bool isShowCol;
    bool isShowVec;
    
    
    bool useGrp;
};


// Draw override class for drawing manip in VP2.0
//
class DPointDisplayOverride final : public MHWRender::MPxGeometryOverride
{
public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DPointDisplayOverride", [&]() {
            return new DPointDisplayOverride(obj);
        });
    }
//    static MPxDrawOverride* Creator(const MObject& obj)
//    {
//    }
	
	~DPointDisplayOverride() override;
	
	MHWRender::DrawAPI supportedDrawAPIs() const override;
	
//	virtual bool isBounded( const MDagPath& objPath, const MDagPath& cameraPath) const;
//	virtual MBoundingBox boundingBox( const MDagPath& objPath, const MDagPath& cameraPath) const;
	
	
    bool hasUIDrawables() const override { return false; }
	
    void updateDG() override;
    bool isIndexingDirty(const MHWRender::MRenderItem &item) override {
        (void)item;
        return true;
    }
    bool isStreamDirty(const MHWRender::MVertexBufferDescriptor &desc) override {
        (void)desc;
        return mMultiplierChanged || mShaderUpdateFailed;
    }
    void updateRenderItems(const MDagPath &path, MHWRender::MRenderItemList& list) override;
    void populateGeometry(const MHWRender::MGeometryRequirements &requirements,
                          const MHWRender::MRenderItemList &renderItems,
                          MHWRender::MGeometry &data) override;
    void cleanUp() override {}
	
	
	bool traceCallSequence() const override
	{
		// Return true if internal tracing is desired.
		return false;
	}
	void handleTraceMessage( const MString &message ) const override
	{
		MGlobal::displayInfo("DPointDisplayOverride: " + message);
		
	}
	
	struct DrawData {
        
		float pointSize = 3.0f;
		MColor solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
		
		std::vector<float> ptBuffer;
		unsigned int pointCount = 0U;
        
	};
	
	
private:
    DPointDisplayOverride(const MObject& obj);
    MHWRender::MShaderInstance* mSolidUIShader;

    MObject mDGNode;
//	MPointArray ptArray;
    bool mMultiplierChanged;
    bool mShaderUpdateFailed = false;
    
	DrawData dData;
	
};
