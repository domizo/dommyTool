#pragma once
#include "primitive_metadata_compute_doubles.h"
#define buildAttributeApplicationBudget originalAttributeApplicationBudget
#include "common/utility/DAttributeTransferPlan.h"
#undef buildAttributeApplicationBudget
using IDsListArray = std::vector<IDs>;
using DScalarListArray = std::vector<DScalarList>;
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
static bool badRamp = false, badNeighbors = false, noNeighbors = false;
struct TransferRamp {
    bool valid;
    TransferRamp(const MPlug&, MStatus* status) {
        *status = api("ramp.bind"); valid = bool(*status);
    }
    void getValueAtPosition(float, float& value, MStatus* status) {
        assert(valid); *status = api("ramp.sample");
        if (*status) value = badRamp ? NAN : 1.0f;
    }
};
#define MRampAttribute TransferRamp
inline void CreateSearchDataPoint(const DPointList& points, PointCloud<double>& cloud) {
    voidApi("search.cloud");
    for (const auto& p : points) cloud.pts.push_back({p[0], p[1], p[2]});
}
struct PointSearchKDTreeRadiusWithDistanceOp {
    IDsListArray& ids;
    DScalarListArray& distances;
    PointSearchKDTreeRadiusWithDistanceOp(DPkdtree&, const DPointList&, int, double,
                                        IDsListArray& ids, DScalarListArray& distances)
        : ids(ids), distances(distances) {}
    void operator()(const tbb::blocked_range<std::size_t>& range) {
        voidApi("search.radius");
        for (std::size_t i = range.first; i < range.last; ++i) {
            if (noNeighbors) continue;
            ids[i] = badNeighbors ? IDs{targetInput.numberOfPoints()} : IDs{0U, 1U};
            distances[i].assign(ids[i].size(), 0.5);
        }
    }
};
namespace dommy { namespace maya {
inline MStatus createPluginData(MFnPluginData&, int, DPData*& value) {
    const MStatus status = api("output.create");
    if (status) value = nullOutput ? nullptr : &pointOutput;
    return status;
}
inline MStatus publishPluginDataObject(MDataHandle& handle, const MObject& object, const DPData& data) {
    if (!data.hasValidPointTopology()) return MS::kFailure;
    return publishTypedPluginDataObject<DPData>(handle, object, DPData::id);
}
} }
