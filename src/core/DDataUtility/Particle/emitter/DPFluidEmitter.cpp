//
//  DPFluidEmitter.cpp
//  dommyTool
//
//  Created by yuzo on 12/1/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPFluidEmitter.h"

#include "dpoint_ids.h"
#include "DPData.h"
#include "DPointDataAccess.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"

#include <maya/MColor.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnFluid.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>
#include <maya/MTransformationMatrix.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct DPFluidEmitter::EmissionValues
{
    float density;
    float heat;
    float fuel;
    bool densityEnabled;
    bool heatEnabled;
    bool fuelEnabled;
    bool emitColor;
    MColor color;
};

namespace {

const double kEmissionRateConversion = 0.01;
const int kMaximumJitterSamples = 8;

bool isFiniteColor(const MColor& color)
{
    return dommy::numeric::canRepresentAsFloat(color.r) &&
           dommy::numeric::canRepresentAsFloat(color.g) &&
           dommy::numeric::canRepresentAsFloat(color.b) &&
           dommy::numeric::canRepresentAsFloat(color.a);
}

template <typename ValueType>
bool copyScalarAttribute(const DPAttributeData::ConstPtr& attribute,
                         std::size_t expectedCount,
                         std::vector<float>& output)
{
    if (!attribute ||
        attribute->typeName() !=
            TypedDPAttributeData<ValueType>::staticTypeName() ||
        attribute->attrSize() != expectedCount) {
        return false;
    }

    const typename TypedDPAttributeData<ValueType>::ConstPtr typedAttribute =
        std::static_pointer_cast<const TypedDPAttributeData<ValueType> >(
            attribute);
    const std::vector<ValueType>& source = typedAttribute->value();
    if (source.size() != expectedCount) return false;

    std::vector<float> converted(expectedCount);
    for (std::size_t index = 0U; index < expectedCount; ++index) {
        if (!dommy::numeric::checkedFloatCast(
                static_cast<double>(source[index]), converted[index])) {
            return false;
        }
    }
    output.swap(converted);
    return true;
}

template <typename ValueType>
bool copyColorAttribute(const DPAttributeData::ConstPtr& attribute,
                        std::size_t expectedCount,
                        std::vector<MColor>& output)
{
    if (!attribute ||
        attribute->typeName() !=
            TypedDPAttributeData<ValueType>::staticTypeName() ||
        attribute->attrSize() != expectedCount) {
        return false;
    }

    const typename TypedDPAttributeData<ValueType>::ConstPtr typedAttribute =
        std::static_pointer_cast<const TypedDPAttributeData<ValueType> >(
            attribute);
    const std::vector<ValueType>& source = typedAttribute->value();
    if (source.size() != expectedCount) return false;

    std::vector<MColor> converted(expectedCount);
    for (std::size_t index = 0U; index < expectedCount; ++index) {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        if (!dommy::numeric::checkedFloatCast(
                static_cast<double>(source[index][0]), red) ||
            !dommy::numeric::checkedFloatCast(
                static_cast<double>(source[index][1]), green) ||
            !dommy::numeric::checkedFloatCast(
                static_cast<double>(source[index][2]), blue)) {
            return false;
        }
        converted[index] = MColor(red, green, blue);
    }
    output.swap(converted);
    return true;
}

bool loadScalarAttribute(const DPData& data,
                         const std::string& name,
                         std::size_t expectedCount,
                         std::vector<float>& values,
                         bool& usesAttribute)
{
    values.clear();
    usesAttribute = false;
    if (name.empty()) return true;

    const DPAttributeData::ConstPtr attribute = data.getAttr(name);
    if (!attribute) return true;
    usesAttribute = true;

    switch (attribute->getAttrType()) {
        case DP_ATTR_INT:
            return copyScalarAttribute<int32_t>(
                attribute, expectedCount, values);
        case DP_ATTR_UINT32:
            return copyScalarAttribute<uint32_t>(
                attribute, expectedCount, values);
        case DP_ATTR_INT64:
            return copyScalarAttribute<int64_t>(
                attribute, expectedCount, values);
        case DP_ATTR_FLOAT:
            return copyScalarAttribute<float>(
                attribute, expectedCount, values);
        case DP_ATTR_DOUBLE:
            return copyScalarAttribute<double>(
                attribute, expectedCount, values);
        default:
            return false;
    }
}

bool loadColorAttribute(const DPData& data,
                        const std::string& name,
                        std::size_t expectedCount,
                        std::vector<MColor>& values,
                        bool& usesAttribute)
{
    values.clear();
    usesAttribute = false;
    if (name.empty()) return true;

    const DPAttributeData::ConstPtr attribute = data.getAttr(name);
    if (!attribute) return true;
    usesAttribute = true;

    switch (attribute->getAttrType()) {
        case DP_ATTR_VEC3I:
            return copyColorAttribute<Imath::V3i>(
                attribute, expectedCount, values);
        case DP_ATTR_VEC3F:
            return copyColorAttribute<Imath::V3f>(
                attribute, expectedCount, values);
        case DP_ATTR_VEC3D:
            return copyColorAttribute<Imath::V3d>(
                attribute, expectedCount, values);
        default:
            return false;
    }
}

bool readStringInput(MDataBlock& data,
                     const MObject& attribute,
                     std::string& value,
                     MStatus& status)
{
    MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) return false;
    value = handle.asString().asChar();
    return true;
}

