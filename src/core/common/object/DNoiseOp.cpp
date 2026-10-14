//
//  DNoiseOp.cpp
//  dommyTool
//
//  Created by yuzo on 2016/07/13.
//
//

#include "DNoiseOp.h"
#include "noise/DWoodNoise.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"


#include <random>

#include "DDataOp.h"


#pragma mark generate method

namespace {

dommy::noise::WoodSettings makeWoodSettings(const NoiseBaseParams& params)
{
    dommy::noise::WoodSettings settings;
    settings.seed = static_cast<std::uint32_t>(params.mSeed);
    settings.warpOctaves = params.mTurbulanceRoughness;
    settings.ringFrequency = params.mWooodFrequency;
    settings.grainStretch = params.mWooodGrain;
    settings.grainAmount = params.mWooodScale;
    settings.bias = params.mWooodBias;
    settings.warpFrequency = params.mFrequency;
    settings.warpAmount = params.mTurbulancePower;
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        settings.rotationRadians[axis] = dommy::math::degreesToRadians(
            std::remainder(params.mWooodAngle[axis], 360.0));
    return settings;
}

MStatus storeDivergence(
    const MVector& source,
    const NoiseBaseParams& params,
    DVec3Data& destination)
{
    if (!dommy::maya::isFiniteVector(source)) {
        return MS::kInvalidParameter;
    }

    MVector result = source;
    if (params.mNormalize) {
        if (!dommy::maya::normalizedVectorOrZero(result, result)) {
            return MS::kInvalidParameter;
        }
    }
    if (params.mInvert) result *= -1.0;

    float values[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!dommy::numeric::checkedFloatCast(result[axis], values[axis])) {
            return MS::kInvalidParameter;
        }
    }
    destination = DVec3Data(values[0], values[1], values[2]);
    return MS::kSuccess;
}

MStatus makeNoisePosition(
    const DPoint& point,
    const MVector& scale,
    const MVector& offset,
    MPoint& result)
{
    result = MPoint(
        point[0] * scale[0] + offset[0],
        point[1] * scale[1] + offset[1],
        point[2] * scale[2] + offset[2]);
    return dommy::maya::isFinitePoint(result)
        ? MStatus(MS::kSuccess)
        : MStatus(MS::kInvalidParameter);
}

} // namespace

MStatus validateNoiseBaseParams(const NoiseBaseParams& p)
{
    if (p.mNoiseType < NOISE_TYPE_RANDOM || p.mNoiseType >= NOISE_TYPE_COUNT ||
        p.mSeed < 0 || p.mOctave < 1 || p.mOctave > noise::module::PERLIN_MAX_OCTAVE ||
        p.mTurbulanceRoughness < 1 || p.mTurbulanceRoughness > noise::module::PERLIN_MAX_OCTAVE ||
        p.mNoiseQuality < noise::QUALITY_FAST || p.mNoiseQuality > noise::QUALITY_BEST ||
        !std::isfinite(p.mAmplitude) || !std::isfinite(p.mFrequency) || p.mFrequency < 0.0f ||
        !std::isfinite(p.mLacunarity) || !std::isfinite(p.mPersistence) ||
        !std::isfinite(p.mVoronoiDisplace) || !std::isfinite(p.mTurbulancePower) ||
        !std::isfinite(p.mWooodFrequency) || !std::isfinite(p.mWooodGrain) ||
        !std::isfinite(p.mWooodScale) || !std::isfinite(p.mWooodBias) ||
        !std::isfinite(p.mUniformScale) || !dommy::maya::isFiniteVector(p.mWooodAngle) ||
        !dommy::maya::isFiniteVector(p.mOffsetVec) || !dommy::maya::isFiniteVector(p.mScaleVec))
        return MS::kInvalidParameter;
    return MS::kSuccess;
}

