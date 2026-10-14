//
//  DMExplodeView.hpp
//  dommyTool
//
//  Created by yuzo on 2017/04/29.
//
//





#pragma once



#include <maya/MPxNode.h>

//DMeshをベクトルの方向に拡散
class DMExplodeView final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aEnable;
    
    
    static MObject aScale;
    
    static MObject aCenter;
    
    static MObject aPieceCenterAttributeName;
    
    
    static MObject aVectorType;
    static MObject aVectorAttributeName;
    
    
    static MObject aPieceGroup;
    
    
    static MObject outDM;
    
    
private:
    
};





