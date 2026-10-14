//
//  DMayaHeaders.h
//  dommyTool
//
//  Created by yuzo on 5/26/16.
//
//

#ifndef DMayaHeaders_h
#define DMayaHeaders_h






#pragma mark Maya Base
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MConditionMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MSceneMessage.h>

#include <maya/MRandom.h>
#include <maya/MPlane.h>

#pragma mark from 2018
#include <maya/MDGContextGuard.h>


#pragma mark Selection
#include <maya/MSelectionList.h>
#include <maya/MSelectionMask.h>
#include <maya/MItSelectionList.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItDependencyGraph.h>

#include <maya/MFnSet.h>


#pragma mark Eval
#include <maya/MEvaluationNode.h>
#include <maya/MEvaluationManager.h>

#pragma mark Manipulator
#include <maya/MFnDistanceManip.h>
#include <maya/MFnFreePointTriadManip.h>
#include <maya/MFnScaleManip.h>
#include <maya/MFnStateManip.h>
#include <maya/MFnFreePointTriadManip.h>
#include <maya/MFnToggleManip.h>
#include <maya/MFnRotateManip.h>

#pragma mark Animation
#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MAnimUtil.h>

#pragma mark Expression
#include <maya/MFnExpression.h>

#pragma mark M3dview
#include <maya/MShaderManager.h>
#include <maya/MDrawRegistry.h>
#include <maya/MGeometryExtractor.h>

#pragma mark File
#include <maya/MFileObject.h>
#include <maya/MIOStream.h>
#include <maya/MFileIO.h>


#pragma mark Cmd command
#include <maya/MArgList.h>
#include <maya/MArgParser.h>
#include <maya/MArgDatabase.h>
#include <maya/MSyntax.h>

#pragma mark Attribute
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MRampAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MCurveAttribute.h>
#include <maya/MFnMessageAttribute.h>



#pragma mark Data

#include <maya/MString.h>
#include <maya/MStringResource.h>
#include <maya/MPoint.h>
#include <maya/MFloatPoint.h>


#pragma mark Data Array

#include <maya/MIntArray.h>
#include <maya/MFloatArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MVectorArray.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MStringArray.h>
#include <maya/MUintArray.h>

#pragma mark Fn Data
#include <maya/MFnPluginData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnMatrixData.h> 
#include <maya/MFnComponentListData.h>

#pragma mark Fn Data Array
#include <maya/MFnIntArrayData.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnSingleIndexedComponent.h>


#pragma mark Transform
#include <maya/MQuaternion.h>
#include <maya/MFnTransform.h>







#pragma  mark Connection
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>



#pragma  mark geometry
#include <maya/MBoundingBox.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnPfxGeometry.h>
#include <maya/MFnFluid.h>
#include <maya/MFnCamera.h>

#pragma  mark Iteraror
#include <maya/MItGeometry.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshFaceVertex.h>


#pragma  mark MPx
#include <maya/MPxCommand.h>
#include <maya/MPxNode.h>
#include <maya/MPxSurfaceShape.h>
#include <maya/MPxSurfaceShapeUI.h>

#pragma  mark node
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>

#pragma  mark particle dynamics
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnDynSweptGeometryData.h>
#include <maya/MDynSweptLine.h>


#pragma  mark data builder
#include <maya/MArrayDataBuilder.h>


#pragma mark Render
#include <maya/MRenderLine.h>
#include <maya/MRenderLineArray.h>
#include <maya/MMaterial.h>
#include <maya/MDrawData.h>
#include <maya/MDrawRequest.h>

#pragma mark Dynamics
#include <maya/MDynamicsUtil.h>
#include <maya/MFnField.h>

#pragma mark NObject
#include <maya/MFnNObjectData.h>
#include <maya/MnCloth.h>
#include <maya/MnParticle.h>
#include <maya/MnSolver.h>
#include <maya/MnRigid.h>

#pragma mark Joint
#include <maya/MFnIkJoint.h>


#pragma mark Compute
#include <maya/MComputation.h>

#pragma mark meta
#include <maya/adskDataStream.h>
#include <maya/adskDataChannel.h>
#include <maya/adskDataAssociations.h>
#include <maya/adskDataAccessor.h>
#include <maya/adskDataAccessorMaya.h>
#include <maya/adskDataHandle.h>
#include <maya/adskDataStructure.h>


#pragma mark VP
#include <maya/MGeometryExtractor.h>


#include <memory>


#endif /* DMayaHeaders_h */
