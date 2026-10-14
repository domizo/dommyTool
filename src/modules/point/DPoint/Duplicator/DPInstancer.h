//
//  DPInstancer.hpp
//  dommyTool
//
//  Created by yuzo on 2017/06/24.
//

//DPointを元にメッシュを生成


#pragma once

#include "common/utility/DMayaEntryPointGuard.h"
#include <vector>

#include <maya/MPxNode.h>
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

#include "DTransformOp.h"


enum {
    DP_INST_COPY_MESH = 0,
    DP_INST_COPY_ASS,
	DP_INST_COPY_ASS_SEQUENCE,
    DP_INST_COPY_TYPE_COUNT
};typedef short DP_INST_COPY_TYPE;

enum {
	DP_INST_NONE = 0,
	DP_INST_POINT,
	DP_INST_BBOX,
	DP_INST_WIRE,
	DP_INST_SHADE,
	DP_INST_TYPE_COUNT
};typedef short DP_INST_DISPLAY_TYPE;


class DPInstancer final : public MPxLocatorNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& data ) override;
	SchedulingType schedulingType() const override { return kUntrusted; }
	
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
	
	
	
	//attribute
	
	static MObject inDP;
	
	//input type
	static MObject aCopyObjectType;
	
	//input mesh
	static MObject inMesh;
	
	//ass file options
	static MObject aReadAssFile;
	static MObject aAssPrefix;
	static MObject aIsOverrideShader;
	static MObject aIsVerboseNodeList;
	
	//Sequence
	static MObject aTimeRange;
	static MObject aTimeOffset;
	static MObject aTimeAttribute;
	
	
	//Display Attribute
	static MObject aDisplayType;
	
	
	//Edit attribute
    static MObject aAimVector;
    static MObject aAxisVector;
    
	static MObject aNormalVector;
	static MObject aNormalAttribute;
	
	static MObject aTangentVector;
	static MObject aTangentAttribute;
	
	
	static MObject aAnimationType;
	static MObject aAnimMatrixAttribute;
	static MObject aInAnimMatrix;
	
	
	static MObject aLocalTranslate;
	static MObject aGlobalTranslate;
    static MObject aLocalRotate;
	static MObject aGlobalRotate;
    static MObject aGlobalScale;
	static MObject aUniformScale;
	
    
    //attribute
	static MObject aLocalTranslateAttribute;
	static MObject aGlobalTranslateAttribute;
	static MObject aLocalRotateAttribute;
	static MObject aGlobalRotateAttribute;
	static MObject aGlobalScaleAttribute;
	static MObject aUniformScaleAttribute;
	
	//random
	static MObject aSeed;
	static MObject aLocalTranslateRandomMin;
	static MObject aLocalTranslateRandomMax;
	static MObject aGlobalTranslateRandomMin;
	static MObject aGlobalTranslateRandomMax;
	static MObject aLocalRotateRandomMin;
	static MObject aLocalRotateRandomMax;
	static MObject aGlobalRotateRandomMin;
	static MObject aGlobalRotateRandomMax;
	static MObject aGlobalScaleRandomMin;
	static MObject aGlobalScaleRandomMax;
	static MObject aUniformScaleRandomMinMax;
    
    //render attribute
    static MObject aIsRender;
    
    static MObject aiOpaque;
    static MObject aiMatte;
    static MObject aiSelfShadows;
    static MObject aiReceiveShadows;
    
    static MObject aSidedness;
    static MObject aInvertNormals;
    
    static MObject aExportAttribute;
    
    //shader
    static MObject aShader;
	
    static MObject aTargetNodeNames;
	static MObject aReplaceShader;
	static MObject aReplaceShaderList;
	
};



class DPInstancerOverride final : public MHWRender::MPxGeometryOverride
{
public:
    static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DPInstancerOverride", [&]() {
            return new DPInstancerOverride(obj);
        });
    }
		~DPInstancerOverride() override;
		
		MHWRender::DrawAPI supportedDrawAPIs() const override;
	
	
	
		bool hasUIDrawables() const override { return false; }
		
		void updateDG() override;
		bool isIndexingDirty(const MHWRender::MRenderItem&) override {
			return true;
		}
		bool isStreamDirty(
			const MHWRender::MVertexBufferDescriptor&) override {
			return mDataChanged || mShaderUpdateFailed;
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
		MGlobal::displayInfo("DPInstancerOverride: " + message);
		
	}
	
	struct DrawData {
		
		//ass file bbox
		std::string assFile;
		MPoint minPt = MPoint(-1.0, -1.0, -1.0);
		MPoint maxPt = MPoint(1.0, 1.0, 1.0);
		
		//ass Sequence bbox lis
		std::vector<MPointArray> bboxSequenceList;
		MIntArray bboxConnectArray;
		
		
		
		short displayType = 0;
        DP_INST_COPY_TYPE mCopyType = DP_INST_COPY_MESH;
		
//		DPointList mPointList;
		std::vector<float> pointList;
		std::vector<unsigned int> indexList;
		std::vector<float> normalList;
		
//        size_t pointCount;
	};
	
private:
	MStatus evalParms(DTransformParams& parms);
	void clearDrawData() noexcept;
	void updateDGImpl();
	
	
	DPInstancerOverride(const MObject& obj);
	MHWRender::MShaderInstance* mBaseUIShader;
	MHWRender::MShaderInstance* mPointUIShader;
	MHWRender::MShaderInstance* mWireUIShader;
	
	
	
	MObject mInstancerNode;
	bool mDataChanged;
	bool mShaderUpdateFailed = false;
//	MDagPath fPath;
	
	DrawData dData;
	
};
