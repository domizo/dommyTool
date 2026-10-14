//
//  DOpenVDBField.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/24.
//

#include "DOpenVDBField.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaFieldData.h"
#include "common/utility/DMayaFieldDraw.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DNumericValidation.h"




#include <maya/MIOStream.h>

#include "DMayaHeaders.h"

#include "DOpenVDBDataHeaders.h"
#include "DNodeOperation.h"
//
//


#include "DBBoxOp.h"


#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include "../Common/DVDBComputeGuard.h"

namespace {

bool validBoxSamplingPoint(const openvdb::Vec3d& indexPoint)
{
    // BoxSampler floors each index and visits the +1 neighbor on every axis.
    // OpenVDB coordinates are signed 32-bit; admitting INT32_MAX as a base
    // would overflow the interpolation stencil even though the cast fits.
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        std::int32_t base = 0;
        if (!std::isfinite(indexPoint[axis]) ||
            !dommy::numeric::checkedNumericCast(std::floor(indexPoint[axis]), base) ||
            base == std::numeric_limits<std::int32_t>::max()) return false;
    }
    return true;
}

MStatus setZeroForces(unsigned int count, MVectorArray& outputForce)
{
    return dommy::maya::buildCheckedArray<MVectorArray>(
        count, [](unsigned int, MVector& force) -> MStatus {
            force = MVector::zero;
            return MS::kSuccess;
        }, outputForce);
}

template<typename GridType>
MStatus sampleVelocityGrid(const GridType& grid,
                           const MVectorArray& points,
                           double magnitude,
                           MVectorArray& outputForce)
{
    const openvdb::math::Transform& transform = grid.transform();
    return dommy::maya::buildCheckedArray<MVectorArray>(
        points.length(), [&](unsigned int index, MVector& output) -> MStatus {
            output = MVector::zero;
            const MVector& point = points[index];
            if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
                !std::isfinite(point.z)) {
                return MS::kSuccess;
            }

            const openvdb::Vec3d indexPoint = transform.worldToIndex(
                openvdb::Vec3d(point.x, point.y, point.z));
            if (!validBoxSamplingPoint(indexPoint)) return MS::kInvalidParameter;
            typename GridType::ValueType value(0.0);
            openvdb::tools::BoxSampler::sample<typename GridType::TreeType>(
                grid.tree(), indexPoint, value);

            const MVector force(value[0] * magnitude,
                                value[1] * magnitude,
                                value[2] * magnitude);
            if (std::isfinite(force.x) && std::isfinite(force.y) &&
                std::isfinite(force.z)) {
                output = force;
            }
            return MS::kSuccess;
        }, outputForce);
}

} // namespace



MTypeId DOpenVDBField::id( ID_VDB_FIELD );

MObject DOpenVDBField::aVdbInput;
MObject DOpenVDBField::aVdbSelectedGridNames;


void *DOpenVDBField::creator()
{
    return ::dommy::maya::createObject("DOpenVDBField", [&]() {
        return new DOpenVDBField;
    });
}


MStatus DOpenVDBField::initialize()
try {
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    
    //input vdb
    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));
    
    MFnStringData strData;
    const MObject aVdbSelectedGridNamesDefaultData = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString, aVdbSelectedGridNamesDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, mOutputForce));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, mOutputForce));

    return( MS::kSuccess );
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DOpenVDBField", "initialize", std::current_exception());
}




MStatus DOpenVDBField::compute(const MPlug& plug, MDataBlock& block) try
{
    if (plug != mOutputForce) return MS::kUnknownParameter;
    MStatus status;
    const unsigned int logicalIndex = plug.logicalIndex(&status);
    DOMMY_RETURN_IF_FAILURE(status);

    dommy::maya::FieldInputArrays receptors;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFieldInputs(
        block, logicalIndex, mInputData, mInputPositions, mInputVelocities,
        mInputMass, receptors));
    MVectorArray forces;
    DOMMY_RETURN_IF_FAILURE(applyNoMaxDist(
        block, receptors.positions, receptors.velocities, receptors.masses, forces));
    if (forces.length() != receptors.positions.length()) return MS::kFailure;
    return dommy::maya::publishFieldForces(
        block, plug, mOutputForce, logicalIndex, forces);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBField")





MStatus DOpenVDBField::applyNoMaxDist( MDataBlock& block,
                                   const MVectorArray &points,
                                   const MVectorArray &velocities,
                                   const MDoubleArray &masses,
                                   MVectorArray &outputForce )
{
    // Preserve the public entry point: sample the first selected vector grid,
    // or produce one zero force per receptor when the input cannot drive it.
    const unsigned int receptorSize = points.length();
    if (receptorSize > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;

    // A Maya field output must always contain one force per receptor.
    if (receptorSize != velocities.length()) {
        return setZeroForces(receptorSize, outputForce);
    }

    (void)masses;
    MStatus status;
    const MDataHandle magnitudeHandle = block.inputValue(mMagnitude, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double magValue = magnitudeHandle.asDouble();
    if (!std::isfinite(magValue)) {
        return setZeroForces(receptorSize, outputForce);
    }

    MDataHandle inputVdbHandle = block.inputValue(aVdbInput, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (inputVdbHandle.data().isNull()) {
        return setZeroForces(receptorSize, outputForce);
    }
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
        inputVdbHandle, DOpenVDBData::id, inputVdb));
    if (!inputVdb || !inputVdb->hasValidData()) return MS::kInvalidParameter;

    const MDataHandle selectionHandle = block.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selectionStr = selectionHandle.asString().asChar();
    GridCPtrVec grids;
    getGrids(grids, *inputVdb, selectionStr);
    if (grids.empty()) {
        return setZeroForces(receptorSize, outputForce);
    }
    if (!grids.front()) return MS::kInvalidParameter;

    const openvdb::GridBase& velocityGrid = *grids.front();
    const DVdbGridType gridType = getVdbGridType(velocityGrid);
    if (gridType == DVdbGridType::kVec3f) {
        return sampleVelocityGrid(
            static_cast<const openvdb::Vec3SGrid&>(velocityGrid),
            points, magValue, outputForce);
    } else if (gridType == DVdbGridType::kVec3d) {
        return sampleVelocityGrid(
            static_cast<const openvdb::Vec3DGrid&>(velocityGrid),
            points, magValue, outputForce);
    }
    return setZeroForces(receptorSize, outputForce);
}







void DOpenVDBField::draw(M3dView& view, const MDagPath&, M3dView::DisplayStyle,
                         M3dView::DisplayStatus) try
{
    dommy::maya::drawFieldIcon(view);
}
DOMMY_DRAW_CATCH("DOpenVDBField")
