//
//  DMInfo.hpp
//  dommyTool
//
//  Created by yuzo on 2018/04/17.
//

#pragma once



#include <maya/MPxNode.h>




//DMeshの現在のシェイプの情報をターゲットシェイプ等を利用してアトリビュートを作成
class DMInfo final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    static MObject targetDM;
    
//    static MObject aAttributeName;
    
//    static MObject aIsDeform;
//    static MObject aIsVector;
    static MObject aVectorScale;
    static MObject aNormalize;
    
    static MObject outDM;
    
    
private:
    
    
    
};
