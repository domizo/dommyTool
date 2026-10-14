//
//  DCurveClothFromSurface.hpp
//  dommyTool
//
//  Created by yuzo on 5/6/16.
//
//

#pragma once

#include <maya/MPxNode.h>


class DCurveClothFromSurface final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inSurface;
	
	static MObject samplingU;
	static MObject samplingV;
	
	static MObject waveHeight;
	
	static MObject aAddBiasPoint;
    static MObject aWaveWidthBias;
	
    //attribute
    static MObject aAddNormalAttribute;
    static MObject aNormalAttributeName;
    
	static MObject outDC;
	
private:
	
};
