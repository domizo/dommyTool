//
//  DOpenVDBFromDP.hpp
//  dommyTool
//
//  Created by yuzo on 11/28/16.
//
//

#pragma once

#include <string>
#include <vector>

#include <maya/MPxNode.h>

#include <openvdb/Grid.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/ParticlesToLevelSet.h>
#include <openvdb/tools/TopologyToLevelSet.h>
#include <openvdb/util/NullInterrupter.h>

#include "DPData.h"
#include "DOpenVDBData.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "DOpenVDBCommon.h"

#include "DOpenVDBAttributeTransferUtil.h"



class DOpenVDBFromDP final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;

private:
	MStatus convertWithAttributes(openvdb::FloatGrid::Ptr outputGrid, ParticleList& paList,
                               const DPData& ptGeo,
                               DOpenVDBData& vdbdata,
                               openvdb::util::NullInterrupter& boss,
                               MDataBlock& data,
                               float maximumRadiusVoxels);
	
	
	void constructGenericAttributeList(
        const DPData& dpdata,
        DOpenVDBData& vdbdata,
        const openvdb::math::Transform::Ptr& transform,
        const std::vector<TransferAttributeData>& exportAttributes,
        const openvdb::Int32Grid& closestPointIndexGrid);
	
    template <class ValueType, typename AttrType>
    void addAttributeSingle(
        const std::vector<AttrType>& sourceValues,
        DOpenVDBData& vdbdata,
        const openvdb::math::Transform::Ptr& transform,
        const openvdb::Int32Grid& closestPointIndexGrid,
        const std::string& gridName,
        AttrType defaultValue);
    
    template <class ValueType, typename AttrType>
    void addAttributeVector(
        const std::vector<AttrType>& sourceValues,
        DOpenVDBData& vdbdata,
        const openvdb::math::Transform::Ptr& transform,
        const openvdb::Int32Grid& closestPointIndexGrid,
        const std::string& gridName,
        int vectorType,
        AttrType defaultValue);
	
    
    template <class ValueType, typename AttrType>
    void addAttributeTuple(
        const std::vector<AttrType>& sourceValues,
        DOpenVDBData& vdbdata,
        const openvdb::math::Transform::Ptr& transform,
        int attributeTupleSize,
        const openvdb::Int32Grid& closestPointIndexGrid,
        const std::string& gridName,
        AttrType defaultValue);

public:
	static MObject inDP;
	
	static MObject aIsLevelSet;
	static MObject aGridName;
	static MObject aIsFogVolume;
	static MObject aFogVolumeGridName;
	static MObject aIsMaskVolume;
	static MObject aMaskVolumeName;
	
	static MObject aVoxelSize;
	static MObject aWorldSpaceUnits;
	static MObject aBandWidth;
	static MObject aBandWidthWS;
	
	static MObject aRadius;
	static MObject aRadiusMin;
	static MObject aRadiusAttribute;
	
	static MObject aPrune;
	static MObject aMaskWidth;
	
	static MObject aTrailType;
	
	static MObject aVelocityAttribute;
	static MObject aVelocityMultiplier;
	static MObject aVelocitySpacing;
    
    
    static MObject aPointAttributes;
    static MObject aAttributeName;
    static MObject aNewGridName;
    static MObject aVectorType;
	
	
	static MObject aVdbOutput;
	
	
};
