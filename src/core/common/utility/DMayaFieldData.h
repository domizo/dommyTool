#pragma once

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MVectorArray.h>

namespace dommy {
namespace maya {

struct FieldInputArrays {
    MVectorArray positions;
    MVectorArray velocities;
    MDoubleArray masses;
};

namespace field_detail {

template <typename FunctionSet, typename Attribute, typename Array>
inline MStatus readArray(MDataHandle& compound, const Attribute& attribute, Array& result)
{
    MStatus status;
    MDataHandle child = compound.child(attribute);
    FunctionSet reader(child.data(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const Array values = reader.array(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (values.length() > generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
    return result.copy(values);
}

} // namespace field_detail

// Read only the requested logical element, without evaluating other field
// receptors. The caller owns this temporary bundle and must discard it on error.
template <typename Attribute>
inline MStatus readFieldInputs(MDataBlock& data, unsigned int logicalIndex,
                              const Attribute& input, const Attribute& positions,
                              const Attribute& velocities, const Attribute& masses,
                              FieldInputArrays& result)
{
    MStatus status;
    MArrayDataHandle elements = data.outputArrayValue(input, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(elements.jumpToElement(logicalIndex));
    MDataHandle receptor = elements.inputValue(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(field_detail::readArray<MFnVectorArrayData>(
        receptor, positions, result.positions));
    DOMMY_RETURN_IF_FAILURE(field_detail::readArray<MFnVectorArrayData>(
        receptor, velocities, result.velocities));
    return field_detail::readArray<MFnDoubleArrayData>(receptor, masses, result.masses);
}

// Build a typed force payload before touching the output builder. Commit the
// requested sparse element, then acknowledge the evaluated plug. A native
// commit/clean failure is propagated; Maya writes cannot be rolled back here.
template <typename Attribute>
inline MStatus publishFieldForces(MDataBlock& data, const MPlug& requested,
                                 const Attribute& output, unsigned int logicalIndex,
                                 const MVectorArray& forces)
{
    if (forces.length() > generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
    MStatus status;
    MFnVectorArrayData payloadFactory;
    const MObject payload = payloadFactory.create(forces, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataHandle array = data.outputArrayValue(output, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MArrayDataBuilder builder = array.builder(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    MDataHandle element = builder.addElement(logicalIndex, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(element.set(payload));
    DOMMY_RETURN_IF_FAILURE(array.set(builder));
    return data.setClean(requested);
}

} // namespace maya
} // namespace dommy
