#pragma once

#include "maya_attribute_doubles.h"
#include "point_input_fixture.h"
#include "DDataUtility/Data/DPAttributeData.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DNumericValidation.h"
#include <maya/MPoint.h>

#include <array>
#include <cmath>
#include <map>

// Test-only host boundaries. There is no simulation, DG storage, ownership or
// Maya unit conversion here; production entry-point bodies run unchanged.
bool operator!=(const MObject& a, const MObject& b) { return a.name != b.name; }
struct FluidTime {
    enum Unit { kSeconds };
    double seconds;
    explicit FluidTime(double value) : seconds(value) {}
    double as(Unit) const { return seconds; }
    bool operator<(const FluidTime& other) const { return seconds < other.seconds; }
};
#define MTime FluidTime

struct MColor {
    float r, g, b, a;
    MColor(float red = 0, float green = 0, float blue = 0, float alpha = 1)
        : r(red), g(green), b(blue), a(alpha) {}
};
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
struct DPData {
    static const int id=22;
    bool topology=true;
    bool hasValidPointTopology()const{return topology;}
    DPointList positions = DPointList(1, DPoint(0, 0, 0));
    std::map<std::string, DPAttributeData::Ptr> attributes;
    std::size_t numberOfPoints() const { return positions.size(); }
    const DPointList* getPositionPtrConst() const { return &positions; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? nullptr : found->second;
    }
};

struct EmittedVoxel {
    float scale;
    int x, y, z;
    std::array<float, 3> values;
    bool colorEnabled;
    MColor color;
};
struct FluidFixture {
    DPData input;
    bool hasInput = true;
    short mode = 1;
    std::array<double, 3> base = {{0, 0, 0}};
    // Zero = disabled, 1 = dynamic, 2 = static.
    std::array<int, 3> channels = {{1, 1, 1}};
    std::array<unsigned int, 3> resolution = {{1, 1, 1}};
    std::map<std::string, std::string> names = {
        {"density", "density"}, {"heat", "heat"}, {"fuel", "fuel"}, {"color", "col"}};
    bool emitColor = false, colorGrid = true, staticFalloff = false;
    bool jitter = false;
    double deltaTime = 1, currentTime = 2, startTime = 1;
    double rate = 100, dropoff = 0;
    int gridReads = 0, inputReads = 0, indexOffset = 0;
    int randomGets = 0, randomSets = 0, randomResets = 0, updates = 0;
    float falloffValue = 0;
    std::vector<EmittedVoxel> emitted;
};
static FluidFixture fixture;

struct MDataHandle {
    std::string attribute;
    bool valid;
    MString asString() const { assert(valid); return fixture.names.at(attribute).c_str(); }
    short asShort() const { assert(valid); return fixture.mode; }
    MObject data()const{
        assert(valid);++pointInputFixture.dataReads;
        return fixture.hasInput?MObject{attribute}:MObject::kNullObj;
    }
};
struct MPlug {
    MPlug(const MObject&,const MObject&){}
    bool isConnected(MStatus* status)const{return pointInputConnection(status);}
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        if(attribute.name=="input")++fixture.inputReads;
        return {attribute.name, attribute.name=="input"||bool(*status)};
    }
};
#define DOMMY_POINT_INPUT_DATA fixture.input
#define DOMMY_POINT_INPUT_PRESENT fixture.hasInput
#include "point_input_binding_fixture.h"

