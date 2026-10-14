#pragma once
// Attribute registration and data-handle boundaries only; not a Maya emulator.
#include "maya_attribute_doubles.h"
#include <maya/MString.h>
#include "common/utility/DAttributeTransferPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>
#define CHECK_MSTATUS(x) { const MStatus s=(x); if (!s) s.pAPIerror("probe",__LINE__); }
struct MFnStringData {
 MObject create(const char* value, MStatus* status=nullptr) {
  const MStatus s=operation("string.create"); if(status)*status=s;
  return s ? MObject{value} : MObject::kNullObj;
 }
};
struct DPData {static constexpr MFnData::Type id=MFnData::kAny;};
struct DMeshData {static constexpr MFnData::Type id=MFnData::kAny;};
struct DCurveData {static constexpr MFnData::Type id=MFnData::kAny;};
struct MVector {
 MVector(double=0,double=0,double=0) {}
 explicit MVector(const double*) {}
 static const MVector zero,yAxis;
};
const MVector MVector::zero;
const MVector MVector::yAxis(0,1,0);
using int2=int[2]; using double2=double[2]; using double3=double[3];
static int invalidReads=0; static int2 integers={0,0}; static double2 doubles={0,0}; static double3 vectorValues={0,0,0};
static bool objectBoundsEnabled=false, groupObjectConnected=true;
static short groupBoundType=0;
static std::string groupBoundActionAttribute="aIsBoundingAction", groupBoundTypeAttribute="aBoundingType";
struct MDataHandle {
 bool valid=false;
 std::string attribute;
 void read() const {if(!valid)++invalidReads;}
 short asShort() const {read();return attribute==groupBoundTypeAttribute?groupBoundType:0;}
 bool asBool() const {read();return attribute==groupBoundActionAttribute && objectBoundsEnabled;}
 double asDouble() const {read();return 0;}
 MString asString() const {read();return "group";}
 int2& asInt2() const {read();return integers;}
 double2& asDouble2() const {read();return doubles;}
 double3& asDouble3() const {read();return vectorValues;}
};
struct MDataBlock {
 MDataHandle inputValue(MObject attr,MStatus* status) { *status=operation("input "+attr.name); MDataHandle h;h.valid=bool(*status);h.attribute=attr.name;return h;}
};
struct MPlug {
 std::string attribute;
 MPlug(MObject,MObject attr):attribute(attr.name){}
 bool isConnected(MStatus* status=nullptr) const {
  const auto result=operation("connected "+attribute);if(status)*status=result;
  return result && groupObjectConnected;
 }
};
namespace dommy {namespace math {inline double degreesToRadians(double value){return value * 0.017453292519943295;}}}
