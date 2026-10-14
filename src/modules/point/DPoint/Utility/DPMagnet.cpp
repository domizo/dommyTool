//
//  DPMagnet.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/26.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPMagnet.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "DPoint/Common/DPointForceOutput.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointDeformMath.h"

#include "DNodeOperation.h"


#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

#pragma mark Utilities Magnet

namespace {

    bool isFiniteDPoint(const DPoint& point)
    {
        return std::isfinite(static_cast<double>(point[0])) &&
               std::isfinite(static_cast<double>(point[1])) &&
               std::isfinite(static_cast<double>(point[2]));
    }

    MStatus effectiveWeight(const MagneteParams& params,
                            const DScalarData rawWeight,
                            double& result)
    {
        if (!std::isfinite(static_cast<double>(rawWeight)) ||
            !std::isfinite(static_cast<double>(params.m_magnitude))) {
            return MS::kInvalidParameter;
        }

        double candidate =
            static_cast<double>(params.m_magnitude) *
            static_cast<double>(rawWeight);
        if (!std::isfinite(candidate)) return MS::kFailure;

        if (params.isClamp) {
            if (!std::isfinite(static_cast<double>(params.mMinClamp)) ||
                !std::isfinite(static_cast<double>(params.mMaxClamp))) {
                return MS::kInvalidParameter;
            }
            const double minimum = std::min(
                static_cast<double>(params.mMinClamp),
                static_cast<double>(params.mMaxClamp));
            const double maximum = std::max(
                static_cast<double>(params.mMinClamp),
                static_cast<double>(params.mMaxClamp));
            candidate = std::max(minimum, std::min(maximum, candidate));
        }

        result = candidate;
        return MS::kSuccess;
    }

    MStatus movePointToward(DPoint& point,
                            const DPoint& target,
                            const MagneteParams& params,
                            const DScalarData rawWeight)
    {
        double weight = 0.0;
        MStatus status = effectiveWeight(params, rawWeight, weight);
        if (!status) return status;

        DPoint candidate;
        if (!dommy::point::moveToward(point, target, weight, candidate)) {
            return MS::kFailure;
        }
        point = candidate;
        return MS::kSuccess;
    }

    MStatus validatePointWeights(const DPointList& points,
                                 const DScalarDataList& weights)
    {
        if (points.size() != weights.size()) return MS::kInvalidParameter;
        for (std::size_t index = 0U; index < points.size(); ++index) {
            if (!isFiniteDPoint(points[index]) ||
                !std::isfinite(static_cast<double>(weights[index]))) {
                return MS::kInvalidParameter;
            }
        }
        return MS::kSuccess;
    }
    
    class MagnetPointOp
    {
    public:
        
		MagnetPointOp(const MagneteParams &param,
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
                if (!movePointToward(
                        (*m_pts)[i], mParam->m_targetPt,
                        *mParam, (*m_weight)[i])) {
                    m_succeeded->store(false, std::memory_order_relaxed);
                    return;
                }
            }
        }
        
