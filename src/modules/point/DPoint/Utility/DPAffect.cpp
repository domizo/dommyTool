//
//  DPAffect.cpp
//  dommyTool
//
//  Created by yuzo on 11/24/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAffect.h"
#include "common/utility/DStatusOperation.h"
#include "../Common/DPointForceOutput.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"



#include "DNodeOperation.h"

#include "dommyMath.h"
#include "math/DRemapMath.h"
#include "common/utility/DScaledProduct.h"


#include <tbb/blocked_range.h>

#include <cmath>


#pragma mark Utilities Affect

namespace {

    float inverseNormalizedDistance(const float distance,
                                    const float minimum,
                                    const float maximum)
    {
        float normalized = 0.0f;
        return dommy::math::normalizeRange(distance, minimum, maximum, normalized)
            ? 1.0f - normalized
            : 0.0f;
    }

    MStatus sampleAffectRamp(AffectParams& params,
                             const float position,
                             float& value)
    {
        if (!std::isfinite(position)) return MS::kInvalidParameter;

        MStatus status;
        params.m_ramp_attr.getValueAtPosition(position, value, &status);
        if (!status) return status;
        return std::isfinite(value) ? MStatus(MS::kSuccess)
                                    : MStatus(MS::kFailure);
    }

    MStatus writeAffectValue(AffectParams& params,
                             const float rampPosition,
                             const double outputScale,
                             DScalarData& output)
    {
        float rampValue = 0.0f;
        MStatus status = sampleAffectRamp(params, rampPosition, rampValue);
        if (!status) return status;

        double mapped = 0.0, candidate = 0.0;
        return dommy::math::checkedLinearInterpolation(
                   double(params.valueRangeMin), double(params.valueRangeMax), double(rampValue), mapped) &&
               dommy::numeric::scaledProduct(std::array<double, 2>{{mapped, outputScale}}, candidate) &&
               dommy::numeric::checkedFloatCast(candidate, output)
            ? MStatus(MS::kSuccess)
            : MStatus(MS::kFailure);
    }

    MStatus pointDistance(const DPoint& first,
                          const DPoint& second,
                          float& distance)
    {
        dommy::direction::Delta delta;
        if (!dommy::direction::difference(first, second, delta) ||
            !dommy::numeric::checkedFloatCast(delta.length, distance)) {
            return MS::kInvalidParameter;
        }
        return MS::kSuccess;
    }

    class AffectPointOp
    {
    public:
        
        AffectPointOp(AffectParams &param, const DPointList &pts, DScalarDataList &vals)
        : mParam(&param)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                float dist = 0.0f;
                MStatus status = pointDistance(
                    mParam->sourcePt, (*m_pts)[i], dist);
                if (!status) return status;
//                double dist = mParam->sourcePt.distanceTo(MPoint((*m_pts)[i][0],(*m_pts)[i][1],(*m_pts)[i][2]));
                
                float newVal = inverseNormalizedDistance(
                    dist, mParam->m_distanceMin, mParam->m_distanceMax);
//                if (dist>=mParam->m_distanceMin && dist<=mParam->m_distanceMax) {
//                    newVal = 1.0-(dist-mParam->m_distanceMin)/dif;
//                }
                
                status = writeAffectValue(
                    *mParam, newVal, mParam->m_scale, (*m_vals)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        AffectParams *mParam;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
    };//AffectPointOp
    
    
    class AffectAxisOp
    {
    public:
        
        AffectAxisOp(AffectParams &param, const DPointList &pts, DScalarDataList &vals)
        : mParam(&param)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            
            MPoint axisSourceVec(mParam->m_axisSourceVec);
            MPoint axisTargetVec(mParam->m_axisTargetVec);
            if (!dommy::maya::isFinitePoint(axisSourceVec) ||
                !dommy::maya::isFinitePoint(axisTargetVec)) {
                return MS::kInvalidParameter;
            }
            
            const bool useAxisDistance = mParam->m_useAxisDistance;
            
            const double rawAxisDistance =
                axisSourceVec.distanceTo(axisTargetVec);
            float axisDistance = 0.0f;
            if (!dommy::numeric::checkedFloatCast(
                    rawAxisDistance, axisDistance)) {
                return MS::kInvalidParameter;
            }
            float minDist = mParam->m_distanceMin;
            float maxDist= mParam->m_distanceMax;
            
            if (useAxisDistance) {
                minDist = 0.0;//+slideDistanceVal;
                maxDist = axisDistance;//+slideDistanceVal;
            }
            