struct MFnFluid {
    enum FluidMethod { kZero, kDynamicGrid, kStaticGrid };
    enum ColorMethod { kNoColor, kDynamicColorGrid, kStaticColorGrid };
    enum FluidGradient { kGradient };
    enum FalloffMethod { kNoFalloff, kStaticFalloffGrid };
    explicit MFnFluid(const MObject&) {}
    MObject object() const { return MObject::kNullObj; }
    MStatus getDensityMode(FluidMethod& mode, FluidGradient&) {
        mode = static_cast<FluidMethod>(fixture.channels[0]); return operation("density mode");
    }
    MStatus getTemperatureMode(FluidMethod& mode, FluidGradient&) {
        mode = static_cast<FluidMethod>(fixture.channels[1]); return operation("heat mode");
    }
    MStatus getFuelMode(FluidMethod& mode, FluidGradient&) {
        mode = static_cast<FluidMethod>(fixture.channels[2]); return operation("fuel mode");
    }
    MStatus getColorMode(ColorMethod& mode) {
        mode = fixture.colorGrid ? kDynamicColorGrid : kNoColor; return operation("color mode");
    }
    MStatus getFalloffMode(FalloffMethod& mode) {
        mode = fixture.staticFalloff ? kStaticFalloffGrid : kNoFalloff;
        return operation("falloff mode");
    }
    MStatus getResolution(unsigned int& x, unsigned int& y, unsigned int& z) {
        ++fixture.gridReads;
        x = fixture.resolution[0]; y = fixture.resolution[1]; z = fixture.resolution[2];
        return operation("resolution");
    }
    MStatus getDimensions(double& x, double& y, double& z) {
        x = y = z = 1; return operation("dimensions");
    }
    unsigned int gridSize(MStatus* status) {
        *status = operation("grid size");
        return fixture.resolution[0] * fixture.resolution[1] * fixture.resolution[2];
    }
    int index(int x, int y, int z) {
        return x + static_cast<int>(fixture.resolution[0]) *
            (y + static_cast<int>(fixture.resolution[1]) * z) + fixture.indexOffset;
    }
    void emitIntoArrays(float scale, int x, int y, int z,
                        float density, float heat, float fuel, bool color, const MColor& value) {
        fixture.emitted.push_back({scale, x, y, z, {{density, heat, fuel}}, color, value});
    }
    float* falloff(MStatus* status) {
        *status = operation("falloff buffer"); return &fixture.falloffValue;
    }
    MStatus voxelCenterPosition(int, int, int, MPoint& point) {
        point = MPoint(); return operation("voxel center");
    }
    MStatus updateGrid() { ++fixture.updates; return operation("update"); }
};

// Point-mode goldens use an identity transform and a single centered voxel.
// Arbitrary Maya transforms and voxel geometry remain native validation gates.
MPoint& operator*=(MPoint& point, const MMatrix&) { return point; }
MVector operator-(const MPoint& a, const MPoint& b) {
    return MVector(a.x - b.x, a.y - b.y, a.z - b.z);
}
double operator*(const MVector& a, const MVector& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
namespace MSpace { enum Space { kWorld }; }
struct MTransformationMatrix {
    explicit MTransformationMatrix(const MMatrix&) {}
    MStatus getScale(double* scale, MSpace::Space) {
        scale[0] = scale[1] = scale[2] = 1; return operation("scale");
    }
};

struct DPFluidEmitter {
    MObject thisMObject()const{return {"fluidEmitter"};}
    struct EmissionValues;
    static MObject inDP, aDPEmitType, aDensityAttribute, aHeatAttribute,
        aFuelAttribute, aColorAttribute;
    MStatus fluidEmitter(const MObject&, const MMatrix&, int);
    MStatus emitFromGridAttributes(MFnFluid&, MDataBlock&,
        const dommy::fluid::GridPlan&, const EmissionValues&);
    MStatus emitFromPoints(MFnFluid&, const MMatrix&, MDataBlock&,
        double, double, double, bool, const dommy::fluid::GridPlan&, const EmissionValues&);
    MDataBlock forceCache() { return {}; }
    MTime getDeltaTime(int, MDataBlock&) { return MTime(fixture.deltaTime); }
    MTime getCurrentTime(MDataBlock&) { return MTime(fixture.currentTime); }
    MTime getStartTime(int, MDataBlock&) { return MTime(fixture.startTime); }
    void resetRandomState(int, MDataBlock&) { ++fixture.randomResets; }
    void getRandomState(int, MDataBlock&) { ++fixture.randomGets; }
    void setRandomState(int, MDataBlock&) { ++fixture.randomSets; }
    double fluidDensityEmission(MDataBlock&) { return fixture.base[0]; }
    double fluidHeatEmission(MDataBlock&) { return fixture.base[1]; }
    double fluidFuelEmission(MDataBlock&) { return fixture.base[2]; }
    bool fluidEmitColor(MDataBlock&) { return fixture.emitColor; }
    MColor fluidColor(MDataBlock&) { return MColor(0, 0, 1); }
    double fluidDropoff(MDataBlock&) { return fixture.dropoff; }
    double getRate(MDataBlock&) { return fixture.rate; }
    double getMinDistance(MDataBlock&) { return 0; }
    double getMaxDistance(MDataBlock&) { return 1; }
    bool fluidJitter(MDataBlock&) { return fixture.jitter; }
    double randgen() { return 0.5; }
};
MObject DPFluidEmitter::inDP{"input"}, DPFluidEmitter::aDPEmitType{"mode"},
    DPFluidEmitter::aDensityAttribute{"density"}, DPFluidEmitter::aHeatAttribute{"heat"},
    DPFluidEmitter::aFuelAttribute{"fuel"}, DPFluidEmitter::aColorAttribute{"color"};
