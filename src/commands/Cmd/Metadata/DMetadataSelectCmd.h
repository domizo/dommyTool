//
//  DMetadataSelectCmd.hpp
//  dommyTool
//
//  Created by yuzo on 12/23/16.
//
//

#pragma once


#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MPxCommand.h>

class DMetadataSelectCmd final : public MPxCommand
{
public:
	MStatus	doIt ( const MArgList& ) override;
    static void *creator() {
        return ::dommy::maya::createObject("DMetadataSelectCmd", [&]() {
            return new DMetadataSelectCmd;
        });
    }
};