            if(axisDistance>0.0) {
                MPlane axisPlane;
                MVector pNorm;
                if (!dommy::maya::normalizedVector(
                        MVector(axisTargetVec-axisSourceVec), pNorm)) {
                    return MS::kInvalidParameter;
                }
                
                float d = -pNorm.x*axisSourceVec.x-pNorm.y*axisSourceVec.y-pNorm.z*axisSourceVec.z;
                if (!std::isfinite(d)) return MS::kFailure;
                if (useAxisDistance) {
                    d -= mParam->m_slideDistance;
                    if (!std::isfinite(d)) return MS::kFailure;
                }
                axisPlane.setPlane(pNorm, d);
                //						if (VERBOSE) cout << "plane n:" << pNorm << endl;
                //						if (VERBOSE) cout << "plane d:" << d << endl;
                
                for (size_t i = r.begin(); i != r.end(); i++) {
                    const MPoint point(
                        (*m_pts)[i][0], (*m_pts)[i][1], (*m_pts)[i][2]);
                    if (!dommy::maya::isFinitePoint(point)) {
                        return MS::kInvalidParameter;
                    }
                    float dist = 0.0f;
                    if (!dommy::numeric::checkedFloatCast(
                            axisPlane.directedDistance(point), dist)) {
                        return MS::kFailure;
                    }
                    
                    float newVal = 0.0f;
//                    if (dist>=minDist && dist<=maxDist) {
//                        newVal = 1.0-dist/maxDist;
                    newVal = inverseNormalizedDistance(dist, minDist, maxDist);
//                    }
                    
                    MStatus status = writeAffectValue(
                        *mParam, newVal, mParam->m_scale, (*m_vals)[i]);
                    if (!status) return status;
                }
                
            }
            return MS::kSuccess;
        }
        
    private:
        AffectParams *mParam;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
    };//AffectAxisOp
    
    
    
    class AffectCurveOp
    {
    public:
        AffectCurveOp(AffectParams &param, const DPointList& pts, DScalarDataList& vals)
        : mParam(&param)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsCurve crvFn(mParam->m_object, &status);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point(
                    (*m_pts)[i][0], (*m_pts)[i][1], (*m_pts)[i][2]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                const double rawDistance =
                    crvFn.distanceToPoint(point, MSpace::kWorld, &status);
                float dist = 0.0f;
                if (!status ||
                    !dommy::numeric::checkedFloatCast(rawDistance, dist)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                
                float newVal = inverseNormalizedDistance(
                    dist, mParam->m_distanceMin, mParam->m_distanceMax);
//                if (dist>=mParam->m_distanceMin && dist<=mParam->m_distanceMax) {
//                    newVal = 1.0-dist/mParam->m_distanceMax;
//                }
                
                status = writeAffectValue(
                    *mParam, newVal, mParam->m_scale, (*m_vals)[i]);
                if (!status) return status;
                
            }
            return MS::kSuccess;
        }
        
    private:
        AffectParams *mParam;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
    };//AffectCurveOp
    
    
    class AffectSurfaceOp
    {
    public:
        
        AffectSurfaceOp(AffectParams &param, const DPointList& pts, DScalarDataList& vals)
        : mParam(&param)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsSurface surfFn(mParam->m_object, &status);
            if (!status) return status;
            
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                MDoubleArray uList,vList;
                MPointArray hitArray;
                MPoint pt((*m_pts)[i][0],(*m_pts)[i][1],(*m_pts)[i][2]);
                if (!dommy::maya::isFinitePoint(pt)) {
                    return MS::kInvalidParameter;
                }
                
                float dist=0.0;
                bool isInner = false;
                bool hasIntersection = false;
                if (mParam->m_isCheckInner) {
                    hasIntersection = surfFn.intersect(
                                                     pt, mParam->m_innerCheckAngle,
                                                     uList, vList, hitArray,
                                                     kMFnNurbsEpsilon,
                                                     MSpace::kWorld,
                                                     false, NULL, false, NULL, &status);
                    if (!status) return status;
                }
                if (hasIntersection) {
                    //                                cout << "uList:" << uList.length() << endl;
                    //                                cout << "hitArray:" << hitArray.length() << endl;
                    if (hitArray.length()%2!=0) {//even is out. odd is in.奇数だと内側
                        dist = mParam->m_innerValue;
                        isInner = true;
                    }
                    
                }
                
                if (!isInner) {
                    const double rawDistance =
                        surfFn.distanceToPoint(pt, MSpace::kWorld, &status);
                    if (!status ||
                        !dommy::numeric::checkedFloatCast(
                            rawDistance, dist)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }
                }
                    
                
                float newVal = inverseNormalizedDistance(
                    dist, mParam->m_distanceMin, mParam->m_distanceMax);
//                if (dist>=mParam->m_distanceMin && dist<=mParam->m_distanceMax) {
//                    newVal = 1.0-dist/mParam->m_distanceMax;
//                }
                
