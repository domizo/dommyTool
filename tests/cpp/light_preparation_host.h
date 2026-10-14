#pragma once

// Reuse the explicit value/array/matrix shapes, not an Arnold implementation.
#include "arnold_export_host.h"
#include "light_transform_methods.inc"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DNumericValidation.h"

#ifdef DOMMY_SDK_CHECK
#include <maya/MUserData.h>
#include <maya/MFrameContext.h>
#include "common/utility/DMayaPlugDataHandle.h"
#else
int liveUserData = 0, livePlugHandles = 0;
struct MUserData {
    explicit MUserData(bool) { api("cache.construct"); ++liveUserData; }
    virtual ~MUserData() { --liveUserData; }
};
namespace MHWRender { struct MFrameContext {}; }
namespace dommy { namespace maya {
class ScopedPlugDataHandle {
    MDataHandle value_;
    bool live_ = false;
public:
    ~ScopedPlugDataHandle() { if (live_) --livePlugHandles; }
    MDataHandle& handle() { return value_; }
    MStatus read(const MPlug&) {
        const MStatus status = apiStatus("plug.handle");
        if (status) { live_ = true; ++livePlugHandles; }
        return status;
    }
};
inline bool isFloatRepresentableVector(const MVector& value) {
    return isFiniteVector(value) && numeric::canRepresentAsFloat(value.x) &&
        numeric::canRepresentAsFloat(value.y) && numeric::canRepresentAsFloat(value.z);
}
}}
#endif

template <typename T> MStatus readPlugValue(const MObject&, const MObject&, T&);
const DPData* getInputDPfromHandle(MDataHandle);

#ifndef DOMMY_SDK_CHECK
DPData preparationPoints;
MDataBlock preparationControls;
const MTypeId DPData::id = 1;

template <typename T> T readPreparationValue(const Value&);
template <> inline bool readPreparationValue<bool>(const Value& value) { return value.scalar != 0; }
template <> inline int readPreparationValue<int>(const Value& value) { return int(value.scalar); }
template <> inline double readPreparationValue<double>(const Value& value) { return value.scalar; }
template <> inline MString readPreparationValue<MString>(const Value& value) { return value.string; }
template <> inline MVector readPreparationValue<MVector>(const Value& value) {
    return {value.components[0],value.components[1],value.components[2]};
}
template <typename T> MStatus readPlugValue(const MObject&, const MObject& attr, T& output) {
    const MStatus status = apiStatus("plug.read");
    if (!status) return status;
    if (attr.id == failAttribute) return MS::kFailure;
    output = readPreparationValue<T>(preparationControls.values[attr.id]);
    return MS::kSuccess;
}
const DPData* getInputDPfromHandle(MDataHandle) {
    return apiStatus("point.handle") && validInput ? &preparationPoints : nullptr;
}
template <typename T, typename ArrayType>
void readPreparationArray(const DPData& points, const std::string& name, ArrayType& output) {
    api("attribute.array");
    ArrayType candidate;
    candidate.values = points.attrListValue<T>(name);
    if (!output.copy(candidate)) throw std::runtime_error("attribute copy failed");
}
void getMDoubleArrayFromAttribute(const DPData& points, const std::string& name, MDoubleArray& output) {
    if (points.getAttrTypeName(name) != "double") return;
    readPreparationArray<double>(points,name,output);
}
void getMVectorArrayFromAttribute(const DPData& points, const std::string& name, MVectorArray& output) {
    if (points.getAttrTypeName(name) != "vec3") return;
    readPreparationArray<MVector>(points,name,output);
}
void getMMatrixArrayFromAttribute(const DPData& points, const std::string& name, MMatrixArray& output) {
    if (points.getAttrTypeName(name) != "matrix") return;
    readPreparationArray<MMatrix>(points,name,output);
}
#endif
