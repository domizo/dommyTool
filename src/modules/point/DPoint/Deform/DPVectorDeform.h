//
//  DPVectorDeform.hpp
//  dommyTool
//
//  Created by yuzo on 2017/04/01.
//
//
//ポイントをベクトルデータによって変形させます


#pragma once



#include <maya/MPxNode.h>


class DPVectorDeform final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDP;
    
    
    
    static MObject aVectorType;
    static MObject aBaseVector;
    static MObject aVectorAttributeName;
    
    static MObject aMagnitude;
    
    static MObject aUseClamp;
    static MObject aMaxStrength;
    
    static MObject aWeightAttributeName;
    static MObject aWeight;
    
    
    static MObject outDP;
    
    
private:
    
    
    
};

