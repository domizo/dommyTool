//
//  DPEditDouble.hpp
//  dommyTool
//
//  Created by yuzo on 2016/03/13.
//
//

#pragma once


#include <maya/MPxNode.h>


class DPEditDouble final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inputDataArray;
    
    static MObject isUseExpression;
    static MObject expression;
    static MObject exValue;
    
    static MObject editDataArray;
    static MObject dataIndex;
    static MObject dataValue;
    
    static MObject outDataArray;
    
};

