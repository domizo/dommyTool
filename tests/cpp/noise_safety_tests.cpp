#include "common/utility/DPointDeformMath.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DAttributePublication.h"
#include <ImathVec.h>
#include <module.h>
#include "noise/DWoodNoise.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace MS { enum Code { kSuccess, kFailure, kInvalidParameter }; }
struct MStatus {
    MS::Code value;
    MStatus(MS::Code v = MS::kSuccess) : value(v) {}
    explicit operator bool() const { return value == MS::kSuccess; }
    bool operator==(MS::Code v) const { return value == v; }
    bool operator!=(MS::Code v) const { return value != v; }
};
#define CHECK_MSTATUS_AND_RETURN_IT(status) do { if (!(status)) return status; } while (false)
#define DOMMY_RETURN_IF_FAILURE(expr) do { const MStatus checked = (expr); if (!checked) return checked; } while (false)
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using DScalarData = float;
using DScalarDataList = std::vector<float>;
using DVec3Data = Imath::V3f;
using DVec3DataList = std::vector<DVec3Data>;
using MVector = Imath::V3d;
// Division is supplied by the shared Imath vector double.
MVector operator^(const MVector& first, const MVector& second) { return first.cross(second); }
struct MPoint {
    double x, y, z, w;
    MPoint(double x=0, double y=0, double z=0) : x(x), y(y), z(z), w(1) {}
    double operator[](unsigned int axis) const { return axis==0 ? x : axis==1 ? y : axis==2 ? z : w; }
    MPoint operator+(const MPoint& p) const { return MPoint(x+p.x,y+p.y,z+p.z); }
    MPoint operator+(const MVector& v) const { return MPoint(x+v[0],y+v[1],z+v[2]); }
    MPoint operator*(double factor) const { return MPoint(x*factor,y*factor,z*factor); }
};
using MFloatVector = MVector;
struct MFloatPointArray { unsigned int count=0; unsigned int length() const { return count; } };
struct MFloatArray {};
using float3 = std::array<float, 3>;
namespace MFnData { enum Type { kInvalid, kMesh, kNurbsCurve, kNurbsSurface }; }
namespace MSpace { enum Space { kObject, kWorld }; }
const double kMFnNurbsEpsilon = 1e-3;
struct Mesh;
struct MObject {
    const Mesh* mesh = nullptr;
    bool token = false;
    bool isNull() const { return !mesh && !token; }
    static const MObject kNullObj;
};
const MObject MObject::kNullObj;
using FN_DECIMAL = float;
int fastCalls = 0, fastSetupCalls = 0;
float fastValue = 1.25f;
// A boundary double, NOT an implementation of the external noise algorithm.
struct FastNoise {
    enum NoiseType { Value, ValueFractal, Perlin, PerlinFractal, Simplex,
        SimplexFractal, Cellular, WhiteNoise, Cubic, CubicFractal };
    enum Interp { Linear, Hermite, Quintic };
    enum FractalType { FBM, Billow, RigidMulti };
    enum CellularDistanceFunction { Euclidean, Manhattan, Natural };
    enum CellularReturnType { CellValue, NoiseLookup, Distance, Distance2,
        Distance2Add, Distance2Sub, Distance2Mul, Distance2Div };
    void SetSeed(int) { ++fastSetupCalls; }
    void SetNoiseType(NoiseType) {}
    void SetInterp(Interp) {}
    void SetFrequency(float) {}
    void SetFractalOctaves(unsigned int) {}
    void SetFractalLacunarity(float) {}
    void SetFractalGain(float) {}
    void SetFractalType(FractalType) {}
    void SetCellularDistanceFunction(CellularDistanceFunction) {}
    void SetCellularReturnType(CellularReturnType) {}
    void SetGradientPerturbAmp(float) {}
    float GetNoise(float, float, float) const { ++fastCalls; return fastValue; }
};
struct SizedPoints { std::size_t count; std::size_t size() const { return count; } };

namespace tbb {
template<typename T> struct blocked_range {
    T first, last;
    blocked_range(T a, T b, T = 1) : first(a), last(b) {}
    T begin() const { return first; }
    T end() const { return last; }
};
template<typename T, typename Body> void parallel_for(const blocked_range<T>& range, Body body) {
    const T middle = range.begin() + (range.end() - range.begin()) / 2;
    body(blocked_range<T>(range.begin(), middle));
    body(blocked_range<T>(middle, range.end()));
}
}
struct Failures {
    std::map<std::string, int> calls, at;
    bool fail(const std::string& name) { return ++calls[name] == at[name]; }
    void reset() { calls.clear(); at.clear(); }
} failures;

