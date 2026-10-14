//
//  DPointVolumeEmit.h
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#pragma once

#include <cstddef>
#include <random>
#include <string>
#include <vector>
#include <maya/MDataHandle.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MPxNode.h>
#include <maya/MRampAttribute.h>

#include "DType.h"

enum {
    D_VOLUME_EMIT_SPHERE = 0,
    D_VOLUME_EMIT_SPHERE_FILL,
    D_VOLUME_EMIT_BOX,
    D_VOLUME_EMIT_BOX_FILL,
    D_VOLUME_EMIT_MESH,
    D_VOLUME_EMIT_SPHERE_PACKING,
    D_VOLUME_EMIT_TYPE_COUNT
};typedef int D_VOLUME_EMIT_TYPE;


enum {
    D_PACK_3D_RANDOM = 0,
    D_PACK_3D_GRADATION,
    D_PACK_3D_MAP,
    D_PACK_3D_EXPRESSION,
    D_PACKING_3D_TYPE_COUNT
};typedef short D_PACKIN_3D_TYPE;


struct PackedCircle3D
{
    PackedCircle3D(DPoint pos, float radius )
    : m_pt(pos)
    , m_radius(radius)
    { }
    
    DPoint m_pt;
    float m_radius;
};



class DPointVolumeEmit final : public MPxNode
{
public:
    
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    //method
    bool getMitchellBestCandidate(
        const MTime& evaluationTime, PackedCircle3D& result);
    float getDistanceToNearestSample(const PackedCircle3D& candidate) const;
    bool placeNewSample(const MTime& evaluationTime);
    
	
    
    // Attributes
    static MObject inputGeometry;
	
    static MObject emitType;
    static MObject aCount;
    static MObject radius;
    static MObject volumeSize;
    static MObject aBoxGrid;
    
    static MObject nSeed;
    
    //object volume
    static MObject aSpread;
    static MObject aMode;
    static MObject aDensity;
    static MObject aIsLocal;
    static MObject aPerVoxel;
    
    static MObject aInterior;
    
    static MObject aVoxelType;
    static MObject aVoxelSize;
    static MObject aExteriorBandWidth;
    static MObject aInteriorBandWidth;
    static MObject aFillInterior;
    static MObject aUnsignedDistanceField;
    
    //sphere packing
    static MObject aRadiusRange;
    static MObject aPackingType;
    static MObject aPackingAxisType;
    static MObject aPackingSampling;
    static MObject aPackingGradientBiasRamp;
    static MObject aUseRadiusBias;
    static MObject aRadiusBiasRamp;
    static MObject aBiasRange;
    
    //for Map
    static MObject inPackingUVObject;
    static MObject aMap;
    static MObject aIs3DMap;
    static MObject aExpression;
    //pre packing Points
    static MObject aPrePackingDP;
    static MObject aRadiusAttribute;
    static MObject aBaseRadius;
    static MObject aPreDPGroupName;
    
    
    static MObject outDP;
    
private:
    bool getMapValues(
        const std::vector<DPoint>& points, std::vector<float>& values);
    
//for sphere packing
    D_PACKIN_3D_TYPE m_pack_type = D_PACK_3D_RANDOM;
    D_AXIS_TYPE m_pack_axis_type = D_X_AXIS;
    std::vector<PackedCircle3D> placedPoints;
    std::mt19937 m_eng;
    std::uniform_real_distribution<float> m_distribx;
    std::uniform_real_distribution<float> m_distriby;
    std::uniform_real_distribution<float> m_distribz;
    
    MRampAttribute m_packGradientRamp;
    
    
    std::size_t m_max_count = 0;
    
    bool m_use_rad_bias = false;
    MRampAttribute m_radiusBiasRamp;
    float m_bias_range = 0.0f;
    
    //space
    float m_grid_minX = 0.0f, m_grid_maxX = 0.0f;
    float m_grid_minY = 0.0f, m_grid_maxY = 0.0f;
    float m_grid_minZ = 0.0f, m_grid_maxZ = 0.0f;
    float m_grid_sizeX = 0.0f, m_grid_sizeY = 0.0f, m_grid_sizeZ = 0.0f;
    std::size_t m_sampling = 0;
    
    //radius
    float m_min_rad = 0.0f, m_max_rad = 0.0f;
    float m_rad_size = 0.0f;

    MDataHandle m_obj_hnd;
    bool m_is3d_calc = false;
    MFnMesh meshFn;
    MFnNurbsSurface srfFn;
    std::string m_expresion_str;
    
};
