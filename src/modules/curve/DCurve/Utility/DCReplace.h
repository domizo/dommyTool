//
//  DCReplace.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/26.
//

#pragma once



#include <maya/MPxNode.h>


#include "DType.h"

//指定したグループ毎でDCItemを置き換えます。
class DCReplace final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    static MObject aTargetDC;
    
    static MObject aSource;
    static MObject aRestMatrix;
    
    
    static MObject outDC;
    
private:
    
};
