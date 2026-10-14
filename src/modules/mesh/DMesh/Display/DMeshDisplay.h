//
//  DMeshDisplay.hpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
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


#include <vector>
#include <string>
#include <algorithm>

#include "DPointDisplay.h"
#include "DPointDataHeaders.h"


//using namespace MHWRender;


enum {
	DM_DPY_FLAT = 0,
	DM_DPY_HARD,
    DM_DPY_SMOOTH,
    DM_DISPLAY_TYPE_COUNT
};typedef short DM_DISPLAY_TYPE;

enum {
    DM_DISPLAY_COLOR_SOLID = 0,
    DM_DISPLAY_COLOR_RANDOM,
    DM_DISPLAY_COLOR_TYPE_COUNT
};typedef short DM_DISPLAY_COLOR_TYPE;

class DMeshDisplay final : public MPxLocatorNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& data ) override;

    void draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;

    bool isBounded() const override;
    MBoundingBox boundingBox() const override;

    static void* creator();
    static MStatus initialize();

    static MTypeId typeId;
    static const MString typeName;
    static MString classification;
    static MString registrantId;

    // attributes
    static MObject inDM;

    static MObject aIsWireFrame;
    static MObject aIsVertex;
    static MObject aIsShade;

    static MObject aDisplayMode;

    static MObject aPointSize;
    static MObject aLineWidth;
    static MObject aColorType;
    static MObject aSolidColor;
    // for render
    static MObject aIsRender;
    static MObject aiOpaque;
    static MObject aiSelfShadows;
    static MObject aiMatte;
    static MObject aIsStaticObject;

    // Mesh Option
    static MObject aSmoothing;
    static MObject aCreaseEdgeGroup;
    static MObject aExportUVs;

    // subdiv Option
    static MObject aSubdivType;
    static MObject aSubdivIterations;
    static MObject aSubdivAdaptiveError;
    static MObject aSubdivDicingCamera;
    static MObject aSubdivAdaptiveMetric;
    static MObject aSubdivUVSmoothing;
    static MObject aSubdivAdaptiveSpace;
    static MObject aSubdivSmoothDerivs;

    static MObject aDispMap;
    static MObject aDispHeight;
    static MObject aDispZeroValue;
    static MObject aDispPadding;
    static MObject aDispAutobump;

    static MObject aSidedness;
    static MObject aInvertNormals;

    static MObject aExportMeshAttribute;
    static MObject aExportPointAttribute;

    static MObject aGroupMatte;
	static MObject aGroupMatteEnable;
	static MObject aGroupMatteName;
	static MObject aMatteGroups;
	
	
	static MObject aMeshShader;
	static MObject aDisplacementShader;
	
	
	
	static MObject aNodeInfo;
	
};




//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


class DMeshDisplayOverride final : public MHWRender::MPxGeometryOverride
{
public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DMeshDisplayOverride", [&]() {
            return new DMeshDisplayOverride(obj);
        });
    }
		~DMeshDisplayOverride() override;

        MHWRender::DrawAPI supportedDrawAPIs() const override;

        bool hasUIDrawables() const override {
            return false;
        }

        void updateDG() override;
        bool isIndexingDirty(const MHWRender::MRenderItem&) override {
            return true;
        }
        bool isStreamDirty(const MHWRender::MVertexBufferDescriptor&) override {
            return mDataChanged || mShaderUpdateFailed;
        }
        void updateRenderItems(const MDagPath& path, MHWRender::MRenderItemList& list) override;
        void populateGeometry(const MHWRender::MGeometryRequirements& requirements,
                              const MHWRender::MRenderItemList& renderItems,
                              MHWRender::MGeometry& data) override;
        void cleanUp() override {}

        bool traceCallSequence() const override {
            // Return true if internal tracing is desired.
            return false;
        }
        void handleTraceMessage(const MString& message) const override {
            MGlobal::displayInfo("DMeshDisplayOverride: " + message);
        }

        struct DrawData {
            DM_DISPLAY_TYPE displayType = DM_DPY_HARD;
            bool isWireframe = false;
            bool isVertex = false;
            bool isShade = false;

            float pointSize = 3.0f;
            float lineWidth = 1.0f;

            short colorType = DM_DISPLAY_COLOR_SOLID;
            MColor solidColor = MColor(1.0f, 1.0f, 1.0f, 1.0f);

            // Expanded triangle and wire streams refer back to this point buffer.
            std::vector<float> mPointBuffer;
            std::vector<unsigned int> mFaceIndexList;
            std::vector<unsigned int> mWireIndexList;

            std::vector<float> colBuffer;
            std::vector<float> normBuffer;

            size_t primCount = 0U;
            size_t pointCount = 0U;
        };

      private:
        DMeshDisplayOverride(const MObject& obj);

        // BaseShader
        MHWRender::MShaderInstance* mSolidUIShader;
        MHWRender::MShaderInstance* mPointUIShader;
        MHWRender::MShaderInstance* mFlatUIShader;
        MHWRender::MShaderInstance* mWireUIShader;

        // Color
        MHWRender::MShaderInstance* mColorMeshShader;
        //    MHWRender::MShaderInstance* mWireColorUIShader;

        // info Shader
        //    MHWRender::MShaderInstance* mIndexUIShader;

        //    MHWRender::MShaderInstance* mVectorUIShader;
        //    MHWRender::MShaderInstance* mValueUIShader;
        //    MHWRender::MShaderInstance* mValue2UIShader;
        //    MHWRender::MShaderInstance* mValue3UIShader;

        // group shader
        //    MHWRender::MShaderInstance* mGroupPolyShader;
        //    MHWRender::MShaderInstance* mGroupLineShader;
        //    MHWRender::MShaderInstance* mGroupPointShader;

        MObject mMeshDisplayNode;
        bool mDataChanged;
        bool mShaderUpdateFailed = false;

        DrawData dData;
};
