#pragma once

#include "motion_rotation_doubles.h"
#include "common/utility/DAttributeLookup.h"

using DScalarDataList = std::vector<float>;
namespace {
bool shortScalarInput = false;
}

// Attribute conversion and Maya handle/array decoding are modeled here.
// Numeric conversion uses the production checked copy, preserving float rounding.
inline void getDScalarArrayFromAttribute(const DPData& data, const std::string& name,
                                         DScalarDataList& values) {
    const auto found = data.scalars.find(name);
    if (found == data.scalars.end())
        return;
    const MStatus status = api("attribute scalar.read");
    if (!status || !dommy::numeric::checkedScalarArrayCopy(found->second, found->second.size(),
                                                           data.numberOfPoints(), values))
        throw std::runtime_error("invalid scalar attribute");
    if (shortAttribute && !values.empty())
        values.pop_back();
}
inline MStatus getDScalarArrayFromPlug(MDataHandle handle, DScalarDataList& values) {
    handle.check();
    const MStatus status = api("scalar.read");
    if (!status)
        return status;
    if (handle.input->scalarInput) {
        float value;
        if (!dommy::numeric::checkedNumericCast(handle.input->number, value))
            return MS::kInvalidParameter;
        std::fill(values.begin(), values.end(), value);
    } else if (!dommy::numeric::checkedScalarArrayCopy(
                   handle.input->scalars, handle.input->scalars.size(), values.size(), values)) {
        return MS::kInvalidParameter;
    }
    if (shortScalarInput && !values.empty())
        values.pop_back();
    return MS::kSuccess;
}
