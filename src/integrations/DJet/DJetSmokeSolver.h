//
//  DJetSmokeSolver.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/24.
//

#pragma once

#include "PluginFeatures.h"

#if DOMMY_WITH_JET

#include <set>
#include <vector>

#include <jet/jet.h>



#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxNode.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MTime.h>



class DJetSmokeSolver final : public MPxLocatorNode
{
public:
    
    DJetSmokeSolver();
    ~DJetSmokeSolver() override;
    void postConstructor() override;
    void draw(M3dView& view, const MDagPath& path,
              M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;
    
    
    bool isBounded() const override {
        return false;
    }
    MBoundingBox boundingBox() const override
    {
        MObject node = thisMObject();
        MPoint corner1(-1, -1, -1);
        MPoint corner2(1, 1, 1);
        return MBoundingBox(corner1, corner2);
    }
    
    bool excludeAsLocator() const override {
        return false;
    }
    bool isTransparent() const override {
        return false;
    }
    
    static MTypeId typeId;
    static const MString typeName;
    static  void * creator();
    static  MStatus initialize();
    
    bool setInternalValueInContext(
        const MPlug& plug,
        const MDataHandle& dataHandle,
        MDGContext& context) override;
    
    
    
    MStatus     compute( const MPlug& plug, MDataBlock& data ) override;
    
    
    MStatus initSolver(MDataBlock& data);
    void clearSolver();
    //Attribute
    
    
    static MObject aEnabled;
    static MObject aCurrentTime;
    static MObject aStartTime;
    static MObject aFrameRate;
    
    //fluid options
    static MObject aResolusion;
    static MObject aGridSize;
    
    
    
    static MObject aOutFluid;
    
    
    
    static float collisionMarginOffset; //mb
    
protected:
    
    jet::GridSmokeSolver3Ptr m_solver;
    jet::Frame m_frame;
    
    
protected:
    
    MTime m_prevTime;
    
};


#endif