struct Attribute {
    bool vector = false;
    bool doublePrecision = false;
    std::vector<Imath::V3d> vectors;
    std::vector<float> scalars;
    std::size_t attrSize() const { return vector ? vectors.size() : scalars.size(); }
    const char* typeName() const { return vector ? (doublePrecision ? "vectorDouble" : "vector") : "float"; }
};
struct DPAttributeData { using Ptr = std::shared_ptr<Attribute>; };
struct DPFloatAttr : Attribute {
    explicit DPFloatAttr(const std::vector<float>& values) { scalars = values; }
};
struct DPVec3fAttr : Attribute {
    explicit DPVec3fAttr(const DVec3DataList& values) {
        vector = true;
        for (const auto& value : values) vectors.push_back(MVector(value));
    }
};
struct DPVec3dAttr : Attribute {
    explicit DPVec3dAttr(const std::vector<MVector>& values) {
        vector = true; doublePrecision = true; vectors = values;
    }
};
enum { DP_ATTR_PRE_FLOAT, DP_ATTR_PRE_DOUBLE };
struct Mesh {
    DPointList points{DPoint(0,0,0), DPoint(1,0,0)};
    std::map<std::string, DPAttributeData::Ptr> attrs;
    MVector normal{0,1,0};
    const DPointList& getPositionConst() const { return points; }
    const DPointList* getPositionPtrConst() const { return &points; }
    std::size_t numberOfPoints() const { return points.size(); }
    bool replacePositions(const DPointList& values) {
        if (failures.fail("positions")) return false;
        for (const auto& point : values)
            for (unsigned int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(point[axis])) return false;
        points = values; return true;
    }
    std::map<std::string, DPAttributeData::Ptr>::iterator beginAttr() { return attrs.begin(); }
    std::map<std::string, DPAttributeData::Ptr>::iterator endAttr() { return attrs.end(); }
    DPAttributeData::Ptr getAttr(const std::string& name) const {
        const auto found = attrs.find(name);
        return found == attrs.end() ? DPAttributeData::Ptr() : found->second;
    }
    void addAttr(const std::string& name, const Attribute& value) {
        if (!failures.fail("attribute")) attrs[name] = std::make_shared<Attribute>(value);
    }
};
struct DMeshData {
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Mesh>>::iterator beginMeshes() { return meshes.begin(); }
    std::vector<std::shared_ptr<Mesh>>::iterator endMeshes() { return meshes.end(); }
    std::vector<std::shared_ptr<Mesh>>::iterator beginPrims() { return meshes.begin(); }
    std::vector<std::shared_ptr<Mesh>>::iterator endPrims() { return meshes.end(); }
};
namespace dommy { namespace maya {
bool isFiniteVector(const MVector& vector) {
    return std::isfinite(vector[0]) && std::isfinite(vector[1]) && std::isfinite(vector[2]);
}
bool isFinitePoint(const MPoint& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) && std::isfinite(point.w);
}
bool normalizedVectorOrZero(const MVector& value, MVector& output) {
    if (!isFiniteVector(value)) return false;
    const double length = std::hypot(std::hypot(value[0],value[1]),value[2]);
    if (!std::isfinite(length)) return false;
    output = length == 0 ? MVector(0,0,0) : value/length;
    return true;
}
bool normalizedVector(const MVector& value, MVector& output) {
    return normalizedVectorOrZero(value,output) && output.dot(output)>0;
}
} namespace attribute {
bool validArrayConversionBudget(std::size_t count, std::size_t size) {
    return count <= generation::kMaxGeneratedPointCount && count <= 256U * 1024U * 1024U / size;
}
bool convertVector3Attribute(const DPAttributeData::Ptr& source, std::size_t count,
                             std::vector<Imath::V3d>& result) {
    if (!source || !source->vector || source->vectors.size() != count) return false;
    for (const auto& vector : source->vectors)
        if (!maya::isFiniteVector(vector)) return false;
    result = source->vectors; return true;
}
} }
enum class DMeshPointSpace { Local };
MStatus generateMMeshFromDmesh(const Mesh& mesh, MObject& object, DMeshPointSpace) {
    if (failures.fail("mesh")) return MS::kFailure;
    object.mesh = &mesh; return MS::kSuccess;
}
struct MFnMesh {
    const Mesh* mesh;
    MFnMesh(const MObject& object, MStatus* status) : mesh(object.mesh) {
        *status = failures.fail("bindMesh") ? MS::kFailure : MS::kSuccess;
    }
    MStatus getVertexNormal(int index, MVector& normal, MSpace::Space) {
        assert(mesh && index >= 0 && std::size_t(index) < mesh->points.size());
        if (failures.fail("normal")) return MS::kFailure;
        normal = mesh->normal; return MS::kSuccess;
    }
    MStatus getClosestNormal(const MPoint&, MVector& normal, MSpace::Space, int* polygon, void*) {
        if (failures.fail("closestNormal")) return MS::kFailure;
        *polygon=0; normal=mesh ? mesh->normal : MVector(0,1,0); return MS::kSuccess;
    }
    bool allIntersections(const MPoint&, const MFloatVector&, void*, void*, bool, MSpace::Space,
        float, bool, void*, bool, MFloatPointArray& hits, MFloatArray*, void*, void*, void*, void*,
        float, MStatus* status);
};
MPoint queryClosest;
MVector queryNormal(0,1,0);
bool rayReportsHit = false;
unsigned int rayHitCount = 0;
bool MFnMesh::allIntersections(const MPoint&, const MFloatVector&, void*, void*, bool, MSpace::Space,
    float, bool, void*, bool, MFloatPointArray& hits, MFloatArray*, void*, void*, void*, void*,
    float, MStatus* status) {
    *status = failures.fail("ray") ? MS::kFailure : MS::kSuccess;
    hits.count=rayHitCount; return rayReportsHit;
}
struct MFnNurbsCurve {
    MFnNurbsCurve(const MObject&, MStatus* status) {
        *status = failures.fail("bindCurve") ? MS::kFailure : MS::kSuccess;
    }
    MPoint closestPoint(const MPoint&, double* parameter, double, MSpace::Space, MStatus* status) {
        *status = failures.fail("curveClosest") ? MS::kFailure : MS::kSuccess;
        *parameter=0.25; return queryClosest;
    }
    MVector normal(double, MSpace::Space, MStatus* status) {
        *status = failures.fail("curveNormal") ? MS::kFailure : MS::kSuccess; return queryNormal;
    }
};
struct MFnNurbsSurface {
    MFnNurbsSurface(const MObject&, MStatus* status) {
        *status = failures.fail("bindSurface") ? MS::kFailure : MS::kSuccess;
    }
    MPoint closestPoint(const MPoint&, double* u, double* v, bool, double, MSpace::Space, MStatus* status) {
        *status = failures.fail("surfaceClosest") ? MS::kFailure : MS::kSuccess;
        *u=*v=0.25; return queryClosest;
    }
    MVector normal(double, double, MSpace::Space, MStatus* status) {
        *status = failures.fail("surfaceNormal") ? MS::kFailure : MS::kSuccess; return queryNormal;
    }
};
namespace dommy { namespace maya {
struct MeshClosestPointQuery {
    MStatus create(const MObject&) { return failures.fail("intersector") ? MS::kFailure : MS::kSuccess; }
    MStatus getClosestPoint(const MPoint&, MPoint& result) {
        if (failures.fail("meshClosest")) return MS::kFailure;
        result=queryClosest; return MS::kSuccess;
    }
};
} }
bool referenceConnected = false;
struct MPlug {
    MPlug(const MObject&, int) {}
    bool isConnected(MStatus* status) const {
        *status = failures.fail("connection") ? MS::kFailure : MS::kSuccess; return referenceConnected;
    }
};
MObject thisMObject() { return MObject(); }
struct MString { std::string text{"N"}; const char* asChar() const { return text.c_str(); } };
struct Value {
    float number = 1; int integer = 0; bool flag = false; float3 vector{{0,1,0}}; MString string;
    MObject object; MFnData::Type type=MFnData::kInvalid;
};
struct MDataHandle {
    Value value;
    bool valid = true;
    int* reads = nullptr;
    void check() const { assert(valid && reads); ++*reads; }
    float asFloat() const { check(); return value.number; }
    double asDouble() const { check(); return value.number; }
    short asShort() const { check(); return static_cast<short>(value.integer); }
    int asInt() const { check(); return value.integer; }
    bool asBool() const { check(); return value.flag; }
    const float3& asFloat3() const { check(); return value.vector; }
    MString asString() const { check(); return value.string; }
    MObject data() const { check(); return value.object; }
    MFnData::Type type() const { check(); return value.type; }
};
enum { aNoiseType, aSeed, aFrequency, aOctave, aLacunarity, aPersistence,
       aIsVoronoiDistance, aVoronoiDisplacement, aTurbulencePower, aTurbulenceRoughnesss,
       aWooodFrequency, aWooodGrain, aWooodScale, aWooodBias, aWooodAngle,
       aOffset, aScale, aUniformScale, aUseSeedOffset, aIsNoiseAttribute,
       aDeformType, aDeformVector, aNoiseAttributeName, aVectorAttributeName, aAmplitube,
       aInterpType, aGain, aFractalType, aCellularDistanceFunction, aCellularReturnType,
       aPositionWarpAmp, aAmplitude, aIsAbsolute, aTopRatio, aUnderRatio, aSideRatio,
       aNormalize, aInvert, aEpsilon, aNormalVectorType, aNormalVector, aReferenceObject,
       aIsDeform, aPrecision };