                status = writeAffectValue(
                    *mParam, newVal, mParam->m_scale, (*m_vals)[i]);
                if (!status) return status;
                
            }
            return MS::kSuccess;
        }
        
    private:
        AffectParams *mParam;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
    };//AffectSurfaceOp

    class AffectMeshOp
    {
    public:
        
        AffectMeshOp(AffectParams &param, const DPointList& pts, DScalarDataList& vals)
        : mParam(&param)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            
            MStatus status;
            MFnMesh meshFn(mParam->m_object, &status);
            if (!status) return status;
            MMeshIsectAccelParams acc = meshFn.autoUniformGridParams();
            dommy::maya::MeshClosestPointQuery closestPointQuery;
            status = closestPointQuery.create(mParam->m_object);
            if (!status) return status;
            
            MFloatVector floatAngleVec(mParam->m_innerCheckAngle.x,mParam->m_innerCheckAngle.y,mParam->m_innerCheckAngle.z);
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                MPoint pt((*m_pts)[i][0],(*m_pts)[i][1],(*m_pts)[i][2]);
                if (!dommy::maya::isFinitePoint(pt)) {
                    return MS::kInvalidParameter;
                }
                MFloatPointArray hitArray;
                MFloatArray hitRayParamArray;
                
                float dist=0.0;
                bool isInner = false;
                bool hasIntersections = false;
                if (mParam->m_isCheckInner) {
                    hasIntersections = meshFn.allIntersections(
                                                            pt, floatAngleVec, NULL, NULL,
                                                            false, MSpace::kWorld, 9999, false,
                                                            &acc, false, hitArray, &hitRayParamArray,
                                                            NULL, NULL, NULL, NULL, 1.0e-6f, &status);
                    if (!status) return status;
                }
                if (hasIntersections) {
                    if (hitArray.length()%2!=0) {//even is out. odd is in.奇数だと内側
                        dist = mParam->m_innerValue;
                        isInner = true;
                    }
                }
                
                
                if (!isInner) {
                    MPoint clpt;
                    status = closestPointQuery.getClosestPoint(pt, clpt);
                    if (!status || !dommy::maya::isFinitePoint(clpt) ||
                        !dommy::numeric::checkedFloatCast(
                            clpt.distanceTo(pt), dist)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }
                }
                
                
                float newVal = inverseNormalizedDistance(
                    dist, mParam->m_distanceMin, mParam->m_distanceMax);
//                if (dist>=mParam->m_distanceMin && dist<=mParam->m_distanceMax) {
//                    newVal = 1.0-dist/mParam->m_distanceMax;
//                }
                
                status = writeAffectValue(
                    *mParam, newVal, mParam->m_scale, (*m_vals)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        AffectParams *mParam;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
    };//AffectMeshOp
    
    
    
    
    class AffectDPOp
    {
    public:
        
        AffectDPOp(AffectParams &param, const DPointList& pts, DScalarDataList& vals)
        : mParam(&param)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            if (!mParam->m_targetPts ||
                mParam->m_maxDistArray.size() != mParam->m_targetPts->size() ||
                mParam->m_slideArray.size() != mParam->m_targetPts->size() ||
                mParam->m_scaleArray.size() != mParam->m_targetPts->size()) {
                return MS::kInvalidParameter;
            }
            
//            double dpMinDist = mParam->m_distanceMin;
            const double dpMinDist =
                mParam->m_distanceMin-mParam->m_slideDistance;
            if (!std::isfinite(dpMinDist)) return MS::kInvalidParameter;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                size_t j = 0;
                for (auto &tIt:*mParam->m_targetPts) {
//                for (uint j=0; j<mParam->m_targetPts->size(); j++) {
                    float dist = 0.0f;
                    MStatus status = pointDistance((*m_pts)[i], tIt, dist);
                    if (!status) return status;
//                    double dist=MPoint(pt).distanceTo(MPoint((*tIt)[0],(*tIt)[1],(*tIt)[2]));
//                    double dist=MPoint(pt).distanceTo(MPoint((*mParam->m_targetPts)[j][0],(*mParam->m_targetPts)[j][1],(*mParam->m_targetPts)[j][2]));

                    const double rawMinDistance =
                        dpMinDist+mParam->m_slideArray[j];
                    const double rawMaxDistance =
                        mParam->m_maxDistArray[j]+mParam->m_slideArray[j];
                    float tmpMinDist = 0.0f;
                    float tmpMaxDist = 0.0f;
                    if (!dommy::numeric::checkedFloatCast(
                            rawMinDistance, tmpMinDist) ||
                        !dommy::numeric::checkedFloatCast(
                            rawMaxDistance, tmpMaxDist) ||
                        !std::isfinite(mParam->m_scaleArray[j])) {
                        return MS::kInvalidParameter;
                    }
                    float newVal = inverseNormalizedDistance(
                        dist, tmpMinDist, tmpMaxDist);
//                    if (dist>=tmpMinDist && dist<=tmpMaxDist) {
                    
                        float rampVal = 0.0f;
                        status = sampleAffectRamp(
                            *mParam, newVal, rampVal);
                        if (!status) return status;
                        
                        // DPoint mode scales the delta only; other modes scale
                        // the whole mapped value. Retain that scene contract.
                        double candidate = 0.0;
                        if (!dommy::math::checkedMaskedLinearInterpolation(
                                double(mParam->valueRangeMin), double(mParam->valueRangeMax),
                                double(rampVal), double(mParam->m_scaleArray[j]), candidate) ||
                            !dommy::numeric::checkedFloatCast(
                                candidate, newVal)) {
                            return MS::kFailure;
                        }
                    
                        if ((*m_vals)[i]<newVal) (*m_vals)[i] = newVal;
                        
//                    }
                    j++;
                }
                
            }
            return MS::kSuccess;
        }
        
    private:
        AffectParams *mParam;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
    };//DistanceMeshOp
    
    
    
    
    
    // The byte budget covers centers/results, target scalar staging and
    // partition tables, with the shared four-copy safety allowance.
    bool validAffectBudget(std::size_t queries, std::size_t partitions,
                           std::size_t targets, std::size_t centerWork = 0U)
    {
        using namespace dommy::generation;
        const std::size_t maximum = 256U * 1024U * 1024U / 4U;
        std::size_t bytes = 0U, partBytes = 0U, targetBytes = 0U;
        return dommy::point::validPointForceBudget(queries, partitions, targets, 1U, centerWork) &&
            checkedProduct(queries, sizeof(DPoint) + sizeof(float), maximum, bytes) &&
            checkedProduct(partitions, sizeof(DScalarDataList) + sizeof(void*), maximum, partBytes) &&
            checkedProduct(targets, 3U * sizeof(float), maximum, targetBytes) &&
            checkedSum(bytes, partBytes, maximum, bytes) &&
            checkedSum(bytes, targetBytes, maximum, bytes);
    }

    class AffectGen
    {
    public:
        AffectGen(AffectParams &param,
                  const std::vector<const DPointList* > &pts,
                  std::vector<DScalarDataList > &vals)
        : m_pts(&pts)
        , m_vals(&vals)
        , mParam(&param)
        {
        
        }
        
        
        MStatus operator()()
        {
            if (m_pts->size() != m_vals->size()) {
                return MS::kInvalidParameter;
            }

            std::size_t queryCount = 0U;
            for (const DPointList* partition : *m_pts) {
                if (!partition || !dommy::generation::checkedSum(
                        queryCount, partition->size(),
                        dommy::generation::kMaxGeneratedPointCount, queryCount))
                    return MS::kInvalidParameter;
            }
            if (mParam->m_type == AFFECT_DPOINT && !mParam->m_targetPts)
                return MS::kInvalidParameter;
            const std::size_t targets = mParam->m_type == AFFECT_DPOINT
                ? mParam->m_targetPts->size() : 1U;
            if (!validAffectBudget(queryCount, m_pts->size(), targets))
                return MS::kInvalidParameter;

            switch (mParam->m_type) {
                case AFFECT_POINT://point
                {
                    for (size_t n=0; n<m_pts->size(); ++n) {
                        if (!(*m_pts)[n] ||
                            (*m_pts)[n]->size() != (*m_vals)[n].size()) {
                            return MS::kInvalidParameter;
                        }
                        AffectPointOp op(*mParam, *(*m_pts)[n], (*m_vals)[n]);
                        
                        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                        const MStatus status = op(range);
                        if (!status) return status;
                    }
                }
                    break;
                case AFFECT_AXIS_LINE://axis
                {
                    for (size_t n=0; n<m_pts->size(); ++n) {
                        if (!(*m_pts)[n] ||
                            (*m_pts)[n]->size() != (*m_vals)[n].size()) {
                            return MS::kInvalidParameter;
                        }
                        AffectAxisOp op(*mParam, *(*m_pts)[n], (*m_vals)[n]);
                        
                        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                        const MStatus status = op(range);
                        if (!status) return status;
                    }
                }
                    break;
                case AFFECT_OBJECT://sorce object
                {
                    if(mParam->m_objectType == MFnData::kNurbsCurve) {//Nurbs Curve Object
                        
                        size_t n=0;
                        for (n=0; n<m_pts->size(); ++n) {
                            if (!(*m_pts)[n] ||
                                (*m_pts)[n]->size() != (*m_vals)[n].size()) {
                                return MS::kInvalidParameter;
                            }
                            AffectCurveOp op(*mParam, *(*m_pts)[n], (*m_vals)[n]);
                            
                            tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                            const MStatus status = op(range);
                            if (!status) return status;
                        }
                        
                    } else if (mParam->m_objectType == MFnData::kNurbsSurface) {//Nurbs Surface Object
                        
                        size_t n=0;
                        for (n=0; n<m_pts->size(); ++n) {
                            if (!(*m_pts)[n] ||
                                (*m_pts)[n]->size() != (*m_vals)[n].size()) {
                                return MS::kInvalidParameter;
                            }
                            AffectSurfaceOp op(*mParam, *(*m_pts)[n], (*m_vals)[n]);
                            
                            tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                            const MStatus status = op(range);
                            if (!status) return status;
                        }
                        
                        
                    } else if (mParam->m_objectType == MFnData::kMesh) {//Mesh Object
                        
                        size_t n=0;
                        for (n=0; n<m_pts->size(); ++n) {
                            if (!(*m_pts)[n] ||
                                (*m_pts)[n]->size() != (*m_vals)[n].size()) {
                                return MS::kInvalidParameter;
                            }
                            AffectMeshOp op(*mParam, *(*m_pts)[n], (*m_vals)[n]);
                            
                            tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                            const MStatus status = op(range);
                            if (!status) return status;
                        }
                    } else {
                        return MS::kInvalidParameter;
                    }
                }
                    break;
                case AFFECT_DPOINT://sorce DP
                {
                    
                    size_t n=0;
                    for (n=0; n<m_pts->size(); ++n) {
                        if (!(*m_pts)[n] ||
                            (*m_pts)[n]->size() != (*m_vals)[n].size()) {
                            return MS::kInvalidParameter;
                        }
                        AffectDPOp op(*mParam, *(*m_pts)[n], (*m_vals)[n]);
                        
                        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                        const MStatus status = op(range);
                        if (!status) return status;
                        
                        
                    }
                    
                }
                    break;
                default:
                    return MS::kInvalidParameter;
            }
            return MS::kSuccess;
        }
        
        const std::vector<const DPointList* > *m_pts;
        std::vector<DScalarDataList > *m_vals;
        
        AffectParams *mParam;
    }; //AffectGen
        
    
}//Utilities Affect



