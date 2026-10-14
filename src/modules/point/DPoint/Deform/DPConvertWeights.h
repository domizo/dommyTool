//
//  DPConvertWeights.hpp
//  dommyTool
//
//  Created by yuzo on 2016/04/30.
//
//

#pragma once





#include <maya/MPxNode.h>

/*****************************************************************

double dataの配列データをデフォーマなどのウェイトデータに変換
 
*****************************************************************/


class DPConvertWeights final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inputDataArray;
    
    static MObject aWeightsList;
    static MObject aWeights;
    
};