MStatus createNoiseFromPosition(const NoiseBaseParams &param, const DPointList &newPosition, DScalarDataList &signals) try {
    if (signals.size() != newPosition.size() ||
        newPosition.size() > dommy::generation::kMaxGeneratedPointCount ||
        !validateNoiseBaseParams(param)) return MS::kInvalidParameter;
    DScalarDataList candidate(newPosition.size(), 0.0f);
    
    
    switch (param.mNoiseType) {
        case 0://random
        {
            std::mt19937 eng(param.mSeed);
            std::uniform_real_distribution<DScalarData> distrib(-1.0, std::nextafter(1.0, std::numeric_limits<DScalarData>::max()));
            auto gen = std::bind(distrib, eng);
            generate(begin(candidate), end(candidate), gen);
            
            
        }
            break;
        case 1:
        default://perlin noise
        {
            
            noise::module::Perlin noiseMod;
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetOctaveCount(param.mOctave);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetLacunarity(param.mLacunarity);
            noiseMod.SetPersistence(param.mPersistence);
            noiseMod.SetNoiseQuality(param.mNoiseQuality);
            
            
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            NoiseFromDP body = NoiseFromDP(noiseMod,
                                           param.mOffsetVec,
                                           param.mScaleVec*param.mUniformScale,
                                           newPosition,
                                           candidate);
            tbb::parallel_for( range, body );
            
            
        }
            break;
        
        case 2://Billow
        {
            noise::module::Billow noiseMod;
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetOctaveCount(param.mOctave);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetLacunarity(param.mLacunarity);
            noiseMod.SetPersistence(param.mPersistence);
            noiseMod.SetNoiseQuality(param.mNoiseQuality);
            
            
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            NoiseFromDP body = NoiseFromDP(noiseMod,
                                           param.mOffsetVec,
                                           param.mScaleVec*param.mUniformScale,
                                           newPosition,
                                           candidate);
            tbb::parallel_for( range, body );
            
        }
            break;
        case 3://Voronoi
        {
            noise::module::Voronoi noiseMod;
            noiseMod.SetSeed (param.mSeed);
            noiseMod.SetFrequency (param.mFrequency);
            noiseMod.EnableDistance (param.mUseVoronoiDistance);
            noiseMod.SetDisplacement (param.mVoronoiDisplace);
            
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            NoiseFromDP body = NoiseFromDP(noiseMod,
                                           param.mOffsetVec,
                                           param.mScaleVec*param.mUniformScale,
                                           newPosition,
                                           candidate);
            tbb::parallel_for( range, body );
            
        }
            break;
        case 4://Turblance
        {
            noise::module::Perlin pnoise;
            pnoise.SetSeed(param.mSeed);
            pnoise.SetOctaveCount(param.mOctave);
            pnoise.SetFrequency(param.mFrequency);
            pnoise.SetLacunarity(param.mLacunarity);
            pnoise.SetPersistence(param.mPersistence);
            pnoise.SetNoiseQuality(param.mNoiseQuality);
            
            noise::module::Turbulence noiseMod;
            noiseMod.SetSourceModule (0, pnoise);
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetPower(param.mTurbulancePower);
            noiseMod.SetRoughness(param.mTurbulanceRoughness);
            
            
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            NoiseFromDP body = NoiseFromDP(noiseMod,
                                           param.mOffsetVec,
                                           param.mScaleVec*param.mUniformScale,
                                           newPosition,
                                           candidate);
            tbb::parallel_for( range, body );
            
        }
            break;
        case 5://RidgedMulti
        {
            noise::module::RidgedMulti noiseMod;
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetOctaveCount(param.mOctave);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetLacunarity(param.mLacunarity);
            noiseMod.SetNoiseQuality(param.mNoiseQuality);
            
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            NoiseFromDP body = NoiseFromDP(noiseMod,
                                           param.mOffsetVec,
                                           param.mScaleVec*param.mUniformScale,
                                           newPosition,
                                           candidate);
            tbb::parallel_for( range, body );
            
        }
            break;
        case 6://Wood
        {
            
            dommy::noise::WoodNoise finalWood(makeWoodSettings(param));
            
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            NoiseFromDP body = NoiseFromDP(finalWood,
                                           param.mOffsetVec,
                                           param.mScaleVec*param.mUniformScale,
                                           newPosition,
                                           candidate);
            tbb::parallel_for( range, body );
            
        }
            break;
        
    }
    
    
    if (param.isAbs) {
        for (auto &it :candidate) {
            it = fabs(it);
        }
    }
    
    signals.swap(candidate);
    return MS::kSuccess;
} catch (const noise::Exception&) {
    return MS::kInvalidParameter;
}



