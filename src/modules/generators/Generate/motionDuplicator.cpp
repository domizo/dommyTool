//
//  motionDuplicator.cpp
//  dommyTool
//
//  Created by yuzo on 2015/01/10.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "motionDuplicator.h"
#include "common/utility/DStatusOperation.h"


#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MDataBlock.h>
#include <maya/MArrayDataBuilder.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MDGContextGuard.h>
#include <maya/MDagPath.h>



#include <maya/MPointArray.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MTransformationMatrix.h>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsSurfaceData.h>
#include <maya/MFnNurbsSurface.h>

#include <maya/MFnAnimCurve.h>
#include <maya/MAnimControl.h>
#include <maya/MGlobal.h>
#include <maya/MTime.h>
#include <maya/MStringArray.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>


#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMotionGenerationPlan.h"
#include "dommyMath.h"
#include "DRandomGenerator.h"


const MTypeId DTMotionDuplicator::typeId( 0x00338 );

void *DTMotionDuplicator::creator()
{
    return ::dommy::maya::createObject("DTMotionDuplicator", [&]() {
        return new DTMotionDuplicator();
    });
}


MObject DTMotionDuplicator::inputObject;

MObject DTMotionDuplicator::count;

MObject DTMotionDuplicator::delay;
MObject DTMotionDuplicator::seed;

//matrix
MObject DTMotionDuplicator::transformMatrix;
MObject DTMotionDuplicator::transformMatrixTimeRandom;

//translate
MObject DTMotionDuplicator::translateX;
MObject DTMotionDuplicator::translateY;
MObject DTMotionDuplicator::translateZ;
MObject DTMotionDuplicator::translate;

MObject DTMotionDuplicator::translateTimeType;

MObject DTMotionDuplicator::translateDelay;

MObject DTMotionDuplicator::translateXEvalTime;
MObject DTMotionDuplicator::translateYEvalTime;
MObject DTMotionDuplicator::translateZEvalTime;
MObject DTMotionDuplicator::translateEvalTime;

MObject DTMotionDuplicator::translateTimeRandom;

//rotate
MObject DTMotionDuplicator::rotateX;
MObject DTMotionDuplicator::rotateY;
MObject DTMotionDuplicator::rotateZ;
MObject DTMotionDuplicator::rotate;


MObject DTMotionDuplicator::rotateTimeType;
MObject DTMotionDuplicator::rotateDelay;
MObject DTMotionDuplicator::rotateXEvalTime;
MObject DTMotionDuplicator::rotateYEvalTime;
MObject DTMotionDuplicator::rotateZEvalTime;
MObject DTMotionDuplicator::rotateEvalTime;
MObject DTMotionDuplicator::rotateTimeRandom;

MObject DTMotionDuplicator::rotateCenterX;
MObject DTMotionDuplicator::rotateCenterY;
MObject DTMotionDuplicator::rotateCenterZ;
MObject DTMotionDuplicator::rotateCenter;


//scale
MObject DTMotionDuplicator::scaleX;
MObject DTMotionDuplicator::scaleY;
MObject DTMotionDuplicator::scaleZ;
MObject DTMotionDuplicator::scale;

MObject DTMotionDuplicator::scaleTimeType;
MObject DTMotionDuplicator::scaleDelay;
MObject DTMotionDuplicator::scaleXEvalTime;
MObject DTMotionDuplicator::scaleYEvalTime;
MObject DTMotionDuplicator::scaleZEvalTime;
MObject DTMotionDuplicator::scaleEvalTime;
MObject DTMotionDuplicator::scaleTimeRandom;



MObject DTMotionDuplicator::scaleCenterX;
MObject DTMotionDuplicator::scaleCenterY;
MObject DTMotionDuplicator::scaleCenterZ;
MObject DTMotionDuplicator::scaleCenter;



//offset
MObject DTMotionDuplicator::offsetX;
MObject DTMotionDuplicator::offsetY;
MObject DTMotionDuplicator::offsetZ;
MObject DTMotionDuplicator::offset;

MObject DTMotionDuplicator::offsetXEvalTime;
MObject DTMotionDuplicator::offsetYEvalTime;
MObject DTMotionDuplicator::offsetZEvalTime;
MObject DTMotionDuplicator::offsetEvalTime;


MObject DTMotionDuplicator::outputObjects;

MObject DTMotionDuplicator::time;


