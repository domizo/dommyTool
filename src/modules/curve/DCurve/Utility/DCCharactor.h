//
//  DCCharactor.hpp
//  dommyTool
//
//  Created by yuzo on 2017/01/26.
//
//




#pragma once



#include <maya/MPxNode.h>

//カーブのwidth,length等の特徴を生成
class DCCharactor final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    void postConstructor() override;
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject aAddLengthAttribute;
    static MObject aLengthAttributeName;
    
    
    
    static MObject aAddWidthAttribute;
    static MObject aWidthAttributeName;
    static MObject aWidthType;
    
    static MObject aBaseWidth;
    static MObject aTipWidth;
    
    static MObject aWidthRamp;
	
    
    static MObject aIsRandom;
    static MObject aSeed;
    static MObject aWidthRange;
    static MObject aWidthScale;
    
	
	
	static MObject aAddParamAttribute;
	static MObject aParamAttributeName;
	static MObject aIsNormalizedParam;
	
	
	
	
	
    static MObject aIsChangeKnot;
    static MObject aKnotType;
    
    static MObject outDC;
    
private:
    
};




