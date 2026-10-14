//
//  DBlindDataSelectCmd.hpp
//  dommyTool
//
//  Created by yuzo on 12/21/16.
//
//


#pragma once


#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxCommand.h>

class DBlindDataSelectCmd final : public MPxCommand
{
public:
	MStatus	doIt ( const MArgList& ) override;
    static void *creator() {
        return ::dommy::maya::createObject("DBlindDataSelectCmd", [&]() {
            return new DBlindDataSelectCmd;
        });
    }
};
