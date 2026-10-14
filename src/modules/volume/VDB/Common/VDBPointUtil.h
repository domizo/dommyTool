//
//  VDBPointUtil.h
//  dommyTool
//
//  Created by yuzo on 2017/08/30.
//

#pragma once

#include "DMayaHeaders.h"
#include "DOpenVDBDataHeaders.h"

#include "DType.h"
#include "common/utility/DGenerationLimits.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

class PointAccessor
{
public:
    explicit PointAccessor(
        DPointList* pts,
        std::size_t maximumPointCount =
            dommy::generation::kMaxGeneratedPointCount)
        : mPoints(pts), mMaximumPointCount(maximumPointCount)
    {
        if (mPoints == nullptr) {
            throw std::invalid_argument("PointAccessor requires a point list");
        }
    }
    void add(const openvdb::Vec3R &pos)
    {
        if (mPoints->size() >= mMaximumPointCount) {
            throw std::length_error("VDB point generation limit exceeded");
        }
        if (!std::isfinite(pos.x()) || !std::isfinite(pos.y()) ||
            !std::isfinite(pos.z())) {
            throw std::invalid_argument("VDB generated a non-finite point");
        }
        mPoints->push_back(DPoint(pos.x(), pos.y(), pos.z()));
    }
protected:
    DPointList*    mPoints;
    std::size_t mMaximumPointCount;
};


