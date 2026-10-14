//
//  DCurveShape.hpp
//  dommyTool
//
//  Created by yuzo on 3/25/17.
//
//

#pragma once

#include "DMayaHeaders.h"

#include "DCurveDataHeaders.h"

#include <maya/MPxSurfaceShape.h>

#include <memory>

class DCurveShape : public MPxSurfaceShape {
  public:
    DCurveShape();
    ~DCurveShape() override;

    void postConstructor() override;
    MStatus compute(const MPlug&, MDataBlock&) override;

    bool isBounded() const override;
    MBoundingBox boundingBox() const override;

    static void* creator();
    static MStatus initialize();
    const DCurveData* geometry() const;

  private:
    mutable std::unique_ptr<DCurveData> dCurveGeom;

    // Attributes
    //
    static MObject inDC;

    static MObject aIsMultiEndKnot;

    static MObject aCurveWidthType;
    static MObject aConstantWidth;
    static MObject aWidthAttributeName;

    static MObject aExportCurveAttribute;
    static MObject aExportPointAttribute;

    // for arnold

    static MObject aRenderMode;
    static MObject aBasisMode;
    static MObject aMinPixelWidth;

    static MObject aOrientType;
    static MObject aOrientAttributeName;

  public:
    // Shape type id
    //
    static MTypeId id;
};
