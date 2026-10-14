//
//  DPMatrix.hpp
//  dommyTool
//
//  Created by yuzo on 2016/03/05.
//
//

#ifndef DPMatrix_h
#define DPMatrix_h


#include <maya/MPxNode.h>

//matrixのデータ配列を作成します
//inDPが接続されている場合はその配列のサイズ分、
//そうでない場合はdataSizeによって決定されます。

class DPMatrix final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inMatrix;
    
    static MObject dataSize;
    
    //random
	static MObject seed;
    static MObject translateNoise;
    static MObject rotateNoise;
    static MObject scaleNoise;
    
    static MObject outMatrixArray;
    
};


#endif /* DPMatrix_h */
