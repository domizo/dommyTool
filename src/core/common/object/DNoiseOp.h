//
//  DNoiseOp.hpp
//  dommyTool
//
//  Created by yuzo on 2016/07/13.
//
//

#pragma once

#include <cmath>
#include <stdio.h>
#include <vector>

#include "DMayaHeaders.h"
#include "DPData.h"
#include "PluginFeatures.h"

#include "perlin.h"
#include "turbulence.h"
#include "billow.h"
#include "voronoi.h"
#include "ridgedmulti.h"
#include "module.h"


#include <tbb/tick_count.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <tbb/combinable.h>

#include "DNodeOperation.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DDirectionMath.h"

#if DOMMY_WITH_FASTNOISE
#include "FastNoise.h"
#endif
#include "dommyMath.h"
#include "common/utility/DMayaRandomValueGeneration.h"

#pragma mark Noise Utilities


//namespace {
    enum {
        NOISE_TYPE_UNKNOWN = -1,
        NOISE_TYPE_RANDOM = 0,
        NOISE_TYPE_PERLIN,
        NOISE_TYPE_BILLOW,
        NOISE_TYPE_VORONOI,
        NOISE_TYPE_TURBULANCE,
        NOISE_TYPE_RIGDEDMULTI,
        NOISE_TYPE_WOOD,
        NOISE_TYPE_COUNT
    };typedef short NoiseBaseType;

enum {
    NOISE_DEFROM_NONE = 0,
    NOISE_DEFROM_VECTOR,
    NOISE_DEFROM_ATTRIBUTE,
    NOISE_DEFROM_CURVE_NORMAL,
    NOISE_DEFROM_MESH_NORMAL,
    NOISE_DEFROM_COUNT
};typedef short NoiseDeformType;

enum {
    NOISE_CALC_POS_WORLD = 0,
    NOISE_CALC_PARAM,
    NOISE_CALC_POSITION_COUNT
};typedef short NoiseCalcPositionType;

#if DOMMY_WITH_FASTNOISE
enum {
    FAST_NOISE_VALUE = 0,
    FAST_NOISE_VALUEFRACTAL,
    FAST_NOISE_PERLIN,
    FAST_NOISE_PERLINFRACTAL,
    FAST_NOISE_SIMPLEX,
    FAST_NOISE_SIMPLEXFRACTAL,
    FAST_NOISE_CELLULAR,
    FAST_NOISE_WHITENOISE,
    FAST_NOISE_CUBIC,
    FAST_NOISE_CUBICFRACTAL,
    FAST_NOISE_TYPE_COUNT
};typedef short FastNoiseType;
enum {
    FN_INTERP_LINEAR = 0,
    FN_INTERP_HERMITE,
    FN_INTERP_QUINTIC,
    FN_INTERP_TYPE_COUNT
};typedef short FNInterpType;


enum {
    FN_FRAC_FBM = 0,
    FN_FRAC_BILLOW,
    FN_FRAC_RIGIDMULTI,
    FN_FRAC_TYPE_COUNT
};typedef short FNFractalType;

enum {
    FN_CELL_DIST_EUCLIDEAN = 0,
    FN_CELL_DIST_MANHATTAN,
    FN_CELL_DIST_NATURAL,
    FN_CELL_DIST_TYPE_COUNT
};typedef short FNCellularDistanceType;