#pragma mark Affect Class

const MTypeId DPAffect::typeId( ID_DP_AFFECT );

MObject DPAffect::inDD;

MObject DPAffect::aAttributeClass;
MObject DPAffect::aAffectType;

MObject DPAffect::aDistance;
MObject DPAffect::aSlideDistance;

MObject DPAffect::aSourcePoint;

MObject DPAffect::aAxisSource;
MObject DPAffect::aAxisTarget;
MObject DPAffect::aAxisSourceMatrix;
MObject DPAffect::aAxisTargetMatrix;
MObject DPAffect::aUseAxisDistance;

MObject DPAffect::aInObject;
MObject DPAffect::aTolerance;
MObject DPAffect::aIsCheckInner;
MObject DPAffect::aInnerCheckAngle;
MObject DPAffect::aMeshResolution;
MObject DPAffect::aInnerValue;

MObject DPAffect::aTargetDP;
MObject DPAffect::aDistanceAttribute;
MObject DPAffect::aSlideAttribute;
MObject DPAffect::aScaleAttribute;

MObject DPAffect::aAffectRamp;
MObject DPAffect::aValueRange;
MObject DPAffect::aScale;
MObject DPAffect::aOutAttribute;

MObject DPAffect::outDD;


void *DPAffect::creator() {
    return ::dommy::maya::createObject("DPAffect", [&]() {
        return new DPAffect();
    });
}





