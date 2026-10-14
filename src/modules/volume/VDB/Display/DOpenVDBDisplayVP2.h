//
//  DOpenVDBDisplay.hpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#pragma once

#include "common/utility/DMayaEntryPointGuard.h"
#include <vector>

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include <openvdb/io/Stream.h>

#include <maya/MPxLocatorNode.h>
#include <maya/MBoundingBox.h>
#include <maya/MColor.h>

// Viewport 2.0 includes
#include <maya/MDrawRegistry.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>
#include <maya/MShaderManager.h>
#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MPointArray.h>

class DOpenVDBDisplayVP2 final : public MPxLocatorNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    void draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;
    bool isBounded() const override;
    MBoundingBox boundingBox() const override;
    SchedulingType schedulingType() const override {
        return kSerial;
    }

    static void* creator();
    static MStatus initialize();

    static const MString typeName;
    static MTypeId typeId;
    static MString classification;
    static MString registrantId;

    static MObject aVdbInput;
    static MObject aVdbAllGridNames;
    static MObject aVdbSelectedGridNames;

    static MObject aVisualizeBBox;
    static MObject aVisualizeInternalNodes;
    static MObject aVisualizeLeafNodes;
    static MObject aVisualizeActiveTiles;
    static MObject aVisualizeActiveVoxels;
    static MObject aVisualizeSurface;
    static MObject aIsovalue;
    static MObject aLowDetail;
    static MObject aPointSize;
    static MObject aColorType;
    static MObject aInvertSurface;

    static MObject aIsClip;
    static MObject aClipAxis;
    static MObject aClipPosition;
    static MObject aClipWidth;

    static MObject aDisplayVector;
    static MObject aVectorScale;
    static MObject aIsDrawHeads;
    static MObject aVectorColor;

    static MObject aNodeInfo;

    // render attribute

    // common
    static MObject aiOpaque;
    static MObject aiMatte;
    static MObject aiSelfShadows;
    static MObject aiReceiveShadows;
    static MObject aSidedness;
    static MObject aInvertNormals;
    static MObject aRayBias;

    // volume
    static MObject aVolumeMode;
    // volume
    static MObject aStepSize;
    static MObject aStepScale;
    static MObject aVolumePadding;
    static MObject aIsCompress;

    // implicit surface
    static MObject aMinBox;
    static MObject aMaxBox;
    static MObject aFieldChannel;
    static MObject aSamples;
    static MObject aSolver;

    static MObject aExportGrid;
    static MObject aExportVelocityGrid;
    static MObject aVelocityScale;
    static MObject aVelocityFps;
    static MObject aVelocityShutterStart;
    static MObject aVelocityShutterEnd;
    static MObject aVelocityThreshold;

    static MObject aVdbShader;

  private:
};

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

class DOpenVDBDisplayVP2Override final : public MHWRender::MPxGeometryOverride {
  public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj) {
        return ::dommy::maya::createObject("DOpenVDBDisplayVP2Override", [&]() {
            return new DOpenVDBDisplayVP2Override(obj);
        });
    }

    ~DOpenVDBDisplayVP2Override() override;

    MHWRender::DrawAPI supportedDrawAPIs() const override;

    bool hasUIDrawables() const override {
        return false;
    }

    void updateDG() override;
    bool isIndexingDirty(const MHWRender::MRenderItem& item) override {
        (void)item;
        return true;
    }
    bool isStreamDirty(const MHWRender::MVertexBufferDescriptor& descriptor) override {
        (void)descriptor;
        return mMultiplierChanged || mShaderUpdateFailed;
    }
    void updateRenderItems(const MDagPath& path, MHWRender::MRenderItemList& list) override;
    void populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                          const MHWRender::MRenderItemList& renderItems,
                          MHWRender::MGeometry& data) override;
    void cleanUp() override;

  private:
    DOpenVDBDisplayVP2Override(const MObject& obj);
    void clearGeometryBuffers();
    void clearViewportState();

    MHWRender::MShaderInstance* mSolidUIShader;
    MHWRender::MShaderInstance* mVolumeUIShader;
    MHWRender::MShaderInstance* mVectorUIShader;

    MHWRender::MShaderInstance* bboxUIShader;
    MHWRender::MShaderInstance* nodeUIShader;
    MHWRender::MShaderInstance* leafUIShader;
    MHWRender::MShaderInstance* tileUIShader;

    MObject mLocatorNode;
    bool mMultiplierChanged;
    bool mShaderUpdateFailed = false;

    bool visBBox;
    bool visNode;
    bool visLeaf;
    bool visTile;
    bool visSurf;
    bool visPoint;
    bool visVector;

    // Evaluated only in updateDG; render callbacks consume this cached snapshot.
    float mPointSize = 2.0f;
    float mVectorScale = 1.0f;
    bool mDrawHeads = true;
    MColor mVectorColor = MColor(1.0f, 1.0f, 0.0f, 1.0f);

    std::vector<lineBufferObjectVP2> mBBoxBuffersVP2;
    std::vector<lineBufferObjectVP2> mNodeBuffersVP2;
    std::vector<lineBufferObjectVP2> mLeafBuffersVP2;
    std::vector<lineBufferObjectVP2> mTileBuffersVP2;

    std::vector<valueBufferObjectVP2> mPointBuffersVP2;
    std::vector<valueBufferObjectVP2> mVectorBuffersVP2;
    std::vector<BufferObjectVP2> mSurfaceBuffersVP2;

};