enum {
    FN_CELL_RTN_CELLVALUE = 0,
    FN_CELL_RTN_NOISELOOKUP,
    FN_CELL_RTN_DISTANCE,
    FN_CELL_RTN_DISTANCE2,
    FN_CELL_RTN_DISTANCE2ADD,
    FN_CELL_RTN_DISTANCE2SUB,
    FN_CELL_RTN_DISTANCE2MUL,
    FN_CELL_RTN_DISTANCE2DIV,
    FN_CELL_RTN_TYPE_COUNT
};typedef short FNCellularReturnType;
#endif

    struct NoiseBaseParams
    {
        NoiseBaseParams()
        : mSeed(1)
        , mNoiseType(NOISE_TYPE_RANDOM)
        , mAmplitude(1.0f)
        , mFrequency(1.0f)
        , mOctave(1)
        , mLacunarity(2.0f)
        , mPersistence(0.5f)
        , mUseVoronoiDistance(false)
        , mVoronoiDisplace(0.0f)
        , mTurbulancePower(0.0f)
        , mTurbulanceRoughness(1)
        , mWooodFrequency(1.0f)
        , mWooodGrain(0.0f)
        , mWooodScale(1.0f)
        , mWooodBias(0.0f)
        , mWooodAngle(0.0, 0.0, 0.0)
        , mOffsetVec(0.0, 0.0, 0.0)
        , mScaleVec(1.0, 1.0, 1.0)
        , mUniformScale(1.0f)
        , mNoiseQuality(noise::QUALITY_BEST)
        , isAbs(false)
        , mNormalize(true)
        , mInvert(false)
        , m_referenceObjectType(MFnData::kInvalid)
        , normalType(0)
        , normalVecBase(0.0, 1.0, 0.0)
        , epsilon(0.0001)
        , mTopRatio(1.0f)
        , mUnderRatio(1.0f)
        , mSideRatio(1.0f)
        { }
        
        int mSeed;
        NoiseBaseType mNoiseType;
		
		float mAmplitude;
        float mFrequency;
        int mOctave;
        float mLacunarity;
        float mPersistence;
        
        //voronoi
        bool mUseVoronoiDistance;
        float mVoronoiDisplace;
        
        //turbulance
        float mTurbulancePower;
        int mTurbulanceRoughness;
        
        //Wood
        float mWooodFrequency;
        float mWooodGrain;
        float mWooodScale;
        float mWooodBias;
        MVector mWooodAngle;
        
        
        
        MVector mOffsetVec;
        MVector mScaleVec;
        float mUniformScale;
        noise::NoiseQuality mNoiseQuality;
        bool isAbs;
        bool mNormalize;
        bool mInvert;
        
        //for dfnoise
        MObject m_referenceObject;
        MFnData::Type m_referenceObjectType;
        short normalType;
        MVector normalVecBase;
        double epsilon;
//        short precision;
		
		//for pyro
		float mTopRatio;
		float mUnderRatio;
		float mSideRatio;
    };
    
#if DOMMY_WITH_FASTNOISE
struct FastNoiseParams
{
    FastNoiseParams()
        : mSeed(1)
        , mNoiseType(FAST_NOISE_SIMPLEX)
        , mInterpType(FN_INTERP_QUINTIC)
        , mFractalType(FN_FRAC_FBM)
        , mCellDistanceType(FN_CELL_DIST_EUCLIDEAN)
        , mCellReturnType(FN_CELL_RTN_CELLVALUE)
        , mAmplitude(1.0f)
        , mFrequency(1.0f)
        , mOctave(1U)
        , mLacunarity(2.0f)
        , mFractalGain(0.5f)
        , mPositionWarpAmp(0.0f)
        , mOffsetVec(0.0, 0.0, 0.0)
        , mScaleVec(1.0, 1.0, 1.0)
        , mUniformScale(1.0)
    { }
    
    int mSeed;
    FastNoiseType mNoiseType;
    FNInterpType mInterpType;
    FNFractalType mFractalType;
    FNCellularDistanceType mCellDistanceType;
    FNCellularReturnType mCellReturnType;
    
    float mAmplitude;
    float mFrequency;
    unsigned int mOctave;
    float mLacunarity;
    float mFractalGain;
    
    float mPositionWarpAmp;
    
    
    MVector mOffsetVec;
    MVector mScaleVec;
    double mUniformScale;
    
    
    
    FastNoise mFNoise;
};
#endif




//}//Noise Utilities




class NoiseFromDP {
public:
    DScalarDataList *signals;
    const DPointList *newPosition;
    const MVector offsetVal;
    const MVector scaleVal;
    const noise::module::Module *mod;
	
    NoiseFromDP(const noise::module::Module &mod,
                const MVector &offsetVal,
                const MVector &scaleVal,
                const DPointList &newPosition,
                DScalarDataList &signals) :
    signals(&signals),
    newPosition(&newPosition),
    offsetVal(offsetVal),
    scaleVal(scaleVal),
    mod(&mod)
    { }
	
    void operator() (const tbb::blocked_range<std::size_t>& r) const {
        for (std::size_t i = r.begin(); i != r.end(); ++i) {
            double position[3];
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                position[axis] = (double((*newPosition)[i][axis]) - offsetVal[axis]) * scaleVal[axis];
                if (!std::isfinite(position[axis])) throw noise::ExceptionInvalidParam();
            }
            const double value = mod->GetValue(position[0], position[1], position[2]);
            if (!dommy::numeric::checkedFloatCast(value, (*signals)[i]))
                throw noise::ExceptionInvalidParam();
        }
    }
};


#if DOMMY_WITH_FASTNOISE
// Validates the exact floating-point coordinates used by the legacy backend,
// including frequency/octave scaling and spare room for simplex skew and neighbors.
bool makeFastNoisePosition(const FastNoiseParams& params, const DPoint& point,
                           FN_DECIMAL (&position)[3]);

