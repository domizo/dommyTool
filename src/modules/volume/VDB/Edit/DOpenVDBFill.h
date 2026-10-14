//
//  DOpenVDBFill.h
//  dommyTool
//
//  Created by yuzo on 2017/12/16.
//

#pragma once
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class DOpenVDBFill final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;

    static MObject aBounds;
    static MObject aMinCoord;
    static MObject aMaxCoord;
    static MObject aMinWorldCoord;
    static MObject aMaxWorldCoord;

    static MObject aInMesh;

    static MObject aValue;
    static MObject aActive;
    static MObject aSparse;

    static MObject aVdbOutput;
};
