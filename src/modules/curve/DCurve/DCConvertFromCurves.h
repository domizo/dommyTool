//
//  DConvertMultiCurves.hpp
//  dommyTool
//
//  Created by yuzo on 2016/05/01.
//
//

#pragma once


#include <maya/MPxNode.h>


class DCConvertFromCurves final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject aConvertType;
    static MObject inCurves;
    static MObject aInHierarchy;
    
    static MObject aCurrentTime;
    
    static MObject outDC;
    
private:
    
};