class FastNoiseFromDP {
public:
    FastNoiseFromDP(FastNoiseParams& params, const DPointList& points, DScalarDataList& signals)
        : params(params), points(points), signals(signals) {}
    void operator()(const tbb::blocked_range<std::size_t>& range) const {
        for (std::size_t i = range.begin(); i != range.end(); ++i) {
            FN_DECIMAL position[3];
            if (!makeFastNoisePosition(params, points[i], position))
                throw noise::ExceptionInvalidParam();
            const double signal = params.mFNoise.GetNoise(position[0], position[1], position[2]);
            if (!dommy::numeric::checkedFloatCast(signal, signals[i]))
                throw noise::ExceptionInvalidParam();
        }
    }
private:
    FastNoiseParams& params;
    const DPointList& points;
    DScalarDataList& signals;
};
#endif




class PyroNoiseFromPosition {
public:
    DScalarDataList *densityArray;
    const DPointList *newPosition;
    const DPoint basepPosition;
	const NoiseBaseParams mNoiseParam;
    const float m_radius;
    const float m_density;
    const noise::module::Module *mod;
	
    PyroNoiseFromPosition(const noise::module::Module &mod,
                          const DPoint &basepPosition,
						  const NoiseBaseParams &noiseParam,
                          const float &m_radius,
                          const float &m_density,
                          const DPointList &newPosition,
                          DScalarDataList *densityArray) :
    densityArray(densityArray),
    newPosition(&newPosition),
    basepPosition(basepPosition),
	mNoiseParam(noiseParam),
    m_radius(m_radius),
    m_density(m_density),
    mod(&mod)
    { }
	
    void operator()(const tbb::blocked_range<std::size_t>& range) const {
        for (std::size_t i = range.begin(); i != range.end(); ++i) {
            dommy::direction::Delta delta;
            if (!dommy::direction::difference(basepPosition, (*newPosition)[i], delta))
                throw noise::ExceptionInvalidParam();
            // Preserve the historical near-origin angular convention.
            const double y = delta.length > std::numeric_limits<float>::epsilon()
                ? delta.values[1] / delta.length : 0.0;
            double amplitude = mNoiseParam.mAmplitude;
            double radius = m_radius;
            const double dot = std::fabs(y);
            if (dot > 0.0) {
                const double ratio = y < 0.0 ? mNoiseParam.mUnderRatio : mNoiseParam.mTopRatio;
                amplitude -= (amplitude * dot) * ratio;
                radius -= (radius * dot) * ratio;
                if (dot < 0.45) {
                    const double side = (1.0 - dot / 0.45) * mNoiseParam.mSideRatio;
                    amplitude += side;
                    radius += side;
                }
            }
            double position[3];
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                position[axis] = delta.values[axis] / mNoiseParam.mUniformScale + mNoiseParam.mOffsetVec[axis];
                if (!std::isfinite(position[axis])) throw noise::ExceptionInvalidParam();
            }
            const double signal = mod->GetValue(position[0], position[1], position[2]);
            const double distance = delta.length + signal * amplitude;
            if (!std::isfinite(signal) || !std::isfinite(amplitude) ||
                !std::isfinite(radius) || !std::isfinite(distance)) throw noise::ExceptionInvalidParam();
            (*densityArray)[i] = distance < radius ? m_density : 0.0f;
        }
    }
};


MStatus validatePyroNoiseParams(const NoiseBaseParams& params);

class PyroNoiseFromCurve {
public:
	DScalarDataList *densityArray;
	const DPointList *newPosition;
	const NoiseBaseParams mNoiseParam;
	const float m_radius;
	const float m_density;
	const noise::module::Module *mod;
	const MObject crvObj;
	
	PyroNoiseFromCurve(const noise::module::Module &mod,
					const NoiseBaseParams &noiseParam,
					const float &m_radius,
					const float &m_density,
						const MObject &crvObj,
						const DPointList &newPosition,
						DScalarDataList *densityArray) :
		densityArray(densityArray),
		newPosition(&newPosition),
		mNoiseParam(noiseParam),
		m_radius(m_radius),
		m_density(m_density),
		mod(&mod),
		crvObj(crvObj)
	{ }
	
