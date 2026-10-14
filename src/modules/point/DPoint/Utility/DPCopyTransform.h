//
//  DPCopyTransform.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/15.
//

#pragma once



#include <maya/MPxNode.h>

//指定した数分のDDataをコピー生成する
class DPCopyTransform final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDD;
    
    static MObject aCount;
    
    static MObject aTranslate;
    static MObject aRotate;
    static MObject aScale;
    static MObject aPivot;
    static MObject aUniformScale;
    
    static MObject aRotateOrder;
    
    //for dmesh and dcurve
    static MObject aFreezeTransform;
    
    static MObject outDD;
    
    
private:
    
};
