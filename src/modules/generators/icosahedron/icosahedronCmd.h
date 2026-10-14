//
//  icosahedronCmd.h
//  dommyTool
//
//  Created by yuzo on 3/29/13.
//
//

#pragma once

#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MDagModifier.h>
#include <maya/MObject.h>
#include <maya/MPxCommand.h>

class icosahedronCmd final : public MPxCommand
{
public:
    MStatus doIt(const MArgList&) override;
    MStatus undoIt() override;
    MStatus redoIt() override;
    bool isUndoable() const override { return true; }
    
    static void *creator() {
        return ::dommy::maya::createObject("icosahedronCmd", [&]() {
            return new icosahedronCmd;
        });
    }
    
private:
    MDagModifier dagMod;
    MObject outputMeshNode;
    MObject shadingGroup;
    bool hasShadingMembership = false;
};