struct MDataBlock {
    int calls = 0, failAt = 0, reads = 0;
    std::map<int, Value> values;
    MDataBlock() { values[aOctave].integer = 1; values[aTurbulenceRoughnesss].integer = 1; }
    MDataHandle inputValue(int attribute, MStatus* status) {
        MDataHandle handle; handle.value = values[attribute]; handle.reads = &reads;
        handle.valid = ++calls != failAt;
        *status = handle.valid ? MS::kSuccess : MS::kFailure;
        return handle;
    }
};
#include "noise_methods.inc"

template<typename Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const noise::ExceptionInvalidParam&) { rejected = true; }
    assert(rejected);
}

void testIntegerArithmetic() {
    const int values[] = {INT_MIN, -100000, -1, 0, 1, 100000, INT_MAX};
    for (int x : values) for (int seed : values) {
        const int y = -7, z = 13;
        std::uint64_t n = (1619ULL * static_cast<std::uint32_t>(x) +
            31337ULL * static_cast<std::uint32_t>(y) + 6971ULL * static_cast<std::uint32_t>(z) +
            1013ULL * static_cast<std::uint32_t>(seed)) & 0x7fffffffULL;
        n ^= n >> 13;
        const std::uint64_t square = (n * n) & 0xffffffffULL;
        const std::uint64_t inner = (square * 60493ULL + 19990303ULL) & 0xffffffffULL;
        const int expected = static_cast<int>((n * inner + 1376312589ULL) & 0x7fffffffULL);
        assert(noise::IntValueNoise3D(x, y, z, seed) == expected);
    }
    assert(noise::WrapSeed(INT_MAX, 1U) == INT_MIN);
    assert(noise::WrapSeed(INT_MIN, 0xffffffffU) == INT_MAX);
    assert(noise::WrapSeed(-1, 1U) == 0);
}

void testGoldenValues() {
    // Recorded before the patch using Clang -fwrapv, the historical 32-bit
    // wrapping contract. Tests themselves do NOT use -fwrapv or UB suppression.
    const double expected[9][5] = {
        {0.34653662010498043,-0.77567675979003914,0.69616847599978615,0.12183305943567663,-0.47101498216422816},
        {0.5501224151279297,-0.25845395574414065,-0.10532276465203988,-0.89525541099315797,0.061182476319047802},
        {0.74634293676562502,0.02393587353125004,-0.3331699486704231,0.23599060075426245,0.47639763488325038},
        {0.43649717895532236,0.83799028791064489,-0.92636635092876496,0.36386296069666535,0.83670107083309986},
        {0.15271773958740226,-0.34839378917480446,0.037691516551166315,-0.88839906897146947,0.34648847460265803},
        {-0.39094242179687499,-0.68686515640625001,0.61096120560370148,0.12059604330361343,-0.23492768468829286},
        {0.88823113022167965,0.30771226044335942,-0.59775148956421653,1.2063772493173004,0.19215280203491092},
        {-0.59456515287695311,-0.27961969424609368,0.064786655298145812,-0.80148677916040811,-0.23987345583214056},
        {0.87017741105234381,0.65193282210468761,-0.99354428896659097,0.15438706732634105,0.020597210740189328}
    };
    const int seeds[] = {0,13,INT_MAX,INT_MIN};
    const double positions[] = {0.25,-1.25,10.5};
    for (int s = 0; s < 4; ++s) {
        noise::module::Perlin p; p.SetSeed(seeds[s]);
        noise::module::Billow b; b.SetSeed(seeds[s]);
        noise::module::RidgedMulti r; r.SetSeed(seeds[s]);
        noise::module::Voronoi v; v.SetSeed(seeds[s]); v.EnableDistance(true);
        noise::module::Turbulence t; t.SetSourceModule(0,p); t.SetSeed(seeds[s]);
        const noise::module::Module* modules[] = {&p,&b,&r,&v,&t};
        for (int x = 0; x < 3; ++x) for (int m = 0; m < 5; ++m) {
            const double value = modules[m]->GetValue(positions[x],0.75,-0.5);
            assert(std::fabs(value - expected[(s == 3 ? 0 : s) * 3 + x][m]) < 1e-12);
        }
    }
}

