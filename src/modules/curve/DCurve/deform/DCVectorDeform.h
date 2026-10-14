//
//  DCVectorDeform.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/23.
//
//

//カーブをベクトルデータによって変形させます


#pragma once



#include <maya/MPxNode.h>


class DCVectorDeform final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    
    
    static MObject aVectorType;
    static MObject aBaseVector;
    static MObject aVectorAttributeName;
    
    static MObject aMagnitude;
    
    static MObject aWeightAttributeName;
    static MObject aWeight;
    
    
    static MObject outDC;
    
private:
    
};