    private:
		const MagneteParams *mParam;
        DPointList *m_pts;
		const DScalarDataList *m_weight;
        std::atomic<bool> *m_succeeded;
    };//MagnetPointOp
    
    class MagnetCurveOp
    {
    public:
        
        MagnetCurveOp(
            const MagneteParams& param,
            DPointList& pts,
            const DScalarDataList& weight)
        : mParam(&param)
        , m_pts(&pts)
		, m_weight(&weight)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsCurve crvFn(mParam->m_targetObject, &status);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                const MPoint closestPoint = crvFn.closestPoint(
                    point, NULL, kMFnNurbsEpsilon,
                    MSpace::kWorld, &status);
                if (!status ||
                    !dommy::maya::isFloatRepresentablePoint(closestPoint)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                status = movePointToward(
                    (*m_pts)[i], MPToDP(closestPoint),
                    *mParam, (*m_weight)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        const MagneteParams *mParam;
        DPointList *m_pts;
		const DScalarDataList *m_weight;
    };//MagnetCurveOp
    
    class MagnetSurfaceOp
    {
    public:
        
        MagnetSurfaceOp(
            const MagneteParams& param,
            DPointList& pts,
            const DScalarDataList& weight)
        : mParam(&param)
        ,m_pts(&pts)
		, m_weight(&weight)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsSurface surfFn(mParam->m_targetObject, &status);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                double parameterU = 0.0;
                double parameterV = 0.0;
                const MPoint closestPoint = surfFn.closestPoint(
                    point, &parameterU, &parameterV, false, 0.001,
                    MSpace::kWorld, &status);
                if (!status ||
                    !dommy::maya::isFloatRepresentablePoint(closestPoint) ||
                    !std::isfinite(parameterU) || !std::isfinite(parameterV)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                status = movePointToward(
                    (*m_pts)[i], MPToDP(closestPoint),
                    *mParam, (*m_weight)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        const MagneteParams *mParam;
        DPointList *m_pts;
		const DScalarDataList *m_weight;
    };//MagnetSurfaceOp
    
    class MagnetMeshOp
    {
    public:
        
        MagnetMeshOp(
            const MagneteParams& param,
            const dommy::maya::MeshClosestPointQuery& query,
            DPointList& pts,
            const DScalarDataList& weight)
        : mParam(&param)
        , m_query(&query)
        ,m_pts(&pts)
		, m_weight(&weight)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point = DPToMP((*m_pts)[i]);
                MPoint closestPoint;
                MStatus status =
                    m_query->getClosestPoint(point, closestPoint);
                if (!status ||
                    !dommy::maya::isFloatRepresentablePoint(closestPoint)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                status = movePointToward(
                    (*m_pts)[i], MPToDP(closestPoint),
                    *mParam, (*m_weight)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        const MagneteParams *mParam;
        const dommy::maya::MeshClosestPointQuery *m_query;
        DPointList *m_pts;
		const DScalarDataList *m_weight;
		
    };//MagnetMeshOp
    
    
    class MagnetDPOp
    {
    public:
        
        MagnetDPOp(const MagneteParams &param,
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

                double nearestDistance =
                    std::numeric_limits<double>::max();
                std::size_t nearestId = 0U;
                for (std::size_t targetIndex = 0U;
                     targetIndex < mParam->m_targetPts->size();
                     ++targetIndex) {
                    dommy::direction::Delta delta;
                    if (!dommy::direction::difference(
                            (*m_pts)[i],
                            (*mParam->m_targetPts)[targetIndex],
                            delta)) {
                        m_succeeded->store(
                            false, std::memory_order_relaxed);
                        return;
                    }
                    if (delta.length < nearestDistance) {
                        nearestDistance = delta.length;
                        nearestId = targetIndex;
                    }
                }
                if (!movePointToward(
                        (*m_pts)[i], (*mParam->m_targetPts)[nearestId],
                        *mParam, (*m_weight)[i])) {
                    m_succeeded->store(false, std::memory_order_relaxed);
                    return;
                }
            }
            
        }
        
        
        
    private:
		const MagneteParams *mParam;
        DPointList *m_pts;
		const DScalarDataList *m_weight;
        std::atomic<bool> *m_succeeded;
    };//MagnetDPOp
	
	
	
	class MagnetDCOp
	{
	public:
		
		MagnetDCOp(const MagneteParams &param,
                   DPointList& pts,
                   const DScalarDataList &weight,
                   const std::vector<MObject> &crvObjects)
		: mParam(&param)
		,m_pts(&pts)
		, m_weight(&weight)
		, m_crvObjects(&crvObjects)
		{ }
		
		MStatus operator()(const tbb::blocked_range<size_t>& r) const
		{
            if (m_crvObjects->empty()) return MS::kInvalidParameter;

			for (size_t i = r.begin(); i != r.end(); i++) {
				const MPoint point = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }

				double nearestDistance =
                    std::numeric_limits<double>::max();
				MPoint nearestPoint;
                bool found = false;

				for (std::vector<MObject>::const_iterator iterator =
                         m_crvObjects->begin();
                     iterator != m_crvObjects->end();
                     ++iterator) {
                    MStatus status;
					MFnNurbsCurve curve(*iterator, &status);
                    if (!status) return status;

					const double distance = curve.distanceToPoint(
                        point, MSpace::kWorld, &status);
                    if (!status || !std::isfinite(distance)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }
					if (distance < nearestDistance) {
                        const MPoint candidate = curve.closestPoint(
                            point, NULL, kMFnNurbsEpsilon,
                            MSpace::kWorld, &status);
                        if (!status ||
                            !dommy::maya::isFloatRepresentablePoint(candidate)) {
                            return status ? MStatus(MS::kFailure) : status;
                        }
						nearestDistance = distance;
						nearestPoint = candidate;
                        found = true;
					}
				}

                if (!found) return MS::kFailure;
                MStatus status = movePointToward(
                    (*m_pts)[i], MPToDP(nearestPoint),
                    *mParam, (*m_weight)[i]);
                if (!status) return status;
			}
            return MS::kSuccess;
		}
		
		
		
	private:
		const MagneteParams *mParam;
		DPointList *m_pts;
		const DScalarDataList *m_weight;
		const std::vector<MObject> *m_crvObjects;
	};//MagnetDCOp
	
	
	
	
	
    class MagnetGen
    {
    public:
        MagnetGen(const MagneteParams& param,
                  std::vector<DPointList*>& pts,
                  const std::vector<DScalarDataList>& weights)
        : m_pts(&pts)
        , m_weights(&weights)
        , mParam(&param)
        { }

        MStatus operator()() const
        {
            if (m_pts->size() != m_weights->size()) {
                return MS::kInvalidParameter;
            }
            std::size_t pointCount = 0U;
            for (const DPointList* points : *m_pts) {
                if (!points || !dommy::generation::checkedSum(pointCount, points->size(),
                        dommy::generation::kMaxGeneratedPointCount, pointCount))
                    return MS::kInvalidParameter;
            }
            if (!dommy::point::validPointForceTargetBudget(pointCount, m_pts->size(),
                    mParam->m_type, 1U, mParam->m_targetPts, mParam->m_dcdata))
                return MS::kInvalidParameter;
            for (std::size_t groupIndex = 0U;
                 groupIndex < m_pts->size();
                 ++groupIndex) {
                if (!(*m_pts)[groupIndex]) return MS::kInvalidParameter;
                const MStatus status = validatePointWeights(
                    *(*m_pts)[groupIndex], (*m_weights)[groupIndex]);
                if (!status) return status;
            }

            switch (mParam->m_type) {
                case 0: {
                    if (!isFiniteDPoint(mParam->m_targetPt)) {
                        return MS::kInvalidParameter;
                    }
                    for (size_t n = 0U; n < m_pts->size(); ++n) {
                        std::atomic<bool> succeeded(true);
                        MagnetPointOp op(
                            *mParam, *(*m_pts)[n],
                            (*m_weights)[n], succeeded);
                        tbb::parallel_for(
                            tbb::blocked_range<size_t>(
                                0U, (*m_pts)[n]->size(), 10000U),
                            op);
                        if (!succeeded.load(std::memory_order_relaxed)) {
                            return MS::kFailure;
                        }
                    }
                    return MS::kSuccess;
                }
                case 1: {
                    const MFnData::Type objectType =
                        mParam->m_targetObjectType;
                    if (objectType == MFnData::kNurbsCurve) {
                        for (size_t n = 0U; n < m_pts->size(); ++n) {
                            MagnetCurveOp op(
                                *mParam, *(*m_pts)[n], (*m_weights)[n]);
                            const MStatus status = op(
                                tbb::blocked_range<size_t>(
                                    0U, (*m_pts)[n]->size(), 10000U));
                            if (!status) return status;
                        }
                        return MS::kSuccess;
                    }
                    if (objectType == MFnData::kNurbsSurface) {
                        for (size_t n = 0U; n < m_pts->size(); ++n) {
                            MagnetSurfaceOp op(
                                *mParam, *(*m_pts)[n], (*m_weights)[n]);
                            const MStatus status = op(
                                tbb::blocked_range<size_t>(
                                    0U, (*m_pts)[n]->size(), 10000U));
                            if (!status) return status;
                        }
                        return MS::kSuccess;
                    }
                    if (objectType == MFnData::kMesh) {
                        dommy::maya::MeshClosestPointQuery query;
                        MStatus status = query.create(
                            mParam->m_targetObject);
                        if (!status) return status;
                        for (size_t n = 0U; n < m_pts->size(); ++n) {
                            MagnetMeshOp op(
                                *mParam, query, *(*m_pts)[n],
                                (*m_weights)[n]);
                            status = op(tbb::blocked_range<size_t>(
                                0U, (*m_pts)[n]->size(), 10000U));
                            if (!status) return status;
                        }
                        return MS::kSuccess;
                    }
                    return MS::kInvalidParameter;
                }
                case 2: {
                    if (!mParam->m_targetPts ||
                        mParam->m_targetPts->empty()) {
                        return MS::kInvalidParameter;
                    }
                    for (DPointList::const_iterator iterator =
                             mParam->m_targetPts->begin();
                         iterator != mParam->m_targetPts->end();
                         ++iterator) {
                        if (!isFiniteDPoint(*iterator)) {
                            return MS::kInvalidParameter;
                        }
                    }
                    for (size_t n = 0U; n < m_pts->size(); ++n) {
                        std::atomic<bool> succeeded(true);
                        MagnetDPOp op(
                            *mParam, *(*m_pts)[n],
                            (*m_weights)[n], succeeded);
                        tbb::parallel_for(
                            tbb::blocked_range<size_t>(
                                0U, (*m_pts)[n]->size(), 10000U),
                            op);
                        if (!succeeded.load(std::memory_order_relaxed)) {
                            return MS::kFailure;
                        }
                    }
                    return MS::kSuccess;
                }
                case 3: {
                    if (!mParam->m_dcdata ||
                        !mParam->m_dcdata->numberOfCurves()) {
                        return MS::kInvalidParameter;
                    }

                    std::vector<MObject> curveObjects;
                    for (auto iterator = mParam->m_dcdata->beginCurves();
                         iterator != mParam->m_dcdata->endCurves();
                         ++iterator) {
                        MStatus status;
                        MFnNurbsCurveData curveData;
                        MObject curveObject = curveData.create(&status);
                        if (!status || curveObject.isNull()) {
                            return status ? MStatus(MS::kFailure) : status;
                        }

                        MFnNurbsCurve curve;
                        const MFnNurbsCurve::Form form =
                            (*iterator)->isCurveOpen()
                                ? MFnNurbsCurve::kOpen
                                : MFnNurbsCurve::kClosed;
                        curve.create(
                            (*iterator)->getCvs(), (*iterator)->getKnots(),
                            (*iterator)->getDegree(), form,
                            false, false, curveObject, &status);
                        if (!status) return status;
                        curveObjects.push_back(curveObject);
                    }

                    for (size_t n = 0U; n < m_pts->size(); ++n) {
                        MagnetDCOp op(
                            *mParam, *(*m_pts)[n],
                            (*m_weights)[n], curveObjects);
                        const MStatus status = op(
                            tbb::blocked_range<size_t>(
                                0U, (*m_pts)[n]->size(), 10000U));
                        if (!status) return status;
                    }
                    return MS::kSuccess;
                }
                default:
                    return MS::kInvalidParameter;
            }
        }

        std::vector<DPointList* > *m_pts;
        const std::vector<DScalarDataList > *m_weights;
        const MagneteParams *mParam;
    };

    MStatus configureMagnetTarget(MagneteParams& params, MDataBlock& data)
    {
        if (params.m_type == 2) {
            MStatus status;
            const MDataHandle handle = data.inputValue(DPMagnet::inTargetDP, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const DPData* targetData = nullptr;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, targetData));
            if (!targetData || !targetData->hasValidPointTopology()) return MS::kInvalidParameter;
            params.m_targetPts = targetData->getPositionPtrConst();
            return params.m_targetPts && !params.m_targetPts->empty()
                ? MStatus(MS::kSuccess)
                : MStatus(MS::kInvalidParameter);
        }
        if (params.m_type == 3) {
            const DCurveData* targetData = nullptr;
            const MStatus status = getValidInputDC(
                DPMagnet::inTargetDC, data, targetData);
            if (!status) return status;
            params.m_dcdata = targetData;
            return params.m_dcdata && params.m_dcdata->numberOfCurves()
                ? MStatus(MS::kSuccess)
                : MStatus(MS::kInvalidParameter);
        }
        return MS::kSuccess;
    }
    
    
}// Utilities





#pragma mark DPMagnet class


const MTypeId DPMagnet::typeId( ID_DP_MAGNET );


MObject DPMagnet::inDD;

MObject DPMagnet::aMagnitude;

MObject DPMagnet::aMagnetType;

MObject DPMagnet::aIsClamp;
MObject DPMagnet::aClampRange;

MObject DPMagnet::aTargetPoint;

MObject DPMagnet::inTargetObject;
MObject DPMagnet::inTargetDP;
MObject DPMagnet::inTargetDC;

MObject DPMagnet::aWeightAttributeName;
MObject DPMagnet::aWeight;

MObject DPMagnet::outDD;



void *DPMagnet::creator() {
    return ::dommy::maya::createObject("DPMagnet", [&]() {
        return new DPMagnet();
    });
}

MStatus DPMagnet::evalMagnetParms(MDataBlock& data, MagneteParams& parms) {
    MStatus status;

    MDataHandle handle = data.inputValue(aMagnitude, &status);
    if (!status) return status;
    parms.m_magnitude = handle.asFloat();

    handle = data.inputValue(aMagnetType, &status);
    if (!status) return status;
    parms.m_type = handle.asShort();
    if (parms.m_type < 0 || parms.m_type > 3) {
        return MS::kInvalidParameter;
    }

    handle = data.inputValue(aTargetPoint, &status);
    if (!status) return status;
    const float3& targetPointValue = handle.asFloat3();
    parms.m_targetPt = DPoint(
        targetPointValue[0], targetPointValue[1], targetPointValue[2]);

    handle = data.inputValue(aIsClamp, &status);
    if (!status) return status;
    parms.isClamp = handle.asBool();

    handle = data.inputValue(aClampRange, &status);
    if (!status) return status;
    const float2& clampRange = handle.asFloat2();
    parms.mMinClamp = clampRange[0];
    parms.mMaxClamp = clampRange[1];

    handle = data.inputValue(aWeightAttributeName, &status);
    if (!status) return status;
    parms.m_weight_attr = handle.asString().asChar();

    handle = data.inputValue(aWeight, &status);
    if (!status) return status;
    parms.m_global_weight = handle.asFloat();

    if (parms.m_type == 1) {
        MDataHandle objectHandle =
            data.inputValue(inTargetObject, &status);
        if (!status || objectHandle.data().isNull()) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        const MFnData::Type type = objectHandle.type();
        if (type != MFnData::kMesh &&
            type != MFnData::kNurbsCurve &&
            type != MFnData::kNurbsSurface) {
            return MS::kInvalidParameter;
        }
        parms.m_targetObject = objectHandle.data();
        parms.m_targetObjectType = type;
    }

    if (!std::isfinite(static_cast<double>(parms.m_magnitude)) ||
        !std::isfinite(static_cast<double>(parms.m_global_weight)) ||
        !isFiniteDPoint(parms.m_targetPt) ||
        (parms.isClamp &&
         (!std::isfinite(static_cast<double>(parms.mMinClamp)) ||
          !std::isfinite(static_cast<double>(parms.mMaxClamp))))) {
        return MS::kInvalidParameter;
    }

    return MS::kSuccess;
}


MStatus DPMagnet::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDD, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::point::PointForceInput input;
    DOMMY_RETURN_IF_FAILURE(input.bind(inputHandle));

    MagneteParams params;
    DOMMY_RETURN_IF_FAILURE(evalMagnetParms(data, params));
    DOMMY_RETURN_IF_FAILURE(configureMagnetTarget(params, data));
    if (!dommy::point::validPointForceTargetBudget(
            input.pointCount, input.partitionCount, params.m_type, 1U,
            params.m_targetPts, params.m_dcdata)) return MS::kInvalidParameter;

    status = dommy::point::evaluatePointForceOutput(
        input, data, outDD, params.m_weight_attr, params.m_global_weight,
        [&](std::vector<DPointList*>& positions,
            const std::vector<DScalarDataList>& weights) {
            MagnetGen generator(params, positions, weights);
            return generator();
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPMagnet")




MStatus DPMagnet::initialize() try {
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
    
    aMagnitude = nAttr.create( "Magnitude", "mag", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMagnitude));
	
	
	aIsClamp = nAttr.create( "IsClamp", "isclp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsClamp));
	
    aClampRange = nAttr.create( "ClampRange", "clrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aClampRange));

	
	
    
    
    MFnEnumAttribute enumFn;
    aMagnetType = enumFn.create( "MagnetType", "mgty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Traget Point", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Object", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DPoint", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DCurve", 3 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMagnetType));
    
    
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
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aMagnetType, outDD ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMagnitude, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsClamp, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aClampRange, outDD ));
	
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetPoint, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetObject, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDP, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDC, outDD ));
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeightAttributeName, outDD ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outDD ) );
	
	
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DPMagnet", "initialize", std::current_exception());
}