void testCoordinateBoundaries() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    noise::module::Voronoi voronoi;
    for (double value : {nan,inf,-inf,1e10,-1e10,double(INT_MIN),double(INT_MAX)}) {
        rejects([&] { voronoi.GetValue(value,0,0); });
        rejects([&] { noise::GradientCoherentNoise3D(0,value,0); });
        rejects([&] { noise::ValueCoherentNoise3D(0,0,value); });
    }
    for (double value : {double(INT_MIN)+4.0, double(INT_MAX)-4.0})
        assert(std::isfinite(voronoi.GetValue(value,0,0)));
    rejects([&] { voronoi.GetValue(double(INT_MAX)-3.0,0,0); });
    voronoi.SetFrequency(2);
    rejects([&] { voronoi.GetValue(1e10,0,0); });
    rejects([&] { noise::MakeInt32Range(nan); });
    rejects([&] { noise::MakeInt32Range(inf); });
    assert(std::isfinite(noise::MakeInt32Range(1e200)));
    assert(noise::CheckedLatticeLower(-1.0) == -2);
    assert(noise::CheckedLatticeLower(0.0) == -1);
}

void testGeneration() {
    NoiseBaseParams params;
    DPointList points{DPoint(0.25f,0.75f,-0.5f),DPoint(1.25f,0.75f,-0.5f)};
    DScalarDataList signals(2,9.0f), original(signals);
    for (int type = NOISE_TYPE_RANDOM; type < NOISE_TYPE_COUNT; ++type) {
        params.mNoiseType = static_cast<short>(type);
        assert(createNoiseFromPosition(params,points,signals) == MS::kSuccess);
        assert(signals.size() == points.size());
        for (float value : signals) assert(std::isfinite(value));
    }
    signals = original; params.mNoiseType = NOISE_TYPE_VORONOI;
    points[1][0] = 1e10f;
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    assert(signals == original); // A later split-range failure cannot publish earlier work.
    points[1][0] = 1.25f;
    params.mOffsetVec[0] = 1e10;
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    assert(signals == original);
    params = NoiseBaseParams(); params.mNoiseType = NOISE_TYPE_PERLIN;
    params.mLacunarity = 1e30f; params.mOctave = 30;
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    assert(signals == original);
    params = NoiseBaseParams(); params.mNoiseType = NOISE_TYPE_UNKNOWN;
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    params = NoiseBaseParams(); params.mOctave = 31;
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    params = NoiseBaseParams(); params.mFrequency = std::numeric_limits<float>::infinity();
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    params = NoiseBaseParams(); signals.clear();
    assert(createNoiseFromPosition(params,points,signals) == MS::kInvalidParameter && signals.empty());
    points.clear(); assert(createNoiseFromPosition(params,points,signals) == MS::kSuccess);
}

void testInputFailures() {
    for (auto reader : {read_mesh, read_curve}) {
        MDataBlock valid; NoiseBaseParams params;
        assert(reader(valid,params) == MS::kSuccess && valid.calls == 18);
        for (int failure = 1; failure <= valid.calls; ++failure) {
            MDataBlock data; data.failAt = failure;
            assert(reader(data,params) == MS::kFailure && data.reads == failure-1);
        }
    }
    MDataBlock valid; assert(read_controls(valid) == MS::kSuccess && valid.calls == 25);
    for (int failure = 1; failure <= valid.calls; ++failure) {
        MDataBlock data; data.failAt = failure;
        assert(read_controls(data) == MS::kFailure && data.reads == failure-1);
    }
    MDataBlock invalid; invalid.values[aDeformType].integer = NOISE_DEFROM_CURVE_NORMAL;
    assert(read_controls(invalid) == MS::kInvalidParameter);
    MDataBlock fast; FastNoiseParams params;
    assert(read_fast(fast,params) == MS::kSuccess && fast.calls == 15);
    for (int failure = 1; failure <= fast.calls; ++failure) {
        MDataBlock data; data.failAt = failure;
        assert(read_fast(data,params) == MS::kFailure && data.reads == failure-1);
    }
    fast = MDataBlock(); fast.values[aOctave].integer = -1;
    assert(read_fast(fast,params) == MS::kInvalidParameter);
    fast = MDataBlock(); fast.values[aNoiseType].integer = FAST_NOISE_TYPE_COUNT;
    assert(read_fast(fast,params) == MS::kInvalidParameter);
    for (auto reader : {read_point, read_pyro}) {
        MDataBlock good; good.values[aNoiseType].integer = NOISE_TYPE_PERLIN;
        NoiseBaseParams noiseParams;
        assert(reader(good,noiseParams) == MS::kSuccess);
        assert(good.calls == (reader == read_point ? 20 : 22));
        for (int failure = 1; failure <= good.calls; ++failure) {
            MDataBlock data; data.failAt = failure;
            assert(reader(data,noiseParams) == MS::kFailure && data.reads == failure-1);
        }
    }
    for (int failure = 1; failure <= 15; ++failure) {
        MDataBlock data; data.failAt = failure;
        assert(read_point_fast(data,params) == MS::kFailure && data.reads == failure-1);
    }
    invalid = MDataBlock(); invalid.values[aIsNoiseAttribute].flag = true;
    invalid.values[aNoiseAttributeName].string.text.clear();
    assert(read_controls(invalid) == MS::kInvalidParameter);
}

void testPointDisplacement() {
    Mesh input;
    input.points.assign(1,DPoint(0.25f,0.75f,-0.5f));
    NoiseBaseParams params;
    params.mNoiseType = NOISE_TYPE_VORONOI;
    params.mUseVoronoiDistance = true;
    params.mVoronoiDisplace = 1.0f;
    params.mSeed = INT_MAX;
    params.mAmplitude = 0.0f;
    DPointList output;
    assert(evaluate_points(&input,params,NOISE_DEFROM_VECTOR,
        MVector(3e38,0,0),"N",output) == MS::kSuccess);
    assert(output.size() == 1);
    for (unsigned int axis = 0; axis < 3; ++axis) assert(output[0][axis] == input.points[0][axis]);
    params.mAmplitude = 1.0f;
    const DPointList original = output;
    assert(evaluate_points(&input,params,NOISE_DEFROM_VECTOR,
        MVector(3e38,0,0),"N",output) == MS::kInvalidParameter);
    assert(output.size() == original.size());
    for (unsigned int axis = 0; axis < 3; ++axis) assert(output[0][axis] == original[0][axis]);
    assert(evaluate_points(&input,params,NOISE_DEFROM_ATTRIBUTE,
        MVector(0,1,0),"missing",output) == MS::kInvalidParameter);
}

