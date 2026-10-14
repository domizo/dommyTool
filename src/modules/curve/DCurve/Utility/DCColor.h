//
//  DCColor.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/01.
//
//

//カーブのカラーを適用


#pragma once



#include <maya/MPxNode.h>


class DCColor final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
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
    
    
    
    static MObject outDC;

    
private:
    
};