#if DOMMY_WITH_FASTNOISE
MStatus validateFastNoiseParams(const FastNoiseParams& p)
{
    if (p.mNoiseType < FAST_NOISE_VALUE || p.mNoiseType >= FAST_NOISE_TYPE_COUNT ||
        p.mInterpType < FN_INTERP_LINEAR || p.mInterpType >= FN_INTERP_TYPE_COUNT ||
        p.mFractalType < FN_FRAC_FBM || p.mFractalType >= FN_FRAC_TYPE_COUNT ||
        p.mCellDistanceType < FN_CELL_DIST_EUCLIDEAN || p.mCellDistanceType >= FN_CELL_DIST_TYPE_COUNT ||
        p.mCellReturnType < FN_CELL_RTN_CELLVALUE || p.mCellReturnType >= FN_CELL_RTN_TYPE_COUNT ||
        p.mSeed < 0 || p.mOctave < 1U || p.mOctave > 30U ||
        !std::isfinite(p.mAmplitude) || !std::isfinite(p.mFrequency) || p.mFrequency < 0.0f ||
        !std::isfinite(p.mLacunarity) || !std::isfinite(p.mFractalGain) ||
        !std::isfinite(p.mPositionWarpAmp) || !std::isfinite(p.mUniformScale) ||
        !dommy::maya::isFiniteVector(p.mOffsetVec) || !dommy::maya::isFiniteVector(p.mScaleVec))
        return MS::kInvalidParameter;
    // SetFractalGain/Octaves calculate this normalization even for non-fractal noise.
    FN_DECIMAL amplitude = 1;
    FN_DECIMAL sum = 1;
    for (unsigned int octave = 1U; octave < p.mOctave; ++octave) {
        amplitude *= p.mFractalGain;
        sum += amplitude;
        if (!std::isfinite(amplitude) || !std::isfinite(sum)) return MS::kInvalidParameter;
    }
    if (sum == 0 || !std::isfinite(FN_DECIMAL(1) / sum)) return MS::kInvalidParameter;
    return MS::kSuccess;
}

bool makeFastNoisePosition(const FastNoiseParams& p, const DPoint& point,
                           FN_DECIMAL (&position)[3])
{
    const bool fractal = p.mNoiseType == FAST_NOISE_VALUEFRACTAL ||
        p.mNoiseType == FAST_NOISE_PERLINFRACTAL || p.mNoiseType == FAST_NOISE_SIMPLEXFRACTAL ||
        p.mNoiseType == FAST_NOISE_CUBICFRACTAL;
    // The backend uses int lattice indices (including i+j+k for simplex).
    const FN_DECIMAL limit = static_cast<FN_DECIMAL>(std::numeric_limits<int>::max() / 16);
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        const double transformed = (double(point[axis]) - p.mOffsetVec[axis]) *
            (p.mScaleVec[axis] * p.mUniformScale);
        if (!std::isfinite(transformed) ||
            std::abs(transformed) > std::numeric_limits<FN_DECIMAL>::max()) return false;
        position[axis] = static_cast<FN_DECIMAL>(transformed);
        FN_DECIMAL scaled = position[axis] * p.mFrequency;
        const unsigned int octaves = fractal ? p.mOctave : 1U;
        for (unsigned int octave = 0U; octave < octaves; ++octave) {
            if (!std::isfinite(scaled) || std::abs(scaled) > limit) return false;
            if (octave + 1U < octaves) scaled *= p.mLacunarity;
        }
    }
    return true;
}

