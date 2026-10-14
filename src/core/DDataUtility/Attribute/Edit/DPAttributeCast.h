//
//  DPAttributeCast.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/08.
//
//







#pragma once



#include <maya/MPxNode.h>



enum {
    DPA_CAST_TO_INT = 0,
    DPA_CAST_TO_FLOAT,
    DPA_CAST_TO_DOUBLE,
    DPA_CAST_TYPE_COUNT
};typedef short DPA_CAST_TYPE;



///DPointのアトリビュートタイプを変更
class DPAttributeCast final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDP;
    
    static MObject aAttributeName;
    
    static MObject aCastType;
    
    static MObject outDP;
    
    
private:
    
};
