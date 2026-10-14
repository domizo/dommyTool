//
//  DPGoal.cpp
//  dommyTool
//
//  Created by yuzo on 2017/04/11.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPGoal.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGoalMath.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "DPoint/Common/DPAttributeAccess.h"

#include <atomic>
#include <cmath>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

const MTypeId DPGoal::typeId( ID_DP_GOAL );


MObject DPGoal::inDP;


MObject DPGoal::aVelocityAttribute;


MObject DPGoal::aGoalType;

MObject DPGoal::aPosition;
MObject DPGoal::aTargetDP;

MObject DPGoal::aMagnitude;
MObject DPGoal::aDamping;
MObject DPGoal::aSpringConstant;

MObject DPGoal::aUseMass;
MObject DPGoal::aMassAttribute;

MObject DPGoal::outDP;



void *DPGoal::creator() {
    return ::dommy::maya::createObject("DPGoal", [&]() {
        return new DPGoal();
    });
}


MStatus DPGoal::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outDP) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* inputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, inputDPData));
    if (!inputDPData || !inputDPData->hasValidPointTopology()) return MS::kInvalidParameter;
    MDataHandle goalTypeHandle = data.inputValue(aGoalType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short goalType = goalTypeHandle.asShort();
    if (goalType < 0 || goalType > 1) return MS::kInvalidParameter;

    MDataHandle velocityNameHandle =
        data.inputValue(aVelocityAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string velocityAttributeName =
        velocityNameHandle.asString().asChar();
    if (velocityAttributeName.empty()) return MS::kInvalidParameter;

    MDataHandle dampingHandle = data.inputValue(aDamping, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float damping = dampingHandle.asFloat();
    MDataHandle springHandle = data.inputValue(aSpringConstant, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float springConstant = springHandle.asFloat();
    MDataHandle magnitudeHandle = data.inputValue(aMagnitude, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float magnitude = magnitudeHandle.asFloat();
    if (!dommy::point::validateGoalParameters(
            springConstant, damping, magnitude)) {
        return MS::kInvalidParameter;
    }

    MDataHandle useMassHandle = data.inputValue(aUseMass, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useMass = useMassHandle.asBool();
    MDataHandle massNameHandle = data.inputValue(aMassAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string massAttributeName =
        massNameHandle.asString().asChar();
    if (useMass && massAttributeName.empty()) {
        return MS::kInvalidParameter;
    }

    DPoint positionTarget(0.0f, 0.0f, 0.0f);
    const DPointList* targetPoints = nullptr;
    if (goalType == 0) {
        MDataHandle positionHandle = data.inputValue(aPosition, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const float3& position = positionHandle.asFloat3();
        if (!std::isfinite(static_cast<double>(position[0])) ||
            !std::isfinite(static_cast<double>(position[1])) ||
            !std::isfinite(static_cast<double>(position[2]))) {
            return MS::kInvalidParameter;
        }
        positionTarget = DPoint(position[0], position[1], position[2]);
    } else {
        const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const DPData* targetDPData = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, targetDPData));
        if (!targetDPData || !targetDPData->hasValidPointTopology()) return MS::kInvalidParameter;
        if (targetDPData->numberOfPoints() !=
            inputDPData->numberOfPoints()) {
            EchoWarning(
                name().asChar(),
                "Target DP must contain exactly one target per input point.");
            return MS::kInvalidParameter;
        }
        targetPoints = targetDPData->getPositionPtrConst();
        if (!targetPoints ||
            targetPoints->size() != targetDPData->numberOfPoints()) {
            return MS::kFailure;
        }
    }

    const size_t pointCount = inputDPData->numberOfPoints();
    if (!dommy::attribute::validArrayConversionBudget(pointCount,
            sizeof(Imath::V3d) + sizeof(DScalarData) + sizeof(DVec3Data)))
        return MS::kInvalidParameter;
    const bool velocityAttributeExists = inputDPData->isExistAttr(velocityAttributeName);
    DVec3DataList finalVelocities;
    if (pointCount != 0U) {
        const DPointList* positions =
            inputDPData->getPositionPtrConst();
        if (!positions || positions->size() != pointCount) {
            return MS::kFailure;
        }

        // Preserve double attributes through the force calculation. Only the
        // final float-backed output is narrowed by computeGoalVelocityComponent.
        std::vector<Imath::V3d> velocities;
        if (velocityAttributeExists) {
            const DPAttributeData::ConstPtr attribute =
                inputDPData->getAttr(velocityAttributeName);
            if (!attribute ||
                (attribute->getAttrType() != DP_ATTR_VEC3F &&
                 attribute->getAttrType() != DP_ATTR_VEC3D)) {
                EchoWarning(
                    name().asChar(),
                    "Velocity attribute must be a float or double vector.");
                return MS::kInvalidParameter;
            }

            if (!dommy::attribute::convertVector3Attribute(
                    attribute, pointCount, velocities)) {
                return MS::kInvalidParameter;
            }
        } else {
            velocities.assign(pointCount, Imath::V3d(0.0, 0.0, 0.0));
        }

        DScalarDataList pointMass(pointCount, 1.0f);
        if (useMass &&
            inputDPData->isExistAttr(massAttributeName) &&
            !getCheckedDScalarArrayFromAttribute(
                *inputDPData, massAttributeName,
                pointCount, pointMass)) {
            EchoWarning(
                name().asChar(),
                "Mass attribute must be a finite scalar per point.");
            return MS::kInvalidParameter;
        }

        finalVelocities.assign(pointCount, DVec3Data(0.0f, 0.0f, 0.0f));
        std::atomic<bool> valid(true);
        const size_t grainSize = DGetGlobalProcessNum(pointCount);
        tbb::parallel_for(
            tbb::blocked_range<size_t>(
                0U, pointCount, grainSize),
            [&](const tbb::blocked_range<size_t>& range) {
                for (size_t index = range.begin();
                     index != range.end(); ++index) {
                    if (!valid.load(std::memory_order_relaxed)) continue;
                    const DPoint& target =
                        goalType == 0
                            ? positionTarget
                            : (*targetPoints)[index];
                    DVec3Data candidate(0.0f, 0.0f, 0.0f);
                    for (int axis = 0; axis < 3; ++axis) {
                        if (!dommy::point::computeGoalVelocityComponent(
                                (*positions)[index][axis],
                                target[axis],
                                velocities[index][axis],
                                springConstant,
                                damping,
                                magnitude,
                                pointMass[index],
                                candidate[axis])) {
                            valid.store(
                                false, std::memory_order_relaxed);
                            break;
                        }
                    }
                    if (valid.load(std::memory_order_relaxed)) {
                        finalVelocities[index] = candidate;
                    }
                }
            });
        if (!valid.load(std::memory_order_relaxed)) {
            return MS::kInvalidParameter;
        }

    }

    status = dommy::maya::setPluginDataOutput<DPData>(data, outDP, DPData::id,
        [&](DPData& output) {
            output.setDPData(*inputDPData);
            if (pointCount != 0U) {
                if (velocityAttributeExists)
                    output.setDPAttributeValues(velocityAttributeName, finalVelocities);
                else
                    output.addAttr(velocityAttributeName,
                        DPVec3fAttr(finalVelocities, DVec3Data(0.0f, 0.0f, 0.0f)));
            }
            return MS::kSuccess;
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPGoal")

    




MStatus DPGoal::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    MFnGenericAttribute outAttr;
    
    
    // ----------------------- INPUTS --------------------------
    
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    const MObject aVelocityAttributeDefault = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityAttribute = tAttr.create("VelocityAttribute", "vattr", MFnData::kString, aVelocityAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityAttribute));
    
    
    MFnEnumAttribute enumFn;
    aGoalType = enumFn.create( "GoalType", "gt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Position", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DP", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGoalType));
    
    
    aPosition = nAttr.create( "Position", "pos", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0, 0.0, 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPosition));
    
    aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));
   
    
    
    
    
    aMagnitude = nAttr.create( "Magnitude", "mag", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMagnitude));
    
    aSpringConstant = nAttr.create( "SpringConstant", "spcnst", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpringConstant));
    
    aDamping = nAttr.create( "Damping", "dmp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.5 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));
    
    
    aUseMass = nAttr.create( "UseMass", "uma", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseMass));
    
    const MObject aMassAttributeDefault = fnStringData.create("mass", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMassAttribute = tAttr.create("MassAttribute", "mattr", MFnData::kString, aMassAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassAttribute));
    
    
    
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoalType, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPosition, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMagnitude, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDamping, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpringConstant, outDP ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseMass, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMassAttribute, outDP ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DPGoal", "initialize", std::current_exception());
}