MStatus buildScalarGridPlan(MFnFluid& fluid, dommy::fluid::GridPlan& plan)
{
    unsigned int xResolution = 0U;
    unsigned int yResolution = 0U;
    unsigned int zResolution = 0U;
    MStatus status = fluid.getResolution(
        xResolution, yResolution, zResolution);
    if (!status) return status;

    double xDimension = 0.0;
    double yDimension = 0.0;
    double zDimension = 0.0;
    status = fluid.getDimensions(xDimension, yDimension, zDimension);
    if (!status) return status;

    if (!dommy::fluid::buildGridPlan(
            xResolution, yResolution, zResolution,
            xDimension, yDimension, zDimension,
            -0.5 * xDimension, -0.5 * yDimension, -0.5 * zDimension,
            1U, false, dommy::generation::kMaxGeneratedPointCount, plan)) {
        MGlobal::displayError(
            "DPFluidEmitter fluid grid exceeds the supported safety limit.");
        return MS::kFailure;
    }

    const unsigned int gridSize = fluid.gridSize(&status);
    if (!status) return status;
    if (static_cast<std::size_t>(gridSize) != plan.scalar.voxelCount) {
        MGlobal::displayError(
            "DPFluidEmitter fluid grid size does not match its resolution.");
        return MS::kFailure;
    }
    return MS::kSuccess;
}

} // namespace

MTypeId DPFluidEmitter::typeId(ID_DP_FLUID_EMITTER);

MObject DPFluidEmitter::inDP;
MObject DPFluidEmitter::aDPEmitType;
MObject DPFluidEmitter::aDensityAttribute;
MObject DPFluidEmitter::aHeatAttribute;
MObject DPFluidEmitter::aFuelAttribute;
MObject DPFluidEmitter::aColorAttribute;

void* DPFluidEmitter::creator()
{
    return ::dommy::maya::createObject("DPFluidEmitter", [&]() {
        return new DPFluidEmitter();
    });
}

