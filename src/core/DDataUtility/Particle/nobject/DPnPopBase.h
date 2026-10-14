//
//  DPnPopBase.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/21.
//

#pragma once


#include "DMayaHeaders.h"


class DPnPopBase : public MPxNode
{
public:
    
    virtual MStatus getParam();
    virtual MStatus runProcess();
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    
    static MObject inPop;
    static MObject outPop;
    
private:
    
};
