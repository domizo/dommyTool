#pragma once

#include "DMayaUnaryMath.h"
#include "DVectorNodeMath.h"
#include "DDataUtility/Data/DPData.h"
#include "Data/DCurveData.h"
#include "DMesh/Data/DMeshData.h"
#include "common/utility/DMayaPluginDataOutput.h"

namespace dommy {
namespace maya {

struct VectorNodeSettings {
    std::string sourceName, outputName, targetName;
    int targetType = 0;
    MObject targetPlug;
    DMathVec3DataList target;
};

inline MStatus readVectorNodeName(MDataBlock& data, const MObject& attribute, std::string& name)
{
    MStatus status;
    const MDataHandle input = data.inputValue(attribute, &status);
    if (!status) return status;
    name = input.asString().asChar();
    return attribute::validAttributeName(name) ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Node>
MStatus readVectorNodeTarget(MDataBlock&, bool, VectorNodeSettings&, std::false_type)
{
    return MS::kSuccess;
}

template <typename Node>
MStatus readVectorNodeTarget(MDataBlock& data, bool arrayOutput,
                              VectorNodeSettings& settings, std::true_type)
{
    MStatus status;
    const MDataHandle type = data.inputValue(Node::aType, &status);
    if (!status) return status;
    settings.targetType = type.asShort();
    switch (settings.targetType) {
    case 0: {
        const MDataHandle input = data.inputValue(Node::aConstantValue, &status);
        if (!status) return status;
        const double3& v = input.asDouble3();
        const DMathVec3Data value(v[0], v[1], v[2]);
        if (!math::finiteVectorNodeValue(value)) return MS::kInvalidParameter;
        settings.target.assign(1U, value);
        return MS::kSuccess;
    }
    case 1:
        // From Attribute has no owner in the direct Maya-array path.
        if (arrayOutput) return MS::kInvalidParameter;
        return readVectorNodeName(data, Node::aTargetAttribute, settings.targetName);
    case 2:
        settings.targetPlug = Node::inTargetDataArray;
        return MS::kSuccess;
    default: return MS::kInvalidParameter;
    }
}

inline MStatus readVectorNodeArray(MDataHandle input, std::size_t count,
                                    std::size_t operands, DMathVec3DataList& values)
{
    switch (input.type()) {
    case MFnData::kVectorArray:
        return readUnaryMathVectors<MFnVectorArrayData>(input, values, count, operands);
    case MFnData::kPointArray:
        return readUnaryMathVectors<MFnPointArrayData>(input, values, count, operands);
    default: return MS::kInvalidParameter;
    }
}

inline MStatus prepareVectorNodeTarget(MDataBlock& data, std::size_t count,
                                        VectorNodeSettings& settings)
{
    if (!math::validMathBudget(count, 1U)) return MS::kInvalidParameter;
    if (settings.targetType != 2) return MS::kSuccess;
    MStatus status;
    const MDataHandle input = data.inputValue(settings.targetPlug, &status);
    if (!status) return status;
    return readVectorNodeArray(input, count, 1U, settings.target);
}

inline MStatus publishVectorNodeArray(MDataBlock& data, const MPlug& plug,
                                       const MObject& outputAttribute, const DMathScalarDataList& values)
{
    MDoubleArray output;
    DOMMY_RETURN_IF_FAILURE((math::writeScalarMathArray<double>(values.size(),
        [&](std::size_t i) { return values[i]; }, output)));
    return DOutputDataMayaDoubleArray(data, plug, outputAttribute, output);
}

inline MStatus publishVectorNodeArray(MDataBlock& data, const MPlug& plug,
                                       const MObject& outputAttribute, const DMathVec3DataList& values)
{
    MVectorArray output;
    DOMMY_RETURN_IF_FAILURE((math::writeVectorMathArray<MVector>(values.size(),
        [&](std::size_t i) -> const DMathVec3Data& { return values[i]; }, output)));
    return DOutputDataMayaVectorArray(data, plug, outputAttribute, output);
}

template <typename Owner, typename Operation>
MStatus processVectorNodeOwner(Owner& output, std::size_t count,
                                VectorNodeSettings& settings, Operation operation)
{
    return math::processVectorNodeAttribute(output, count, settings.sourceName, settings.outputName,
        settings.targetType, settings.targetName, settings.target, 0U, operation)
        ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Iterator, typename Operation>
MStatus processVectorNodeParts(Iterator begin, Iterator end,
                                VectorNodeSettings& settings, Operation operation)
{
    return math::processVectorNodePartitions(begin, end, settings.sourceName, settings.outputName,
        settings.targetType, settings.targetName, settings.target, operation)
        ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Operation>
MStatus processVectorNodeData(DPData& output,
                               VectorNodeSettings& settings, Operation operation)
{
    return processVectorNodeOwner(output, output.numberOfPoints(), settings, operation);
}

template <typename Operation>
MStatus processVectorNodeData(DCurveData& output,
                               VectorNodeSettings& settings, Operation operation)
{
    // Preserve primitive-attribute precedence over point attributes.
    if (output.getAttr(settings.sourceName))
        return processVectorNodeOwner(output, output.numberOfCurves(), settings, operation);
    return processVectorNodeParts(output.beginCurves(), output.endCurves(), settings, operation);
}

template <typename Operation>
MStatus processVectorNodeData(DMeshData& output,
                               VectorNodeSettings& settings, Operation operation)
{
    if (output.getAttr(settings.sourceName))
        return processVectorNodeOwner(output, output.numberOfMesh(), settings, operation);
    return processVectorNodeParts(output.beginMeshes(), output.endMeshes(), settings, operation);
}

inline bool vectorNodeDataCount(const DPData& input, const VectorNodeSettings&, std::size_t& count)
{
    count = input.numberOfPoints();
    return math::validMathBudget(count, 1U);
}

inline bool vectorNodeDataCount(const DCurveData& input, const VectorNodeSettings& settings,
                                 std::size_t& count)
{
    if (!input.getAttr(settings.sourceName))
        return math::vectorNodePartitionCount(input.beginCurves(), input.endCurves(), count);
    count = input.numberOfCurves();
    return math::validMathBudget(count, 1U);
}

inline bool vectorNodeDataCount(const DMeshData& input, const VectorNodeSettings& settings,
                                 std::size_t& count)
{
    if (!input.getAttr(settings.sourceName))
        return math::vectorNodePartitionCount(input.beginMeshes(), input.endMeshes(), count);
    count = input.numberOfMesh();
    return math::validMathBudget(count, 1U);
}

inline MStatus copyVectorNodeData(DPData& output, const DPData& input)
{
    return invokeStatusOperation([&](DPData& candidate) { candidate.setDPData(input); }, output);
}

template <typename Data>
MStatus copyVectorNodeData(Data& output, const Data& input) { return output.copyFrom(input); }

template <typename Data, typename Operation>
MStatus publishVectorNodeData(MDataHandle input, MDataBlock& data, const MPlug& plug,
                               const MObject& outputAttribute, VectorNodeSettings& settings,
                               Operation operation)
{
    const Data* source = nullptr;
    DOMMY_RETURN_IF_FAILURE(getPluginDataFromHandle(input, Data::id, source));
    if (!source || !validatePluginData(*source)) return MS::kInvalidParameter;
    // Reject over-budget work and malformed external targets before copying
    // the separately validated procedural payload. Read each target once.
    std::size_t count = 0U;
    if (!vectorNodeDataCount(*source, settings, count)) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(prepareVectorNodeTarget(data, count, settings));
    DOMMY_RETURN_IF_FAILURE((setPluginDataOutput<Data>(data, outputAttribute, Data::id,
        [&](Data& output) -> MStatus {
            DOMMY_RETURN_IF_FAILURE(copyVectorNodeData(output, *source));
            return processVectorNodeData(output, settings, operation);
        })));
    return data.setClean(plug);
}

// Every registered vector node delegates both outputs here. Geometry and
// attribute updates are staged and validated before modifying a Maya output.
template <typename Node, math::VectorNodeKind Kind>
MStatus computeVectorNode(const MPlug& plug, MDataBlock& data)
{
    if (plug != Node::outDD && plug != Node::outDataArray) return MS::kUnknownParameter;
    using Operation = math::VectorNodeOperation<Kind>;
    VectorNodeSettings settings;
    DOMMY_RETURN_IF_FAILURE((readVectorNodeTarget<Node>(data, plug == Node::outDataArray,
        settings, std::integral_constant<bool, Operation::binary>())));
    MStatus status;
    if (plug == Node::outDataArray) {
        const MDataHandle input = data.inputValue(Node::inSourceDataArray, &status);
        if (!status) return status;
        math::VectorNodeInput source;
        DOMMY_RETURN_IF_FAILURE(readVectorNodeArray(input, kInferInputCount,
            Operation::binary ? 1U : 0U, source.values));
        DOMMY_RETURN_IF_FAILURE(prepareVectorNodeTarget(data, source.values.size(), settings));
        std::vector<typename Operation::Result> result;
        if (!math::evaluateVectorNode(source, settings.target, settings.targetType == 0,
                0U, Operation(), result)) return MS::kInvalidParameter;
        return publishVectorNodeArray(data, plug, Node::outDataArray, result);
    }
    DOMMY_RETURN_IF_FAILURE(readVectorNodeName(data, Node::aSourceAttribute, settings.sourceName));
    DOMMY_RETURN_IF_FAILURE(readVectorNodeName(data, Node::aOutAttribute, settings.outputName));
    const MDataHandle input = data.inputValue(Node::inDD, &status);
    if (!status) return status;
    if (input.typeId() == DPData::id)
        return publishVectorNodeData<DPData>(input, data, plug, Node::outDD, settings, Operation());
    if (input.typeId() == DCurveData::id)
        return publishVectorNodeData<DCurveData>(input, data, plug, Node::outDD, settings, Operation());
    if (input.typeId() == DMeshData::id)
        return publishVectorNodeData<DMeshData>(input, data, plug, Node::outDD, settings, Operation());
    return MS::kInvalidParameter;
}

} // namespace maya
} // namespace dommy