namespace {
MStatus configureFastNoise(FastNoiseParams& parms)
{
    if (!validateFastNoiseParams(parms)) return MS::kInvalidParameter;
    parms.mFNoise.SetSeed(parms.mSeed);
    
    switch (parms.mNoiseType) {
        case FAST_NOISE_VALUE:
            parms.mFNoise.SetNoiseType(FastNoise::Value);
            break;
        case FAST_NOISE_VALUEFRACTAL:
            parms.mFNoise.SetNoiseType(FastNoise::ValueFractal);
            break;
        case FAST_NOISE_PERLIN:
            parms.mFNoise.SetNoiseType(FastNoise::Perlin);
            break;
        case FAST_NOISE_PERLINFRACTAL:
            parms.mFNoise.SetNoiseType(FastNoise::PerlinFractal);
            break;
        case FAST_NOISE_SIMPLEX:
            parms.mFNoise.SetNoiseType(FastNoise::Simplex);
            break;
        case FAST_NOISE_SIMPLEXFRACTAL:
            parms.mFNoise.SetNoiseType(FastNoise::SimplexFractal);
            break;
        case FAST_NOISE_CELLULAR:
            parms.mFNoise.SetNoiseType(FastNoise::Cellular);
            break;
        case FAST_NOISE_WHITENOISE:
            parms.mFNoise.SetNoiseType(FastNoise::WhiteNoise);
            break;
        case FAST_NOISE_CUBIC:
            parms.mFNoise.SetNoiseType(FastNoise::Cubic);
            break;
        case FAST_NOISE_CUBICFRACTAL:
            parms.mFNoise.SetNoiseType(FastNoise::CubicFractal);
            break;
        default:
            break;
    }
    
    switch (parms.mInterpType) {
        case FN_INTERP_LINEAR:
            parms.mFNoise.SetInterp(FastNoise::Linear);
            break;
        case FN_INTERP_HERMITE:
            parms.mFNoise.SetInterp(FastNoise::Hermite);
            break;
        case FN_INTERP_QUINTIC:
            parms.mFNoise.SetInterp(FastNoise::Quintic);
            break;
        default:
            break;
    }
    
    parms.mFNoise.SetFrequency(parms.mFrequency);
    // Avoid transient overflowing normalization with a previously configured gain.
    parms.mFNoise.SetFractalOctaves(1U);
    parms.mFNoise.SetFractalLacunarity(parms.mLacunarity);
    parms.mFNoise.SetFractalGain(parms.mFractalGain);
    parms.mFNoise.SetFractalOctaves(parms.mOctave);
    
    switch (parms.mFractalType) {
        case FN_FRAC_FBM:
            parms.mFNoise.SetFractalType(FastNoise::FBM);
            break;
        case FN_FRAC_BILLOW:
            parms.mFNoise.SetFractalType(FastNoise::Billow);
            break;
        case FN_FRAC_RIGIDMULTI:
            parms.mFNoise.SetFractalType(FastNoise::RigidMulti);
            break;
        default:
            break;
    }
    
    
    if (parms.mNoiseType == FAST_NOISE_CELLULAR) {
        
        switch (parms.mCellDistanceType) {
            case FN_CELL_DIST_EUCLIDEAN:
                parms.mFNoise.SetCellularDistanceFunction(FastNoise::Euclidean);
                break;
            case FN_CELL_DIST_MANHATTAN:
                parms.mFNoise.SetCellularDistanceFunction(FastNoise::Manhattan);
                break;
            case FN_CELL_DIST_NATURAL:
                parms.mFNoise.SetCellularDistanceFunction(FastNoise::Natural);
                break;
            default:
                break;
        }
        
        switch (parms.mCellReturnType) {
            case FN_CELL_RTN_CELLVALUE:
                parms.mFNoise.SetCellularReturnType(FastNoise::CellValue);
                break;
            case FN_CELL_RTN_NOISELOOKUP:
                // Preserve the legacy fallback: no lookup module is configured.
                parms.mFNoise.SetCellularReturnType(FastNoise::CellValue);
                break;
            case FN_CELL_RTN_DISTANCE:
                parms.mFNoise.SetCellularReturnType(FastNoise::Distance);
                break;
            case FN_CELL_RTN_DISTANCE2:
                parms.mFNoise.SetCellularReturnType(FastNoise::Distance2);
                break;
            case FN_CELL_RTN_DISTANCE2ADD:
                parms.mFNoise.SetCellularReturnType(FastNoise::Distance2Add);
                break;
            case FN_CELL_RTN_DISTANCE2SUB:
                parms.mFNoise.SetCellularReturnType(FastNoise::Distance2Sub);
                break;
            case FN_CELL_RTN_DISTANCE2MUL:
                parms.mFNoise.SetCellularReturnType(FastNoise::Distance2Mul);
                break;
            case FN_CELL_RTN_DISTANCE2DIV:
                parms.mFNoise.SetCellularReturnType(FastNoise::Distance2Div);
                break;
            default:
                break;
        }
    }
    
    
    parms.mFNoise.SetGradientPerturbAmp(parms.mPositionWarpAmp);
    return MS::kSuccess;
}
} // namespace

