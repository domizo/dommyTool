//
//  DMColor.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/12.
//
//





#pragma once



#include <maya/MPxNode.h>

///DMeshのカラーを適用
class DMColor final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aAssigneType;
    static MObject aColorType;
    static MObject aColorName;
    static MObject aUVAttrName;
    //solid
    static MObject aSolidColor;
    //ramp
    static MObject aRampColor;
    //texture
    static MObject aIs3DCalc;
    static MObject aTexColor;
    
    //random
    static MObject aSeed;
    static MObject aMinRange;
    static MObject aMaxRange;
    
    //expression
    static MObject aExpression;
    
    //palette
    static MObject aPaletteType;
    static MObject aPaletteColor;
    
    
    
    static MObject outDM;
    
    
private:
    
};


