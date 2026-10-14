//
//  DCFillBetween.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/27.
//

#pragma once


#include <maya/MPxNode.h>

///DCurveとDcurveの間を形をブレンドしながら埋める
class DCFillBetween final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aCurveCount;
    static MObject aSampling;
    
    static MObject aExportPrimitiveAttribute;
    static MObject aExportPointAttribute;
    
    static MObject outDC;
    
private:
    
};