struct ZeroNoise : noise::module::Module {
    ZeroNoise() : noise::module::Module(0) {}
    int GetSourceModuleCount() const override { return 0; }
    double GetValue(double, double, double) const override { return 0; }
};

void testPyroMathAndBudget() {
    NoiseBaseParams params;
    params.mNoiseType = NOISE_TYPE_PERLIN;
    params.mAmplitude = 0;
    params.mUniformScale = 1e20f;
    DPointList points{DPoint(1e20f,0,0)};
    DScalarDataList density(1,9);
    assert(createPyroNoiseFromPosition(params,DPoint(0),1,2e20f,points,density) == MS::kSuccess);
    assert(density[0] == 1); // The old float norm overflowed to infinity.
    points[0] = DPoint(3e38f,0,0);
    params.mUniformScale = 1e38f;
    assert(createPyroNoiseFromPosition(params,DPoint(-3e38f,0,0),1,3e38f,points,density) == MS::kSuccess);
    assert(density[0] == 0); // Coordinate subtraction must also occur in double.
    params = NoiseBaseParams(); params.mNoiseType = NOISE_TYPE_PERLIN;
    params.mAmplitude = 0;
    params.mTopRatio = 0.75f; params.mUnderRatio = 0.0f;
    points = {DPoint(0,1,0),DPoint(0,-1,0),DPoint(0),DPoint(1e-9f,0,0)};
    density.assign(points.size(),9);
    ZeroNoise zero;
    PyroNoiseFromPosition body(zero,DPoint(0),params,2,1,points,&density);
    body(tbb::blocked_range<std::size_t>(0,points.size()));
    assert((density == DScalarDataList{0,1,1,1}));
    params.mTopRatio = 0; params.mSideRatio = 1;
    points.assign(1,DPoint(1,0.1f,0)); density.assign(1,9);
    PyroNoiseFromPosition side(zero,DPoint(0),params,0.5f,1,points,&density);
    side(tbb::blocked_range<std::size_t>(0,1));
    assert(density[0] == 1);
    params = NoiseBaseParams(); params.mNoiseType = NOISE_TYPE_PERLIN;
    points = {DPoint(0.25f),DPoint(0.5f)}; density.assign(2,9);
    for (short type = NOISE_TYPE_PERLIN; type < NOISE_TYPE_COUNT; ++type) {
        params.mNoiseType = type;
        assert(createPyroNoiseFromPosition(params,DPoint(0),1,2,points,density) == MS::kSuccess);
    }
    density.assign(2,9); const auto original = density;
    points[1][0] = std::numeric_limits<float>::quiet_NaN();
    assert(createPyroNoiseFromPosition(params,DPoint(0),1,2,points,density) == MS::kInvalidParameter);
    assert(density == original);
    params.mSeed = INT_MAX;
    assert(pyro_budget(SizedPoints{1},1,params) == MS::kSuccess);
    assert(pyro_budget(SizedPoints{1},2,params) == MS::kInvalidParameter);
    params.mSeed = 0;
    assert(pyro_budget(SizedPoints{10000},10000,params) == MS::kSuccess);
    assert(pyro_budget(SizedPoints{10001},10001,params) == MS::kInvalidParameter);
    assert(pyro_budget(SizedPoints{10000000},0,params) == MS::kSuccess);
    assert(pyro_budget(SizedPoints{10000001},0,params) == MS::kInvalidParameter);
    assert(pyro_budget(SizedPoints{0},10000001,params) == MS::kInvalidParameter);
}

void testFastBoundary() {
    FastNoiseParams params;
    DPointList points{DPoint(0.25f),DPoint(1.25f)};
    DScalarDataList signals(2,9), original(signals);
    fastValue = 1.25f;
    assert(createFastNoiseFromPosition(params,points,signals) == MS::kSuccess);
    assert((signals == DScalarDataList{1.25f,1.25f}));
    signals = original;
    const auto invalid = [&](const FastNoiseParams& p, const DPointList& positions) {
        FastNoiseParams copy = p;
        fastCalls = fastSetupCalls = 0;
        assert(createFastNoiseFromPosition(copy,positions,signals) == MS::kInvalidParameter);
        assert(signals == original && fastCalls == 0 && fastSetupCalls == 0);
    };
    for (float coordinate : {1e10f,-1e10f,std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::quiet_NaN()}) {
        auto large = points; large[1][0] = coordinate; invalid(params,large);
    }
    params.mScaleVec[0] = 1e30; invalid(params,points);
    params = FastNoiseParams(); params.mOffsetVec[0] = 1e30; invalid(params,points);
    params = FastNoiseParams(); params.mFrequency = 1e30f; invalid(params,points);
    params = FastNoiseParams(); params.mNoiseType = FAST_NOISE_PERLINFRACTAL;
    params.mOctave = 3; params.mLacunarity = 20000; invalid(params,points);
    params = FastNoiseParams(); params.mFractalGain = 1e30f; params.mOctave = 30; invalid(params,points);
    params = FastNoiseParams(); params.mFractalGain = -1; params.mOctave = 2; invalid(params,points);
    params = FastNoiseParams(); params.mFrequency = 0;
    assert(createFastNoiseFromPosition(params,DPointList(2,DPoint(1e30f)),signals) == MS::kSuccess);
    params = FastNoiseParams(); signals = original;
    fastValue = std::numeric_limits<float>::quiet_NaN();
    assert(createFastNoiseFromPosition(params,points,signals) == MS::kInvalidParameter);
    assert(signals == original);
    fastValue = 1.25f;
    params.mSeed = INT_MAX;
    assert(advanceFastNoiseSeed(params,0,true) == MS::kSuccess);
    assert(advanceFastNoiseSeed(params,1,true) == MS::kInvalidParameter && params.mSeed == INT_MAX);
    params.mSeed = 4;
    for (std::size_t offset = 0; offset < 4; ++offset) assert(advanceFastNoiseSeed(params,offset,true));
    assert(params.mSeed == 10);
}

DMeshData sampleMeshes() {
    DMeshData data;
    for (int i = 0; i < 2; ++i) {
        auto mesh = std::make_shared<Mesh>();
        auto vectors = std::make_shared<Attribute>(); vectors->vector = true;
        vectors->vectors.assign(2,MVector(0,2,0)); mesh->attrs["N"] = vectors;
        data.meshes.push_back(mesh);
    }
    return data;
}