MStatus DPAffect::evalAffectParms(MDataBlock& data, AffectParams& parms) {
    MStatus stat;

    MDataHandle aAffectTypeHandle = data.inputValue(aAffectType, &stat);
    if (!stat) return stat;
    parms.m_type = aAffectTypeHandle.asShort();
    if (parms.m_type < AFFECT_POINT ||
        parms.m_type >= AFFECT_TYPE_COUNT) {
        return MS::kInvalidParameter;
    }

    MDataHandle aSourcePointHandle = data.inputValue(aSourcePoint, &stat);
    if (!stat) return stat;
    const float3 &srcpt = aSourcePointHandle.asFloat3();
    if (!std::isfinite(srcpt[0]) || !std::isfinite(srcpt[1]) ||
        !std::isfinite(srcpt[2])) {
        return MS::kInvalidParameter;
    }
    parms.sourcePt = DPoint(srcpt[0], srcpt[1], srcpt[2]);

    MDataHandle aDistanceHandle = data.inputValue(aDistance, &stat);
    if (!stat) return stat;
    const float2 &distanceVal = aDistanceHandle.asFloat2();
    MDataHandle aSlideDistanceHandle = data.inputValue(aSlideDistance, &stat);
    if (!stat) return stat;
    parms.m_slideDistance = aSlideDistanceHandle.asFloat();
    MDataHandle aValueRangeHandle = data.inputValue(aValueRange, &stat);
    if (!stat) return stat;
    const float2 &valueRangeVal = aValueRangeHandle.asFloat2();
    MDataHandle aScaleHandle = data.inputValue(aScale, &stat);
    if (!stat) return stat;
    parms.m_scale = aScaleHandle.asFloat();

    if (!std::isfinite(distanceVal[0]) ||
        !std::isfinite(distanceVal[1]) ||
        !std::isfinite(parms.m_slideDistance) ||
        !std::isfinite(valueRangeVal[0]) ||
        !std::isfinite(valueRangeVal[1]) ||
        !std::isfinite(parms.m_scale)) {
        return MS::kInvalidParameter;
    }

    MPlug affectRampPlug(thisMObject(), aAffectRamp);
    parms.m_ramp_attr = MRampAttribute(affectRampPlug, &stat);
    if (!stat) return stat;

    MDataHandle aToleranceHandle = data.inputValue(aTolerance, &stat);
    if (!stat) return stat;
    parms.m_tolerance = aToleranceHandle.asDouble();
    if (!std::isfinite(parms.m_tolerance) ||
        parms.m_tolerance < kMFnNurbsEpsilon) {
        return MS::kInvalidParameter;
    }

    MDataHandle aIsCheckInnerHandle = data.inputValue(aIsCheckInner, &stat);
    if (!stat) return stat;
    parms.m_isCheckInner = aIsCheckInnerHandle.asBool();
    MDataHandle aInnerCheckAngleHandle = data.inputValue(aInnerCheckAngle, &stat);
    if (!stat) return stat;
    const float3 &innerCheckVec = aInnerCheckAngleHandle.asFloat3();
    const MVector innerDirection(
        innerCheckVec[0], innerCheckVec[1], innerCheckVec[2]);
    if (!dommy::maya::isFiniteVector(innerDirection)) {
        return MS::kInvalidParameter;
    }
    if (parms.m_isCheckInner &&
        !dommy::maya::normalizedVector(
            innerDirection, parms.m_innerCheckAngle)) {
        return MS::kInvalidParameter;
    }

    MDataHandle aInnerValueHandle = data.inputValue(aInnerValue, &stat);
    if (!stat) return stat;
    parms.m_innerValue = aInnerValueHandle.asFloat();
    MDataHandle aMeshResolutionHandle = data.inputValue(aMeshResolution, &stat);
    if (!stat) return stat;
    parms.meshResolution = aMeshResolutionHandle.asInt();
    if (!std::isfinite(parms.m_innerValue) ||
        parms.meshResolution < 100) {
        return MS::kInvalidParameter;
    }

    MDataHandle objectHandle = data.inputValue(aInObject, &stat);
    if (!stat) return stat;
    parms.m_object = objectHandle.data();
    parms.m_objectType = objectHandle.type();

    if (!dommy::numeric::checkedFloatCast(
            static_cast<double>(distanceVal[0]) +
                parms.m_slideDistance,
            parms.m_distanceMin) ||
        !dommy::numeric::checkedFloatCast(
            static_cast<double>(distanceVal[1]) +
                parms.m_slideDistance,
            parms.m_distanceMax)) {
        return MS::kInvalidParameter;
    }

    float minVal = valueRangeVal[0];
    float maxVal = valueRangeVal[1];
	dommy::math::orderBounds(minVal, maxVal);
    parms.valueRangeMin = minVal;
    parms.valueRangeMax = maxVal;

    //for axis
	MPlug srcMatPlug(thisMObject(), aAxisSourceMatrix);
	const bool sourceConnected = srcMatPlug.isConnected(&stat);
    if (!stat) return stat;
	if(sourceConnected) {
		MDataHandle aAxisSourceMatrixHandle = data.inputValue(aAxisSourceMatrix, &stat);
		if (!stat) return stat;
		MMatrix mat = aAxisSourceMatrixHandle.asMatrix();
        if (!dommy::maya::isFiniteMatrix(mat)) {
            return MS::kInvalidParameter;
        }
		MTransformationMatrix transMat(mat);
		parms.m_axisSourceVec =transMat.getTranslation(MSpace::kWorld, &stat);
        if (!stat) return stat;
	} else {
		MDataHandle aAxisSourceHandle = data.inputValue(aAxisSource, &stat);
		if (!stat) return stat;
		const float3 &axisSrc = aAxisSourceHandle.asFloat3();
        parms.m_axisSourceVec = MPoint(axisSrc[0],axisSrc[1],axisSrc[2]);
	}
    if (!dommy::maya::isFinitePoint(parms.m_axisSourceVec)) {
        return MS::kInvalidParameter;
    }

	MPlug tarMatPlug(thisMObject(), aAxisTargetMatrix);
	const bool targetConnected = tarMatPlug.isConnected(&stat);
    if (!stat) return stat;
	if(targetConnected) {
		MDataHandle aAxisTargetMatrixHandle = data.inputValue(aAxisTargetMatrix, &stat);
		if (!stat) return stat;
		MMatrix mat = aAxisTargetMatrixHandle.asMatrix();
        if (!dommy::maya::isFiniteMatrix(mat)) {
            return MS::kInvalidParameter;
        }
		MTransformationMatrix transMat(mat);
		parms.m_axisTargetVec =transMat.getTranslation(MSpace::kWorld, &stat);
        if (!stat) return stat;
	} else {
        MDataHandle aAxisTargetHandle = data.inputValue(aAxisTarget, &stat);
        if (!stat) return stat;
        const float3 &axisTar = aAxisTargetHandle.asFloat3();
		parms.m_axisTargetVec = MPoint(axisTar[0],axisTar[1],axisTar[2]);
	}
    if (!dommy::maya::isFinitePoint(parms.m_axisTargetVec)) {
        return MS::kInvalidParameter;
    }

    MDataHandle aUseAxisDistanceHandle = data.inputValue(aUseAxisDistance, &stat);
    if (!stat) return stat;
    parms.m_useAxisDistance = aUseAxisDistanceHandle.asBool();
	MDataHandle aAttributeClassHandle = data.inputValue(aAttributeClass, &stat);
	if (!stat) return stat;
	parms.m_attr_class = aAttributeClassHandle.asShort();
    if (parms.m_attr_class != DP_ATTR_CLASS_POINT &&
        parms.m_attr_class != DP_ATTR_CLASS_PRIMITIVE) {
        return MS::kInvalidParameter;
    }

    return MS::kSuccess;
}


