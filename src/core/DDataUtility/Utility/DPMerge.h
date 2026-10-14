//
//  DPMerge.h
//  dommyTool
//
//  Created by yuzo on 2015/03/26.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPMerge final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject inDPs;
    static MObject outDP;
};
