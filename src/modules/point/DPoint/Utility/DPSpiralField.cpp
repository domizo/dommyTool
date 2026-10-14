//
//  DPSpiralField.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/03.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSpiralField.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DPointForceBudget.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"




#include "DNodeOperation.h"
#include "DDataOp.h"


#include <tbb/blocked_range.h>

#include <algorithm>
#include <cmath>



#pragma mark Utilities Spiral

namespace {
    
    
    //ポジション毎の情報データを格納
    struct SpiralCacheData
    {
        SpiralCacheData()
        : m_lookVector(MVector::zero)
        , m_twistVector(MVector::zero)
        , m_tangent(MVector(0.0,0.0,0.0))
        , m_distance(0.0f)
        , m_rampPosition(0.0f)
        { }
        
        MVector m_lookVector;
        MVector m_twistVector;
        MVector m_tangent;
        float m_distance;
        float m_rampPosition;
    };
    
    
    
    
    
    
    class SpiralCurveOp
    {
    public:
        
        SpiralCurveOp(
            const SpiralParams& param,
            const DPointList& pts,
            std::vector<SpiralCacheData>& data)
        : mParam(&param)
        , m_pts(&pts)
        , m_data(&data)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsCurve curve(mParam->m_curveObject, &status);
            if (!status) return status;

            const double length =
                curve.length(kMFnNurbsEpsilon, &status);
            if (!status || !std::isfinite(length) ||
                length <= kMFnNurbsEpsilon) {
                return status ? MStatus(MS::kInvalidParameter) : status;
            }
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }

                double parameter = 0.0;
                const MPoint closestPoint = curve.closestPoint(
                    point, &parameter, kMFnNurbsEpsilon,
                    MSpace::kWorld, &status);
                if (!status ||
                    !dommy::maya::isFinitePoint(closestPoint) ||
                    !std::isfinite(parameter)) {
                    return status ? MStatus(MS::kFailure) : status;
                }

                SpiralCacheData& cache = (*m_data)[i];
                cache.m_lookVector = closestPoint - point;
                if (!dommy::maya::isFloatRepresentableVector(
                        cache.m_lookVector) ||
                    !dommy::numeric::checkedFloatCast(
                        point.distanceTo(closestPoint),
                        cache.m_distance)) {
                    return MS::kFailure;
                }

                status = curve.getParamAtPoint(
                    closestPoint, parameter,
                    kMFnNurbsEpsilon, MSpace::kWorld);
                if (!status) return status;
                if (!std::isfinite(parameter)) return MS::kFailure;

                cache.m_tangent = curve.tangent(
                    parameter, MSpace::kWorld, &status);
                if (!status ||
                    !dommy::maya::isFloatRepresentableVector(
                        cache.m_tangent)) {
                    return status ? MStatus(MS::kFailure) : status;
                }

                cache.m_twistVector =
                    -(cache.m_lookVector ^ cache.m_tangent);
                if (!dommy::maya::isFloatRepresentableVector(
                        cache.m_twistVector)) {
                    return MS::kFailure;
                }

