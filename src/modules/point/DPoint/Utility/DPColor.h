//
//  DPColor.hpp
//  dommyTool
//
//  Created by yuzo on 2016/06/23.
//
//

#ifndef DPColor_h
#define DPColor_h


#include <maya/MPxNode.h>

class DPColor final : public MPxNode
{
public:
    void postConstructor() override;
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDP;
    
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
    
    
    
    static MObject outDP;
    
};



#endif /* DPColor_h */
