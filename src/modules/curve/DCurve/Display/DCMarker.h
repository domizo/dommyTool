//
//  DCMarker.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/10.
//

//DCurveの情報のみを表示

#pragma once

#include "common/utility/DMayaEntryPointGuard.h"
#include <vector>
#include <string>
#include <algorithm>


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



#include "DPointDisplay.h"
#include "DGroupCommon.h"

class DCMarker final : public MPxLocatorNode
{
public:
    DCMarker();
    ~DCMarker() override;
    
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
    static MObject    inDC;
    
    static MObject aIsCenterVisible;
    
    static MObject aShowIndex;
    static MObject aIndexType;
    
    
    static MObject aLineWidth;
    static MObject aSolidColor;
    
	    static MObject aAllAttributeNames;
	    static MObject aIsColor;
	    static MObject aIsValue;
	    static MObject aIsVector;
	    static MObject aVisibleAttributeName;
    
    static MObject aIsBlackTransparency;
    static MObject aIsDrawHeads;
    static MObject aVelocityScale;
    
    //group
	    static MObject aAllGroupNames;
	    static MObject aIsGroup;
	    static MObject aGroupName;
    static MObject aGroupColor;
    static MObject aGroupTransparency;
    static MObject aPointSize;
    
    static MObject aNodeInfo;
	};




//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



class DCMarkerOverride final : public MHWRender::MPxGeometryOverride
{
public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DCMarkerOverride", [&]() {
            return new DCMarkerOverride(obj);
        });
    }
	    ~DCMarkerOverride() override;
	    
	    MHWRender::DrawAPI supportedDrawAPIs() const override;
    
    
    
	    bool hasUIDrawables() const override { return false; }
	    
	    void updateDG() override;
    bool isIndexingDirty(const MHWRender::MRenderItem &) override { return true;}
    bool isStreamDirty(const MHWRender::MVertexBufferDescriptor &) override {  return mDataChanged || mShaderUpdateFailed; }
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
        MGlobal::displayInfo("DCMarkerOverride: " + message);
        
    }
    
    struct DrawData {
        
        
        
        //center data
        bool isCenterVis = false;
        std::vector<float> centerPtBuffer;
        
        
        
        float pointSize = 5.0f;
        float lineWidth = 2.0f;
        MColor solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
        
        bool showIndex = false;
        short indexType = 0;
        std::vector<size_t> crvFirstIndex;//for index
        
        std::vector<float> colBuffer;
        std::vector<float> vecBuffer;
        std::vector<float> val3Buffer;
        
        std::vector<float> cvsBuffer;
        std::vector<unsigned int> indexBuffer;
        
        size_t primCount = 0U;
        size_t pointCount = 0U;
        
        
        bool isColor = false;
        bool isValue = false;
        bool isVector = false;
        
        short infoClass = 0;
        bool isDrawHeads = false;
        bool isBlackTransparency = false;
        float velocityScale = 1.0f;
        
        
        DP_DISPLAY_VALUE_TYPE valType = DP_VAL_FLOAT;
        
        
        
        
        //Group
        bool isGroup = false;
        DComponentType grpType = D_COMP_POINT;
        MColor groupColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
        float groupAlpha = 0.7f;
        
        //Display Group Data
        std::vector<float> grpPtBuffer;
        std::vector<unsigned int> grpIndexBuffer;
        size_t totalGroupPointCount = 0U;
        
        
        
    };
    
private:
    
    DCMarkerOverride(const MObject& obj);
    
    
    MHWRender::MShaderInstance* mCenterUIShader;
    MHWRender::MShaderInstance* mIndexUIShader;
    MHWRender::MShaderInstance* mColorLineShader;
    
    MHWRender::MShaderInstance* mVectorUIShader;
    MHWRender::MShaderInstance* mValueUIShader;
    MHWRender::MShaderInstance* mValue2UIShader;
    MHWRender::MShaderInstance* mValue3UIShader;
    
    //group shader
    MHWRender::MShaderInstance* mGroupLineShader;
    MHWRender::MShaderInstance* mGroupPointShader;
    
    MObject mDGNode;
    bool mDataChanged;
    bool mShaderUpdateFailed = false;
    
    DrawData dData;
    
};
