// Historical procedural volume-noise node.
#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBNoise.h"
#include "DOpenVDBNoiseMath.h"
#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "../Common/DVDBComputeGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MGlobal.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFloatVector.h>
#include <openvdb/math/Operators.h>
#include <openvdb/math/Stencils.h>
#include <openvdb/tools/Interpolation.h>
#include <memory>
#include "perlin.h"
#include "dvdb_ids.h"

const MTypeId DOpenVDBNoise::typeId(ID_VDB_DOPENVDB_NOISE);

MObject DOpenVDBNoise::aVdbSelectedGridNames;
MObject DOpenVDBNoise::aVdbInput;

MObject DOpenVDBNoise::maskMode;
MObject DOpenVDBNoise::maskName;
MObject DOpenVDBNoise::maskThreshold;
MObject DOpenVDBNoise::maskFallOff;
MObject DOpenVDBNoise::maskVdbInput;

MObject DOpenVDBNoise::aVdbOutput;

MObject DOpenVDBNoise::amplitude;
MObject DOpenVDBNoise::frequency;
MObject DOpenVDBNoise::octaves;
MObject DOpenVDBNoise::lacunarity;
MObject DOpenVDBNoise::gain;
MObject DOpenVDBNoise::roughness;
MObject DOpenVDBNoise::surfaceOffset;
MObject DOpenVDBNoise::noiseOffset;
MObject DOpenVDBNoise::noiseMode;
MObject DOpenVDBNoise::aTime;

namespace {
using dommy::vdb::NoiseSettings;

template <typename GridType>
bool applyNoise(Grid& grid, const noise::module::Perlin& generator, const NoiseSettings& settings,
                const Grid* mask) {
    // Never use the unchecked VDB downcast until both dynamic grid types agree.
    if (!grid.isType<GridType>() || (mask && !mask->isType<GridType>()))
        return false;
    typedef openvdb::math::Gradient<openvdb::math::GenericMap, openvdb::math::CD_2ND> Gradient;
    typedef openvdb::math::CPT<openvdb::math::GenericMap, openvdb::math::CD_2ND> CPT;
    typedef openvdb::math::SecondOrderDenseStencil<GridType> Stencil;
    typedef typename GridType::TreeType Tree;
    GridType& output = vdbGridCast<GridType>(grid);
    const GridType* maskGrid = mask ? vdbGridCast<GridType>(mask) : nullptr;
    const openvdb::math::Transform& transform = output.transform();
    Stencil stencil(output);
    openvdb::math::GenericMap map(output);
    // Reuse the mask accessor/buffer across values, as in the legacy mode-2 loop.
    const std::unique_ptr<Stencil> maskStencil(
        maskGrid && settings.mMaskMode == 2 ? new Stencil(*maskGrid) : nullptr);
    const openvdb::Vec3R offset(settings.mNOffset[0], settings.mNOffset[1], settings.mNOffset[2]);

    // Keep the historical active-value iteration (including tiles), CPT and
    // mutable-output stencil; do not silently change the generated appearance.
    for (typename GridType::ValueOnIter it = output.beginValueOn(); it; ++it) {
        if (!std::isfinite(*it))
            return false;
        const openvdb::Coord coordinate = it.getCoord();
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::vdb::noiseStencilCoordinate(coordinate[axis]))
                return false;
        if (maskGrid)
            stencil.moveTo(coordinate);
        else
            stencil.moveTo(it);
        double weight = 1.0, frequencyMultiplier = 1.0;
        if (maskGrid) {
            const openvdb::Vec3R world = transform.indexToWorld(coordinate);
            const openvdb::Vec3R voxel = maskGrid->transform().worldToIndex(world);
            int index[3] = {0, 0, 0};
            for (int axis = 0; axis < 3; ++axis)
                if (!dommy::vdb::noiseMaskCoordinate(voxel[axis], settings.mMaskMode == 2,
                                                     index[axis]))
                    return false;
            typename GridType::ValueType sample;
            // The sampler's boolean denotes active topology, not query success.
            // Inactive/background values must still participate in the mask.
            openvdb::tools::BoxSampler::sample<Tree>(maskGrid->tree(), voxel, sample);
            if (!std::isfinite(sample))
                return false;
            double alignment = 0.0;
            if (settings.mMaskMode == 2) {
                maskStencil->moveTo(openvdb::Coord(index[0], index[1], index[2]));
                const auto sourceGradient = Gradient::result(map, stencil);
                const auto maskGradient = Gradient::result(map, *maskStencil);
                // Retain the legacy unnormalized, source-map alignment test.
                // Promote components before multiplication to avoid float overflow.
                alignment = std::abs(double(sourceGradient[0]) * double(maskGradient[0]) +
                                     double(sourceGradient[1]) * double(maskGradient[1]) +
                                     double(sourceGradient[2]) * double(maskGradient[2]));
            }
            if (!dommy::vdb::noiseMaskWeight(double(sample), settings, alignment, weight))
                return false;
            if (weight == 0.0)
                continue;
            if (settings.mMaskMode == 3)
                frequencyMultiplier = double(sample);
        }
        const openvdb::Vec3R world = transform.indexToWorld(CPT::result(map, stencil) + offset);
        double noise = 0.0;
        typename GridType::ValueType value;
        if (!dommy::vdb::calculateVdbNoise(std::array<double, 3>{{world[0], world[1], world[2]}},
                                           generator, settings, frequencyMultiplier, noise) ||
            !dommy::vdb::noiseOutputValue(*it, noise, weight, settings.mOffset, value))
            return false;
        it.setValue(value);
    }
    return true;
}
} // namespace