    MStatus operator()(const tbb::blocked_range<uint>& range) const try
    {
        if (!densityArray || !newPosition || !mod ||
            !validatePyroNoiseParams(mNoiseParam) ||
            !std::isfinite(m_radius) || m_radius < 0.0f || !std::isfinite(m_density) ||
            densityArray->size() != newPosition->size() ||
            newPosition->size() > dommy::generation::kMaxGeneratedPointCount ||
            range.begin() > range.end() || range.end() > newPosition->size())
            return MS::kInvalidParameter;
        MStatus status;
        MFnNurbsCurve curve(crvObj, &status);
        if (!status) return status;
        DScalarDataList candidate(*densityArray);
        for (std::size_t i = range.begin(); i < range.end(); ++i) {
            candidate[i] = 0.0f;
            const DPoint& point = (*newPosition)[i];
            const MPoint current(point[0], point[1], point[2]);
            if (!dommy::maya::isFinitePoint(current)) return MS::kInvalidParameter;
            double parameter = 0.0;
            const MPoint closest = curve.closestPoint(
                current, &parameter, 0.01, MSpace::kWorld, &status);
            if (!status) return status;
            if (!std::isfinite(parameter) || !dommy::maya::isFinitePoint(closest))
                return MS::kInvalidParameter;
            dommy::direction::Delta delta;
            if (!dommy::direction::difference(closest, current, delta))
                return MS::kInvalidParameter;
            // Radius + amplitude is not a valid culling bound: amplitude can
            // be negative and the configured multi-octave signal can exceed 1.
            double position[3];
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                position[axis] = delta.values[axis] / mNoiseParam.mUniformScale +
                    mNoiseParam.mOffsetVec[axis];
                if (!std::isfinite(position[axis])) return MS::kInvalidParameter;
            }
            const double signal = mod->GetValue(position[0], position[1], position[2]);
            const double noiseValue = signal * double(mNoiseParam.mAmplitude);
            const double radius = double(m_radius) +
                (delta.length > std::numeric_limits<double>::epsilon() ? std::abs(noiseValue) : 0.0);
            if (!std::isfinite(signal) || !std::isfinite(noiseValue) || !std::isfinite(radius))
                return MS::kInvalidParameter;
            candidate[i] = delta.length < radius ? m_density : 0.0f;
        }
        densityArray->swap(candidate);
        return MS::kSuccess;
    } catch (const noise::Exception&) {
        return MS::kInvalidParameter;
    }
};



class PyroNoiseFromMesh {
public:
	DScalarDataList *densityArray;
	const DPointList *newPosition;
	const NoiseBaseParams mNoiseParam;
	const float m_radius;
	const float m_density;
	const noise::module::Module *mod;
	const MObject meshObj;
	
	PyroNoiseFromMesh(const noise::module::Module &mod,
					   const NoiseBaseParams &noiseParam,
					   const float &m_radius,
					   const float &m_density,
						   const MObject &meshObj,
						   const DPointList &newPosition,
						   DScalarDataList *densityArray) :
		densityArray(densityArray),
		newPosition(&newPosition),
		mNoiseParam(noiseParam),
		m_radius(m_radius),
		m_density(m_density),
		mod(&mod),
		meshObj(meshObj)
	{ }
	
    MStatus operator()(const tbb::blocked_range<uint>& range) const try
    {
        if (!densityArray || !newPosition || !mod ||
            !validatePyroNoiseParams(mNoiseParam) ||
            !std::isfinite(m_radius) || m_radius < 0.0f || !std::isfinite(m_density) ||
            densityArray->size() != newPosition->size() ||
            newPosition->size() > dommy::generation::kMaxGeneratedPointCount ||
            range.begin() > range.end() || range.end() > newPosition->size())
            return MS::kInvalidParameter;
        MStatus status;
        MFnMesh mesh(meshObj, &status);
        if (!status) return status;
        dommy::maya::MeshClosestPointQuery closestPointQuery;
        status = closestPointQuery.create(meshObj);
        if (!status) return status;
        const MFloatVector direction(0.0f, 1.0f, 0.0f);
        DScalarDataList candidate(*densityArray);
        for (std::size_t i = range.begin(); i < range.end(); ++i) {
            candidate[i] = 0.0f;
            const DPoint& point = (*newPosition)[i];
            const MPoint current(point[0], point[1], point[2]);
            if (!dommy::maya::isFinitePoint(current)) return MS::kInvalidParameter;
            MFloatPointArray hits;
            MFloatArray hitParameters;
            // Preserve the historical parity test and ray interval. Large/open
            // mesh classification remains a Maya runtime compatibility check.
            const bool hit = mesh.allIntersections(current, direction, nullptr, nullptr,
                false, MSpace::kWorld, 9999, false, nullptr, false, hits, &hitParameters,
                nullptr, nullptr, nullptr, nullptr, 1.0e-6f, &status);
            if (!status) return status;
            if (hit && hits.length() % 2U != 0U) {
                candidate[i] = m_density;
                continue;
            }
            MPoint closest;
            status = closestPointQuery.getClosestPoint(current, closest);
            if (!status) return status;
            dommy::direction::Delta delta;
            if (!dommy::direction::difference(closest, current, delta))
                return MS::kInvalidParameter;
            // Mesh mode historically samples absolute position, unlike curve mode.
            double position[3];
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                position[axis] = current[axis] / mNoiseParam.mUniformScale +
                    mNoiseParam.mOffsetVec[axis];
                if (!std::isfinite(position[axis])) return MS::kInvalidParameter;
            }
            const double signal = mod->GetValue(position[0], position[1], position[2]);
            const double noiseValue = signal * double(mNoiseParam.mAmplitude);
            const double distance = delta.length + noiseValue;
            if (!std::isfinite(signal) || !std::isfinite(noiseValue) || !std::isfinite(distance))
                return MS::kInvalidParameter;
            candidate[i] = distance < double(m_radius) ? m_density : 0.0f;
        }
        densityArray->swap(candidate);
        return MS::kSuccess;
    } catch (const noise::Exception&) {
        return MS::kInvalidParameter;
    }
};




