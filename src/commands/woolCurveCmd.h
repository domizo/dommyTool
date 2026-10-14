#ifndef WOOLCURVECMD_H
#define WOOLCURVECMD_H

#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxCommand.h>
#include <maya/MDGModifier.h>
#include <maya/MDagModifier.h>


class woolCurveCmd final : public MPxCommand
	{
	public:
		MStatus	doIt ( const MArgList& ) override;
		MStatus undoIt() override;
		MStatus redoIt() override;
		bool isUndoable() const override { return true; }
		
        static void *creator() {
            return ::dommy::maya::createObject("woolCurveCmd", [&]() {
                return new woolCurveCmd;
            });
        }
		
	private:
		MDGModifier dgMod;
		MDagModifier dagMod;
	};

#endif