                const double currentLength =
                    curve.findLengthFromParam(parameter, &status);
                if (!status || !std::isfinite(currentLength) ||
                    !dommy::numeric::clampUnitToFloat(
                        currentLength / length,
                        cache.m_rampPosition)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
            }
            return MS::kSuccess;
        }
        
    private:
        const SpiralParams *mParam;
        const DPointList *m_pts;
        
        std::vector<SpiralCacheData> *m_data;
    };//SpiralCurveOp
    
    bool interpolateRange(float minimum,
                          float maximum,
                          float position,
                          float& result)
    {
        if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
            !std::isfinite(position)) {
            return false;
        }
        const double candidate =
            static_cast<double>(minimum) +
            (static_cast<double>(maximum) -
             static_cast<double>(minimum)) *
                static_cast<double>(position);
        return dommy::numeric::checkedFloatCast(candidate, result);
    }
    
    class SpiralCaclOp
    {
    public:
        
        SpiralCaclOp(SpiralParams &param, const std::vector<SpiralCacheData>& datas, DVec3DataList &vecs)
        : mParam(&param)
        , m_data(&datas)
        , m_vec(&vecs)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            for (size_t i = r.begin(); i != r.end(); i++) {
                const SpiralCacheData& cache = (*m_data)[i];
                if ( mParam->m_is_max_dist &&
                    (mParam->m_max_distance < cache.m_distance)) {
                    continue;
                }

                MVector vector;
                if (!dommy::maya::normalizedVectorOrZero(
                        cache.m_twistVector, vector)) {
                    return MS::kFailure;
                }

                float radiusRampValue = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(
                        mParam->m_radius_ramp,
                        cache.m_rampPosition,
                        radiusRampValue)) {
                    return MS::kFailure;
                }
                float radius = 0.0f;
                if (!interpolateRange(
                        mParam->m_rad_min, mParam->m_rad_max,
                        radiusRampValue, radius) ||
                    radius < 0.0f) {
                    return MS::kInvalidParameter;
                }

                float distancePosition = 0.0f;
                if (radius <= static_cast<float>(kMFnNurbsEpsilon)) {
                    distancePosition =
                        cache.m_distance <=
                                static_cast<float>(kMFnNurbsEpsilon)
                            ? 0.0f
                            : 1.0f;
                } else if (!dommy::numeric::clampUnitToFloat(
                               static_cast<double>(cache.m_distance) /
                                   static_cast<double>(radius),
                               distancePosition)) {
                    return MS::kFailure;
                }

                float centerRampValue = 0.0f;
                float centerDistanceRampValue = 0.0f;
                float upRampValue = 0.0f;
                float upDistanceRampValue = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(
                        mParam->m_center_ramp,
                        cache.m_rampPosition,
                        centerRampValue) ||
                    !dommy::maya::sampleFiniteRamp(
                        mParam->m_center_dist_ramp,
                        distancePosition,
                        centerDistanceRampValue) ||
                    !dommy::maya::sampleFiniteRamp(
                        mParam->m_up_ramp,
                        cache.m_rampPosition,
                        upRampValue) ||
                    !dommy::maya::sampleFiniteRamp(
                        mParam->m_up_dist_ramp,
                        distancePosition,
                        upDistanceRampValue)) {
                    return MS::kFailure;
                }

                float centerScale = 0.0f;
                float centerDistanceScale = 0.0f;
                float upScale = 0.0f;
                float upDistanceScale = 0.0f;
                if (!interpolateRange(
                        mParam->m_center_min, mParam->m_center_max,
                        centerRampValue, centerScale) ||
                    !interpolateRange(
                        mParam->m_center_dist_min,
                        mParam->m_center_dist_max,
                        centerDistanceRampValue,
                        centerDistanceScale) ||
                    !interpolateRange(
                        mParam->m_up_min, mParam->m_up_max,
                        upRampValue, upScale) ||
                    !interpolateRange(
                        mParam->m_up_dist_min,
                        mParam->m_up_dist_max,
                        upDistanceRampValue,
                        upDistanceScale)) {
                    return MS::kFailure;
                }

                vector += cache.m_lookVector *
                    (static_cast<double>(centerScale) *
                     static_cast<double>(centerDistanceScale));
                vector += cache.m_tangent *
                    (static_cast<double>(upScale) *
                     static_cast<double>(upDistanceScale));
                if (!dommy::maya::isFloatRepresentableVector(vector) ||
                    !dommy::direction::adjust(
                        vector, mParam->m_invert,
                        mParam->m_normalize,
                        mParam->m_magnitude,
                        (*m_vec)[i])) {
                    return MS::kFailure;
                }
            }
            return MS::kSuccess;
        }
        
        
        
    private:
        SpiralParams *mParam;
