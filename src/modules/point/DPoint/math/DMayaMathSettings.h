#pragma once

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <string>
#include <utility>

namespace dommy {
namespace maya {

struct MathNodeSettings {
    std::string sourceAttribute, outputAttribute;
    bool deleteOriginal = false;
    bool createOutputAttribute = false;
};

// Read into a local candidate. No caller state changes if a handle or string
// conversion fails; the node's compute guard owns exception reporting.
template <typename Node>
MStatus readMathNodeSettings(MDataBlock& data, MathNodeSettings& settings)
{
    MathNodeSettings candidate;
    MStatus status;
    const MDataHandle source = data.inputValue(Node::aSourceAttribute, &status);
    if (!status) return status;
    candidate.sourceAttribute = source.asString().asChar();
    const MDataHandle output = data.inputValue(Node::aOutAttribute, &status);
    if (!status) return status;
    candidate.outputAttribute = output.asString().asChar();
    const MDataHandle remove = data.inputValue(Node::aIsDeleteOriginal, &status);
    if (!status) return status;
    candidate.deleteOriginal = remove.asBool();
    const MDataHandle create = data.inputValue(Node::aIsOutAttribute, &status);
    if (!status) return status;
    candidate.createOutputAttribute = create.asBool();
    settings = std::move(candidate);
    return MS::kSuccess;
}

// Legacy operation aliases are int, while their Maya enum attributes are
// short. Keep validation in the existing math-processing implementation.
inline MStatus readMathNodeOperation(MDataBlock& data, const MObject& attribute,
                                     int& operation)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) return status;
    operation = handle.asShort();
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy
