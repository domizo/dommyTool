//
//  DCExtend.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/09.
//




#pragma once


#include <maya/MPxNode.h>

//カーブの最初、あるいは最後のベクトルを利用してカーブの長さを伸ばす
class DCExtend final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aDirectionType;
    
    static MObject aStartLength;
    static MObject aEndLength;
    
    static MObject aStartInvert;
    static MObject aEndInvert;
    
    
    static MObject aAnotherCurve;
    static MObject aIsDeleteOriginal;
    static MObject aStartOffset;
    static MObject aEndOffset;
    
    
    
    static MObject outDC;
    
private:
    
};