void testMeshPublicationAndFailures() {
    NoiseBaseParams params;
    for (bool publish : {false,true}) {
        failures.reset(); auto data = sampleMeshes(); const auto input = data.meshes[0]->getAttr("N");
        assert(evaluate_meshes(&data,params,false,publish,NOISE_DEFROM_ATTRIBUTE,
            MVector(0,1,0),"N","N",1) == MS::kSuccess);
        const auto output = data.meshes[0]->getAttr("N");
        assert(input->vector && input->vectors[0][1] == 2);
        if (publish) {
            assert(!output->vector && output != input);
            assert(std::fabs(data.meshes[0]->points[0][1] - 2 * output->scalars[0]) < 1e-6);
        } else assert(output == input); // OFF preserves the attribute object and type.
    }
    for (const std::string operation : {"mesh","bindMesh","normal","positions","attribute"}) {
        for (int ordinal = 1; ordinal <= (operation == "normal" ? 4 : 2); ++ordinal) {
            failures.reset(); failures.at[operation] = ordinal;
            const auto input = sampleMeshes(); DMeshData candidate;
            for (const auto& mesh : input.meshes) candidate.meshes.push_back(std::make_shared<Mesh>(*mesh));
            assert(evaluate_meshes(&candidate,params,false,true,NOISE_DEFROM_MESH_NORMAL,
                MVector(0,1,0),"noiseval","N",1) == MS::kFailure);
            // The actual compute publishes only after this loop succeeds.
            assert(input.meshes[0]->points[0][1] == 0 && !input.meshes[0]->getAttr("noiseval"));
        }
    }
    for (const std::string operation : {"mesh","bindMesh","normal"}) {
        failures.reset(); failures.at[operation] = 1;
        auto data = sampleMeshes(); std::vector<MVector> vectors(2);
        assert(fast_normals(data.meshes[0],vectors) == MS::kFailure);
    }
    failures.reset(); auto invalid = sampleMeshes();
    invalid.meshes[0]->normal[1] = std::numeric_limits<double>::infinity();
    assert(evaluate_meshes(&invalid,params,false,false,NOISE_DEFROM_MESH_NORMAL,
        MVector(0,1,0),"N","N",1) == MS::kInvalidParameter);
    failures.reset(); auto data = sampleMeshes(); params.mSeed = INT_MAX;
    assert(evaluate_meshes(&data,params,true,false,NOISE_DEFROM_NONE,
        MVector(0,1,0),"N","N",1) == MS::kInvalidParameter);
    params = NoiseBaseParams(); data = sampleMeshes();
    assert(evaluate_meshes(&data,params,false,false,NOISE_DEFROM_ATTRIBUTE,
        MVector(0,1,0),"N","missing",1) == MS::kInvalidParameter);
}

void testPointFastPublication() {
    FastNoiseParams params;
    const auto noNormals = [](const DPointList&, std::vector<MVector>&) -> MStatus {
        return MS::kFailure;
    };
    for (bool publish : {false,true}) {
        failures.reset(); auto data = sampleMeshes(); auto& item = *data.meshes[0];
        const auto input = item.getAttr("N");
        assert(deformFastNoiseGeometry(item,params,NOISE_DEFROM_ATTRIBUTE,MVector(0,1,0),
            "N",publish,"N",noNormals) == MS::kSuccess);
        assert(item.points[0][1] == 2.5f && input->vector);
        if (publish) assert(!item.getAttr("N")->vector && item.getAttr("N") != input);
        else assert(item.getAttr("N") == input);
    }
    for (const std::string operation : {"mesh","bindMesh","normal","positions","attribute"}) {
        for (int ordinal = 1; ordinal <= (operation == "normal" ? 2 : 1); ++ordinal) {
            failures.reset(); failures.at[operation] = ordinal;
            auto data = sampleMeshes(); const auto input = data.meshes[0];
            auto candidate = std::make_shared<Mesh>(*input);
            assert(deformFastNoiseGeometry(*candidate,params,NOISE_DEFROM_MESH_NORMAL,
                MVector(0,1,0),"N",true,"noiseval",
                [&](const DPointList& points, std::vector<MVector>& vectors) -> MStatus {
                    return point_fast_normals(candidate,points,vectors);
                }) == MS::kFailure);
            assert(input->points[0][1] == 0 && !input->getAttr("noiseval"));
        }
    }
    failures.reset(); auto data = sampleMeshes(); auto& item = *data.meshes[0];
    params.mAmplitude = 0;
    assert(deformFastNoiseGeometry(item,params,NOISE_DEFROM_VECTOR,MVector(3e38,0,0),
        "N",false,"N",noNormals) == MS::kSuccess);
    for (unsigned int axis = 0; axis < 3; ++axis) assert(item.points[0][axis] == 0);
    assert(deformFastNoiseGeometry(item,params,NOISE_DEFROM_ATTRIBUTE,MVector(0,1,0),
        "missing",false,"N",noNormals) == MS::kInvalidParameter);
}

NoiseBaseParams dfParameters() {
    NoiseBaseParams params;
    params.mNoiseType = NOISE_TYPE_PERLIN;
    params.normalType = 1;
    params.normalVecBase = MVector(0,1,0);
    return params;
}

template <typename List> bool sameTriples(const List& first, const List& second) {
    if (first.size() != second.size()) return false;
    for (std::size_t i=0; i<first.size(); ++i)
        for (unsigned int axis=0; axis<3; ++axis)
            if (first[i][axis] != second[i][axis]) return false;
    return true;
}

