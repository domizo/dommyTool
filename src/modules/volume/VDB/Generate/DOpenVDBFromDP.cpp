//
//  DOpenVDBFromDP.cpp
//  dommyTool
//
//  Created by yuzo on 11/28/16.
//
//

#include "DOpenVDBFromDP.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "../Common/DVDBComputeGuard.h"
#include "../Common/DVdbParticleRasterPlan.h"
#include "../Edit/DOpenVDBParticleConversionPolicy.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include <locale>
#include <sstream>
#include <stdexcept>






#include "DNodeOperation.h"

#include "DMayaHeaders.h"

#include "dvdb_ids.h"

const MTypeId DOpenVDBFromDP::typeId( ID_VDB_DOPENVDB_FROM_DP );

MObject DOpenVDBFromDP::inDP;
MObject DOpenVDBFromDP::aVdbOutput;

MObject DOpenVDBFromDP::aIsLevelSet;
MObject DOpenVDBFromDP::aGridName;
MObject DOpenVDBFromDP::aIsFogVolume;
MObject DOpenVDBFromDP::aFogVolumeGridName;
MObject DOpenVDBFromDP::aIsMaskVolume;
MObject DOpenVDBFromDP::aMaskVolumeName;

MObject DOpenVDBFromDP::aVoxelSize;
MObject DOpenVDBFromDP::aWorldSpaceUnits;
MObject DOpenVDBFromDP::aBandWidth;
MObject DOpenVDBFromDP::aBandWidthWS;

MObject DOpenVDBFromDP::aRadius;
MObject DOpenVDBFromDP::aRadiusMin;
MObject DOpenVDBFromDP::aRadiusAttribute;

MObject DOpenVDBFromDP::aPrune;
MObject DOpenVDBFromDP::aMaskWidth;

MObject DOpenVDBFromDP::aTrailType;

MObject DOpenVDBFromDP::aVelocityAttribute;
MObject DOpenVDBFromDP::aVelocityMultiplier;
MObject DOpenVDBFromDP::aVelocitySpacing;

MObject DOpenVDBFromDP::aPointAttributes;
MObject DOpenVDBFromDP::aAttributeName;
MObject DOpenVDBFromDP::aNewGridName;
MObject DOpenVDBFromDP::aVectorType;

namespace {

const unsigned int kMaxExportedPointAttributeCount = 1024U;

bool isValidClosestPointIndex(openvdb::Int32 pointIndex, std::size_t pointCount)
{
    return pointIndex >= 0 &&
           static_cast<std::size_t>(pointIndex) < pointCount;
}

} // namespace


