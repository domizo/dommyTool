//
//  DCExplodeView.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/28.
//






#pragma once



#include <maya/MPxNode.h>

//DCurveをアイテム毎にベクトルの方向に拡散
class DCExplodeView final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject aEnable;
    
    static MObject aScale;
    
    static MObject aCenter;
    static MObject aPieceCenterAttributeName;
    
    
    static MObject aVectorType;
    static MObject aVectorAttributeName;
    
    
    static MObject aPieceGroup;
    
    
    static MObject outDC;
    
    
private:
    
};