void testDFNoiseArithmeticAndPublication() {
    NoiseBaseParams params = dfParameters();
    const DPointList original{DPoint(.25f,.75f,-.5f),DPoint(.5f,.75f,-.5f)};
    DVec3DataList raw(original.size());
    assert(createDFNoiseFromPosition(params,original,raw));
    assert(raw[0].length() > .9f);
    for (short precision : {short(DP_ATTR_PRE_FLOAT),short(DP_ATTR_PRE_DOUBLE)}) {
        for (float amplitude : {0.f,2.f,-2.f}) {
            failures.reset(); params.mAmplitude=amplitude;
            Mesh points; points.points=original;
            // Replacing a pre-existing scalar must be checked and allow a type change.
            points.attrs["noise"] = std::make_shared<DPFloatAttr>(std::vector<float>(2,8));
            DMeshData primitives;
            primitives.meshes.push_back(std::make_shared<Mesh>(points));
            assert(deformDFNoiseGeometry(points,params,true,true,precision,"noise"));
            assert(deformDFNoisePrimitives(primitives,params,false,true,true,precision,"noise"));
            assert(sameTriples(points.points,primitives.meshes[0]->points));
            const auto attribute=points.getAttr("noise");
            assert(attribute && attribute->vector && attribute->doublePrecision == (precision==DP_ATTR_PRE_DOUBLE));
            for (std::size_t i=0; i<original.size(); ++i) for (unsigned int axis=0; axis<3; ++axis) {
                const float expected = float(double(original[i][axis])+double(raw[i][axis])*amplitude);
                assert(points.points[i][axis] == expected);
                assert(attribute->vectors[i][axis] == raw[i][axis]);
            }
        }
    }
    failures.reset(); params = dfParameters();
    Mesh off; off.points=original;
    off.attrs["noise"] = std::make_shared<DPFloatAttr>(std::vector<float>(2,8));
    const auto prior=off.getAttr("noise");
    assert(deformDFNoiseGeometry(off,params,false,false,DP_ATTR_PRE_FLOAT,"noise"));
    assert(off.getAttr("noise")==prior && sameTriples(off.points,original));
    assert(deformDFNoiseGeometry(off,params,true,true,DP_ATTR_PRE_FLOAT,"")==MS::kInvalidParameter);
    assert(sameTriples(off.points,original));
    assert(deformDFNoiseGeometry(off,params,false,false,9,"noise")==MS::kInvalidParameter);
    for (const std::string operation : {"positions","attribute"}) {
        failures.reset(); failures.at[operation]=1;
        Mesh candidate=off;
        assert(deformDFNoiseGeometry(candidate,params,true,true,DP_ATTR_PRE_FLOAT,"new")==MS::kFailure);
        assert(sameTriples(off.points,original) && !off.getAttr("new"));
    }
    failures.reset(); Mesh empty; empty.points.clear();
    assert(deformDFNoiseGeometry(empty,params,false,true,DP_ATTR_PRE_FLOAT,"empty"));
    assert(empty.getAttr("empty") && empty.getAttr("empty")->attrSize()==0);
    params.mNormalize=false; params.normalVecBase=MVector(0,1e10,0); params.mAmplitude=3e38f;
    Mesh large; large.points=original;
    assert(deformDFNoiseGeometry(large,params,true,true,DP_ATTR_PRE_FLOAT,"noise")==MS::kInvalidParameter);
    assert(sameTriples(large.points,original) && !large.getAttr("noise"));
    params.mAmplitude=0;
    assert(deformDFNoiseGeometry(large,params,true,true,DP_ATTR_PRE_FLOAT,"noise"));
    assert(sameTriples(large.points,original));
    params=dfParameters();
    assert(validateDFNoiseSeedRange(INT_MAX,1,true));
    assert(validateDFNoiseSeedRange(INT_MAX,2,true)==MS::kInvalidParameter);
    assert(validateDFNoiseSeedRange(0,65536,true));
    assert(validateDFNoiseSeedRange(0,65537,true)==MS::kInvalidParameter);
    assert(validateDFNoiseSeedRange(0,std::numeric_limits<std::size_t>::max(),true)==MS::kInvalidParameter);
    assert(validateDFNoiseSeedRange(INT_MAX,1000000,false));
    params.mSeed=INT_MAX;
    auto data=sampleMeshes();
    assert(deformDFNoisePrimitives(data,params,true,false,false,DP_ATTR_PRE_FLOAT,"noise")==MS::kInvalidParameter);
}

void testDFInputAndWorkerFailures() {
    failures.reset(); referenceConnected=false;
    MDataBlock valid; valid.values[aNoiseType].integer=NOISE_TYPE_PERLIN;
    NoiseBaseParams params;
    assert(read_df(valid,params) && valid.calls==24);
    for (int failure=1; failure<=24; ++failure) {
        MDataBlock data; data.failAt=failure;
        assert(read_df(data,params)==MS::kFailure && data.reads==failure-1);
    }
    valid=MDataBlock(); valid.values[aNoiseType].integer=NOISE_TYPE_PERLIN;
    assert(read_df_controls(valid) && valid.calls==29);
    for (int failure=1; failure<=29; ++failure) {
        MDataBlock data; data.failAt=failure;
        assert(read_df_controls(data)==MS::kFailure && data.reads==failure-1);
    }
    valid=MDataBlock(); valid.values[aNoiseType].integer=NOISE_TYPE_PERLIN;
    valid.values[aIsNoiseAttribute].flag=true; valid.values[aNoiseAttributeName].string.text="";
    assert(read_df_controls(valid)==MS::kInvalidParameter);
    valid=MDataBlock(); valid.values[aNoiseType].integer=NOISE_TYPE_PERLIN;
    valid.values[aNormalVectorType].integer=2;
    assert(read_df(valid,params) && params.normalType==0);
    referenceConnected=true; failures.reset(); failures.at["connection"]=1;
    assert(read_df(valid,params)==MS::kFailure);
    failures.reset();
    valid=MDataBlock(); valid.values[aNoiseType].integer=NOISE_TYPE_PERLIN;
    valid.values[aNormalVectorType].integer=2;
    assert(read_df(valid,params)==MS::kInvalidParameter);
    valid.values[aReferenceObject].object.token=true;
    valid.values[aReferenceObject].type=MFnData::kNurbsCurve;
    valid.calls=valid.reads=0; valid.failAt=25;
    assert(read_df(valid,params)==MS::kFailure && valid.reads==24);
    valid.calls=valid.reads=0; valid.failAt=0;
    assert(read_df(valid,params) && params.normalType==2 && !params.m_referenceObject.isNull());
    referenceConnected=false;

    const DPointList points{DPoint(.25f,.75f,-.5f),DPoint(.5f,.75f,-.5f)};
    const DVec3DataList original(2,DVec3Data(9,9,9));
    DVec3DataList signals=original;
    for (short type=NOISE_TYPE_PERLIN; type<NOISE_TYPE_COUNT; ++type) {
        params=dfParameters(); params.mNoiseType=type;
        assert(createDFNoiseFromPosition(params,points,signals));
    }
    params=dfParameters(); signals=original;
    auto invalidPoints=points; invalidPoints[1][0]=std::numeric_limits<float>::quiet_NaN();
    assert(createDFNoiseFromPosition(params,invalidPoints,signals)==MS::kInvalidParameter);
    assert(sameTriples(signals,original));
    params.mNoiseType=NOISE_TYPE_VORONOI; invalidPoints=points; invalidPoints[1][0]=1e10f;
    assert(createDFNoiseFromPosition(params,invalidPoints,signals)==MS::kInvalidParameter);
    assert(sameTriples(signals,original));
    for (const auto type : {MFnData::kNurbsCurve,MFnData::kNurbsSurface,MFnData::kMesh}) {
        params=dfParameters(); params.normalType=2;
        params.m_referenceObject.token=true; params.m_referenceObjectType=type;
        const std::vector<std::string> operations = type==MFnData::kNurbsCurve
            ? std::vector<std::string>{"bindCurve","curveClosest","curveNormal"}
            : type==MFnData::kNurbsSurface ? std::vector<std::string>{"bindSurface","surfaceClosest","surfaceNormal"}
            : std::vector<std::string>{"bindMesh","closestNormal"};
        for (const auto& operation : operations) {
            failures.reset(); failures.at[operation]=1; signals=original;
            assert(createDFNoiseFromPosition(params,points,signals)==MS::kFailure);
            assert(sameTriples(signals,original));
        }
        failures.reset(); assert(createDFNoiseFromPosition(params,points,signals));
    }
    failures.reset();
}

