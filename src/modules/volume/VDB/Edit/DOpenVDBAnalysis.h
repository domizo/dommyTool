//
//  DOpenVDBAnalysis.h
//  dommyTool
//
//  Created by yuzo on 2017/01/11.
//
//

#pragma once

#include <maya/MPxNode.h>

class DOpenVDBAnalysis final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;

    static MObject aOperator;

    static MObject aMaskVdbInput;
    static MObject aMaskGridNames;

    static MObject aOutputName;
    static MObject aCustomName;

    static MObject aVdbOutput;
};