MStatus DTMotionDuplicator::initialize() try
{
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute typedAttr;
    MFnEnumAttribute enumFn;
    MFnUnitAttribute unitFn;

    inputObject  = typedAttr.create("inputObject", "io", MFnGeometryData::kAny, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setHidden( true ));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setWritable( true ));

    count = nAttr.create("count", "c", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 5 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));

    delay = nAttr.create("delay", "d", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));

    seed = nAttr.create("seed", "see", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));

    MFnMatrixAttribute matAttr;

    transformMatrix = matAttr.create("transformMatrix", "tm",MFnMatrixAttribute::kDouble, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setCached( false ));

    transformMatrixTimeRandom = nAttr.create("transformMatrixTimeRandom", "tmr", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));

    translateX  = nAttr.create("translateX", "tx", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    translateY  = nAttr.create("translateY", "ty", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    translateZ  = nAttr.create("translateZ", "tz", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    translate = nAttr.create("translate", "t",
                         translateX,
                         translateY,
                         translateZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    translateTimeType = enumFn.create("translateTimeType", "ttt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Global Delay", DT_GLOBAL_DELAY ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Local Delay", DT_LOCAL_DELAY ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Eval Time", DT_EVAL_TIME ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));

    translateDelay  = nAttr.create("translateDelay", "td", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    translateXEvalTime  = nAttr.create("translateXEvalTime", "tex", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    translateYEvalTime  = nAttr.create("translateYEvalTime", "tey", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    translateZEvalTime  = nAttr.create("translateZEvalTime", "tez", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    translateEvalTime = nAttr.create("translateEvalTime", "tet",
                             translateXEvalTime,
                             translateYEvalTime,
                             translateZEvalTime, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    translateTimeRandom  = nAttr.create("translateTimeRandom", "ttr", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    rotateX  = nAttr.create("rotateX", "rx", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    rotateY  = nAttr.create("rotateY", "ry", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    rotateZ  = nAttr.create("rotateZ", "rz", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    rotate = nAttr.create("rotate", "r",
                             rotateX,
                             rotateY,
                             rotateZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    rotateTimeType = enumFn.create("rotateTimeType", "rtt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Global Delay", DT_GLOBAL_DELAY ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Local Delay", DT_LOCAL_DELAY ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Eval Time", DT_EVAL_TIME ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));

    rotateDelay  = nAttr.create("rotateDelay", "rd", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    rotateXEvalTime  = nAttr.create("rotateXEvalTime", "rex", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    rotateYEvalTime  = nAttr.create("rotateYEvalTime", "rey", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    rotateZEvalTime  = nAttr.create("rotateZEvalTime", "rez", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    rotateEvalTime = nAttr.create("rotateEvalTime", "ret",
                                     rotateXEvalTime,
                                     rotateYEvalTime,
                                     rotateZEvalTime, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    rotateTimeRandom  = nAttr.create("rotateTimeRandom", "rtr", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    rotateCenterX  = nAttr.create("rotateCenterX", "rcx", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    rotateCenterY  = nAttr.create("rotateCenterY", "rcy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    rotateCenterZ  = nAttr.create("rotateCenterZ", "rcz", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    rotateCenter = nAttr.create("rotateCenter", "rc",
                          rotateCenterX,
                          rotateCenterY,
                          rotateCenterZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    scaleX  = nAttr.create("scaleX", "sx", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleY  = nAttr.create("scaleY", "sy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleZ  = nAttr.create("scaleZ", "sz", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    scale = nAttr.create("scale", "s",
                                scaleX,
                                scaleY,
                                scaleZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));

    scaleTimeType = enumFn.create("scaleTimeType", "stt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Global Delay", DT_GLOBAL_DELAY ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Local Delay", DT_LOCAL_DELAY ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Eval Time", DT_EVAL_TIME ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));

    scaleDelay  = nAttr.create("scaleDelay", "sd", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    scaleXEvalTime  = nAttr.create("scaleXEvalTime", "sex", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleYEvalTime  = nAttr.create("scaleYEvalTime", "sey", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleZEvalTime  = nAttr.create("scaleZEvalTime", "sez", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    scaleEvalTime = nAttr.create("scaleEvalTime", "set",
                                  scaleXEvalTime,
                                  scaleYEvalTime,
                                  scaleZEvalTime, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    scaleTimeRandom  = nAttr.create("scaleTimeRandom", "str", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    scaleCenterX  = nAttr.create("scaleCenterX", "scx", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleCenterY  = nAttr.create("scaleCenterY", "scy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleCenterZ  = nAttr.create("scaleCenterZ", "scz", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    scaleCenter = nAttr.create("scaleCenter", "sc",
                         scaleCenterX,
                         scaleCenterY,
                         scaleCenterZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    offsetX  = nAttr.create("offsetX", "ox", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetY  = nAttr.create("offsetY", "oy", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetZ  = nAttr.create("offsetZ", "oz", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    offset = nAttr.create("offset", "o",
                             offsetX,
                             offsetY,
                             offsetZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    offsetXEvalTime  = nAttr.create("offsetXEvalTime", "oxe", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetYEvalTime  = nAttr.create("offsetYEvalTime", "oye", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetZEvalTime  = nAttr.create("offsetZEvalTime", "oze", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    offsetEvalTime = nAttr.create("offsetEvalTime", "oet",
                          offsetXEvalTime,
                          offsetYEvalTime,
                          offsetZEvalTime, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));

    outputObjects = typedAttr.create("outputObjects", "oos", MFnGeometryData::kAny,
        MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setReadable( true ));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setWritable( false ));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(typedAttr.setUsesArrayDataBuilder( true ));

    time = unitFn.create("time", "ti", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(unitFn.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(unitFn.setKeyable(true));

    //add attribute

    DOMMY_RETURN_IF_FAILURE(addAttribute(inputObject));
    DOMMY_RETURN_IF_FAILURE(addAttribute(count));
    DOMMY_RETURN_IF_FAILURE(addAttribute(delay));
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));

    DOMMY_RETURN_IF_FAILURE(addAttribute(transformMatrix));
    DOMMY_RETURN_IF_FAILURE(addAttribute(transformMatrixTimeRandom));

    DOMMY_RETURN_IF_FAILURE(addAttribute(translate));
    DOMMY_RETURN_IF_FAILURE(addAttribute(translateTimeType));
    DOMMY_RETURN_IF_FAILURE(addAttribute(translateDelay));
    DOMMY_RETURN_IF_FAILURE(addAttribute(translateEvalTime));
    DOMMY_RETURN_IF_FAILURE(addAttribute(translateTimeRandom));

    DOMMY_RETURN_IF_FAILURE(addAttribute(rotate));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rotateTimeType));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rotateDelay));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rotateEvalTime));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rotateTimeRandom));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rotateCenter));

    DOMMY_RETURN_IF_FAILURE(addAttribute(scale));

    DOMMY_RETURN_IF_FAILURE(addAttribute(scaleTimeType));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scaleDelay));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scaleEvalTime));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scaleTimeRandom));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scaleCenter));

    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offsetEvalTime));

    DOMMY_RETURN_IF_FAILURE(addAttribute(outputObjects));

    DOMMY_RETURN_IF_FAILURE(addAttribute(time));

    //set affect attribute
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputObject, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(count, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(delay, outputObjects));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(seed, outputObjects));

    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(transformMatrix, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(transformMatrixTimeRandom, outputObjects));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(translate, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(translateTimeType, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(translateDelay, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(translateEvalTime, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(translateTimeRandom, outputObjects));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(rotate, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(rotateCenter, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(rotateTimeType, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(rotateDelay, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(rotateEvalTime, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(rotateTimeRandom, outputObjects));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(scale, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(scaleCenter, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(scaleTimeType, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(scaleDelay, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(scaleEvalTime, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(scaleTimeRandom, outputObjects));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outputObjects));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(offsetEvalTime, outputObjects));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(time, outputObjects));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DTMotionDuplicator", "initialize", std::current_exception());
}



namespace {

enum MotionGeometryKind {
    kMotionCurve,
    kMotionSurface,
    kMotionMesh
};

bool validTimingType(const int value)
{
    return value >= DT_GLOBAL_DELAY && value < DT_MOTION_TIMING_COUNT;
}

MStatus queryMotionMeshSize(MFnMesh& mesh, dommy::generation::MotionGeometrySize& output)
{
    using namespace dommy::generation;
    MotionGeometrySize candidate;
    MStatus status;
    const int points = mesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int faces = mesh.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int connections = mesh.numFaceVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int normals = mesh.numNormals(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int uvSets = mesh.numUVSets(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int colorSets = mesh.numColorSets(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t uvSetCount = 0U, colorSetCount = 0U, perSet = 0U;
    if (!checkedCount(points, kMaxGeneratedPointCount, candidate.points) ||
        !checkedCount(faces, kMaxGeneratedFaceCount, candidate.faces) ||
        !checkedCount(connections, kMaxWorkItemCount, candidate.connections) ||
        !checkedCount(normals, kMaxWorkItemCount, candidate.normals) ||
        !checkedCount(uvSets, kMaxMotionGeometrySets, uvSetCount) ||
        !checkedCount(colorSets, kMaxMotionGeometrySets, colorSetCount) ||
        !checkedSum(uvSetCount, colorSetCount, kMaxMotionGeometrySets, candidate.sets) ||
        !checkedSum(candidate.faces, candidate.connections, kMaxWorkItemCount, perSet) ||
        !checkedProduct(perSet, candidate.sets, kMaxWorkItemCount, candidate.assignments))
        return MS::kInvalidParameter;

    // Bound the name-array cardinality before requesting it. Query every set,
    // not only the current set: MFnMesh::copy retains the source mesh data.
    for (unsigned int kind = 0U; kind < 2U; ++kind) {
        const std::size_t expected = kind == 0U ? uvSetCount : colorSetCount;
        if (expected == 0U) continue;
        MStringArray names;
        DOMMY_RETURN_IF_FAILURE(kind == 0U ? mesh.getUVSetNames(names) : mesh.getColorSetNames(names));
        if (names.length() != expected) return MS::kFailure;
        for (unsigned int i = 0U; i < names.length(); ++i) {
            if (!checkedSum(candidate.nameCharacters, names[i].length(),
                            kMaxGeneratedStringByteCount / 4U, candidate.nameCharacters))
                return MS::kInvalidParameter;
            const int rawCount = kind == 0U ? mesh.numUVs(names[i], &status)
                                           : mesh.numColors(names[i], &status);
            DOMMY_RETURN_IF_FAILURE(status);
            std::size_t count = 0U;
            std::size_t& total = kind == 0U ? candidate.uvs : candidate.colors;
            if (!checkedCount(rawCount, kMaxWorkItemCount, count) ||
                !checkedSum(total, count, kMaxWorkItemCount, total)) return MS::kInvalidParameter;
        }
    }
    output = candidate;
    return MS::kSuccess;
}

MStatus queryMotionCurveSize(MFnNurbsCurve& curve, dommy::generation::MotionGeometrySize& output)
{
    MStatus status;
    const int points = curve.numCVs(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int knots = curve.numKnots(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::generation::MotionGeometrySize candidate;
    if (!dommy::generation::checkedCount(points, dommy::generation::kMaxGeneratedPointCount, candidate.points) ||
        !dommy::generation::checkedCount(knots, dommy::generation::kMaxWorkItemCount, candidate.knots))
        return MS::kInvalidParameter;
    output = candidate;
    return MS::kSuccess;
}

MStatus queryMotionSurfaceSize(MFnNurbsSurface& surface, dommy::generation::MotionGeometrySize& output)
{
    using namespace dommy::generation;
    MStatus status;
    const int pointsU = surface.numCVsInU(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int pointsV = surface.numCVsInV(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int knotsU = surface.numKnotsInU(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int knotsV = surface.numKnotsInV(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t u = 0U, v = 0U, ku = 0U, kv = 0U;
    MotionGeometrySize candidate;
    if (!checkedCount(pointsU, kMaxGeneratedPointCount, u) ||
        !checkedCount(pointsV, kMaxGeneratedPointCount, v) ||
        !checkedProduct(u, v, kMaxGeneratedPointCount, candidate.points) ||
        !checkedCount(knotsU, kMaxWorkItemCount, ku) ||
        !checkedCount(knotsV, kMaxWorkItemCount, kv) ||
        !checkedSum(ku, kv, kMaxWorkItemCount, candidate.knots)) return MS::kInvalidParameter;
    output = candidate;
    return MS::kSuccess;
}

MStatus plugConnectionState(const MPlug& plug, bool& connected)
{
    MStatus status;
    connected = plug.isConnected(&status);
    return status;
}

MStatus findNodePlug(MFnDependencyNode& node,
                     const MObject& attribute,
                     MPlug& plug)
{
    MStatus status;
    const MPlug candidate = node.findPlug(attribute, true, &status);
    if (!status || candidate.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }
    plug = candidate;
    return MS::kSuccess;
}

MStatus readFloat3(MDataBlock& block,
                   const MObject& attribute,
                   std::array<double, 3>& values)
{
    MStatus status;
    const MDataHandle handle = block.inputValue(attribute, &status);
    if (!status) return status;
    const float* const raw = handle.asFloat3();
    if (!raw) return MS::kFailure;
    for (std::size_t axis = 0U; axis < values.size(); ++axis) {
        values[axis] = static_cast<double>(raw[axis]);
        if (!std::isfinite(values[axis])) return MS::kFailure;
    }
    return MS::kSuccess;
}

MStatus readDouble3(MDataBlock& block,
                    const MObject& attribute,
                    std::array<double, 3>& values)
{
    MStatus status;
    const MDataHandle handle = block.inputValue(attribute, &status);
    if (!status) return status;
    const double* const raw = handle.asDouble3();
    if (!raw) return MS::kFailure;
    for (std::size_t axis = 0U; axis < values.size(); ++axis) {
        values[axis] = raw[axis];
        if (!std::isfinite(values[axis])) return MS::kFailure;
    }
    return MS::kSuccess;
}

MStatus sampleTimedValues(std::vector<double>& values,
                          const MPlug& source,
                          const std::size_t count,
                          const int timingType,
                          const double globalDelay,
                          const double localDelay,
                          const double evaluationStep,
                          const std::vector<double>& randomOffsets,
                          const MTime& evaluationTime)
{
    switch (timingType) {
        case DT_GLOBAL_DELAY:
            return dommy::maya::sampleScalarPlugSeries(
                values, source, count, evaluationTime,
                -globalDelay, -globalDelay, randomOffsets);
        case DT_LOCAL_DELAY:
            return dommy::maya::sampleScalarPlugSeries(
                values, source, count, evaluationTime,
                -localDelay, -localDelay, randomOffsets);
        case DT_EVAL_TIME: {
            const MTime zeroTime(0.0, evaluationTime.unit());
            return dommy::maya::sampleScalarPlugSeries(
                values, source, count, zeroTime,
                evaluationStep, evaluationStep, std::vector<double>());
        }
        default:
            return MS::kFailure;
    }
}

MStatus buildRandomOffsets(std::vector<double>& offsets,
                           const std::size_t count,
                           const double amplitude,
                           dommy::math::RandomGenerator& random)
{
    if (!std::isfinite(amplitude) || amplitude < 0.0) {
        return MS::kFailure;
    }
    if (amplitude == 0.0) {
        offsets.clear();
        return MS::kSuccess;
    }

    std::vector<double> generated;
    generated.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const double value = random.uniform(-amplitude, amplitude);
        if (!std::isfinite(value)) return MS::kFailure;
        generated.push_back(value);
    }
    offsets.swap(generated);
    return MS::kSuccess;
}

double sampledOrDefault(const std::vector<double>& values,
                        const std::size_t index,
                        const double defaultValue)
{
    return index < values.size() ? values[index] : defaultValue;
}

MStatus transformPoints(const MPointArray& source,
                        const MMatrix& transform,
                        const std::array<double, 3>& offset,
                        MPointArray& result)
{
    if (!dommy::maya::isFiniteMatrix(transform)) return MS::kFailure;

    MPointArray candidate(source);
    for (unsigned int index = 0U; index < candidate.length(); ++index) {
        MPoint point = source[index] * transform;
        point.x += offset[0];
        point.y += offset[1];
        point.z += offset[2];
        if (!dommy::maya::isFinitePoint(point)) return MS::kFailure;
        candidate[index] = point;
    }
    result = candidate;
    return MS::kSuccess;
}

} // namespace

MStatus DTMotionDuplicator::compute(const MPlug& plug,
                                    MDataBlock& block) try
{
    if (plug != outputObjects) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle timeHandle = block.inputValue(time, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MTime evaluationTime = timeHandle.asTime();
    const MDataHandle countHandle = block.inputValue(count, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int countValue = countHandle.asInt();
    const MDataHandle delayHandle = block.inputValue(delay, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double delayValue = delayHandle.asFloat();
    const MDataHandle seedHandle = block.inputValue(seed, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double seedValue = seedHandle.asFloat();
    const MDataHandle matrixRandomHandle = block.inputValue(transformMatrixTimeRandom, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double matrixRandomTime = matrixRandomHandle.asFloat();

    const MDataHandle translateTimingHandle = block.inputValue(translateTimeType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int translateTiming = translateTimingHandle.asShort();
    const MDataHandle rotateTimingHandle = block.inputValue(rotateTimeType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int rotateTiming = rotateTimingHandle.asShort();
    const MDataHandle scaleTimingHandle = block.inputValue(scaleTimeType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int scaleTiming = scaleTimingHandle.asShort();

    const MDataHandle translateDelayHandle = block.inputValue(translateDelay, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double translateDelayValue = translateDelayHandle.asFloat();
    const MDataHandle translateRandomHandle = block.inputValue(translateTimeRandom, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double translateRandomTime = translateRandomHandle.asFloat();
    const MDataHandle rotateDelayHandle = block.inputValue(rotateDelay, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double rotateDelayValue = rotateDelayHandle.asFloat();
    const MDataHandle rotateRandomHandle = block.inputValue(rotateTimeRandom, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double rotateRandomTime = rotateRandomHandle.asFloat();
    const MDataHandle scaleDelayHandle = block.inputValue(scaleDelay, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double scaleDelayValue = scaleDelayHandle.asFloat();
    const MDataHandle scaleRandomHandle = block.inputValue(scaleTimeRandom, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double scaleRandomTime = scaleRandomHandle.asFloat();

    std::array<double, 3> translateEvaluationTime;
    std::array<double, 3> rotateEvaluationTime;
    std::array<double, 3> scaleEvaluationTime;
    std::array<double, 3> offsetEvaluationTime;
    std::array<double, 3> globalOffset;
    std::array<double, 3> rotatePivot;
    std::array<double, 3> scalePivot;
    std::array<double, 3> baseTranslation;
    std::array<double, 3> baseRotation;
    std::array<double, 3> baseScale;
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, translateEvalTime, translateEvaluationTime));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, rotateEvalTime, rotateEvaluationTime));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, scaleEvalTime, scaleEvaluationTime));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, offsetEvalTime, offsetEvaluationTime));
    DOMMY_RETURN_IF_FAILURE(readDouble3(
        block, offset, globalOffset));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, rotateCenter, rotatePivot));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, scaleCenter, scalePivot));
    DOMMY_RETURN_IF_FAILURE(readDouble3(
        block, translate, baseTranslation));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, rotate, baseRotation));
    DOMMY_RETURN_IF_FAILURE(readFloat3(
        block, scale, baseScale));
    const MDataHandle matrixHandle = block.inputValue(transformMatrix, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MMatrix baseMatrix = matrixHandle.asMatrix();
    if (!dommy::maya::isFiniteMatrix(baseMatrix)) {
        return MS::kFailure;
    }

    const double finiteValues[] = {
        evaluationTime.value(), delayValue, seedValue, matrixRandomTime,
        translateDelayValue, translateRandomTime, rotateDelayValue,
        rotateRandomTime, scaleDelayValue, scaleRandomTime};
    for (std::size_t index = 0U;
         index < sizeof(finiteValues) / sizeof(finiteValues[0]); ++index) {
        if (!std::isfinite(finiteValues[index])) return MS::kFailure;
    }
    if (!validTimingType(translateTiming) ||
        !validTimingType(rotateTiming) ||
        !validTimingType(scaleTiming) ||
        matrixRandomTime < 0.0 || translateRandomTime < 0.0 ||
        rotateRandomTime < 0.0 || scaleRandomTime < 0.0 ||
        seedValue < static_cast<double>(
                        std::numeric_limits<std::int32_t>::min()) ||
        seedValue > static_cast<double>(
                        std::numeric_limits<std::int32_t>::max())) {
        return MS::kFailure;
    }

    std::size_t checkedCount = 0U;
    if (!dommy::generation::checkedCount(
            countValue, dommy::generation::kMaxGeneratedTransformCount,
            checkedCount)) {
        MGlobal::displayError(
            "DTMotionDuplicator: count exceeds the generation budget.");
        return MS::kFailure;
    }
    if (checkedCount == 0U) {
        return dommy::maya::setObjectArrayOutput(
            block, outputObjects, plug, std::vector<MObject>());
    }

    MDataHandle inputObjectHandle =
        block.inputValue(inputObject, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MFnData::Type inputType = inputObjectHandle.type();
    MotionGeometryKind geometryKind;
    MObject inputObjectData;
    MPointArray sourcePoints;
    MFnNurbsCurve inputCurve;
    MFnNurbsSurface inputSurface;
    MFnMesh inputMesh;
    dommy::generation::MotionGeometrySize sourceSize;

    switch (inputType) {
        case MFnData::kNurbsCurve:
            geometryKind = kMotionCurve;
            inputObjectData = inputObjectHandle.asNurbsCurve();
            DOMMY_RETURN_IF_FAILURE(
                inputCurve.setObject(inputObjectData));
            DOMMY_RETURN_IF_FAILURE(queryMotionCurveSize(inputCurve, sourceSize));
            break;
        case MFnData::kNurbsSurface:
            geometryKind = kMotionSurface;
            inputObjectData = inputObjectHandle.asNurbsSurface();
            DOMMY_RETURN_IF_FAILURE(
                inputSurface.setObject(inputObjectData));
            DOMMY_RETURN_IF_FAILURE(queryMotionSurfaceSize(inputSurface, sourceSize));
            break;
        case MFnData::kMesh: {
            geometryKind = kMotionMesh;
            inputObjectData = inputObjectHandle.asMesh();
            DOMMY_RETURN_IF_FAILURE(inputMesh.setObject(inputObjectData));
            DOMMY_RETURN_IF_FAILURE(queryMotionMeshSize(inputMesh, sourceSize));
            break;
        }
        default:
            return MS::kUnknownParameter;
    }
    if (inputObjectData.isNull()) return MS::kFailure;
    dommy::generation::MotionDuplicationBudget budget;
    if (!dommy::generation::buildMotionDuplicationBudget(
            countValue, sourceSize, budget)) {
        MGlobal::displayError(
            "DTMotionDuplicator: source geometry and count exceed the "
            "aggregate generation budget.");
        return MS::kFailure;
    }

    // No position array or history/output allocation precedes admission.
    switch (geometryKind) {
        case kMotionCurve:
            DOMMY_RETURN_IF_FAILURE(inputCurve.getCVs(sourcePoints, MSpace::kObject));
            break;
        case kMotionSurface:
            DOMMY_RETURN_IF_FAILURE(inputSurface.getCVs(sourcePoints, MSpace::kObject));
            break;
        case kMotionMesh:
            DOMMY_RETURN_IF_FAILURE(inputMesh.getPoints(sourcePoints, MSpace::kObject));
            break;
    }
    if (sourcePoints.length() != sourceSize.points) return MS::kFailure;
    for (unsigned int index = 0U; index < sourcePoints.length(); ++index)
        if (!dommy::maya::isFinitePoint(sourcePoints[index])) return MS::kFailure;

    const std::int32_t signedSeed =
        static_cast<std::int32_t>(seedValue);
    dommy::math::RandomGenerator randomGenerator(
        static_cast<std::uint32_t>(signedSeed));
    std::vector<double> matrixRandomOffsets;
    std::vector<double> translateRandomOffsets;
    std::vector<double> rotateRandomOffsets;
    std::vector<double> scaleRandomOffsets;
    DOMMY_RETURN_IF_FAILURE(buildRandomOffsets(
        matrixRandomOffsets, budget.objectCount,
        matrixRandomTime, randomGenerator));
    DOMMY_RETURN_IF_FAILURE(buildRandomOffsets(
        translateRandomOffsets, budget.objectCount,
        translateRandomTime, randomGenerator));
    DOMMY_RETURN_IF_FAILURE(buildRandomOffsets(
        rotateRandomOffsets, budget.objectCount,
        rotateRandomTime, randomGenerator));
    DOMMY_RETURN_IF_FAILURE(buildRandomOffsets(
        scaleRandomOffsets, budget.objectCount,
        scaleRandomTime, randomGenerator));

    MFnDependencyNode dependencyNode(thisMObject(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MPlug matrixPlug;
    MPlug offsetPlugs[3];
    MPlug translatePlugs[3];
    MPlug rotatePlugs[3];
    MPlug scalePlugs[3];
    DOMMY_RETURN_IF_FAILURE(findNodePlug(
        dependencyNode, transformMatrix, matrixPlug));
    const MObject offsetAttributes[3] = {offsetX, offsetY, offsetZ};
    const MObject translateAttributes[3] = {
        translateX, translateY, translateZ};
    const MObject rotateAttributes[3] = {rotateX, rotateY, rotateZ};
    const MObject scaleAttributes[3] = {scaleX, scaleY, scaleZ};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        DOMMY_RETURN_IF_FAILURE(findNodePlug(
            dependencyNode, offsetAttributes[axis], offsetPlugs[axis]));
        DOMMY_RETURN_IF_FAILURE(findNodePlug(
            dependencyNode, translateAttributes[axis],
            translatePlugs[axis]));
        DOMMY_RETURN_IF_FAILURE(findNodePlug(
            dependencyNode, rotateAttributes[axis], rotatePlugs[axis]));
        DOMMY_RETURN_IF_FAILURE(findNodePlug(
            dependencyNode, scaleAttributes[axis], scalePlugs[axis]));
    }

    bool matrixConnected = false;
    std::array<bool, 3> offsetConnected = {{false, false, false}};
    std::array<bool, 3> translateConnected = {{false, false, false}};
    std::array<bool, 3> rotateConnected = {{false, false, false}};
    std::array<bool, 3> scaleConnected = {{false, false, false}};
    DOMMY_RETURN_IF_FAILURE(
        plugConnectionState(matrixPlug, matrixConnected));
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        DOMMY_RETURN_IF_FAILURE(
            plugConnectionState(offsetPlugs[axis], offsetConnected[axis]));
        DOMMY_RETURN_IF_FAILURE(plugConnectionState(
            translatePlugs[axis], translateConnected[axis]));
        DOMMY_RETURN_IF_FAILURE(
            plugConnectionState(rotatePlugs[axis], rotateConnected[axis]));
        DOMMY_RETURN_IF_FAILURE(
            plugConnectionState(scalePlugs[axis], scaleConnected[axis]));
    }

    std::vector<MMatrix> sampledMatrices;
    if (matrixConnected) {
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::sampleMatrixPlugSeries(
                sampledMatrices, matrixPlug, budget.objectCount,
                evaluationTime, -delayValue, -delayValue,
                matrixRandomOffsets));
    }

    std::array<std::vector<double>, 3> sampledOffsets;
    std::array<std::vector<double>, 3> sampledTranslations;
    std::array<std::vector<double>, 3> sampledRotations;
    std::array<std::vector<double>, 3> sampledScales;
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        if (offsetConnected[axis]) {
            const MTime zeroTime(0.0, evaluationTime.unit());
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::sampleScalarPlugSeries(
                    sampledOffsets[axis], offsetPlugs[axis],
                    budget.objectCount, zeroTime,
                    offsetEvaluationTime[axis],
                    offsetEvaluationTime[axis],
                    std::vector<double>()));
        } else {
            sampledOffsets[axis].reserve(budget.objectCount);
            for (std::size_t index = 0U;
                 index < budget.objectCount; ++index) {
                const double value =
                    globalOffset[axis] * static_cast<double>(index + 1U);
                if (!std::isfinite(value)) return MS::kFailure;
                sampledOffsets[axis].push_back(value);
            }
        }

        if (translateConnected[axis]) {
            DOMMY_RETURN_IF_FAILURE(sampleTimedValues(
                sampledTranslations[axis], translatePlugs[axis],
                budget.objectCount, translateTiming, delayValue,
                translateDelayValue, translateEvaluationTime[axis],
                translateRandomOffsets, evaluationTime));
        }
        if (rotateConnected[axis]) {
            DOMMY_RETURN_IF_FAILURE(sampleTimedValues(
                sampledRotations[axis], rotatePlugs[axis],
                budget.objectCount, rotateTiming, delayValue,
                rotateDelayValue, rotateEvaluationTime[axis],
                rotateRandomOffsets, evaluationTime));
        }
        if (scaleConnected[axis]) {
            DOMMY_RETURN_IF_FAILURE(sampleTimedValues(
                sampledScales[axis], scalePlugs[axis],
                budget.objectCount, scaleTiming, delayValue,
                scaleDelayValue, scaleEvaluationTime[axis],
                scaleRandomOffsets, evaluationTime));
        }
    }

    std::vector<MObject> outputDataObjects;
    outputDataObjects.reserve(budget.objectCount);
    for (std::size_t index = 0U; index < budget.objectCount; ++index) {
        const MVector translation(
            sampledOrDefault(
                sampledTranslations[0], index, baseTranslation[0]),
            sampledOrDefault(
                sampledTranslations[1], index, baseTranslation[1]),
            sampledOrDefault(
                sampledTranslations[2], index, baseTranslation[2]));
        double rotation[3] = {
            dommy::math::degreesToRadians(sampledOrDefault(
                sampledRotations[0], index, baseRotation[0])),
            dommy::math::degreesToRadians(sampledOrDefault(
                sampledRotations[1], index, baseRotation[1])),
            dommy::math::degreesToRadians(sampledOrDefault(
                sampledRotations[2], index, baseRotation[2]))};
        double scaleValues[3] = {
            sampledOrDefault(sampledScales[0], index, baseScale[0]),
            sampledOrDefault(sampledScales[1], index, baseScale[1]),
            sampledOrDefault(sampledScales[2], index, baseScale[2])};
        const std::array<double, 3> offsetValues = {{
            sampledOffsets[0][index],
            sampledOffsets[1][index],
            sampledOffsets[2][index]}};
        if (!dommy::maya::isFiniteVector(translation) ||
            !std::isfinite(rotation[0]) ||
            !std::isfinite(rotation[1]) ||
            !std::isfinite(rotation[2]) ||
            !std::isfinite(scaleValues[0]) ||
            !std::isfinite(scaleValues[1]) ||
            !std::isfinite(scaleValues[2])) {
            return MS::kFailure;
        }

        const MMatrix sampledMatrix = sampledMatrices.empty()
            ? baseMatrix
            : sampledMatrices[index];
        MTransformationMatrix transform(sampledMatrix);
        DOMMY_RETURN_IF_FAILURE(transform.setRotatePivot(
            MPoint(rotatePivot[0], rotatePivot[1], rotatePivot[2]),
            MSpace::kObject, false));
        DOMMY_RETURN_IF_FAILURE(transform.setScalePivot(
            MPoint(scalePivot[0], scalePivot[1], scalePivot[2]),
            MSpace::kObject, false));
        DOMMY_RETURN_IF_FAILURE(
            transform.addScale(scaleValues, MSpace::kObject));
        DOMMY_RETURN_IF_FAILURE(transform.addRotation(
            rotation, MTransformationMatrix::kXYZ, MSpace::kObject));
        DOMMY_RETURN_IF_FAILURE(
            transform.addTranslation(translation, MSpace::kObject));
        const MMatrix finalMatrix = transform.asMatrix();
        if (!dommy::maya::isFiniteMatrix(finalMatrix)) {
            return MS::kFailure;
        }

        MPointArray transformedPoints;
        DOMMY_RETURN_IF_FAILURE(transformPoints(
            sourcePoints, finalMatrix, offsetValues, transformedPoints));

        MObject outputData;
        switch (geometryKind) {
            case kMotionCurve: {
                MFnNurbsCurveData creator;
                outputData = creator.create(&status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (outputData.isNull()) return MS::kFailure;
                const MObject curveObject =
                    inputCurve.copy(inputObjectData, outputData, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (curveObject.isNull()) return MS::kFailure;
                MFnNurbsCurve outputCurve(curveObject, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                DOMMY_RETURN_IF_FAILURE(
                    outputCurve.setCVs(transformedPoints));
                break;
            }
            case kMotionSurface: {
                MFnNurbsSurfaceData creator;
                outputData = creator.create(&status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (outputData.isNull()) return MS::kFailure;
                const MObject surfaceObject =
                    inputSurface.copy(inputObjectData, outputData, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (surfaceObject.isNull()) return MS::kFailure;
                MFnNurbsSurface outputSurface(surfaceObject, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                DOMMY_RETURN_IF_FAILURE(
                    outputSurface.setCVs(transformedPoints));
                break;
            }
            case kMotionMesh: {
                MFnMeshData creator;
                outputData = creator.create(&status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (outputData.isNull()) return MS::kFailure;
                const MObject meshObject =
                    inputMesh.copy(inputObjectData, outputData, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (meshObject.isNull()) return MS::kFailure;
                MFnMesh outputMesh(meshObject, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                DOMMY_RETURN_IF_FAILURE(
                    outputMesh.setPoints(
                        transformedPoints, MSpace::kObject));
                break;
            }
        }
        outputDataObjects.push_back(outputData);
    }

    return dommy::maya::setObjectArrayOutput(
        block, outputObjects, plug, outputDataObjects);
}
DOMMY_COMPUTE_CATCH("DTMotionDuplicator")
