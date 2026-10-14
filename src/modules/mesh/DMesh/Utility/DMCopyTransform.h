//
//  DMCopyTransform.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/30.
//


#pragma once



#include <maya/MPxNode.h>

//指定した数分のメッシュをコピー生成する
class DMCopyTransform final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aCount;
    
    static MObject aTranslate;
    static MObject aRotate;
    static MObject aScale;
    static MObject aPivot;
    static MObject aUniformScale;
    
    static MObject aRotateOrder;
    
    static MObject aFreezeTransform;
    
    static MObject outDM;
    
    
private:
    
};