MStatus DPAffect::compute(const MPlug& plug, MDataBlock& data)
{
    try {
        if (plug != outDD) return MS::kUnknownParameter;
        AffectParams params;
        DOMMY_RETURN_IF_FAILURE(evalAffectParms(data, params));
        MStatus status;
        MDataHandle nameHandle = data.inputValue(aOutAttribute, &status);
        if (!status) return status;
        std::string outAttrName = nameHandle.asString().asChar();
        if (outAttrName.empty()) outAttrName = "affect";
        if (params.m_type == AFFECT_OBJECT && params.m_object.isNull())
            return MS::kFailure;

        MDataHandle inputHandle = data.inputValue(inDD, &status);
        if (!status) return status;
        dommy::point::PointForceInput input;
        DOMMY_RETURN_IF_FAILURE(input.bind(inputHandle));
        if (!input.points && !input.curves && !input.meshes)
            return MS::kInvalidParameter;
        const bool primitive = !input.points &&
            params.m_attr_class == DP_ATTR_CLASS_PRIMITIVE;
        const std::size_t queryCount = primitive ? input.partitionCount : input.pointCount;
        const bool evaluate = input.partitionCount != 0U;
        const DPData* target = nullptr;
        std::size_t targetCount = 1U;
        if (evaluate && params.m_type == AFFECT_DPOINT) {
            MDataHandle targetHandle = data.inputValue(aTargetDP, &status);
            if (!status) return status;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
                targetHandle, DPData::id, target));
            if (!target || !target->hasValidPointTopology()) return MS::kInvalidParameter;
            params.m_targetPts = target->getPositionPtrConst();
            if (!params.m_targetPts) return MS::kInvalidParameter;
            targetCount = params.m_targetPts->size();
        }
        // Bound all partitions together before copying data, building centers,
        // converting target attributes, allocating results or sampling ramps.
        if (!validAffectBudget(queryCount, input.partitionCount, targetCount,
                               primitive ? input.pointCount : 0U))
            return MS::kInvalidParameter;
        if (target && targetCount != 0U) {
            MDataHandle distanceHandle = data.inputValue(aDistanceAttribute, &status);
            if (!status) return status;
            MDataHandle slideHandle = data.inputValue(aSlideAttribute, &status);
            if (!status) return status;
            MDataHandle scaleHandle = data.inputValue(aScaleAttribute, &status);
            if (!status) return status;
            if (!dommy::point::readPointForceScalars(
                    target->getAttr(distanceHandle.asString().asChar()), targetCount,
                    params.m_distanceMax, params.m_maxDistArray) ||
                !dommy::point::readPointForceScalars(
                    target->getAttr(slideHandle.asString().asChar()), targetCount,
                    params.m_slideDistance, params.m_slideArray) ||
                !dommy::point::readPointForceScalars(
                    target->getAttr(scaleHandle.asString().asChar()), targetCount,
                    params.m_scale, params.m_scaleArray))
                return MS::kInvalidParameter;
        }

        // Preserve the historical DP + valid empty target contract: remove the
        // named field without generating a replacement. Invalid targets fail.
        const bool removeOnly = input.points && evaluate &&
            params.m_type == AFFECT_DPOINT && targetCount == 0U;
        DPointList centers;
        std::vector<const DPointList*> points;
        std::vector<DScalarDataList> values;
        if (evaluate && !removeOnly) {
            points.reserve(primitive ? 1U : input.partitionCount);
            if (input.points) {
                points.push_back(input.points->getPositionPtrConst());
            } else if (primitive) {
                centers.resize(queryCount, DPoint(0.0f));
                if (input.curves) input.curves->getPrimitiveCenterList(centers);
                else input.meshes->getPrimitiveCenterList(centers);
                if (centers.size() != queryCount) return MS::kFailure;
                points.push_back(&centers);
            } else if (input.curves) {
                for (auto it = input.curves->beginCurves(); it != input.curves->endCurves(); ++it)
                    points.push_back((*it)->getPositionPtrConst());
            } else {
                for (auto it = input.meshes->beginMeshes(); it != input.meshes->endMeshes(); ++it)
                    points.push_back((*it)->getPositionPtrConst());
            }
            values.reserve(points.size());
            for (const DPointList* partition : points) {
                if (!partition) return MS::kInvalidParameter;
                values.emplace_back(partition->size(), 0.0f);
            }
            AffectGen affectgen(params, points, values);
            DOMMY_RETURN_IF_FAILURE(affectgen());
        }

        if (input.points) {
            DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DPData>(
                data, outDD, DPData::id, [&](DPData& output) -> MStatus {
                    output.setDPData(*input.points);
                    if (evaluate) {
                        checkDPAttribute(outAttrName, output, true);
                        if (!removeOnly) output.addAttrWithValues(outAttrName, values[0]);
                    }
                    return MS::kSuccess;
                }));
        } else if (input.curves) {
            DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DCurveData>(
                data, outDD, DCurveData::id, [&](DCurveData& output) -> MStatus {
                    DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input.curves));
                    if (evaluate && primitive) {
                        checkDDataAttribute(outAttrName, output, true);
                        output.addAttrWithValues(outAttrName, values[0]);
                    } else if (evaluate) {
                        checkDDataPointAttribute(outAttrName, output, true);
                        std::size_t index = 0U;
                        for (auto it = output.beginCurves(); it != output.endCurves(); ++it)
                            (*it)->addAttrWithValues(outAttrName, values[index++]);
                    }
                    return MS::kSuccess;
                }));
        } else {
            DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DMeshData>(
                data, outDD, DMeshData::id, [&](DMeshData& output) -> MStatus {
                    DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input.meshes));
                    if (evaluate && primitive) {
                        checkDDataAttribute(outAttrName, output, true);
                        output.addDPAttributeValues(outAttrName, values[0]);
                    } else if (evaluate) {
                        checkDMeshPointAttribute(outAttrName, output, true);
                        std::size_t index = 0U;
                        for (auto it = output.beginMeshes(); it != output.endMeshes(); ++it)
                            (*it)->addAttrWithValues(outAttrName, values[index++]);
                    }
                    return MS::kSuccess;
                }));
        }
        return data.setClean(plug);
    }
    DOMMY_COMPUTE_CATCH("DPAffect")
}

