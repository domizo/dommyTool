//
//  DCurlNoiseField.h
//  dommyTool
//
//  Created by yuzo on 2015/03/18.
//
//

#ifndef DOMMY_DCURL_NOISE_FIELD_H
#define DOMMY_DCURL_NOISE_FIELD_H

#include <maya/MVector.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MPxFieldNode.h>

class DCurlNoiseField final : public MPxFieldNode {
  public:
    DCurlNoiseField() = default;
    ~DCurlNoiseField() override = default;

    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& block) override;

    void draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;

    //
    // attributes.
    //

    static MObject curlSpeed;
    static MObject curlAmplitude;
    static MObject curlAgeSpeed;

    static MObject noiseFrequency;
    static MObject noiseFrequencyX;
    static MObject noiseFrequencyY;
    static MObject noiseFrequencyZ;

    static MObject phaseRate;
    static MObject phaseRateX;
    static MObject phaseRateY;
    static MObject phaseRateZ;

    static MObject phaseOffset;
    static MObject phaseOffsetX;
    static MObject phaseOffsetY;
    static MObject phaseOffsetZ;

    // base perlinNoise attribute
    static MObject positionNoiseFrequency;
    static MObject positionNoiseScale;
    static MObject positionNoiseScaleX;
    static MObject positionNoiseScaleY;
    static MObject positionNoiseScaleZ;
    // Other data members
    //
    static MTypeId id;

  private:
    MStatus calculateForces(MDataBlock& block, const MVectorArray& points, const MDoubleArray& ages,
                            MVectorArray& outputForce);
    MStatus ownerPosition(MDataBlock& block, MVectorArray& ownerPositions);
    MStatus getWorldPosition(MDataBlock& block, MVector& position);
};

#endif // DOMMY_DCURL_NOISE_FIELD_H
