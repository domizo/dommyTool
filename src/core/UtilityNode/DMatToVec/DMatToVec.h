//
//  DMatToVec.hpp
//  dommyTool
//
//  Created by yuzo on 2018/05/20.
//


#pragma once

#include <maya/MPxNode.h>

//MatrixをVector(translate, rotate, scale)に変換
class DMatToVec final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject inMatrix;
    
    static MObject aType;
    static MObject aSpace;
    
    
    static MObject outVector;
    
    
private:
    
};
