//
//  DPDistant.cpp
//  dommyTool
//
//  Created by yuzo on 3/11/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPDistance.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"
#include "DPoint/Common/DPointForceOutput.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"



#include "DNodeOperation.h"


#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <atomic>
#include <cmath>
#include <limits>

// Utilities

namespace {

    bool isFiniteDPoint(const DPoint& point)
    {
        return std::isfinite(static_cast<double>(point[0])) &&
               std::isfinite(static_cast<double>(point[1])) &&
               std::isfinite(static_cast<double>(point[2]));
    }

    MStatus pointDistance(const DPoint& first,
                          const DPoint& second,
                          DScalarData& result)
    {
        dommy::direction::Delta delta;
        float candidate = 0.0f;
        if (!dommy::direction::difference(first, second, delta) ||
            !dommy::numeric::checkedFloatCast(delta.length, candidate)) {
            return MS::kInvalidParameter;
        }
        result = candidate;
        return MS::kSuccess;
    }

    MStatus pointDistance(const MPoint& first,
                          const MPoint& second,
                          DScalarData& result)
    {
        if (!dommy::maya::isFinitePoint(first) ||
            !dommy::maya::isFinitePoint(second)) {
            return MS::kInvalidParameter;
        }
        float candidate = 0.0f;
        if (!dommy::numeric::checkedFloatCast(
                first.distanceTo(second), candidate)) {
            return MS::kFailure;
        }
        result = candidate;
        return MS::kSuccess;
    }

    struct DistanceTarget
    {
        DistanceTarget()
            : type(0),
              point(0.0f, 0.0f, 0.0f),
              object(MObject::kNullObj),
              objectType(MFnData::kInvalid),
              points(NULL)
        {
        }

        short type;
        DPoint point;
        MObject object;
        MFnData::Type objectType;
        const DPointList* points;
    };
    
    // Shared by the compute allocation boundary and the standalone dispatcher.
    // Native queries count as one visit; their internal Maya cost is not bounded.
    bool validDistanceBudget(const DistanceTarget& target,
                             std::size_t queries, std::size_t partitions)
    {
        if (target.type < 0 || target.type > 2) return false;
        const std::size_t targets = target.type == 2 && target.points ?
            target.points->size() : 1U;
        if (target.type == 2 && (!target.points || target.points->empty() ||
            targets > static_cast<std::size_t>(std::numeric_limits<int>::max())))
            return false;
        return dommy::point::validPointForceBudget(queries, partitions, targets, 1U) &&
            dommy::attribute::validArrayConversionBudget(queries,
                sizeof(DPoint) + 2U * (sizeof(float) + sizeof(int)));
    }

    class DistanceBetweenPointOp
    {
    public:
        
        DistanceBetweenPointOp(
            const DPoint& targetPt,
            const DPointList& pts,
            DScalarDataList& vals,
            std::atomic<bool>& succeeded)
        : m_targetPt(targetPt)
        ,m_pts(&pts)
        , m_vals(&vals)
        , m_succeeded(&succeeded)
        { }
        
        void operator()(const tbb::blocked_range<size_t>& r) const
        {
            for (size_t i = r.begin(); i != r.end(); i++) {
                if (!m_succeeded->load(std::memory_order_relaxed)) return;
                if (!pointDistance(
                        m_targetPt, (*m_pts)[i], (*m_vals)[i])) {
                    m_succeeded->store(false, std::memory_order_relaxed);
                    return;
                }
            }
        }
        
    private:
        DPoint m_targetPt;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
        std::atomic<bool> *m_succeeded;
        
    };//DistanceBetweenPointOp
    
    class DistanceCurveOp
    {
    public:
        
        DistanceCurveOp(const MObject& obj, const DPointList& pts, DScalarDataList& vals)
        : m_obj(obj)
        ,m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsCurve crvFn(m_obj, &status);
            if (!status) return status;
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point = DPToMP((*m_pts)[i]);
                if (!dommy::maya::isFinitePoint(point)) {
                    return MS::kInvalidParameter;
                }
                const double distance =
                    crvFn.distanceToPoint(point, MSpace::kWorld, &status);
                float candidate = 0.0f;
                if (!status ||
                    !dommy::numeric::checkedFloatCast(
                        distance, candidate)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                (*m_vals)[i] = candidate;
            }
            return MS::kSuccess;
        }
        
