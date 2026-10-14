//
//  DPAvoid.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/29.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAvoid.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDirectionMath.h"
#include "DPoint/Common/DPointForceOutput.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"


#include "DNodeOperation.h"


#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <tbb/combinable.h>

#include <atomic>
#include <cmath>
#include <limits>



#pragma mark Utilities Magnet

namespace {

    const int kMaximumAvoidIterations = 10000;

    bool isFiniteDPoint(const DPoint& point)
    {
        return std::isfinite(static_cast<double>(point[0])) &&
               std::isfinite(static_cast<double>(point[1])) &&
               std::isfinite(static_cast<double>(point[2]));
    }

    MStatus displaceDPoint(DPoint& point,
                           const DPoint& target,
                           const double radius,
                           const double weight,
                           bool& displaced)
    {
        displaced = false;
        if (!std::isfinite(radius) || radius <= 0.0 ||
            !std::isfinite(weight)) {
            return MS::kInvalidParameter;
        }

        dommy::direction::Delta delta;
        if (!dommy::direction::difference(target, point, delta)) {
            return MS::kInvalidParameter;
        }
        if (delta.length >= radius ||
            delta.length <= std::numeric_limits<double>::epsilon()) {
            return MS::kSuccess;
        }

        const double scale = (radius - delta.length) * weight / delta.length;
        if (!std::isfinite(scale)) return MS::kFailure;

        float candidate[3] = {0.0f, 0.0f, 0.0f};
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::checkedFloatCast(
                    static_cast<double>(point[axis]) +
                        delta.values[axis] * scale,
                    candidate[axis])) {
                return MS::kFailure;
            }
        }
        point = DPoint(candidate[0], candidate[1], candidate[2]);
        displaced = true;
        return MS::kSuccess;
    }

    MStatus displaceMayaPoint(MPoint& point,
                              const MPoint& target,
                              const double radius,
                              const double weight,
                              bool& displaced)
    {
        displaced = false;
        if (!dommy::maya::isFinitePoint(point) ||
            !dommy::maya::isFinitePoint(target) ||
            !std::isfinite(radius) || radius <= 0.0 ||
            !std::isfinite(weight)) {
            return MS::kInvalidParameter;
        }

        const double distance = point.distanceTo(target);
        if (!std::isfinite(distance)) return MS::kFailure;
        if (distance >= radius ||
            distance <= std::numeric_limits<double>::epsilon()) {
            return MS::kSuccess;
        }

        MVector direction;
        if (!dommy::maya::normalizedVector(point - target, direction)) {
            return MS::kFailure;
        }
        const MPoint candidate =
            point + direction * ((radius - distance) * weight);
        if (!dommy::maya::isFloatRepresentablePoint(candidate)) {
            return MS::kFailure;
        }

        point = candidate;
        displaced = true;
        return MS::kSuccess;
    }

    class AvoidPointOp
    {
    public:
        
        AvoidPointOp(AvoidParams &param,
                     DPointList &pts,
                     const DScalarDataList &weight,
                     std::atomic<bool>& succeeded)
        : mParam(&param)
        , m_pts(&pts)
        , m_weight(&weight)
        , m_succeeded(&succeeded)
        { }
        
        void operator()(const tbb::blocked_range<size_t>& r) const
        {
            for (size_t i = r.begin(); i != r.end(); i++) {
                if (!m_succeeded->load(std::memory_order_relaxed)) return;

                bool displaced = false;
                if (!displaceDPoint(
                        (*m_pts)[i], mParam->m_targetPt,
                        mParam->m_radius, (*m_weight)[i], displaced)) {
                    m_succeeded->store(false, std::memory_order_relaxed);
                    return;
                }
            }
        }
        
        
        
        
    private:
        AvoidParams *mParam;
        DPointList *m_pts;
        const DScalarDataList *m_weight;
        std::atomic<bool>* m_succeeded;
        
    };//AvoidPointOp
    
    class AvoidCurveOp
    {
    public:
        
        AvoidCurveOp(AvoidParams &param, DPointList& pts, const DScalarDataList &weight)
        : mParam(&param)
        , m_pts(&pts)
        , m_weight(&weight)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsCurve crvFn(mParam->m_object, &status);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                
                MPoint tmpPt = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(tmpPt)) {
                    return MS::kInvalidParameter;
                }

                bool pointChanged = false;
                for (std::size_t j = 0U;
                     j < static_cast<std::size_t>(mParam->m_iterate);
                     ++j) {
                    const MPoint closestPoint = crvFn.closestPoint(
                        tmpPt, NULL, kMFnNurbsEpsilon,
                        MSpace::kWorld, &status);
                    if (!status || !dommy::maya::isFinitePoint(closestPoint)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }

                    bool displaced = false;
                    status = displaceMayaPoint(
                        tmpPt, closestPoint, mParam->m_radius,
                        (*m_weight)[i], displaced);
                    if (!status) return status;
                    pointChanged = pointChanged || displaced;
                }

                if (pointChanged) {
                    (*m_pts)[i][0] = tmpPt[0];
                    (*m_pts)[i][1] = tmpPt[1];
                    (*m_pts)[i][2] = tmpPt[2];
                }
            }
            return MS::kSuccess;
        }
        
    private:
        AvoidParams *mParam;
        DPointList *m_pts;
        
        const DScalarDataList *m_weight;
    };//AvoidCurveOp
    
    class AvoidSurfaceOp
    {
    public:
        
        AvoidSurfaceOp(AvoidParams &param, DPointList& pts, const DScalarDataList &weight)
        : mParam(&param)
        ,m_pts(&pts)
        , m_weight(&weight)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsSurface surfFn(mParam->m_object, &status);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                
                
                
                MPoint tmpPt = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(tmpPt)) {
                    return MS::kInvalidParameter;
                }

                bool pointChanged = false;
                for (std::size_t j = 0U;
                     j < static_cast<std::size_t>(mParam->m_iterate);
                     ++j) {
                    double paramU,paramV;
                    const MPoint closestPoint = surfFn.closestPoint(
                        tmpPt, &paramU, &paramV, false, 0.001,
                        MSpace::kWorld, &status);
                    if (!status || !dommy::maya::isFinitePoint(closestPoint)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }

                    bool displaced = false;
                    status = displaceMayaPoint(
                        tmpPt, closestPoint, mParam->m_radius,
                        (*m_weight)[i], displaced);
                    if (!status) return status;
                    pointChanged = pointChanged || displaced;
                }

                if (pointChanged) {
                    (*m_pts)[i][0] = tmpPt[0];
                    (*m_pts)[i][1] = tmpPt[1];
                    (*m_pts)[i][2] = tmpPt[2];
                }
            }
            return MS::kSuccess;
        }
        
    private:
        AvoidParams *mParam;
        DPointList *m_pts;
        
        const DScalarDataList *m_weight;
    };//AvoidSurfaceOp
    
    class AvoidMeshOp
    {
    public:
        
        AvoidMeshOp(AvoidParams &param, DPointList& pts, const DScalarDataList &weight)
        : mParam(&param)
        ,m_pts(&pts)
        , m_weight(&weight)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            dommy::maya::MeshClosestPointQuery closestPointQuery;
            MStatus status =
                closestPointQuery.create(mParam->m_object);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                
                MPoint tmpPt = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(tmpPt)) {
                    return MS::kInvalidParameter;
                }

                bool pointChanged = false;
                for (std::size_t j = 0U;
                     j < static_cast<std::size_t>(mParam->m_iterate);
                     ++j) {
                    MPoint closestPoint;
                    status = closestPointQuery.getClosestPoint(
                        tmpPt, closestPoint);
                    if (!status ||
                        !dommy::maya::isFinitePoint(closestPoint)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }

                    bool displaced = false;
                    status = displaceMayaPoint(
                        tmpPt, closestPoint, mParam->m_radius,
                        (*m_weight)[i], displaced);
                    if (!status) return status;
                    pointChanged = pointChanged || displaced;
                }

                if (pointChanged) {
                    (*m_pts)[i][0] = tmpPt[0];
                    (*m_pts)[i][1] = tmpPt[1];
                    (*m_pts)[i][2] = tmpPt[2];
                }
            }
            return MS::kSuccess;
        }
        
    private:
        AvoidParams *mParam;
        DPointList *m_pts;
        
        const DScalarDataList *m_weight;
        
    };//AvoidMeshOp
    
    
    class AvoidDPOp
    {
    public:
        
        AvoidDPOp(AvoidParams &param,
                  DPointList& pts,
                  const DScalarDataList &weight,
                  std::atomic<bool>& succeeded)
        : mParam(&param)
        ,m_pts(&pts)
        , m_weight(&weight)
        , m_succeeded(&succeeded)
        { }
        
        void operator()(const tbb::blocked_range<size_t>& r) const
        {
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                if (!m_succeeded->load(std::memory_order_relaxed)) return;

                DPoint tmpPt((*m_pts)[i]);
                bool pointChanged = false;

                for (std::size_t j = 0U;
                     j < static_cast<std::size_t>(mParam->m_iterate);
                     ++j) {
                    for (std::size_t targetIndex = 0U;
                         targetIndex < mParam->m_targetPts->size();
                         ++targetIndex) {
                        bool displaced = false;
                        if (!displaceDPoint(
                                tmpPt,
                                (*mParam->m_targetPts)[targetIndex],
                                (*m_radius)[targetIndex],
                                (*m_weight)[i],
                                displaced)) {
                            m_succeeded->store(
                                false, std::memory_order_relaxed);
                            return;
                        }
                        pointChanged = pointChanged || displaced;
                    }
                }

                if (pointChanged) {
                    (*m_pts)[i] = tmpPt;
                }
            }
            
        }
        
        
        DScalarDataList *m_radius;
        
    private:
        AvoidParams *mParam;
        DPointList *m_pts;
        const DScalarDataList *m_weight;
        std::atomic<bool>* m_succeeded;
        
    };//AVoidDPOp
    
    
    
    class AvoidDCOp
    {
    public:
        
        AvoidDCOp(AvoidParams &param, DPointList& pts, const DScalarDataList &weight, std::vector<MObject> &crvObjects)
        : mParam(&param)
        ,m_pts(&pts)
        , m_weight(&weight)
        , m_crvObjects(&crvObjects)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            for (size_t i = r.begin(); i != r.end(); i++) {
                MPoint tmpPt = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(tmpPt)) {
                    return MS::kInvalidParameter;
                }

                bool pointChanged = false;
                for (std::size_t j = 0U;
                     j < static_cast<std::size_t>(mParam->m_iterate);
                     ++j) {
                    for (std::size_t curveIndex = 0U;
                         curveIndex < m_crvObjects->size();
                         ++curveIndex) {
                        MStatus status;
                        MFnNurbsCurve curveFn(
                            (*m_crvObjects)[curveIndex], &status);
                        if (!status) return status;

                        const MPoint closestPoint = curveFn.closestPoint(
                            tmpPt, NULL, kMFnNurbsEpsilon,
                            MSpace::kObject, &status);
                        if (!status ||
                            !dommy::maya::isFinitePoint(closestPoint)) {
                            return status ? MStatus(MS::kFailure) : status;
                        }

                        bool displaced = false;
                        status = displaceMayaPoint(
                            tmpPt, closestPoint, (*m_radius)[curveIndex],
                            (*m_weight)[i], displaced);
                        if (!status) return status;
                        pointChanged = pointChanged || displaced;
                    }
                }

                if (pointChanged) {
                    (*m_pts)[i][0] = tmpPt[0];
                    (*m_pts)[i][1] = tmpPt[1];
                    (*m_pts)[i][2] = tmpPt[2];
                }
            }
            return MS::kSuccess;
        }
        
        
        DScalarDataList *m_radius;
        
        
    private:
        AvoidParams *mParam;
        DPointList *m_pts;
        const DScalarDataList *m_weight;
        std::vector<MObject> *m_crvObjects;
    };//AvoidDCOp
    
    
    
    
    
    class AvoidGen
    {
    public:
        AvoidGen(std::vector<DPointList* > &pts,
                  const std::vector<DScalarDataList > &weights)
        : m_pts(&pts)
        , m_weights(&weights)
        , mParam(nullptr)
        {}
        
        MStatus operator()()
        {
            if (!mParam || m_pts->size() != m_weights->size() ||
                !std::isfinite(mParam->m_radius) ||
                mParam->m_radius <= 0.0f ||
                mParam->m_iterate < 1 ||
                mParam->m_iterate > kMaximumAvoidIterations ||
                mParam->m_type < 0 || mParam->m_type > 3 ||
                !isFiniteDPoint(mParam->m_targetPt)) {
                return MS::kInvalidParameter;
            }

            std::size_t pointCount = 0U;
            for (const DPointList* points : *m_pts) {
                if (!points || !dommy::generation::checkedSum(pointCount, points->size(),
                        dommy::generation::kMaxGeneratedPointCount, pointCount))
                    return MS::kInvalidParameter;
            }
            if (!dommy::point::validPointForceTargetBudget(pointCount, m_pts->size(),
                    mParam->m_type, mParam->m_type == 0 ? 1U : static_cast<std::size_t>(mParam->m_iterate),
                    mParam->m_targetPts, mParam->m_dcdata)) return MS::kInvalidParameter;

            for (std::size_t listIndex = 0U;
                 listIndex < m_pts->size();
                 ++listIndex) {
                if (!(*m_pts)[listIndex] ||
                    (*m_pts)[listIndex]->size() !=
                        (*m_weights)[listIndex].size()) {
                    return MS::kInvalidParameter;
                }
                for (const DPoint& point : *(*m_pts)[listIndex]) {
                    if (!isFiniteDPoint(point)) {
                        return MS::kInvalidParameter;
                    }
                }
                for (const DScalarData weight :
                     (*m_weights)[listIndex]) {
                    if (!std::isfinite(weight)) {
                        return MS::kInvalidParameter;
                    }
                }
            }

            if (mParam->m_type == 2) {
                if (!mParam->m_targetPts ||
                    mParam->radiusList.size() !=
                        mParam->m_targetPts->size()) {
                    return MS::kInvalidParameter;
                }
                for (std::size_t index = 0U;
                     index < mParam->m_targetPts->size();
                     ++index) {
                    if (!isFiniteDPoint((*mParam->m_targetPts)[index]) ||
                        !std::isfinite(mParam->radiusList[index]) ||
                        mParam->radiusList[index] <= 0.0f) {
                        return MS::kInvalidParameter;
                    }
                }
            } else if (mParam->m_type == 3) {
                if (!mParam->m_dcdata ||
                    mParam->radiusList.size() !=
                        mParam->m_dcdata->numberOfCurves()) {
                    return MS::kInvalidParameter;
                }
                for (const DScalarData radius : mParam->radiusList) {
                    if (!std::isfinite(radius) || radius <= 0.0f) {
                        return MS::kInvalidParameter;
                    }
                }
            }

            switch (mParam->m_type) {
                case 0://target point
                {
                    for (size_t n=0,N=m_pts->size(); n<N; ++n) {
                        std::atomic<bool> succeeded(true);
                        AvoidPointOp op(
                            *mParam, *(*m_pts)[n], (*m_weights)[n],
                            succeeded);
                        
                        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                        tbb::parallel_for( range, op );
                        if (!succeeded.load(std::memory_order_relaxed)) {
                            return MS::kFailure;
                        }
                    }
                }
                    break;
                case 1://sorce object
                {
                    if(mParam->m_objectType == MFnData::kNurbsCurve) {//Nurbs Curve Object
                        
                        
                        for (size_t n=0,N=m_pts->size(); n<N; ++n) {
                            AvoidCurveOp op(*mParam, *(*m_pts)[n], (*m_weights)[n]);
                            
                            tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                            const MStatus status = op(range);
                            if (!status) return status;
                        }
                        
                    } else if (mParam->m_objectType == MFnData::kNurbsSurface) {//Nurbs Surface Object
                        
                        
                        for (size_t n=0,N=m_pts->size(); n<N; ++n) {
                            AvoidSurfaceOp op(*mParam, *(*m_pts)[n], (*m_weights)[n]);
                            
                            tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                            const MStatus status = op(range);
                            if (!status) return status;
                        }
                        
                        
                    } else if (mParam->m_objectType == MFnData::kMesh) {//Mesh Object
                        
                        
                        for (size_t n=0,N=m_pts->size(); n<N; ++n) {
                            AvoidMeshOp op(*mParam, *(*m_pts)[n], (*m_weights)[n]);
                            
                            
                            tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                            const MStatus status = op(range);
                            if (!status) return status;
                        }
                    } else {
                        return MS::kInvalidParameter;
                    }
                }
                    break;
                case 2://sorce DP
                {
                    
                    
                    for (size_t n=0,N=m_pts->size(); n<N; ++n) {
                        std::atomic<bool> succeeded(true);
                        AvoidDPOp op(
                            *mParam, *(*m_pts)[n], (*m_weights)[n],
                            succeeded);
                        op.m_radius = &mParam->radiusList;
                        
                        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                        tbb::parallel_for( range, op );
                        if (!succeeded.load(std::memory_order_relaxed)) {
                            return MS::kFailure;
                        }
                    }
                    
                }
                    break;
                case 3://sorce DC
                {
                    std::vector<MObject> crvObjects;
                    
//                    cout << "dcurve count:" << mParam->m_dcdata->numberOfCurves() << endl;
                    for (auto cIt=mParam->m_dcdata->beginCurves(),ecIt=mParam->m_dcdata->endCurves(); cIt!=ecIt; ++cIt) {
                        //read curve
                        
                        MObject crvObj;
                        const MStatus status =
                            generateMCurveFromDcurve(*cIt, crvObj);
                        if (!status || crvObj.isNull()) {
                            return status ? MStatus(MS::kFailure) : status;
                        }
//                        MFnNurbsCurve crvFn(crvObj);
                        
//                        MFnNurbsCurve crvFn;
//                        MObject crvObj = MFnNurbsCurveData().create();
//                        MFnNurbsCurve::Form isOpen = (*cIt)->isCurveOpen()?MFnNurbsCurve::Form::kOpen:MFnNurbsCurve::Form::kClosed;
//                        crvFn.create((*cIt)->getCvs(), (*cIt)->getKnots(), (*cIt)->getDegree(), isOpen, false, false, crvObj, &stat);
//                        CHECK_MSTATUS(stat);
                        
                        crvObjects.push_back(crvObj);
                    }
//                    cout << "crvObjects count:" << crvObjects.size() << endl;
                    
                    
                    for (size_t n=0,N=m_pts->size(); n<N; ++n) {
                        
                        AvoidDCOp op(*mParam, *(*m_pts)[n], (*m_weights)[n], crvObjects);
                        op.m_radius = &mParam->radiusList;
                        
                        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, (*m_pts)[n]->size(), 10000);
                        const MStatus status = op(range);
                        if (!status) return status;
                        
                    }
                    
                }
                    break;
                default:
                    return MS::kInvalidParameter;
            }

            for (const DPointList* points : *m_pts) {
                for (const DPoint& point : *points) {
                    if (!isFiniteDPoint(point)) return MS::kFailure;
                }
            }
            return MS::kSuccess;
        }
        
        
        std::vector<DPointList* > *m_pts;
        const std::vector<DScalarDataList > *m_weights;
