//
//  polyBlastCmd.h
//  dommyTool
//
//  Created by yuzo on 1/30/13.
//
//

#ifndef __dommyTool__polyBlastCmd__
#define __dommyTool__polyBlastCmd__


#include "basePolyModifierCmd.h"
#include "polyBlastFty.h"

class polyBlast final : public basePolyModifierCmd {
	
public:
	////////////////////
	// Public Methods //
	////////////////////
	
	polyBlast() = default;
	~polyBlast() override = default;
	
	static		void* creator();
	
	bool		isUndoable() const override;
	
	MStatus		doIt( const MArgList& ) override;
	MStatus		redoIt() override;
	MStatus		undoIt() override;
	
	/////////////////////////////
	// polyModifierCmd Methods //
	/////////////////////////////
	
	MStatus		initModifierNode( MObject modifierNode ) override;
	MStatus		directModifier( MObject mesh ) override;
	
private:
	// Retained only for the current command so history and direct-modification
	// paths consume the same validated face selection.
	MObject fComponentList;
	MIntArray pSelectedFaces;
	polyBlastFty			pBlastFactory;
};

#endif /* defined(__dommyTool__polyBlastCmd__) */
