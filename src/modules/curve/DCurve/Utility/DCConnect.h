//
//  DCConnect.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/08.
//




#pragma once


#include <maya/MPxNode.h>

//隣接するDCurve同士を接続して一つのカーブにする
class DCConnect final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aDistance;
    
    static MObject aIsAngleCheck;
    static MObject aAngle;
    
    
    
    //curve Option
    static MObject aIsOpen;
    static MObject aDegree;
    
    static MObject outDC;
    
private:
    
};