void testObjectPyroNoise() {
    NoiseBaseParams params; params.mNoiseType=NOISE_TYPE_PERLIN; params.mAmplitude=-2;
    DPointList points{DPoint(.5f,0,0),DPoint(2,0,0)};
    DScalarDataList density(2,9);
    MObject object; object.token=true;
    ZeroNoise zero;
    queryClosest=MPoint(); failures.reset();
    PyroNoiseFromCurve curve(zero,params,1,1,object,points,&density);
    assert(curve(tbb::blocked_range<unsigned int>(0,2)));
    assert((density==DScalarDataList{1,0}));
    // Real Perlin can exceed unit amplitude; multiplication must stay double.
    noise::module::Perlin perlin; perlin.SetSeed(1); perlin.SetOctaveCount(2); perlin.SetPersistence(10);
    params.mAmplitude=3e38f; points.assign(1,DPoint(.125f,0,0)); density.assign(1,9);
    const double signal=perlin.GetValue(.125,0,0);
    assert(std::abs(signal)>1 && std::isfinite(signal*double(params.mAmplitude)));
    PyroNoiseFromCurve largeCurve(perlin,params,1,1,object,points,&density);
    assert(largeCurve(tbb::blocked_range<unsigned int>(0,1)) && density[0]==1);
    rayReportsHit=false; rayHitCount=0;
    PyroNoiseFromMesh largeMesh(perlin,params,1,1,object,points,&density);
    assert(largeMesh(tbb::blocked_range<unsigned int>(0,1)));
    assert(density[0]==(double(.125f)+signal*double(params.mAmplitude)<1 ? 1.f : 0.f));
    params.mAmplitude=0; params.mUniformScale=1e20f;
    points.assign(1,DPoint(1e20f,0,0)); density.assign(1,9);
    PyroNoiseFromCurve farCurve(zero,params,2e20f,1,object,points,&density);
    assert(farCurve(tbb::blocked_range<unsigned int>(0,1)) && density[0]==1);
    PyroNoiseFromMesh farMesh(zero,params,2e20f,1,object,points,&density);
    assert(farMesh(tbb::blocked_range<unsigned int>(0,1)) && density[0]==1);
    params.mUniformScale=1; points={DPoint(.5f,0,0),DPoint(2,0,0)};
    for (bool meshMode : {false,true}) {
        const std::vector<std::string> operations = meshMode
            ? std::vector<std::string>{"bindMesh","intersector","ray","meshClosest"}
            : std::vector<std::string>{"bindCurve","curveClosest"};
        for (const auto& operation : operations) {
            for (int failure=1; failure <= (operation=="ray" || operation=="meshClosest" || operation=="curveClosest" ? 2 : 1); ++failure) {
                failures.reset(); failures.at[operation]=failure; density.assign(2,9);
                PyroNoiseFromCurve c(zero,params,1,1,object,points,&density);
                PyroNoiseFromMesh m(zero,params,1,1,object,points,&density);
                const auto range=tbb::blocked_range<unsigned int>(0,2);
                assert((meshMode ? m(range) : c(range))==MS::kFailure);
                assert((density==DScalarDataList{9,9}));
            }
        }
    }
    failures.reset(); failures.at["ray"]=1; rayReportsHit=true; rayHitCount=1;
    density.assign(2,9);
    PyroNoiseFromMesh badRay(zero,params,1,1,object,points,&density);
    assert(badRay(tbb::blocked_range<unsigned int>(0,2))==MS::kFailure);
    assert((density==DScalarDataList{9,9})); // A hit must not hide failed query status.
    failures.reset(); rayReportsHit=false; rayHitCount=0;
    points[1][0]=std::numeric_limits<float>::quiet_NaN();
    PyroNoiseFromCurve invalid(zero,params,1,1,object,points,&density);
    assert(invalid(tbb::blocked_range<unsigned int>(0,2))==MS::kInvalidParameter);
    assert((density==DScalarDataList{9,9}));
}

int main() {
    testIntegerArithmetic(); testGoldenValues(); testCoordinateBoundaries();
    testGeneration(); testInputFailures(); testMeshPublicationAndFailures();
    testPointDisplacement(); testPyroMathAndBudget(); testFastBoundary(); testPointFastPublication();
    testDFNoiseArithmeticAndPublication(); testDFInputAndWorkerFailures(); testObjectPyroNoise();
    std::cout << "Noise safety tests: OK (real libnoise; mocked host and split ranges)\n";
}
