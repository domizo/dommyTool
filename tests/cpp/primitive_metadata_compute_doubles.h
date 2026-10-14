#pragma once
#include "mesh_group_compute_doubles.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include <set>

// Curve ownership is the host boundary. Attribute storage/validation and all
// four complete production compute bodies are compiled without substitutions.
struct DCurveData : DMeshData {
    static const int id = 23;
    using ConstDCurveIter = ConstDMeshIter;
    using DCurveIter = ConstDCurveIter;
    std::size_t numberOfCurves() const { return numberOfMesh(); }
    ConstDCurveIter beginCurves() const { return beginMeshes(); }
    ConstDCurveIter endCurves() const { return endMeshes(); }
    bool hasValidCurveTopology() const { return hasValidMeshTopology(); }
    bool getCheckedTotalCvs(std::size_t& count) const { return getCheckedTotalVertex(count); }
};
static DCurveData curveInput, curveOutput;
namespace dommy { namespace maya {
template <> inline MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DCurveData*& value) {
    handle.check();
    const MStatus status = api("curve.bind");
    if (status) value = nullInput ? nullptr : &curveInput;
    return status;
}
inline MStatus createPluginData(MFnPluginData&, int, DCurveData*& value) {
    const MStatus status = api("output.create");
    if (status) value = nullOutput ? nullptr : &curveOutput;
    return status;
}
} }
