//
//  icosahedronNode.h
//  dommyTool
//
//  Created by yuzo on 3/29/13.
//
//

#ifndef __dommyTool__icosahedronNode__
#define __dommyTool__icosahedronNode__

#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPxNode.h>
class icosahedronNode final : public MPxNode {
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
	//attribute
	static MObject frequency;
	static MObject radius;
	
	//output
    static MObject outPutMesh;
};

#endif /* defined(__dommyTool__icosahedronNode__) */
