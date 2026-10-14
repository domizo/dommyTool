


#ifndef SPACEDUPLICATERCMD_H
#define SPACEDUPLICATERCMD_H

#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxCommand.h>
#include <maya/MDagModifier.h>
#include <maya/MObject.h>

class spaceDuplicaterCmd final : public MPxCommand 
	{
	public:
		MStatus	doIt ( const MArgList& ) override;
		MStatus undoIt() override;
		MStatus redoIt() override;
		bool isUndoable() const override { return true; }
		
        static void *creator() {
            return ::dommy::maya::createObject("spaceDuplicaterCmd", [&]() {
                return new spaceDuplicaterCmd;
            });
        }
		static MSyntax newSyntax();
		
	private:
		MDagModifier dagMod;
		MObject outputMeshNode;
		MObject shadingGroup;
		bool hasShadingMembership = false;
	};

#endif