    private:
        const MObject m_obj;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
        
    };//DistanceCurveOp
    
    class DistanceSurfaceOp
    {
    public:
        
        DistanceSurfaceOp(const MObject& obj, const DPointList& pts, DScalarDataList& vals)
        : m_obj(obj)
        ,m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            MStatus status;
            MFnNurbsSurface surfFn(m_obj, &status);
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
                if (!status || !std::isfinite(parameterU) ||
                    !std::isfinite(parameterV)) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                status = pointDistance(
                    point, closestPoint, (*m_vals)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        const MObject m_obj;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
        
    };//DistanceSurfaceOp
    
    class DistanceMeshOp
    {
    public:
        
        DistanceMeshOp(
            const dommy::maya::MeshClosestPointQuery& query,
            const DPointList& pts,
            DScalarDataList& vals)
        : m_query(&query)
        , m_pts(&pts)
        , m_vals(&vals)
        { }
        
        MStatus operator()(const tbb::blocked_range<size_t>& r) const
        {
            for (size_t i = r.begin(); i != r.end(); i++) {
                const MPoint point = DPToMP((*m_pts)[i]);
                MPoint closestPoint;
                MStatus status =
                    m_query->getClosestPoint(point, closestPoint);
                if (!status) return status;
                status = pointDistance(
                    point, closestPoint, (*m_vals)[i]);
                if (!status) return status;
            }
            return MS::kSuccess;
        }
        
    private:
        const dommy::maya::MeshClosestPointQuery *m_query;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
        
    };//DistanceMeshOp
    
    
    class DistanceDPOp
    {
    public:
        
        DistanceDPOp(
            const DPointList& targetPts,
            const DPointList& pts,
            DScalarDataList& vals,
            DIntArray& ids,
            std::atomic<bool>& succeeded)
        : m_targetPts(&targetPts)
        ,m_pts(&pts)
        , m_vals(&vals)
		, m_ids(&ids)
        , m_succeeded(&succeeded)
        { }
        
        void operator()(const tbb::blocked_range<size_t>& r) const
        {
            
            for (size_t i = r.begin(); i != r.end(); i++) {
                if (!m_succeeded->load(std::memory_order_relaxed)) return;

                double nearestValue =
                    std::numeric_limits<double>::max();
                std::size_t nearestId = 0U;
                for (size_t j = 0, targetCount = m_targetPts->size();
                     j < targetCount; ++j) {
                    dommy::direction::Delta delta;
                    if (!dommy::direction::difference(
                            (*m_pts)[i], (*m_targetPts)[j], delta)) {
                        m_succeeded->store(
                            false, std::memory_order_relaxed);
                        return;
                    }
                    if (delta.length < nearestValue) {
                        nearestValue = delta.length;
                        nearestId = j;
                    }
                }

                float distance = 0.0f;
                if (!dommy::numeric::checkedFloatCast(
                        nearestValue, distance)) {
                    m_succeeded->store(false, std::memory_order_relaxed);
                    return;
                }
                (*m_vals)[i] = distance;
                (*m_ids)[i] = static_cast<int>(nearestId);
            }
            
        }
        
        
        
    private:
        const DPointList *m_targetPts;
        const DPointList *m_pts;
        DScalarDataList *m_vals;
		DIntArray *m_ids;
        std::atomic<bool> *m_succeeded;
    };//DistanceMeshOp
    
    
    
    class DistanceGen
    {
    public:
        DistanceGen(const DistanceTarget& target,
                    const std::vector<const DPointList*>& pts,
                    std::vector<DScalarDataList>& vals,
                    std::vector<DIntArray>& ids)
        : m_target(&target)
        , m_pts(&pts)
        , m_vals(&vals)
        , m_ids(&ids)
        { }
        
        MStatus operator()() const
        {
            if (!m_pts || !m_vals || m_pts->size() != m_vals->size() ||
                !validDistanceBudget(*m_target, 0U, m_pts->size()))
                return MS::kInvalidParameter;
            std::size_t queryCount = 0U;
            for (size_t n = 0U; n < m_pts->size(); ++n) {
                const DPointList* partition = (*m_pts)[n];
                if (!partition || (*m_vals)[n].size() != partition->size() ||
                    !dommy::generation::checkedSum(queryCount, partition->size(),
                        dommy::generation::kMaxGeneratedPointCount, queryCount))
                    return MS::kInvalidParameter;
            }
            if (!validDistanceBudget(*m_target, queryCount, m_pts->size()))
                return MS::kInvalidParameter;
            for (const DPointList* partition : *m_pts)
                for (const DPoint& point : *partition)
                    if (!isFiniteDPoint(point)) return MS::kInvalidParameter;
            if (m_target->type == 2) {
                if (!m_ids || m_ids->size() != m_pts->size()) return MS::kInvalidParameter;
                for (const DPoint& point : *m_target->points)
                    if (!isFiniteDPoint(point)) return MS::kInvalidParameter;
                for (size_t n = 0U; n < m_pts->size(); ++n)
                    if ((*m_ids)[n].size() != (*m_pts)[n]->size()) return MS::kInvalidParameter;
            }
            
            switch (m_target->type) {
                case 0: {
                    if (!isFiniteDPoint(m_target->point)) {
                        return MS::kInvalidParameter;
                    }
                    for (size_t n = 0U; n < m_pts->size(); ++n) {
                        std::atomic<bool> succeeded(true);
                        DistanceBetweenPointOp op(
                            m_target->point, *(*m_pts)[n],
                            (*m_vals)[n], succeeded);
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
                    if (m_target->object.isNull()) {
                        return MS::kInvalidParameter;
                    }
                    if (m_target->objectType == MFnData::kNurbsCurve) {
                        for (size_t n = 0U; n < m_pts->size(); ++n) {
                            DistanceCurveOp op(
                                m_target->object, *(*m_pts)[n],
                                (*m_vals)[n]);
                            const MStatus status = op(
                                tbb::blocked_range<size_t>(
                                    0U, (*m_pts)[n]->size(), 10000U));
                            if (!status) return status;
                        }
                        return MS::kSuccess;
                    }
                    if (m_target->objectType == MFnData::kNurbsSurface) {
                        for (size_t n = 0U; n < m_pts->size(); ++n) {
                            DistanceSurfaceOp op(
                                m_target->object, *(*m_pts)[n],
                                (*m_vals)[n]);
                            const MStatus status = op(
                                tbb::blocked_range<size_t>(
                                    0U, (*m_pts)[n]->size(), 10000U));
                            if (!status) return status;
                        }
                        return MS::kSuccess;
                    }
                    if (m_target->objectType == MFnData::kMesh) {
                        dommy::maya::MeshClosestPointQuery query;
                        MStatus status = query.create(m_target->object);
                        if (!status) return status;
                        for (size_t n = 0U; n < m_pts->size(); ++n) {
                            DistanceMeshOp op(
                                query, *(*m_pts)[n], (*m_vals)[n]);
                            status = op(tbb::blocked_range<size_t>(
                                0U, (*m_pts)[n]->size(), 10000U));
                            if (!status) return status;
                        }
                        return MS::kSuccess;
                    }
                    return MS::kInvalidParameter;
                }
                case 2: {
                    for (size_t n = 0U; n < m_pts->size(); ++n) {
                        std::atomic<bool> succeeded(true);
                        DistanceDPOp op(
                            *m_target->points, *(*m_pts)[n],
                            (*m_vals)[n], (*m_ids)[n], succeeded);
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
                default:
					return MS::kInvalidParameter;
            }
        }
        
        const DistanceTarget *m_target;
        const std::vector<const DPointList* > *m_pts;
        std::vector<DScalarDataList > *m_vals;
		std::vector<DIntArray > *m_ids;
    };
    
    
}// Utilities






const MTypeId DPDistance::typeId( ID_DP_DISTANCE );


MObject DPDistance::inDD;
MObject DPDistance::aComponentType;
MObject DPDistance::distAttrName;
MObject DPDistance::targetType;

MObject DPDistance::targetPoint;
MObject DPDistance::inObject;
MObject DPDistance::inTargetDP;

MObject DPDistance::aAddNearID;
MObject DPDistance::aNearIDAttributeName;

MObject DPDistance::outDD;



void *DPDistance::creator() {
    return ::dommy::maya::createObject("DPDistance", [&]() {
        return new DPDistance();
    });
}


MStatus DPDistance::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;
    MStatus stat;

    MDataHandle inputHandle = data.inputValue(inDD, &stat);
    if (!stat) return stat;
    dommy::point::PointForceInput input;
    DOMMY_RETURN_IF_FAILURE(input.bind(inputHandle));
    const MObject dataObj = inputHandle.data();

    if (dataObj.isNull()) {
        EchoWarning(name().asChar(), "input data is null.");
        return MS::kFailure;
    }

    MDataHandle handle = data.inputValue(aComponentType, &stat);
    if (!stat) return stat;
    const short compType = handle.asShort();
    if (compType < 0 || compType > 1) {
        return MS::kInvalidParameter;
    }

    handle = data.inputValue(targetType, &stat);
    if (!stat) return stat;
    const short targetTypeVal = handle.asShort();
    if (targetTypeVal < 0 || targetTypeVal > 2) {
        return MS::kInvalidParameter;
    }

    handle = data.inputValue(targetPoint, &stat);
    if (!stat) return stat;
    const double3& targetPointValue = handle.asDouble3();
    float targetComponents[3] = {0.0f, 0.0f, 0.0f};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        if (!dommy::numeric::checkedFloatCast(
                targetPointValue[axis], targetComponents[axis])) {
            return MS::kInvalidParameter;
        }
    }

    handle = data.inputValue(distAttrName, &stat);
    if (!stat) return stat;
    const std::string attrName = handle.asString().asChar();

    handle = data.inputValue(aAddNearID, &stat);
    if (!stat) return stat;
    const bool addNearId = handle.asBool();

    handle = data.inputValue(aNearIDAttributeName, &stat);
    if (!stat) return stat;
    const std::string nearidAttrName = handle.asString().asChar();
    if (attrName.empty() ||
        (addNearId &&
         (nearidAttrName.empty() || nearidAttrName == attrName))) {
        return MS::kInvalidParameter;
    }

    DistanceTarget distanceTarget;
    distanceTarget.type = targetTypeVal;
    distanceTarget.point = DPoint(
        targetComponents[0], targetComponents[1], targetComponents[2]);

    if (targetTypeVal == 1) {
        MDataHandle objectHandle =
            data.inputValue(inObject, &stat);
        if (!stat || objectHandle.data().isNull()) {
            return stat ? MStatus(MS::kInvalidParameter) : stat;
        }
        distanceTarget.objectType = objectHandle.type();
        if (distanceTarget.objectType != MFnData::kMesh &&
            distanceTarget.objectType != MFnData::kNurbsCurve &&
            distanceTarget.objectType != MFnData::kNurbsSurface) {
            return MS::kInvalidParameter;
        }
        distanceTarget.object = objectHandle.data();
    } else if (targetTypeVal == 2) {
        const MDataHandle targetHandle = data.inputValue(inTargetDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const DPData* targetData = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
            targetHandle, DPData::id, targetData));
        if (!targetData || !targetData->hasValidPointTopology())
            return MS::kInvalidParameter;
        distanceTarget.points = targetData->getPositionPtrConst();
        if (!distanceTarget.points ||
            distanceTarget.points->empty() ||
            distanceTarget.points->size() >
                static_cast<std::size_t>(
                    std::numeric_limits<int>::max())) {
            return MS::kInvalidParameter;
        }
    }

    // Count all source partitions before output copies or value/ID arrays.
    // Primitive mode retains its first-local-point query, not a centroid.
    const std::size_t queryCount = input.points || compType == 1 ?
        input.pointCount : input.partitionCount;
    if (!validDistanceBudget(distanceTarget, queryCount, input.partitionCount))
        return MS::kInvalidParameter;

    std::vector<const DPointList*> points;
    DPointList primitivePoints;
    points.reserve(input.points ? 1U : input.partitionCount);
    if (!input.points && compType == 0) primitivePoints.reserve(queryCount);
    const auto appendPartition = [&](const DPointList* positions, std::size_t count) {
        if (!positions || positions->size() != count) return false;
        if (compType == 0) {
            if (positions->empty()) return false;
            primitivePoints.push_back(positions->front());
        } else {
            points.push_back(positions);
        }
        return true;
    };
    if (input.points) {
        points.push_back(input.points->getPositionPtrConst());
    } else if (input.curves) {
        for (auto it = input.curves->beginCurves(); it != input.curves->endCurves(); ++it)
            if (!appendPartition((*it)->getPositionPtrConst(), (*it)->numberOfPoints()))
                return MS::kInvalidParameter;
    } else if (input.meshes) {
        for (auto it = input.meshes->beginMeshes(); it != input.meshes->endMeshes(); ++it)
            if (!appendPartition((*it)->getPositionPtrConst(), (*it)->numberOfPoints()))
                return MS::kInvalidParameter;
    }
    if (!input.points && compType == 0 && input.partitionCount != 0U)
        points.push_back(&primitivePoints);
    std::vector<DScalarDataList> values;
    std::vector<DIntArray> ids;
    values.reserve(points.size());
    ids.reserve(points.size());
    for (const DPointList* partition : points) {
        if (!partition) return MS::kInvalidParameter;
        values.emplace_back(partition->size(), 0.0f);
        ids.emplace_back(partition->size(), -1);
    }
    DistanceGen evaluate(distanceTarget, points, values, ids);
    // Empty curve/mesh containers historically pass through without attributes
    // or native geometry queries. An empty DP still receives an empty field.
    if (!points.empty()) DOMMY_RETURN_IF_FAILURE(evaluate());
    const bool writeIds = addNearId && targetTypeVal == 2;

    if (input.points) {
        stat = dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id,
            [&](DPData& output) {
                output.setDPData(*input.points);
                checkDPAttribute(attrName, output, true);
                output.addAttrWithValues(attrName, values[0]);
                if (writeIds) {
                    checkDPAttribute(nearidAttrName, output, true);
                    output.addAttrWithValues(nearidAttrName, ids[0]);
                }
                return MS::kSuccess;
            });
    } else if (input.curves) {
        stat = dommy::maya::setPluginDataOutput<DCurveData>(data, outDD, DCurveData::id,
            [&](DCurveData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input.curves));
                if (input.partitionCount == 0U) return MStatus(MS::kSuccess);
                if (compType == 0) {
                    checkDDataAttribute(attrName, output, true);
                    output.addAttrWithValues(attrName, values[0]);
                    if (writeIds) {
                        checkDDataAttribute(nearidAttrName, output, true);
                        output.addAttrWithValues(nearidAttrName, ids[0]);
                    }
                } else {
                    checkDDataPointAttribute(attrName, output, true);
                    if (writeIds) checkDDataPointAttribute(nearidAttrName, output, true);
                    std::size_t index = 0U;
                    for (auto it = output.beginCurves(); it != output.endCurves(); ++it, ++index) {
                        (*it)->addAttrWithValues(attrName, values[index]);
                        if (writeIds) (*it)->addAttrWithValues(nearidAttrName, ids[index]);
                    }
                }
                return MStatus(MS::kSuccess);
            });
    } else {
        stat = dommy::maya::setPluginDataOutput<DMeshData>(data, outDD, DMeshData::id,
            [&](DMeshData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input.meshes));
                if (input.partitionCount == 0U) return MStatus(MS::kSuccess);
                if (compType == 0) {
                    checkDDataAttribute(attrName, output, true);
                    output.addDPAttributeValues(attrName, values[0]);
                    if (writeIds) {
                        checkDDataAttribute(nearidAttrName, output, true);
                        output.addDPAttributeValues(nearidAttrName, ids[0]);
                    }
                } else {
                    checkDDataPointAttribute(attrName, output, true);
                    if (writeIds) checkDDataPointAttribute(nearidAttrName, output, true);
                    std::size_t index = 0U;
                    for (auto it = output.beginMeshes(); it != output.endMeshes(); ++it, ++index) {
                        (*it)->addAttrWithValues(attrName, values[index]);
                        if (writeIds) (*it)->addAttrWithValues(nearidAttrName, ids[index]);
                    }
                }
                return MStatus(MS::kSuccess);
            });
    }
    return stat ? data.setClean(plug) : stat;
}
DOMMY_COMPUTE_CATCH("DPDistance")