//        std::vector<std::vector<double> > *m_radius;
        
        AvoidParams *mParam;
    }; //AvoidGen
    
    
}// Utilities


#pragma mark DPAvoid class


const MTypeId DPAvoid::typeId( ID_DP_AVOID );


MObject DPAvoid::inDD;

MObject DPAvoid::aRadius;
MObject DPAvoid::aRadiusAtribute;
MObject DPAvoid::aIterate;

MObject DPAvoid::aAvoidType;
MObject DPAvoid::aTargetPoint;
MObject DPAvoid::inTargetObject;
MObject DPAvoid::inTargetDP;
MObject DPAvoid::inTargetDC;

MObject DPAvoid::aWeightAttributeName;
MObject DPAvoid::aWeight;

MObject DPAvoid::outDD;



void *DPAvoid::creator() {
    return ::dommy::maya::createObject("DPAvoid", [&]() {
        return new DPAvoid();
    });
}



MStatus DPAvoid::evalMagnetParms(MDataBlock& data, AvoidParams& parms) {
    MStatus stat;

    MDataHandle handle = data.inputValue(aRadius, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_radius = handle.asFloat();
    handle = data.inputValue(aIterate, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_iterate = handle.asInt();
    handle = data.inputValue(aAvoidType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_type = handle.asShort();
    if (!std::isfinite(parms.m_radius) || parms.m_radius <= 0.0f ||
        parms.m_iterate < 1 ||
        parms.m_iterate > kMaximumAvoidIterations ||
        parms.m_type < 0 || parms.m_type > 3) {
        return MS::kInvalidParameter;
    }

    handle = data.inputValue(aTargetPoint, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float3& targetPointVal = handle.asFloat3();
    if (!stat ||
        !std::isfinite(targetPointVal[0]) ||
        !std::isfinite(targetPointVal[1]) ||
        !std::isfinite(targetPointVal[2])) {
        return stat ? MStatus(MS::kInvalidParameter) : stat;
    }
    parms.m_targetPt =  DPoint(targetPointVal[0],targetPointVal[1],targetPointVal[2]);
    MDataHandle objectHandle = data.inputValue(inTargetObject, &stat);
    if (!stat) return stat;
    parms.m_object = objectHandle.data();
    parms.m_objectType = objectHandle.type();
    return stat;
}


MStatus DPAvoid::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDD, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::point::PointForceInput input;
    DOMMY_RETURN_IF_FAILURE(input.bind(inputHandle));

    AvoidParams params;
    DOMMY_RETURN_IF_FAILURE(evalMagnetParms(data, params));
    MDataHandle handle = data.inputValue(aWeightAttributeName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string weightName = handle.asString().asChar();
    handle = data.inputValue(aWeight, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float weight = handle.asFloat();
    handle = data.inputValue(aRadiusAtribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string radiusName = handle.asString().asChar();
    if (!std::isfinite(weight)) return MS::kInvalidParameter;
    if (params.m_type == 1 && params.m_object.isNull()) return MS::kFailure;

    const DPData* targetPoints = nullptr;
    const DCurveData* targetCurves = nullptr;
    if (params.m_type == 2) {
        handle = data.inputValue(inTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, targetPoints));
        if (!targetPoints || !targetPoints->hasValidPointTopology()) return MS::kInvalidParameter;
        params.m_targetPts = targetPoints->getPositionPtrConst();
    } else if (params.m_type == 3) {
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inTargetDC, data, targetCurves));
        params.m_dcdata = targetCurves;
    }

    // Point-target mode historically performs a single displacement regardless
    // of Iterate. Other modes perform ordered, iterative closest-point queries.
    const std::size_t iterations = params.m_type == 0 ? 1U : static_cast<std::size_t>(params.m_iterate);
    if (!dommy::point::validPointForceTargetBudget(
            input.pointCount, input.partitionCount, params.m_type, iterations,
            params.m_targetPts, params.m_dcdata)) return MS::kInvalidParameter;
    if (targetPoints && !dommy::point::readPointForceScalars(
            targetPoints->getAttr(radiusName), targetPoints->numberOfPoints(),
            params.m_radius, params.radiusList)) return MS::kInvalidParameter;
    if (targetCurves && !dommy::point::readPointForceScalars(
            targetCurves->getAttr(radiusName), targetCurves->numberOfCurves(),
            params.m_radius, params.radiusList)) return MS::kInvalidParameter;

    status = dommy::point::evaluatePointForceOutput(
        input, data, outDD, weightName, weight,
        [&](std::vector<DPointList*>& positions,
            const std::vector<DScalarDataList>& weights) {
            AvoidGen avGen(positions, weights);
            avGen.mParam = &params;
            DOMMY_RETURN_IF_FAILURE(avGen());
            return MStatus(MS::kSuccess);
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPAvoid")

        
        


MStatus DPAvoid::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    MFnGenericAttribute outAttr;
    
    
    // ----------------------- INPUTS --------------------------
    
    inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
    
    aRadius = nAttr.create( "Radius", "rad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    
    const MObject aRadiusAtributeDefault = fnStringData.create("rad", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAtribute = tAttr.create("RadiusAtribute", "radattr", MFnData::kString, aRadiusAtributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAtribute));
    
    aIterate = nAttr.create( "Iterate", "itr", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(kMaximumAvoidIterations));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIterate));
    
    
    MFnEnumAttribute enumFn;
    aAvoidType = enumFn.create( "AvoidType", "avgty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Traget Point", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Object", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DPoint", 2 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DCurve", 3 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAvoidType));
    
    
    
    aTargetPoint = nAttr.create( "TargetPoint", "tpt", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetPoint));
    
    inTargetObject = gAttr.create( "inTargetObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetObject));
    
    
    inTargetDP = tAttr.create("inTargetDP", "intdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDP));
    
    inTargetDC = tAttr.create("inTargetDC", "intdc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDC));
    
    
    
    const MObject aWeightAttributeNameDefault = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttributeName = tAttr.create("WeightAttributeName", "weiattrn", MFnData::kString, aWeightAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttributeName));
    
    aWeight = nAttr.create( "Weight", "wei", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));
    
    
    
    
    
    
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
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadius, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusAtribute, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIterate, outDD ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAvoidType, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetPoint, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetObject, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDP, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDC, outDD ));
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttributeName, outDD ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDD ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DPAvoid", "initialize", std::current_exception());
}
