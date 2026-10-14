//
//  DOpenVDBClip.h
//  dommyTool
//
//  Created by yuzo on 2017/01/06.
//
//

#pragma once

#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class DOpenVDBClip final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;

    static MObject aMaskName;
    static MObject aMaskVdbInput;

    static MObject aClipType;
    static MObject aInObject;
    static MObject aClipAxis;
    static MObject aClipPosition;
    static MObject aClipWidth;

    static MObject aKeepInside;

    static MObject aVdbOutput;
};