MStatus DPAffect::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    MFnEnumAttribute attrClassEnumFn;
    aAttributeClass = attrClassEnumFn.create( "AttributeClass", "attrcls", DP_ATTR_CLASS_POINT, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.addField( "Point", DP_ATTR_CLASS_POINT ));
    DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.addField( "Primitive", DP_ATTR_CLASS_PRIMITIVE ));
    DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeClass));

    MFnEnumAttribute enumFn;
    aAffectType = enumFn.create( "AffectType", "aft", AFFECT_POINT, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Point", AFFECT_POINT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Axis Line", AFFECT_AXIS_LINE ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Object", AFFECT_OBJECT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "DPoint", AFFECT_DPOINT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAffectType));

    aDistance = nAttr.create( "Distance", "dist", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));

    aSlideDistance = nAttr.create( "SlideDistance", "sdist", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSlideDistance));

#pragma mark point
    aSourcePoint = nAttr.create( "SourcePoint", "spt", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSourcePoint));

#pragma mark axis line
    aAxisSource = nAttr.create( "AxisSource", "axsrc", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisSource));

    aAxisTarget = nAttr.create( "AxisTarget", "axtar", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisTarget));

    MFnMatrixAttribute matAttr;
    aAxisSourceMatrix = matAttr.create( "AxisSourceMatrix", "asrcmat",MFnMatrixAttribute::kFloat, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisSourceMatrix));

    aAxisTargetMatrix = matAttr.create( "AxisTargetMatrix", "atarmat",MFnMatrixAttribute::kFloat, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisTargetMatrix));

    aUseAxisDistance = nAttr.create( "UseAxisDistance", "uaxd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseAxisDistance));

#pragma mark object
    aInObject = gAttr.create( "InObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));

    aTolerance = nAttr.create( "Tolerance", "tol", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(kMFnNurbsEpsilon));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    aIsCheckInner = nAttr.create( "IsCheckInner", "ici", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCheckInner));

    aInnerCheckAngle = nAttr.create( "InnerCheckAngle", "icang", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInnerCheckAngle));

    aMeshResolution = nAttr.create( "MeshResolution", "meres", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(3000));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1500));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMeshResolution));

    aInnerValue = nAttr.create( "InnerValue", "inval", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInnerValue));

