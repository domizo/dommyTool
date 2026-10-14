#pragma once
#include "modules/point/DPoint/DType.h"
#include "transform_curve_compute_sdk.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributePublication.h"

// The existing API boundary supplies native Maya/attribute types. These are
// the additional declaration-only point storage operations used by Blend.
class BlendApiPoint : public DPData {
public:
    void setDPData(const BlendApiPoint&);
    void removeAttr(const std::string&);
};
#define DPData BlendApiPoint
