//
//  polyBlastNode.h
//  dommyTool
//
//  Created by yuzo on 1/30/13.
//
//

#ifndef __dommyTool__polyBlastNode__
#define __dommyTool__polyBlastNode__



#include "basePolyModifierNode.h"
#include "polyBlastFty.h"

// General Includes
//
#include <maya/MTypeId.h>

class polyBlastNode final : public basePolyModifierNode {
public:
	MStatus compute( const MPlug& plug, MDataBlock& data ) override;
	
	static  void*		creator();
	static  MStatus		initialize();
	
public:
	
	// There needs to be a MObject handle declared for each attribute that
	// the node will have.  These handles are needed for getting and setting
	// the values later.
	//
	// polyModifierNode has predefined the standard inMesh and outMesh attributes.
	//
	// We define an input attribute for our UV list input
	//
	static  MObject		faceList;
	static  MObject		deletePercent;
	
	// The typeid is a unique 32bit indentifier that describes this node.
	// It is used to save and retrieve nodes of this type from the binary
	// file format.  If it is not unique, it will cause file IO problems.
	//
	static	MTypeId		id;
	
	polyBlastFty			pBlastFactory;
};



#endif /* defined(__dommyTool__polyBlastNode__) */
