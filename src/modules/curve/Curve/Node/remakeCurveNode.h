//
//  remakeCurveNode.h
//  dommyTool
//
//  Created by yuzo on 1/9/15.
//
//

#pragma once

#include <maya/MPxNode.h>

class remakeCurveNode final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject inputCurve;

    static MObject sampling;
    static MObject curveLength;
    static MObject offset;

    static MObject outputCurve;
};
