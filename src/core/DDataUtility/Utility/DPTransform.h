//
//  DPTransform.h
//  dommyTool
//
//  Created by yuzo on 3/25/15.
//
//

#pragma once

#include <maya/MPxNode.h>

#include <maya/MPxManipContainer.h>
#include "DType.h"

/// Applies a transform directly to supported point-backed custom data.
class DPTransform final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;

    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;

    static MObject inDD;

    static MObject aTranslate;
    static MObject aRotateX;
    static MObject aRotateY;
    static MObject aRotateZ;
    static MObject aRotate;
    static MObject aRotatePivot;
    static MObject aScale;

    static MObject transformMatrix;

    static MObject outDD;

  private:
    MStatus process(const std::vector<DPointList*>& points, MDataBlock& data);
};

class DPTransformManip final : public MPxManipContainer {
  public:
    DPTransformManip() = default;
    ~DPTransformManip() override = default;

    static void* creator();
    static MStatus initialize();
    static MTypeId id;

    MStatus createChildren() override;
    MStatus connectToDependNode(const MObject& node) override;

  private:
    MDagPath fTranslateManip;
    MDagPath fRotateManip;
    MDagPath fScaleManip;
};
