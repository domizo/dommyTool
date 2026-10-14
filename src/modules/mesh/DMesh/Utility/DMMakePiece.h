//
//  DMMakePiece.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/30.
//

#pragma once



#include <maya/MPxNode.h>


#include "DType.h"

//指定したグループ毎でのDMItemを生成
class DMMakePiece final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aPieceGroupName;
    
    static MObject outDM;
    
    
private:
    
    
    
};
