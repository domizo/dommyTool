#pragma once

#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <iterator>

#define CHECK_MSTATUS(x) { const MStatus status = (x); if (!status) status.pAPIerror("probe", __LINE__); }
struct PointUvTypeId {
    unsigned int value;
    unsigned int id() const { return value; }
};
struct DPData { static const PointUvTypeId id; };
struct DCurveData { static const PointUvTypeId id; };
struct DMeshData { static const PointUvTypeId id; };
const PointUvTypeId DPData::id = {1};
const PointUvTypeId DCurveData::id = {2};
const PointUvTypeId DMeshData::id = {3};
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const auto result = operation("string data " + std::string(value));
        if (status) *status = result;
        return result ? MObject{value} : MObject::kNullObj;
    }
};
