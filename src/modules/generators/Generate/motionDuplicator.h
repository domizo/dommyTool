//
//  motionDuplicator.h
//  dommyTool
//
//  Created by yuzo on 2015/01/10.
//
//

#ifndef __dommyTool__motionDuplicator__
#define __dommyTool__motionDuplicator__


#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxNode.h>

enum {
    DT_GLOBAL_DELAY = 0,
    DT_LOCAL_DELAY,
    DT_EVAL_TIME,
    
    DT_MOTION_TIMING_COUNT
};typedef int DT_MOTION_TIMING_TYPE;

class DTMotionDuplicator final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inputObject;
    
    static MObject count;
    
    
    //animation Data
    static MObject delay;
    static MObject seed;
    
    
    static MObject transformMatrix;
    static MObject transformMatrixTimeRandom;
    
    
    
    //translate
    static MObject translateX;
    static MObject translateY;
    static MObject translateZ;
    static MObject translate;
    
    static MObject translateTimeType;
    
    static MObject translateDelay;
    
    static MObject translateXEvalTime;
    static MObject translateYEvalTime;
    static MObject translateZEvalTime;
    static MObject translateEvalTime;
    
    static MObject translateTimeRandom;
    
    
    
    
    //rotate
    static MObject rotateX;
    static MObject rotateY;
    static MObject rotateZ;
    static MObject rotate;
    
    static MObject rotateTimeType;
    
    static MObject rotateDelay;
    
    static MObject rotateXEvalTime;
    static MObject rotateYEvalTime;
    static MObject rotateZEvalTime;
    static MObject rotateEvalTime;
    
    static MObject rotateTimeRandom;
    
    
    static MObject rotateCenterX;
    static MObject rotateCenterY;
    static MObject rotateCenterZ;
    static MObject rotateCenter;
    
    
    
    
    //scale
    static MObject scaleX;
    static MObject scaleY;
    static MObject scaleZ;
    static MObject scale;
    
    static MObject scaleTimeType;
    static MObject scaleDelay;
    
    static MObject scaleXEvalTime;
    static MObject scaleYEvalTime;
    static MObject scaleZEvalTime;
    static MObject scaleEvalTime;
    
    static MObject scaleTimeRandom;
    
    
    static MObject scaleCenterX;
    static MObject scaleCenterY;
    static MObject scaleCenterZ;
    static MObject scaleCenter;

    
    
    //offset
    static MObject offsetX;
    static MObject offsetY;
    static MObject offsetZ;
    static MObject offset;
    
    static MObject offsetXEvalTime;
    static MObject offsetYEvalTime;
    static MObject offsetZEvalTime;
    static MObject offsetEvalTime;
    
    static MObject outputObjects;
    
    
    static MObject time;
    
    
private:
    
    
};



#endif /* defined(__dommyTool__motionDuplicator__) */
