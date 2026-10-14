//
//  DClusterWeightCmd.hpp
//  dommyTool
//
//  Created by yuzo on 2018/06/07.
//



#pragma once


#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MDagPath.h>
#include <maya/MFloatArray.h>
#include <maya/MObject.h>
#include <maya/MObjectHandle.h>
#include <maya/MPoint.h>
#include <maya/MPxCommand.h>

class DClusterWeightCmd final : public MPxCommand
{
public:
    DClusterWeightCmd();
    ~DClusterWeightCmd() override;
    MStatus doIt(const MArgList&) override;
    MStatus undoIt() override;
    MStatus redoIt() override;
    bool isUndoable() const override { return true; }
    
    static void *creator() {
        return ::dommy::maya::createObject("DClusterWeightCmd", [&]() {
            return new DClusterWeightCmd;
        });
    }
    static MSyntax newSyntax();
    static MStatus buildSyntax(MSyntax& syntax);
    MStatus parseArgs(const MArgList& args);
    
    
    
private:
    MStatus buildWeightPlan(MObject sourceComponent);
    MStatus applyWeights(MFloatArray& weights);

    MString sourceMeshName;
    MString targetClusterName;
    MPoint centerPt;
    double calcDistance;

    MDagPath srcMeshPath;
    MObjectHandle clusterHandle;
    MObject componentObject;
    MFloatArray originalWeights;
    MFloatArray targetWeights;
    bool planReady;
    bool applied;
};