MStatus DPFluidEmitter::compute(const MPlug&, MDataBlock&) try
{
    // Inherited emitter outputs are intentionally delegated to Maya's base
    // implementation by returning kUnknownParameter.
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPFluidEmitter")

MStatus DPFluidEmitter::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;
    inDP = typedAttribute.create(
        "inDP", "idp", DPData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(inDP);
    if (!status) return status;

    MFnEnumAttribute enumAttribute;
    aDPEmitType = enumAttribute.create("DPEmitType", "det", 1, &status);
    if (!status) return status;
    status = enumAttribute.addField("Point", 0);
    if (!status) return status;
    status = enumAttribute.addField("Vector Field", 1);
    if (!status) return status;
    status = addAttribute(aDPEmitType);
    if (!status) return status;

    MFnStringData stringData;
    MObject defaultValue = stringData.create("density", &status);
    if (!status) return status;
    aDensityAttribute = typedAttribute.create(
        "DensityAttribute", "denattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;
    status = addAttribute(aDensityAttribute);
    if (!status) return status;

    defaultValue = stringData.create("heat", &status);
    if (!status) return status;
    aHeatAttribute = typedAttribute.create(
        "HeatAttribute", "heattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;
    status = addAttribute(aHeatAttribute);
    if (!status) return status;

    defaultValue = stringData.create("fuel", &status);
    if (!status) return status;
    aFuelAttribute = typedAttribute.create(
        "FuelAttribute", "fueattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;
    status = addAttribute(aFuelAttribute);
    if (!status) return status;

    defaultValue = stringData.create("col", &status);
    if (!status) return status;
    aColorAttribute = typedAttribute.create(
        "ColorAttribute", "colattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;
    status = addAttribute(aColorAttribute);
    if (!status) return status;

    const MObject inputs[] = {
        inDP, aDPEmitType, aDensityAttribute, aHeatAttribute,
        aFuelAttribute, aColorAttribute};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mEmissionFunction);
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus DPFluidEmitter::fluidEmitter(const MObject& fluidObject,
                                     const MMatrix& worldMatrix,
                                     int plugIndex) try
{
    // Maya passes an internal fluid data object here, not the DAG fluid shape.
    // Autodesk's simpleFluidEmitter sample identifies that object by the null
    // MFnBase::object() result. A non-null object means there is no internal
    // target to modify for this callback.
    MFnFluid fluid(fluidObject);
    if (fluid.object() != MObject::kNullObj) return MS::kSuccess;

    if (plugIndex < 0 || !dommy::maya::isFiniteMatrix(worldMatrix)) {
        MGlobal::displayError(
            "DPFluidEmitter received an invalid target index or transform.");
        return MS::kFailure;
    }

    MDataBlock data = forceCache();
    const double deltaTime =
        getDeltaTime(plugIndex, data).as(MTime::kSeconds);
    if (!std::isfinite(deltaTime)) {
        MGlobal::displayError("DPFluidEmitter delta time must be finite.");
        return MS::kFailure;
    }
    if (deltaTime <= 0.0) return MS::kSuccess;

    const MTime currentTime = getCurrentTime(data);
    const MTime startTime = getStartTime(plugIndex, data);
    if (!std::isfinite(currentTime.as(MTime::kSeconds)) ||
        !std::isfinite(startTime.as(MTime::kSeconds))) {
        MGlobal::displayError("DPFluidEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (currentTime < startTime) {
        resetRandomState(plugIndex, data);
        return MS::kSuccess;
    }

    const double density = fluidDensityEmission(data);
    const double heat = fluidHeatEmission(data);
    const double fuel = fluidFuelEmission(data);
    EmissionValues emission;
    if (!dommy::numeric::checkedFloatCast(density, emission.density) ||
        !dommy::numeric::checkedFloatCast(heat, emission.heat) ||
        !dommy::numeric::checkedFloatCast(fuel, emission.fuel)) {
        MGlobal::displayError(
            "DPFluidEmitter emission values must be finite float values.");
        return MS::kFailure;
    }
    const bool requestedColor = fluidEmitColor(data);
    emission.emitColor = requestedColor;
    emission.color = fluidColor(data);
    if (emission.emitColor && !isFiniteColor(emission.color)) {
        MGlobal::displayError("DPFluidEmitter color must be finite.");
        return MS::kFailure;
    }

    MFnFluid::FluidMethod densityMode;
    MFnFluid::FluidMethod temperatureMode;
    MFnFluid::FluidMethod fuelMode;
    MFnFluid::ColorMethod colorMode;
    MFnFluid::FluidGradient gradient;
    MFnFluid::FalloffMethod falloffMode;
    MStatus status = fluid.getDensityMode(densityMode, gradient);
    if (!status) return status;
    status = fluid.getTemperatureMode(temperatureMode, gradient);
    if (!status) return status;
    status = fluid.getFuelMode(fuelMode, gradient);
    if (!status) return status;
    status = fluid.getColorMode(colorMode);
    if (!status) return status;
    status = fluid.getFalloffMode(falloffMode);
    if (!status) return status;

    emission.densityEnabled =
        densityMode == MFnFluid::kDynamicGrid ||
        densityMode == MFnFluid::kStaticGrid;
    emission.heatEnabled =
        temperatureMode == MFnFluid::kDynamicGrid ||
        temperatureMode == MFnFluid::kStaticGrid;
    emission.fuelEnabled =
        fuelMode == MFnFluid::kDynamicGrid ||
        fuelMode == MFnFluid::kStaticGrid;
    const bool densityToEmit = density != 0.0 && emission.densityEnabled;
    const bool heatToEmit = heat != 0.0 && emission.heatEnabled;
    const bool fuelToEmit = fuel != 0.0 && emission.fuelEnabled;
    const bool colorToEmit =
        requestedColor &&
        (colorMode == MFnFluid::kDynamicColorGrid ||
         colorMode == MFnFluid::kStaticColorGrid);
    emission.emitColor = colorToEmit;
    if (!emission.densityEnabled) emission.density = 0.0f;
    if (!emission.heatEnabled) emission.heat = 0.0f;
    if (!emission.fuelEnabled) emission.fuel = 0.0f;
    const bool writeFalloff =
        falloffMode == MFnFluid::kStaticFalloffGrid;

    MDataHandle emitTypeHandle = data.inputValue(aDPEmitType, &status);
    if (!status) return status;
    const short emitType = emitTypeHandle.asShort();

    // Only Point mode uses the node amounts directly. Grid attributes replace
    // those fallback values and must be validated before deciding not to emit.
    if (emitType == 0 && !densityToEmit && !heatToEmit && !fuelToEmit &&
        !colorToEmit && !writeFalloff) {
        return MS::kSuccess;
    }

    dommy::fluid::GridPlan gridPlan;
    status = buildScalarGridPlan(fluid, gridPlan);
    if (!status) return status;

    switch (emitType) {
        case 0: {
            double dropoff = fluidDropoff(data);
            if (!std::isfinite(dropoff) || dropoff < 0.0) {
                MGlobal::displayError(
                    "DPFluidEmitter dropoff must be finite and non-negative.");
                return MS::kFailure;
            }

            getRandomState(plugIndex, data);
            status = emitFromPoints(
                fluid, worldMatrix, data, deltaTime,
                kEmissionRateConversion, dropoff, writeFalloff,
                gridPlan, emission);
            setRandomState(plugIndex, data);
            return status;
        }
        case 1:
            return emitFromGridAttributes(
                fluid, data, gridPlan, emission);
        default:
            MGlobal::displayError(
                "DPFluidEmitter has an unsupported emission type.");
            return MS::kFailure;
    }
}
DOMMY_COMPUTE_CATCH("DPFluidEmitter")

MStatus DPFluidEmitter::emitFromPoints(
    MFnFluid& fluid,
    const MMatrix& fluidWorldMatrix,
    MDataBlock& data,
    double deltaTime,
    double conversion,
    double dropoff,
    bool writeFalloff,
    const dommy::fluid::GridPlan& gridPlan,
    const EmissionValues& emission)
{
    const DPData* inputData = nullptr;
    MStatus status = getOptionalInputDP(thisMObject(), inDP, data, inputData);
    if (!status) return status;
    if (inputData == nullptr) {
        MGlobal::displayWarning("DPFluidEmitter requires valid DPoint input.");
        return MS::kSuccess;
    }

    const DPointList* const positions = inputData->getPositionPtrConst();
    if (positions == nullptr || positions->empty()) return MS::kSuccess;
    if (positions->size() > dommy::generation::kMaxGeneratedPointCount) {
        MGlobal::displayError(
            "DPFluidEmitter DPoint input exceeds the supported point limit.");
        return MS::kFailure;
    }
    for (std::size_t index = 0U; index < positions->size(); ++index) {
        const DPoint& point = (*positions)[index];
        if (!std::isfinite(point[0]) || !std::isfinite(point[1]) ||
            !std::isfinite(point[2])) {
            MGlobal::displayError(
                "DPFluidEmitter DPoint positions must be finite.");
            return MS::kFailure;
        }
    }

    const double rate = getRate(data);
    double minimumDistance = getMinDistance(data);
    double maximumDistance = getMaxDistance(data);
    const double timeScaledRate = rate * deltaTime * conversion;
    if (!std::isfinite(rate) || !std::isfinite(minimumDistance) ||
        !std::isfinite(maximumDistance) ||
        !std::isfinite(timeScaledRate)) {
        MGlobal::displayError(
            "DPFluidEmitter point-emission parameters must be finite.");
        return MS::kFailure;
    }

    MTransformationMatrix fluidTransform(fluidWorldMatrix);
    double scale[3] = {0.0, 0.0, 0.0};
    status = fluidTransform.getScale(scale, MSpace::kWorld);
    if (!status) return status;

    const double scaleLength = std::sqrt(
        scale[0] * scale[0] + scale[1] * scale[1] +
        scale[2] * scale[2]);
    if (!std::isfinite(scaleLength) || scaleLength <= 0.0) {
        MGlobal::displayError(
            "DPFluidEmitter fluid scale must be finite and non-zero.");
        return MS::kFailure;
    }
    if (scaleLength > 0.1) dropoff /= scaleLength;
    if (!std::isfinite(dropoff) || dropoff < 0.0) return MS::kFailure;

    const double worldCellSize[3] = {
        std::fabs(scale[0] * gridPlan.cellSize[0]),
        std::fabs(scale[1] * gridPlan.cellSize[1]),
        std::fabs(scale[2] * gridPlan.cellSize[2])};
    const double minimumCellSize = std::min(
        worldCellSize[0], std::min(worldCellSize[1], worldCellSize[2]));
    const double maximumCellSize = std::max(
        worldCellSize[0], std::max(worldCellSize[1], worldCellSize[2]));
    if (!std::isfinite(minimumCellSize) ||
        !std::isfinite(maximumCellSize) || minimumCellSize <= 0.0) {
        MGlobal::displayError(
            "DPFluidEmitter transformed voxel sizes must be finite and positive.");
        return MS::kFailure;
    }

    const double voxelDiagonal = minimumCellSize * std::sqrt(3.0);
    if (!std::isfinite(voxelDiagonal) || voxelDiagonal <= 0.0) {
        return MS::kFailure;
    }
    if (maximumDistance <= minimumDistance ||
        maximumDistance <= 0.5 * voxelDiagonal) {
        if (minimumDistance < 0.0) minimumDistance = 0.0;
        maximumDistance = minimumDistance + 0.5 * voxelDiagonal;
        dropoff = 0.0;
    }
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0) {
        return MS::kFailure;
    }

    const bool jitter = fluidJitter(data);
    int sampleCount = 1;
    if (jitter) {
        const double sampleEstimate = maximumCellSize / minimumCellSize + 0.5;
        if (!std::isfinite(sampleEstimate)) return MS::kFailure;
        sampleCount = sampleEstimate >= kMaximumJitterSamples
            ? kMaximumJitterSamples
            : std::max(1, static_cast<int>(sampleEstimate));
    }

    if (!dommy::numeric::canRepresentAsFloat(
            timeScaledRate / static_cast<double>(sampleCount))) {
        MGlobal::displayError(
            "DPFluidEmitter time-scaled rate exceeds the float range.");
        return MS::kFailure;
    }

    std::size_t workItems = 0U;
    if (!dommy::generation::checkedProduct(
            positions->size(), gridPlan.scalar.voxelCount,
            dommy::generation::kMaxWorkItemCount, workItems) ||
        !dommy::generation::checkedProduct(
            workItems, static_cast<std::size_t>(sampleCount),
            dommy::generation::kMaxWorkItemCount, workItems)) {
        MGlobal::displayError(
            "DPFluidEmitter point-to-voxel scan exceeds the work limit.");
        return MS::kFailure;
    }

    float* falloffValues = nullptr;
    if (writeFalloff) {
        falloffValues = fluid.falloff(&status);
        if (!status || falloffValues == nullptr) {
            MGlobal::displayError(
                "DPFluidEmitter could not access the static falloff grid.");
            return status ? MS::kFailure : status;
        }
    }

    const int resolution[3] = {
        static_cast<int>(gridPlan.scalar.dimensions[0]),
        static_cast<int>(gridPlan.scalar.dimensions[1]),
        static_cast<int>(gridPlan.scalar.dimensions[2])};
    bool falloffChanged = false;

    for (std::size_t pointIndex = 0U;
         pointIndex < positions->size(); ++pointIndex) {
        const DPoint& sourcePosition = (*positions)[pointIndex];
        const MPoint emitterWorldPosition(
            sourcePosition[0], sourcePosition[1], sourcePosition[2]);

        for (int x = 0; x < resolution[0]; ++x) {
            for (int y = 0; y < resolution[1]; ++y) {
                for (int z = 0; z < resolution[2]; ++z) {
                    MPoint voxelCenter;
                    status = fluid.voxelCenterPosition(x, y, z, voxelCenter);
                    if (!status || !dommy::maya::isFinitePoint(voxelCenter)) {
                        return status ? MS::kFailure : status;
                    }

                    bool voxelAffected = false;
                    for (int sample = 0; sample < sampleCount; ++sample) {
                        MPoint samplePoint(voxelCenter);
                        if (jitter) {
                            samplePoint.x +=
                                (randgen() - 0.5) * gridPlan.cellSize[0];
                            samplePoint.y +=
                                (randgen() - 0.5) * gridPlan.cellSize[1];
                            samplePoint.z +=
                                (randgen() - 0.5) * gridPlan.cellSize[2];
                        }
                        samplePoint *= fluidWorldMatrix;
                        if (!dommy::maya::isFinitePoint(samplePoint)) {
                            MGlobal::displayError(
                                "DPFluidEmitter produced a non-finite sample point.");
                            return MS::kFailure;
                        }

                        const MVector difference =
                            samplePoint - emitterWorldPosition;
                        const double distanceSquared = difference * difference;
                        if (!std::isfinite(distanceSquared) ||
                            distanceSquared < 0.0) {
                            return MS::kFailure;
                        }
                        const double distance = std::sqrt(distanceSquared);
                        if (distance < minimumDistance ||
                            distance > maximumDistance) {
                            continue;
                        }
                        voxelAffected = true;

                        const double emittedValue =
                            timeScaledRate * std::exp(-dropoff * distanceSquared) /
                            static_cast<double>(sampleCount);
                        float emittedScale = 0.0f;
                        if (!dommy::numeric::checkedFloatCast(
                                emittedValue, emittedScale)) {
                            MGlobal::displayError(
                                "DPFluidEmitter produced an invalid emission value.");
                            return MS::kFailure;
                        }
                        if (emittedScale != 0.0f) {
                            fluid.emitIntoArrays(
                                emittedScale, x, y, z,
                                emission.density, emission.heat, emission.fuel,
                                emission.emitColor, emission.color);
                        }
                    }

                    if (falloffValues != nullptr && voxelAffected) {
                        const int rawIndex = fluid.index(x, y, z);
                        std::size_t checkedIndex = 0U;
                        if (!dommy::generation::checkedCount(
                                rawIndex, gridPlan.scalar.voxelCount - 1U,
                                checkedIndex)) {
                            MGlobal::displayError(
                                "DPFluidEmitter received an invalid falloff index.");
                            return MS::kFailure;
                        }

                        const double centerLength = std::sqrt(
                            voxelCenter.x * voxelCenter.x +
                            voxelCenter.y * voxelCenter.y +
                            voxelCenter.z * voxelCenter.z);
                        float falloffValue = 0.0f;
                        if (!dommy::numeric::clampUnitToFloat(
                                1.0 - 0.2 * centerLength / std::sqrt(3.0),
                                falloffValue)) {
                            return MS::kFailure;
                        }
                        falloffValues[checkedIndex] = falloffValue;
                        falloffChanged = true;
                    }
                }
            }
        }
    }

    if (falloffChanged) return fluid.updateGrid();
    return MS::kSuccess;
}

MStatus DPFluidEmitter::emitFromGridAttributes(
    MFnFluid& fluid,
    MDataBlock& data,
    const dommy::fluid::GridPlan& gridPlan,
    const EmissionValues& emission)
{
    const DPData* inputData = nullptr;
    MStatus status = getOptionalInputDP(thisMObject(), inDP, data, inputData);
    if (!status) return status;
    if (inputData == nullptr) {
        MGlobal::displayWarning("DPFluidEmitter requires valid DPoint input.");
        return MS::kSuccess;
    }

    const std::size_t pointCount = inputData->numberOfPoints();
    if (pointCount != gridPlan.scalar.voxelCount) {
        MGlobal::displayError(
            "DPFluidEmitter grid-attribute mode requires one DPoint per voxel.");
        return MS::kFailure;
    }

    std::string densityAttributeName;
    std::string heatAttributeName;
    std::string fuelAttributeName;
    std::string colorAttributeName;
    if (!readStringInput(
            data, aDensityAttribute, densityAttributeName, status) ||
        !readStringInput(data, aHeatAttribute, heatAttributeName, status) ||
        !readStringInput(data, aFuelAttribute, fuelAttributeName, status) ||
        !readStringInput(data, aColorAttribute, colorAttributeName, status)) {
        return status;
    }

    std::vector<float> densityValues;
    std::vector<float> heatValues;
    std::vector<float> fuelValues;
    std::vector<MColor> colorValues;
    bool usesDensityAttribute = false;
    bool usesHeatAttribute = false;
    bool usesFuelAttribute = false;
    bool usesColorAttribute = false;
    if ((emission.densityEnabled &&
         !loadScalarAttribute(
             *inputData, densityAttributeName, pointCount,
             densityValues, usesDensityAttribute)) ||
        (emission.heatEnabled &&
         !loadScalarAttribute(
             *inputData, heatAttributeName, pointCount,
             heatValues, usesHeatAttribute)) ||
        (emission.fuelEnabled &&
         !loadScalarAttribute(
             *inputData, fuelAttributeName, pointCount,
             fuelValues, usesFuelAttribute)) ||
        (emission.emitColor &&
         !loadColorAttribute(
             *inputData, colorAttributeName, pointCount,
             colorValues, usesColorAttribute))) {
        MGlobal::displayError(
            "DPFluidEmitter attributes must have compatible types, finite values, "
            "and exactly one value per voxel.");
        return MS::kFailure;
    }

    const int resolution[3] = {
        static_cast<int>(gridPlan.scalar.dimensions[0]),
        static_cast<int>(gridPlan.scalar.dimensions[1]),
        static_cast<int>(gridPlan.scalar.dimensions[2])};
    for (int x = 0; x < resolution[0]; ++x) {
        for (int y = 0; y < resolution[1]; ++y) {
            for (int z = 0; z < resolution[2]; ++z) {
                const int rawIndex = fluid.index(x, y, z);
                std::size_t voxelIndex = 0U;
                if (!dommy::generation::checkedCount(
                        rawIndex, pointCount - 1U, voxelIndex)) {
                    MGlobal::displayError(
                        "DPFluidEmitter received an invalid voxel index.");
                    return MS::kFailure;
                }

                const float density = usesDensityAttribute
                    ? densityValues[voxelIndex] : emission.density;
                const float heat = usesHeatAttribute
                    ? heatValues[voxelIndex] : emission.heat;
                const float fuel = usesFuelAttribute
                    ? fuelValues[voxelIndex] : emission.fuel;
                const MColor& color = usesColorAttribute
                    ? colorValues[voxelIndex] : emission.color;
                if (density > 0.0f || heat > 0.0f || fuel > 0.0f) {
                    fluid.emitIntoArrays(
                        1.0f, x, y, z, density, heat, fuel,
                        emission.emitColor, color);
                }
            }
        }
    }
    return MS::kSuccess;
}