#pragma mark target dp

    aTargetDP = tAttr.create("TargetDP", "tardp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    MObject attrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDistanceAttribute = tAttr.create("DistanceAttribute", "disattr", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistanceAttribute));

    aSlideAttribute = tAttr.create("SlideAttribute", "sldattr", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSlideAttribute));

    aScaleAttribute = tAttr.create("ScaleAttribute", "sclattr", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScaleAttribute));

#pragma mark value option

    aAffectRamp = MRampAttribute::createCurveRamp("AffectRamp", "afr", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAffectRamp));

    aValueRange = nAttr.create( "ValueRange", "valr", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRange));

    aScale = nAttr.create( "Scale", "sc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    MObject nAttrName = fnStringData.create("aval", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOutAttribute = tAttr.create("OutAttribute", "outattr", MFnData::kString, nAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutAttribute));

    //create custom data
    outDD = gAttr.create( "outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeClass, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAffectType, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistance, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSlideDistance, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourcePoint, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisSource, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisTarget, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisSourceMatrix, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisTargetMatrix, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseAxisDistance, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTolerance, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCheckInner, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInnerCheckAngle, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMeshResolution, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInnerValue, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistanceAttribute, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSlideAttribute, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScaleAttribute, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAffectRamp, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aValueRange, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAffect", "initialize", std::current_exception());
}