//        DPointList *m_pts;
        const std::vector<SpiralCacheData> *m_data;
        DVec3DataList *m_vec;
    };//SpiralCaclOp
    
    
    
    
    bool validSpiralBudget(std::size_t points, std::size_t partitions)
    {
        using namespace dommy::generation;
        const std::size_t maximum = 256U * 1024U * 1024U / 4U;
        std::size_t bytes = 0U, partitionBytes = 0U;
        // Count native curve queries and five ramp samples per point. This
        // bounds query visits, not Maya's internal closest-point complexity.
        return dommy::point::validPointForceBudget(points, partitions, 1U, 10U) &&
            checkedProduct(points, sizeof(SpiralCacheData) + sizeof(DVec3Data), maximum, bytes) &&
            checkedProduct(partitions, sizeof(DVec3DataList) + sizeof(void*), maximum, partitionBytes) &&
            checkedSum(bytes, partitionBytes, maximum, bytes);
    }

    class SpiralGen
    {
    public:
        SpiralGen(SpiralParams& params,
                  const std::vector<const DPointList*>& pts,
                  std::vector<DVec3DataList>& vectors)
        : mParam(&params)
        , m_pts(&pts)
        , m_vec(&vectors)
        {}
        
        MStatus operator()() const
        {
            if (!m_pts || !m_vec ||
                m_pts->size() != m_vec->size()) {
                return MS::kInvalidParameter;
            }

            std::size_t count = 0U;
            for (const DPointList* partition : *m_pts) {
                if (!partition || !dommy::generation::checkedSum(
                        count, partition->size(),
                        dommy::generation::kMaxGeneratedPointCount, count))
                    return MS::kInvalidParameter;
            }
            if (!validSpiralBudget(count, m_pts->size()))
                return MS::kInvalidParameter;

            for (size_t n = 0U; n < m_pts->size(); ++n) {
                if (!(*m_pts)[n] ||
                    (*m_vec)[n].size() != (*m_pts)[n]->size()) {
                    return MS::kInvalidParameter;
                }
                
                //create position info from curve
                std::vector<SpiralCacheData> datalist((*m_pts)[n]->size(), SpiralCacheData());
                
                SpiralCurveOp crvop(*mParam, *(*m_pts)[n], datalist);
                
                tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 100);
				MStatus status = crvop(range);
                if (!status) return status;
                
                
                //create vector data
                
                
                SpiralCaclOp calop(*mParam, datalist, (*m_vec)[n]);
                
                tbb::blocked_range<size_t> range2 = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 100);
				status = calop(range2);
                if (!status) return status;
                
            }
            return MS::kSuccess;
        }
        
        SpiralParams *mParam;
        const std::vector<const DPointList* > *m_pts;
        std::vector<DVec3DataList > *m_vec;
    };

    MStatus readSortedRange(MDataBlock& data,
                            const MObject& attribute,
                            float& minimum,
                            float& maximum)
    {
        MStatus status;
        MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) return status;
        const float2& range = handle.asFloat2();
        if (!std::isfinite(range[0]) || !std::isfinite(range[1])) {
            return MS::kInvalidParameter;
        }
        minimum = std::min(range[0], range[1]);
        maximum = std::max(range[0], range[1]);
        return MS::kSuccess;
    }

    MStatus assignCurveRamp(const MObject& node,
                            const MObject& attribute,
                            MRampAttribute& ramp)
    {
        MStatus status;
        MRampAttribute candidate(node, attribute, &status);
        if (!status) return status;
        if (!candidate.isCurveRamp(&status) || !status) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        ramp = candidate;
        return MS::kSuccess;
    }
}







const MTypeId DPSpiralField::typeId( ID_DP_SPIRAL_FIELD );

MObject DPSpiralField::inDP;

MObject DPSpiralField::inObject;


MObject DPSpiralField::aRadiusRamp;
MObject DPSpiralField::aRadiusRange;




MObject DPSpiralField::aIsMaxDistance;
MObject DPSpiralField::aMaxDistance;


