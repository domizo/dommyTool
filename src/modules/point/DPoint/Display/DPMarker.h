//
//  DPMarker.hpp
//  dommyTool
//
//  Created by yuzo on 2018/01/18.
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


#include "DPointDisplay.h"
#include "DType.h"



class DPMarker final : public MPxLocatorNode
{
public:
    DPMarker();
    ~DPMarker() override;
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    
    void draw( M3dView & view, const MDagPath & path,
               M3dView::DisplayStyle style,
               M3dView::DisplayStatus status ) override;
    
    bool isBounded() const override;
    MBoundingBox boundingBox() const override;
    
//    virtual MSelectionMask getShapeSelectionMask() const;
    
    static void *creator();
    static MStatus initialize();
    
    static MTypeId typeId;
    static const MString typeName;
    static    MString classification;
    static    MString registrantId;
    
    
    static MObject inDP;
    
    
    static MObject aShowIndex;
    
    static MObject aPointSize;
    static MObject aSolidColor;
    
    static MObject aNodeInfo;
    
    
    
    //attribute
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
    
};

class DPMarkerData : public MUserData
{
public:
    DPMarkerData();
    virtual ~DPMarkerData() {};
    
    
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
class DPMarkerOverride final : public MHWRender::MPxGeometryOverride
{
public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DPMarkerOverride", [&]() {
            return new DPMarkerOverride(obj);
        });
    }
    
    ~DPMarkerOverride() override;
    
    MHWRender::DrawAPI supportedDrawAPIs() const override;
    
    //    virtual bool isBounded( const MDagPath& objPath, const MDagPath& cameraPath) const;
    //    virtual MBoundingBox boundingBox( const MDagPath& objPath, const MDagPath& cameraPath) const;
    
    
    bool hasUIDrawables() const override { return false; }
    
    void updateDG() override;
    bool isIndexingDirty(const MHWRender::MRenderItem&) override { return true; }
    bool isStreamDirty(
        const MHWRender::MVertexBufferDescriptor&) override {
        return mMultiplierChanged || mShaderUpdateFailed;
    }
    void updateRenderItems(
        const MDagPath& path,
        MHWRender::MRenderItemList& list) override;
    void populateGeometry(
        const MHWRender::MGeometryRequirements& requirements,
        const MHWRender::MRenderItemList& renderItems,
        MHWRender::MGeometry& data) override;
    void cleanUp() override {}
    
    
    bool traceCallSequence() const override
    {
        // Return true if internal tracing is desired.
        return false;
    }
    void handleTraceMessage(const MString& message) const override
    {
        MGlobal::displayInfo("DPMarkerOverride: " + message);
        
    }
    
    struct DrawData {
        
        float pointSize = 3.0f;
        MColor solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);
        
        bool showIndex = false;
        
        
        //Point Data
        std::vector<float> ptBuffer;
        
        //Attribute Data
        std::vector<float> colBuffer;
        std::vector<float> vecBuffer;
        std::vector<float> val3Buffer;
        
        
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
        float groupAlpha = 1.0f;
        
        
        //Group Data
        std::vector<float> grpPtBuffer;
        size_t totalGroupPointCount = 0U;
        
    };
    
    
private:
    
    DPMarkerOverride(const MObject& obj);
    
    MHWRender::MShaderInstance* mIndexUIShader;
    MHWRender::MShaderInstance* mColorUIShader;
    MHWRender::MShaderInstance* mVectorUIShader;
    
    MHWRender::MShaderInstance* mValueUIShader;
    MHWRender::MShaderInstance* mValue2UIShader;
    MHWRender::MShaderInstance* mValue3UIShader;
    
    
    //group shader
    MHWRender::MShaderInstance* mGroupPointShader;
    
    
    MObject mDGNode;
    
    
    bool mMultiplierChanged;
    
    
    bool mShaderUpdateFailed = false;
    
    DrawData dData;
    
};
