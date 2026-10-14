//
//  DCameraFrustum.hpp
//  dommyTool
//
//  Created by yuzo on 4/27/16.
//
//

#ifndef DCameraFrustum_h
#define DCameraFrustum_h



#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxLocatorNode.h>
#include <maya/MBoundingBox.h>

// Viewport 2.0 includes
#include <maya/MDrawRegistry.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MPointArray.h>

class DCameraFrustum final : public MPxLocatorNode
{
public:
	DCameraFrustum();
	~DCameraFrustum() override;
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	void draw(M3dView & view, const MDagPath & path,
			  M3dView::DisplayStyle style, M3dView::DisplayStatus status) override;
	
	bool isBounded() const override;
	MBoundingBox boundingBox() const override;
	
	static void * creator();
	static  MStatus initialize();
	static MTypeId typeId;
	static const MString typeName;
	static	MString		classification;
	static	MString		registrantId;
	
	struct FrustumData {
		FrustumData()
			: nearClipping(0.0), farClipping(0.0), aspect(0.0),
			  focalLength(0.0), nearHSize(0.0), nearVSize(0.0),
			  nearHOffset(0.0), nearVOffset(0.0), farHSize(0.0),
			  farVSize(0.0), farHOffset(0.0), farVOffset(0.0),
			  extendNearSize(0.0), extendFarSize(0.0)
		{
			for (unsigned int index = 0U; index < 24U; ++index) {
				frustumMat[index] = 0.0f;
			}
		}

		float frustumMat[24];
		
		double nearClipping;
		double farClipping;
		
		double aspect;
		double focalLength;
		
		double nearHSize,nearVSize;
		double nearHOffset;
		double nearVOffset;
		
		double farHSize,farVSize;
		double farHOffset;
		double farVOffset;
		
		double extendNearSize;
		double extendFarSize;
	};
	
	MStatus getCamData(FrustumData& camf) const;
	
	
	static MObject aCameraName;
	
	static MObject aVisibleMode;
	static MObject aCustumNearPlane;
    static MObject aCustumFarPlane;
	
	static MObject extendNearArea;
	static MObject extendFarArea;
	
    static MObject aOutMesh;
    static MObject aTime;
	
private:
	MBoundingBox mBBox;
};


class DCameraFrustumData : public MUserData
{
public:
	DCameraFrustumData()
		: MUserData(false), nearClipping(0.0), farClipping(0.0),
		  aspect(0.0), focalLength(0.0), nearHSize(0.0), nearVSize(0.0),
		  nearHOffset(0.0), nearVOffset(0.0), farHSize(0.0),
		  farVSize(0.0), farHOffset(0.0), farVOffset(0.0),
		  extendNearSize(0.0), extendFarSize(0.0)
	{
		for (unsigned int index = 0U; index < 24U; ++index) {
			frustumMat[index] = 0.0f;
		}
	}
	virtual ~DCameraFrustumData() {}
	
	float frustumMat[24];
	
	double nearClipping;
	double farClipping;
	
	double aspect;
	double focalLength;
	
	double nearHSize,nearVSize;
	double nearHOffset;
	double nearVOffset;
	
	double farHSize,farVSize;
	double farHOffset;
	double farVOffset;
	
	double extendNearSize;
	double extendFarSize;
};

class DCameraFrustumOverride final : public MHWRender::MPxDrawOverride
{
public:
    static MHWRender::MPxDrawOverride* Creator(const MObject& obj)
    {
        return ::dommy::maya::createObject("DCameraFrustumOverride", [&]() {
            return new DCameraFrustumOverride(obj);
        });
    }
	
	~DCameraFrustumOverride() override;
	
	MHWRender::DrawAPI supportedDrawAPIs() const override;
	
	bool isBounded( const MDagPath& objPath,
				   const MDagPath& cameraPath) const override;
	
	MBoundingBox boundingBox( const MDagPath& objPath,
							 const MDagPath& cameraPath) const override;
	
	MUserData* prepareForDraw( const MDagPath& objPath,
									  const MDagPath& cameraPath,
									  const MHWRender::MFrameContext& frameContext,
									  MUserData* oldData) override;
	
	bool hasUIDrawables() const override { return true; }
	
	void addUIDrawables(
								const MDagPath& objPath,
								MHWRender::MUIDrawManager& drawManager,
								const MHWRender::MFrameContext& frameContext,
								const MUserData* data) override;
	
	static void draw(const MHWRender::MDrawContext&, const MUserData*) {}
	
private:
	DCameraFrustumOverride(const MObject& obj);
};









#endif /* DCameraFrustum_h */
