//
//  DPPointDeform.h
//  dommyTool
//
//  Created by yuzo on 2017/11/18.
//





#pragma once



#include <maya/MPxNode.h>

///ポイントを参照するポイントの補間値（ラティス）によって変形させます
class DPPointDeform final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDP;
    
    static MObject inRestMesh;
    static MObject inAnimMesh;
    
    static MObject aRadius;
    
    static MObject outDP;
    
    
private:
    
    
    
};
