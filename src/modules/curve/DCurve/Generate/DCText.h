//
//  DCText.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/16.
//


#pragma once

#include <maya/MPxNode.h>

///FontDataからDCurveを生成
class DCText final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
    //Attribute
    static MObject aText;
    static MObject aFontName;
    static MObject aSize;
    static MObject aSampling;
    
    static MObject aTracking;
    
    static MObject aGroupCharPiece;
    static MObject aGroupName;
    
    static MObject outDC;
};


