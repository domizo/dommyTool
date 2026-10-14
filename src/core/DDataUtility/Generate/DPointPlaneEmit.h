//
//  DPointPlaneEmit.h
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#pragma once

#include <cstddef>
#include <vector>
#include <maya/MDataHandle.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MPoint.h>
#include <maya/MPxNode.h>
#include <maya/MRampAttribute.h>

#include "DPointPackingCommon.h"

#include <random>
enum {
    DT_PLANE_EMIT_GRID = 0,
    DT_PLANE_EMIT_TILE,
    DT_PLANE_EMIT_RANDOM,
    DT_PLANE_EMIT_BLUENOISE,
    DT_PLANE_EMIT_SPHERE_PACKING,
    DT_PLANE_EMIT_PHYLLOTAXIS,
    DT_PLANE_EMIT_TYPE_COUNT
};
typedef short DT_PLANE_EMIT_TYPE;

enum { D_DIST_UNIFORM = 0, D_DIST_POISSON, D_DIST_LOYDS_RELAXATION, D_DISTRIBUTE_TYPE_COUNT };
typedef short D_DISTRIBUTE_TYPE;

enum { D_DF_ANGLE_A = 0, D_DF_ANGLE_B, D_DF_ANGLE_C, D_DF_ANGLE_CUSTOM, D_PHYLLOTAXIS_TYPE_COUNT };
typedef short D_PHYLLOTAXIS_TYPE;

class DPointPlaneEmit final : public MPxNode {
  public:
    void postConstructor() override;
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override {
        return kSerial;
    }

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    // attribute
    static MObject emitType;

    static MObject sizeX;
    static MObject sizeY;

    static MObject gridX;
    static MObject gridY;

    static MObject count;

    // Tile attribute
    static MObject aTileStep;
    static MObject aCenter;

    static MObject aRandomType;

    static MObject seed;

    static MObject aPoissonMinDistance;
    static MObject aRelaxIteration;

    // sphere packing
    static MObject aRadiusRange;
    static MObject aPackingType;
    static MObject aPackingSampling;
    static MObject aPackingGradientBiasRamp;
    static MObject aUseRadiusBias;
    static MObject aRadiusBiasRamp;
    static MObject aBiasRange;

    // for Map
    static MObject inObject;
    static MObject aMap;

    // pre packing Points
    static MObject aPrePackingDP;
    static MObject aRadiusAttribute;
    static MObject aBaseRadius;
    static MObject aPreDPGroupName;

    // Phyllotaxi
    static MObject aSpread;
    static MObject aAngleType;
    static MObject aCustomAngle;

    static MObject outDP;

  private:
    bool getMitchellBestCandidate(PackedCircle& result);
    float getDistanceToNearestSample(const PackedCircle& candidate) const;
    bool placeNewSample();
    bool getMapValues(
        const std::vector<MPoint>& points, std::vector<float>& values);

    std::vector<PackedCircle> placedPoints;
    std::mt19937 m_eng;
    std::uniform_real_distribution<float> m_distribx;
    std::uniform_real_distribution<float> m_distriby;
    MRampAttribute m_packGradientRamp;
    D_PACKING_TYPE m_pack_type = D_PACK_RANDOM;

    bool m_use_rad_bias = false;
    MRampAttribute m_radiusBiasRamp;
    float m_bias_range = 0.0f;
    MDataHandle m_obj_hnd;

    size_t m_max_count = 0;

    float m_grid_minX = 0.0f;
    float m_grid_sizeX = 0.0f;
    std::size_t m_sampling = 0;

    float m_min_rad = 0.0f;
    float m_max_rad = 0.0f;
    float m_rad_size = 0.0f;

    MFnMesh meshFn;
    MFnNurbsSurface srfFn;
};
