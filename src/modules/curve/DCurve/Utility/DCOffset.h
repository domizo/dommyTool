//
//  DCOffset.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/13.
//

//DCurveを特定のVectorを軸にオフセットする

#pragma once


#include <maya/MPxNode.h>


class DCOffset final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aDirectionType;
    static MObject aOffsetVector;
    static MObject aOffsetVectorAttribute;
    
    static MObject aUseAimVector;
    static MObject aAimVector;
    static MObject aUpVector;
    
    static MObject aDsitance;
    
    static MObject aSubdivide;
    
    static MObject aCheckOverlap;
    static MObject aTolerance;
    static MObject outDC;
    
private:
    
};