void* DOpenVDBNoise::creator() {
    return ::dommy::maya::createObject("DOpenVDBNoise", [&]() {
        return new DOpenVDBNoise();
    });
}

MStatus DOpenVDBNoise::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    MStatus stat;
    const MDataHandle inputHandle = data.inputValue(aVdbInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DOpenVDBData::id, inputVdb));
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kInvalidParameter;

    NoiseSettings settings;
    const MDataHandle frequencyHandle = data.inputValue(frequency, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mFreq = frequencyHandle.asFloat();
    const MDataHandle lacunarityHandle = data.inputValue(lacunarity, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mLacunarity = lacunarityHandle.asFloat();
    const MDataHandle roughnessHandle = data.inputValue(roughness, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mRoughness = roughnessHandle.asFloat();
    const MDataHandle offsetHandle = data.inputValue(surfaceOffset, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mOffset = offsetHandle.asFloat();
    const MDataHandle amplitudeHandle = data.inputValue(amplitude, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mAmp = amplitudeHandle.asFloat();
    const MDataHandle gainHandle = data.inputValue(gain, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mGain = gainHandle.asFloat();
    const MDataHandle modeHandle = data.inputValue(noiseMode, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mNoiseMode = modeHandle.asInt();
    const MDataHandle octavesHandle = data.inputValue(octaves, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    settings.mOctaves = octavesHandle.asInt();
    const MDataHandle noiseOffsetHandle = data.inputValue(noiseOffset, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const MFloatVector offset = noiseOffsetHandle.asFloatVector();
    settings.mNOffset = {{offset[0], offset[1], offset[2]}};

    const Grid* maskGrid = nullptr;
    MDataHandle maskHandle = data.inputValue(maskVdbInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    // A disconnected optional input is not an invalid plug-in data object.
    if (!maskHandle.data().isNull()) {
        const DOpenVDBData* maskVdb = nullptr;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getPluginDataFromHandle(maskHandle, DOpenVDBData::id, maskVdb));
        if (!maskVdb || !maskVdb->hasValidData())
            return MS::kInvalidParameter;
        const MDataHandle maskNameHandle = data.inputValue(maskName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string selection = maskNameHandle.asString().asChar();
        GridCPtrVec masks;
        if (!getSelectedGrids(masks, selection, *maskVdb))
            MGlobal::displayWarning("No mask grids are selected.");
        if (!masks.empty()) {
            maskGrid = masks.front().get();
            const MDataHandle maskModeHandle = data.inputValue(maskMode, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            settings.mMaskMode = maskModeHandle.asInt();
            const MDataHandle thresholdHandle = data.inputValue(maskThreshold, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            settings.mThreshold = thresholdHandle.asFloat();
            const MDataHandle fallOffHandle = data.inputValue(maskFallOff, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            settings.mFallOff = fallOffHandle.asFloat();
            if (masks.size() > 1U)
                MGlobal::displayWarning("More than one reference grid was found; using the first.");
        }
    }
    if (!dommy::vdb::validNoiseSettings(settings, maskGrid != nullptr)) {
        MGlobal::displayError(
            "DVDBNoise requires finite controls, valid modes and octaves in [1, 32].");
        return MS::kInvalidParameter;
    }
    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string selection = selectionHandle.asString().asChar();

    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    GridCPtrVec grids;
    if (!getSelectedGrids(grids, selection, *inputVdb, *outputVdb))
        MGlobal::displayWarning("No grids are selected.");

    // Preflight the whole batch before copying or editing the first tree.
    std::size_t work = 0U;
    for (const GridCPtr& source : grids) {
        const bool isFloat = source->isType<openvdb::FloatGrid>();
        const bool isDouble = source->isType<openvdb::DoubleGrid>();
        if (!isFloat && !isDouble)
            continue;
        if (maskGrid && ((isFloat && !maskGrid->isType<openvdb::FloatGrid>()) ||
                         (isDouble && !maskGrid->isType<openvdb::DoubleGrid>()))) {
            MGlobal::displayError("DVDBNoise source and mask must have the same scalar grid type.");
            return MS::kInvalidParameter;
        }
        if (!dommy::vdb::addNoiseWork(source->activeVoxelCount(), settings.mOctaves, work)) {
            MGlobal::displayError("DVDBNoise exceeds the aggregate noise-sample safety budget.");
            return MS::kInvalidParameter;
        }
    }
    noise::module::Perlin generator;
    generator.SetOctaveCount(1);
    generator.SetFrequency(settings.mFreq);
    generator.SetLacunarity(settings.mLacunarity);
    generator.SetPersistence(settings.mRoughness);
    generator.SetNoiseQuality(noise::QUALITY_BEST);
    for (const GridCPtr& source : grids) {
        const bool isFloat = source->isType<openvdb::FloatGrid>();
        const bool isDouble = source->isType<openvdb::DoubleGrid>();
        if (!isFloat && !isDouble) {
            MGlobal::displayWarning(
                ("Skipped '" + source->getName() + "', unsupported type.").c_str());
            outputVdb->insert(*source);
            continue;
        }
        GridPtr candidate = source->deepCopyGrid();
        if (!candidate)
            return MS::kFailure;
        const bool success =
            isFloat ? applyNoise<openvdb::FloatGrid>(*candidate, generator, settings, maskGrid)
                    : applyNoise<openvdb::DoubleGrid>(*candidate, generator, settings, maskGrid);
        if (!success) {
            MGlobal::displayError(
                "DVDBNoise encountered an invalid coordinate or nonrepresentable value.");
            return MS::kInvalidParameter;
        }
        outputVdb->insert(candidate);
    }
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBNoise")

MStatus DOpenVDBNoise::initialize() try {
    MStatus stat;

    // attributes

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnStringData strData;

    const MObject selectionDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames =
        tAttr.create("SelectedGridNames", "grids", MFnData::kString, selectionDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    const MObject maskDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    maskName = tAttr.create("maskName", "refn", MFnData::kString, maskDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskName));

    amplitude = nAttr.create("amplitude", "amp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(amplitude));

    frequency = nAttr.create("frequency", "freq", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(frequency));

    octaves = nAttr.create("octaves", "oct", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(octaves));

    lacunarity = nAttr.create("lacunarity", "lac", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(lacunarity));

    gain = nAttr.create("gain", "ga", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(gain));

    roughness = nAttr.create("roughness", "rough", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(roughness));

    surfaceOffset = nAttr.create("surfaceOffset", "soff", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(surfaceOffset));

    noiseOffset = nAttr.create("noiseOffset", "noff", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(noiseOffset));

    MFnEnumAttribute noiseModeAttr;
    noiseMode = noiseModeAttr.create("noiseMode", "nmd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(noiseModeAttr.addField("Straight", 0));
    DOMMY_RETURN_IF_FAILURE(noiseModeAttr.addField("Absolute", 1));
    DOMMY_RETURN_IF_FAILURE(noiseModeAttr.addField("Inverse Absolute", 2));
    DOMMY_RETURN_IF_FAILURE(noiseModeAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(noiseMode));

    MFnEnumAttribute eAttr;
    maskMode = eAttr.create("maskMode", "mmd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("No noise if mask < threshold", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("No noise if mask > threshold", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("No noise if mask > threshold & normals align", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Use mask as frequency multiplier", 3));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskMode));

    maskThreshold = nAttr.create("maskThreshold", "thres", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskThreshold));

    maskFallOff = nAttr.create("maskFallOff", "fall", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskFallOff));

    MFnUnitAttribute uniAttr;
    aTime = uniAttr.create("CurrentTime", "time", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTime));

    // input / output

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    maskVdbInput =
        tAttr.create("maskVdbInput", "maskvdb", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maskVdbInput));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskMode, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskFallOff, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(maskThreshold, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(amplitude, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(frequency, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(octaves, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(lacunarity, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(gain, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(roughness, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(surfaceOffset, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseMode, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseOffset, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTime, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBNoise", "initialize",
                                                  std::current_exception());
}
