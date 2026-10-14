#pragma once

#include "DPointDataHeaders.h"

#include <maya/MFnData.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>

#include <vector>

enum {
    DP_RAY_PROJECTION = 0,
    DP_RAY_MINIMUM,
    DP_RAY_COUNT
};
typedef short DPRayType;

struct DPRayParams
{
    DPRayParams()
        : m_type(DP_RAY_PROJECTION)
        , mIsReverse(false)
        , mIsTransform(true)
        , mIsFarthest(false)
        , mRayTolerance(0.01)
        , mScale(1.0f)
        , mLift(0.0f)
        , mIsMaxDistance(false)
        , mMaxDist(0.0f)
        , mIsCreateRayGrp(false)
        , mRayVec(0.0, 1.0, 0.0)
        , m_object()
        , m_objectType(MFnData::kInvalid)
    {
    }

    DPRayType m_type;
    bool mIsReverse;
    bool mIsTransform;
    bool mIsFarthest;

    double mRayTolerance;
    float mScale;
    float mLift;

    bool mIsMaxDistance;
    float mMaxDist;
    bool mIsCreateRayGrp;

    MVector mRayVec;
    MObject m_object;
    MFnData::Type m_objectType;
};

namespace dommy {
namespace ray {

// Maya geometry function sets are intentionally used from this serial
// boundary. Their closest-point and intersection methods are not treated as
// generally safe to call from arbitrary TBB worker threads.
class Generator
{
public:
    Generator(
        const DPRayParams& params,
        std::vector<DPointList*>& points,
        std::vector<DPIdList>& hits);

    Generator(
        const DPRayParams& params,
        std::vector<DPointList*>& points,
        std::vector<DPIdList>& hits,
        std::vector<DPIdList>& selections);

    Generator(
        const DPRayParams& params,
        std::vector<DPointList*>& points,
        std::vector<DPIdList>& hits,
        std::vector<DPIdList>& selections,
        std::vector<DVec3DataList>& directions);

    MStatus operator()() const;

private:
    const DPRayParams& params_;
    std::vector<DPointList*>& points_;
    std::vector<DPIdList>& hits_;
    std::vector<DPIdList>* selections_;
    std::vector<DVec3DataList>* directions_;
};

} // namespace ray
} // namespace dommy