MStatus createFastNoiseFromPosition(FastNoiseParams& param, const DPointList& newPosition,
                                    DScalarDataList& signals) try
{
    if (!validateFastNoiseParams(param) || signals.size() != newPosition.size() ||
        newPosition.size() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;
    // Validate before backend setup/evaluation or allocation of a second output buffer.
    for (const DPoint& point : newPosition) {
        FN_DECIMAL position[3];
        if (!makeFastNoisePosition(param, point, position)) return MS::kInvalidParameter;
    }
    const MStatus status = configureFastNoise(param);
    if (!status) return status;
    DScalarDataList candidate(newPosition.size(), 0.0f);
    const tbb::blocked_range<std::size_t> range(0U, newPosition.size(), 10000U);
    tbb::parallel_for(range, FastNoiseFromDP(param, newPosition, candidate));
    signals.swap(candidate);
    return MS::kSuccess;
} catch (const noise::Exception&) {
    return MS::kInvalidParameter;
}
#endif

MStatus validateDFNoiseParams(const NoiseBaseParams& params)
{
    if (!validateNoiseBaseParams(params) || params.mNoiseType == NOISE_TYPE_RANDOM ||
        params.normalType < 0 || params.normalType > 2 ||
        !std::isfinite(params.epsilon) ||
        params.epsilon <= std::numeric_limits<double>::epsilon() ||
        !dommy::maya::isFiniteVector(params.normalVecBase)) return MS::kInvalidParameter;
    return MS::kSuccess;
}

MStatus createDFNoiseFromPosition(
    const NoiseBaseParams &param,
    const DPointList &newPosition,
    DVec3DataList &signals) try {

    if (!validateDFNoiseParams(param) || signals.size() != newPosition.size() ||
        newPosition.size() > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }
    
    
    switch (param.mNoiseType) {
        case NOISE_TYPE_PERLIN://perlin noise
        {
            noise::module::Perlin pnoise;
            pnoise.SetSeed(param.mSeed);
            pnoise.SetOctaveCount(param.mOctave);
            pnoise.SetFrequency(param.mFrequency);
            pnoise.SetLacunarity(param.mLacunarity);
            pnoise.SetPersistence(param.mPersistence);
            pnoise.SetNoiseQuality(param.mNoiseQuality);
            
            
            return createDevergenceFreenoise(
                pnoise, param, newPosition, signals);
        }
            break;
        case NOISE_TYPE_BILLOW://Billow
        {
            noise::module::Billow noiseMod;
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetOctaveCount(param.mOctave);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetLacunarity(param.mLacunarity);
            noiseMod.SetPersistence(param.mPersistence);
            noiseMod.SetNoiseQuality(param.mNoiseQuality);
            
            
            return createDevergenceFreenoise(
                noiseMod, param, newPosition, signals);
            
        }
            break;
        case NOISE_TYPE_VORONOI://Voronoi
        {
            noise::module::Voronoi noiseMod;
            noiseMod.SetSeed (param.mSeed);
            noiseMod.SetFrequency (param.mFrequency);
            noiseMod.EnableDistance (param.mUseVoronoiDistance);
            noiseMod.SetDisplacement (param.mVoronoiDisplace);
            
            return createDevergenceFreenoise(
                noiseMod, param, newPosition, signals);
        }
            break;
        case NOISE_TYPE_TURBULANCE://Turblance
        {
            noise::module::Perlin pnoise;
            pnoise.SetSeed(param.mSeed);
            pnoise.SetOctaveCount(param.mOctave);
            pnoise.SetFrequency(param.mFrequency);
            pnoise.SetLacunarity(param.mLacunarity);
            pnoise.SetPersistence(param.mPersistence);
            pnoise.SetNoiseQuality(param.mNoiseQuality);
            
            noise::module::Turbulence noiseMod;
            noiseMod.SetSourceModule (0, pnoise);
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetPower(param.mTurbulancePower);
            noiseMod.SetRoughness(param.mTurbulanceRoughness);
            
            return createDevergenceFreenoise(
                noiseMod, param, newPosition, signals);
        }
            break;
        case NOISE_TYPE_RIGDEDMULTI://RidgedMulti
        {
            noise::module::RidgedMulti noiseMod;
            noiseMod.SetSeed(param.mSeed);
            noiseMod.SetOctaveCount(param.mOctave);
            noiseMod.SetFrequency(param.mFrequency);
            noiseMod.SetLacunarity(param.mLacunarity);
            noiseMod.SetNoiseQuality(param.mNoiseQuality);
            
            return createDevergenceFreenoise(
                noiseMod, param, newPosition, signals);
        }
            break;
        case NOISE_TYPE_WOOD://Wood
        {
            
            dommy::noise::WoodNoise finalWood(makeWoodSettings(param));
            
            return createDevergenceFreenoise(
                finalWood, param, newPosition, signals);
        }
            break;
        default:
            return MS::kInvalidParameter;
            
    }
    return MS::kFailure;
} catch (const noise::Exception&) {
    return MS::kInvalidParameter;
}



MStatus createDevergenceFreenoise(
    const noise::module::Module &mod,
    const NoiseBaseParams &param,
    const DPointList &pts,
    DVec3DataList &devergence) try
{
    if (!validateDFNoiseParams(param) || devergence.size() != pts.size() ||
        pts.size() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;

    const MVector offsetValue = param.mOffsetVec;
    const MVector scaleValue = param.mScaleVec * param.mUniformScale;
    if (!dommy::maya::isFiniteVector(scaleValue)) {
        return MS::kInvalidParameter;
    }

    MVector noiseOffset;
    if (!dommy::maya::normalizedVector(
            MVector(7.77, 3.33, 1.11), noiseOffset)) {
        return MS::kFailure;
    }

    std::vector<MVector> referenceNormals;
    if (param.normalType == 2) {
        if (param.m_referenceObject.isNull() ||
            (param.m_referenceObjectType != MFnData::kNurbsCurve &&
             param.m_referenceObjectType != MFnData::kNurbsSurface &&
             param.m_referenceObjectType != MFnData::kMesh)) {
            return MS::kInvalidParameter;
        }

        referenceNormals.assign(pts.size(), MVector(0.0, 1.0, 0.0));
        MStatus status;
        if (param.m_referenceObjectType == MFnData::kNurbsCurve) {
            MFnNurbsCurve curve(param.m_referenceObject, &status);
            if (!status) return status;

            for (std::size_t index = 0U; index < pts.size(); ++index) {
                const MPoint point(pts[index][0], pts[index][1], pts[index][2]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                double parameter = 0.0;
                const MPoint closestPoint = curve.closestPoint(
                    point,
                    &parameter,
                    kMFnNurbsEpsilon,
                    MSpace::kWorld,
                    &status);
                if (!status ||
                    !std::isfinite(parameter) ||
                    !dommy::maya::isFinitePoint(closestPoint)) {
                    return status
                        ? MStatus(MS::kInvalidParameter) : status;
                }
                referenceNormals[index] =
                    curve.normal(parameter, MSpace::kWorld, &status);
                if (!status ||
                    !dommy::maya::isFiniteVector(referenceNormals[index])) {
                    return status
                        ? MStatus(MS::kInvalidParameter) : status;
                }
            }
        } else if (
            param.m_referenceObjectType == MFnData::kNurbsSurface) {
            MFnNurbsSurface surface(param.m_referenceObject, &status);
            if (!status) return status;

            for (std::size_t index = 0U; index < pts.size(); ++index) {
                const MPoint point(pts[index][0], pts[index][1], pts[index][2]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                double parameterU = 0.0;
                double parameterV = 0.0;
                const MPoint closestPoint = surface.closestPoint(
                    point,
                    &parameterU,
                    &parameterV,
                    false,
                    kMFnNurbsEpsilon,
                    MSpace::kWorld,
                    &status);
                if (!status ||
                    !std::isfinite(parameterU) ||
                    !std::isfinite(parameterV) ||
                    !dommy::maya::isFinitePoint(closestPoint)) {
                    return status
                        ? MStatus(MS::kInvalidParameter) : status;
                }
                referenceNormals[index] = surface.normal(
                    parameterU,
                    parameterV,
                    MSpace::kWorld,
                    &status);
                if (!status ||
                    !dommy::maya::isFiniteVector(referenceNormals[index])) {
                    return status
                        ? MStatus(MS::kInvalidParameter) : status;
                }
            }
        } else if (param.m_referenceObjectType == MFnData::kMesh) {
            MFnMesh mesh(param.m_referenceObject, &status);
            if (!status) return status;

            for (std::size_t index = 0U; index < pts.size(); ++index) {
                const MPoint point(pts[index][0], pts[index][1], pts[index][2]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                int closestPolygon = -1;
                status = mesh.getClosestNormal(
                    point,
                    referenceNormals[index],
                    MSpace::kWorld,
                    &closestPolygon,
                    nullptr);
                if (!status ||
                    closestPolygon < 0 ||
                    !dommy::maya::isFiniteVector(referenceNormals[index])) {
                    return status
                        ? MStatus(MS::kInvalidParameter) : status;
                }
            }
        } else {
            return MS::kInvalidParameter;
        }
    }

    DVec3DataList candidate(pts.size(), DVec3Data(0.0f, 0.0f, 0.0f));
    for (std::size_t index = 0U; index < pts.size(); ++index) {
        MPoint basePoint;
        MStatus status = makeNoisePosition(
            pts[index], scaleValue, offsetValue, basePoint);
        if (!status) return status;

        const MVector baseVector =
            getGradientVector(mod, basePoint, param.epsilon);
        MVector normalVector;
        if (param.normalType == 0) {
            normalVector = getGradientVector(
                mod, basePoint + noiseOffset, param.epsilon);
        } else if (param.normalType == 1) {
            normalVector = param.normalVecBase;
        } else {
            normalVector = referenceNormals[index];
        }

        status = storeDivergence(
            baseVector ^ normalVector, param, candidate[index]);
        if (!status) return status;
    }

    devergence.swap(candidate);
    return MS::kSuccess;
} catch (const noise::Exception&) {
    return MS::kInvalidParameter;
}



MStatus validatePyroNoiseParams(const NoiseBaseParams& p)
{
    if (!validateNoiseBaseParams(p) || p.mNoiseType == NOISE_TYPE_RANDOM ||
        p.mUniformScale <= std::numeric_limits<float>::epsilon() ||
        !std::isfinite(p.mTopRatio) || !std::isfinite(p.mUnderRatio) || !std::isfinite(p.mSideRatio))
        return MS::kInvalidParameter;
    return MS::kSuccess;
}

MStatus createPyroNoiseFromPosition(const NoiseBaseParams &param, const DPoint &calcPos, const float &density, const float &radius, const DPointList &newPosition, DScalarDataList &signals) try {
    if (!validatePyroNoiseParams(param) || !std::isfinite(density) || !std::isfinite(radius) ||
        radius < 0.0f || signals.size() != newPosition.size() ||
        newPosition.size() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!std::isfinite(calcPos[axis])) return MS::kInvalidParameter;
    DScalarDataList candidate(newPosition.size(), 0.0f);
	
	
	switch (param.mNoiseType) {
		case NOISE_TYPE_PERLIN:
		default://perlin noise
		{
			
			noise::module::Perlin noiseMod;
			noiseMod.SetSeed(param.mSeed);
			noiseMod.SetOctaveCount(param.mOctave);
			noiseMod.SetFrequency(param.mFrequency);
			noiseMod.SetLacunarity(param.mLacunarity);
			noiseMod.SetPersistence(param.mPersistence);
			noiseMod.SetNoiseQuality(param.mNoiseQuality);
			
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            PyroNoiseFromPosition body = PyroNoiseFromPosition(noiseMod,
                                                               calcPos,
                                                               param,
                                                               radius,
                                                               density,
                                                               newPosition,
                                                               &candidate);
			tbb::parallel_for( range, body );
			
		}
			break;
			
		case NOISE_TYPE_BILLOW://Billow
		{
			noise::module::Billow noiseMod;
			noiseMod.SetSeed(param.mSeed);
			noiseMod.SetOctaveCount(param.mOctave);
			noiseMod.SetFrequency(param.mFrequency);
			noiseMod.SetLacunarity(param.mLacunarity);
			noiseMod.SetPersistence(param.mPersistence);
			noiseMod.SetNoiseQuality(param.mNoiseQuality);
			
			
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            PyroNoiseFromPosition body = PyroNoiseFromPosition(noiseMod,
                                                               calcPos,
                                                               param,
                                                               radius,
                                                               density,
                                                               newPosition,
                                                               &candidate);
			tbb::parallel_for( range, body );
			
		}
			break;
		case NOISE_TYPE_VORONOI://Voronoi
		{
			noise::module::Voronoi noiseMod;
			noiseMod.SetSeed (param.mSeed);
			noiseMod.SetFrequency (param.mFrequency);
			noiseMod.EnableDistance (param.mUseVoronoiDistance);
			noiseMod.SetDisplacement (param.mVoronoiDisplace);
			
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            PyroNoiseFromPosition body = PyroNoiseFromPosition(noiseMod,
                                                               calcPos,
                                                               param,
                                                               radius,
                                                               density,
                                                               newPosition,
                                                               &candidate);
			tbb::parallel_for( range, body );
			
		}
			break;
		case NOISE_TYPE_TURBULANCE://Turblance
		{
			noise::module::Perlin pnoise;
			pnoise.SetSeed(param.mSeed);
			pnoise.SetOctaveCount(param.mOctave);
			pnoise.SetFrequency(param.mFrequency);
			pnoise.SetLacunarity(param.mLacunarity);
			pnoise.SetPersistence(param.mPersistence);
			pnoise.SetNoiseQuality(param.mNoiseQuality);
			
			noise::module::Turbulence noiseMod;
			noiseMod.SetSourceModule (0, pnoise);
			noiseMod.SetSeed(param.mSeed);
			noiseMod.SetFrequency(param.mFrequency);
			noiseMod.SetPower(param.mTurbulancePower);
			noiseMod.SetRoughness(param.mTurbulanceRoughness);
			
			
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            PyroNoiseFromPosition body = PyroNoiseFromPosition(noiseMod,
                                                               calcPos,
                                                               param,
                                                               radius,
                                                               density,
                                                               newPosition,
                                                               &candidate);
			tbb::parallel_for( range, body );
			
		}
			break;
		case NOISE_TYPE_RIGDEDMULTI://RidgedMulti
		{
			noise::module::RidgedMulti noiseMod;
			noiseMod.SetSeed(param.mSeed);
			noiseMod.SetOctaveCount(param.mOctave);
			noiseMod.SetFrequency(param.mFrequency);
			noiseMod.SetLacunarity(param.mLacunarity);
			noiseMod.SetNoiseQuality(param.mNoiseQuality);
			
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            PyroNoiseFromPosition body = PyroNoiseFromPosition(noiseMod,
                                                               calcPos,
                                                               param,
                                                               radius,
                                                               density,
                                                               newPosition,
                                                               &candidate);
			tbb::parallel_for( range, body );
			
		}
			break;
		case NOISE_TYPE_WOOD://Wood
		{
			
			dommy::noise::WoodNoise finalWood(makeWoodSettings(param));
			
            tbb::blocked_range<std::size_t> range = tbb::blocked_range<std::size_t>(0, newPosition.size(), 10000);
            PyroNoiseFromPosition body = PyroNoiseFromPosition(finalWood,
                                                               calcPos,
                                                               param,
                                                               radius,
                                                               density,
                                                               newPosition,
                                                               &candidate);
			tbb::parallel_for( range, body );
			
		}
			break;
			
	}
	
    signals.swap(candidate);
    return MS::kSuccess;
} catch (const noise::Exception&) {
    return MS::kInvalidParameter;
}



namespace {

template <typename Scalar, typename Bounds>
void generateRandomMayaVectors(int seed, const Bounds& minimum, const Bounds& maximum,
                               std::size_t count, MVectorArray& values)
{
    std::vector<MVector> generated;
    dommy::random::generateRealTriples<Scalar>(seed, minimum, maximum, count, generated);
    const MStatus status = dommy::maya::buildCheckedArray(count,
        [&](unsigned int i, MVector& value) -> MStatus {
            value = generated[i];
            return MS::kSuccess;
        }, values);
    if (status != MS::kSuccess)
        throw std::runtime_error("Maya random vector array generation failed");
}

} // namespace

void DGenerateRandomValue3(int seed, const float3 minimum, const float3 maximum,
                           size_t count, MVectorArray& values)
{
    generateRandomMayaVectors<float>(seed, minimum, maximum, count, values);
}

void DGenerateRandomValue3(int seed, const double3 minimum, const double3 maximum,
                           size_t count, MVectorArray& values)
{
    generateRandomMayaVectors<double>(seed, minimum, maximum, count, values);
}

void DGenerateRandomValue3(int seed, const float3 minimum, const float3 maximum,
                           size_t count, DVec3DataList& values)
{
    dommy::random::generateRealTriples<float>(seed, minimum, maximum, count, values);
}

void DGenerateRandomValue3(int seed, const double3 minimum, const double3 maximum,
                           size_t count, DVec3DataList& values)
{
    dommy::random::generateRealTriples<float>(seed, minimum, maximum, count, values);
}

void DGenerateRandomValue3(int seed, const MVector minimum, const MVector maximum,
                           size_t count, DVec3DataList& values)
{
    dommy::random::generateRealTriples<float>(seed, minimum, maximum, count, values);
}