MStatus DPDistance::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	
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
    
    
    MFnEnumAttribute compEnumFn;
    aComponentType = compEnumFn.create( "ComponentType", "cmpty", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(compEnumFn.addField( "Primitive", 0 ));
    DOMMY_RETURN_IF_FAILURE(compEnumFn.addField( "Point", 1 ));
    DOMMY_RETURN_IF_FAILURE(compEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(compEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aComponentType));
    
    
    MFnStringData fnStringData;
    MObject attrName = fnStringData.create("dist", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    distAttrName = tAttr.create("distAttrName", "dattrn", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(distAttrName));
    
    
    MFnEnumAttribute enumFn;
    targetType = enumFn.create( "targetType", "tt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Traget Point", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Object", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DPoint", 2 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetType));
    
    
    targetPoint = nAttr.create( "targetPoint", "tpt", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetPoint));
    
    inObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));
    
    
    inTargetDP = tAttr.create("inTargetDP", "intdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDP));
	
	
    aAddNearID = nAttr.create( "AddNearID", "adnid", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddNearID));
    
    const MObject aNearIDAttributeNameDefault = fnStringData.create("nearid", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    aNearIDAttributeName = tAttr.create("NearIDAttributeName", "nrattrnm", MFnData::kString, aNearIDAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNearIDAttributeName));
    
	
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
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aComponentType, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( distAttrName, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( targetType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( targetPoint, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inObject, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDP, outDD ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAddNearID, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNearIDAttributeName, outDD ));
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPDistance", "initialize", std::current_exception());
}
