//
//  DPointDataExportCmd.h
//  dommyTool
//
//  Created by yuzo on 3/14/15.
//
//

#pragma once

#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <vector>

class DPointDataExportCmd final : public MPxCommand
{
public:
	DPointDataExportCmd() = default;
	~DPointDataExportCmd() override = default;

	MStatus doIt(const MArgList& args) override;
    static void* creator() {
        return ::dommy::maya::createObject("DPointDataExportCmd", [&]() {
            return new DPointDataExportCmd;
        });
    }
	static MSyntax newSyntax();
    static MStatus buildSyntax(MSyntax& syntax);

private:
	MStatus parseArgs(const MArgList& args);

	MString filePath;
	std::vector<MString> savedNodeNames;
};
