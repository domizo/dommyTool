//
//  DPScatter.hpp
//  dommyTool
//
//  Created by yuzo on 5/16/16.
//
//

#pragma once

#include <cstddef>
#include <maya/MDataHandle.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>
#include <maya/MPxNode.h>
#include <maya/MRampAttribute.h>

#include <openvdb/openvdb.h>
#include <openvdb/tools/VolumeToSpheres.h>

#include "DPointPackingCommon.h"

#include <random>
#include <vector>


struct DTriangle {
    unsigned int id_a;
    unsigned int id_b;
    unsigned int id_c;
};

class DPScatter final : public MPxNode
{
public:
    
    DPScatter();
    ~DPScatter() override;
    
	MStatus setDependentsDirty(const MPlug& plug, MPlugArray& plugArray) override;
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	// Attributes
	static MObject inObject;
	
	static MObject aTotalCount;
	
    static MObject aEmitType;
    
	static MObject aSeed;
	
    //relax
	static MObject aIsRelax;
	static MObject aRadius;
	static MObject aMaxEffectiveRadius;
    static MObject aRelaxIterations;
    static MObject aRelaxTolerance;
    
    
    //sphere packing
    static MObject aRadiusRange;
    static MObject aPackingType;
    static MObject aPackingSampling;
    static MObject aPackingGradientBiasRamp;
    static MObject aUseRadiusBias;
    static MObject aRadiusBiasRamp;
    static MObject aBiasRange;
    
    //for Map
    static MObject aMap;
    
    //pre packing Points
    static MObject aPrePackingDP;
    static MObject aRadiusAttribute;
    static MObject aBaseRadius;
    static MObject aPreDPGroupName;
    
	//output
	static MObject outDP;
    
private:
    
    
    openvdb::tools::ClosestSurfacePoint< openvdb::FloatGrid >::Ptr clpGrid;
    
    //triangle data
    MPointArray m_pts;
    std::vector<DTriangle> m_triIds;
    
    
    //sphere packing
    std::vector<PackedCircle> placedPoints;
    std::mt19937 m_eng;
    std::uniform_real_distribution<float> m_distribx;
    std::uniform_real_distribution<float> m_distriby;
    std::uniform_int_distribution<int> m_triIdDist;
    
    
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
    
	MFnMesh m_meshFn;
    
    //method
    bool getMitchellBestCandidate(PackedCircle& result);
    float getDistanceToNearestSample(const PackedCircle& candidate) const;
    bool placeNewSample();
    bool getMapValues(
        const std::vector<MPoint>& points, std::vector<float>& values);
};
