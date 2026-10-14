//
//  DPMeshDuplicator.hpp
//  dommyTool
//
//  Created by yuzo on 3/1/16.
//
//

#pragma once


#include <maya/MPxNode.h>

#include "DTransformOp.h"


class DPMeshDuplicator final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
    
	MStatus evalAnimMatrixParms(MDataBlock& data, DTransformParams& parms);
	
	
	static MObject inDP;
	
    static MObject inDM;
	static MObject aInObjects;
    
    static MObject aObjectType;
    static MObject aObjectNumber;
    static MObject aSeed;
    static MObject aObjectIdAttribute;
    
    
    //normal and tangent
    static MObject aAimVector;
    static MObject aAxisVector;
    static MObject aNormalVector;
    static MObject aNormalAttribute;
    static MObject aTangentVector;
    static MObject aTangentAttribute;
	
    
    static MObject aAnimationType;
	static MObject aAnimMatrixAttribute;
	static MObject aInAnimMatrix;
    static MObject aMatrixAssignType;
    
    static MObject aLocalTranslate;
    static MObject aGlobalTranslate;
    static MObject aLocalRotate;
    static MObject aGlobalRotate;
    static MObject aGlobalScale;
    static MObject aUniformScale;
	
	static MObject aIsOutUV;
	static MObject aUVType;
    
    //attribute
    static MObject aLocalTranslateAttribute;
    static MObject aGlobalTranslateAttribute;
    static MObject aLocalRotateAttribute;
    static MObject aGlobalRotateAttribute;
    static MObject aGlobalScaleAttribute;
    static MObject aUniformScaleAttribute;
    
    //random
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
    
    //transfer Attribute
    static MObject aTransferAttributes;
	
	
	static MObject aExpression;
	
    static MObject outDM;
    
};