//base noise
MStatus validateNoiseBaseParams(const NoiseBaseParams& params);
// Transactional: invalid input/library failures leave signals unchanged.
MStatus createNoiseFromPosition(const NoiseBaseParams &param, const DPointList &newPosition, DScalarDataList &signals);
// FastNoise is an environment-provided optional dependency.
#if DOMMY_WITH_FASTNOISE
MStatus validateFastNoiseParams(const FastNoiseParams& params);
MStatus createFastNoiseFromPosition(FastNoiseParams &param, const DPointList &newPosition, DScalarDataList &signals);
#endif

//dfnoise
MStatus validateDFNoiseParams(const NoiseBaseParams& params);
// Both divergence entry points stage their vectors until evaluation succeeds.
MStatus createDevergenceFreenoise(const noise::module::Module &mod,
                                  const NoiseBaseParams &param,
                                  const DPointList &pts,
                                  DVec3DataList &devergence);

MStatus createDFNoiseFromPosition(const NoiseBaseParams &param,
                                  const DPointList &newPosition,
                                  DVec3DataList &signals);

//pyro noise
MStatus createPyroNoiseFromPosition(const NoiseBaseParams &param, const DPoint &calcPos, const float &density, const float &radius, const DPointList &newPosition, DScalarDataList &signals);


inline MVector getGradientVector(const noise::module::Module &noiseMod,
                                 const MPoint &basePt,
                                 const double &epsilon) {
    double n0 = noiseMod.GetValue(basePt.x,basePt.y,basePt.z);
	
    MPoint NxPt(basePt+MPoint(1,0,0)*epsilon);
    MPoint NyPt(basePt+MPoint(0,1,0)*epsilon);
    MPoint NzPt(basePt+MPoint(0,0,1)*epsilon);
    double nx = noiseMod.GetValue(NxPt.x,NxPt.y,NxPt.z);
    double ny = noiseMod.GetValue(NyPt.x,NyPt.y,NyPt.z);
    double nz = noiseMod.GetValue(NzPt.x,NzPt.y,NzPt.z);
    
    return MVector(nx - n0, ny - n0, nz - n0)/epsilon;
}


// Legacy adapters retain inclusive real upper bounds and throw at compute guards.
template <typename S, typename T>
void DGenerateRandomMayaArray(int seed, S minimum, S maximum, size_t count, T& values)
{
    if (dommy::maya::generateRandomArray(seed, minimum, maximum, count, values) != MS::kSuccess)
        throw std::runtime_error("Maya random array generation failed");
}

template <typename T>
void DGenerateRandomValue(int seed, T minimum, T maximum, size_t count, std::vector<T>& values)
{
    dommy::random::generateValues(static_cast<std::uint32_t>(seed),
                                 minimum, maximum, count, values, true);
}

void DGenerateRandomValue3(int seed, const float3 min, const float3 max, size_t arraySize, MVectorArray &values);
void DGenerateRandomValue3(int seed, const double3 min, const double3 max, size_t arraySize, DVec3DataList &values);
void DGenerateRandomValue3(int seed, const float3 min, const float3 max, size_t arraySize, DVec3DataList &values);
void DGenerateRandomValue3(int seed, const double3 min, const double3 max, size_t arraySize, MVectorArray &values);
void DGenerateRandomValue3(int seed, const MVector min, const MVector max, size_t arraySize, DVec3DataList &values);
