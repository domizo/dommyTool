//
//  DCResample.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/22.
//
//




#pragma once



#include <maya/MPxNode.h>

//カーブを再構築
class DCResample final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    
    static MObject aSampleType;
    
    static MObject aLength;
    static MObject aNumSpan;
    static MObject aDegree;
    static MObject aKeepRange;
    static MObject aEndKnots;
    
    static MObject aKeepEndPoints;
    static MObject aKeepTangents;
    static MObject aKeepCV;
    static MObject aTolerance;
    
    
    
    static MObject outDC;
    
private:
    
};

