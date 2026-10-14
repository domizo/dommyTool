//
//  DMRest.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/02.
//

#pragma once

#include <maya/MPxNode.h>


///DMeshのポイントをローカルからワールド、あるいはワールドからローカルに変換する
class DMRest final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDM;
    
    static MObject aType;
    
    static MObject outDM;
    
};