MStatus DOpenVDBFromDP::convertWithAttributes(openvdb::FloatGrid::Ptr outputGrid,
                                           ParticleList& paList,
                                           const DPData& dpdata,
                                           DOpenVDBData& vdbdata,
                                           openvdb::util::NullInterrupter& boss,
                                           MDataBlock& data,
                                           float maximumRadiusVoxels)
{
    MStatus stat;
    MDataHandle valueHandle;
	openvdb::tools::ParticlesToLevelSet<openvdb::FloatGrid, openvdb::Int32, openvdb::util::NullInterrupter>
	raster(*outputGrid, &boss);
	
	valueHandle = data.inputValue(aRadiusMin, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    raster.setRmin(valueHandle.asFloat());
	raster.setRmax(maximumRadiusVoxels);
	valueHandle = data.inputValue(aTrailType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    bool velocityTrails = valueHandle.asShort() == 1;
	
	if (velocityTrails && paList.hasVelocity()) {
		valueHandle = data.inputValue(aVelocitySpacing, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        raster.rasterizeTrails(paList, valueHandle.asFloat());
	} else if (paList.hasRadius()){
		raster.rasterizeSpheres(paList);
	} else {
		raster.rasterizeSpheres(paList, paList.radiusMult());
	}
	
	// always prune to produce a valid narrow-band level set.
	raster.finalize(/*prune=*/true);
	
	openvdb::Int32Grid::Ptr closestPtnIdxGrid = raster.attributeGrid();
	
	if (raster.ignoredParticles()) {
		std::ostringstream ostr;
		ostr << "Ignored " << raster.getMinCount() << " small and " << raster.getMaxCount()
		<< " large particles (hint: change Minimum Radius in Voxels)";
		MGlobal::displayWarning(ostr.str().c_str());
	}
	
	if (boss.wasInterrupted()) return MS::kFailure;
	
	// Transfer point attributes.
#pragma mark export point attributeの取得

    MArrayDataHandle arrayHandle = data.inputArrayValue(aPointAttributes, &stat);
    if (stat != MS::kSuccess) {
        MGlobal::displayWarning("Unable to read VDB point-attribute settings.");
        return MS::kFailure;
    }

    const unsigned int count = arrayHandle.elementCount(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (count > kMaxExportedPointAttributeCount) {
        MGlobal::displayWarning(
            "Point attributes were not exported because the requested grid "
            "count exceeds the safety limit.");
        return MS::kFailure;
    }
    std::vector<TransferAttributeData> exportAttributes;
    exportAttributes.reserve(count);

    for (unsigned int i = 0; i < count; ++i) {
        stat = arrayHandle.jumpToArrayElement(i);
        if (stat != MS::kSuccess) {
            MGlobal::displayWarning(
                "Skipped an unreadable VDB point-attribute entry.");
            return stat;
        }
        MDataHandle attrHnd = arrayHandle.inputValue(&stat);
        if (stat != MS::kSuccess) {
            MGlobal::displayWarning(
                "Skipped an invalid VDB point-attribute entry.");
            return stat;
        }

        const int vectorType = attrHnd.child(aVectorType).asShort();
        if (vectorType < 0 || vectorType >= openvdb::NUM_VEC_TYPES) return MS::kInvalidParameter;
        const std::string attributeName =
            attrHnd.child(aAttributeName).asString().asChar();
        const std::string gridName =
            attrHnd.child(aNewGridName).asString().asChar();
        exportAttributes.emplace_back(attributeName, gridName, vectorType);
    }

	if (!exportAttributes.empty()) {
        if (!closestPtnIdxGrid) {
            MGlobal::displayWarning(
                "Point attributes were not exported because the closest-point "
                "index grid is unavailable.");
            return MS::kFailure;
        }
		openvdb::math::Transform::Ptr transform = outputGrid->transformPtr();
        constructGenericAttributeList(
            dpdata, vdbdata, transform, exportAttributes,
            *closestPtnIdxGrid);
	}
    return MS::kSuccess;
}

// Helper methods for point attribute transfer

void DOpenVDBFromDP::constructGenericAttributeList(
    const DPData& dpdata,
    DOpenVDBData& vdbdata,
    const openvdb::math::Transform::Ptr& transform,
    const std::vector<TransferAttributeData>& exportAttributes,
    const openvdb::Int32Grid& closestPointIndexGrid)
{
    if (!transform) {
        MGlobal::displayWarning(
            "Point attributes were not exported because the VDB transform is unavailable.");
        return;
    }

	for (std::size_t i = 0; i < exportAttributes.size(); ++i) {
		const std::string& attributeName = exportAttributes[i].attributeName();
		if (attributeName.empty()) continue;
		
        //attribute check
        if (!dpdata.isExistAttr(attributeName)) {
            std::ostringstream ostr;
            ostr << "Skipped unrecognized attribute: '" << attributeName << "'";
            MGlobal::displayWarning(ostr.str().c_str());
            continue;
        }
		
		//attribute type
        DPAttributeData::ConstPtr attr =
            dpdata.getAttrPtr(attributeName);
        if (!attr) {
            MGlobal::displayWarning("Skipped an unavailable point attribute.");
            continue;
        }
        const std::string typeStr = attr->typeName();
		
		//vector type
		const int vectorType = exportAttributes[i].vectorType();
		
		//new name
        const std::string& requestedGridName = exportAttributes[i].gridName();
		const std::string gridName =
            requestedGridName.empty() ? attributeName : requestedGridName;
		
        
        
		
		
        if (typeStr == "int") {
			const auto m = std::dynamic_pointer_cast<const DPIntAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
			
			addAttributeSingle<openvdb::Int32, int32_t>(
                m->value(), vdbdata, transform, closestPointIndexGrid,
                gridName, m->getDefaultValue());
			
		} else if (typeStr == "uint32") {
			
			const auto m = std::dynamic_pointer_cast<const DPUInt32Attr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
			
			addAttributeSingle<openvdb::Int64, uint32_t>(
                m->value(), vdbdata, transform, closestPointIndexGrid,
                gridName, m->getDefaultValue());
			
		} else if (typeStr == "int64") {
			
			const auto m = std::dynamic_pointer_cast<const DPInt64Attr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
			
			addAttributeSingle<openvdb::Int64, int64_t>(
                m->value(), vdbdata, transform, closestPointIndexGrid,
                gridName, m->getDefaultValue());
			
		} else if (typeStr == "float") {
			
			const auto m = std::dynamic_pointer_cast<const DPFloatAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
			
			addAttributeSingle<float, float>(
                m->value(), vdbdata, transform, closestPointIndexGrid,
                gridName, m->getDefaultValue());
			
		} else if (typeStr == "double") {
			
			const auto m = std::dynamic_pointer_cast<const DPDoubleAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
			
			addAttributeSingle<double, double>(
                m->value(), vdbdata, transform, closestPointIndexGrid,
                gridName, m->getDefaultValue());
			
		} else if (typeStr == "vec2i" || typeStr == "vec2f" || typeStr == "vec2d") {
			
			if (typeStr == "vec2i") {
				const auto m = std::dynamic_pointer_cast<const DPVec2iAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
				
				addAttributeTuple<openvdb::Int32, Imath::V2i>(
                    m->value(), vdbdata, transform, 2,
                    closestPointIndexGrid, gridName, m->getDefaultValue());
				
			} else if (typeStr == "vec2f") {
				const auto m = std::dynamic_pointer_cast<const DPVec2fAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
				
				addAttributeTuple<float, Imath::V2f>(
                    m->value(), vdbdata, transform, 2,
                    closestPointIndexGrid, gridName, m->getDefaultValue());
				
			}else if (typeStr == "vec2d") {
				const auto m = std::dynamic_pointer_cast<const DPVec2dAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
				
				addAttributeTuple<double, Imath::V2d>(
                    m->value(), vdbdata, transform, 2,
                    closestPointIndexGrid, gridName, m->getDefaultValue());
			}
			
		} else if (typeStr == "vec3i" || typeStr == "vec3f" || typeStr == "vec3d") {
			
			
			
			
			if (typeStr == "vec3i") {
                const auto m = std::dynamic_pointer_cast<const DPVec3iAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
                
				addAttributeVector<openvdb::Vec3i, Imath::V3i>(
                    m->value(), vdbdata, transform, closestPointIndexGrid,
                    gridName, vectorType, m->getDefaultValue());
			} else if (typeStr == "vec3f") {
                const auto m = std::dynamic_pointer_cast<const DPVec3fAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
                
				addAttributeVector<openvdb::Vec3s, Imath::V3f>(
                    m->value(), vdbdata, transform, closestPointIndexGrid,
                    gridName, vectorType, m->getDefaultValue());
			}else if (typeStr == "vec3d") {
                const auto m = std::dynamic_pointer_cast<const DPVec3dAttr>(attr);
                if (!m) throw std::invalid_argument("Invalid point attribute type");
                
				addAttributeVector<openvdb::Vec3d, Imath::V3d>(
                    m->value(), vdbdata, transform, closestPointIndexGrid,
                    gridName, vectorType, m->getDefaultValue());
			}
			
		} else {
			MGlobal::displayWarning("Skipped unrecognized attribute type");
		}
		
		
		
	}
	
}


typedef openvdb::tree::IteratorRange<openvdb::Int32Tree::LeafCIter> IterRange;



template <class ValueType, typename AttrType>
void DOpenVDBFromDP::addAttributeSingle(
    const std::vector<AttrType>& sourceValues,
    DOpenVDBData& vdbdata,
    const openvdb::math::Transform::Ptr& transform,
    const openvdb::Int32Grid& closestPointIndexGrid,
    const std::string& gridName,
    AttrType defaultValue)
{
	    typedef typename openvdb::Int32Grid::TreeType::ValueConverter<ValueType>::Type TreeType;
	    typedef typename openvdb::Grid<TreeType> GridType;

	    // Get the attribute's default value.
	    ValueType defValue = ValueType(defaultValue);
	    
	    // Construct a new tree that matches the closestPtnIdxGrid's active voxel topology.
	    typename TreeType::Ptr tree(new TreeType(
            closestPointIndexGrid.tree(), defValue, openvdb::TopologyCopy()));
	    typename GridType::Ptr mgrid(GridType::create(tree));
        if (!mgrid) throw std::runtime_error("VDB attribute grid creation failed");
	    
	    mgrid->setName(gridName);
	    
	    typename GridType::Accessor mAccessor = mgrid->getAccessor();
	    IterRange range(closestPointIndexGrid.tree().beginLeaf());
	    
	    openvdb::Int32Tree::LeafNodeType::ValueOnCIter iter;
	    
	    for ( ; range; ++range) {
	        iter = range.iterator()->beginValueOn();
	        for ( ; iter; ++iter) {
                const openvdb::Int32 pointIndex = iter.getValue();
                if (!isValidClosestPointIndex(pointIndex, sourceValues.size())) {
                    continue;
                }
	            mAccessor.setValue(
                    iter.getCoord(),
                    ValueType(sourceValues[static_cast<std::size_t>(pointIndex)]));
	        } // end sparse voxel iteration.
	    } // end leaf-node iteration

	    mgrid->setTransform(transform);
	    vdbdata.insert(mgrid);
}




template <class ValueType, typename AttrType>
void DOpenVDBFromDP::addAttributeVector(
    const std::vector<AttrType>& sourceValues,
    DOpenVDBData& vdbdata,
    const openvdb::math::Transform::Ptr& transform,
    const openvdb::Int32Grid& closestPointIndexGrid,
    const std::string& gridName,
    int vectorType,
    AttrType defaultValue)
{
	    typedef typename openvdb::Int32Grid::TreeType::ValueConverter<ValueType>::Type TreeType;
	    typedef typename openvdb::Grid<TreeType> GridType;

	    ValueType defValue(defaultValue[0],defaultValue[1],defaultValue[2]);

	    // Construct a new tree that matches the closestPtnIdxGrid's active voxel topology.
	    typename TreeType::Ptr tree(new TreeType(
            closestPointIndexGrid.tree(), defValue, openvdb::TopologyCopy()));
	    typename GridType::Ptr mgrid(GridType::create(tree));
        if (!mgrid) throw std::runtime_error("VDB attribute grid creation failed");
		
        const int vectorTypeCount = static_cast<int>(openvdb::NUM_VEC_TYPES);
        if (vectorType < 0 || vectorType >= vectorTypeCount) {
            MGlobal::displayWarning(
                "Skipped a point vector attribute with an invalid VDB vector type.");
            return;
        }
	    mgrid->setVectorType(static_cast<openvdb::VecType>(vectorType));
	    mgrid->setName(gridName);
	    
	    typename GridType::Accessor mAccessor = mgrid->getAccessor();
	    IterRange range(closestPointIndexGrid.tree().beginLeaf());
	    
	    openvdb::Int32Tree::LeafNodeType::ValueOnCIter iter;
	    
	    for ( ; range; ++range) {
	        iter = range.iterator()->beginValueOn();
	        for ( ; iter; ++iter) {
                const openvdb::Int32 pointIndex = iter.getValue();
                if (!isValidClosestPointIndex(pointIndex, sourceValues.size())) {
                    continue;
                }
                const AttrType& source =
                    sourceValues[static_cast<std::size_t>(pointIndex)];
	            mAccessor.setValue(
                    iter.getCoord(),
                    ValueType(source[0], source[1], source[2]));
	        } // end sparse voxel iteration.
	    } // end leaf-node iteration

	    mgrid->setTransform(transform);
	    vdbdata.insert(mgrid);
}


template <class ValueType, typename AttrType>
void DOpenVDBFromDP::addAttributeTuple(
    const std::vector<AttrType>& sourceValues,
    DOpenVDBData& vdbdata,
    const openvdb::math::Transform::Ptr& transform,
    int attributeTupleSize,
    const openvdb::Int32Grid& closestPointIndexGrid,
    const std::string& gridName,
    AttrType defaultValue)
{
		typedef typename openvdb::Int32Grid::TreeType::ValueConverter<ValueType>::Type TreeType;
		typedef typename openvdb::Grid<TreeType> GridType;

		for (int component = 0; component < attributeTupleSize; ++component) {
			// Get the attribute's default value.
			ValueType defValue = ValueType(defaultValue[component]);
			
			// Construct a new tree that matches the closestPtnIdxGrid's active voxel topology.
			typename TreeType::Ptr tree(new TreeType(
                closestPointIndexGrid.tree(), defValue, openvdb::TopologyCopy()));
			typename GridType::Ptr mgrid(GridType::create(tree));
        if (!mgrid) throw std::runtime_error("VDB attribute grid creation failed");
			
			std::ostringstream name;
			name.imbue(std::locale::classic());
			name << gridName;
			if (attributeTupleSize != 1) name << "_" << component;
			if (!name) throw std::runtime_error("VDB attribute name formatting failed");
			mgrid->setName(name.str());
			
			typename GridType::Accessor mAccessor = mgrid->getAccessor();
			IterRange range(closestPointIndexGrid.tree().beginLeaf());
			
			openvdb::Int32Tree::LeafNodeType::ValueOnCIter iter;
			
			for ( ; range; ++range) {
				iter = range.iterator()->beginValueOn();
				for ( ; iter; ++iter) {
                    const openvdb::Int32 pointIndex = iter.getValue();
                    if (!isValidClosestPointIndex(pointIndex, sourceValues.size())) {
                        continue;
                    }
					mAccessor.setValue(
                        iter.getCoord(),
                        ValueType(
                            sourceValues[static_cast<std::size_t>(pointIndex)]
                                        [component]));
				} // end sparse voxel iteration.
			} // end leaf-node iteration

			mgrid->setTransform(transform);
			vdbdata.insert(mgrid);
		}
}




void* DOpenVDBFromDP::creator()
{
    return ::dommy::maya::createObject("DOpenVDBFromDP", [&]() {
        return new DOpenVDBFromDP();
    });
}



MStatus DOpenVDBFromDP::compute(const MPlug& plug, MDataBlock& data) try {
	
	MStatus stat;
    MDataHandle valueHandle;
	
	if (plug == aVdbOutput) {
		
		
		//get Attribute
		valueHandle = data.inputValue(aIsLevelSet, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const bool isLevelSetGrid = valueHandle.asBool();
		valueHandle = data.inputValue(aGridName, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MString gridNameVal = valueHandle.asString();
		valueHandle = data.inputValue(aIsFogVolume, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool isFogVolumeGrid = valueHandle.asBool();
		valueHandle = data.inputValue(aFogVolumeGridName, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MString fogVolumeGridNameVal = valueHandle.asString();
		valueHandle = data.inputValue(aIsMaskVolume, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool isMaskVolumeGrid = valueHandle.asBool();
		valueHandle = data.inputValue(aMaskVolumeName, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MString maskVolumeNameVal = valueHandle.asString();
		
		if (!isFogVolumeGrid && !isLevelSetGrid && !isMaskVolumeGrid) {
			MGlobal::displayError("No output selected");
			return MS::kFailure;
		}
		
		
		valueHandle = data.inputValue(aVoxelSize, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float mVoxelSize = valueHandle.asFloat();
        
        if (mVoxelSize < 1e-5) {
            std::ostringstream ostr;
            ostr << "The voxel size ("<< mVoxelSize << ") is too small.";
            MGlobal::displayError(ostr.str().c_str());
            return MS::kFailure;
        }
        
        
        
		valueHandle = data.inputValue(aWorldSpaceUnits, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool wsUnits = valueHandle.asBool();
		valueHandle = data.inputValue(aBandWidth, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float bandWidthVal = valueHandle.asFloat();
		valueHandle = data.inputValue(aBandWidthWS, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float bandWidthWSVal = valueHandle.asFloat();
		
		valueHandle = data.inputValue(aRadius, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float dRVal = valueHandle.asFloat();
		valueHandle = data.inputValue(aRadiusMin, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float RminVal = valueHandle.asFloat();
		valueHandle = data.inputValue(aRadiusAttribute, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		std::string radiusAttributeName = valueHandle.asString().asChar();
		
		valueHandle = data.inputValue(aPrune, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		bool pruneVal = valueHandle.asBool();
		valueHandle = data.inputValue(aMaskWidth, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float maskWidthVal = valueHandle.asFloat();
		
		valueHandle = data.inputValue(aTrailType, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const short trailType = valueHandle.asShort();
        if (trailType != 0 && trailType != 1) return MS::kInvalidParameter;
        const bool doSphereConversion = trailType == 0;
		
		valueHandle = data.inputValue(aVelocityMultiplier, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float dVVal = valueHandle.asFloat();
		valueHandle = data.inputValue(aVelocitySpacing, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		float dXVal = valueHandle.asFloat();
		valueHandle = data.inputValue(aVelocityAttribute, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		std::string velocityAttributeName = valueHandle.asString().asChar();
		
		
        
        

        
		
#pragma mark inDPの取得
		
		const DPData* inputDPData = nullptr;
        valueHandle = data.inputValue(inDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(valueHandle, DPData::id, inputDPData));
        if (!inputDPData || !inputDPData->hasValidPointTopology() || !inputDPData->numberOfPoints())
            return MS::kInvalidParameter;
		
		if (inputDPData) {
			
			//get positions
			const DPointList& pts = *inputDPData->getPositionPtrConst();
			
			const std::size_t count = pts.size();
            if (count > (dommy::vdb::kMaxVdbPayloadBytes / 4U) / 160U)
                return MS::kInvalidParameter; // Before allocating Maya particle arrays.
			
			if (count == 0U || count > dommy::generation::kMaxGeneratedPointCount) {
				MGlobal::displayError(
					"DPoint input is empty or exceeds the supported point-count limit.");
				return MStatus::kFailure;
			}
			
			
			//position
			MPointArray posDataArray;
            DOMMY_RETURN_IF_FAILURE(posDataArray.setLength(static_cast<unsigned int>(count)));
			dommy::vdb::ParticleInputExtrema extrema;
			for (std::size_t i = 0; i < count; ++i) {
				if (!extrema.addPosition(pts[i])) {
					MGlobal::displayError(
						"DPoint positions must contain only finite values.");
					return MS::kFailure;
				}
				posDataArray[i] = MPoint(pts[i][0],pts[i][1],pts[i][2]);
			}
			
			//radius
			// ParticleList applies dRVal as the radius multiplier.  Attribute-free
			// particles therefore start with a unit radius so the scale is applied
			// exactly once.
			MDoubleArray radiusArray;
            DOMMY_RETURN_IF_FAILURE(radiusArray.setLength(static_cast<unsigned int>(count)));
            for (unsigned int i = 0; i < radiusArray.length(); ++i) radiusArray[i] = 1.0;
			
			if (!radiusAttributeName.empty() && inputDPData->isExistAttr(radiusAttributeName)) {
                DOMMY_RETURN_IF_FAILURE(radiusArray.clear());
                getMDoubleArrayFromAttribute(*inputDPData, radiusAttributeName, radiusArray);
			}
			if (radiusArray.length() != count) {
				MGlobal::displayError(
					"The radius attribute must contain one value per DPoint.");
				return MS::kFailure;
			}
			for (unsigned int i = 0; i < radiusArray.length(); ++i) {
				if (!extrema.addRadius(radiusArray[i])) {
					MGlobal::displayError(
						"DPoint radii must be finite and non-negative.");
					return MS::kFailure;
				}
			}
			
			//velocity
			MVectorArray velocityArray;
            DOMMY_RETURN_IF_FAILURE(velocityArray.setLength(static_cast<unsigned int>(count)));
            for (unsigned int i = 0; i < velocityArray.length(); ++i) velocityArray[i] = MVector(0,0,0);
			bool hasVelocity = false;
			if (!velocityAttributeName.empty() && inputDPData->isExistAttr(velocityAttributeName)) {
                DOMMY_RETURN_IF_FAILURE(velocityArray.clear());
                getMVectorArrayFromAttribute(*inputDPData, velocityAttributeName, velocityArray);
				hasVelocity = true;
			}
			if (velocityArray.length() != count) {
				MGlobal::displayError(
					"The velocity attribute must contain one value per DPoint.");
				return MS::kFailure;
			}
			for (unsigned int i = 0; i < velocityArray.length(); ++i) {
				if (!extrema.addVelocity(velocityArray[i])) {
					MGlobal::displayError(
						"DPoint velocities must contain only finite values.");
					return MS::kFailure;
				}
			}

			dommy::vdb::ParticleRasterParameters rasterParameters;
			rasterParameters.voxelSize = mVoxelSize;
			rasterParameters.halfBandVoxels = wsUnits
				? static_cast<double>(bandWidthWSVal) / mVoxelSize
				: bandWidthVal;
			rasterParameters.radiusScale = dRVal;
			rasterParameters.minimumRadiusVoxels = RminVal;
			rasterParameters.maskWidth = isMaskVolumeGrid ? maskWidthVal : 0.0;
			rasterParameters.velocityMultiplier = dVVal;
			rasterParameters.velocitySpacing = dXVal;
			rasterParameters.rasterizeTrails = !doSphereConversion && hasVelocity;
			rasterParameters.rasterPassCount = 1U;
			if (isMaskVolumeGrid && maskWidthVal > 0.0f) {
				rasterParameters.rasterPassCount += maskWidthVal < 1.0f ? 2U : 1U;
			}

			dommy::vdb::TreeEditBudget rasterBudget;
            float background = 0;
            MArrayDataHandle attributeSettings = data.inputArrayValue(aPointAttributes, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const unsigned int attributeCount = attributeSettings.elementCount(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (attributeCount > kMaxExportedPointAttributeCount) return MS::kInvalidParameter;
            if (!dommy::vdb::planParticleConversion(count, extrema, rasterParameters,
                    1 + (isLevelSetGrid && isFogVolumeGrid ? 1 : 0) + (isMaskVolumeGrid ? 2 : 0), doSphereConversion ? attributeCount : 0,
                    rasterBudget, background)) {
				MGlobal::displayError(
					"DPoint-to-VDB input is invalid or exceeds the raster coordinate or resource budget.");
				return MS::kFailure;
			}
			const float rasterMaximumRadiusVoxels = static_cast<float>(
				std::max(static_cast<double>(RminVal),
					extrema.maximumRadius() * dRVal *
						(1.0 + rasterParameters.maskWidth) / mVoxelSize) + 1.0);
			
			ParticleList paList(
				posDataArray, radiusArray,
				hasVelocity ? velocityArray : MVectorArray(),
				dRVal, dVVal);
			
			Settings settings;
			settings.mRasterizeTrails = !doSphereConversion;
			settings.mDx = dXVal;
            
            
			#pragma mark get output vdb
			MFnPluginData pluginData;
			pluginData.create(DOpenVDBData::id, &stat);
			if (stat != MS::kSuccess) return stat;
			
	            DOpenVDBData* vdb = nullptr;
	            stat = dommy::maya::getPluginData(pluginData, vdb);
	            if (!stat) return stat;
				if (stat != MS::kSuccess) return stat;
			
			
			openvdb::math::Transform::Ptr transform =
			openvdb::math::Transform::createLinearTransform(mVoxelSize);
            if (!transform) return MS::kFailure;
			
			openvdb::FloatGrid::Ptr outputGrid, minGrid, maxGrid;
			
			
			
			// Set narrow band half-width
			// background was validated by the shared raster plan.
			
			// Perform the particle conversion.
			
			// Construct a new level set grid.
			if (!outputGrid) outputGrid = openvdb::FloatGrid::create(background);
			if (!outputGrid) return MS::kFailure;
			
			outputGrid->setGridClass(openvdb::GRID_LEVEL_SET);
			outputGrid->setTransform(transform);
			
	            
	            if (doSphereConversion) {
	                openvdb::util::NullInterrupter boss;
                if (attributeCount) {
	                    DOMMY_RETURN_IF_FAILURE(convertWithAttributes(outputGrid, paList, *inputDPData, *vdb,
                                              boss, data, rasterMaximumRadiusVoxels));
	                } else {
	                    convert(outputGrid, paList, settings, RminVal,
	                            rasterMaximumRadiusVoxels, pruneVal);
	                }
            } else {
                convert(outputGrid, paList, settings, RminVal,
                        rasterMaximumRadiusVoxels, pruneVal);
            }
			
			if (isMaskVolumeGrid) {
				
				// Max grid
				if (!maxGrid) maxGrid = openvdb::FloatGrid::create(background);
				if (!maxGrid) return MS::kFailure;
				maxGrid->setGridClass(openvdb::GRID_LEVEL_SET);
				maxGrid->setTransform(transform->copy());
				if (maskWidthVal > 0.0f) {
						if (!paList.scaleRadiusMultiplier(
								1.0f + maskWidthVal)) {
							MGlobal::displayError(
								"DPoint radius scaling overflowed.");
							return MS::kFailure;
						}
					convert(maxGrid, paList, settings, RminVal,
						rasterMaximumRadiusVoxels, pruneVal);
					
					// Min grid
					if ( maskWidthVal < 1.0f) {
						if (!minGrid) minGrid = openvdb::FloatGrid::create(background);
						if (!minGrid) return MS::kFailure;
						minGrid->setGridClass(openvdb::GRID_LEVEL_SET);
						minGrid->setTransform(transform->copy());
							if (!paList.scaleRadiusMultiplier(
									(1.0f - maskWidthVal) /
									(1.0f + maskWidthVal))) {
								MGlobal::displayError(
									"DPoint radius scaling overflowed.");
								return MS::kFailure;
							}
						convert(minGrid, paList, settings, RminVal,
							rasterMaximumRadiusVoxels, pruneVal);
						
						// CSG difference
						openvdb::tools::csgDifference(*maxGrid, *minGrid);
					}
				}
				
				// Convert to fog volume
				if (!dommy::vdb::convertMeshVolumeToFog(maxGrid)) return MS::kFailure;
				
				// Store the generated fog volume.
				maxGrid->setName(maskVolumeNameVal.asChar());
					
					vdb->insert(maxGrid);
				}// masked volume grid
			
			// Output level set grid
			if (isLevelSetGrid) {
				outputGrid->setName(gridNameVal.asChar());
				vdb->insert(outputGrid);
			}
			
			// Output fog volume grid
			if ( isFogVolumeGrid) {
				// If no level set grid is exported the original level set
				// grid is modified in place.
				if (isLevelSetGrid) outputGrid = outputGrid->deepCopy();
				
				if (!dommy::vdb::convertMeshVolumeToFog(outputGrid)) return MS::kFailure;
				
				outputGrid->setName(fogVolumeGridNameVal.asChar());
				vdb->insert(outputGrid);
				
			}
			
			
			MDataHandle outHandle = data.outputValue(aVdbOutput, &stat);
			if (!stat) return stat;
			stat = dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
				outHandle, pluginData.object(), DOpenVDBData::id);
			if (!stat) return stat;
			
			return data.setClean(plug);
		}
		
		
	}
	
	return MS::kUnknownParameter;
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFromDP")


MStatus DOpenVDBFromDP::initialize() try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;
	MFnStringData strData;
	MFnGenericAttribute gAttr;
	
	
#pragma mark input attribute
	// ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
	
	
	
	
	aIsLevelSet = nAttr.create("IsLevelSet", "ils", MFnNumericData::kBoolean, 0, &stat);
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsLevelSet));
	
	
	const MObject aGridNameDefault = strData.create("surface", &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	
	aGridName = tAttr.create("GridName", "gn", MFnData::kString, aGridNameDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGridName));
	
	
	aIsFogVolume = nAttr.create("IsFogVolume", "fg", MFnNumericData::kBoolean, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsFogVolume));
	
	const MObject aFogVolumeGridNameDefault = strData.create("density", &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	aFogVolumeGridName = tAttr.create("FogVolumeGridName", "fvgn", MFnData::kString, aFogVolumeGridNameDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aFogVolumeGridName));
	
	aIsMaskVolume = nAttr.create("IsMaskVolume", "imv", MFnNumericData::kBoolean, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMaskVolume));
	
	const MObject aMaskVolumeNameDefault = strData.create("mask", &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	aMaskVolumeName = tAttr.create("MaskVolumeName", "mvn", MFnData::kString, aMaskVolumeNameDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskVolumeName));
    
	aVoxelSize = nAttr.create("VoxelSize", "vs", MFnNumericData::kFloat, 0, &stat);
    
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.02));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVoxelSize));
	
	aWorldSpaceUnits = nAttr.create("WorldSpaceUnits", "wsu", MFnNumericData::kBoolean, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aWorldSpaceUnits));
	
	aBandWidth = nAttr.create("BandWidth", "bw", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aBandWidth));
	
	aBandWidthWS = nAttr.create("BandWidthWS", "bwws", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.3));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aBandWidthWS));
	
	
	aRadius = nAttr.create("Radius", "rad", MFnNumericData::kFloat, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
	
	aRadiusMin = nAttr.create("RadiusMin", "radm", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.5));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-05));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusMin));
	
	const MObject aRadiusAttributeDefault = strData.create("pscale", &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	aRadiusAttribute = tAttr.create("RadiusAttribute", "radattr", MFnData::kString, aRadiusAttributeDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));
	
	
	aPrune = nAttr.create("Prune", "pru", MFnNumericData::kBoolean, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aPrune));
	
	
	aMaskWidth = nAttr.create("MaskWidth", "mw", MFnNumericData::kFloat, 0, &stat);
	
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.25));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMaskWidth));
	
	
	MFnEnumAttribute enumFn;
	aTrailType = enumFn.create( "TrailType", "tt", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Spherical", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Velocity Trail", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTrailType));
	
	const MObject aVelocityAttributeDefault = strData.create("vel", &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	
	aVelocityAttribute = tAttr.create("VelocityAttribute", "velattr", MFnData::kString, aVelocityAttributeDefault, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityAttribute));
	
	aVelocityMultiplier = nAttr.create("VelocityMultiplier", "vmul", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityMultiplier));
	
	aVelocitySpacing = nAttr.create("VelocitySpacing", "velsp", MFnNumericData::kFloat, 0, &stat);
	
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.2));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocitySpacing));
    
    
    
    
    
    //point attribute
    const MObject aAttributeNameDefault = strData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeName = tAttr.create("AttributeName", "atrnm", MFnData::kString, aAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));
    
    const MObject aNewGridNameDefault = strData.create("", &stat);
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    aNewGridName = tAttr.create("NewGridName", "ngrnm", MFnData::kString, aNewGridNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNewGridName));
    
    MFnEnumAttribute eAttr;
    aVectorType = eAttr.create("VectorType", "vecty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < openvdb::NUM_VEC_TYPES; ++i) {
        DOMMY_RETURN_IF_FAILURE(eAttr.addField(openvdb::GridBase::vecTypeExamples(openvdb::VecType(i)).c_str(), openvdb::VecType(i)));
    }
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorType));
    
    MFnCompoundAttribute cmpAttr;
    aPointAttributes = cmpAttr.create("PointAttributes", "ptattrs", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aAttributeName));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aNewGridName));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aVectorType));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointAttributes));
    
    
	
    
    
    
	// Setup the output vdb
	aVdbOutput = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));
	
	
	// Set the attribute dependencies
	DOMMY_RETURN_IF_FAILURE(attributeAffects(message, aVdbOutput));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsLevelSet, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aGridName, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsFogVolume, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aFogVolumeGridName, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsMaskVolume, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskVolumeName, aVdbOutput));
	
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVoxelSize, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aWorldSpaceUnits, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aBandWidth, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aBandWidthWS, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusMin, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusAttribute, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrune, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaskWidth, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrailType, aVdbOutput));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVelocityAttribute, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVelocityMultiplier, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVelocitySpacing, aVdbOutput));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPointAttributes, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAttributeName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNewGridName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorType, aVdbOutput));
    
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBFromDP", "initialize", std::current_exception());
}
