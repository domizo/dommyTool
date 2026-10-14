#pragma once

#include "DPrimitiveExplode.h"
#include "DMayaPluginDataOutput.h"
#include <maya/MPlug.h>
#include <maya/MMatrix.h>

namespace dommy {
namespace maya {

// DMesh retains its serialized float controls; DCurve retains double controls.
// Only the input adapter differs. Planning and publication are shared.
template <typename Node, typename Data, bool FloatInputs>
MStatus computeExplodeNode(const MPlug& plug, MDataBlock& block,
                           const MObject& inputAttribute, const MObject& outputAttribute)
{
    if (plug != outputAttribute) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = block.inputValue(inputAttribute, &status);
    if (!status) return status;
    const Data* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getPluginDataFromHandle(inputHandle, Data::id, input));
    if (!input || !validatePluginData(*input)) return MS::kInvalidParameter;
    const MDataHandle enableHandle = block.inputValue(Node::aEnable, &status);
    if (!status) return status;
    const bool enabled = enableHandle.asBool();
    std::vector<MMatrix> transforms;
    if (enabled) {
        geometry::ExplodeSettings settings;
        const MDataHandle scale = block.inputValue(Node::aScale, &status);
        if (!status) return status;
        settings.scale = FloatInputs ? scale.asFloat() : scale.asDouble();
        const MDataHandle center = block.inputValue(Node::aCenter, &status);
        if (!status) return status;
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            settings.center[axis] = FloatInputs ? center.asFloat3()[axis] : center.asDouble3()[axis];
        const MDataHandle vectorType = block.inputValue(Node::aVectorType, &status);
        if (!status) return status;
        settings.vectorType = vectorType.asShort();
        const MDataHandle centerName = block.inputValue(Node::aPieceCenterAttributeName, &status);
        if (!status) return status;
        settings.centerAttribute = centerName.asString().asChar();
        const MDataHandle vectorName = block.inputValue(Node::aVectorAttributeName, &status);
        if (!status) return status;
        settings.vectorAttribute = vectorName.asString().asChar();
        const MDataHandle pattern = block.inputValue(Node::aPieceGroup, &status);
        if (!status) return status;
        settings.groupPattern = pattern.asString().asChar();
        if (!geometry::buildExplodeTransforms(*input, settings, transforms))
            return MS::kInvalidParameter;
    }
    DOMMY_RETURN_IF_FAILURE(setPluginDataOutput<Data>(block, outputAttribute, Data::id,
        [&](Data& output) -> MStatus {
            DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input));
            if (enabled) {
                std::size_t index = 0U;
                for (auto it = output.beginPrims(); it != output.endPrims(); ++it, ++index) {
                    if (!*it || index >= transforms.size()) return MS::kFailure;
                    (*it)->setTransformMatrix(transforms[index]);
                }
                if (index != transforms.size()) return MS::kFailure;
                output.updateBBox();
            }
            return MS::kSuccess;
        }));
    return block.setClean(plug);
}

} // namespace maya
} // namespace dommy