MObject DPSpiralField::aCenterStreamRamp;
MObject DPSpiralField::aCenterStreamRange;
MObject DPSpiralField::aCenterDistanceRamp;
MObject DPSpiralField::aCenterDistanceRange;

MObject DPSpiralField::aUpStraemRamp;
MObject DPSpiralField::aUpStraemRange;
MObject DPSpiralField::aUpDistanceRamp;
MObject DPSpiralField::aUpDistanceRange;

MObject DPSpiralField::aAttributeName;

MObject DPSpiralField::aInvert;
MObject DPSpiralField::aNormalize;
MObject DPSpiralField::aMagnitude;

MObject DPSpiralField::outDP;


void *DPSpiralField::creator() {
    return ::dommy::maya::createObject("DPSpiralField", [&]() {
        return new DPSpiralField();
    });
}

MStatus DPSpiralField::evalParms(MDataBlock& data, SpiralParams& parms) {
    const MObject node = thisMObject();
    MStatus status = assignCurveRamp(
        node, aRadiusRamp, parms.m_radius_ramp);
    if (!status) return status;
    status = assignCurveRamp(
        node, aCenterStreamRamp, parms.m_center_ramp);
    if (!status) return status;
    status = assignCurveRamp(
        node, aCenterDistanceRamp, parms.m_center_dist_ramp);
    if (!status) return status;
    status = assignCurveRamp(
        node, aUpStraemRamp, parms.m_up_ramp);
    if (!status) return status;
    status = assignCurveRamp(
        node, aUpDistanceRamp, parms.m_up_dist_ramp);
    if (!status) return status;

    status = readSortedRange(
        data, aRadiusRange,
        parms.m_rad_min, parms.m_rad_max);
    if (!status) return status;
    status = readSortedRange(
        data, aCenterStreamRange,
        parms.m_center_min, parms.m_center_max);
    if (!status) return status;
    status = readSortedRange(
        data, aCenterDistanceRange,
        parms.m_center_dist_min, parms.m_center_dist_max);
    if (!status) return status;
    status = readSortedRange(
        data, aUpStraemRange,
        parms.m_up_min, parms.m_up_max);
    if (!status) return status;
    status = readSortedRange(
        data, aUpDistanceRange,
        parms.m_up_dist_min, parms.m_up_dist_max);
    if (!status) return status;

    MDataHandle handle = data.inputValue(aIsMaxDistance, &status);
    if (!status) return status;
    parms.m_is_max_dist = handle.asBool();

    handle = data.inputValue(aMaxDistance, &status);
    if (!status) return status;
    parms.m_max_distance = handle.asFloat();

    handle = data.inputValue(aInvert, &status);
    if (!status) return status;
    parms.m_invert = handle.asBool();

    handle = data.inputValue(aNormalize, &status);
    if (!status) return status;
    parms.m_normalize = handle.asBool();

    handle = data.inputValue(aMagnitude, &status);
    if (!status) return status;
    parms.m_magnitude = handle.asFloat();

    if (!std::isfinite(parms.m_max_distance) ||
        (parms.m_is_max_dist && parms.m_max_distance < 0.0f) ||
        !std::isfinite(parms.m_magnitude) ||
        parms.m_rad_min < 0.0f) {
        return MS::kInvalidParameter;
    }

    return MS::kSuccess;
}



