#pragma once

// Only Maya/custom-data boundaries are doubled. The extracted compute bodies
// use the production XY hull, polygon decomposition, group matcher and batches.
#include "curve_copy_doubles.h"
#include "common/utility/DConvexHull2D.h"
#include "common/utility/DGroupPattern.h"
#include "geometry/DPolygonTriangulator.h"

using ConstDCItem = DCItem;
using DPoint = MPoint;
using DPointList = MPointArray;
using DStringList = std::vector<std::string>;
using DPIndexList = std::vector<std::size_t>;
using DPIdList = std::vector<bool>;
using DPGroupData = TestCurveGroup;
using DPStringAttr = TestCurveStringAttribute;

namespace dommy {
namespace maya {
DCurveData partitionStorage;
template <class T> MStatus createPluginDataObject(MFnData::Type, MObject& object, T*& output) {
    const auto status = api("plugin.create");
    if (status) {
        partitionStorage = DCurveData();
        output = &partitionStorage;
        pending = output;
        object = {"partitionData"};
    }
    return status;
}
inline MStatus publishPluginDataObject(MDataHandle& handle, MObject object, const DCurveData&) {
    return publishTypedPluginDataObject<DCurveData>(handle, object, DCurveData::id);
}
} // namespace maya
} // namespace dommy
