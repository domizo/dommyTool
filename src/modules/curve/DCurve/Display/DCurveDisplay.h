//
//  DMultiCurves.hpp
//  dommyTool
//
//  Created by yuzo on 4/30/16.
//
//

#pragma once


#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxLocatorNode.h>

// Viewport 2.0 includes
#include <maya/MDrawRegistry.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>
#include <maya/MShaderManager.h>
#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MGlobal.h>


// Viewport 2.0 includes



#include <vector>
#include <string>
#include <algorithm>

#include "DPointDisplay.h"


class DCurveDisplay final : public MPxLocatorNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    
    void draw( M3dView & view, const MDagPath & path,
               M3dView::DisplayStyle style,
               M3dView::DisplayStatus status ) override;
    
    bool isBounded() const override;
    MBoundingBox boundingBox() const override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static MTypeId typeId;
    static const MString typeName;
	static MString classification;
	static MString registrantId;
	
	
	
	// attributes
	static MObject	inDC;
    
    static MObject aIsRender;
    static MObject aiOpaque;
    static MObject aiSelfShadows;
    static MObject aiMatte;
    
	static MObject aLineWidth;
	static MObject aColorType;
	static MObject aSolidColor;
	
	
	
	static MObject aNodeInfo;
    
	//for render
    static MObject aIsStaticObject;
	static MObject aIsMultiEndKnot;
	
	static MObject aCurveWidthType;
	static MObject aConstantWidth;
	static MObject aWidthAttributeName;
	
	static MObject aExportCurveAttribute;
	static MObject aExportPointAttribute;
    
	//for arnold
	
	static MObject aRenderMode;
	static MObject aBasisMode;
	static MObject aMinPixelWidth;
	
	static MObject aOrientType;
	static MObject aOrientAttributeName;
	
	static MObject aCurveShader;
	
	
};




//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------





class DCurveDisplayOverride final : public MHWRender::MPxGeometryOverride
{
public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DCurveDisplayOverride", [&]() {
            return new DCurveDisplayOverride(obj);
        });
    }
	    ~DCurveDisplayOverride() override;
	    
	    MHWRender::DrawAPI supportedDrawAPIs() const override;
	
	
	 
		bool hasUIDrawables() const override { return false; }
		
		void updateDG() override;
	bool isIndexingDirty(const MHWRender::MRenderItem &) override { return true; }
	bool isStreamDirty(const MHWRender::MVertexBufferDescriptor &) override { return mDataChanged || mShaderUpdateFailed; }
		void updateRenderItems(const MDagPath &path, MHWRender::MRenderItemList& list) override;
		void populateGeometry(const MHWRender::MGeometryRequirements &requirements, const MHWRender::MRenderItemList &renderItems, MHWRender::MGeometry &data) override;
		void cleanUp() override {}
	
	
		bool traceCallSequence() const override
	{
		// Return true if internal tracing is desired.
		return false;
	}
		void handleTraceMessage( const MString &message ) const override
	{
		MGlobal::displayInfo("DCurveDisplayOverride: " + message);
		
	}
	
	struct DrawData {
		
		float pointSize = 3.0f;
		float lineWidth = 2.0f;
		
		short colorType = 0;
		MColor solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
		
		std::vector<float> colBuffer;
        std::vector<float> cvsBuffer;
		std::vector<unsigned int> indexBuffer;

		size_t primCount = 0U;
		size_t pointCount = 0U;
		
	};

private:
    DCurveDisplayOverride(const MObject& obj);
	MHWRender::MShaderInstance* mSolidUIShader;
	MHWRender::MShaderInstance* mColorLineShader;
	
	
	MObject mCurveDisplayNode;
	bool mDataChanged;
	bool mShaderUpdateFailed = false;
	
	DrawData dData;
};