MStatus DPSpiralField::compute(const MPlug& plug, MDataBlock& data)
{
    try {
        if (plug != outDP) return MS::kUnknownParameter;
        SpiralParams params;
        DOMMY_RETURN_IF_FAILURE(evalParms(data, params));
        MStatus status;
        MPlug objectPlug(thisMObject(), inObject);
        const bool connected = objectPlug.isConnected(&status);
        if (!status) return status;
        if (!connected) return MS::kFailure;
        MDataHandle objectHandle = data.inputValue(inObject, &status);
        if (!status) return status;
        if (objectHandle.type() != MFnData::kNurbsCurve || objectHandle.data().isNull())
            return MS::kInvalidParameter;
        params.m_curveObject = objectHandle.data();

        MDataHandle nameHandle = data.inputValue(aAttributeName, &status);
        if (!status) return status;
        const std::string attrName = nameHandle.asString().asChar();
        if (attrName.empty()) return MS::kInvalidParameter;
        MDataHandle inputHandle = data.inputValue(inDP, &status);
        if (!status) return status;
        const DPData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
            inputHandle, DPData::id, input));
        if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;
        const std::size_t count = input->numberOfPoints();
        if (!validSpiralBudget(count, count == 0U ? 0U : 1U))
            return MS::kInvalidParameter;

        // Evaluate before output construction. Cache/query/ramp failures cannot
        // replace the previous output or leave a partly written field.
        std::vector<DVec3DataList> vectors;
        if (count != 0U) {
            std::vector<const DPointList*> points(1U, input->getPositionPtrConst());
            vectors.emplace_back(count, DVec3Data(0.0f));
            SpiralGen spGen(params, points, vectors);
            DOMMY_RETURN_IF_FAILURE(spGen());
        }
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DPData>(
            data, outDP, DPData::id, [&](DPData& output) -> MStatus {
                output.setDPData(*input);
                if (count != 0U) {
                    checkDPAttribute(attrName, output, true);
                    output.addAttrWithValues(attrName, vectors[0]);
                }
                return MS::kSuccess;
            }));
        return data.setClean(plug);
    }
    DOMMY_COMPUTE_CATCH("DPSpiralField")
}

MStatus DPSpiralField::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    inObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    MRampAttribute RadiusRampAttr;
    aRadiusRamp = RadiusRampAttr.createCurveRamp("RadiusRamp", "radrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusRamp));

    aRadiusRange = nAttr.create( "RadiusRange", "radrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusRange));

    //distance

    aIsMaxDistance = nAttr.create( "IsMaxDistance", "ismxd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMaxDistance));

    aMaxDistance = nAttr.create( "MaxDistance", "mxdist", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxDistance));

    ///Center stream

    MRampAttribute CenterStreamRampAttr;
    aCenterStreamRamp = CenterStreamRampAttr.createCurveRamp("CenterStreamRamp", "cstmrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenterStreamRamp));

    aCenterStreamRange = nAttr.create( "CenterStreamRange", "cstmrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenterStreamRange));

    MRampAttribute CenterDistRampAttr;
    aCenterDistanceRamp = CenterDistRampAttr.createCurveRamp("CenterDistanceRamp", "cdistrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenterDistanceRamp));

    aCenterDistanceRange = nAttr.create( "CenterDistanceRange", "cdistrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenterDistanceRange));

    ///up stream

    MRampAttribute UpStraemRampAttr;
    aUpStraemRamp = UpStraemRampAttr.createCurveRamp("UpStraemRamp", "upstmrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpStraemRamp));

    aUpStraemRange = nAttr.create( "UpStraemRange", "upstmrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpStraemRange));

    MRampAttribute UpDistRampAttr;
    aUpDistanceRamp = UpDistRampAttr.createCurveRamp("UpDistanceRamp", "updistrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpDistanceRamp));

    aUpDistanceRange = nAttr.create( "UpDistanceRange", "updistrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0,5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpDistanceRange));

    const MObject defaultName = fnStringData.create("spiral", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeName = tAttr.create("AttributeName", "attrn", MFnData::kString, defaultName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));

    aInvert = nAttr.create( "Invert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));

    aNormalize = nAttr.create( "Normalize", "norm", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalize));

    aMagnitude = nAttr.create( "Magnitude", "mag", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-50.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMagnitude));

    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inObject, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusRamp, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusRange, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsMaxDistance, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMaxDistance, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCenterStreamRamp, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCenterStreamRange, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCenterDistanceRamp, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCenterDistanceRange, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUpStraemRamp, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUpStraemRange, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUpDistanceRamp, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUpDistanceRange, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeName, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInvert, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalize, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMagnitude, outDP ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPSpiralField", "initialize", std::current_exception());
}
